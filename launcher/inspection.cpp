#include "inspection.h"
#include "setup_core.h"
#include <filesystem>
#include <QCryptographicHash>
#include <QFile>

namespace pt::launcher {
namespace fs = std::filesystem;
static bool file(const fs::path& path) { std::error_code error; return fs::is_regular_file(path, error); }
static bool directory(const fs::path& path) { std::error_code error; return fs::is_directory(path, error); }
static RuntimeInspection runtime(const fs::path& root, const char* program, const char* platform,
    const std::vector<SupportedRuntime>& supported) {
    RuntimeInspection result;
    result.present = file(root / program);
    result.settings = file(root / "data" / "settings.ini");
    result.logs = file(root / "data" / "pt.log");
    if (result.present) {
        QFile binary(QString::fromStdU32String((root / program).u32string()));
        if (binary.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hash.addData(&binary)) return result;
            const auto digest = hash.result().toHex().toStdString();
            for (const auto& release : supported) {
                if (release.platform == platform && release.sha256 == digest) {
                    result.verified = true; result.version = release.version; result.supports_vr = release.supports_vr; break;
                }
            }
        }
    }
    result.detail = result.verified ? "Matching supported runtime " + result.version : result.present ? "Unverified runtime. Install a matching supported release before using integrated settings or Play." :
        "Runtime missing. Add the matching platform runtime to this bundle.";
    return result;
}
BundleInspection InspectBundle(const fs::path& root, const std::vector<SupportedRuntime>& supported) {
    BundleInspection result;
    result.root = fs::absolute(root).lexically_normal();
    if (!directory(result.root)) {
        result.error = "Bundle folder missing. Choose an existing portable bundle folder.";
        return result;
    }
    const fs::path game = result.root / "CUSA01127";
    // Reuse the unchanged upstream header checks for the approved extracted archives.
    const bool psarc = file(game / "chunk1.psarc") && setup::IsPsarc(game / "chunk1.psarc");
    const bool qar = file(game / "texture.qar") && setup::IsQar(game / "texture.qar");
    result.archives = psarc && qar;
    result.path_list = file(game / "pathid_list_ps4.bin") && setup::IsPathList(game / "pathid_list_ps4.bin");
    std::error_code source_error;
    for (fs::directory_iterator it(result.root, fs::directory_options::skip_permission_denied, source_error), end;
         !source_error && it != end; it.increment(source_error)) {
        if (it->is_regular_file(source_error) && setup::Lower(setup::PathUtf8(it->path().extension())) == ".pkg" &&
            setup::IsPackage(it->path())) { result.source = true; break; }
    }
    result.saves = directory(result.root / "saves");
    result.mods = directory(result.root / "mods");
    result.linux_runtime = runtime(result.root / "linux", "pt", "linux", supported);
    result.windows_runtime = runtime(result.root / "windows", "pt.exe", "windows", supported);
    result.can_integrate_settings = result.archives && (result.linux_runtime.verified || result.windows_runtime.verified);
    if (!psarc) result.error = "chunk1.psarc missing or unrecognized. Check CUSA01127 in this bundle.";
    else if (!qar) result.error = "texture.qar missing or unrecognized. Check CUSA01127 in this bundle.";
    return result;
}
}
