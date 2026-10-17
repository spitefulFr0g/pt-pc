#include "supervisor.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUuid>
#include <QSet>
#include <QElapsedTimer>
#include <cstdio>
#ifdef __linux__
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <poll.h>
#endif
namespace pt::launcher {
static QString rootPath(const std::filesystem::path& root) {
    return QString::fromStdU32String(std::filesystem::weakly_canonical(root).u32string());
}
QString SessionEndpoint(const std::filesystem::path& root) {
    return QDir::tempPath() + "/pt-session-" + QString::number(
#ifdef __linux__
getuid()
#else
0
#endif
) + "-" +
        QCryptographicHash::hash(rootPath(root).toUtf8(), QCryptographicHash::Sha256).toHex();
}
QJsonObject SessionRequest(const std::filesystem::path& root, const QJsonObject& request) {
    QLocalSocket socket; socket.connectToServer(SessionEndpoint(root));
    if (!socket.waitForConnected(1000)) return {};
    socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    if (!socket.waitForBytesWritten(1000)) return {};
    QByteArray data;
    while (!data.contains('\n') && data.size() < 65536) {
        if (!socket.bytesAvailable() && !socket.waitForReadyRead(15000)) return {};
        data += socket.readAll();
    }
    return QJsonDocument::fromJson(data).object();
}
#ifdef __linux__
static volatile sig_atomic_t stopping = 0;
static void stop(int) { stopping = 1; }
int SessionEntry(int argc, char** argv, const std::vector<SupportedRuntime>& supported) {
    QCoreApplication app(argc, argv);
    if (argc < 3) return 2;
    const std::filesystem::path root = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdU32String());
    if (!std::filesystem::is_directory(root)) return 2;
    if (QString::fromLocal8Bit(argv[1]) == "--session-command") {
        if (argc != 4) return 2;
        auto response = SessionRequest(root, QJsonDocument::fromJson(argv[3]).object());
        if (response.isEmpty()) return 1;
        puts(QJsonDocument(response).toJson(QJsonDocument::Compact).constData()); return 0;
    }
    QLockFile lock(rootPath(root) + "/.pt-supervisor.lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return SessionRequest(root, {{"v",1},{"op","reconnect"}}).isEmpty() ? 1 : 0;
    int probe = syscall(SYS_pidfd_open, getpid(), 0);
    if (probe < 0) { fputs("Gaming Mode requires Linux pidfd process handles.\n", stderr); return 1; }
    close(probe);
    const QString endpoint = SessionEndpoint(root);
    QLocalServer::removeServer(endpoint);
    QLocalServer server; server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(endpoint)) return 1;
    const auto original = QProcessEnvironment::systemEnvironment();
    const QString session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QProcess gui;
    auto openGui = [&] {
        if (gui.state() != QProcess::NotRunning) return;
        auto environment = original;
        environment.insert("PT_SESSION_BUNDLE", rootPath(root));
        // Only this child receives launcher Qt/plugin and SDL configuration.
        environment.insert("QT_PLUGIN_PATH", QCoreApplication::applicationDirPath() + "/plugins");
        environment.insert("SDL_JOYSTICK_HIDAPI_STEAMDECK", "0");
        gui.setProcessEnvironment(environment);
        gui.start(QCoreApplication::applicationFilePath(), {"--session-gui", rootPath(root)});
        gui.waitForStarted(3000);
    };
    SessionState game = InspectSession(root);
    int gameHandle = -1;
    QString handleIdentity;
    auto refresh = [&] {
        auto current = InspectSession(root);
        if (current.identity != handleIdentity || (current.activity == Activity::Running && gameHandle < 0)) {
            if (gameHandle >= 0) close(gameHandle);
            gameHandle = current.activity == Activity::Running ? syscall(SYS_pidfd_open, current.pid, 0) : -1;
            // Bind the kernel handle to the inspected start identity, including
            // the race where a PID exits/recycles between inspection and open.
            if (gameHandle >= 0 && InspectSession(root).identity != current.identity) {
                close(gameHandle); gameHandle = -1;
            }
        }
        handleIdentity = current.identity;
        game = current;
    };
    auto stopGame = [&](int sig = SIGTERM) {
        refresh();
        if (gameHandle >= 0) {
            // Capture kernel handles for this session's descendants before
            // signalling. Never deliver a signal to a numeric, reusable PID.
            std::vector<int> members;
            pollfd leader{gameHandle, POLLIN, 0};
            if (poll(&leader, 1, 0) == 0 && getpgid(game.pid) == game.pid) {
                std::error_code ec;
                for (std::filesystem::directory_iterator it("/proc", ec), end; !ec && it != end; it.increment(ec)) {
                    bool numeric = false;
                    auto pid = QString::fromStdString(it->path().filename().string()).toLongLong(&numeric);
                    if (!numeric || pid == game.pid || getpgid(pid) != game.pid) continue;
                    int handle = syscall(SYS_pidfd_open, pid, 0);
                    if (handle >= 0) {
                        if (getpgid(pid) == game.pid) members.push_back(handle);
                        else close(handle);
                    }
                }
            }
            if (poll(&leader, 1, 0) == 0) {
                for (int member : members) syscall(SYS_pidfd_send_signal, member, sig, nullptr, 0);
                syscall(SYS_pidfd_send_signal, gameHandle, sig, nullptr, 0);
            }
            for (int member : members) close(member);
        }
    };
    bool shutdown = false;
    QElapsedTimer shutdownTime;
    QSet<QString> jobs;
    auto status = [&] {
        refresh();
        return QJsonObject{{"v",1},{"session",session},{"supervisor",qint64(getpid())},
            {"gui",gui.processId()},{"game",game.activity == Activity::Running ? game.pid : 0},
            {"identity",game.identity},{"jobs",jobs.size()},{"error",game.error}};
    };
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (auto* socket = server.nextPendingConnection()) {
            auto* deadline = new QTimer(socket); deadline->setSingleShot(true); deadline->start(2000);
            QObject::connect(deadline, &QTimer::timeout, socket, &QLocalSocket::abort);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (socket->bytesAvailable() > 65536) { socket->abort(); return; }
                if (!socket->canReadLine()) return;
                auto request = QJsonDocument::fromJson(socket->readLine()).object();
                QString error;
                const auto op = request["op"].toString();
                if (request["v"].toInt() != 1) error = "Unsupported IPC version.";
                else if (shutdown) error = "Session is shutting down.";
                else if (op == "reconnect") openGui();
                else if (op == "play" || op == "play-vr") {
                    if (!jobs.isEmpty()) error = "Managed work is active for this bundle.";
                    else {
                        game = op == "play" ? PlayFlat(root, supported, request["tryAnyway"].toBool()) : PlayVR(root, supported, request["tryAnyway"].toBool());
                        error = game.error;
                    }
                } else if (op == "job-begin") {
                    refresh();
                    if (gui.state() == QProcess::NotRunning || request["job"].toString().isEmpty() || request["job"].toString().size() > 128 || jobs.size() >= 256)
                        error = "Invalid GUI job registration.";
                    else if (game.activity != Activity::Stopped) error = "Game activity blocks managed work.";
                    else jobs.insert(request["job"].toString());
                } else if (op == "job-end") jobs.remove(request["job"].toString());
                else if (op == "shutdown") shutdown = true;
                else if (op == "stop-game") {
                    refresh();
                    if (request["identity"].toString() != game.identity || game.identity.isEmpty()) error = "Stale game identity.";
                    else stopGame();
                } else if (op != "status") error = "Unknown session operation.";
                auto response = status(); if (!error.isEmpty()) response["error"] = error;
                socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            });
        }
    });
    QObject::connect(&gui, &QProcess::finished, &app, [&] { jobs.clear(); });
    signal(SIGTERM, stop); signal(SIGINT, stop); signal(SIGHUP, stop);
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &app, [&] {
        refresh();
        if (stopping || shutdown) {
            if (!shutdownTime.isValid()) shutdownTime.start();
            const bool force = shutdownTime.elapsed() > 3000;
            stopGame(force ? SIGKILL : SIGTERM);
            if (gui.state() != QProcess::NotRunning) { if (force) gui.kill(); else gui.terminate(); }
        }
        if ((stopping || shutdown) && game.activity == Activity::Running) return;
        if (gui.state() == QProcess::NotRunning && game.activity == Activity::Stopped && jobs.isEmpty()) app.quit();
    });
    openGui(); timer.start(100);
    int result = app.exec();
    if (gameHandle >= 0) close(gameHandle);
    server.close(); QLocalServer::removeServer(endpoint);
    return result;
}
#else
int SessionEntry(int, char**, const std::vector<SupportedRuntime>&) { return 2; }
#endif
}
