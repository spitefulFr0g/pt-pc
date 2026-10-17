#include "preparation_fixture.h"
#include "session.h"
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <thread>
#include <chrono>

static void stopped(QProcess& child) {
    int status = 0;
    for (int i=0; i<500; ++i) {
        auto got = waitpid(child.processId(), &status, WUNTRACED | WNOHANG);
        if (got > 0) { assert(WIFSTOPPED(status)); return; }
        usleep(10000);
    }
    child.kill(); assert(false && "child did not reach bounded stop boundary");
}
static void preserved(Fixture& f) {
    assert(get(f.source / "chunk1.psarc") == "PSARsynthetic archive data");
    assert(get(f.source / "unowned.txt") == "leave source untouched");
    assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
    assert(get(f.root / "unowned bundle note.txt") == "unowned");
    assert(get(f.root / "linux/data/pt.ini") == "[network]\ncheck_updates=1\n\n[player]\nvolume=0.7\n");
    assert(get(f.root / "CUSA01127/chunk1.psarc") == "previous damaged archive");
    assert(get(f.root / "CUSA01127/texture.qar") == "previous damaged texture");
    assert(get(f.root / "CUSA01127/source.txt") == "previous source metadata");
    assert(get(f.root / "install-notes.txt") == "previous notes");
}
int main(int argc, char** argv) {
    if (argc == 3 && std::getenv("PT_TEST_HELPER_MODE")) {
        const std::string mode = std::getenv("PT_TEST_HELPER_MODE");
        if (mode == "hang") for (;;) std::this_thread::sleep_for(std::chrono::seconds(1));
        return mode == "failure" ? 23 : 0;
    }
    QCoreApplication app(argc, argv);
    if (argc == 5 && std::string(argv[1]) == "--prepare") {
        auto result = PrepareGame(argv[2], argv[3], {{"linux", argv[4], "fixture"}}, {}, [](const PreparationProgress& p) {
            if (qEnvironmentVariableIsSet("PT_TEST_STOP_COPY") && p.done) raise(SIGSTOP);
        });
        return result.ready ? 0 : 2;
    }
    if (argc == 2 && std::string(argv[1]) == "--uninspectable") {
        assert(prctl(PR_SET_NAME, "pt") == 0);
        assert(prctl(PR_SET_DUMPABLE, 0) == 0);
        raise(SIGSTOP); pause(); return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--proc-read-race") {
        Fixture f;
        assert(InspectSession(f.root).activity == Activity::Stopped);
        assert(!qEnvironmentVariableIsSet("PT_TEST_PROC_READ_RACE"));
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--space") {
        Fixture f;
        assert(fs::space(f.root).available < 3ull*1024*1024*1024);
        auto result = PrepareGame(f.root, f.source, f.supported);
        assert(!result.ready && result.error.contains("3 GiB"));
        assert(!PreparationNeedsRecovery(f.root));
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(get(f.source / "chunk1.psarc") == "PSARsynthetic archive data");
        assert(get(f.root / "unowned bundle note.txt") == "unowned");
        assert(get(f.root / "linux/data/pt.ini") == "[network]\ncheck_updates=1\n\n[player]\nvolume=0.7\n");
        std::cout << "PASS actual fs::space insufficient-space preflight: available=" << fs::space(f.root).available << std::endl; return 0;
    }
    for (const std::string mode : {"failure", "hang"}) {
        qputenv("PT_LAUNCHER_CONTROLLED_HELPER", QCoreApplication::applicationFilePath().toUtf8());
        qputenv("PT_TEST_HELPER_MODE", mode.c_str());
        Fixture f;
        const auto pkg = f.temporary.path().toStdString() + "/synthetic.pkg";
        put(pkg, QByteArray("\x7f" "CNT\0", 4));
        int cancelChecks = 0;
        auto result = PrepareGame(f.root, fs::u8path(pkg), f.supported,
            [&] { return mode == "hang" && ++cancelChecks >= 3; });
        if (mode == "failure") assert(!result.ready && !result.cancelled && result.error.contains("Package helper failed"));
        else assert(!result.ready && result.cancelled);
        assert(get(fs::u8path(pkg)) == QByteArray("\x7f" "CNT\0", 4));
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(get(f.root / "unowned bundle note.txt") == "unowned");
        assert(!PreparationNeedsRecovery(f.root));
        qunsetenv("PT_TEST_HELPER_MODE"); qunsetenv("PT_LAUNCHER_CONTROLLED_HELPER");
        std::cout << "PASS controlled extractor " << mode << " preserves source and bundle\n";
    }
    assert(argc == 2);
    {
        QProcess child; auto env = QProcessEnvironment::systemEnvironment();
        env.insert("LD_PRELOAD", argv[1]); env.insert("PT_TEST_PROC_READ_RACE", "1");
        child.setProcessEnvironment(env);
        child.start(QCoreApplication::applicationFilePath(), {"--proc-read-race"});
        assert(child.waitForFinished(5000));
        std::cerr << child.readAllStandardError().toStdString();
        assert(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0);
        std::cout << "PASS process vanishes during cmdline read" << std::endl;
    }
    for (const auto& boundary : {"copy", "/new/CUSA01127/chunk1.psarc", "/linux/data/pt.ini", "/new/linux/data/pt.ini"}) {
        Fixture f;
        put(f.root / "CUSA01127/chunk1.psarc", "previous damaged archive");
        put(f.root / "CUSA01127/texture.qar", "previous damaged texture");
        put(f.root / "CUSA01127/source.txt", "previous source metadata");
        put(f.root / "install-notes.txt", "previous notes");
        auto env = QProcessEnvironment::systemEnvironment();
        if (std::string(boundary) == "copy") env.insert("PT_TEST_STOP_COPY", "1");
        else { env.insert("LD_PRELOAD", argv[1]); env.insert("PT_TEST_RENAME_BOUNDARY", boundary); }
        QProcess child; child.setProcessEnvironment(env);
        child.start(QCoreApplication::applicationFilePath(), {"--prepare", QString::fromStdString(f.root.string()), QString::fromStdString(f.source.string()), f.runtimeHash});
        assert(child.waitForStarted()); stopped(child);
        assert(PreparationNeedsRecovery(f.root));
        const auto journal = QJsonDocument::fromJson(get(f.root / ".pt-preparation.json")).object();
        assert(journal["phase"].toString() == (std::string(boundary) == "copy" ? "staging" : "committing"));
        bool wrote = false; QString error;
        assert(!WithStoppedBundle(f.root, [&]{ wrote=true; }, &error) && !wrote && error.contains("Another launcher operation"));
        auto play = PlayFlat(f.root, f.supported); assert(play.error.contains("Another launcher operation"));
        auto concurrent = PrepareGame(f.root, f.source, f.supported); assert(!concurrent.ready && concurrent.error.contains("Another launcher operation"));
        child.kill(); assert(child.waitForFinished(5000) && child.exitStatus() == QProcess::CrashExit);
        auto recovered = RecoverPreparation(f.root);
        if (!recovered.error.isEmpty()) std::cerr << recovered.error.toStdString() << '\n';
        assert(recovered.error.isEmpty()); assert(!PreparationNeedsRecovery(f.root)); preserved(f);
        assert(PrepareGame(f.root, f.source, f.supported).ready);
        std::cout << "PASS SIGKILL/recovery/retry and concurrent Play/write/prepare: " << boundary << std::endl;
    }
    {
        Fixture f; QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {"--uninspectable"});
        assert(child.waitForStarted()); stopped(child);
        std::error_code ec; const auto unreadable = fs::read_symlink("/proc/" + std::to_string(child.processId()) + "/exe", ec);
        assert(ec == std::errc::permission_denied);
        assert(!get("/proc/" + std::to_string(child.processId()) + "/cmdline").isEmpty());
        auto state = InspectSession(f.root);
        assert(state.activity == Activity::Uncertain);
        bool wrote = false; QString error;
        assert(!WithStoppedBundle(f.root, [&]{wrote=true;}, &error) && !wrote);
        assert(PlayFlat(f.root, f.supported).activity == Activity::Uncertain);
        assert(!PrepareGame(f.root, f.source, f.supported).error.isEmpty());
        assert(!RecoverPreparation(f.root).error.isEmpty());
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(get(f.source / "chunk1.psarc") == "PSARsynthetic archive data");
        child.kill(); assert(child.waitForFinished(5000));
        std::cout << "PASS genuinely uninspectable process fails closed\n";
    }
}
