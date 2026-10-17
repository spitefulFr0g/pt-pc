#include "preparation.h"
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef __linux__
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace pt::launcher;
#ifdef _WIN32
static constexpr auto platform = "windows";
static constexpr auto runtimeName = "pt.exe";
#else
static constexpr auto platform = "linux";
static constexpr auto runtimeName = "pt";
#endif

static void put(const fs::path& path, const QByteArray& bytes) {
    fs::create_directories(path.parent_path());
    QFile f(QString::fromStdU32String(path.u32string()));
    assert(f.open(QIODevice::WriteOnly));
    assert(f.write(bytes) == bytes.size());
}
static QByteArray get(const fs::path& path) {
    QFile f(QString::fromStdU32String(path.u32string()));
    assert(f.open(QIODevice::ReadOnly)); return f.readAll();
}
static QByteArray qar() {
    QByteArray b(0x24, '\0'); b[0x16] = 'a'; b[0x17] = 'q'; return b;
}
static QByteArray digest(const fs::path& path) {
    return QCryptographicHash::hash(get(path), QCryptographicHash::Sha256).toHex();
}
struct Fixture {
    QTemporaryDir temporary{QDir::tempPath() + "/pt-preparation-XXXXXX-試験"};
    fs::path root, source;
    QString runtimeHash;
    std::vector<SupportedRuntime> supported;
    Fixture() {
        assert(temporary.isValid());
        root = fs::path(temporary.path().toStdU32String()) / "portable bundle";
        source = fs::path(temporary.path().toStdU32String()) / "dump folder 日本語";
        put(root / platform / runtimeName, "fixture runtime");
        runtimeHash = QString::fromLatin1(digest(root / platform / runtimeName));
        supported.push_back({platform, runtimeHash.toStdString(), "synthetic fixture"});
        put(source / "chunk1.psarc", QByteArray("PSARsynthetic archive data"));
        put(source / "texture.qar", qar());
        put(source / "pathid_list_ps4.bin", QByteArray("pt14_ synthetic path list fixture data ............"));
        put(source / "unowned.txt", "leave source untouched");
        put(root / "CUSA01127/player-save.bin", "player data");
        put(root / "unowned bundle note.txt", "unowned");
        put(root / platform / "data/pt.ini", "[network]\ncheck_updates=1\n\n[player]\nvolume=0.7\n");
    }
};

int main() {
    {
        Fixture f;
        auto result = PrepareGame(f.root, f.source, f.supported);
        if (!result.ready) std::cerr << result.error.toStdString() << "\n";
        assert(result.ready && result.error.isEmpty());
        assert(get(f.root / "CUSA01127/chunk1.psarc") == "PSARsynthetic archive data");
        assert(get(f.root / "CUSA01127/texture.qar") == qar());
        assert(get(f.root / "CUSA01127/pathid_list_ps4.bin") == QByteArray("pt14_ synthetic path list fixture data ............"));
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(get(f.root / "unowned bundle note.txt") == "unowned");
        assert(get(f.source / "chunk1.psarc") == "PSARsynthetic archive data");
        const auto ini = get(f.root / platform / "data/pt.ini");
        assert(ini.contains("check_updates=0"));
        assert(ini.contains("volume=0.7"));
        assert(fs::exists(f.root / platform / "data/.pt-launcher-adopted"));
        assert(!PreparationNeedsRecovery(f.root));
        auto second = PrepareGame(f.root, f.source, f.supported);
        assert(second.ready && second.notes.join(' ').contains("without copying or extraction"));
        assert(get(f.root / platform / "data/pt.ini") == ini);
        const fs::path moved = f.root.parent_path() / "moved portable bundle 日本語";
        fs::rename(f.root, moved);
        auto afterMove = PrepareGame(moved, f.source, f.supported);
        assert(afterMove.ready && afterMove.error.isEmpty());
        assert(get(moved / "CUSA01127/player-save.bin") == "player data");
        assert(get(moved / platform / "data/pt.ini") == ini);
    }
    {
        Fixture f;
        bool changed = false;
        auto result = PrepareGame(f.root, f.source, f.supported, {}, [&](const PreparationProgress& p) {
            if (!changed && p.total) { changed = true; put(f.source / "chunk1.psarc", QByteArray("PSARchanged during copy")); }
        });
        assert(changed);
        assert(!result.ready && !result.error.isEmpty());
        assert(!fs::exists(f.root / "CUSA01127/chunk1.psarc"));
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(!PreparationNeedsRecovery(f.root));
    }
    {
        Fixture f;
        QByteArray largeArchive(16 * 1024 * 1024, 'x');
        largeArchive.replace(0, 4, "PSAR");
        put(f.source / "chunk1.psarc", largeArchive);
        bool requested = false;
        auto result = PrepareGame(f.root, f.source, f.supported, [&] { return requested; }, [&](const PreparationProgress& p) {
            if (p.total && p.done) requested = true;
        });
        assert(requested);
        assert(result.cancelled && !result.ready);
        assert(!PreparationNeedsRecovery(f.root));
        assert(!fs::exists(f.root / "CUSA01127/chunk1.psarc"));
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
    }
    {
        Fixture f;
        put(f.root / "CUSA01127/chunk1.psarc", "PSARexisting archive");
        put(f.root / "CUSA01127/texture.qar", qar());
        const QString stageName = ".pt-preparation-0123456789abcdef0123456789abcdef";
        const fs::path stage = f.root / stageName.toStdU32String();
        fs::create_directory(stage);
        put(stage / ".owner", stageName.toUtf8());
        QJsonObject journal{{"version", 1}, {"stage", stageName}, {"phase", "initializing"}, {"files", QJsonArray{}}};
        put(f.root / ".pt-preparation.json", QJsonDocument(journal).toJson(QJsonDocument::Compact));
        assert(PreparationNeedsRecovery(f.root));
        auto recovered = RecoverPreparation(f.root);
        if (!recovered.error.isEmpty()) std::cerr << recovered.error.toStdString() << "\n";
        assert(recovered.error.isEmpty() && recovered.ready);
        assert(!PreparationNeedsRecovery(f.root));
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
    }
    {
        Fixture f;
        put(f.root / "CUSA01127/chunk1.psarc", "PSARexisting archive");
        put(f.root / "CUSA01127/texture.qar", qar());
        const QString stageName = ".pt-preparation-abcdef0123456789abcdef0123456789";
        const fs::path stage = f.root / stageName.toStdU32String();
        fs::create_directories(stage / "old/CUSA01127");
        put(stage / ".owner", stageName.toUtf8());
        put(stage / "old/CUSA01127/source.txt", "old metadata");
        put(f.root / "CUSA01127/source.txt", "new metadata");
        put(f.root / "install-notes.txt", "new notes");
        QJsonArray files{
            QJsonObject{{"name", "CUSA01127/source.txt"}, {"old", QString::fromLatin1(QCryptographicHash::hash("old metadata", QCryptographicHash::Sha256).toHex())}, {"new", QString::fromLatin1(digest(f.root / "CUSA01127/source.txt"))}},
            QJsonObject{{"name", "install-notes.txt"}, {"old", ""}, {"new", QString::fromLatin1(digest(f.root / "install-notes.txt"))}}
        };
        QJsonObject journal{{"version", 1}, {"stage", stageName}, {"phase", "committing"}, {"files", files}};
        put(f.root / ".pt-preparation.json", QJsonDocument(journal).toJson(QJsonDocument::Compact));
        assert(PreparationNeedsRecovery(f.root));
        auto recovered = RecoverPreparation(f.root);
        assert(recovered.error.isEmpty() && recovered.ready);
        assert(get(f.root / "CUSA01127/source.txt") == "old metadata");
        assert(!fs::exists(f.root / "install-notes.txt"));
        assert(!PreparationNeedsRecovery(f.root));
    }
#ifdef __linux__
    if (geteuid() == 0) {
        Fixture f;
        fs::permissions(f.temporary.path().toStdU32String(), fs::perms::all, fs::perm_options::replace);
        fs::permissions(f.temporary.path().toStdU32String(), fs::perms::owner_read | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec, fs::perm_options::replace);
        assert(seteuid(65534) == 0);
        auto result = PrepareGame(f.root, f.source, f.supported);
        assert(seteuid(0) == 0);
        assert(!result.ready && result.error.contains("permission", Qt::CaseInsensitive));
        fs::permissions(f.temporary.path().toStdU32String(), fs::perms::all, fs::perm_options::replace);
    }
#endif
    {
        Fixture f;
        auto result = PrepareGame(f.root, f.source / "absent", f.supported);
        assert(!result.ready && result.error.contains("Select your P.T. game"));
        assert(!fs::exists(f.root / "CUSA01127/chunk1.psarc"));
        assert(get(f.source / "chunk1.psarc") == "PSARsynthetic archive data");
    }
    std::cout << "preparation public-operation cases passed\n";
}
