#include "backend.h"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include "demo.h"
#include "github.h"

// Poll every few seconds while a run is moving, and back off when it's quiet:
// three API calls per poll stays far below GitHub's hourly limit either way.
static constexpr int ActiveInterval = 6000;
static constexpr int IdleInterval = 60000;

static QString findGh() {
    auto path = QStandardPaths::findExecutable("gh");
    if (!path.isEmpty())
        return path;
    // The launcher may start the app without the shell's PATH, and mise keeps
    // gh outside the system directories.
    for (const auto &candidate : {QDir::homePath() + "/.local/share/mise/shims/gh",
                                  QDir::homePath() + "/.local/bin/gh"})
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    return {};
}

Backend::Backend(const QString &repo, bool demo, QObject *parent)
    : QObject(parent), m_repo(repo), m_gh(findGh()), m_demo(demo) {
    m_poll.setSingleShot(true);
    connect(&m_poll, &QTimer::timeout, this, &Backend::refresh);
    connect(this, &Backend::runFinished, this, &Backend::notify);
}

int Backend::selectedRunIndex() const {
    for (int i = 0; i < m_runs.size(); ++i)
        if (m_runs[i].toMap()["id"].toLongLong() == m_runId)
            return i;
    return -1;
}

QVariantMap Backend::selectedRun() const {
    const int index = selectedRunIndex();
    return index < 0 ? QVariantMap{} : m_runs[index].toMap();
}

QVariantMap Backend::selectedJob() const {
    for (const auto &job : m_jobs)
        if (job.toMap()["id"].toLongLong() == m_jobId)
            return job.toMap();
    return {};
}

int Backend::activeCount() const {
    int count = 0;
    for (const auto &run : m_allRuns)
        count += GitHub::isActive(run.toMap()["state"].toString());
    return count;
}

void Backend::gh(const QStringList &args, Done done) {
    if (m_demo) {
        // Answered a moment later, the way a real call is, so the window goes
        // through the same states.
        const auto out = Demo::respond(args.last(), m_started);
        QTimer::singleShot(120, this, [done, out] { done(true, out, {}); });
        return;
    }
    if (m_gh.isEmpty()) {
        done(false, {}, "The GitHub CLI (gh) isn't installed. Install it with `omarchy-pkg-add github-cli` "
                        "and log in with `gh auth login`.");
        return;
    }
    auto *process = new QProcess(this);
    ++m_pending;
    emit busyChanged();
    connect(process, &QProcess::finished, this, [this, process, done](int code, QProcess::ExitStatus status) {
        --m_pending;
        emit busyChanged();
        const auto err = QString::fromUtf8(process->readAllStandardError()).trimmed();
        done(status == QProcess::NormalExit && code == 0, process->readAllStandardOutput(), err);
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        --m_pending;
        emit busyChanged();
        done(false, {}, QString("Couldn't start %1: %2").arg(m_gh, process->errorString()));
        process->deleteLater();
    });
    process->start(m_gh, args);
}

void Backend::refresh() {
    if (m_fetchingRuns)
        return;
    m_fetchingRuns = true;
    m_poll.stop();
    // Asking for one commit's runs by sha finds them however old the commit
    // is, where the latest 40 runs of the repository might not reach back.
    auto path = QString("repos/%1/actions/runs?per_page=40").arg(m_repo);
    if (!m_commit.isEmpty())
        path += "&head_sha=" + m_commit;
    const auto repo = m_repo;
    const auto commit = m_commit;
    gh({"api", path},
       [this, repo, commit](bool ok, const QByteArray &out, const QString &err) {
           m_fetchingRuns = false;
           // The user switched repository or commit while this was in flight.
           if (repo != m_repo || commit != m_commit) {
               refresh();
               return;
           }
           if (!ok) {
               setError(err.isEmpty() ? "Fetching runs from GitHub failed." : err);
               schedule();
               return;
           }
           QString parseError;
           const auto runs = GitHub::parseRuns(out, &parseError);
           if (!parseError.isEmpty()) {
               setError(parseError);
               schedule();
               return;
           }
           setError({});
           applyRuns(runs);
           fetchJobs();
           schedule();
       });
}

void Backend::applyRuns(const QVariantList &runs) {
    const auto finished = m_allRuns.isEmpty() ? QVariantList{} : GitHub::finishedRuns(m_allRuns, runs);
    m_allRuns = runs;
    m_lastUpdated = QDateTime::currentDateTime();
    filterRuns();
    for (const auto &run : finished)
        emit runFinished(run.toMap());
}

void Backend::filterRuns() {
    m_runs.clear();
    for (const auto &value : m_allRuns) {
        const auto run = value.toMap();
        if (!m_showSkipped && run["state"] == "skipped")
            continue;
        if (!m_filter.isEmpty() && run["workflow"] != m_filter)
            continue;
        if (!m_commit.isEmpty() && !run["fullSha"].toString().startsWith(m_commit))
            continue;
        m_runs << run;
    }
    emit runsChanged();

    if (selectedRunIndex() < 0) {
        m_runId = m_runs.isEmpty() ? 0 : m_runs.first().toMap()["id"].toLongLong();
        // Handed one commit, show the run that needs looking at: the one that
        // failed, else the one still going. Newest first is right otherwise.
        if (!m_commit.isEmpty()) {
            for (const auto &want : {"failure", "running"}) {
                const auto found = std::find_if(m_runs.cbegin(), m_runs.cend(), [want](const QVariant &run) {
                    return run.toMap()["state"] == want;
                });
                if (found != m_runs.cend()) {
                    m_runId = found->toMap()["id"].toLongLong();
                    break;
                }
            }
        }
        m_jobs.clear();
        m_jobId = 0;
        m_jobPinned = false;
        emit jobsChanged();
    }
    emit selectionChanged();
}

void Backend::schedule() {
    m_poll.start(activeCount() > 0 ? ActiveInterval : IdleInterval);
}

void Backend::fetchJobs() {
    const auto run = selectedRun();
    if (run.isEmpty() || m_fetchingJobs)
        return;
    // A finished run's jobs don't change, so only ask again when the run has.
    const bool fresh = m_jobsRunId == m_runId && m_jobsUpdatedAt == run["updatedAt"].toString() &&
                       !GitHub::isActive(run["state"].toString());
    if (fresh) {
        fetchLog();
        return;
    }
    const auto runId = m_runId;
    const auto updatedAt = run["updatedAt"].toString();
    m_fetchingJobs = true;
    gh({"api", QString("repos/%1/actions/runs/%2/jobs?per_page=100").arg(m_repo).arg(runId)},
       [this, runId, updatedAt](bool ok, const QByteArray &out, const QString &err) {
           m_fetchingJobs = false;
           if (runId != m_runId) {
               fetchJobs();
               return;
           }
           if (!ok) {
               setError(err.isEmpty() ? "Fetching jobs from GitHub failed." : err);
               return;
           }
           m_jobsUpdatedAt = updatedAt;
           applyJobs(runId, GitHub::parseJobs(out));
           fetchLog();
       });
}

void Backend::applyJobs(qlonglong runId, const QVariantList &jobs) {
    if (runId != m_runId)
        return;
    m_jobsRunId = runId;
    m_jobs = jobs;
    emit jobsChanged();
    pickJob();
    emit selectionChanged();
}

// Shows the job worth looking at: the one that failed, else the one running,
// else the last one that did any work. A job the user picked stays picked.
void Backend::pickJob() {
    if (m_jobPinned && !selectedJob().isEmpty())
        return;
    auto find = [this](auto predicate) -> qlonglong {
        for (const auto &job : m_jobs)
            if (predicate(job.toMap()))
                return job.toMap()["id"].toLongLong();
        return 0;
    };
    qlonglong id = find([](const QVariantMap &job) { return job["state"] == "failure"; });
    if (!id)
        id = find([](const QVariantMap &job) { return job["state"] == "running"; });
    if (!id)
        for (const auto &job : m_jobs)
            if (!job.toMap()["steps"].toList().isEmpty())
                id = job.toMap()["id"].toLongLong();
    if (!id && !m_jobs.isEmpty())
        id = m_jobs.first().toMap()["id"].toLongLong();
    if (id != m_jobId) {
        m_jobId = id;
        m_log.clear();
        m_logJobId = 0;
        emit logReset();
    }
}

void Backend::fetchLog() {
    const auto job = selectedJob();
    if (job.isEmpty() || m_fetchingLog) {
        if (job.isEmpty())
            setLogStatus(m_jobs.isEmpty() ? "" : "Pick a job to see its log.");
        return;
    }
    const auto state = job["state"].toString();
    if (state == "skipped" || (state == "queued" && job["steps"].toList().isEmpty())) {
        m_log.clear();
        setLogStatus(state == "skipped" ? "This job was skipped, so it has no log."
                                        : "This job is waiting for a runner.");
        return;
    }
    if (m_logJobId == m_jobId && m_logFinal)
        return;

    const auto jobId = m_jobId;
    const bool final = !GitHub::isActive(state);
    if (m_logJobId != jobId)
        setLogStatus("Loading log…");
    m_fetchingLog = true;
    gh({"api", "--allow-escape-sequences", QString("repos/%1/actions/jobs/%2/logs").arg(m_repo).arg(jobId)},
       [this, jobId, final](bool ok, const QByteArray &out, const QString &err) {
           m_fetchingLog = false;
           if (jobId != m_jobId) {
               fetchLog();
               return;
           }
           if (!ok) {
               // GitHub only serves a job's log once the runner has uploaded
               // some of it, which lags behind a job that has just started.
               if (!final)
                   setLogStatus("Waiting for GitHub to publish the log of this running job…");
               else
                   setLogStatus(err.isEmpty() ? "GitHub didn't return a log for this job." : err);
               return;
           }
           const bool reset = m_logJobId != jobId;
           m_logJobId = jobId;
           m_logFinal = final;
           m_log.setLines(GitHub::parseLog(out));
           setLogStatus({});
           if (reset)
               emit logReset();
       });
}

void Backend::selectRun(qlonglong id) {
    if (id == m_runId)
        return;
    m_runId = id;
    m_jobs.clear();
    m_jobsRunId = 0;
    m_jobId = 0;
    m_jobPinned = false;
    m_logJobId = 0;
    m_log.clear();
    setLogStatus({});
    emit jobsChanged();
    emit selectionChanged();
    emit logReset();
    fetchJobs();
}

void Backend::selectJob(qlonglong id) {
    m_jobPinned = true;
    if (id == m_jobId)
        return;
    m_jobId = id;
    m_logJobId = 0;
    m_log.clear();
    emit selectionChanged();
    emit logReset();
    fetchLog();
}

void Backend::moveRun(int delta) {
    if (m_runs.isEmpty())
        return;
    const int index = qBound(0, selectedRunIndex() + delta, int(m_runs.size()) - 1);
    selectRun(m_runs[index].toMap()["id"].toLongLong());
}

void Backend::moveJob(int delta) {
    if (m_jobs.isEmpty())
        return;
    int index = 0;
    for (int i = 0; i < m_jobs.size(); ++i)
        if (m_jobs[i].toMap()["id"].toLongLong() == m_jobId)
            index = i;
    index = (index + delta + m_jobs.size()) % m_jobs.size();
    selectJob(m_jobs[index].toMap()["id"].toLongLong());
}

void Backend::show(const QString &repo, const QString &sha) {
    static const QRegularExpression validRepo(R"(^[A-Za-z0-9_.-]{1,100}/[A-Za-z0-9_.-]{1,100}$)");
    static const QRegularExpression validSha(R"(^[0-9a-f]{7,40}$)");
    if (!validRepo.match(repo).hasMatch()) {
        setError(QString("\"%1\" isn't a repository; pass it as owner/name.").arg(repo));
        return;
    }
    const auto commit = validSha.match(sha).hasMatch() ? sha : QString();
    if (repo != m_repo) {
        m_repo = repo;
        m_allRuns.clear();
        m_filter.clear();
        m_runId = 0;
        emit repoChanged();
    }
    m_commit = commit;
    // Picks the commit's run if it is already here; otherwise that happens
    // when its runs arrive.
    filterRuns();
    if (m_fetchingRuns)
        return; // The running fetch sees the change and starts over.
    refresh();
}

void Backend::clearCommit() {
    if (m_commit.isEmpty())
        return;
    m_commit.clear();
    filterRuns();
    refresh();
}

void Backend::cycleFilter() {
    if (!m_commit.isEmpty()) {
        clearCommit();
        return;
    }
    QStringList workflows;
    for (const auto &run : m_allRuns)
        if (!workflows.contains(run.toMap()["workflow"].toString()))
            workflows << run.toMap()["workflow"].toString();
    workflows.sort();
    workflows.prepend(QString());
    m_filter = workflows.value((workflows.indexOf(m_filter) + 1) % workflows.size());
    filterRuns();
    fetchJobs();
}

void Backend::toggleSkipped() {
    m_showSkipped = !m_showSkipped;
    filterRuns();
    fetchJobs();
}

void Backend::copyLog() {
    QGuiApplication::clipboard()->setText(m_log.text());
}

void Backend::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

void Backend::setLogStatus(const QString &status) {
    if (status == m_logStatus)
        return;
    m_logStatus = status;
    emit logStatusChanged();
}

void Backend::notify(const QVariantMap &run) {
    static const QHash<QString, QString> verbs{
        {"success", "passed"}, {"failure", "failed"}, {"cancelled", "was cancelled"}, {"neutral", "finished"}};
    const auto summary = QString("%1 %2").arg(run["workflow"].toString(),
                                              verbs.value(run["state"].toString(), "finished"));
    const auto body = QString("%1\n%2").arg(run["title"].toString(), run["branch"].toString());
    QStringList args{"--app-name=CI Watch", "--icon=ciwatch", summary, body};
    if (run["state"] == "failure")
        args.prepend("--urgency=critical");
    QProcess::startDetached("notify-send", args);
}
