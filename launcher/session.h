#pragma once
#include "inspection.h"
#include "readiness.h"
#include <QString>
#include <functional>

namespace pt::launcher {
enum class Activity { Stopped, Running, Uncertain };
struct SessionState {
    Activity activity = Activity::Uncertain;
    qint64 pid = 0;
    std::filesystem::path bundle;
    QString identity;
    QString error;
    QString log;
};
// Public operations used by the UI and by future bundle mutations.
SessionState InspectSession(const std::filesystem::path& bundle);
SessionState PlayFlat(const std::filesystem::path& bundle, const std::vector<SupportedRuntime>& supported,
                      bool tryAnyway = false);
SessionState PlayVR(const std::filesystem::path& bundle, const std::vector<SupportedRuntime>& supported,
                    bool tryAnyway);
bool WithStoppedBundle(const std::filesystem::path& bundle, const std::function<void()>& mutation, QString* error);
bool ReturnToGame(const SessionState& session, QString* error);
}
