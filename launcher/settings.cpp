#include "settings.h"
#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <map>
#include <sstream>
#include <cmath>
#include <type_traits>
#include "engine/core/ascii.h"
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
std::map<std::string,std::string> graphicsValues(const pt::AppSettings& s) {
    const auto& g = s.graphics;
    const auto& r = s.ray_tracing;
    return {
        {"graphics.anisotropy",std::to_string(g.anisotropy)},
        {"graphics.enhanced_textures",g.enhanced_textures?"1":"0"},
      {"graphics.shadow_quality",std::to_string(g.shadow_quality)},
        {"graphics.ambient_occlusion",g.ambient_occlusion?"1":"0"},
      {"graphics.reflections",g.reflections?"1":"0"},
        {"graphics.bloom",g.bloom?"1":"0"},
      {"graphics.depth_of_field",g.depth_of_field?"1":"0"},
        {"graphics.motion_blur",g.motion_blur?"1":"0"},
      {"graphics.lens_distortion",g.lens_distortion?"1":"0"},
        {"graphics.lens_ghosts",g.lens_ghosts?"1":"0"},
      {"graphics.deband",g.deband?"1":"0"},
        {"graphics.film_grain",std::to_string(g.film_grain)},
      {"graphics.clarity",std::to_string(g.clarity)},
        {"graphics.texture_detail",std::to_string(g.texture_detail)},
      {"graphics.ray_quality",std::to_string(g.ray_quality)},
        {"raytracing.shadows",std::to_string(r.shadows)},
      {"raytracing.ambient_occlusion",r.ambient_occlusion?"1":"0"},
        {"raytracing.contact_shadows",r.contact_shadows?"1":"0"},
      {"raytracing.reflections",r.reflections?"1":"0"},
        {"raytracing.full_shadows",r.full_shadows?"1":"0"},
      {"raytracing.bounce_light",r.bounce_light?"1":"0"},
        {"raytracing.glossy_reflections",r.glossy_reflections?"1":"0"}
    };
}
// Replace only the runtime-effective (last) occurrence; retain every other byte.
// Missing affected keys are added within their section.
std::string patchValues(const std::string& text, const std::map<std::string,std::string>& values) {
    const auto newline = newlineOf(text);
    const bool trailingNewline = text.empty() || text.back() == '\n';
    std::vector<std::string> lines, endings;
    for (size_t start = 0; start < text.size();) {
        const auto next = text.find('\n', start);
        auto end = next == std::string::npos ? text.size() : next;
        std::string ending = next == std::string::npos ? "" : "\n";
        if (next != std::string::npos && end > start && text[end-1] == '\r') {
            --end;
            ending = "\r\n";
        }
        lines.push_back(text.substr(start, end-start));
        endings.push_back(ending);
        start = next == std::string::npos ? text.size() : next+1;
    }
    const auto sectionName = [](const std::string& line) -> std::optional<std::string> {
        const auto value = trim(line);
        if (value.size() >= 2 && value.front() == '[' && value.back() == ']')
            return trim(value.substr(1,value.size()-2));
        return std::nullopt;
    };
    std::map<std::string,size_t> last;
    std::string section;
    for (size_t i=0; i<lines.size(); ++i) {
        if (auto name = sectionName(lines[i])) { section = *name; continue; }
        const auto value = trim(lines[i]);
        if (value.empty() || value.front() == ';' || value.front() == '#') continue;
        const auto equals = lines[i].find('=');
        if (equals == std::string::npos) continue;
        const auto key = section + "." + trim(lines[i].substr(0,equals));
        if (values.contains(key)) last[key] = i;
    }
    for (const auto& [key,index] : last) {
        auto& line = lines[index];
        size_t begin = line.find('=')+1;
        while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t')) ++begin;
        size_t end = line.find_first_of(";#",begin);
        if (end == std::string::npos) end = line.size();
        while (end > begin && (line[end-1] == ' ' || line[end-1] == '\t')) --end;
        const bool boolean = key == "graphics.enhanced_textures" ||
            key == "graphics.ambient_occlusion" || key == "graphics.reflections" ||
            key == "graphics.bloom" || key == "graphics.depth_of_field" ||
            key == "graphics.motion_blur" || key == "graphics.lens_distortion" ||
            key == "graphics.lens_ghosts" || key == "graphics.deband" ||
            (key.starts_with("raytracing.") && key != "raytracing.shadows");
        // Upstream numeric reads accept suffixes, but boolean reads compare the
        // entire token. Retain an affected boolean's note on its own line.
        const bool moveComment = boolean && line.find_first_of(";#",begin) != std::string::npos;
        line = line.substr(0,begin) + values.at(key) +
            (moveComment ? newline : "") + line.substr(end);
    }
    std::map<std::string,std::vector<std::string>> missing;
    for (const auto& [key,value] : values) {
        if (last.contains(key)) continue;
        const auto dot = key.find('.');
        missing[key.substr(0,dot)].push_back(key.substr(dot+1) + " = " + value);
    }
    for (const auto& [wanted, additions] : missing) {
        bool found = false;
        size_t insertion = lines.size();
        for (size_t i=0; i<lines.size(); ++i) {
            if (auto name = sectionName(lines[i])) {
                if (found) { insertion = i; break; }
                found = *name == wanted;
            }
        }
        if (!found) {
            if (!endings.empty() && endings.back().empty()) endings.back() = newline;
            if (!lines.empty() && !lines.back().empty()) {
                lines.push_back("");
                endings.push_back(newline);
            }
            lines.push_back("[" + wanted + "]");
            endings.push_back(newline);
            insertion = lines.size();
        }
        if (insertion == lines.size() && !endings.empty() && endings.back().empty())
            endings.back() = newline;
        std::vector<std::string> addedEndings(additions.size(),newline);
        if (insertion == lines.size() && !trailingNewline) addedEndings.back().clear();
        lines.insert(lines.begin()+static_cast<std::ptrdiff_t>(insertion),additions.begin(),additions.end());
        endings.insert(endings.begin()+static_cast<std::ptrdiff_t>(insertion),addedEndings.begin(),addedEndings.end());
    }
    std::string result;
    for (size_t i=0; i<lines.size(); ++i) result += lines[i] + endings[i];
    return result;
}
// Bounded byte adapter to the pinned LoadAppSettings semantics. The differential
// acceptance test links the unchanged upstream file; no renderer is involved.
template<class T>
void Read(const std::map<std::string, std::string>& values, const char* key, T& out) {
    const auto it = values.find(key);
    if (it == values.end()) return;
    try {
        if constexpr (std::is_same_v<T, bool>) {
            auto value = it->second;
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return pt::AsciiLower(static_cast<char>(c)); });
            out = value == "1" || value == "true" || value == "yes" || value == "on";
        } else if constexpr (std::is_same_v<T, int>) {
            out = std::stoi(it->second);
        } else {
            out = std::stof(it->second);
        }
    } catch (...) {
        // Like upstream, invalid numbers retain the default.
    }
}
pt::AppSettings decodeGraphics(const std::string& text) {
    std::map<std::string, std::string> v;
    std::istringstream input(text);
    std::string line, section;
    while (std::getline(input, line)) {
        auto value = trim(line);
        if (value.empty() || value[0] == ';' || value[0] == '#') continue;
        if (value.front() == '[' && value.back() == ']') {
            section = trim(value.substr(1, value.size()-2));
            continue;
        }
        const auto equals = value.find('=');
        if (equals == std::string::npos) continue;
        auto content = trim(value.substr(equals+1));
        if (content.size() >= 2 && content.front() == '"' && content.back() == '"')
            content = content.substr(1, content.size()-2);
        v[section + "." + trim(value.substr(0, equals))] = content;
    }
    using namespace pt;
    AppSettings out;
    Read(v, "raytracing.shadows", out.ray_tracing.shadows);
    Read(v, "raytracing.ambient_occlusion", out.ray_tracing.ambient_occlusion);
    Read(v, "raytracing.contact_shadows", out.ray_tracing.contact_shadows);
    Read(v, "raytracing.reflections", out.ray_tracing.reflections);
    Read(v, "raytracing.full_shadows", out.ray_tracing.full_shadows);
    Read(v, "raytracing.bounce_light", out.ray_tracing.bounce_light);
    Read(v, "raytracing.glossy_reflections", out.ray_tracing.glossy_reflections);
    Read(v, "graphics.anisotropy", out.graphics.anisotropy);
    Read(v, "graphics.enhanced_textures", out.graphics.enhanced_textures);
    Read(v, "graphics.shadow_quality", out.graphics.shadow_quality);
    Read(v, "graphics.ambient_occlusion", out.graphics.ambient_occlusion);
    Read(v, "graphics.reflections", out.graphics.reflections);
    Read(v, "graphics.bloom", out.graphics.bloom);
    Read(v, "graphics.depth_of_field", out.graphics.depth_of_field);
    Read(v, "graphics.motion_blur", out.graphics.motion_blur);
    Read(v, "graphics.lens_distortion", out.graphics.lens_distortion);
    Read(v, "graphics.lens_ghosts", out.graphics.lens_ghosts);
    Read(v, "graphics.deband", out.graphics.deband);
    Read(v, "graphics.film_grain", out.graphics.film_grain);
    Read(v, "graphics.clarity", out.graphics.clarity);
    Read(v, "graphics.texture_detail", out.graphics.texture_detail);
    Read(v, "graphics.ray_quality", out.graphics.ray_quality);
    if (!v.contains("graphics.lens_ghosts")) {
        AppSettings probe = out;
        probe.graphics.lens_ghosts = true;
        out.graphics.lens_ghosts = DetectGraphicsPreset(probe, false) == GraphicsPreset::Original ||
                                   DetectGraphicsPreset(probe, true) == GraphicsPreset::Original;
    }
    if (!v.contains("graphics.deband")) {
        AppSettings probe = out;
        probe.graphics.deband = false;
        out.graphics.deband = !(DetectGraphicsPreset(probe, false) == GraphicsPreset::Original ||
                                DetectGraphicsPreset(probe, true) == GraphicsPreset::Original);
    }
    out.graphics.shadow_quality=std::clamp(out.graphics.shadow_quality,0,3);
    out.graphics.texture_detail=std::clamp(out.graphics.texture_detail,0,2);
    if (out.graphics.ray_quality == kRayQualityOldUltraIni) {
        out.graphics.ray_quality = 2;
    } else {
        out.graphics.ray_quality=std::clamp(out.graphics.ray_quality,0,4);
        if (out.graphics.ray_quality == 2) {
            out.graphics.ray_quality = 3;
        }
    }
    out.graphics.film_grain=std::isfinite(out.graphics.film_grain)?std::clamp(out.graphics.film_grain,0.0f,1.0f):1.0f;
    out.graphics.clarity=std::isfinite(out.graphics.clarity)?std::clamp(out.graphics.clarity,0.0f,1.0f):0.0f;
    out.ray_tracing.shadows = std::clamp(out.ray_tracing.shadows, 0, 2);
    const int anisotropy = out.graphics.anisotropy;
    out.graphics.anisotropy = anisotropy >= 16 ? 16 : anisotropy >= 8 ? 8 : anisotropy >= 4 ? 4 : anisotropy >= 2 ? 2 : 0;
    return out;
}
DisplaySettingsSnapshot snapshot(const SettingsText& source, bool known, bool rt) {
    auto result = decode(source);
    if (result.available) {
        const auto settings = decodeGraphics(source.bytes);
        result.graphics = settings.graphics;
        result.rayTracing = settings.ray_tracing;
        result.value.graphicsPreset = pt::DetectGraphicsPreset(settings, rt);
        result.value.rayTracingCapabilityKnown = known;
        result.value.rayTracingSupported = rt;
    }
    return result;
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
    return snapshot(readText(settingsPath(bundle,platform)),
        capability != RayTracingCapability::Unknown, capability == RayTracingCapability::Supported);
}
DisplaySettingsApplyResult ApplyDisplaySettings(const fs::path& bundle, Platform platform,
    const std::vector<SupportedRuntime>& supported, const DisplaySettingsSnapshot& base, const DisplaySettingsEdit& edit) {
    DisplaySettingsApplyResult result;
    if (!base.available) { result.status=ApplyStatus::Failed; result.error="Reload platform settings before applying changes."; return result; }
    if ((edit.width && (*edit.width<320 || *edit.width>16384)) ||
        (edit.height && (*edit.height<240 || *edit.height>16384)) ||
        (edit.windowMode && (*edit.windowMode<0 || *edit.windowMode>2)) ||
        (edit.graphicsPreset && (*edit.graphicsPreset < pt::GraphicsPreset::Low || *edit.graphicsPreset > pt::GraphicsPreset::Ultra))) {
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
        auto latest = snapshot(source, base.value.rayTracingCapabilityKnown, base.value.rayTracingSupported);
        if (!latest.available) { failed=true; result.error=latest.error; return; }
        if (!sameEdited(base.value,latest.value,edit) || (edit.graphicsPreset && (base.graphics != latest.graphics || base.rayTracing != latest.rayTracing))) { conflict=true; result.error="These settings changed while you were editing. Reload or reconcile the newer values."; return; }
        if (!edit.width && !edit.height && !edit.windowMode && !edit.graphicsPreset) {
            result.persisted=latest; result.status=ApplyStatus::Applied; return;
        }
        std::error_code directoryError;
        fs::create_directory(path.parent_path(),directoryError);
        if (directoryError) { failed=true; result.error="Cannot create platform settings directory."; return; }
        QSaveFile output(QString::fromStdU32String(path.u32string()));
        std::map<std::string,std::string> values;
        if (edit.width) values["display.width"] = std::to_string(*edit.width);
        if (edit.height) values["display.height"] = std::to_string(*edit.height);
        if (edit.windowMode) values["display.fullscreen"] = std::to_string(*edit.windowMode);
        if (edit.graphicsPreset) {
            pt::AppSettings settings;
            pt::ApplyGraphicsPreset(settings, *edit.graphicsPreset,
                base.value.rayTracingCapabilityKnown && base.value.rayTracingSupported);
            values.merge(graphicsValues(settings));
        }
        const std::string bytes = patchValues(source.bytes,values);
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
        const auto readback = readText(path);
        result.persisted = snapshot(readback, base.value.rayTracingCapabilityKnown, base.value.rayTracingSupported);
        if (readback.valid && readback.bytes != bytes) {
            conflict = true;
            result.error = "Settings changed after saving. Reload or reconcile the newer values.";
            return;
        }
        if (!result.persisted.available) { failed=true; result.error="Settings were written but could not be read back."; return; }
        result.status=ApplyStatus::Applied;
    },&guardError);
    if (!guarded) { result.status=ApplyStatus::Blocked; result.error=guardError; }
    else if (conflict) result.status=ApplyStatus::Conflict;
    else if (failed) result.status=ApplyStatus::Failed;
    return result;
}
}
