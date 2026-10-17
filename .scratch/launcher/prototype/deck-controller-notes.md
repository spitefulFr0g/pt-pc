# Steam Deck controller-flow prototype

Question: can the selected G launcher support controller-only preparation, launch and return with safe focus and process ownership?

Use the prototype skill's **logic/interaction** branch: G's visual direction is already accepted. This adds one controller model, not another aesthetic competition. The source is `deck-controller-source.html`; run `python3 .scratch/launcher/prototype/build-deck-prototype.py` to produce `deck-controller-prototype.html`, a self-contained offline file with the licensed photo and font embedded. Open that generated file directly or serve the existing prototype directory. Original notices remain under `assets/`.

The first script is a pure in-memory interaction model. The surrounding page supplies DOM rendering, browser keyboard/gamepad polling and six guided scenarios. Sample paths represent a fake filesystem. File operations, game sessions, focus ownership and the session helper are simulated; nothing connects to the real bundle, native SDL, Steam or Gamescope. No production game/engine files changed.

## Proposed interaction

- D-pad/left stick move through enabled controls; A activates; B backs out; X opens the keyboard on a path field. Keyboard and pointer remain available. Keep a clear focus ring and small contextual hints. Start on Play, scroll focused controls into view, and restore the originating control after closing a page/modal.
- Keep G's split composition, local forest photo, green palette, gothic title, stable Play label and three tools. Keep actionable readiness/failure messages; omit routine ready/running prose.
- Use an in-app controller file picker, including parent-folder navigation and folder/package selection, plus a built-in keyboard. Done applies a text edit; B cancels its draft and restores field focus. Clear/delete/case actions avoid requiring Steam's overlay keyboard or an external keyboard. Native filesystem checks and Unicode support remain implementation requirements, not results from the sample tree.
- Back during preparation asks before canceling, with Keep preparing selected. Progress must not steal focus; completing work dismisses stale cancellation prompts. Back after editing settings asks before discarding, with Keep editing selected.
- Put the game in the foreground after Play and leave the launcher in the background without consuming gamepad input. Automatically return on game exit if the launcher is still open. If the launcher is brought forward during play, offer Return to game and block duplicate launches/settings writes/updates. Do not steal focus periodically or claim a Steam window-switch UI has been tested.
- Require released/neutral controls before controller navigation resumes after return, reconnect or focus regain. Clear repeats/held state when losing focus or handing off. Keyboard/pointer can remain usable when a controller disconnects. Steam Stop/session shutdown retains the platform's normal meaning; it is different from closing only the GUI window.

## Native input and session recommendation

Use the existing **SDL3 release-3.4.16** dependency directly in the standalone Qt launcher, without linking `pt_engine`. Initialize gamepad/joystick/events only; do not initialize the game's renderer. Pump bounded events on Qt's GUI thread with a roughly 16 ms QTimer; extraction/network work stays off that thread. Normalize controller events into direction/activate/Back/text/page actions and route them through the active page/modal. Native keyboard/pointer use the same commands through ordinary Qt events. Define repeat/dead-zone behavior in the adapter and automate it.

SDL's normal background-input policy is insufficient when the app has only Qt windows: the pinned source checks `SDL_HasWindows()` before suppressing unfocused input. Explicitly gate controller dispatch on actual Qt foreground/visibility and session state, clear repeats on deactivation, and rearm after neutral input. Release launcher gamepad handles/subsystems for game handoff; reopen only on foreground return. Do not propagate launcher-only input hints into the game process.

For the non-Steam shortcut, use Steam's standard Gamepad layout, consumed once through SDL's virtual gamepad path. Disable the launcher's raw Deck HIDAPI driver locally before initialization in this mode to avoid competing Deck ownership. Do not also enable a second Steamworks-to-keyboard emulation path. Custom keyboard-emulating Steam profiles are not the default integration contract; ordinary physical keyboard input remains supported. Ship no fabricated Steam App ID and do not depend on Steamworks initialization for the built-in keyboard.

Steam's entry point should be a small **launcher-owned Linux session supervisor**, not the disposable GUI process. It starts/monitors GUI and game, owns session identity/IPC, and stays alive while either is active. Use process handles/start identity and bundle-scoped locking, not PID alone. Reopen/reconnect to an existing session without spawning a competing game. Closing the GUI leaves the game and helper running; after both close, the helper exits. A GUI crash must not automatically kill its live game. Intentional Steam Stop/session teardown can terminate the complete session normally.

This extends the earlier detached-launch architecture: QProcess detached lifetime alone cannot override external Gamescope supervision. Keep the native runtime/archives/saves/mods and direct/script paths unchanged. The Steam entry point and any one-time non-Steam shortcut setup must be documented; do not silently mutate the user's Steam library. The helper must also track in-flight preparation/update work and coordinate its shutdown/replacement through the existing transaction policy.

Use an explicit Gaming Mode entry point/argument. Keep the session helper GUI-free and pass a clean original game environment separately from the GUI's private Qt/plugin environment. Gaming Mode self-update must preserve the primary session process while replacing helper code: use a verified exec-style takeover at the same PID after game/jobs stop, with journaled recovery and startup acknowledgement. An exit-and-respawn of the root would trigger external session cleanup. See [execve semantics](https://man7.org/linux/man-pages/man2/execve.2.html); this is an implementation contract to test, not a behavior exercised by the HTML simulation.

## Evidence and automated acceptance

Automated browser review exercised all six walkthroughs and 25 assertions covering source selection/completion, cancellation, text draft restore/apply, settings discard, running-session write restrictions, held-A relaunch prevention, neutral rearming, disconnect/keyboard fallback, background input, helper lifetime, stale cancel prompts and VR-only blocking. Photo/font loaded; home/footer and keyboard layouts were reviewed at 728px and 360px without JavaScript errors. The browser viewport was 1280 × 800; the product mockup fits inside it, with study controls/state outside the product. Review checks are commands run by the agent, not a new prototype test suite.

The implementation's required checks remain entirely automated and agent-run:

- Qt UI/event-loop checks at Deck resolution and scaled sizes, covering the same public flows, focus scrolling and disabled controls.
- SDL virtual joystick/button/axis injection through the actual input adapter, including repeat/dead zones, neutral release, disconnect/reconnect, competing input, and foreground/background handling.
- Session-helper subprocess/IPC tests using controlled game-window fixtures: start/failure, GUI close/crash, game exit, duplicate prevention, reconnect, input release/reacquire, focus return, busy transactions and Steam-style session shutdown.
- Nested Gamescope/compositor integration where agent-runnable, with captured process/window results. Label Steam-client, nested compositor and simulated boundaries separately; do not claim full Steam-client behavior from a fake process fixture. A missing mandatory automation seam is implementation work or a concrete tooling blocker, never a request for manual user testing.
- Packaged artifact checks and the broader accepted release criteria continue to apply. No physical Deck, controller or headset test is required.

No native Qt/SDL build, actual Steam Input/Gamescope session, real game launch or physical-device check was performed by this prototype. User accepted the complete controller flow and session-helper behavior. Verdict: use this interaction model as the specification for the standalone Qt/SDL launcher and Linux session helper, retaining the selected G design and fully automated acceptance.

## Primary sources

- Repo SDL pin: `cmake/Dependencies.cmake:8–12`; existing virtual-pad APIs: `src/engine/platform/virtual_pad.cpp:116–141,190`.
- [SDL initialization](https://wiki.libsdl.org/SDL3/SDL_Init), [event pumping](https://wiki.libsdl.org/SDL3/SDL_PumpEvents), [Qt timer](https://doc.qt.io/qt-6/qtimer.html), [Qt focus APIs](https://doc.qt.io/qt-6/qwidget.html).
- [Pinned SDL background-input logic](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.16/src/joystick/SDL_joystick.c), [Deck HIDAPI hint](https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK), [pinned Deck driver](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.16/src/joystick/hidapi/SDL_hidapi_steamdeck.c).
- [Qt detached process semantics](https://doc.qt.io/qt-6.12/qprocess.html), [Gamescope child-process reaper](https://github.com/ValveSoftware/gamescope/blob/master/src/Apps/gamescopereaper.cpp).
- [Steam API initialization](https://partner.steamgames.com/doc/sdk/api), [SDL virtual joystick](https://wiki.libsdl.org/SDL3/SDL_AttachVirtualJoystick), [virtual button update semantics](https://wiki.libsdl.org/SDL3/SDL_SetJoystickVirtualButton).

The subscription-authenticated fact worker read repository and official API/source evidence only. Its recommendations support this proposal; they are not runtime test evidence.
