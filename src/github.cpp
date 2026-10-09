#include "github.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace GitHub {

QString state(const QString &status, const QString &conclusion) {
    if (status == "in_progress")
        return "running";
    if (status != "completed")
        return "queued";
    if (conclusion == "success")
        return "success";
    if (conclusion == "failure" || conclusion == "timed_out" || conclusion == "startup_failure")
        return "failure";
    if (conclusion == "cancelled")
        return "cancelled";
    if (conclusion == "skipped")
        return "skipped";
    if (conclusion == "action_required")
        return "queued";
    return "neutral";
}

bool isActive(const QString &state) {
    return state == "running" || state == "queued";
}

static QJsonArray arrayIn(const QByteArray &json, const char *key, QString *error) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QString("GitHub sent a response that isn't JSON: %1").arg(parseError.errorString());
        return {};
    }
    return document.object().value(key).toArray();
}

QVariantList parseRuns(const QByteArray &json, QString *error) {
    QVariantList runs;
    for (const auto &value : arrayIn(json, "workflow_runs", error)) {
        const auto run = value.toObject();
        const auto status = run["status"].toString();
        const auto conclusion = run["conclusion"].toString();
        const auto started = run["run_started_at"].toString();
        runs << QVariantMap{
            {"id", run["id"].toVariant().toLongLong()},
            {"workflow", run["name"].toString()},
            {"title", run["display_title"].toString()},
            {"branch", run["head_branch"].toString()},
            {"event", run["event"].toString()},
            {"actor", run["actor"].toObject()["login"].toString()},
            {"number", run["run_number"].toInt()},
            {"attempt", run["run_attempt"].toInt()},
            {"sha", run["head_sha"].toString().left(7)},
            {"fullSha", run["head_sha"].toString()},
            {"url", run["html_url"].toString()},
            {"status", status},
            {"conclusion", conclusion},
            {"state", state(status, conclusion)},
            {"startedAt", started.isEmpty() ? run["created_at"].toString() : started},
            {"updatedAt", run["updated_at"].toString()},
        };
    }
    return runs;
}

QVariantList parseJobs(const QByteArray &json, QString *error) {
    QVariantList jobs;
    for (const auto &value : arrayIn(json, "jobs", error)) {
        const auto job = value.toObject();
        QVariantList steps;
        for (const auto &stepValue : job["steps"].toArray()) {
            const auto step = stepValue.toObject();
            steps << QVariantMap{
                {"number", step["number"].toInt()},
                {"name", step["name"].toString()},
                {"state", state(step["status"].toString(), step["conclusion"].toString())},
                {"startedAt", step["started_at"].toString()},
                {"completedAt", step["completed_at"].toString()},
            };
        }
        jobs << QVariantMap{
            {"id", job["id"].toVariant().toLongLong()},
            {"name", job["name"].toString()},
            {"state", state(job["status"].toString(), job["conclusion"].toString())},
            {"url", job["html_url"].toString()},
            {"startedAt", job["started_at"].toString()},
            {"completedAt", job["completed_at"].toString()},
            {"steps", steps},
        };
    }
    return jobs;
}

QList<LogLine> parseLog(const QByteArray &raw) {
    static const QRegularExpression ansi(R"(\x1b\[[0-9;?]*[A-Za-z])");
    static const QRegularExpression stamp(R"(^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?Z) ?)");
    static const QList<QPair<QString, int>> markers{
        {"##[group]", Group},     {"##[error]", Error},   {"##[warning]", Warning},
        {"##[command]", Command}, {"##[notice]", Notice}, {"##[section]", Group},
        // Some actions, actions/checkout among them, echo commands without the ##.
        {"[command]", Command},
    };

    QString text = QString::fromUtf8(raw);
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    text.remove(ansi);
    text.remove('\r');

    QList<LogLine> lines;
    for (const auto &rawLine : text.split('\n')) {
        LogLine line;
        QString rest = rawLine;
        const auto match = stamp.match(rest);
        if (match.hasMatch()) {
            const auto when = QDateTime::fromString(match.captured(1), Qt::ISODateWithMs);
            if (when.isValid())
                line.time = when.toLocalTime().toString("HH:mm:ss");
            rest = rest.mid(match.capturedLength());
        }
        if (rest.startsWith("##[endgroup]"))
            continue;
        for (const auto &[prefix, kind] : markers) {
            if (rest.startsWith(prefix)) {
                rest = rest.mid(prefix.size());
                line.kind = kind;
                break;
            }
        }
        line.text = rest;
        lines << line;
    }
    // The log ends in a newline, which split() turns into one empty line.
    if (!lines.isEmpty() && lines.last().text.isEmpty() && lines.last().time.isEmpty())
        lines.removeLast();
    return lines;
}

QVariantList finishedRuns(const QVariantList &before, const QVariantList &after) {
    QHash<qlonglong, QString> previous;
    for (const auto &run : before)
        previous.insert(run.toMap()["id"].toLongLong(), run.toMap()["state"].toString());

    QVariantList finished;
    for (const auto &value : after) {
        const auto run = value.toMap();
        const auto id = run["id"].toLongLong();
        const auto now = run["state"].toString();
        if (previous.contains(id) && isActive(previous[id]) && !isActive(now) && now != "skipped")
            finished << run;
    }
    return finished;
}

}
