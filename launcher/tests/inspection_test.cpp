#include "inspection.h"
#include <QApplication>
#include <QTimer>
#include <QTemporaryDir>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static void write(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary); out << bytes;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTimer::singleShot(0, [&] {
        QTemporaryDir workspace;
        assert(workspace.isValid());
        auto base = fs::path(workspace.path().toStdU32String());
        auto root = base / "pt issue3 é spaced bundle";
        auto game = root / "CUSA01127";
        write(game / "chunk1.psarc", "PSAR" + std::string(60, '\0'));
        std::string qar(64, '\0'); qar[64 - 0x24 + 0x16] = 'a'; qar[64 - 0x24 + 0x17] = 'q';
        write(game / "texture.qar", qar);
        write(game / "pathid_list_ps4.bin", std::string(40, 'x') + "pt14_hallway");
        write(root / "source.pkg", std::string("\x7f" "CNT") + std::string(60, '\0'));
        write(root / "saves" / "player.sav", "player bytes");
        write(root / "mods" / "custom.txt", "mod bytes");
        write(root / "linux" / "pt", "unrecognized runtime");
        write(root / "linux" / "data" / "pt.log", "old log");
        write(root / "windows" / "pt.exe", "unrecognized runtime");
        write(root / "windows" / "data" / "settings.ini", "old settings");
        auto before = fs::file_size(game / "chunk1.psarc");
        auto inspected = pt::launcher::InspectBundle(root);
        assert(inspected.archives && inspected.path_list);
        assert(inspected.source && inspected.saves && inspected.mods);
        assert(inspected.linux_runtime.present && !inspected.linux_runtime.verified);
        assert(inspected.windows_runtime.present && !inspected.windows_runtime.verified);
        assert(inspected.linux_runtime.logs && inspected.windows_runtime.settings);
        assert(!inspected.can_integrate_settings);
        auto supported = pt::launcher::InspectBundle(root, {{"linux", "758f09d6bfea2f5c0c77d321d36712b4e4ba741c6930213636e88fc4733027a3", "fixture-1"}});
        assert(supported.linux_runtime.verified && supported.linux_runtime.version == "fixture-1");
        assert(!supported.windows_runtime.verified && supported.can_integrate_settings);
        assert(fs::file_size(game / "chunk1.psarc") == before);
        assert(fs::exists(root / "saves" / "player.sav"));
        assert(fs::file_size(root / "source.pkg") == 64);
        assert(fs::file_size(root / "mods" / "custom.txt") == 9);
        auto other = base / "pt issue3 independent";
        fs::create_directories(other);
        auto independent = pt::launcher::InspectBundle(other);
        assert(!independent.archives && !independent.linux_runtime.present);
        auto moved = base / "pt issue3 moved bundle";
        fs::rename(root, moved);
        auto moved_result = pt::launcher::InspectBundle(moved);
        assert(moved_result.archives && moved_result.linux_runtime.present);
        fs::remove(moved / "CUSA01127" / "texture.qar");
        auto missing = pt::launcher::InspectBundle(moved);
        assert(!missing.archives && missing.error.find("texture.qar") != std::string::npos);
        app.quit();
    });
    return app.exec();
}
