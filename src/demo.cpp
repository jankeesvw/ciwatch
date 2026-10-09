#include "demo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace Demo {

// When the app started. Runs and jobs are placed relative to it rather than to
// the current time, so a running build's clock keeps counting between fetches.
static QDateTime origin = QDateTime::currentDateTimeUtc();

static QString iso(const QDateTime &when) {
    return when.toUTC().toString(Qt::ISODate);
}

static QJsonObject run(qint64 id, const QString &workflow, const QString &title, const QString &branch,
                       const QString &sha, const QString &status, const QString &conclusion, int minutesAgo,
                       int seconds, const QString &actor = "mira", const QString &event = "push") {
    const auto now = QDateTime::currentDateTimeUtc();
    const auto started = origin.addSecs(-minutesAgo * 60);
    return {{"id", id}, {"name", workflow}, {"display_title", title}, {"head_branch", branch},
            {"event", event}, {"actor", QJsonObject{{"login", actor}}}, {"run_number", int(id % 10000)},
            {"run_attempt", 1}, {"head_sha", sha}, {"html_url", "https://github.com/acme/storefront"},
            {"status", status}, {"conclusion", conclusion.isEmpty() ? QJsonValue() : QJsonValue(conclusion)},
            {"run_started_at", iso(started)},
            {"updated_at", iso(status == "completed" ? started.addSecs(seconds) : now)}};
}

static QJsonObject job(qint64 id, const QString &name, const QString &status, const QString &conclusion,
                       int startedAgo, int seconds, const QStringList &stepNames, int runningStep = -1) {
    QJsonArray steps;
    for (int i = 0; i < stepNames.size(); ++i) {
        QString stepStatus = "completed", stepConclusion = conclusion;
        if (runningStep >= 0) {
            stepStatus = i < runningStep ? "completed" : i == runningStep ? "in_progress" : "queued";
            stepConclusion = i < runningStep ? "success" : "";
        } else if (conclusion == "failure") {
            stepConclusion = i < stepNames.size() - 2 ? "success" : i == stepNames.size() - 2 ? "failure" : "skipped";
        }
        steps << QJsonObject{{"number", i + 1}, {"name", stepNames[i]}, {"status", stepStatus},
                             {"conclusion", stepConclusion.isEmpty() ? QJsonValue() : QJsonValue(stepConclusion)}};
    }
    const auto started = origin.addSecs(-startedAgo);
    return {{"id", id}, {"name", name}, {"status", status},
            {"conclusion", conclusion.isEmpty() ? QJsonValue() : QJsonValue(conclusion)},
            {"html_url", "https://github.com/acme/storefront"}, {"started_at", iso(started)},
            {"completed_at", status == "completed" ? QJsonValue(iso(started.addSecs(seconds))) : QJsonValue()},
            {"steps", steps}};
}

static const QStringList testSteps{"Set up job", "Check out the code", "Set up Ruby", "Prepare the database",
                                   "Run the tests", "Complete job"};

static QByteArray runs() {
    const QJsonArray list{
        run(9101, "Tests & Deploy", "Add gift cards to checkout", "main", "4f1c2ab9e07d", "in_progress", "", 3, 0),
        run(9100, "Linting & Security", "Add gift cards to checkout", "main", "4f1c2ab9e07d", "completed",
            "success", 3, 48),
        run(9099, "Tests & Deploy", "Show order totals with tax", "order-totals", "b83e01d5c2fa", "completed",
            "failure", 26, 312, "tomas", "pull_request"),
        run(9098, "Linting & Security", "Show order totals with tax", "order-totals", "b83e01d5c2fa",
            "completed", "success", 26, 51, "tomas", "pull_request"),
        run(9097, "Tests & Deploy", "Bump rails from 8.1.1 to 8.1.2", "dependabot/bundler/rails-8.1.2",
            "e2a94c7f1b30", "completed", "success", 71, 403, "dependabot", "pull_request"),
        run(9096, "Tests & Deploy", "Fix rounding in shipping costs", "main", "0d7b3e9a51c4", "completed",
            "success", 128, 451),
        run(9095, "Linting & Security", "Fix rounding in shipping costs", "main", "0d7b3e9a51c4", "completed",
            "success", 128, 44),
        run(9094, "Tests & Deploy", "Send a receipt when an order ships", "main", "7c22f0e4ad19", "completed",
            "success", 205, 437, "tomas"),
        run(9093, "Nightly evaluations", "Nightly evaluations", "main", "7c22f0e4ad19", "completed", "success",
            540, 1260, "mira", "schedule"),
    };
    return QJsonDocument(QJsonObject{{"workflow_runs", list}}).toJson(QJsonDocument::Compact);
}

static QByteArray jobs(qint64 runId) {
    QJsonArray list;
    if (runId == 9101) {
        list << job(1, "changes", "completed", "success", 175, 9, {"Set up job", "Look for changes", "Complete job"})
             << job(2, "test", "in_progress", "", 160, 0, testSteps, 4)
             << job(3, "build", "in_progress", "", 160, 0, {"Set up job", "Build the image", "Push the image", "Complete job"}, 1)
             << job(4, "deploy", "queued", "", 0, 0, {});
    } else if (runId == 9099) {
        list << job(5, "changes", "completed", "success", 1560, 8, {"Set up job", "Look for changes", "Complete job"})
             << job(6, "test", "completed", "failure", 1550, 296, testSteps)
             << job(7, "build", "completed", "success", 1550, 140, {"Set up job", "Build the image", "Complete job"})
             << job(8, "deploy", "completed", "skipped", 1250, 0, {});
    } else {
        list << job(runId * 10, "test", "completed", "success", 600, 380, testSteps)
             << job(runId * 10 + 1, "build", "completed", "success", 600, 150,
                    {"Set up job", "Build the image", "Push the image", "Complete job"});
    }
    return QJsonDocument(QJsonObject{{"jobs", list}}).toJson(QJsonDocument::Compact);
}

static const QStringList tests{
    "CartTest#test_adds_a_gift_card", "CartTest#test_removes_an_expired_gift_card",
    "CheckoutTest#test_pays_with_a_gift_card", "CheckoutTest#test_splits_payment_over_card_and_gift_card",
    "GiftCardTest#test_rejects_a_used_code", "GiftCardTest#test_balance_never_goes_negative",
    "OrderTest#test_ships_to_the_billing_address_by_default", "OrderTest#test_sends_a_receipt",
    "ShippingCostTest#test_rounds_to_whole_cents", "ShippingCostTest#test_free_above_fifty_euros",
    "ProductsControllerTest#test_index", "ProductsControllerTest#test_show",
    "OrdersControllerTest#test_create", "OrdersControllerTest#test_cancel",
};

static QStringList preamble() {
    return {"##[group]Run bin/rails db:prepare", "Created database 'storefront_test'", "##[endgroup]",
            "##[group]Run bin/rails test", "bin/rails test", "shell: /usr/bin/bash -e {0}", "env:",
            "  RAILS_ENV: test", "##[endgroup]", "Running 312 tests in parallel using 4 processes",
            "Run options: --seed 4242", "", "# Running:", ""};
}

static QByteArray log(qint64 jobId, const QDateTime &started) {
    // Fixed to when the app started, so a later fetch only adds lines.
    const auto base = started.addSecs(-170);
    QStringList lines;
    if (jobId == 6) {
        lines = preamble();
        for (int i = 0; i < 9; ++i)
            lines << QString("%1 = %2 s = .").arg(tests[i]).arg(0.12 + i * 0.07, 0, 'f', 2);
        lines << "" << "Failure:" << "OrderTotalTest#test_includes_tax_in_the_total [test/models/order_total_test.rb:18]:"
              << "Expected: 121.0" << "  Actual: 100.0" << ""
              << "bin/rails test test/models/order_total_test.rb:18" << ""
              << "Finished in 41.873s, 7.45 runs/s, 22.64 assertions/s."
              << "312 runs, 948 assertions, 1 failures, 0 errors, 0 skips"
              << "##[error]Process completed with exit code 1.";
    } else if (jobId == 2) {
        // Grows by a line a second, so the window has something to follow.
        lines = preamble();
        const int grown = 18 + int(started.secsTo(QDateTime::currentDateTimeUtc()));
        for (int i = 0; i < grown; ++i)
            lines << QString("%1 = %2 s = .").arg(tests[i % tests.size()]).arg(0.08 + (i * 37 % 50) / 100.0, 0, 'f', 2);
    } else {
        lines = preamble();
        for (const auto &test : tests)
            lines << test + " = 0.21 s = .";
        lines << "" << "Finished in 38.204s" << "312 runs, 948 assertions, 0 failures, 0 errors, 0 skips";
    }
    QByteArray out;
    for (int i = 0; i < lines.size(); ++i)
        out += (base.addMSecs(i * 400).toString(Qt::ISODateWithMs) + " " + lines[i] + "\n").toUtf8();
    return out;
}

QByteArray respond(const QString &path, const QDateTime &started) {
    origin = started;
    const auto parts = path.section('?', 0, 0).split('/');
    if (path.endsWith("/logs"))
        return log(parts.value(parts.size() - 2).toLongLong(), started);
    if (path.contains("/jobs"))
        return jobs(parts.value(parts.size() - 2).toLongLong());
    return runs();
}

}
