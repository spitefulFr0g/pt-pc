#pragma once
// Test-build-only driver: real SDL input into the actual launcher buttons.
#include <SDL3/SDL.h>
#include <QElapsedTimer>
#include <signal.h>

inline void DriveControllerLaunch(QWidget* window, pt::launcher::ControllerAdapter* adapter,
                                  const std::filesystem::path& bundle, QPushButton* action) {
    SDL_VirtualJoystickDesc desc{};
    desc.version = sizeof(desc); desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask = 1u << SDL_GAMEPAD_BUTTON_SOUTH;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_LEFTX) | (1u << SDL_GAMEPAD_AXIS_LEFTY);
    const auto id = SDL_AttachVirtualJoystick(&desc);
    auto* pad = SDL_OpenJoystick(id);
    if (!pad) qFatal("Cannot attach virtual controller");
    auto* timer = new QTimer(window); timer->setInterval(10);
    auto elapsed = std::make_shared<QElapsedTimer>(); elapsed->start();
    auto phase = std::make_shared<int>(0);
    auto phaseTime = std::make_shared<qint64>(0);
    QObject::connect(timer, &QTimer::timeout, window, [=] {
        const auto now = elapsed->elapsed();
        if (now > 8000) qFatal("Controller launch acceptance timed out at phase %d", *phase);
        auto session = pt::launcher::InspectSession(bundle);
        switch (*phase) {
        case 0:
            if (!action->isEnabled() || now < 200) return;
            window->activateWindow(); action->setFocus();
            *phaseTime = now; *phase = 1; break;
        case 1:
            if (now - *phaseTime < 100) return;
            SDL_SetJoystickVirtualButton(pad, SDL_GAMEPAD_BUTTON_SOUTH, true);
            *phaseTime = now; *phase = 2; break;
        case 2:
            // In particular, asynchronous VR must release at request time, while
            // its readiness/launch worker can still be running.
            if (now - *phaseTime < 40) return;
            if (SDL_GetGamepadFromID(id)) qFatal("Controller handle retained during launch handoff, activity=%d, error=%s, focus=%s", int(session.activity), qPrintable(session.error), qPrintable(QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : "none"));
            if (session.activity != pt::launcher::Activity::Running) return;
            *phaseTime = now; *phase = 3; break;
        case 3:
            if (now - *phaseTime < 200) return;
            if (session.activity != pt::launcher::Activity::Running) qFatal("Game did not remain running");
            kill(session.pid, SIGTERM);
            *phase = 4; break;
        case 4:
            if (session.activity != pt::launcher::Activity::Stopped || !action->isEnabled()) return;
            window->activateWindow(); action->setFocus();
            adapter->reopenAfterHandoff();
            *phaseTime = now; *phase = 5; break;
        case 5:
            if (now - *phaseTime < 250) return;
            if (session.activity != pt::launcher::Activity::Stopped) qFatal("Held A launched again on return");
            SDL_SetJoystickVirtualButton(pad, SDL_GAMEPAD_BUTTON_SOUTH, false);
            *phaseTime = now; *phase = 6; break;
        case 6:
            if (now - *phaseTime < 100) return;
            SDL_SetJoystickVirtualButton(pad, SDL_GAMEPAD_BUTTON_SOUTH, true);
            *phase = 7; break;
        case 7:
            if (session.activity != pt::launcher::Activity::Running) return;
            kill(session.pid, SIGTERM);
            adapter->releaseForHandoff(); SDL_CloseJoystick(pad); SDL_DetachVirtualJoystick(id);
            timer->stop(); window->close(); break;
        }
    });
    timer->start();
}
