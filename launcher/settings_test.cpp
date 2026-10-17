#include "tests/settings_fixture.h"
#include "settings.h"
#include <QApplication>
#include <QTemporaryDir>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <QProcess>
#include <QThread>

#ifdef __linux__
#include <unistd.h>
#endif

namespace fs = std::filesystem;
static void put(const fs::path& p, const std::string& s) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << s; }
static std::string get(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {(std::istreambuf_iterator<char>(f)), {}}; }
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temp; assert(temp.isValid());
    const fs::path root = fs::path(temp.path().toStdU32String()) / "bundle 雪 with spaces";
    const auto supported=SettingsRuntimes(root);
    const auto linuxIni = root / "linux/data/pt.ini";
    const auto windowsIni = root / "windows/data/pt.ini";
    const std::string initial = "; keep comment\r\n[display]\r\nwidth = 1280\r\nheight=720\r\nfullscreen = 0\r\nunknown = untouched\r\n[other]\r\nkey = preserve\r\n";
    put(linuxIni, initial); put(windowsIni, "[display]\nwidth=1920\nheight=1080\nfullscreen=1\n");
    put(linuxIni, "[display]\nwidth=\"1920\"\n");
    assert(pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported).value.width == 1920);
    put(linuxIni, initial);
    assert(!pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, {}).available);
    auto current = pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported);
    assert(current.value.graphicsPreset == pt::GraphicsPreset::Custom || current.value.graphicsPreset == pt::GraphicsPreset::Original);
    for (const auto preset : {pt::GraphicsPreset::Low,pt::GraphicsPreset::Original,pt::GraphicsPreset::High,pt::GraphicsPreset::Ultra}) {
        pt::launcher::DisplaySettingsEdit graphicsEdit; graphicsEdit.graphicsPreset=preset;
        auto graphicsSaved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,graphicsEdit);
        assert(graphicsSaved.status==pt::launcher::ApplyStatus::Applied);
        current=graphicsSaved.persisted;
        assert(current.value.graphicsPreset==preset);
        const auto beforeCustom=get(linuxIni);pt::launcher::DisplaySettingsEdit custom;custom.graphicsPreset=pt::GraphicsPreset::Custom;
        auto rejectedCustom=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,custom);
        assert(rejectedCustom.status==pt::launcher::ApplyStatus::Failed&&get(linuxIni)==beforeCustom);
    }
    const auto presetBytes=get(linuxIni);
    for(const auto* key:{"anisotropy = ","enhanced_textures = ","shadow_quality = ","ambient_occlusion = ","reflections = ","bloom = ","depth_of_field = ","motion_blur = ","lens_distortion = ","lens_ghosts = ","deband = ","film_grain = ","clarity = ","texture_detail = ","ray_quality = ","shadows = ","contact_shadows = ","full_shadows = ","bounce_light = ","glossy_reflections = "}) assert(presetBytes.find(key)!=std::string::npos);
    assert(presetBytes.find("unknown = untouched")!=std::string::npos && presetBytes.find("key = preserve")!=std::string::npos);
    auto unknownCapability=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported);
    assert(!unknownCapability.value.rayTracingCapabilityKnown && !unknownCapability.value.rayTracingSupported);
    auto supportedCapability=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported,pt::launcher::RayTracingCapability::Supported);
    assert(supportedCapability.value.rayTracingCapabilityKnown && supportedCapability.value.rayTracingSupported);
    pt::launcher::DisplaySettingsEdit ultraEdit;ultraEdit.graphicsPreset=pt::GraphicsPreset::Ultra;
    supportedCapability=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,supportedCapability,ultraEdit).persisted;
    assert(supportedCapability.value.graphicsPreset==pt::GraphicsPreset::Ultra);
    put(linuxIni,"[display]\nwidth=1280\nheight=720\nfullscreen=0\n[graphics]\ntexture_detail=0\nunknown=custom\n");
    auto manual=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported);
    assert(manual.value.graphicsPreset==pt::GraphicsPreset::Custom);
    current=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported);
    assert(current.available && current.value.width == 1280 && current.value.height == 720 && current.value.windowMode == 0);
    assert(pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Windows, supported).value.width == 1920);
    pt::launcher::DisplaySettingsEdit edit; edit.width = 1600;
    const auto beforeReplacement=get(linuxIni);
    put(root/"linux/pt", "replaced runtime");
    const auto replaced=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,edit);
    assert(replaced.status==pt::launcher::ApplyStatus::Blocked && get(linuxIni)==beforeReplacement);
    SettingsRuntimes(root);
    const std::string mixed="[display]\r\nwidth=1280\nheight=720\r\nfullscreen=0\n";
    put(linuxIni,mixed);
    auto noop=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,{});
    assert(noop.status==pt::launcher::ApplyStatus::Applied && get(linuxIni)==mixed);
    auto saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Applied);
    assert(get(linuxIni)=="[display]\r\nwidth=1600\nheight=720\r\nfullscreen=0\n");
    put(linuxIni,initial);
    put(linuxIni, initial + "; concurrent unrelated\r\n[display]\r\n");
    saved = pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, supported, current, edit);
    assert(saved.status == pt::launcher::ApplyStatus::Applied);
    const auto after = get(linuxIni);
    assert(after.find("; keep comment\r\n") == 0 && after.find("width = 1600\r\n") != std::string::npos);
    assert(after.find("height=720\r\n") != std::string::npos && after.find("unknown = untouched\r\n") != std::string::npos);
    assert(after.find("; concurrent unrelated\r\n") != std::string::npos && after.ends_with("[display]\r\n"));
    current = pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported);
    put(linuxIni, "[display]\nwidth = 1366\nheight=720\nfullscreen=0\nunknown = concurrent\n");
    edit = {}; edit.width = 1600;
    saved = pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, supported, current, edit);
    assert(saved.status == pt::launcher::ApplyStatus::Conflict);
    assert(get(linuxIni).find("width = 1366") != std::string::npos);
    current = pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported);
    edit = {}; edit.windowMode = 2;
    saved = pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, supported, current, edit);
    assert(saved.status == pt::launcher::ApplyStatus::Applied);
    assert(pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported).value.windowMode == 2);
    edit={};edit.width=20000;
    saved=pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, supported,current,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Failed);
    put(linuxIni,"[display]\nwidth=broken\nheight=oops\nfullscreen=999\n");
    current=pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported);
    assert(current.value.width==1600&&current.value.height==900&&current.value.windowMode==2);
#ifdef __linux__
    const auto executable=root/"linux/pt";
    fs::copy_file("/bin/sleep",executable,fs::copy_options::overwrite_existing);
    fs::permissions(executable,fs::perms::owner_exec,fs::perm_options::add);
    qint64 pid=0;
    assert(QProcess::startDetached(QString::fromStdU32String(executable.u32string()),{"20"},{},&pid));
    for(int i=0;i<100&&pt::launcher::InspectSession(root).activity!=pt::launcher::Activity::Running;++i) QThread::msleep(10);
    auto stoppedSnapshot=pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported);
    edit={};edit.width=1700;
    saved=pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, supported,stoppedSnapshot,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Blocked);
    assert(pt::launcher::InspectSession(root).activity==pt::launcher::Activity::Running);
    put(linuxIni,"[display]\nwidth=1750\nheight=900\nfullscreen=1\n");
    QProcess::execute("kill",{"-TERM",QString::number(pid)});
    for(int i=0;i<100&&pt::launcher::InspectSession(root).activity==pt::launcher::Activity::Running;++i) QThread::msleep(10);
    const auto afterGameExit=pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Linux, supported);
    assert(afterGameExit.value.width==1750&&afterGameExit.value.windowMode==1);
#endif
    put(linuxIni,std::string(4*1024*1024+1,'x'));
    assert(!pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported).available);
    fs::remove(root/"windows/data/pt.ini");
    auto missing=pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Windows, supported);
    assert(missing.available&&missing.value.width==1600);
    fs::remove(root/"windows/data");
    edit={}; edit.width=1440;
    saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Windows,supported,missing,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Applied && get(windowsIni).find("width = 1440")!=std::string::npos);
    fs::remove(windowsIni);
    fs::create_directory(root/"windows/data/pt.ini");
    auto windows=pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Windows, supported);
    edit={};edit.width=1440;
    saved=pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Windows, supported,windows,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Failed);
    assert(fs::is_directory(root/"windows/data/pt.ini"));
    fs::remove(root/"windows/data/pt.ini");
    put(root/"windows/data/pt.ini","[display]\nwidth=1280\nheight=720\nfullscreen=0\n");
    windows=pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Windows, supported);
    const auto dataDirectory=root/"windows/data";
    const auto oldPermissions=fs::status(dataDirectory).permissions();
    fs::permissions(dataDirectory,fs::perms::owner_read|fs::perms::owner_exec|
        fs::perms::group_read|fs::perms::group_exec|fs::perms::others_read|fs::perms::others_exec,fs::perm_options::replace);
    edit={};edit.width=1440;
#ifdef __linux__
    const bool rootHarness=geteuid()==0;
    if (rootHarness) {
        // Keep the bundle guard and read path accessible; deny only the save directory.
        fs::permissions(fs::path(temp.path().toStdU32String()),fs::perms::all,fs::perm_options::replace);
        fs::permissions(root,fs::perms::all,fs::perm_options::replace);
        assert(seteuid(65534)==0);
    }
#endif
    saved=pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Windows, supported,windows,edit);
#ifdef __linux__
    if (rootHarness) assert(seteuid(0)==0);
#endif
    assert(saved.status==pt::launcher::ApplyStatus::Failed);
    assert(saved.error.contains("atomically save"));
    assert(get(windowsIni)=="[display]\nwidth=1280\nheight=720\nfullscreen=0\n");
    fs::permissions(dataDirectory,oldPermissions,fs::perm_options::replace);
#ifdef __linux__
    const auto outside=root/"outside";
    put(outside/"pt.ini","[display]\nwidth=1280\n");
    fs::remove_all(dataDirectory);
    fs::create_directory_symlink(outside,dataDirectory);
    assert(!pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Windows,supported).available);
    saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Windows,supported,windows,edit);
    assert(saved.status!=pt::launcher::ApplyStatus::Applied);
    assert(get(outside/"pt.ini")=="[display]\nwidth=1280\n");
#endif

}
