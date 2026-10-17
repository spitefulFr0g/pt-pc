#pragma once
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

