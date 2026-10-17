#include "controller.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>
#include <SDL3/SDL.h>

namespace pt::launcher {
namespace {
constexpr int deadZone = 8000;
constexpr int repeatDelayMs = 360;
constexpr int repeatRateMs = 110;
constexpr int maxEventsPerPump = 128;

bool usable(QWidget* widget) {
    return widget && widget->isVisible() && widget->isEnabled() && widget->focusPolicy() != Qt::NoFocus;
}

bool ownedBy(QWidget* root, QWidget* widget) {
    for (QWidget* current = widget; current; current = current->parentWidget())
        if (current == root) return true;
    return false;
}

void scrollIntoView(QWidget* focused) {
    for (QWidget* parent = focused ? focused->parentWidget() : nullptr; parent; parent = parent->parentWidget()) {
        if (auto* area = qobject_cast<QScrollArea*>(parent)) {
            area->ensureWidgetVisible(focused, 12, 12);
            return;
        }
    }
}

void focusStep(QWidget* scope, bool backwards) {
    if (!scope) return;
    QWidget* current = QApplication::focusWidget();
    if (!current || !scope->isAncestorOf(current)) current = scope;
    QWidget* candidate = current;
    for (int count = 0; count < 256; ++count) {
        candidate = backwards ? candidate->previousInFocusChain() : candidate->nextInFocusChain();
        if (!candidate || candidate == current) return;
        if (candidate == scope || !scope->isAncestorOf(candidate)) continue;
        if (usable(candidate)) {
            candidate->setFocus(Qt::TabFocusReason);
            scrollIntoView(candidate);
            return;
        }
    }
}
}

bool DispatchControllerAction(QWidget* scope, ControllerAction action) {
    if (!scope) return false;
    QWidget* focused = QApplication::focusWidget();
    if (!focused || (focused != scope && !scope->isAncestorOf(focused))) focused = scope;
    switch (action) {
    case ControllerAction::Up: focusStep(scope, true); return true;
    case ControllerAction::Down: focusStep(scope, false); return true;
    case ControllerAction::Left:
    case ControllerAction::Right:
        if (auto* combo = qobject_cast<QComboBox*>(focused)) {
            combo->setCurrentIndex((combo->currentIndex() + (action == ControllerAction::Right ? 1 : combo->count() - 1)) % combo->count());
            return true;
        }
        return false;
    case ControllerAction::Activate:
        if (auto* button = qobject_cast<QAbstractButton*>(focused); button && button->isEnabled()) { button->click(); return true; }
        if (auto* combo = qobject_cast<QComboBox*>(focused)) { combo->showPopup(); return true; }
        return false;
    case ControllerAction::Back: {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        return QApplication::sendEvent(focused, &event);
    }
    case ControllerAction::Edit:
        if (auto* line = qobject_cast<QLineEdit*>(focused)) { line->setFocus(Qt::OtherFocusReason); return true; }
        return false;
    }
    return false;
}

ControllerAdapter::ControllerAdapter(QWidget* window, std::function<bool()> ownsSession, QObject* parent)
    : QObject(parent), window_(window), ownsSession_(std::move(ownsSession)), timer_(new QTimer(this)) {
    // Use SDL's mapped gamepad API, but leave the Deck's raw HIDAPI path to Steam Input.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "0");
    constexpr Uint32 required = SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS;
    if ((SDL_WasInit(required) & required) != required) {
        ownsSdl_ = SDL_InitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS);
    }
    timer_->setInterval(8);
    connect(timer_, &QTimer::timeout, this, [this] { pump(); });
    timer_->start();
    if (window_) {
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
            if (!now || !usable(now)) return;
            if (now->window() == window_) lastFocus_ = now;
            auto* hints = now->window()->findChild<QLabel*>("controllerHints");
            if (hints) {
                const bool editable = qobject_cast<QLineEdit*>(now) || qobject_cast<QTextEdit*>(now) ||
                    qobject_cast<QComboBox*>(now) || qobject_cast<QSpinBox*>(now);
                hints->setText(editable ? "D-pad / stick: move    A: select    B: back    X: edit" :
                    "D-pad / stick: move    A: select    B: back");
            }
        });
        connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState) {
            armed_ = false; horizontal_ = vertical_ = 0; nextRepeatMs_ = 0;
            if (window_ && QApplication::activeWindow() && QApplication::applicationState() == Qt::ApplicationActive) reopenAfterHandoff();
            else releaseForHandoff();
        });
    }
}

ControllerAdapter::~ControllerAdapter() {
    releaseForHandoff();
    if (ownsSdl_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS);
}

bool ControllerAdapter::available() const { return SDL_WasInit(SDL_INIT_GAMEPAD) != 0; }

void ControllerAdapter::releaseForHandoff() {
    if (gamepad_) { SDL_CloseGamepad(static_cast<SDL_Gamepad*>(gamepad_)); gamepad_ = nullptr; }
    instance_ = -1; handedOff_ = true; armed_ = false;
    horizontal_ = vertical_ = 0; nextRepeatMs_ = 0; southWasDown_ = eastWasDown_ = westWasDown_ = false;
    if (ownsSession_) {
        ownsSessionCached_ = ownsSession_();
        nextOwnershipCheckMs_ = QDateTime::currentMSecsSinceEpoch() + 100;
    }
}

void ControllerAdapter::reopenAfterHandoff() {
    if (!available() || !window_ || !QApplication::activeWindow() || QApplication::applicationState() != Qt::ApplicationActive || (ownsSession_ && !ownsSession_())) return;
    handedOff_ = false;
    int num = 0;
    SDL_JoystickID* devices = SDL_GetJoysticks(&num);
    for (int i = 0; devices && i < num; ++i) {
        if (!SDL_IsGamepad(devices[i])) continue;
        SDL_Gamepad* candidate = SDL_OpenGamepad(devices[i]);
        if (candidate) { gamepad_ = candidate; instance_ = SDL_GetGamepadID(candidate); break; }
    }
    SDL_free(devices);
    // Require all controls released after opening. A held button cannot activate a focused action.
    armed_ = false; horizontal_ = vertical_ = 0; nextRepeatMs_ = 0; southWasDown_ = eastWasDown_ = westWasDown_ = false;
}

void ControllerAdapter::pump() {
    if (!window_) return;
    // Focus restoration is local Qt state; controller dispatch below still requires an active Qt window.
    const bool launcherOwnsForeground = !QApplication::activeWindow() || QApplication::activeWindow() == window_;
    if (launcherOwnsForeground && !QApplication::focusWidget()) {
        if (usable(lastFocus_)) lastFocus_->setFocus(Qt::OtherFocusReason);
        else focusStep(window_, false);
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now >= nextOwnershipCheckMs_) {
        ownsSessionCached_ = !ownsSession_ || ownsSession_();
        nextOwnershipCheckMs_ = now + 100;
    }
    const bool foreground = QApplication::applicationState() == Qt::ApplicationActive &&
        ownedBy(window_, QApplication::activeWindow()) && window_->isVisible();
    if (!foreground || !ownsSessionCached_) {
        if (!handedOff_) releaseForHandoff();
        SDL_Event ignored{};
        for (int count = 0; count < maxEventsPerPump && SDL_PollEvent(&ignored); ++count) {}
        return;
    }
    if (handedOff_) reopenAfterHandoff();
    if (!gamepad_) reopenAfterHandoff();
    if (!gamepad_) { SDL_PumpEvents(); return; }
    SDL_Event event{};
    for (int count = 0; count < maxEventsPerPump && SDL_PollEvent(&event); ++count) {
        if (event.type == SDL_EVENT_GAMEPAD_REMOVED && event.gdevice.which == instance_) {
            releaseForHandoff();
            break;
        }
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) { armed_ = false; }
    }
    if (!gamepad_) return;
    SDL_PumpEvents();
    auto* pad = static_cast<SDL_Gamepad*>(gamepad_);
    const Sint16 x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    const Sint16 y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    const bool dpad = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP) || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) ||
        SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    const bool buttonsNeutral = !SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) && !SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST) &&
        !SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST);
    const bool neutral = x > -deadZone && x < deadZone && y > -deadZone && y < deadZone && !dpad && buttonsNeutral;
    if (!armed_) { if (neutral) armed_ = true; else return; }
    const int nextH = x > deadZone ? 1 : x < -deadZone ? -1 : 0;
    const int nextV = y > deadZone ? 1 : y < -deadZone ? -1 : 0;
    QWidget* target = QApplication::activeWindow();
    if (!target || !ownedBy(window_, target)) target = window_;
    const bool dleft = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    const bool dright = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    const bool dup = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    const bool ddown = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    const int navH = dleft ? -1 : dright ? 1 : nextH;
    const int navV = dup ? -1 : ddown ? 1 : nextV;
    if (navH || navV) {
        if (navH != horizontal_ || navV != vertical_) {
            DispatchControllerAction(target, navH ? (navH < 0 ? ControllerAction::Left : ControllerAction::Right) : (navV < 0 ? ControllerAction::Up : ControllerAction::Down));
            nextRepeatMs_ = now + repeatDelayMs;
        } else if (now >= nextRepeatMs_) {
            DispatchControllerAction(target, navH ? (navH < 0 ? ControllerAction::Left : ControllerAction::Right) : (navV < 0 ? ControllerAction::Up : ControllerAction::Down));
            nextRepeatMs_ = now + repeatRateMs;
        }
    } else nextRepeatMs_ = 0;
    horizontal_ = navH; vertical_ = navV;
    const bool south = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
    const bool east = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
    const bool west = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST);
    if (south && !southWasDown_) DispatchControllerAction(target, ControllerAction::Activate);
    if (east && !eastWasDown_) DispatchControllerAction(target, ControllerAction::Back);
    if (west && !westWasDown_) DispatchControllerAction(target, ControllerAction::Edit);
    southWasDown_ = south; eastWasDown_ = east; westWasDown_ = west;
}
}
