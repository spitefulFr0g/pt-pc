#include "readiness.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QSettings>
#include <QCoreApplication>
#include <cstdio>
#ifdef __linux__
#include <signal.h>
#endif
#include <algorithm>

namespace pt::launcher {
namespace {
const RuntimeInspection& matchingRuntime(const BundleInspection& b) {
#ifdef _WIN32
    return b.windows_runtime;
#else
    return b.linux_runtime;
#endif
}
ModeReadiness blocked(LaunchMode mode, const QString& why) { return {mode, ReadinessState::Blocked, why}; }
ModeReadiness unknown(LaunchMode mode, const QString& why) { return {mode, ReadinessState::Unknown, why}; }
ModeReadiness ready(LaunchMode mode, const QString& why) { return {mode, ReadinessState::Ready, why}; }

QString activeManifest() {
    const auto env = QProcessEnvironment::systemEnvironment().value("XR_RUNTIME_JSON");
    if (!env.isEmpty()) return env;
    const auto environment = QProcessEnvironment::systemEnvironment();
    QString home = environment.value("XDG_CONFIG_HOME");
    if (home.isEmpty()) home = QDir::homePath() + "/.config";
    QStringList paths{home};
    paths += environment.value("XDG_CONFIG_DIRS", "/etc/xdg").split(':', Qt::SkipEmptyParts);
    paths += "/etc";
    for (const auto& directory : paths) {
        // Supported target is Linux x86_64, matching the Khronos XR_ARCH_ABI.
        for (const auto& name : {"active_runtime.x86_64.json", "active_runtime.json"}) {
            const auto path = QDir(directory).filePath(QString("openxr/1/") + name);
            if (QFileInfo::exists(path)) return path;
        }
    }
    return {};
}
}

static ModeReadiness InspectReadiness(const BundleInspection& bundle, LaunchMode mode, int probeTimeoutMs) {
    if (mode == LaunchMode::Flat) {
        if (!bundle.archives) return blocked(mode, "Recognized game archives are missing. Add or repair the game files.");
        if (!matchingRuntime(bundle).present) return blocked(mode, "The game runtime is missing. Restore a runtime, then Retry.");
        if (!matchingRuntime(bundle).verified) return unknown(mode, "This game runtime build is unverified. Choose Try Play anyway to attempt a flat launch.");
        return ready(mode, "Flat launch prerequisites are ready.");
    }
    if (!bundle.archives) return blocked(mode, "Recognized game archives are missing. Add or repair the game files.");
    if (!matchingRuntime(bundle).present) return blocked(mode, "The game runtime is missing. Restore a runtime, then Retry.");
    if (!matchingRuntime(bundle).verified) return unknown(mode, "This game runtime build is unverified. VR capability is unknown; choose Try VR anyway to attempt launch.");
    if (!matchingRuntime(bundle).supports_vr) return blocked(mode, "This matching game runtime was built without OpenXR support. Flat play remains available.");
#ifdef _WIN32
    const auto loader = bundle.root / "windows" / "openxr_loader.dll";
    if (!QFileInfo(QString::fromStdU32String(loader.u32string())).isFile())
        return blocked(mode, "The bundled OpenXR loader is missing. Repair the matching runtime package.");
    QSettings machine("HKEY_LOCAL_MACHINE\\SOFTWARE\\Khronos\\OpenXR\\1", QSettings::NativeFormat);
    QSettings user("HKEY_CURRENT_USER\\SOFTWARE\\Khronos\\OpenXR\\1", QSettings::NativeFormat);
    QString manifest = QProcessEnvironment::systemEnvironment().value("XR_RUNTIME_JSON");
    if (manifest.isEmpty()) manifest = user.value("ActiveRuntime").toString();
    if (manifest.isEmpty()) manifest = machine.value("ActiveRuntime").toString();
    if (manifest.isEmpty()) return blocked(mode, "No active OpenXR runtime is registered. Configure one in its vendor app, then Retry.");
    QFile runtimeFile(manifest);
    if (!runtimeFile.open(QIODevice::ReadOnly) || runtimeFile.size() < 0 || runtimeFile.size() > 64 * 1024)
        return unknown(mode, "The active OpenXR runtime registration could not be read within the probe. Choose Try VR anyway to attempt launch.");
    const auto parsed = QJsonDocument::fromJson(runtimeFile.read(64 * 1024 + 1));
    const auto object = parsed.object().value("runtime").toObject();
    const QString library = object.value("library_path").toString();
    if (!parsed.isObject() || library.isEmpty()) return unknown(mode, "The active OpenXR runtime registration is not recognized. Choose Try VR anyway to attempt launch.");
    if (!library.contains('/') && !library.contains('\\'))
        return unknown(mode, "The runtime library uses loader search paths; availability is unknown. Choose Try VR anyway.");
    const QString libraryPath = QFileInfo(library).isAbsolute() ? library : QFileInfo(manifest).dir().absoluteFilePath(library);
    if (!QFileInfo(libraryPath).isFile())
        return blocked(mode, "The registered OpenXR runtime library is missing. Repair its runtime, then Retry.");
    return unknown(mode, "The matching runtime, bundled OpenXR loader, and active runtime registration were found. Headset connection and runtime suitability remain unknown; choose Try VR anyway to attempt launch.");
#elif defined(__linux__)
    const QString manifest = activeManifest();
    if (manifest.isEmpty()) return unknown(mode, "Active OpenXR runtime discovery is inconclusive. Check your runtime configuration, then Retry or choose Try VR anyway.");
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly)) return unknown(mode, "The active OpenXR runtime registration could not be read within the probe. Choose Try VR anyway to attempt launch.");
    if (file.size() < 0 || file.size() > 64 * 1024) return unknown(mode, "The active OpenXR runtime registration is outside the bounded probe size. Choose Try VR anyway to attempt launch.");
    const auto parsed = QJsonDocument::fromJson(file.read(64 * 1024 + 1));
    const auto runtime = parsed.object().value("runtime").toObject();
    const QString library = runtime.value("library_path").toString();
    if (!parsed.isObject() || library.isEmpty()) return unknown(mode, "The active OpenXR runtime registration is not recognized. Choose Try VR anyway to attempt launch.");
    if (!library.contains('/') && !library.contains('\\'))
        return unknown(mode, "The runtime library uses loader search paths; availability is unknown. Choose Try VR anyway.");
    const auto libraryPath = QFileInfo(library).isAbsolute() ? library : QFileInfo(manifest).dir().absoluteFilePath(library);
    if (!QFileInfo(libraryPath).isFile()) return blocked(mode, "The active OpenXR runtime library is missing. Repair its runtime, then Retry.");

    QElapsedTimer timer; timer.start();
    QProcess loaderProbe;
    loaderProbe.setProgram(QStandardPaths::findExecutable("ldconfig"));
    if (loaderProbe.program().isEmpty()) return unknown(mode, "The system loader inventory is unavailable. Choose Try VR anyway to attempt launch.");
    loaderProbe.setArguments({"-p"});
    loaderProbe.setProcessChannelMode(QProcess::MergedChannels);
    loaderProbe.start();
    const int boundedTimeout = qBound(1, probeTimeoutMs, 2000);
    if (!loaderProbe.waitForStarted(std::min(50, boundedTimeout)))
        return unknown(mode, "The OpenXR loader inventory did not start promptly. Choose Try VR anyway to attempt launch.");
    QByteArray inventory;
    while (loaderProbe.state() != QProcess::NotRunning && timer.elapsed() < boundedTimeout) {
        loaderProbe.waitForReadyRead(std::min(10, std::max(1, boundedTimeout - int(timer.elapsed()))));
        inventory += loaderProbe.read(64 * 1024 + 1);
        if (inventory.size() > 64 * 1024) {
            loaderProbe.kill(); loaderProbe.waitForFinished(50);
            return unknown(mode, "The OpenXR loader inventory exceeded its output limit. Choose Try VR anyway.");
        }
    }
    if (loaderProbe.state() != QProcess::NotRunning) {
        loaderProbe.kill(); loaderProbe.waitForFinished(50);
        return unknown(mode, "The OpenXR loader inventory timed out. Choose Try VR anyway to attempt launch.");
    }
    inventory += loaderProbe.read(64 * 1024 + 1);
    if (inventory.size() > 64 * 1024) return unknown(mode, "The OpenXR loader inventory exceeded its output limit. Choose Try VR anyway.");
    if (loaderProbe.exitStatus() != QProcess::NormalExit || loaderProbe.exitCode() != 0)
        return unknown(mode, "The OpenXR loader inventory could not be confirmed. Choose Try VR anyway to attempt launch.");
    if (!inventory.contains("libopenxr_loader.so.1"))
        return unknown(mode, "The system inventory does not establish OpenXR loader availability; private loader discovery remains unknown. Choose Try VR anyway to attempt launch.");
    return unknown(mode, "The supported game runtime, OpenXR loader, and active runtime registration were found. Headset connection and runtime suitability remain unknown; choose Try VR anyway to attempt launch.");
#else
    Q_UNUSED(probeTimeoutMs)
    return unknown(mode, "VR readiness is not probed on this platform. Choose Try VR anyway to attempt launch.");
#endif
}
#ifndef PT_READINESS_PROBE
ModeReadiness CheckReadiness(const BundleInspection& bundle, LaunchMode mode, int probeTimeoutMs) {
    // Pure bundle facts need no host I/O. All host probing, including stat/open,
    // runs in a disposable process so even a stalled filesystem has a deadline.
    if (mode == LaunchMode::Flat || !bundle.archives || !matchingRuntime(bundle).present ||
        !matchingRuntime(bundle).verified || !matchingRuntime(bundle).supports_vr)
        return InspectReadiness(bundle, mode, probeTimeoutMs);
    QProcess probe;
    probe.setProgram(QCoreApplication::applicationDirPath() + "/pt-launcher-readiness-probe"
#ifdef _WIN32
        ".exe"
#endif
    );
    probe.setArguments({QString::fromStdU32String(bundle.root.u32string()), QString::number(probeTimeoutMs)});
    probe.setProcessChannelMode(QProcess::SeparateChannels);
#ifdef __linux__
    probe.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
#endif
    QElapsedTimer timer; timer.start();
    const int budget = qBound(1, probeTimeoutMs, 2000) + 100; // includes child cleanup
    probe.start();
    QByteArray output, diagnostics;
    bool exceeded = false;
    if (probe.waitForStarted(budget)) {
        while (probe.state() != QProcess::NotRunning && timer.elapsed() < budget) {
            probe.waitForReadyRead(std::min(10, std::max(1, budget - int(timer.elapsed()))));
            output += probe.read(8193);
            diagnostics += probe.readAllStandardError();
            if (output.size() + diagnostics.size() > 8192) { exceeded = true; break; }
        }
    }
    if (probe.state() != QProcess::NotRunning) {
#ifdef __linux__
        // The helper's inventory child belongs to the same private session.
        ::kill(-probe.processId(), SIGKILL);
#endif
        probe.kill();
        probe.waitForFinished(100);
        return unknown(mode, exceeded ? "The readiness probe exceeded its output limit. Choose Try VR anyway." :
            "The readiness probe timed out. Choose Try VR anyway.");
    }
    output += probe.read(8193);
    diagnostics += probe.readAllStandardError();
    if (output.size() + diagnostics.size() > 8192 || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0)
        return unknown(mode, "The readiness probe could not be confirmed. Choose Try VR anyway.");
    const auto result = QJsonDocument::fromJson(output).object();
    const int state = result.value("state").toInt(-1);
    if (state < 0 || state > int(ReadinessState::Unknown) || !result.value("explanation").isString())
        return unknown(mode, "The readiness probe is unavailable. Choose Try VR anyway.");
    return {mode, ReadinessState(state), result.value("explanation").toString()};
}
#endif

}

#ifdef PT_READINESS_PROBE
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 3) return 2;
    pt::launcher::BundleInspection bundle;
    bundle.root = std::filesystem::path(QString::fromLocal8Bit(argv[1]).toStdU32String());
    bundle.archives = true;
    bundle.windows_runtime.present = bundle.windows_runtime.verified = bundle.windows_runtime.supports_vr = true;
    bundle.linux_runtime = bundle.windows_runtime;
    auto result = pt::launcher::InspectReadiness(bundle, pt::launcher::LaunchMode::VR, QString::fromLocal8Bit(argv[2]).toInt());
    auto bytes = QJsonDocument(QJsonObject{{"state", int(result.state)}, {"explanation", result.explanation}}).toJson(QJsonDocument::Compact);
    return std::fwrite(bytes.constData(), 1, bytes.size(), stdout) == size_t(bytes.size()) ? 0 : 3;
}
#endif
