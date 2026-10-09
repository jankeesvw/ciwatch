#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "github.h"
#include "logmodel.h"
#include "theme.h"

static QVariantMap run(qlonglong id, const QString &state, const QString &workflow = "Tests & Deploy") {
    return {{"id", id}, {"state", state}, {"workflow", workflow}, {"title", "Change"}, {"branch", "main"}};
}

static QVariantMap job(qlonglong id, const QString &state, bool withSteps = true) {
    QVariantList steps;
    if (withSteps)
        steps << QVariantMap{{"name", "Run"}};
    return {{"id", id}, {"name", QString("job %1").arg(id)}, {"state", state}, {"steps", steps}};
}

class CiwatchTests : public QObject {
    Q_OBJECT

private slots:
    void statesFromStatusAndConclusion()
    {
        QCOMPARE(GitHub::state("in_progress", ""), QString("running"));
        QCOMPARE(GitHub::state("queued", ""), QString("queued"));
        QCOMPARE(GitHub::state("waiting", ""), QString("queued"));
        QCOMPARE(GitHub::state("completed", "success"), QString("success"));
        QCOMPARE(GitHub::state("completed", "timed_out"), QString("failure"));
        QCOMPARE(GitHub::state("completed", "cancelled"), QString("cancelled"));
        QCOMPARE(GitHub::state("completed", "skipped"), QString("skipped"));
    }

    void parsesRuns()
    {
        const QByteArray json = R"({"workflow_runs":[{"id":37935804748,"name":"Tests & Deploy","display_title":"Post announcements","head_branch":"main","event":"push","actor":{"login":"jankeesvw"},"run_number":4335,"run_attempt":1,"head_sha":"7847dc23aa6e551090aff0df","html_url":"https://github.com/x","status":"in_progress","conclusion":null,"run_started_at":"2026-10-09T13:17:36Z","created_at":"2026-10-09T13:17:30Z","updated_at":"2026-10-09T13:17:49Z"}]})";
        QString error;
        const auto runs = GitHub::parseRuns(json, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(runs.size(), 1);
        const auto r = runs.first().toMap();
        QCOMPARE(r["id"].toLongLong(), 37935804748LL);
        QCOMPARE(r["state"].toString(), QString("running"));
        QCOMPARE(r["sha"].toString(), QString("7847dc2"));
        QCOMPARE(r["actor"].toString(), QString("jankeesvw"));
        QCOMPARE(r["startedAt"].toString(), QString("2026-10-09T13:17:36Z"));
    }

    void reportsBrokenJson()
    {
        QString error;
        QVERIFY(GitHub::parseRuns("<html>", &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void parsesJobsWithSteps()
    {
        const QByteArray json = R"({"jobs":[{"id":1,"name":"test","status":"completed","conclusion":"failure","steps":[{"number":1,"name":"Set up job","status":"completed","conclusion":"success"},{"number":2,"name":"Run tests","status":"completed","conclusion":"failure"}]}]})";
        const auto jobs = GitHub::parseJobs(json);
        QCOMPARE(jobs.size(), 1);
        const auto steps = jobs.first().toMap()["steps"].toList();
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps[1].toMap()["state"].toString(), QString("failure"));
    }

    void parsesLogMarkersAndStripsNoise()
    {
        const QByteArray raw = QByteArray("\xEF\xBB\xBF") + "2026-10-09T13:17:39.8972190Z Current runner\r\n"
                               "2026-10-09T13:17:39.9008832Z ##[group]Runner Image\n"
                               "2026-10-09T13:17:39.9028323Z ##[endgroup]\n"
                               "2026-10-09T13:17:40.0000000Z \x1b[36;1mnpm test\x1b[0m\n"
                               "2026-10-09T13:17:41.0000000Z [command]/usr/bin/git log\n"
                               "2026-10-09T13:17:42.0000000Z ##[warning]Deprecated\n"
                               "2026-10-09T13:17:43.0000000Z ##[error]Process completed with exit code 1.\n";
        const auto lines = GitHub::parseLog(raw);
        QCOMPARE(lines.size(), 6);
        QCOMPARE(lines[0].text, QString("Current runner"));
        QCOMPARE(lines[0].time.size(), 8);
        QCOMPARE(lines[1].kind, int(GitHub::Group));
        QCOMPARE(lines[1].text, QString("Runner Image"));
        QCOMPARE(lines[2].text, QString("npm test"));
        QCOMPARE(lines[3].kind, int(GitHub::Command));
        QCOMPARE(lines[4].kind, int(GitHub::Warning));
        QCOMPARE(lines[5].kind, int(GitHub::Error));
        QCOMPARE(lines[5].text, QString("Process completed with exit code 1."));
    }

    void logModelAppendsGrowingLogs()
    {
        LogModel model;
        model.setLines(GitHub::parseLog("one\ntwo\n"));
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.setLines(GitHub::parseLog("one\ntwo\n##[error]three\n"));
        QCOMPARE(inserted.size(), 1);
        QCOMPARE(reset.size(), 0);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.firstError(), 2);
        QCOMPARE(model.nextError(2), -1);
        QCOMPARE(model.text(), QString("one\ntwo\nthree"));
    }

    void finishedRunsOnlyReportsTransitions()
    {
        const QVariantList before{run(1, "running"), run(2, "queued"), run(3, "success"), run(4, "running")};
        const QVariantList after{run(1, "failure"), run(2, "running"), run(3, "success"), run(4, "skipped"),
                                 run(5, "success")};
        const auto finished = GitHub::finishedRuns(before, after);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().toMap()["id"].toLongLong(), 1LL);
    }

    void backendHidesSkippedAndSelectsFirst()
    {
        Backend backend("owner/repo");
        backend.applyRuns({run(1, "skipped", "Claude Code"), run(2, "running"), run(3, "success", "Linting")});
        QCOMPARE(backend.runs().size(), 2);
        QCOMPARE(backend.selectedRunId(), 2LL);
        QCOMPARE(backend.activeCount(), 1);
        backend.toggleSkipped();
        QCOMPARE(backend.runs().size(), 3);
        QCOMPARE(backend.selectedRunId(), 2LL);
    }

    void backendCyclesWorkflowFilter()
    {
        Backend backend("owner/repo");
        backend.applyRuns({run(1, "success", "Tests & Deploy"), run(2, "success", "Linting")});
        backend.cycleFilter();
        QCOMPARE(backend.filter(), QString("Linting"));
        QCOMPARE(backend.runs().size(), 1);
        QCOMPARE(backend.selectedRunId(), 2LL);
        backend.cycleFilter();
        backend.cycleFilter();
        QCOMPARE(backend.filter(), QString());
        QCOMPARE(backend.runs().size(), 2);
    }

    void backendPicksTheFailedJob()
    {
        Backend backend("owner/repo");
        backend.applyRuns({run(1, "failure")});
        backend.applyJobs(1, {job(10, "success"), job(11, "failure"), job(12, "skipped", false)});
        QCOMPARE(backend.selectedJobId(), 11LL);
    }

    void backendFollowsTheRunningJobUntilPinned()
    {
        Backend backend("owner/repo");
        backend.applyRuns({run(1, "running")});
        backend.applyJobs(1, {job(10, "running"), job(11, "queued", false)});
        QCOMPARE(backend.selectedJobId(), 10LL);
        backend.applyJobs(1, {job(10, "success"), job(11, "running")});
        QCOMPARE(backend.selectedJobId(), 11LL);
        backend.selectJob(10);
        backend.applyJobs(1, {job(10, "success"), job(11, "running")});
        QCOMPARE(backend.selectedJobId(), 10LL);
    }

    void backendAnnouncesFinishedRuns()
    {
        Backend backend("owner/repo");
        QSignalSpy finished(&backend, &Backend::runFinished);
        backend.applyRuns({run(1, "running")});
        QCOMPARE(finished.size(), 0);
        backend.applyRuns({run(1, "success")});
        QCOMPARE(finished.size(), 1);
    }

    void backendShowsOneCommitAndPrefersItsFailure()
    {
        Backend backend("owner/repo");
        auto a = run(1, "success");
        a["fullSha"] = "aaaaaaa1111";
        auto b = run(2, "success");
        b["fullSha"] = "bbbbbbb2222";
        auto c = run(3, "failure", "Linting");
        c["fullSha"] = "bbbbbbb2222";
        backend.applyRuns({a, b, c});
        QCOMPARE(backend.selectedRunId(), 1LL);
        backend.show("owner/repo", "bbbbbbb2222");
        QCOMPARE(backend.commit(), QString("bbbbbbb2222"));
        QCOMPARE(backend.runs().size(), 2);
        QCOMPARE(backend.selectedRunId(), 3LL);
        backend.clearCommit();
        QCOMPARE(backend.runs().size(), 3);
    }

    void backendRefusesAnUnknownRepositoryShape()
    {
        Backend backend("owner/repo");
        backend.show("not a repo; rm -rf");
        QCOMPARE(backend.repo(), QString("owner/repo"));
        QVERIFY(!backend.error().isEmpty());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }
};

QTEST_MAIN(CiwatchTests)
#include "ciwatch_tests.moc"
