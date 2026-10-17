#include "session.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <signal.h>

namespace fs = std::filesystem;

static void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << bytes;
    assert(out.good());
}

static std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

static void copyFile(const fs::path& from, const fs::path& to) {
    fs::create_directories(to.parent_path());
    fs::copy_file(from, to, fs::copy_options::overwrite_existing);
}

static std::string emptyPsarc() {
    std::string bytes(64, '\0');
    std::memcpy(bytes.data(), "PSAR", 4);
    std::memcpy(bytes.data() + 8, "zlib", 4);
    const auto be32 = [&](size_t offset, uint32_t value) {
        bytes[offset] = char(value >> 24); bytes[offset + 1] = char(value >> 16);
        bytes[offset + 2] = char(value >> 8); bytes[offset + 3] = char(value);
    };
    be32(12, 64); // 32-byte header, one 30-byte entry, one 2-byte block-size value
    be32(16, 30);
    be32(20, 1); // empty manifest entry
    be32(24, 65536);
    bytes[57] = 0; bytes[58] = 0; bytes[59] = 0; bytes[60] = 0; bytes[61] = 64; // entry data offset
    return bytes;
}

static bool contains(const fs::path& path, const std::string& text) {
    return read(path).find(text) != std::string::npos;
}

static int runCase(const fs::path& package, const fs::path& fixture,
                   const fs::path& runtimeManifest,
                   const fs::path& root, const std::string& scenario) {
    const fs::path linuxDir = root / "linux";
    fs::create_directories(linuxDir);
    copyFile(fixture, linuxDir / "pt");
    const fs::path loader = linuxDir / "libopenxr_loader.so.1";
    if (scenario != "missing-loader") copyFile(package / "libopenxr_loader.so.1", loader);
    if (scenario == "failing-loader") write(loader, "not an ELF library\n");

    const fs::path game = root / "CUSA01127";
    write(game / "chunk1.psarc", emptyPsarc());
    std::string qar(64, '\0');
    qar[0x16 + 64 - 0x24] = 'a'; qar[0x17 + 64 - 0x24] = 'q';
    write(game / "texture.qar", qar);
    write(game / "pathid_list_ps4.bin", std::string(40, 'x') + "pt14_hallway");

    QFile runtime(QString::fromStdU32String((linuxDir / "pt").u32string()));
    assert(runtime.open(QIODevice::ReadOnly));
    const QByteArray hash = QCryptographicHash::hash(runtime.readAll(), QCryptographicHash::Sha256).toHex();
    const fs::path gameLog = linuxDir / "data" / "pt.log";
    const fs::path runtimeLog = root / "xr-test-runtime.log";
    fs::path controlledManifest = root / "active-runtime.json";
    if (scenario == "headless-runtime") {
        copyFile(runtimeManifest, controlledManifest);
        // The source manifest's relative path is valid only in its build directory.
        // Point the copied registration at the same test runtime shared object.
        const auto json = read(controlledManifest);
        const auto marker = std::string("\"library_path\": \"./");
        const auto at = json.find(marker);
        assert(at != std::string::npos);
        const auto end = json.find('"', at + marker.size());
        assert(end != std::string::npos);
        const auto library = runtimeManifest.parent_path() / json.substr(at + marker.size(), end - (at + marker.size()));
        const auto replacement = std::string("\"library_path\": \"") + library.string() + "\"";
        auto updated = json;
        updated.replace(at, end + 1 - at, replacement);
        write(controlledManifest, updated);
    } else if (scenario == "failing-runtime") {
        write(controlledManifest, "{\"file_format_version\":\"1.0.0\",\"runtime\":{\"name\":\"controlled failing runtime\",\"library_path\":\"/lib/x86_64-linux-gnu/libc.so.6\"}}\n");
    } else {
        // An explicitly missing registration prevents this process from consulting
        // or changing the host's active runtime selection.
        controlledManifest = root / "missing-runtime.json";
    }

    qputenv("XR_RUNTIME_JSON", QByteArray::fromStdString(controlledManifest.string()));
    qputenv("PT_XRTEST_LOG", QByteArray::fromStdString(runtimeLog.string()));
    qputenv("LD_LIBRARY_PATH", "/nonexistent/developer-sdk");
    qputenv("LD_PRELOAD", "/nonexistent/developer-injection.so");
    std::vector<pt::launcher::SupportedRuntime> supported{{"linux", hash.toStdString(), "controlled", true}};
    const auto result = pt::launcher::PlayVR(root, supported, true);
    const auto inspected = pt::launcher::InspectBundle(root, supported);
    assert(pt::launcher::CheckReadiness(inspected, pt::launcher::LaunchMode::Flat).state == pt::launcher::ReadinessState::Ready);
    if (scenario == "missing-loader") {
        assert(result.error.contains("bundled OpenXR loader is missing"));
        return 0;
    }

    const std::string expected = scenario == "headless-runtime" ? "vr: runtime pt-port headless test runtime" :
        scenario == "missing-loader" ? "libopenxr_loader.so.1 was not found" :
        scenario == "failing-loader" ? "libopenxr_loader.so.1 was not found" :
        "XR_ERROR_RUNTIME_UNAVAILABLE";
    for (int i = 0; i < 300 && !contains(gameLog, expected); ++i) QThread::msleep(100);
    const bool observed = contains(gameLog, expected);
    const auto session = pt::launcher::InspectSession(root);
    if (session.activity == pt::launcher::Activity::Running && session.pid > 0) {
        ::kill(static_cast<pid_t>(session.pid), SIGTERM);
        for (int i = 0; i < 50 && pt::launcher::InspectSession(root).activity != pt::launcher::Activity::Stopped; ++i)
            QThread::msleep(100);
    }
    if (!observed) {
        std::fprintf(stderr, "%s did not reach expected game log state '%s'; log follows:\n%s\n",
                     scenario.c_str(), expected.c_str(), read(gameLog).c_str());
        return 2;
    }
    if (scenario == "headless-runtime" && !contains(runtimeLog, "instance created for 'P.T. (pt-port)'")) {
        std::fprintf(stderr, "the headless runtime did not receive the packaged game: %s\n", read(runtimeLog).c_str());
        return 3;
    }
    return 0;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 4) return 64;
    const fs::path package = fs::path(QString::fromLocal8Bit(argv[1]).toStdU32String());
    const fs::path fixture = fs::path(QString::fromLocal8Bit(argv[2]).toStdU32String());
    const fs::path runtimeManifest = fs::path(QString::fromLocal8Bit(argv[3]).toStdU32String());
    if (!fs::is_regular_file(package / "pt") || !fs::is_regular_file(package / "libopenxr_loader.so.1") ||
        !fs::is_regular_file(fixture) || !fs::is_regular_file(runtimeManifest)) {
        std::fprintf(stderr, "packaged game, OpenXR loader, controlled runtime fixture, or headless manifest is missing\n");
        return 65;
    }
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 66;
    const fs::path base = fs::path(temporary.path().toStdU32String());
    for (const auto* scenario : {"headless-runtime", "missing-loader", "failing-loader", "missing-runtime", "failing-runtime"}) {
        const fs::path root = base / scenario;
        const int result = runCase(package, fixture, runtimeManifest, root, scenario);
        if (result) return result;
        std::printf("PASS launcher VR boundary: %s\n", scenario);
    }
}
