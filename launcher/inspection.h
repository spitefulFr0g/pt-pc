#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace pt::launcher {
struct SupportedRuntime {
    std::string platform;
    std::string sha256;
    std::string version;
    bool supports_vr = false;
};
struct RuntimeInspection {
    bool present = false;
    bool verified = false;
    bool settings = false;
    bool logs = false;
    bool supports_vr = false;
    std::string detail;
    std::string version;
};
struct BundleInspection {
    std::filesystem::path root;
    bool archives = false;
    bool path_list = false;
    bool source = false;
    bool saves = false;
    bool mods = false;
    bool can_integrate_settings = false;
    RuntimeInspection linux_runtime;
    RuntimeInspection windows_runtime;
    std::string error;
};
BundleInspection InspectBundle(const std::filesystem::path& root,
    const std::vector<SupportedRuntime>& supported = {});
}
