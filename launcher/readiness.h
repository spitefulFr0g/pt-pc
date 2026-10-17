#pragma once

#include "inspection.h"
#include <QString>

namespace pt::launcher {
enum class LaunchMode { Flat, VR };
enum class ReadinessState { Ready, Blocked, Unknown };
struct ModeReadiness {
    LaunchMode mode = LaunchMode::Flat;
    ReadinessState state = ReadinessState::Unknown;
    QString explanation;
};

// This operation only inspects the matching bundle/runtime and the host's
// OpenXR loader/runtime registration. It never creates an XR instance and
// therefore cannot claim that a headset is connected or presenting.
// Requires QCoreApplication and the matching pt-launcher-readiness-probe next
// to the application. Host probes have a clamped 1–2000 ms work budget plus
// 100 ms for child cleanup and up to 100 ms for forced helper reaping.
ModeReadiness CheckReadiness(const BundleInspection& bundle, LaunchMode mode,
                             int probeTimeoutMs = 200);
}
