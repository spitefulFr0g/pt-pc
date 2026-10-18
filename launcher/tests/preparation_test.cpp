#include "preparation_fixture.h"

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
    if (const char* pkg = std::getenv("PT_LAUNCHER_TEST_INVALID_PKG")) {
        assert(std::getenv("PT_LAUNCHER_STAGED_EXTRACTOR"));
        Fixture f;
        const auto bytes = get(fs::u8path(pkg));
        auto result = PrepareGame(f.root, fs::u8path(pkg), f.supported);
        assert(!result.ready && !result.error.isEmpty());
        assert(get(fs::u8path(pkg)) == bytes);
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(get(f.root / "unowned bundle note.txt") == "unowned");
        assert(!fs::exists(f.root / "CUSA01127/chunk1.psarc"));
        assert(!PreparationNeedsRecovery(f.root));
    }
    if (const char* pkg = std::getenv("PT_LAUNCHER_TEST_PKG")) {
        assert(std::getenv("PT_LAUNCHER_STAGED_EXTRACTOR"));
        Fixture f;
        auto sourceBytes = get(fs::u8path(pkg));
        auto result = PrepareGame(f.root, fs::u8path(pkg), f.supported);
        if (!result.ready) std::cerr << result.error.toStdString() << "\n";
        assert(result.ready && result.error.isEmpty());
        assert(get(f.root / "CUSA01127/chunk1.psarc").startsWith("PSAR"));
        assert(get(f.root / "CUSA01127/texture.qar").size() == 0x40);
        assert(get(fs::u8path(pkg)) == sourceBytes);
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(get(f.root / "unowned bundle note.txt") == "unowned");
        assert(!PreparationNeedsRecovery(f.root));
    }
#ifndef _WIN32
    {
        Fixture f;
        const auto alias = f.root.parent_path() / "bundle escape alias";
        fs::create_directory_symlink(f.root, alias);
        auto result = PrepareGame(alias, f.source, f.supported);
        assert(!result.ready && result.error.contains("Linked preparation path is unsafe"));
        assert(!fs::exists(alias / ".pt-preparation.json"));
        assert(get(f.root / "CUSA01127/player-save.bin") == "player data");
        assert(get(f.source / "chunk1.psarc") == "PSARsynthetic archive data");
    }
#endif
    std::cout << "preparation public-operation cases passed\n";
}
