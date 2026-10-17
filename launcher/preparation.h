#pragma once
#include "inspection.h"
#include <QString>
#include <QStringList>
#include <functional>

namespace pt::launcher {
struct PreparationProgress { QString message; quint64 done = 0, total = 0; };
struct PreparationResult { bool ready = false; bool cancelled = false; QString error; QStringList notes; };
// Blocking operations: run on a worker. Callbacks run on that same worker.
PreparationResult PrepareGame(const std::filesystem::path& bundle, const std::filesystem::path& source,
    const std::vector<SupportedRuntime>& supported, const std::function<bool()>& cancelled = {},
    const std::function<void(const PreparationProgress&)>& progress = {});
PreparationResult RecoverPreparation(const std::filesystem::path& bundle);
bool PreparationNeedsRecovery(const std::filesystem::path& bundle);
}
