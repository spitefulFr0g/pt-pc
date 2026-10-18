#include "session.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QFile>
#include <QCryptographicHash>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <signal.h>
namespace fs = std::filesystem;
static void write(const fs::path& p, const std::string& text) { fs::create_directories(p.parent_path()); std::ofstream(p) << text; }
static std::string read(const fs::path& p) { std::ifstream f(p); return {std::istreambuf_iterator<char>(f), {}}; }
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); assert(argc == 2);
    for (const auto* mode : {"--test-controller-flat", "--test-controller-vr"}) {
        QTemporaryDir temp; assert(temp.isValid());
        const auto root = fs::path(temp.path().toStdU32String()) / "controller bundle";
        write(root / "linux/pt", "#!/bin/sh\nexec >/dev/null 2>&1\nprintf 'start\\n' >> starts\nenv > environment\nprintf '%s\\n' \"$@\" > arguments\nsleep 30 & wait\n");
        fs::permissions(root / "linux/pt", fs::perms::owner_exec, fs::perm_options::add);
        write(root / "CUSA01127/chunk1.psarc", "PSAR" + std::string(60, '\0'));
        std::string qar(64, '\0'); qar[0x16 + 64 - 0x24] = 'a'; qar[0x17 + 64 - 0x24] = 'q';
        write(root / "CUSA01127/texture.qar", qar);
        QFile binary(QString::fromStdU32String((root / "linux/pt").u32string())); assert(binary.open(QIODevice::ReadOnly));
        const auto hash = QCryptographicHash::hash(binary.readAll(), QCryptographicHash::Sha256).toHex();
        QProcess ui;
        // A real bounded slow inventory keeps the asynchronous VR request pending.
        write(root / "runtime.so", "controlled runtime");
        write(root / "active-runtime.json", "{\"runtime\":{\"library_path\":\"./runtime.so\"}}");
        write(root / "tools/ldconfig", "#!/bin/sh\n/bin/sleep 0.2\n");
        fs::permissions(root / "tools/ldconfig", fs::perms::owner_exec, fs::perm_options::add);
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("XR_RUNTIME_JSON", QString::fromStdU32String((root / "active-runtime.json").u32string()));
        env.insert("PATH", QString::fromStdU32String((root / "tools").u32string()) + ":" + env.value("PATH"));
        env.insert("PT_CONTROLLER_ENV_SENTINEL", "original-game-environment");
        ui.setProcessEnvironment(env);
        ui.start(QString::fromLocal8Bit(argv[1]), {mode, QString::fromStdU32String(root.u32string()), "--test-supported-sha256", QString::fromLatin1(hash)});
        const bool finished = ui.waitForFinished(12000);
        if (!finished) { ui.kill(); ui.waitForFinished(); }
        auto state = pt::launcher::InspectSession(root);
        if (state.activity == pt::launcher::Activity::Running) kill(state.pid, SIGTERM);
        const auto output = ui.readAllStandardError();
        if (!finished || ui.exitStatus() != QProcess::NormalExit || ui.exitCode()) { fprintf(stderr, "%s: %s", mode, output.constData()); return 1; }
        assert(read(root / "linux/starts") == "start\nstart\n");
        const auto gameEnv = read(root / "linux/environment");
        assert(gameEnv.find("PT_CONTROLLER_ENV_SENTINEL=original-game-environment") != std::string::npos);
        assert(gameEnv.find("SDL_JOYSTICK_HIDAPI_STEAMDECK=") == std::string::npos);
        assert(gameEnv.find("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=") == std::string::npos);
        assert(gameEnv.find("SteamAppId=") == std::string::npos);
        const auto args = read(root / "linux/arguments");
        assert(args.find(mode == std::string("--test-controller-vr") ? "--vr\n" : "--no-vr\n") != std::string::npos);
    }
}
