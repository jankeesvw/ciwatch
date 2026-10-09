#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantList>

// Turns GitHub Actions API responses into the plain maps QML shows. Kept free
// of processes and timers so the tests can feed it recorded responses.
namespace GitHub {

enum LineKind { Plain = 0, Group, Error, Warning, Command, Notice };

struct LogLine {
    QString time;
    QString text;
    int kind = Plain;
};

// One word for a run, job, or step: running, queued, success, failure,
// cancelled, skipped, or neutral.
QString state(const QString &status, const QString &conclusion);
bool isActive(const QString &state);

QVariantList parseRuns(const QByteArray &json, QString *error = nullptr);
QVariantList parseJobs(const QByteArray &json, QString *error = nullptr);
QList<LogLine> parseLog(const QByteArray &raw);

// Runs that were running or queued before and have finished now, so the app
// can tell the user without them watching the window.
QVariantList finishedRuns(const QVariantList &before, const QVariantList &after);

}
