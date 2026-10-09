#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

// Invented runs, jobs and logs for screenshots and for trying the app without
// a GitHub login: one build running with a log that grows, one that failed,
// and a few that passed. Answers the same API paths the app asks gh for.
namespace Demo {

QByteArray respond(const QString &path, const QDateTime &started);

}
