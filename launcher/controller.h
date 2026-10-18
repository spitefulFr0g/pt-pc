#pragma once

#include <QWidget>
#include <QPointer>
#include <functional>
#include <memory>

class QTimer;

namespace pt::launcher {
enum class ControllerAction { Up, Down, Left, Right, Activate, Back, Edit };

// Normalized launcher action seam shared by SDL and Qt controls. Edit selects the
// native draft and sends QEvent::RequestSoftwareInputPanel to the field; the later
// keyboard can consume that public Qt event. Combo Edit opens its native choices.
bool DispatchControllerAction(QWidget* scope, ControllerAction action);

class ControllerAdapter final : public QObject {
public:
    explicit ControllerAdapter(QWidget* window, std::function<bool()> ownsSession = {}, QObject* parent = nullptr);
    ~ControllerAdapter() override;
    bool available() const;
    void releaseForHandoff();
    void reopenAfterHandoff();
private:
    void pump();
    void dispatch(QWidget* scope, ControllerAction action);
    QWidget* window_;
    QPointer<QWidget> lastFocus_;
    std::function<bool()> ownsSession_;
    QTimer* timer_;
    void* gamepad_ = nullptr;
    int instance_ = -1;
    bool ownsSdl_ = false;
    bool armed_ = false;
    bool handedOff_ = false;
    bool southWasDown_ = false;
    bool eastWasDown_ = false;
    bool westWasDown_ = false;
    int horizontal_ = 0;
    int vertical_ = 0;
    qint64 nextRepeatMs_ = 0;
    qint64 nextOwnershipCheckMs_ = 0;
    bool ownsSessionCached_ = true;
};
}
