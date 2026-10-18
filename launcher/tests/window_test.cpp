#include "session.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <signal.h>
namespace fs = std::filesystem;
static void write(const fs::path& path, const std::string& bytes) { fs::create_directories(path.parent_path()); std::ofstream(path, std::ios::binary) << bytes; }
int main(int argc, char** argv) {
    QApplication app(argc, argv); assert(argc == 2);
    QTemporaryDir temporary; assert(temporary.isValid());
    auto root = fs::path(temporary.path().toStdU32String()) / "window bundle";
    fs::create_directories(root / "linux");
    fs::copy_file(fs::path(argv[1]), root / "linux" / "pt");
    fs::permissions(root / "linux" / "pt", fs::perms::owner_exec, fs::perm_options::add);
    write(root / "CUSA01127" / "chunk1.psarc", "PSAR" + std::string(60, '\0'));
    std::string qar(64, '\0'); qar[64 - 0x24 + 0x16] = 'a'; qar[64 - 0x24 + 0x17] = 'q'; write(root / "CUSA01127" / "texture.qar", qar);
    QFile binary(QString::fromStdU32String((root / "linux" / "pt").u32string())); assert(binary.open(QIODevice::ReadOnly));
    QCryptographicHash hash(QCryptographicHash::Sha256); assert(hash.addData(&binary));
    auto state = pt::launcher::PlayFlat(root, {{"linux", hash.result().toHex().toStdString(), "window-fixture"}});
    assert(state.activity == pt::launcher::Activity::Running);
    QString error; bool focused = false;
    for (int i = 0; i < 10 && !focused; ++i) { QThread::msleep(100); focused = pt::launcher::ReturnToGame(state, &error); }
    kill(state.pid, SIGTERM);
    assert(focused);
}
