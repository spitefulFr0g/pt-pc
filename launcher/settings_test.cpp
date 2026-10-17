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
#include "engine/core/log.h"
namespace pt { void LogWrite(LogLevel, std::string_view) {} }

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
    const auto sharedCache=root/"saves/enhanced-textures-2048/fixture.cache";
    put(sharedCache,"shared cache bytes");
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
        pt::AppSettings expected;
        pt::ApplyGraphicsPreset(expected,preset,false);
        assert(current.graphics==expected.graphics && current.rayTracing==expected.ray_tracing);
        const auto beforeCustom=get(linuxIni);pt::launcher::DisplaySettingsEdit custom;custom.graphicsPreset=pt::GraphicsPreset::Custom;
        auto rejectedCustom=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,custom);
        assert(rejectedCustom.status==pt::launcher::ApplyStatus::Failed&&get(linuxIni)==beforeCustom);
    }
    // Legacy Ultra is interpreted by the runtime as current Ultra.
    auto legacyUltra=get(linuxIni);
    const auto ray=legacyUltra.find("ray_quality = 3");
    assert(ray!=std::string::npos);
    legacyUltra.replace(ray,15,"ray_quality = 2");
    put(linuxIni,legacyUltra);
    assert(pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported).value.graphicsPreset==pt::GraphicsPreset::Ultra);
    // Independent upstream oracle: same exact bytes, both capability interpretations.
    const auto checkUpstream = [&](const std::string& bytes) {
        put(linuxIni,bytes);
        pt::AppSettings upstream;
        assert(pt::LoadAppSettings(linuxIni,upstream));
        for (const auto capability : {pt::launcher::RayTracingCapability::Unknown,
                                     pt::launcher::RayTracingCapability::Unsupported,
                                     pt::launcher::RayTracingCapability::Supported}) {
            const auto actual=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported,capability);
            assert(actual.available);
            assert(actual.graphics==upstream.graphics);
            assert(actual.rayTracing==upstream.ray_tracing);
            assert(actual.value.graphicsPreset==pt::DetectGraphicsPreset(upstream,capability==pt::launcher::RayTracingCapability::Supported));
        }
    };
    checkUpstream("");
    for (const auto preset : {pt::GraphicsPreset::Low,pt::GraphicsPreset::Original,pt::GraphicsPreset::High,pt::GraphicsPreset::Ultra}) {
        pt::AppSettings upstream;
        pt::ApplyGraphicsPreset(upstream,preset,true);
        assert(pt::SaveAppSettings(linuxIni,upstream));
        const auto bytes=get(linuxIni);
        checkUpstream(bytes);
        auto older=bytes;
        for (const auto key : {"lens_ghosts = ", "deband = "}) {
            const auto begin=older.find(key);
            assert(begin!=std::string::npos);
            older.erase(begin,older.find('\n',begin)-begin+1);
        }
        checkUpstream(older);
        for (const auto ray : {"-1","2","5","99","broken"}) checkUpstream(bytes+"\n[graphics]\nray_quality="+ray+"\n");
        checkUpstream(bytes+"\n[graphics]\nanisotropy=99\nshadow_quality=99\ntexture_detail=99\nfilm_grain=nan\nclarity=inf\n[raytracing]\nshadows=99\nreflections=YES\n");
        checkUpstream(bytes+"\n[graphics]\nanisotropy=7\nfilm_grain=-2\nclarity=2\nbloom=TRUE\ndepth_of_field=ON\n");
        checkUpstream(bytes+"\n[graphics]\nfilm_grain=.25\nfilm_grain=invalid\n");
    }
    put(linuxIni,legacyUltra);
    assert(get(sharedCache)=="shared cache bytes");
    // Runtime booleans do not accept inline comments as part of a true token.
    put(linuxIni,"[graphics]\r\nbloom = FALSE ; keep bloom note\n; bloom=untouched comment\r\n[other]\nvalue = unchanged # literal\n");
    auto withComment=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported);
    pt::launcher::DisplaySettingsEdit originalPreset;
    originalPreset.graphicsPreset=pt::GraphicsPreset::Original;
    const auto commentSaved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,withComment,originalPreset);
    assert(commentSaved.status==pt::launcher::ApplyStatus::Applied && commentSaved.persisted.value.graphicsPreset==pt::GraphicsPreset::Original);
    assert(get(linuxIni).find("; keep bloom note")!=std::string::npos);
    assert(get(linuxIni).find("; bloom=untouched comment\r\n")!=std::string::npos);
    assert(get(linuxIni).find("[other]\nvalue = unchanged # literal\n")!=std::string::npos);
    put(linuxIni,legacyUltra);
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
    for (const auto capability : {pt::launcher::RayTracingCapability::Unknown,
                                 pt::launcher::RayTracingCapability::Unsupported,
                                 pt::launcher::RayTracingCapability::Supported}) {
        auto base=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported,capability);
        for (const auto preset : {pt::GraphicsPreset::Low,pt::GraphicsPreset::Original,pt::GraphicsPreset::High,pt::GraphicsPreset::Ultra}) {
            pt::launcher::DisplaySettingsEdit change;
            change.graphicsPreset=preset;
            const auto applied=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,base,change);
            assert(applied.status==pt::launcher::ApplyStatus::Applied);
            pt::AppSettings expected;
            pt::ApplyGraphicsPreset(expected,preset,capability==pt::launcher::RayTracingCapability::Supported);
            assert(applied.persisted.graphics==expected.graphics && applied.persisted.rayTracing==expected.ray_tracing);
            base=applied.persisted;
        }
    }
    assert(get(sharedCache)=="shared cache bytes");
    put(linuxIni,"[display]\nwidth=1280\nheight=720\nfullscreen=0\n[graphics]\ntexture_detail=0\nunknown=custom\n");
    auto manual=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported);
    assert(manual.value.graphicsPreset==pt::GraphicsPreset::Custom);
    // Different Custom configurations must conflict over any affected field.
    pt::launcher::DisplaySettingsEdit presetEdit;
    presetEdit.graphicsPreset=pt::GraphicsPreset::High;
    put(linuxIni,"[display]\nwidth=1280\nheight=720\nfullscreen=0\n[graphics]\ntexture_detail=0\nclarity=.3\n");
    const auto externalCustom=get(linuxIni);
    auto conflict=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,manual,presetEdit);
    assert(conflict.status==pt::launcher::ApplyStatus::Conflict && get(linuxIni)==externalCustom);
    for (int invalid : {-1,4,5,99}) {
        presetEdit.graphicsPreset=static_cast<pt::GraphicsPreset>(invalid);
        const auto invalidResult=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,manual,presetEdit);
        assert(invalidResult.status==pt::launcher::ApplyStatus::Failed && get(linuxIni)==externalCustom);
    }
    current=pt::launcher::ReadDisplaySettings(root,pt::launcher::Platform::Linux,supported);
    assert(current.available && current.value.width == 1280 && current.value.height == 720 && current.value.windowMode == 0);
    assert(pt::launcher::ReadDisplaySettings(root, pt::launcher::Platform::Windows, supported).value.width == 1920);
    pt::launcher::DisplaySettingsEdit edit; edit.width = 1600; edit.graphicsPreset=pt::GraphicsPreset::High;
    const auto beforeReplacement=get(linuxIni);
    put(root/"linux/pt", "replaced runtime");
    const auto replaced=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,edit);
    assert(replaced.status==pt::launcher::ApplyStatus::Blocked && get(linuxIni)==beforeReplacement);
    SettingsRuntimes(root);
    edit.graphicsPreset.reset();
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
    edit={};edit.width=1700;edit.graphicsPreset=pt::GraphicsPreset::Ultra;
    saved=pt::launcher::ApplyDisplaySettings(root, pt::launcher::Platform::Linux, supported,stoppedSnapshot,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Blocked);
    assert(pt::launcher::InspectSession(root).activity==pt::launcher::Activity::Running);
    qint64 secondPid=0;
    assert(QProcess::startDetached(QString::fromStdU32String(executable.u32string()),{"20"},{},&secondPid));
    for(int i=0;i<100&&pt::launcher::InspectSession(root).activity!=pt::launcher::Activity::Uncertain;++i) QThread::msleep(10);
    assert(pt::launcher::InspectSession(root).activity==pt::launcher::Activity::Uncertain);
    const auto beforeUncertain=get(linuxIni);
    saved=pt::launcher::ApplyDisplaySettings(root,pt::launcher::Platform::Linux,supported,current,edit);
    assert(saved.status==pt::launcher::ApplyStatus::Blocked && get(linuxIni)==beforeUncertain);
    QProcess::execute("kill",{"-TERM",QString::number(secondPid)});
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
