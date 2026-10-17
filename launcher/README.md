# Portable bundle inspection slice

Build the standalone C++20 Qt Widgets launcher from `launcher/`; this target does not configure or link the game engine. Release builds require shared Qt 6.12.0. Copy `launcher/assets/` beside the executable when staging it. The included artwork and font notices must accompany distribution.

Run `launcher/run-acceptance.sh [evidence-directory]` from a noninteractive shell. It configures, builds, runs the Qt event-loop fixture test, and saves logs; any failure returns nonzero. Fixtures are disposable and contain synthetic headers and player files only. `QT_QPA_PLATFORM=offscreen` is the default for container execution.

A compatibility smoke run used Ubuntu 24.04 in Docker 29.8.1 with GCC 13.3.0, Qt 6.4.2 and zlib 1.3. Its offscreen Qt event-loop test passed. This is not a Qt 6.12.0 or native compositor pass. The Windows host reports build 26200, Visual Studio 2022 Community and CMake, but no `C:\Qt` kit.

The public `InspectBundle` operation reads `CUSA01127` archives with unchanged upstream header checks and reports separate Linux/Windows runtimes, settings and logs. Runtime verification requires an explicit supported release hash list. The shipped UI has no approved release inventory yet, so it never calls a runtime verified or enables Play/Settings. This fixture proves exact hash matching; it does not prove a real release or complete archive integrity. The UI's disabled buttons are deliberate pending the later launch/settings/update tickets.

## Pending mandatory evidence

- Native Qt 6.12.0 Linux build and packaged xcb/Wayland UI: no Qt 6.12.0 SDK is installed here. The container smoke build uses distro Qt only and does not satisfy this gate.
- Windows 11 MSVC 2022 and Qt 6.12.0 build, execution and package: PowerShell is accessible, but the checked host has no visible Qt kit; no Windows build or UI run is claimed.
- Real supported runtime metadata/hashes, native visual screenshot, and package dependency inventory are not yet available from this slice.
