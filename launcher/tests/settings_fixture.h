#pragma once
#include "inspection.h"
#include <QCryptographicHash>
#include <QFile>
#include <filesystem>
#include <fstream>

// Isolated actual executable bytes, never a production identity bypass.
inline std::vector<pt::launcher::SupportedRuntime> SettingsRuntimes(const std::filesystem::path& root) {
    std::vector<pt::launcher::SupportedRuntime> supported;
    for (const auto& platform : {"linux", "windows"}) {
        const auto executable=root/platform/(std::string(platform)=="linux" ? "pt" : "pt.exe");
        std::filesystem::create_directories(executable.parent_path());
#ifdef __linux__
        std::filesystem::copy_file("/bin/sleep",executable,std::filesystem::copy_options::overwrite_existing);
#else
        std::ofstream(executable,std::ios::binary)<<"isolated runtime fixture";
#endif
        QFile file(QString::fromStdU32String(executable.u32string()));
        if (!file.open(QIODevice::ReadOnly)) std::abort();
        supported.push_back({platform,QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex().toStdString(),"settings fixture"});
    }
    return supported;
}
