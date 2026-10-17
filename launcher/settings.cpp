#include "settings.h"
#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace pt::launcher {
namespace fs = std::filesystem;
namespace {
fs::path settingsPath(const fs::path& bundle, Platform platform) {
    return bundle / (platform == Platform::Linux ? "linux" : "windows") / "data" / "pt.ini";
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
            found = trim(value.substr(equals + 1));
    }
    return found;
}
int runtimeInt(const std::string& value, int fallback, int low, int high) {
    if (value.empty()) return fallback;
    try { return std::clamp(std::stoi(value), low, high); } catch (...) { return fallback; }
}
DisplaySettingsSnapshot decode(const fs::path& path) {
    DisplaySettingsSnapshot result;
    std::error_code fileError;
    if (fs::exists(path,fileError) && !fs::is_regular_file(path,fileError)) {
        result.error="Platform settings path is not a regular file."; return result;
    }
    if (fileError) { result.error="Cannot inspect the platform settings path."; return result; }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::error_code error;
        if (!fs::exists(path,error) && !error) { result.available=true; return result; }
        result.error = "Cannot read platform settings. Check the runtime folder and permissions."; return result;
    }
    std::string text((std::istreambuf_iterator<char>(input)), {});
    if (input.bad()) { result.error = "Cannot read platform settings completely."; return result; }
    result.available = true;
    result.value.width = runtimeInt(keyValue(text, "width"), 1600, 320, 16384);
    result.value.height = runtimeInt(keyValue(text, "height"), 900, 240, 16384);
    result.value.windowMode = runtimeInt(keyValue(text, "fullscreen"), 0, 0, 2);
    return result;
}
std::string newlineOf(const std::string& text) { return text.find("\r\n") != std::string::npos ? "\r\n" : "\n"; }
std::string patch(const std::string& text, const DisplaySettingsEdit& edit) {
    const auto nl = newlineOf(text); const bool trailingNewline=text.empty() || text.back()=='\n';
    std::vector<std::string> lines; std::istringstream in(text); std::string line;
    while (std::getline(in, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); lines.push_back(line); }
    if (text.empty()) lines.clear();
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
            if (!lines.empty() && !lines.back().empty()) lines.push_back({});
            lines.push_back("[display]"); sectionEnd=lines.size();
        }
        lines.insert(lines.begin()+static_cast<std::ptrdiff_t>(sectionEnd), missing.begin(), missing.end());
    }
    std::string result;
    for (size_t i=0;i<lines.size();++i) {
        result += lines[i];
        if (i+1<lines.size() || trailingNewline) result += nl;
    }
    return result;
}
bool sameEdited(const DisplaySettings& a, const DisplaySettings& b, const DisplaySettingsEdit& edit) {
    return (!edit.width || a.width == b.width) && (!edit.height || a.height == b.height) &&
           (!edit.windowMode || a.windowMode == b.windowMode);
}
}
DisplaySettingsSnapshot ReadDisplaySettings(const fs::path& bundle, Platform platform) {
    return decode(settingsPath(bundle, platform));
}
DisplaySettingsApplyResult ApplyDisplaySettings(const fs::path& bundle, Platform platform,
    const DisplaySettingsSnapshot& base, const DisplaySettingsEdit& edit) {
    DisplaySettingsApplyResult result;
    if (!base.available) { result.status=ApplyStatus::Failed; result.error="Reload platform settings before applying changes."; return result; }
    if ((edit.width && (*edit.width<320 || *edit.width>16384)) ||
        (edit.height && (*edit.height<240 || *edit.height>16384)) ||
        (edit.windowMode && (*edit.windowMode<0 || *edit.windowMode>2))) {
        result.status=ApplyStatus::Failed; result.error="Display values are outside the supported runtime range."; return result;
    }
    QString guardError; bool conflict=false, failed=false;
    const bool guarded=WithStoppedBundle(bundle,[&] {
        const auto path=settingsPath(bundle,platform);
        auto latest=decode(path);
        if (!latest.available) { failed=true; result.error=latest.error; return; }
        if (!sameEdited(base.value,latest.value,edit)) { conflict=true; result.error="These display settings changed while you were editing. Reload or reconcile the newer values."; return; }
        std::ifstream input(path,std::ios::binary);
        std::string source((std::istreambuf_iterator<char>(input)),{});
        if (input.bad()) { failed=true; result.error="Cannot read the latest platform settings text."; return; }
        QSaveFile output(QString::fromStdU32String(path.u32string()));
        const auto bytes=patch(source,edit);
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes.data(),static_cast<qint64>(bytes.size()))!=static_cast<qint64>(bytes.size())) {
            failed=true; result.error="Cannot atomically save display settings: " + output.errorString(); return;
        }
        std::ifstream recheckInput(path,std::ios::binary);
        std::string rechecked((std::istreambuf_iterator<char>(recheckInput)),{});
        if (recheckInput.bad() || rechecked!=source) {
            output.cancelWriting(); conflict=true;
            result.error="Platform settings changed during Apply. Reload or reconcile the newer values.";
            return;
        }
        const auto immediatelyBeforeReplace=InspectSession(bundle);
        if (immediatelyBeforeReplace.activity!=Activity::Stopped) {
            output.cancelWriting(); result.status=ApplyStatus::Blocked;
            result.error=immediatelyBeforeReplace.error.isEmpty() ? "Game activity started while settings were being saved." : immediatelyBeforeReplace.error;
            return;
        }
        if (!output.commit()) { failed=true; result.error="Cannot atomically save display settings: " + output.errorString(); return; }
        result.persisted=decode(path);
        if (!result.persisted.available) { failed=true; result.error="Settings were written but could not be read back."; return; }
        result.status=ApplyStatus::Applied;
    },&guardError);
    if (!guarded) { result.status=ApplyStatus::Blocked; result.error=guardError; }
    else if (conflict) result.status=ApplyStatus::Conflict;
    else if (failed) result.status=ApplyStatus::Failed;
    return result;
}
}
