#include "../controller.h"
#include "../settings_dialog.h"
#include "settings_fixture.h"
#include <QTemporaryDir>
#include <QMessageBox>
#include <fstream>
#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QEventLoop>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QAbstractItemView>
#include <QSpinBox>
#include <QScrollBar>
#include <SDL3/SDL.h>
#include <cassert>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
#ifdef __linux__
    assert(QApplication::platformName() == "xcb"); // CTest owns the real display setup.
#endif
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

    // A real enabled but empty combo is a valid state while choices load.
    QComboBox empty(&window);
    layout->addWidget(&empty); empty.show(); empty.setFocus(); app.processEvents();
    pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Right);
    assert(empty.currentIndex() == -1);
    empty.hide(); first->setFocus(); app.processEvents();

    QComboBox choices(&window); choices.addItems({"Windowed", "Borderless", "Exclusive"});
    layout->addWidget(&choices); choices.show(); choices.setFocus(); app.processEvents();
    assert(pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Activate));
    app.processEvents();
    assert(QApplication::activePopupWidget());
    pt::launcher::DispatchControllerAction(QApplication::activePopupWidget(), pt::launcher::ControllerAction::Down);
    pt::launcher::DispatchControllerAction(QApplication::activePopupWidget(), pt::launcher::ControllerAction::Activate);
    app.processEvents();
    assert(choices.currentIndex() == 1 && !QApplication::activePopupWidget());
    choices.hide(); first->setFocus(); app.processEvents();

    QSpinBox resolution(&window); resolution.setRange(320, 16384); resolution.setValue(1280);
    layout->addWidget(&resolution); resolution.show(); resolution.setFocus(); app.processEvents();
    assert(pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Right));
    assert(resolution.value() == 1281);
    assert(pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Edit));
    assert(resolution.findChild<QLineEdit*>()->selectedText() == "1281");
    resolution.hide(); first->setFocus(); app.processEvents();

    QScrollArea area(&window); area.setWidgetResizable(true); area.setMaximumHeight(100);
    auto* scrollContent = new QWidget;
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    QPushButton* scrollFirst = nullptr; QPushButton* scrollLast = nullptr;
    for (int i = 0; i < 25; ++i) {
        auto* button = new QPushButton(QString::number(i), scrollContent);
        scrollLayout->addWidget(button);
        if (!scrollFirst) scrollFirst = button;
        scrollLast = button;
    }
    area.setWidget(scrollContent); layout->addWidget(&area); area.show(); app.processEvents();
    scrollFirst->setFocus();
    for (int i = 0; i < 24; ++i)
        pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Down);
    app.processEvents();
    assert(QApplication::focusWidget() == scrollLast);
    assert(area.verticalScrollBar()->value() > 0);
    assert(area.viewport()->rect().contains(scrollLast->mapTo(area.viewport(), scrollLast->rect().center())));
    area.hide(); first->setFocus(); app.processEvents();

    bool ownsSession = true;
    const QByteArray oldDeckHint = qgetenv("SDL_JOYSTICK_HIDAPI_STEAMDECK");
    const bool hadDeckHint = qEnvironmentVariableIsSet("SDL_JOYSTICK_HIDAPI_STEAMDECK");
    qputenv("SDL_JOYSTICK_HIDAPI_STEAMDECK", "1");
    pt::launcher::ControllerAdapter adapter(&window, [&] { return ownsSession; });
    assert(!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, true));
    assert(qgetenv("SDL_JOYSTICK_HIDAPI_STEAMDECK") == "1"); // Local hint must not rewrite the game environment.
    if (hadDeckHint) qputenv("SDL_JOYSTICK_HIDAPI_STEAMDECK", oldDeckHint);
    else qunsetenv("SDL_JOYSTICK_HIDAPI_STEAMDECK");
    assert(adapter.available());
    SDL_VirtualJoystickDesc description{};
    description.version = sizeof(description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.naxes = SDL_GAMEPAD_AXIS_COUNT;
    description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    // Enable all buttons so SDL's auto-mapping preserves raw virtual indices.
    description.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1u;
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
    for (int i = 0; i < 4096; ++i) { SDL_Event event{}; event.type = SDL_EVENT_USER; assert(SDL_PushEvent(&event)); }
    QTimer::singleShot(25, &loop, &QEventLoop::quit); loop.exec();
    assert(SDL_HasEvent(SDL_EVENT_USER)); // A bounded pump leaves work for later Qt turns.
    SDL_FlushEvent(SDL_EVENT_USER);
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

    for (int i = 0; i < 10; ++i) adapter.reopenAfterHandoff();
    adapter.releaseForHandoff();
    assert(SDL_GetGamepadFromID(virtualId) == nullptr); // Every acquired reference is released.
    adapter.reopenAfterHandoff();
    QTimer::singleShot(60, &loop, &QEventLoop::quit); loop.exec();

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

    // A real synchronous modal enters a nested Qt event loop from the SDL click.
    int modalActivations = 0;
    bool modalOpened = false;
    auto nestedConnection = QObject::connect(last, &QPushButton::clicked, [&] {
        modalOpened = true;
        QDialog dialog(&window);
        auto* modalLayout = new QVBoxLayout(&dialog);
        auto* dangerous = new QPushButton("Discard", &dialog);
        modalLayout->addWidget(dangerous);
        QObject::connect(dangerous, &QPushButton::clicked, [&] { ++modalActivations; });
        QTimer::singleShot(80, &dialog, [&] {
            assert(modalActivations == 0); // Held A must not click the modal default.
            SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, false);
        });
        QTimer::singleShot(140, &dialog, [&] {
            SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_EAST, true);
        });
        bool timedOut = false;
        QTimer::singleShot(250, &dialog, [&] { timedOut = true; dialog.reject(); });
        dialog.exec();
        assert(!timedOut); // B must work while exec() is nested inside the A action.
        assert(modalActivations == 0);
    });
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, false);
    QTimer::singleShot(40, &loop, &QEventLoop::quit); loop.exec();
    last->setFocus();
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, true);
    QTimer::singleShot(360, &loop, &QEventLoop::quit); loop.exec();
    assert(modalOpened && modalActivations == 0);
    QObject::disconnect(nestedConnection);
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, false);
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_EAST, false);
    window.activateWindow(); QApplication::setActiveWindow(&window); first->setFocus();
    QTimer::singleShot(60, &loop, &QEventLoop::quit); loop.exec();

    // Invalidate ownership inside the adapter's polling-cache interval.
    adapter.releaseForHandoff(); adapter.reopenAfterHandoff();
    QTimer::singleShot(20, &loop, &QEventLoop::quit); loop.exec();
    ownsSession = false;
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, true);
    QTimer::singleShot(35, &loop, &QEventLoop::quit); loop.exec();
    assert(activations == 2); // No stale ownership may authorize a click.
    SDL_SetJoystickVirtualButton(virtualDevice, SDL_GAMEPAD_BUTTON_SOUTH, false);
    ownsSession = true;
    adapter.releaseForHandoff(); adapter.reopenAfterHandoff();
    QTimer::singleShot(60, &loop, &QEventLoop::quit); loop.exec();

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
    edit->setText("draft resolution");
    assert(pt::launcher::DispatchControllerAction(&window, pt::launcher::ControllerAction::Edit));
    assert(edit->selectedText() == "draft resolution");
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
    QTimer::singleShot(60, &loop, &QEventLoop::quit); loop.exec();
    QTemporaryDir settingsTemp; assert(settingsTemp.isValid());
    const auto settingsRoot = std::filesystem::path(settingsTemp.path().toStdU32String());
    const auto settingsRuntime = SettingsRuntimes(settingsRoot);
    std::filesystem::create_directories(settingsRoot / "linux/data");
    std::ofstream(settingsRoot / "linux/data/pt.ini") << "[display]\nwidth=1280\nheight=720\nfullscreen=0\n";
    pt::launcher::SettingsDialog settingsDialog(settingsRoot, pt::launcher::Platform::Linux, settingsRuntime, &window);
    assert(settingsDialog.loaded()); settingsDialog.show(); settingsDialog.activateWindow();
    QApplication::setActiveWindow(&settingsDialog);
    auto* settingsWidth = settingsDialog.findChild<QSpinBox*>("displayWidth");
    settingsWidth->setFocus();
    auto wait = [&](int ms) { QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); };
    auto pulse = [&](SDL_GamepadButton button) {
        SDL_SetJoystickVirtualButton(reconnected, button, true); wait(50);
        SDL_SetJoystickVirtualButton(reconnected, button, false); wait(50);
    };
    wait(80); pulse(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    assert(settingsWidth->value() == 1281);
    pulse(SDL_GAMEPAD_BUTTON_WEST);
    assert(settingsWidth->findChild<QLineEdit*>()->selectedText() == "1281");
    // Real Settings -> unsaved-changes exec modal -> safe default through SDL A.
    QTimer::singleShot(60, &settingsDialog, [&] {
        auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        assert(prompt && prompt->defaultButton()->text() == "Keep editing");
        assert(QApplication::focusWidget() == prompt->defaultButton());
        SDL_SetJoystickVirtualButton(reconnected, SDL_GAMEPAD_BUTTON_EAST, false);
    });
    QTimer::singleShot(120, &settingsDialog, [&] { SDL_SetJoystickVirtualButton(reconnected, SDL_GAMEPAD_BUTTON_SOUTH, true); });
    QTimer::singleShot(180, &settingsDialog, [&] { SDL_SetJoystickVirtualButton(reconnected, SDL_GAMEPAD_BUTTON_SOUTH, false); });
    SDL_SetJoystickVirtualButton(reconnected, SDL_GAMEPAD_BUTTON_EAST, true); wait(260);
    assert(settingsDialog.isVisible() && settingsWidth->value() == 1281);
    assert(QApplication::activeModalWidget() == &settingsDialog);
    assert(QApplication::focusWidget() == settingsWidth);
    auto* settingsApply = settingsDialog.findChild<QPushButton*>("settingsApply");
    settingsApply->setFocus(); pulse(SDL_GAMEPAD_BUTTON_SOUTH);
    assert(pt::launcher::ReadDisplaySettings(settingsRoot, pt::launcher::Platform::Linux, settingsRuntime).value.width == 1281);
    pulse(SDL_GAMEPAD_BUTTON_EAST);
    assert(!settingsDialog.isVisible());
    adapter.releaseForHandoff();
    SDL_CloseJoystick(reconnected);
    assert(SDL_DetachVirtualJoystick(reconnectedId));
    return 0;
}
