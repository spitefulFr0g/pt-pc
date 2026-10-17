# Portable bundle inspection slice

Build the standalone C++20 Qt Widgets launcher from `launcher/`; this target does not configure or link the game engine. Release builds require shared Qt 6.12.0. Copy `launcher/assets/` beside the executable when staging it. The included artwork and font notices must accompany distribution.

Run `launcher/run-acceptance.sh [evidence-directory]` from a noninteractive shell with CMake, a C++20 compiler, zlib development files, and Qt 6.12.0 available through `CMAKE_PREFIX_PATH`. It configures, builds, runs the Qt event-loop fixture test, and saves logs; any failure returns nonzero. Fixtures are disposable and contain synthetic headers and player files only. Set `QT_QPA_PLATFORM=offscreen` for a headless run.

## Validation record

On 2026-10-17, the noninteractive acceptance command passed in an Ubuntu 24.04 container from image `ubuntu@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac`. The build used GCC 13.3.0, CMake 3.28.3, zlib 1.3, and the Qt 6.12.0 Linux x86_64 SDK installed with aqtinstall 3.3.0. The Qt event-loop fixture test passed. Offscreen desktop and narrow captures were produced from this Qt 6.12.0 build. The detailed configure, build, test, screenshot, and hash evidence is in [`evidence/issue3-qt612/`](evidence/issue3-qt612/).

This pass covers compilation and the Qt event loop with an offscreen platform plugin. It does not establish native Wayland/xcb plugin loading or a packaged clean-bundle run. An earlier Ubuntu 24.04 Docker smoke run used distro Qt 6.4.2 and is not relied on for the Qt 6.12.0 result.

## Pending checks

- Windows 11 MSVC 2022 and Qt 6.12.0 build, execution, and package. The host is Windows 11 build 26200 with Visual Studio Community 17.14.27, but `cl.exe` and the MSVC x64 toolset are absent. The current account is not an administrator; [Microsoft's installer requires elevation to add Visual Studio components](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio), so this check remains pending. No Windows result is claimed.
- Native Linux Wayland/xcb plugin loading and packaged UI verification.
- A supported release inventory with real runtime hashes. Until a downstream release provides approved paired metadata and hashes, detected runtimes remain unverified and Play/Settings stay disabled. Synthetic fixtures verify the hash-matching behavior only; they do not stand in for a real release.

## Inspection behavior

The public `InspectBundle` operation reads `CUSA01127` archives with unchanged upstream header checks and reports separate Linux/Windows runtimes, settings, and logs. Archive recognition is independent of runtime verification. The shipped UI has no approved release inventory yet, so it never calls a runtime verified or enables Play/Settings. This fixture does not prove complete archive integrity. The disabled actions are deliberate while their implementation tickets remain outstanding.

## Flat session operations (issue #4)

`InspectSession`, `PlayFlat`, `WithStoppedBundle`, and `ReturnToGame` are the public
session operations. Play uses the current platform runtime only when its SHA-256
matches an approved release hash supplied at build time through
`PT_SUPPORTED_LINUX_SHA256` or `PT_SUPPORTED_WINDOWS_SHA256`. The repository does
not yet contain a real downstream release inventory, so default builds leave
Play disabled for unverified runtimes. A test-only launcher target supplies a
synthetic fixture hash and clicks the actual Play button. No game or player data
format is modified.

The launcher remembers the selected bundle through Qt application preferences.
A detached game process survives closing the GUI. On reopening, process activity
is inspected anew. A bundle-scoped lock serializes launcher operations while
process scanning detects compatible direct and script launches, which do not
participate in that lock. `WithStoppedBundle` is the public mutation guard for
later settings, preparation, and update slices; it checks activity while holding
the lock. On Linux, process identity includes `/proc` start ticks; on Windows it
includes the kernel process creation time. Uncertain identity blocks mutations.

Run `launcher/run-issue4-qt612-container.sh` from a noninteractive shell to
provision Qt 6.12.0 if needed, build in Ubuntu 24.04, run both Qt fixture tests,
and save logs to `launcher/evidence/issue4-qt612/`. The command returns nonzero
on setup, build, or test failure. The test creates disposable synthetic archives,
a shell process fixture, and player files; it needs no game files or account.
It uses Qt's offscreen plugin for session actions and Xvfb/Openbox/xcb for a
controlled game window and Return to game. The original setup recipe and prior evidence remain
under `launcher/evidence/issue3-qt612/`.

Native compositor window focus and a clean packaged bundle remain pending.
On X11, Return to game uses `xdotool` when available; on Wayland, the launcher
points to the compositor's window switcher. Windows 11 MSVC/Qt build and runtime
checks remain pending because the MSVC x64 toolset is unavailable on this host.
