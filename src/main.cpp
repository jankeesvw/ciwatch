// ciwatch — live GitHub Actions status and logs for one repository.

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>

#include "backend.h"
#include "theme.h"

// One window, however often the bar widget asks for one: a second launch
// hands its repository and commit to the window that is already open.
static QString socketName() {
    return QString("ciwatch-%1").arg(qEnvironmentVariable("USER", "user"));
}

static bool handOver(const QString &repo, const QString &sha) {
    QLocalSocket socket;
    socket.connectToServer(socketName());
    if (!socket.waitForConnected(300))
        return false;
    socket.write((repo + '\n' + sha + '\n').toUtf8());
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

static void raise(QQuickWindow *window) {
    window->show();
    window->raise();
    window->requestActivate();
    // Wayland doesn't let an app take focus on its own; on Hyprland the
    // compositor can be asked to do it.
    if (!QStandardPaths::findExecutable("hyprctl").isEmpty())
        QProcess::startDetached("hyprctl", {"dispatch", "focuswindow", "class:^(ciwatch)$"});
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("ciwatch");
    app.setApplicationVersion("0.2.0");

    // Matches the window to ciwatch.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("ciwatch");
    app.setWindowIcon(QIcon::fromTheme("ciwatch"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Live GitHub Actions status and logs for one repository.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("repo", "The repository to watch, as owner/name (default wbso-ai/application).");
    QCommandLineOption shaOption("sha", "Show only the runs of this commit.", "sha");
    parser.addOption(shaOption);
    QCommandLineOption demoOption("demo", "Show invented runs instead of asking GitHub, for screenshots and trying it out.");
    parser.addOption(demoOption);
    parser.process(app);
    const bool demo = parser.isSet(demoOption);
    const auto repo = parser.positionalArguments().value(0, demo ? "acme/storefront" : "wbso-ai/application");
    const auto sha = parser.value(shaOption).toLower();

    if (!demo && handOver(repo, sha))
        return 0;

    QQuickStyle::setStyle("Material");

    Theme theme;
    Backend backend(repo, demo);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    // A demo window stays out of the way of the real one. A socket left
    // behind by a crash would stop the next launch listening.
    QLocalServer server;
    if (!demo) {
        QLocalServer::removeServer(socketName());
        server.setSocketOptions(QLocalServer::UserAccessOption);
        server.listen(socketName());
    }
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (auto *client = server.nextPendingConnection()) {
            // The sender writes one short request and hangs up, so the whole
            // of it is here by the time the connection closes.
            QObject::connect(client, &QLocalSocket::disconnected, client, [&, client] {
                const auto request = QString::fromUtf8(client->readAll().left(1024)).split('\n');
                client->deleteLater();
                backend.show(request.value(0), request.value(1));
                if (window)
                    raise(window);
            });
        }
    });

    backend.show(repo, sha);
    return app.exec();
}
