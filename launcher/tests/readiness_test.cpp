#include "readiness.h"
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <QCoreApplication>
#ifdef __linux__
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
static void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    assert(temporary.isValid());
    const fs::path root(temporary.path().toStdU32String());
    pt::launcher::BundleInspection bundle;
    bundle.root = root;
    bundle.archives = true;
#ifdef _WIN32
    auto& matchingRuntime = bundle.windows_runtime;
#else
    auto& matchingRuntime = bundle.linux_runtime;
#endif
    matchingRuntime.present = true;
    matchingRuntime.verified = true;
    matchingRuntime.supports_vr = true;

    auto flat = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::Flat);
    assert(flat.state == pt::launcher::ReadinessState::Ready);
    auto unsupportedBundle = bundle;
#ifdef _WIN32
    auto& unsupportedRuntime = unsupportedBundle.windows_runtime;
#else
    auto& unsupportedRuntime = unsupportedBundle.linux_runtime;
#endif
    unsupportedRuntime.verified = false;
    unsupportedRuntime.present = true;
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::Flat).state == pt::launcher::ReadinessState::Unknown);
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Unknown);
    unsupportedRuntime.present = false;
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Blocked);
    unsupportedBundle = bundle;
#ifdef _WIN32
    unsupportedBundle.windows_runtime.supports_vr = false;
#else
    unsupportedBundle.linux_runtime.supports_vr = false;
#endif
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Blocked);
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::Flat).state == pt::launcher::ReadinessState::Ready);

#ifdef __linux__
    const auto manifest = root / "active_runtime.json";
    write(root / "libopenxr_runtime.so", "controlled runtime fixture");
    write(manifest, "{\"runtime\":{\"name\":\"controlled fixture\",\"library_path\":\"./libopenxr_runtime.so\"}}\n");
    qputenv("XR_RUNTIME_JSON", QString::fromStdU32String(manifest.u32string()).toLocal8Bit());
    auto missingLoader = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(missingLoader.state == pt::launcher::ReadinessState::Blocked);
    assert(pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::Flat).state == pt::launcher::ReadinessState::Ready);
    write(root / "linux/libopenxr_loader.so.1", "controlled loader fixture");
    auto unknown = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(unknown.state == pt::launcher::ReadinessState::Unknown);
    assert(unknown.explanation.contains("Headset connection"));
    write(manifest, std::string(65537, 'x'));
    auto oversized = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(oversized.state == pt::launcher::ReadinessState::Unknown && oversized.explanation.contains("size"));
    write(manifest, "{\"runtime\":{\"library_path\":\"./libopenxr_runtime.so\"}}");
    fs::permissions(manifest, fs::perms::none);
    if (geteuid() != 0) {
        auto inaccessible = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
        assert(inaccessible.state == pt::launcher::ReadinessState::Unknown && inaccessible.explanation.contains("could not be read"));
    }
    fs::permissions(manifest, fs::perms::owner_read | fs::perms::owner_write);
    qunsetenv("XR_RUNTIME_JSON");
    qputenv("XDG_CONFIG_HOME", QString::fromStdU32String((root / "config").u32string()).toLocal8Bit());
    write(root / "config/openxr/1/active_runtime.x86_64.json", "{\"runtime\":{\"library_path\":\"" + (root / "libopenxr_runtime.so").string() + "\"}}");
    write(root / "config/openxr/1/active_runtime.json", "invalid fallback must not override architecture-specific manifest");
    auto xdg = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(xdg.state == pt::launcher::ReadinessState::Unknown && xdg.explanation.contains("Headset connection"));
    qputenv("XR_RUNTIME_JSON", QString::fromStdU32String(manifest.u32string()).toLocal8Bit());
    fs::remove(manifest);
    assert(mkfifo(manifest.c_str(), 0600) == 0);
    QElapsedTimer timer; timer.start();
    auto fifo = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR, 60);
    assert(fifo.state == pt::launcher::ReadinessState::Unknown);
    assert(fifo.explanation.contains("timed out"));
    assert(timer.elapsed() >= 50 && timer.elapsed() < 500);
    assert(waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD);
#endif
}
