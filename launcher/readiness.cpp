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
    const auto user = QDir::homePath() + "/.config/openxr/1/active_runtime.json";
    if (QFileInfo::exists(user)) return user;
    const QStringList paths{ "/etc/xdg/openxr/1/active_runtime.json" };
    for (const auto& path : paths) if (QFileInfo::exists(path)) return path;
    return {};
}
}

ModeReadiness CheckReadiness(const BundleInspection& bundle, LaunchMode mode, int probeTimeoutMs) {
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
    QString manifest = user.value("ActiveRuntime").toString();
    if (manifest.isEmpty()) manifest = machine.value("ActiveRuntime").toString();
    if (manifest.isEmpty()) return blocked(mode, "No active OpenXR runtime is registered. Configure one in its vendor app, then Retry.");
    QFile runtimeFile(manifest);
    if (!runtimeFile.open(QIODevice::ReadOnly) || runtimeFile.size() < 0 || runtimeFile.size() > 64 * 1024)
        return unknown(mode, "The active OpenXR runtime registration could not be read within the probe. Choose Try VR anyway to attempt launch.");
    const auto parsed = QJsonDocument::fromJson(runtimeFile.readAll());
    const auto object = parsed.object().value("runtime").toObject();
    const QString library = object.value("library_path").toString();
    if (!parsed.isObject() || library.isEmpty()) return unknown(mode, "The active OpenXR runtime registration is not recognized. Choose Try VR anyway to attempt launch.");
    const QString libraryPath = QFileInfo(library).isAbsolute() ? library : QFileInfo(manifest).dir().absoluteFilePath(library);
    if (!QFileInfo(libraryPath).isFile())
        return blocked(mode, "The registered OpenXR runtime library is missing. Repair its runtime, then Retry.");
    return unknown(mode, "The matching runtime, bundled OpenXR loader, and active runtime registration were found. Headset connection and runtime suitability remain unknown; choose Try VR anyway to attempt launch.");
#elif defined(__linux__)
    const QString manifest = activeManifest();
    if (manifest.isEmpty()) return blocked(mode, "No active OpenXR runtime manifest was found. Configure one in your system, then Retry.");
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly)) return unknown(mode, "The active OpenXR runtime registration could not be read within the probe. Choose Try VR anyway to attempt launch.");
    if (file.size() < 0 || file.size() > 64 * 1024) return unknown(mode, "The active OpenXR runtime registration is outside the bounded probe size. Choose Try VR anyway to attempt launch.");
    const auto parsed = QJsonDocument::fromJson(file.readAll());
    const auto runtime = parsed.object().value("runtime").toObject();
    const QString library = runtime.value("library_path").toString();
    if (!parsed.isObject() || library.isEmpty()) return unknown(mode, "The active OpenXR runtime registration is not recognized. Choose Try VR anyway to attempt launch.");
    const auto libraryPath = QFileInfo(library).isAbsolute() ? library : QFileInfo(manifest).dir().absoluteFilePath(library);
    if (!QFileInfo(libraryPath).isFile()) return blocked(mode, "The active OpenXR runtime library is missing. Repair its runtime, then Retry.");

    QElapsedTimer timer; timer.start();
    QProcess loaderProbe;
    loaderProbe.setProgram(QStandardPaths::findExecutable("ldconfig"));
    if (loaderProbe.program().isEmpty()) return unknown(mode, "The system loader inventory is unavailable. Choose Try VR anyway to attempt launch.");
    loaderProbe.setArguments({"-p"});
    loaderProbe.start();
    const int boundedTimeout = qBound(1, probeTimeoutMs, 2000);
    if (!loaderProbe.waitForStarted(std::min(50, boundedTimeout)))
        return unknown(mode, "The OpenXR loader inventory did not start promptly. Choose Try VR anyway to attempt launch.");
    const int remaining = std::max(1, boundedTimeout - int(timer.elapsed()));
    if (!loaderProbe.waitForFinished(remaining)) {
        loaderProbe.kill(); loaderProbe.waitForFinished(50);
        return unknown(mode, "The OpenXR loader inventory timed out. Choose Try VR anyway to attempt launch.");
    }
    if (loaderProbe.exitStatus() != QProcess::NormalExit || loaderProbe.exitCode() != 0)
        return unknown(mode, "The OpenXR loader inventory could not be confirmed. Choose Try VR anyway to attempt launch.");
    if (!loaderProbe.readAllStandardOutput().contains("libopenxr_loader.so.1"))
        return blocked(mode, "libopenxr_loader.so.1 is missing. Install the OpenXR loader for this system, then Retry.");
    return unknown(mode, "The supported game runtime, OpenXR loader, and active runtime registration were found. Headset connection and runtime suitability remain unknown; choose Try VR anyway to attempt launch.");
#else
    Q_UNUSED(probeTimeoutMs)
    return unknown(mode, "VR readiness is not probed on this platform. Choose Try VR anyway to attempt launch.");
#endif
}
}
