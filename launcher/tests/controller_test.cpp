#include "../controller.h"
#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QEventLoop>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <SDL3/SDL.h>
#include <cassert>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* first = new QPushButton("First", &window);
    auto* disabled = new QPushButton("Disabled", &window); disabled->setEnabled(false);
    auto* last = new QPushButton("Last", &window);
    auto* edit = new QLineEdit(&window);
    edit->setFocusPolicy(Qt::NoFocus);
    auto* hints = new QLabel("D-pad / stick: move    A: select    B: back", &window); hints->setObjectName("controllerHints");
    layout->addWidget(first); layout->addWidget(disabled); layout->addWidget(last); layout->addWidget(edit); layout->addWidget(hints);
    window.show(); window.activateWindow(); QApplication::setActiveWindow(&window);
    app.processEvents();
    first->setFocus(); app.processEvents();
    assert(pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Down));
    app.processEvents();
    assert(QApplication::focusWidget() == last);

    bool ownsSession = true;
    pt::launcher::ControllerAdapter adapter(&window, [&] { return ownsSession; });
    assert(adapter.available());
    SDL_VirtualJoystickDesc description{};
    description.version = sizeof(description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.naxes = SDL_GAMEPAD_AXIS_COUNT;
    description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    description.button_mask = (1u << SDL_GAMEPAD_BUTTON_SOUTH) | (1u << SDL_GAMEPAD_BUTTON_EAST) |
        (1u << SDL_GAMEPAD_BUTTON_WEST) | (1u << SDL_GAMEPAD_BUTTON_DPAD_UP) |
        (1u << SDL_GAMEPAD_BUTTON_DPAD_DOWN) | (1u << SDL_GAMEPAD_BUTTON_DPAD_LEFT) |
        (1u << SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    description.axis_mask = (1u << SDL_GAMEPAD_AXIS_LEFTX) | (1u << SDL_GAMEPAD_AXIS_LEFTY);
    const SDL_JoystickID virtualId = SDL_AttachVirtualJoystick(&description);
    assert(virtualId > 0);
    SDL_Joystick* virtualDevice = SDL_OpenJoystick(virtualId);
    assert(virtualDevice);
    app.processEvents();
    QEventLoop loop;
    QTimer::singleShot(80, &loop, &QEventLoop::quit);
    loop.exec();
    assert(SDL_IsGamepad(virtualId));
    first->setFocus(); app.processEvents();
    assert(SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 5000));
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();
    assert(QApplication::focusWidget() == first); // Inside the documented 8,000-unit dead zone.
    assert(SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 0));
    QTimer::singleShot(50, &loop, &QEventLoop::quit);
    loop.exec();
    int navigationFocusChanges = 0;
    QObject::connect(&app, &QApplication::focusChanged, [&](QWidget*, QWidget* now) {
        if (now == first || now == last) ++navigationFocusChanges;
    });
    assert(SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 24000));
    QTimer::singleShot(80, &loop, &QEventLoop::quit);
    loop.exec();
    assert(QApplication::focusWidget() == last);
    QTimer::singleShot(560, &loop, &QEventLoop::quit);
    loop.exec();
    assert(navigationFocusChanges >= 3); // Immediate move, 360 ms initial delay, then 110 ms repeats.
    SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 0);
    QTimer::singleShot(80, &loop, &QEventLoop::quit);
    loop.exec();

    int activations = 0;
    QObject::connect(first, &QPushButton::clicked, [&] { ++activations; });
    first->setFocus(); app.processEvents();
    assert(SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, true));
    QTimer::singleShot(220, &loop, &QEventLoop::quit);
    loop.exec();
    assert(activations == 1); // A is edge-triggered, even while held across many SDL polls.
    adapter.releaseForHandoff();
    adapter.reopenAfterHandoff();
    QTimer::singleShot(60, &loop, &QEventLoop::quit);
    loop.exec();
    assert(activations == 1); // Held A cannot rearm after handoff.
    assert(SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, false));
    QTimer::singleShot(60, &loop, &QEventLoop::quit);
    loop.exec();
    assert(SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, true));
    QTimer::singleShot(60, &loop, &QEventLoop::quit);
    loop.exec();
    assert(activations == 2);
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, false);

    QDialog modal(&window);
    auto* modalLayout = new QVBoxLayout(&modal);
    auto* modalAction = new QPushButton("Close modal", &modal);
    modalLayout->addWidget(modalAction);
    modal.setModal(true); modal.show(); app.processEvents(); QApplication::setActiveWindow(&modal); modalAction->setFocus(); app.processEvents();
    assert(SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_EAST, true));
    QTimer::singleShot(80, &loop, &QEventLoop::quit);
    loop.exec();
    assert(!modal.isVisible()); // B follows the active modal's normal Escape behavior.
    window.activateWindow(); QApplication::setActiveWindow(&window);
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_EAST, false);
    QTimer::singleShot(60, &loop, &QEventLoop::quit);
    loop.exec();
    assert(QApplication::focusWidget() == first); // The prior valid launcher control regains focus.

    QWidget unrelatedWindow;
    unrelatedWindow.show(); unrelatedWindow.activateWindow(); QApplication::setActiveWindow(&unrelatedWindow);
    assert(SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 24000));
    QTimer::singleShot(120, &loop, &QEventLoop::quit);
    loop.exec();
    assert(QApplication::activeWindow() == &unrelatedWindow); // Another Qt window owns the foreground.
    SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 0);
    window.activateWindow(); QApplication::setActiveWindow(&window); first->setFocus(); app.processEvents();
    QTimer::singleShot(120, &loop, &QEventLoop::quit);
    loop.exec(); // Foreground return reopens and neutral-rearms the same pad.

    edit->setFocusPolicy(Qt::StrongFocus); edit->setFocus(); app.processEvents();
    assert(hints->text().contains("X: edit"));
    assert(pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Edit));
    assert(QApplication::focusWidget() == edit);
    first->setFocus(); app.processEvents();
    ownsSession = false;
    QTimer::singleShot(120, &loop, &QEventLoop::quit);
    loop.exec();
    assert(SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 24000));
    QTimer::singleShot(160, &loop, &QEventLoop::quit);
    loop.exec();
    assert(QApplication::focusWidget() == first); // Session ownership suppresses otherwise valid pad input.
    ownsSession = true;
    SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 0);
    QTimer::singleShot(120, &loop, &QEventLoop::quit);
    loop.exec(); // Reopen on return and require neutral before rearming.
    SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 24000);
    QTimer::singleShot(160, &loop, &QEventLoop::quit);
    loop.exec();
    assert(QApplication::focusWidget() == last);
    SDL_SetJoystickVirtualAxis(virtualDevice, SDL_GAMEPAD_AXIS_LEFTY, 0);
    adapter.releaseForHandoff();
    SDL_CloseJoystick(virtualDevice);
    assert(SDL_DetachVirtualJoystick(virtualId));

    const SDL_JoystickID reconnectedId = SDL_AttachVirtualJoystick(&description);
    assert(reconnectedId > 0);
    SDL_Joystick* reconnected = SDL_OpenJoystick(reconnectedId);
    assert(reconnected);
    adapter.reopenAfterHandoff();
    first->setFocus(); app.processEvents();
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec(); // Neutral state is observed before reconnect navigation is armed.
    assert(SDL_SetJoystickVirtualAxis(reconnected, SDL_GAMEPAD_AXIS_LEFTY, 24000));
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();
    assert(QApplication::focusWidget() == last); // Reconnected device restores controller navigation.
    SDL_SetJoystickVirtualAxis(reconnected, SDL_GAMEPAD_AXIS_LEFTY, 0);
    adapter.releaseForHandoff();
    SDL_CloseJoystick(reconnected);
    assert(SDL_DetachVirtualJoystick(reconnectedId));
    return 0;
}
