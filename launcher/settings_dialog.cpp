#include "settings_dialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace pt::launcher {
SettingsDialog::SettingsDialog(std::filesystem::path bundle, Platform platform, QWidget* parent)
    : QDialog(parent), bundle_(std::move(bundle)), platform_(platform) {
    setWindowTitle("Display settings");
    setModal(true);
    setObjectName("displaySettingsDialog");
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    width_ = new QSpinBox(this); width_->setObjectName("displayWidth"); width_->setRange(320,16384); width_->setEnabled(false);
    height_ = new QSpinBox(this); height_->setObjectName("displayHeight"); height_->setRange(240,16384); height_->setEnabled(false);
    mode_ = new QComboBox(this); mode_->setObjectName("displayWindowMode");
    mode_->setEnabled(false);
    mode_->addItems({"Windowed","Borderless fullscreen","Exclusive fullscreen"});
    form->addRow("Resolution width",width_); form->addRow("Resolution height",height_); form->addRow("Window mode",mode_);
    layout->addLayout(form);
    status_ = new QLabel(this); status_->setObjectName("settingsStatus"); status_->setWordWrap(true); layout->addWidget(status_);
    auto* buttons = new QDialogButtonBox(this);
    auto* applyButton=buttons->addButton("Apply",QDialogButtonBox::ApplyRole); applyButton->setObjectName("settingsApply");
    auto* discardButton=buttons->addButton("Discard",QDialogButtonBox::DestructiveRole); discardButton->setObjectName("settingsDiscard");
    auto* reloadButton=buttons->addButton("Reload",QDialogButtonBox::ResetRole); reloadButton->setObjectName("settingsReload");
    auto* closeButton=buttons->addButton(QDialogButtonBox::Close); closeButton->setObjectName("settingsClose");
    layout->addWidget(buttons);
    connect(applyButton,&QPushButton::clicked,this,[this]{apply();});
    connect(discardButton,&QPushButton::clicked,this,[this]{reload(); message_="Changes discarded."; status_->setText(message_);});
    connect(reloadButton,&QPushButton::clicked,this,[this]{reload(); message_="Settings reloaded from disk."; status_->setText(message_);});
    connect(closeButton,&QPushButton::clicked,this,[this]{if(confirmDiscard()) accept();});
    reload();
    gameWasRunning_=InspectSession(bundle_).activity==Activity::Running;
    activityMonitor_=new QTimer(this); activityMonitor_->setInterval(500);
    connect(activityMonitor_,&QTimer::timeout,this,[this] {
        const auto activity=InspectSession(bundle_).activity;
        if(activity==Activity::Running) gameWasRunning_=true;
        if(gameWasRunning_ && activity==Activity::Stopped) {
            gameWasRunning_=false;
            const bool edited=width_->value()!=base_.value.width || height_->value()!=base_.value.height ||
                              mode_->currentIndex()!=base_.value.windowMode;
            if(!edited) {
                reload();
                message_="Game exited. Display settings reloaded from the runtime.";
            } else {
                message_="Game exited and may have changed display settings. Apply to reconcile, or Reload to discard your edits.";
            }
            status_->setText(message_);
        }
    });
    activityMonitor_->start();
}
QString SettingsDialog::statusText() const { return status_->text(); }
void SettingsDialog::reload() {
    base_=ReadDisplaySettings(bundle_,platform_);
    width_->setEnabled(base_.available); height_->setEnabled(base_.available); mode_->setEnabled(base_.available);
    if (!base_.available) { message_=base_.error; status_->setText(message_); return; }
    width_->setValue(base_.value.width); height_->setValue(base_.value.height); mode_->setCurrentIndex(base_.value.windowMode);
    message_.clear(); status_->setText("Loaded current platform settings.");
}
void SettingsDialog::apply() {
    if (!base_.available) { message_="Settings are unavailable. Reload after correcting the runtime settings file."; status_->setText(message_); return; }
    DisplaySettingsEdit edit;
    if (width_->value()!=base_.value.width) edit.width=width_->value();
    if (height_->value()!=base_.value.height) edit.height=height_->value();
    if (mode_->currentIndex()!=base_.value.windowMode) edit.windowMode=mode_->currentIndex();
    const auto result=ApplyDisplaySettings(bundle_,platform_,base_,edit);
    message_=result.error;
    if (result.status==ApplyStatus::Applied) {
        base_=result.persisted;
        width_->setValue(base_.value.width); height_->setValue(base_.value.height); mode_->setCurrentIndex(base_.value.windowMode);
        message_="Display settings saved and read back.";
    } else if (result.status==ApplyStatus::Conflict) {
        QMessageBox prompt(QMessageBox::Warning,"Settings changed",result.error,QMessageBox::NoButton,this);
        auto* reconcile=prompt.addButton("Reconcile my edits",QMessageBox::AcceptRole);
        auto* reloadButton=prompt.addButton("Reload newer settings",QMessageBox::ResetRole);
        prompt.setDefaultButton(reloadButton); prompt.setEscapeButton(reloadButton); prompt.exec();
        if (prompt.clickedButton()==reconcile) {
            auto newer=ReadDisplaySettings(bundle_,platform_);
            if (newer.available) { base_=newer; message_="Newer settings loaded as the base. Apply again to reconcile your edited values."; }
            else message_=newer.error;
            } else reload();
    } else if (result.status==ApplyStatus::Blocked) message_="Game activity blocks settings writes. " + result.error;
    status_->setText(message_);
}
bool SettingsDialog::confirmDiscard() {
    const bool changed=width_->value()!=base_.value.width || height_->value()!=base_.value.height || mode_->currentIndex()!=base_.value.windowMode;
    if (!changed) return true;
    QMessageBox prompt(QMessageBox::Question,"Discard display changes?","Unsaved display changes will be discarded.",QMessageBox::NoButton,this);
    auto* keep=prompt.addButton("Keep editing",QMessageBox::RejectRole);
    auto* discard=prompt.addButton("Discard changes",QMessageBox::DestructiveRole);
    prompt.setDefaultButton(keep); prompt.setEscapeButton(keep); prompt.exec();
    if (prompt.clickedButton()==discard) { reload(); return true; }
    return false;
}
void SettingsDialog::reject() { if (confirmDiscard()) QDialog::reject(); }
}
