#pragma once
#include "settings.h"
#include <QDialog>

class QSpinBox;
class QComboBox;
class QLabel;
class QTimer;
namespace pt::launcher {
class SettingsDialog final : public QDialog {
public:
    SettingsDialog(std::filesystem::path bundle, Platform platform, std::vector<SupportedRuntime> supported, QWidget* parent = nullptr);
    void reject() override;
    bool loaded() const { return base_.available; }
    QString statusText() const;
private:
    void reload();
    void apply();
    bool confirmDiscard();
    std::filesystem::path bundle_;
    Platform platform_;
    std::vector<SupportedRuntime> supported_;
    DisplaySettingsSnapshot base_;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QComboBox* mode_ = nullptr;
    QLabel* status_ = nullptr;
    QTimer* activityMonitor_ = nullptr;
    bool gameWasRunning_ = false;
    QString message_;
};
}
