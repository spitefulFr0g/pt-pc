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
