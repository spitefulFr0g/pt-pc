#include "readiness.h"
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
}

int main() {
    QTemporaryDir temporary;
    assert(temporary.isValid());
    const fs::path root(temporary.path().toStdU32String());
    pt::launcher::BundleInspection bundle;
    bundle.root = root;
    bundle.archives = true;
    bundle.linux_runtime.present = true;
    bundle.linux_runtime.verified = true;
    bundle.linux_runtime.supports_vr = true;

    auto flat = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::Flat);
    assert(flat.state == pt::launcher::ReadinessState::Ready);
    auto unsupportedBundle = bundle;
    unsupportedBundle.linux_runtime.verified = false;
    unsupportedBundle.linux_runtime.present = true;
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::Flat).state == pt::launcher::ReadinessState::Unknown);
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Unknown);
    unsupportedBundle.linux_runtime.present = false;
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Blocked);
    unsupportedBundle = bundle;
    unsupportedBundle.linux_runtime.supports_vr = false;
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::VR).state == pt::launcher::ReadinessState::Blocked);
    assert(pt::launcher::CheckReadiness(unsupportedBundle, pt::launcher::LaunchMode::Flat).state == pt::launcher::ReadinessState::Ready);

#ifdef __linux__
    const auto manifest = root / "active_runtime.json";
    write(root / "libopenxr_runtime.so", "controlled runtime fixture");
    write(manifest, "{\"runtime\":{\"name\":\"controlled fixture\",\"library_path\":\"./libopenxr_runtime.so\"}}\n");
    qputenv("XR_RUNTIME_JSON", QString::fromStdU32String(manifest.u32string()).toLocal8Bit());
    const auto tools = root / "tools";
    const auto loader = tools / "ldconfig";
    write(loader, "#!/bin/sh\nprintf '%s\\n' 'libopenxr_loader.so.1 (libc6,x86-64) => /lib/libopenxr_loader.so.1'\n");
    fs::permissions(loader, fs::perms::owner_exec, fs::perm_options::add);
    const auto oldPath = qgetenv("PATH");
    qputenv("PATH", QString::fromStdU32String(tools.u32string()).toLocal8Bit());
    auto unknown = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(unknown.state == pt::launcher::ReadinessState::Unknown);
    assert(unknown.explanation.contains("Headset connection"));

    write(loader, "#!/bin/sh\nexit 0\n");
    auto missingLoader = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(missingLoader.state == pt::launcher::ReadinessState::Blocked);

    write(loader, "#!/bin/sh\nsleep 2\n");
    QElapsedTimer timer; timer.start();
    auto timedOut = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR, 60);
    assert(timedOut.state == pt::launcher::ReadinessState::Unknown);
    assert(timer.elapsed() < 500);
    qputenv("PATH", oldPath);
#endif
}
