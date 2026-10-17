# Portable bundle inspection slice

## Import your game

Extract the portable preview and run `pt-launcher.exe` on Windows or
`./pt-launcher` on Linux. The application automatically finds its bundled
runtime. Choose **Game files — choose .pkg**, drop a local package onto the
window, or pass its path as the launcher's sole command-line argument. Use
**Import extracted game folder** for an already decrypted dump, including its
`CUSA01127` folder. **Open existing portable bundle** switches to another
prepared bundle.

Preparation runs in the background with progress and cancellation. It uses the
self-contained `extractor/PT.PkgExtract` helper included with the platform
runtime; no .NET SDK or separate extraction tool is needed. The extractor accepts
P.T. fPKGs and reports unsupported encrypted retail packages or damaged/unrelated
sources. The source stays at its original location. Prepared archives are
staged and checked before installation, while saves, mods and unrelated files
are preserved. Interrupted preparation is recovered when the bundle is opened.
When preparation succeeds, choose **Play**.

## Unsigned paired preview bundles

The manual workflow builds the current launcher natively with shared Qt 6.12.0
and pairs it with the published upstream v1.0.5 portable runtime. The Windows
and Linux runtime ZIPs have fixed SHA-256 pins in
`tools/ci/fetch_launcher_preview_runtime.py`; replacement assets fail verification.
CI checks that the checkout's game code still matches the upstream release's
source tag before pairing it. Each launcher embeds the actual runtime executable's
hash through `PT_SUPPORTED_LINUX_SHA256` or `PT_SUPPORTED_WINDOWS_SHA256`.

The source snapshot's Windows game CMake references two unpublished files,
`tools/dx12/spirv_to_hlsl.cpp` and `tests/unicode_paths_test.rc`. These preview
jobs therefore reuse the released runtime on both platforms. `runtime-origin.json`
records the release, release source revision, original archive folder/build label,
download URL, archive checksum, and executable checksum. This records the
upstream release's declared provenance without asserting that its archive's
build label is a published source commit.

Run **Launcher preview** manually on the `integration/launcher` branch, choosing
an unused preview version. For example:

```sh
gh workflow run launcher-preview.yml --repo spitefulFr0g/pt-pc \
  --ref integration/launcher -f version=0.1.0-preview.2 -f create_draft=true
```

Both platform jobs must build and smoke-check successfully before CI creates an
unpublished draft prerelease. Download and test its Windows ZIP and Linux tar.gz,
then publish the draft when ready. No signing secret is needed. Build evidence
and temporary Actions artifacts are retained for 14 days; draft release assets
remain attached to the release. The launcher preview tag does not trigger the
separate macOS game packaging workflow.

For a local build with the same dependencies and Qt prefix as CI:

```sh
export QT_ROOT=/path/to/Qt/6.12.0/gcc_64
export CMAKE_PREFIX_PATH="$QT_ROOT"
python3 tools/ci/build_launcher_preview.py --platform linux \
  --version 0.1.0-preview.2
python3 tools/ci/smoke_launcher_preview.py \
  --artifacts dist/launcher-preview --platform linux --evidence build/preview-smoke
```

The lower-level package command accepts `--runtime-stage`, `--launcher-stage`,
`--platform`, `--version`, `--revision`, and `--out`. `--launcher-build` can replace
`--launcher-stage` and installs into a temporary stage using that build's Qt prefix.
Windows uses `--platform windows`, staged `pt.exe`, a ZIP archive, and Qt's
`windeployqt` with compiler runtime deployment. Linux requires `patchelf`,
`dpkg-query`, `ldd`, and Qt's xcb and Wayland plugins. The Linux smoke check
requires `xvfb-run` and starts the extracted launcher through xcb in a fresh
path containing spaces. It checks the archive SHA-256 sidecar, every inventory
hash, and screenshots from the actual QApplication event loop. The inventory
records the runtime source hash, the supported hash compiled into the launcher,
the Qt version, and file sizes/hashes. The package command fails if the launcher
does not embed the exact runtime hash.

The archive has `windows/pt.exe` or `linux/pt`, the launcher at its root
(`pt-launcher.exe` on Windows or a `pt-launcher` entry script on Linux), Qt
libraries/plugins, assets, licenses, and `preview-inventory.json`. The Linux
script changes no global Qt or library environment; the game launched through
PlayFlat inherits the normal system environment. The runtime staging command
removes the upstream updater, stale runtime file list, and loop preview images
captured from the original game. No game archives are included. These bundles are unsigned manual tests;
PlayFlat passes `--no-update-check`, and upstream in-game updating is
unsupported for this mixed bundle. Direct runtime launches should also pass
`--no-update-check`. The archive includes `README-preview.md`, the project
license, Qt LGPL/GPL exception texts, and runtime and platform dependency
notices.

Import a package or extracted folder through the launcher as described above.
Existing manually prepared bundles with the archives under `CUSA01127` at the
bundle root can still be selected with **Open existing portable bundle**. The
preview covers preparation, bundle inspection and flat launch with the matching runtime.
Integrated settings, VR, controller features, secure updating,
native Wayland, SteamOS/Hyprland/Gaming Mode, and gameplay remain unverified or
pending. A passing CI smoke run is not a gameplay result.

Build the standalone C++20 Qt Widgets launcher from `launcher/`; this target does not configure or link the game engine. Release builds require shared Qt 6.12.0. Copy `launcher/assets/` beside the executable when staging it. The included artwork and font notices must accompany distribution.

Run `launcher/run-acceptance.sh [evidence-directory]` from a noninteractive shell with CMake, a C++20 compiler, zlib development files, and Qt 6.12.0 available through `CMAKE_PREFIX_PATH`. It configures, builds, runs the Qt event-loop fixture test, and saves logs; any failure returns nonzero. Fixtures are disposable and contain synthetic headers and player files only. Set `QT_QPA_PLATFORM=offscreen` for a headless run.

## Earlier inspection validation

The records below describe the inspection and session slices before preview
packaging. Preview builds supply a real paired runtime hash; default source
builds still leave it unset. The preview workflow above adds native packaged
startup checks to these earlier results.

On 2026-10-17, the noninteractive acceptance command passed in an Ubuntu 24.04 container from image `ubuntu@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac`. The build used GCC 13.3.0, CMake 3.28.3, zlib 1.3, and the Qt 6.12.0 Linux x86_64 SDK installed with aqtinstall 3.3.0. The Qt event-loop fixture test passed. Offscreen desktop and narrow captures were produced from this Qt 6.12.0 build. The detailed configure, build, test, screenshot, and hash evidence is in [`evidence/issue3-qt612/`](evidence/issue3-qt612/).

This pass covers compilation and the Qt event loop with an offscreen platform plugin. It does not establish native Wayland/xcb plugin loading or a packaged clean-bundle run. An earlier Ubuntu 24.04 Docker smoke run used distro Qt 6.4.2 and is not relied on for the Qt 6.12.0 result.

## Earlier inspection checks

- Windows 11 MSVC 2022 and Qt 6.12.0 build, execution, and package. The host is Windows 11 build 26200 with Visual Studio Community 17.14.27, but `cl.exe` and the MSVC x64 toolset are absent. The current account is not an administrator; [Microsoft's installer requires elevation to add Visual Studio components](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio), so this check remains pending. No Windows result is claimed.
- Native Linux Wayland/xcb plugin loading and packaged UI verification.
- A supported release inventory with real runtime hashes. Until a downstream release provides approved paired metadata and hashes, detected runtimes remain unverified and Play/Settings stay disabled. Synthetic fixtures verify the hash-matching behavior only; they do not stand in for a real release.

## Inspection behavior

The public `InspectBundle` operation reads `CUSA01127` archives with unchanged upstream header checks and reports separate Linux/Windows runtimes, settings, and logs. Archive recognition is independent of runtime verification. Default source builds have no configured runtime hash and leave Play disabled. Preview builds configure the paired executable's hash and can enable Play for that runtime. Settings remains unfinished. This fixture does not prove complete archive integrity.

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
