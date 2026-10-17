#include "settings.h"
#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <map>
#include <sstream>
#include "engine/platform/settings.h"
#include "engine/platform/graphics_presets.h"

namespace pt::launcher {
namespace fs = std::filesystem;
namespace {
fs::path settingsPath(const fs::path& bundle, Platform platform) {
    return bundle / (platform == Platform::Linux ? "linux" : "windows") / "data" / "pt.ini";
}
// Reject linked components (including Windows junction/reparse resolution) before
// reads, directory creation, and replacement. The selected bundle itself may move.
bool containedSettings(const fs::path& bundle, Platform platform, QString& error) {
    std::error_code ec;
    auto expected=fs::canonical(bundle,ec);
    if (ec) { error="Cannot resolve the bundle folder."; return false; }
    for (const auto& component : {platform==Platform::Linux ? "linux" : "windows", "data", "pt.ini"}) {
        expected /= component;
        const auto status=fs::symlink_status(expected,ec);
        if (ec==std::errc::no_such_file_or_directory) { ec.clear(); continue; }
        if (ec || fs::is_symlink(status)) { error="Settings path must not contain links or reparse escapes."; return false; }
        if (fs::exists(status)) {
            const auto resolved=fs::canonical(expected,ec);
            if (ec || resolved!=expected) { error="Settings path escapes its platform folder."; return false; }
        }
    }
    return true;
}
std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
std::string keyValue(const std::string& text, const char* wanted) {
    std::istringstream input(text); std::string line, section, found;
    while (std::getline(input, line)) {
        auto value = trim(line);
        if (value.empty() || value[0] == ';' || value[0] == '#') continue;
        if (value.front() == '[' && value.back() == ']') { section = trim(value.substr(1, value.size()-2)); continue; }
        const auto equals = value.find('=');
        if (equals != std::string::npos && section == "display" && trim(value.substr(0, equals)) == wanted)
        {
            found = trim(value.substr(equals + 1));
            if (found.size() >= 2 && found.front() == '"' && found.back() == '"')
                found = found.substr(1, found.size()-2);
        }
    }
    return found;
}
int runtimeInt(const std::string& value, int fallback, int low, int high) {
    if (value.empty()) return fallback;
    try { return std::clamp(std::stoi(value), low, high); } catch (...) { return fallback; }
}
struct SettingsText {
    bool valid=false;
    bool exists=false;
    std::string bytes;
    QString error;
};
SettingsText readText(const fs::path& path) {
    SettingsText result;
    std::error_code ec;
    result.exists=fs::exists(path,ec);
    if (ec) { result.error="Cannot inspect platform settings."; return result; }
    if (!result.exists) { result.valid=true; return result; }
    if (!fs::is_regular_file(path,ec) || ec) { result.error="Platform settings path is not a regular file."; return result; }
    QFile input(QString::fromStdU32String(path.u32string()));
    if (!input.open(QIODevice::ReadOnly)) { result.error="Cannot read platform settings: " + input.errorString(); return result; }
    constexpr qint64 limit=4*1024*1024;
    const auto bytes=input.read(limit+1);
    if (input.error()!=QFileDevice::NoError || bytes.size()>limit || !input.atEnd()) {
        result.error="Cannot read platform settings completely (maximum 4 MiB)."; return result;
    }
    result.bytes=bytes.toStdString(); result.valid=true; return result;
}
DisplaySettingsSnapshot decode(const SettingsText& source) {
    DisplaySettingsSnapshot result;
    if (!source.valid) { result.error=source.error; return result; }
    result.available = true;
    result.value.width = runtimeInt(keyValue(source.bytes, "width"), 1600, 320, 16384);
    result.value.height = runtimeInt(keyValue(source.bytes, "height"), 900, 240, 16384);
    result.value.windowMode = runtimeInt(keyValue(source.bytes, "fullscreen"), 0, 0, 2);
    return result;
}
std::string newlineOf(const std::string& text) { return text.find("\r\n") != std::string::npos ? "\r\n" : "\n"; }
std::string patch(const std::string& text, const DisplaySettingsEdit& edit) {
    const auto nl = newlineOf(text); const bool trailingNewline=text.empty() || text.back()=='\n';
    std::vector<std::string> lines, endings;
    for (size_t start=0; start<text.size();) {
        const auto newline=text.find('\n',start);
        auto end=newline==std::string::npos ? text.size() : newline;
        std::string ending=newline==std::string::npos ? "" : "\n";
        if (newline!=std::string::npos && end>start && text[end-1]=='\r') { --end; ending="\r\n"; }
        lines.push_back(text.substr(start,end-start)); endings.push_back(ending);
        start=newline==std::string::npos ? text.size() : newline+1;
    }
    const auto appendLine=[&](const std::string& line) {
        if (!endings.empty() && endings.back().empty()) endings.back()=nl;
        lines.push_back(line); endings.push_back(trailingNewline ? nl : "");
    };
    bool inDisplay = false, foundSection = false;
    std::map<std::string, bool> written{{"width", false}, {"height", false}, {"fullscreen", false}};
    const auto replaceValue = [&](const std::string& key) -> std::optional<int> {
        if (key == "width") return edit.width;
        if (key == "height") return edit.height;
        if (key == "fullscreen") return edit.windowMode;
        return std::nullopt;
    };
    // The runtime parser uses the last duplicate key, so replace only its effective occurrence.
    std::map<std::string, size_t> last;
    for (size_t i=0; i<lines.size(); ++i) {
        const auto t=trim(lines[i]);
        if (!t.empty() && t.front()=='[' && t.back()==']') inDisplay = trim(t.substr(1,t.size()-2))=="display";
        else if (inDisplay) {
            const auto eq=lines[i].find('=');
            if (eq!=std::string::npos) { auto k=trim(lines[i].substr(0,eq)); if (replaceValue(k)) last[k]=i; }
        }
    }
    const std::map<std::string, int> values{{"width", edit.width.value_or(0)}, {"height", edit.height.value_or(0)}, {"fullscreen", edit.windowMode.value_or(0)}};
    for (const auto& [key,index] : last) {
        if (!replaceValue(key)) continue;
        const auto eq=lines[index].find('=');
        size_t valueStart=eq+1;
        while (valueStart<lines[index].size() && (lines[index][valueStart]==' ' || lines[index][valueStart]=='\t')) ++valueStart;
        size_t suffix=lines[index].find_first_of(";#",valueStart);
        if (suffix==std::string::npos) {
            suffix=lines[index].size();
            while(suffix>valueStart&&(lines[index][suffix-1]==' '||lines[index][suffix-1]=='\t'))--suffix;
        } else {
            while(suffix>valueStart&&(lines[index][suffix-1]==' '||lines[index][suffix-1]=='\t'))--suffix;
        }
        lines[index] = lines[index].substr(0,valueStart) + std::to_string(values.at(key)) + lines[index].substr(suffix);
        written[key]=true;
    }
    size_t sectionEnd = lines.size();
    for (size_t i=0;i<lines.size();++i) {
        auto t=trim(lines[i]);
        if (!t.empty() && t.front()=='[' && t.back()==']') {
            if (foundSection) { sectionEnd=i; break; }
            if (trim(t.substr(1,t.size()-2))=="display") foundSection=true;
        }
    }
    std::vector<std::string> missing;
    for (const auto& key : {"width","height","fullscreen"}) if (replaceValue(key) && !written[key])
        missing.push_back(std::string(key) + " = " + std::to_string(values.at(key)));
    if (!missing.empty()) {
        if (!foundSection) {
            if (!lines.empty() && !lines.back().empty()) appendLine({});
            appendLine("[display]"); sectionEnd=lines.size();
        }
        if (sectionEnd==lines.size() && !endings.empty() && endings.back().empty()) endings.back()=nl;
        std::vector<std::string> addedEndings(missing.size(),nl);
        if (sectionEnd==lines.size() && !trailingNewline) addedEndings.back().clear();
        lines.insert(lines.begin()+static_cast<std::ptrdiff_t>(sectionEnd), missing.begin(), missing.end());
        endings.insert(endings.begin()+static_cast<std::ptrdiff_t>(sectionEnd), addedEndings.begin(), addedEndings.end());
    }
    std::string result;
    for (size_t i=0;i<lines.size();++i) {
        result += lines[i];
        result += endings[i];
    }
    return result;
}
std::map<std::string,std::string> graphicsValues(const pt::AppSettings& s) {
    const auto& g=s.graphics; const auto& r=s.ray_tracing;
    return {{"graphics.anisotropy",std::to_string(g.anisotropy)}, {"graphics.enhanced_textures",g.enhanced_textures?"1":"0"},
      {"graphics.shadow_quality",std::to_string(g.shadow_quality)}, {"graphics.ambient_occlusion",g.ambient_occlusion?"1":"0"},
      {"graphics.reflections",g.reflections?"1":"0"}, {"graphics.bloom",g.bloom?"1":"0"},
      {"graphics.depth_of_field",g.depth_of_field?"1":"0"}, {"graphics.motion_blur",g.motion_blur?"1":"0"},
      {"graphics.lens_distortion",g.lens_distortion?"1":"0"}, {"graphics.lens_ghosts",g.lens_ghosts?"1":"0"},
      {"graphics.deband",g.deband?"1":"0"}, {"graphics.film_grain",std::to_string(g.film_grain)},
      {"graphics.clarity",std::to_string(g.clarity)}, {"graphics.texture_detail",std::to_string(g.texture_detail)},
      {"graphics.ray_quality",std::to_string(g.ray_quality)}, {"raytracing.shadows",std::to_string(r.shadows)},
      {"raytracing.ambient_occlusion",r.ambient_occlusion?"1":"0"}, {"raytracing.contact_shadows",r.contact_shadows?"1":"0"},
      {"raytracing.reflections",r.reflections?"1":"0"}, {"raytracing.full_shadows",r.full_shadows?"1":"0"},
      {"raytracing.bounce_light",r.bounce_light?"1":"0"}, {"raytracing.glossy_reflections",r.glossy_reflections?"1":"0"}};
}
std::string patchGraphics(const std::string& text, const std::map<std::string,std::string>& values) {
    const auto nl=newlineOf(text); const bool trailing=text.empty()||text.back()=='\n';
    std::vector<std::string> lines,endings;
    for(size_t start=0;start<text.size();) { auto n=text.find('\n',start); auto e=n==std::string::npos?text.size():n; std::string ending=n==std::string::npos?"":"\n"; if(n!=std::string::npos&&e>start&&text[e-1]=='\r'){--e;ending="\r\n";} lines.push_back(text.substr(start,e-start));endings.push_back(ending);start=n==std::string::npos?text.size():n+1; }
    std::map<std::string,size_t> last; std::string section;
    for(size_t i=0;i<lines.size();++i){auto t=trim(lines[i]);if(t.size()>2&&t.front()=='['&&t.back()==']'){section=trim(t.substr(1,t.size()-2));continue;} auto eq=lines[i].find('=');if(eq!=std::string::npos){auto key=section+"."+trim(lines[i].substr(0,eq));if(values.contains(key))last[key]=i;}}
    for(const auto& [key,index]:last){auto eq=lines[index].find('=');size_t v=eq+1;while(v<lines[index].size()&&(lines[index][v]==' '||lines[index][v]=='\t'))++v;size_t end=lines[index].find_first_of(";#",v);if(end==std::string::npos){end=lines[index].size();while(end>v&&(lines[index][end-1]==' '||lines[index][end-1]=='\t'))--end;}else while(end>v&&(lines[index][end-1]==' '||lines[index][end-1]=='\t'))--end;lines[index]=lines[index].substr(0,v)+values.at(key)+lines[index].substr(end);}
    for(const auto& wanted:{std::string("raytracing"),std::string("graphics")}) { bool exists=false;size_t end=lines.size();section.clear(); for(size_t i=0;i<lines.size();++i){auto t=trim(lines[i]);if(t.size()>2&&t.front()=='['&&t.back()==']'){if(section==wanted){end=i;break;}section=trim(t.substr(1,t.size()-2));if(section==wanted)exists=true;}} if(section==wanted)exists=true;
      std::vector<std::string> add; for(const auto& [key,value]:values)if(key.rfind(wanted+".",0)==0&&!last.contains(key))add.push_back(key.substr(wanted.size()+1)+" = "+value);
      if(add.empty())continue; if(!exists){if(!lines.empty()&&!lines.back().empty()){lines.push_back("");endings.push_back(nl);}lines.push_back("["+wanted+"]");endings.push_back(nl);end=lines.size();}
      if(end==lines.size()&&!endings.empty()&&endings.back().empty())endings.back()=nl;std::vector<std::string> ens(add.size(),nl);if(end==lines.size()&&!trailing)ens.back().clear();lines.insert(lines.begin()+static_cast<std::ptrdiff_t>(end),add.begin(),add.end());endings.insert(endings.begin()+static_cast<std::ptrdiff_t>(end),ens.begin(),ens.end()); }
    std::string out;for(size_t i=0;i<lines.size();++i)out+=lines[i]+endings[i];return out;
}
pt::AppSettings decodeGraphics(const std::string& text) {
    pt::AppSettings s; std::istringstream in(text); std::string line,section;
    while(std::getline(in,line)){auto t=trim(line);if(t.empty()||t[0]==';'||t[0]=='#')continue;if(t.front()=='['&&t.back()==']'){section=trim(t.substr(1,t.size()-2));continue;}auto eq=t.find('=');if(eq==std::string::npos)continue;std::string key=section+"."+trim(t.substr(0,eq)),v=trim(t.substr(eq+1));if(v.size()>1&&v.front()=='"'&&v.back()=='"')v=v.substr(1,v.size()-2);try{
      auto integer=[&]{return std::stoi(v);};auto real=[&]{return std::stof(v);};auto boolean=[&]{return v=="1"||v=="true"||v=="yes"||v=="on";};auto&g=s.graphics;auto&r=s.ray_tracing;
      if(key=="graphics.anisotropy")g.anisotropy=integer();else if(key=="graphics.enhanced_textures")g.enhanced_textures=boolean();else if(key=="graphics.shadow_quality")g.shadow_quality=integer();else if(key=="graphics.ambient_occlusion")g.ambient_occlusion=boolean();else if(key=="graphics.reflections")g.reflections=boolean();else if(key=="graphics.bloom")g.bloom=boolean();else if(key=="graphics.depth_of_field")g.depth_of_field=boolean();else if(key=="graphics.motion_blur")g.motion_blur=boolean();else if(key=="graphics.lens_distortion")g.lens_distortion=boolean();else if(key=="graphics.lens_ghosts")g.lens_ghosts=boolean();else if(key=="graphics.deband")g.deband=boolean();else if(key=="graphics.film_grain")g.film_grain=real();else if(key=="graphics.clarity")g.clarity=real();else if(key=="graphics.texture_detail")g.texture_detail=integer();else if(key=="graphics.ray_quality")g.ray_quality=integer();else if(key=="raytracing.shadows")r.shadows=integer();else if(key=="raytracing.ambient_occlusion")r.ambient_occlusion=boolean();else if(key=="raytracing.contact_shadows")r.contact_shadows=boolean();else if(key=="raytracing.reflections")r.reflections=boolean();else if(key=="raytracing.full_shadows")r.full_shadows=boolean();else if(key=="raytracing.bounce_light")r.bounce_light=boolean();else if(key=="raytracing.glossy_reflections")r.glossy_reflections=boolean();
    }catch(...){} }
    return s;
}
bool sameEdited(const DisplaySettings& a, const DisplaySettings& b, const DisplaySettingsEdit& edit) {
    return (!edit.width || a.width == b.width) && (!edit.height || a.height == b.height) &&
           (!edit.windowMode || a.windowMode == b.windowMode);
}
}
bool verifiedRuntime(const fs::path& bundle, Platform platform, const std::vector<SupportedRuntime>& supported) {
    const auto inspection=InspectBundle(bundle,supported);
    return platform==Platform::Linux ? inspection.linux_runtime.verified : inspection.windows_runtime.verified;
}
DisplaySettingsSnapshot ReadDisplaySettings(const fs::path& bundle, Platform platform,
    const std::vector<SupportedRuntime>& supported, RayTracingCapability capability) {
    if (!verifiedRuntime(bundle,platform,supported)) {
        DisplaySettingsSnapshot result;
        result.error="Runtime unverified. Align with a matching supported release before editing settings.";
        return result;
    }
    QString error;
    if (!containedSettings(bundle,platform,error)) { DisplaySettingsSnapshot result; result.error=error; return result; }
    const auto source=readText(settingsPath(bundle,platform));auto result=decode(source);if(result.available){const bool rt=capability==RayTracingCapability::Supported;result.value.graphicsPreset=pt::DetectGraphicsPreset(decodeGraphics(source.bytes),rt);result.value.rayTracingCapabilityKnown=capability!=RayTracingCapability::Unknown;result.value.rayTracingSupported=rt;}return result;
}
DisplaySettingsApplyResult ApplyDisplaySettings(const fs::path& bundle, Platform platform,
    const std::vector<SupportedRuntime>& supported, const DisplaySettingsSnapshot& base, const DisplaySettingsEdit& edit) {
    DisplaySettingsApplyResult result;
    if (!base.available) { result.status=ApplyStatus::Failed; result.error="Reload platform settings before applying changes."; return result; }
    if ((edit.width && (*edit.width<320 || *edit.width>16384)) ||
        (edit.height && (*edit.height<240 || *edit.height>16384)) ||
        (edit.windowMode && (*edit.windowMode<0 || *edit.windowMode>2)) ||
        (edit.graphicsPreset && *edit.graphicsPreset==pt::GraphicsPreset::Custom)) {
        result.status=ApplyStatus::Failed; result.error=edit.graphicsPreset&&*edit.graphicsPreset==pt::GraphicsPreset::Custom
            ? "Custom is detected from settings and cannot be applied as a preset."
            : "Display values are outside the supported runtime range."; return result;
    }
    QString guardError; bool conflict=false, failed=false;
    const bool guarded=WithStoppedBundle(bundle,[&] {
        if (!verifiedRuntime(bundle,platform,supported)) {
            result.status=ApplyStatus::Blocked;
            result.error="Runtime unverified. Align with a matching supported release before editing settings.";
            return;
        }
        if (!containedSettings(bundle,platform,result.error)) { failed=true; return; }
        const auto path=settingsPath(bundle,platform);
        const auto source=readText(path);
        auto latest=decode(source);if(latest.available){const bool rt=base.value.rayTracingCapabilityKnown&&base.value.rayTracingSupported;latest.value.graphicsPreset=pt::DetectGraphicsPreset(decodeGraphics(source.bytes),rt);latest.value.rayTracingCapabilityKnown=base.value.rayTracingCapabilityKnown;latest.value.rayTracingSupported=rt;}
        if (!latest.available) { failed=true; result.error=latest.error; return; }
        if (!sameEdited(base.value,latest.value,edit) || (edit.graphicsPreset && base.value.graphicsPreset!=latest.value.graphicsPreset)) { conflict=true; result.error="These settings changed while you were editing. Reload or reconcile the newer values."; return; }
        if (!edit.width && !edit.height && !edit.windowMode && !edit.graphicsPreset) {
            result.persisted=latest; result.status=ApplyStatus::Applied; return;
        }
        std::error_code directoryError;
        fs::create_directory(path.parent_path(),directoryError);
        if (directoryError) { failed=true; result.error="Cannot create platform settings directory."; return; }
        QSaveFile output(QString::fromStdU32String(path.u32string()));
        std::string bytes=patch(source.bytes,edit);
        if(edit.graphicsPreset){auto settings=decodeGraphics(source.bytes);pt::ApplyGraphicsPreset(settings,*edit.graphicsPreset,base.value.rayTracingCapabilityKnown&&base.value.rayTracingSupported);bytes=patchGraphics(bytes,graphicsValues(settings));}
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes.data(),static_cast<qint64>(bytes.size()))!=static_cast<qint64>(bytes.size())) {
            failed=true; result.error="Cannot atomically save display settings: " + output.errorString(); return;
        }
        const auto rechecked=readText(path);
        if (!rechecked.valid || rechecked.exists!=source.exists || rechecked.bytes!=source.bytes ||
            !sameEdited(base.value,decode(rechecked).value,edit)) {
            output.cancelWriting(); conflict=true;
            result.error="Platform settings changed during Apply. Reload or reconcile the newer values.";
            return;
        }
        if (!containedSettings(bundle,platform,result.error) || !verifiedRuntime(bundle,platform,supported)) {
            output.cancelWriting(); result.status=ApplyStatus::Blocked;
            if (result.error.isEmpty()) result.error="Runtime changed during Apply. Align with the matching supported release.";
            return;
        }
        const auto immediatelyBeforeReplace=InspectSession(bundle);
        if (immediatelyBeforeReplace.activity!=Activity::Stopped) {
            output.cancelWriting(); result.status=ApplyStatus::Blocked;
            result.error=immediatelyBeforeReplace.error.isEmpty() ? "Game activity started while settings were being saved." : immediatelyBeforeReplace.error;
            return;
        }
        if (!output.commit()) { failed=true; result.error="Cannot atomically save display settings: " + output.errorString(); return; }
        result.persisted=decode(readText(path));if(result.persisted.available){const bool rt=base.value.rayTracingCapabilityKnown&&base.value.rayTracingSupported;result.persisted.value.graphicsPreset=pt::DetectGraphicsPreset(decodeGraphics(bytes),rt);result.persisted.value.rayTracingCapabilityKnown=base.value.rayTracingCapabilityKnown;result.persisted.value.rayTracingSupported=rt;}
        if (!result.persisted.available) { failed=true; result.error="Settings were written but could not be read back."; return; }
        result.status=ApplyStatus::Applied;
    },&guardError);
    if (!guarded) { result.status=ApplyStatus::Blocked; result.error=guardError; }
    else if (conflict) result.status=ApplyStatus::Conflict;
    else if (failed) result.status=ApplyStatus::Failed;
    return result;
}
}
