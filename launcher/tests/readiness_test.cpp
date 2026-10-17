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
    const auto oldLocale = qgetenv("LC_ALL");
    qputenv("LC_ALL", "C"); // helper startup diagnostics must not corrupt its result
    auto unknown = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(unknown.state == pt::launcher::ReadinessState::Unknown);
    assert(unknown.explanation.contains("Headset connection"));
    qputenv("LC_ALL", oldLocale);

    write(loader, "#!/bin/sh\nexit 0\n");
    auto missingLoader = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR);
    assert(missingLoader.state == pt::launcher::ReadinessState::Unknown);

    qputenv("PT_TEST_PROBE_PID", QString::fromStdU32String((root / "probe.pid").u32string()).toLocal8Bit());
    write(loader, "#!/bin/sh\nprintf '%s' \"$$\" > \"$PT_TEST_PROBE_PID\"\nexec /bin/sleep 2\n");
    QElapsedTimer timer; timer.start();
    auto timedOut = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR, 60);
    assert(timedOut.state == pt::launcher::ReadinessState::Unknown);
    assert(timedOut.explanation.contains("timed out"));
    assert(timer.elapsed() >= 50 && timer.elapsed() < 500);
    std::ifstream pidFile(root / "probe.pid"); int probePid = 0; pidFile >> probePid;
    assert(probePid > 0 && kill(probePid, 0) == -1 && errno == ESRCH);
    write(loader, "#!/bin/sh\nexec /usr/bin/yes libopenxr_loader.so.1\n");
    auto flooded = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR, 200);
    assert(flooded.state == pt::launcher::ReadinessState::Unknown);
    assert(flooded.explanation.contains("output limit"));
    write(loader, "#!/bin/sh\nprintf '%s' libopenxr_loader.so.1\n");
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
    qputenv("PATH", oldPath);
    fs::remove(manifest);
    assert(mkfifo(manifest.c_str(), 0600) == 0);
    timer.restart();
    auto fifo = pt::launcher::CheckReadiness(bundle, pt::launcher::LaunchMode::VR, 60);
    assert(fifo.state == pt::launcher::ReadinessState::Unknown);
    assert(fifo.explanation.contains("timed out"));
    assert(timer.elapsed() >= 50 && timer.elapsed() < 500);
    assert(waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD);
#endif
}
