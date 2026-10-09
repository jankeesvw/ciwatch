#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <functional>

#include "logmodel.h"

// Watches the GitHub Actions runs of one repository through the gh CLI, which
// already holds the user's login. Polls quickly while something is running
// and slowly when everything is done.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString repo READ repo NOTIFY repoChanged)
    Q_PROPERTY(QVariantList runs READ runs NOTIFY runsChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)
    Q_PROPERTY(qlonglong selectedRunId READ selectedRunId NOTIFY selectionChanged)
    Q_PROPERTY(qlonglong selectedJobId READ selectedJobId NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRunIndex READ selectedRunIndex NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedRun READ selectedRun NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedJob READ selectedJob NOTIFY selectionChanged)
    Q_PROPERTY(LogModel *log READ log CONSTANT)
    Q_PROPERTY(QString logStatus READ logStatus NOTIFY logStatusChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY runsChanged)
    Q_PROPERTY(QString filter READ filter NOTIFY runsChanged)
    Q_PROPERTY(QString commit READ commit NOTIFY runsChanged)
    Q_PROPERTY(bool showSkipped READ showSkipped NOTIFY runsChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY runsChanged)

public:
    explicit Backend(const QString &repo, bool demo = false, QObject *parent = nullptr);

    QString repo() const { return m_repo; }
    QVariantList runs() const { return m_runs; }
    QVariantList jobs() const { return m_jobs; }
    qlonglong selectedRunId() const { return m_runId; }
    qlonglong selectedJobId() const { return m_jobId; }
    int selectedRunIndex() const;
    QVariantMap selectedRun() const;
    QVariantMap selectedJob() const;
    LogModel *log() { return &m_log; }
    QString logStatus() const { return m_logStatus; }
    QString error() const { return m_error; }
    bool busy() const { return m_pending > 0; }
    QDateTime lastUpdated() const { return m_lastUpdated; }
    QString filter() const { return m_filter; }
    QString commit() const { return m_commit; }
    bool showSkipped() const { return m_showSkipped; }
    int activeCount() const;

    Q_INVOKABLE void refresh();
    // Shows one repository, and with a sha only the runs of that commit, which
    // is how the bar widget hands over a build. Another repository starts over.
    Q_INVOKABLE void show(const QString &repo, const QString &sha = {});
    Q_INVOKABLE void clearCommit();
    Q_INVOKABLE void selectRun(qlonglong id);
    Q_INVOKABLE void selectJob(qlonglong id);
    Q_INVOKABLE void moveRun(int delta);
    Q_INVOKABLE void moveJob(int delta);
    Q_INVOKABLE void cycleFilter();
    Q_INVOKABLE void toggleSkipped();
    Q_INVOKABLE void copyLog();

    // Applies a runs response the way a poll does. Public so tests can drive
    // the selection and notification logic without a network.
    void applyRuns(const QVariantList &runs);
    void applyJobs(qlonglong runId, const QVariantList &jobs);

signals:
    void repoChanged();
    void runsChanged();
    void jobsChanged();
    void selectionChanged();
    void logStatusChanged();
    void logReset();
    void errorChanged();
    void busyChanged();
    void runFinished(const QVariantMap &run);

private:
    using Done = std::function<void(bool ok, const QByteArray &out, const QString &err)>;
    void gh(const QStringList &args, Done done);
    void fetchJobs();
    void fetchLog();
    void filterRuns();
    void pickJob();
    void setError(const QString &error);
    void setLogStatus(const QString &status);
    void schedule();
    void notify(const QVariantMap &run);

    QString m_repo;
    QString m_gh;
    bool m_demo = false;
    QDateTime m_started = QDateTime::currentDateTimeUtc();
    QVariantList m_allRuns;
    QVariantList m_runs;
    QVariantList m_jobs;
    qlonglong m_runId = 0;
    qlonglong m_jobsRunId = 0;
    QString m_jobsUpdatedAt;
    qlonglong m_jobId = 0;
    bool m_jobPinned = false;
    qlonglong m_logJobId = 0;
    bool m_logFinal = false;
    bool m_fetchingRuns = false;
    bool m_fetchingJobs = false;
    bool m_fetchingLog = false;
    LogModel m_log;
    QString m_logStatus;
    QString m_error;
    int m_pending = 0;
    QDateTime m_lastUpdated;
    QString m_filter;
    QString m_commit;
    bool m_showSkipped = false;
    QTimer m_poll;
};
