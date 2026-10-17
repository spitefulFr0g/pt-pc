#pragma once
#include "session.h"
#include "engine/platform/graphics_presets.h"
#include <optional>

namespace pt::launcher {
enum class Platform { Linux, Windows };
struct DisplaySettings {
    int width = 1600;
    int height = 900;
    int windowMode = 0; // Supported runtime: 0 windowed, 1 borderless, 2 exclusive.
    pt::GraphicsPreset graphicsPreset = pt::GraphicsPreset::Custom;
    bool rayTracingCapabilityKnown = false;
    bool rayTracingSupported = false;
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
    std::optional<pt::GraphicsPreset> graphicsPreset;
};
enum class ApplyStatus { Applied, Conflict, Blocked, Failed };
struct DisplaySettingsApplyResult {
    ApplyStatus status = ApplyStatus::Failed;
    DisplaySettingsSnapshot persisted;
    QString error;
};
// Public settings contract for subsequent settings slices (#8/#9): callers supply
// the same explicit release identities as InspectBundle/PrepareGame. Empty or
// nonmatching identities make Read unavailable; Apply revalidates under the bundle
// mutation guard. A snapshot is the user's comparison base, and only optional edit
// fields are deliberate changes. Conflict requires reload or dirty-field rebase.
// No edited fields means no write. INI input is bounded to 4 MiB. Linked platform,
// data, or settings paths are rejected. A missing data directory is created on save.
// Conflict detection compares/patches one byte snapshot and checks it before commit;
// it cannot provide compare-and-swap against an uncooperative external writer.
// Adapter pinned to the unchanged supported runtime settings semantics at source revision 60fe8b3.
inline constexpr const char* kDisplaySettingsRuntimeRevision = "60fe8b324aca24c264e89378ae0d2c61f3a674af";
// Capability is deliberately explicit. Unknown is treated as no ray tracing support.
enum class RayTracingCapability { Unknown, Unsupported, Supported };
DisplaySettingsSnapshot ReadDisplaySettings(const std::filesystem::path& bundle, Platform platform,
    const std::vector<SupportedRuntime>& supported, RayTracingCapability capability = RayTracingCapability::Unknown);
DisplaySettingsApplyResult ApplyDisplaySettings(const std::filesystem::path& bundle, Platform platform,
    const std::vector<SupportedRuntime>& supported, const DisplaySettingsSnapshot& base, const DisplaySettingsEdit& edit);
}
