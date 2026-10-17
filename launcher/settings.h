#pragma once
#include "session.h"
#include <optional>

namespace pt::launcher {
enum class Platform { Linux, Windows };
struct DisplaySettings {
    int width = 1600;
    int height = 900;
    int windowMode = 0; // Supported runtime: 0 windowed, 1 borderless, 2 exclusive.
};
struct DisplaySettingsSnapshot {
    bool available = false;
    DisplaySettings value;
    QString error;
};
struct DisplaySettingsEdit {
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> windowMode;
};
enum class ApplyStatus { Applied, Conflict, Blocked, Failed };
struct DisplaySettingsApplyResult {
    ApplyStatus status = ApplyStatus::Failed;
    DisplaySettingsSnapshot persisted;
    QString error;
};
// Adapter pinned to the unchanged supported runtime settings semantics at source revision 60fe8b3.
inline constexpr const char* kDisplaySettingsRuntimeRevision = "60fe8b324aca24c264e89378ae0d2c61f3a674af";
DisplaySettingsSnapshot ReadDisplaySettings(const std::filesystem::path& bundle, Platform platform);
DisplaySettingsApplyResult ApplyDisplaySettings(const std::filesystem::path& bundle, Platform platform,
    const DisplaySettingsSnapshot& base, const DisplaySettingsEdit& edit);
}
