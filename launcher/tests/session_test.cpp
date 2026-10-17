#include "session.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QProcess>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <signal.h>

namespace fs = std::filesystem;
static void write(const fs::path& p, const std::string& s) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << s; }
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc == 4 && (QString::fromLocal8Bit(argv[1]) == "--hold-lock" || QString::fromLocal8Bit(argv[1]) == "--crash-lock")) {
        const bool crash = QString::fromLocal8Bit(argv[1]) == "--crash-lock";
        QString error;
        bool held = pt::launcher::WithStoppedBundle(fs::path(QString::fromLocal8Bit(argv[2]).toStdU32String()), [&] {
            write(fs::path(QString::fromLocal8Bit(argv[3]).toStdU32String()), "held");
            if (crash) std::_Exit(44);
            QThread::msleep(1000);
        }, &error);
        return held ? 0 : 1;
    }
    QTemporaryDir temporary; assert(temporary.isValid());
    const auto root = fs::path(temporary.path().toStdU32String()) / "bundle with spaces";
    fs::create_directories(root / "linux");
    write(root / "linux" / "pt", "#!/bin/sh\nexec >/dev/null 2>&1\nprintf '%s\\n' \"$@\" > \"$(dirname \"$0\")/arguments.txt\"\nsleep 30 & wait\n");
    fs::permissions(root / "linux" / "pt", fs::perms::owner_exec, fs::perm_options::add);
    write(root / "CUSA01127" / "chunk1.psarc", "PSAR" + std::string(60, '\0'));
    std::string qar(64, '\0'); qar[64 - 0x24 + 0x16] = 'a'; qar[64 - 0x24 + 0x17] = 'q';
    write(root / "CUSA01127" / "texture.qar", qar);
    write(root / "linux" / "data" / "pt.ini", "[display]\nvr=true\n");
    write(root / "linux" / "data" / "pt.log", "old log");
    write(root / "saves" / "save.dat", "keep"); write(root / "mods" / "mod.dat", "keep");
    QFile binary(QString::fromStdU32String((root / "linux" / "pt").u32string())); assert(binary.open(QIODevice::ReadOnly));
    QCryptographicHash hash(QCryptographicHash::Sha256); assert(hash.addData(&binary));
    std::vector<pt::launcher::SupportedRuntime> supported{{"linux", hash.result().toHex().toStdString(), "fixture", true}};
    auto stopped = pt::launcher::InspectSession(root); assert(stopped.activity == pt::launcher::Activity::Stopped);
    assert(argc == 2);
    QProcess ui;
    ui.start(QString::fromLocal8Bit(argv[1]), {"--test-play-and-exit", QString::fromStdU32String(root.u32string()), "--test-supported-sha256", QString::fromStdString(supported[0].sha256)});
    assert(ui.waitForFinished(10000) && ui.exitCode() == 0);
    auto started = pt::launcher::InspectSession(root); assert(started.activity == pt::launcher::Activity::Running);
    QProcess reopenedUi;
    reopenedUi.start(QString::fromLocal8Bit(argv[1]), {"--test-reconnect", QString::fromStdU32String(root.u32string()), "--test-supported-sha256", QString::fromStdString(supported[0].sha256)});
    assert(reopenedUi.waitForFinished(10000) && reopenedUi.exitCode() == 0);
    assert(started.pid > 0 && !started.identity.isEmpty());
    auto repeated = pt::launcher::PlayFlat(root, supported); assert(repeated.activity == pt::launcher::Activity::Running && repeated.pid == started.pid);
    auto reopened = pt::launcher::InspectSession(root); assert(reopened.activity == pt::launcher::Activity::Running && reopened.identity == started.identity);
    const auto alias = fs::path(temporary.path().toStdU32String()) / "bundle alias";
    fs::create_directory_symlink(root, alias);
    assert(pt::launcher::InspectSession(alias).identity == started.identity);
    bool mutated = false; QString error;
    assert(!pt::launcher::WithStoppedBundle(root, [&]{mutated = true;}, &error) && !mutated);
    assert(fs::exists(root / "saves" / "save.dat") && fs::exists(root / "mods" / "mod.dat"));
    assert(fs::file_size(root / "linux" / "data" / "pt.log") == 0);
    for (int i=0; i<100 && !fs::exists(root / "linux" / "arguments.txt"); ++i) QThread::msleep(20);
    std::ifstream arguments(root / "linux" / "arguments.txt"); std::string args((std::istreambuf_iterator<char>(arguments)), {});
    assert(args.find("--game\n") != std::string::npos && args.find("--no-vr\n") != std::string::npos);
    assert(args.find("--no-update-check\n") != std::string::npos);
    assert(args.find((root / "CUSA01127").string()) != std::string::npos);
    assert(args.find((root / "saves").string()) != std::string::npos);
    assert(args.find((root / "mods").string()) != std::string::npos);
    assert(args.find((root / "linux" / "data" / "pt.ini").string()) != std::string::npos);
    assert(args.find((root / "linux" / "data" / "pt.log").string()) != std::string::npos);
    std::ifstream settings(root / "linux" / "data" / "pt.ini"); std::string settingsText((std::istreambuf_iterator<char>(settings)), {});
    assert(settingsText == "[display]\nvr=true\n");
    kill(started.pid, SIGTERM);
    for (int i = 0; i < 100 && pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Running; ++i) QThread::msleep(20);
    assert(pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Stopped);
    qputenv("XR_RUNTIME_JSON", QString::fromStdU32String((root / "missing-active-runtime.json").u32string()).toLocal8Bit());
    auto vrUnknown = pt::launcher::CheckReadiness(pt::launcher::InspectBundle(root, supported), pt::launcher::LaunchMode::VR, 80);
    assert(vrUnknown.state == pt::launcher::ReadinessState::Unknown);
    auto flatStillReady = pt::launcher::CheckReadiness(pt::launcher::InspectBundle(root, supported), pt::launcher::LaunchMode::Flat);
    assert(flatStillReady.state == pt::launcher::ReadinessState::Ready);
    auto refusedVr = pt::launcher::PlayVR(root, supported, false);
    assert(refusedVr.activity == pt::launcher::Activity::Stopped && !refusedVr.error.isEmpty());
    fs::remove(root / "linux" / "arguments.txt");
    auto vrAttempt = pt::launcher::PlayVR(root, supported, true);
    assert(vrAttempt.activity == pt::launcher::Activity::Running);
    for (int i=0; i<100 && !fs::exists(root / "linux" / "arguments.txt"); ++i) QThread::msleep(20);
    std::ifstream vrArguments(root / "linux" / "arguments.txt"); std::string vrArgs((std::istreambuf_iterator<char>(vrArguments)), {});
    assert(vrArgs.find("--vr\n") != std::string::npos && vrArgs.find("--no-vr\n") == std::string::npos);
    assert(vrArgs.find("--no-update-check\n") != std::string::npos);
    assert(vrArgs.find((root / "CUSA01127").string()) != std::string::npos);
    assert(vrArgs.find((root / "saves").string()) != std::string::npos);
    assert(vrArgs.find((root / "mods").string()) != std::string::npos);
    assert(vrArgs.find((root / "linux" / "data" / "pt.ini").string()) != std::string::npos);
    assert(vrArgs.find((root / "linux" / "data" / "pt.log").string()) != std::string::npos);
    std::ifstream stillSameSettings(root / "linux" / "data" / "pt.ini");
    std::string stillSame((std::istreambuf_iterator<char>(stillSameSettings)), {});
    assert(stillSame == "[display]\nvr=true\n");
    kill(vrAttempt.pid, SIGTERM);
    for (int i=0; i<100 && pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Running; ++i) QThread::msleep(20);
    fs::remove(root / "linux" / "arguments.txt");
    QProcess vrUi;
    vrUi.start(QString::fromLocal8Bit(argv[1]), {"--test-vr-control", QString::fromStdU32String(root.u32string()), "--test-supported-sha256", QString::fromStdString(supported[0].sha256)});
    const bool vrUiFinished = vrUi.waitForFinished(10000);
    if (!vrUiFinished) { vrUi.kill(); vrUi.waitForFinished(); std::cerr << vrUi.readAllStandardOutput().constData() << vrUi.readAllStandardError().constData(); }
    assert(vrUiFinished && vrUi.exitCode() == 0);
    std::ifstream uiVrArguments(root / "linux" / "arguments.txt");
    std::string uiVrArgs((std::istreambuf_iterator<char>(uiVrArguments)), {});
    assert(uiVrArgs.find("--vr\n") != std::string::npos && uiVrArgs.find("--no-update-check\n") != std::string::npos);
    auto uiVrSession = pt::launcher::InspectSession(root); assert(uiVrSession.activity == pt::launcher::Activity::Running);
    kill(uiVrSession.pid, SIGTERM);
    for (int i=0; i<100 && pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Running; ++i) QThread::msleep(20);
    fs::remove(root / "linux" / "arguments.txt");
    QProcess flatUnknownUi;
    flatUnknownUi.start(QString::fromLocal8Bit(argv[1]), {"--test-flat-unknown-control", QString::fromStdU32String(root.u32string()), "--test-supported-sha256", QString::fromStdString(supported[0].sha256)});
    assert(flatUnknownUi.waitForFinished(10000) && flatUnknownUi.exitCode() == 0);
    std::ifstream unknownFlatArguments(root / "linux" / "arguments.txt");
    std::string unknownFlatArgs((std::istreambuf_iterator<char>(unknownFlatArguments)), {});
    assert(unknownFlatArgs.find("--no-vr\n") != std::string::npos && unknownFlatArgs.find("--vr\n") == std::string::npos);
    assert(unknownFlatArgs.find("--no-update-check\n") != std::string::npos);
    auto unknownFlatSession = pt::launcher::InspectSession(root); assert(unknownFlatSession.activity == pt::launcher::Activity::Running);
    kill(unknownFlatSession.pid, SIGTERM);
    for (int i=0; i<100 && pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Running; ++i) QThread::msleep(20);
    auto blockerInspection = pt::launcher::InspectBundle(root, supported);
    blockerInspection.linux_runtime.verified = false;
    assert(pt::launcher::CheckReadiness(blockerInspection, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Unknown);
    blockerInspection.linux_runtime.verified = true;
    blockerInspection.linux_runtime.supports_vr = false;
    assert(pt::launcher::CheckReadiness(blockerInspection, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Blocked);
    const auto lockMarker = root / "lock-held";
    QProcess holder;
    holder.start(QCoreApplication::applicationFilePath(), {"--hold-lock", QString::fromStdU32String(root.u32string()), QString::fromStdU32String(lockMarker.u32string())});
    for (int i=0; i<100 && !fs::exists(lockMarker); ++i) QThread::msleep(20);
    assert(fs::exists(lockMarker));
    assert(!pt::launcher::WithStoppedBundle(root, []{}, &error));
    assert(holder.waitForFinished(10000) && holder.exitCode() == 0);
    fs::remove(lockMarker);
    QProcess crashedHolder;
    crashedHolder.start(QCoreApplication::applicationFilePath(), {"--crash-lock", QString::fromStdU32String(root.u32string()), QString::fromStdU32String(lockMarker.u32string())});
    assert(crashedHolder.waitForFinished(10000) && crashedHolder.exitCode() == 44);
    assert(pt::launcher::WithStoppedBundle(root, []{}, &error));
    QProcess crashedUi;
    crashedUi.start(QString::fromLocal8Bit(argv[1]), {"--test-play-and-crash", QString::fromStdU32String(root.u32string()), "--test-supported-sha256", QString::fromStdString(supported[0].sha256)});
    assert(crashedUi.waitForFinished(10000) && crashedUi.exitCode() == 42);
    auto survivedCrash = pt::launcher::InspectSession(root);
    assert(survivedCrash.activity == pt::launcher::Activity::Running);
    kill(survivedCrash.pid, SIGTERM);
    for (int i = 0; i < 100 && pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Running; ++i) QThread::msleep(20);
    assert(pt::launcher::WithStoppedBundle(root, [&]{mutated = true;}, &error) && mutated);
    qint64 directPid = 0;
    assert(QProcess::startDetached(QString::fromStdU32String((root / "linux" / "pt").u32string()), {},
        QString::fromStdU32String((root / "linux").u32string()), &directPid));
    for (int i=0; i<100 && pt::launcher::InspectSession(root).activity != pt::launcher::Activity::Running; ++i) QThread::msleep(20);
    auto direct = pt::launcher::InspectSession(root);
    assert(direct.activity == pt::launcher::Activity::Running && direct.pid == directPid);
    assert(direct.identity != started.identity);
    auto stale = direct; stale.identity = started.identity;
    QString staleError;
    assert(!pt::launcher::ReturnToGame(stale, &staleError) && staleError == "No game window can be resumed.");
    auto againstDirect = pt::launcher::PlayFlat(root, supported);
    assert(againstDirect.activity == pt::launcher::Activity::Running && againstDirect.identity == direct.identity);
    assert(!pt::launcher::WithStoppedBundle(root, []{}, &error));
    qint64 competingPid = 0;
    assert(QProcess::startDetached(QString::fromStdU32String((root / "linux" / "pt").u32string()), {},
        QString::fromStdU32String((root / "linux").u32string()), &competingPid));
    for (int i=0; i<100 && pt::launcher::InspectSession(root).activity != pt::launcher::Activity::Uncertain; ++i) QThread::msleep(20);
    assert(pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Uncertain);
    assert(pt::launcher::PlayFlat(root, supported).activity == pt::launcher::Activity::Uncertain);
    assert(!pt::launcher::WithStoppedBundle(root, []{}, &error));
    const auto independent = fs::path(temporary.path().toStdU32String()) / "second bundle";
    fs::create_directories(independent);
    assert(pt::launcher::InspectSession(independent).activity == pt::launcher::Activity::Stopped);
    assert(pt::launcher::WithStoppedBundle(independent, []{}, &error));
    kill(directPid, SIGTERM);
    kill(competingPid, SIGTERM);
    for (int i=0; i<100 && pt::launcher::InspectSession(root).activity == pt::launcher::Activity::Running; ++i) QThread::msleep(20);
    fs::permissions(root / "linux" / "pt", fs::perms::owner_exec, fs::perm_options::remove);
    auto failure = pt::launcher::PlayFlat(root, supported);
    assert(!failure.error.isEmpty() && failure.activity == pt::launcher::Activity::Stopped);
    QProcess failedUi;
    failedUi.start(QString::fromLocal8Bit(argv[1]), {"--test-start-failure", QString::fromStdU32String(root.u32string()), "--test-supported-sha256", QString::fromStdString(supported[0].sha256)});
    assert(failedUi.waitForFinished(10000) && failedUi.exitCode() == 0);
    fs::permissions(root / "linux" / "pt", fs::perms::owner_exec, fs::perm_options::add);
}
