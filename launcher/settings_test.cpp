#include "settings.h"
#include <QApplication>
#include <QTemporaryDir>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <QProcess>
#include <QThread>

namespace fs = std::filesystem;
static void put(const fs::path& p, const std::string& s) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << s; }
static std::string get(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {(std::istreambuf_iterator<char>(f)), {}}; }
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temp; assert(temp.isValid());
    const fs::path root = fs::path(temp.path().toStdU32String()) / "bundle 雪 with spaces";
    const auto linuxIni = root / "linux/data/pt.ini";
    const auto windowsIni = root / "windows/data/pt.ini";
    const std::string initial = "; keep comment\r\n[display]\r\nwidth = 1280\r\nheight=720\r\nfullscreen = 0\r\nunknown = untouched\r\n[other]\r\nkey = preserve\r\n";
    put(linuxIni, initial); put(windowsIni, "[display]\nwidth=1920\nheight=1080\nfullscreen=1\n");
    auto current = pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux);
    assert(current.available && current.value.width == 1280 && current.value.height == 720 && current.value.windowMode == 0);
    assert(pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Windows).value.width == 1920);
    pt::launcher::DisplaySettingsEdit edit; edit.width = 1600;
    put(linuxIni, initial + "; concurrent unrelated\r\n[display]\r\n");
    auto saved = pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, current, edit);
    assert(saved.status == pt::launcher::ApplyStatus::Applied);
    const auto after = get(linuxIni);
    assert(after.find("; keep comment\r\n") == 0 && after.find("width = 1600\r\n") != std::string::npos);
    assert(after.find("height=720\r\n") != std::string::npos && after.find("unknown = untouched\r\n") != std::string::npos);
    assert(after.find("; concurrent unrelated\r\n") != std::string::npos && after.ends_with("[display]\r\n"));
    current = pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux);
    put(linuxIni, "[display]\nwidth = 1366\nheight=720\nfullscreen=0\nunknown = concurrent\n");
    edit = {}; edit.width = 1600;
    saved = pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, current, edit);
    assert(saved.status == pt::launcher::ApplyStatus::Conflict);
    assert(get(linuxIni).find("width = 1366") != std::string::npos);
    current = pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux);
    edit = {}; edit.windowMode = 2;
    saved = pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, current, edit);
    assert(saved.status == pt::launcher::ApplyStatus::Applied);
    assert(pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux).value.windowMode == 2);
    edit={};edit.width=20000;
    saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,current,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Failed);
    put(linuxIni,"[display]\nwidth=broken\nheight=oops\nfullscreen=999\n");
    current=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux);
    assert(current.value.width==1600&&current.value.height==900&&current.value.windowMode==2);
#ifdef __linux__
    const auto executable=root/"linux/pt";
    fs::copy_file("/bin/sleep",executable,fs::copy_options::overwrite_existing);
    fs::permissions(executable,fs::perms::owner_exec,fs::perm_options::add);
    qint64 pid=0;
    assert(QProcess::startDetached(QString::fromStdU32String(executable.u32string()),{"20"},{},&pid));
    for(int i=0;i<100&&pt::launcher::InspectSession(root).activity!=pt::launcher::Activity::Running;++i) QThread::msleep(10);
    auto stoppedSnapshot=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux);
    edit={};edit.width=1700;
    saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,stoppedSnapshot,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Blocked);
    assert(pt::launcher::InspectSession(root).activity==pt::launcher::Activity::Running);
    put(linuxIni,"[display]\nwidth=1750\nheight=900\nfullscreen=1\n");
    QProcess::execute("kill",{"-TERM",QString::number(pid)});
    for(int i=0;i<100&&pt::launcher::InspectSession(root).activity==pt::launcher::Activity::Running;++i) QThread::msleep(10);
    const auto afterGameExit=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux);
    assert(afterGameExit.value.width==1750&&afterGameExit.value.windowMode==1);
#endif
    fs::remove(root/"windows/data/pt.ini");
    auto missing=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Windows);
    assert(missing.available&&missing.value.width==1600);
    fs::create_directory(root/"windows/data/pt.ini");
    auto windows=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Windows);
    edit={};edit.width=1440;
    saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Windows,windows,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Failed);
    assert(fs::is_directory(root/"windows/data/pt.ini"));
    fs::remove(root/"windows/data/pt.ini");
    put(root/"windows/data/pt.ini","[display]\nwidth=1280\nheight=720\nfullscreen=0\n");
    windows=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Windows);
    const auto dataDirectory=root/"windows/data";
    const auto oldPermissions=fs::status(dataDirectory).permissions();
    fs::permissions(dataDirectory,fs::perms::owner_read|fs::perms::owner_exec,fs::perm_options::replace);
    edit={};edit.width=1440;
    saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Windows,windows,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Failed);
    fs::permissions(dataDirectory,oldPermissions,fs::perm_options::replace);
}
