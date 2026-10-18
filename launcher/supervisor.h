#pragma once
#include "session.h"
#include <QJsonObject>
namespace pt::launcher {
// Versioned local protocol. Empty response means transport failure, never fallback launch.
QJsonObject SessionRequest(const std::filesystem::path&, const QJsonObject&);
QString SessionEndpoint(const std::filesystem::path&);
int SessionEntry(int argc, char** argv, const std::vector<SupportedRuntime>&);
}
