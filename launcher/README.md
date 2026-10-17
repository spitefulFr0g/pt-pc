# Portable bundle inspection slice

## Import your game

Extract the portable preview and run `pt-launcher.exe` on Windows or
`./pt-launcher` on Linux. The application automatically finds its bundled
runtime. Choose **Choose .pkg and prepare**, drop a local package onto the
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
Ticket #10 adds independent flat/VR readiness and a separate VR attempt. It does not establish headset presentation. Controller features, secure updating,
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

## Independent flat and VR readiness (issue #10)

The home screen uses the public `CheckReadiness` operation for each launch mode.
The probe runs away from the Qt thread, bounds the OpenXR manifest to 64 KiB and
the Linux loader inventory to two seconds, and presents unverified runtime
hashes as unknown with mode-specific **Try Play anyway** and **Try VR anyway**
actions. It only reads the active OpenXR registration and never changes the
system selection or creates an XR
instance. A discovered runtime is not evidence of headset connection or
presentation. Flat launch keeps `--no-vr` even when the saved settings enable
VR; the separate VR operation passes `--vr`, the same explicit bundle paths,
and `--no-update-check` without writing the saved preference.

`PT_SUPPORTED_LINUX_VR` and `PT_SUPPORTED_WINDOWS_VR` declare OpenXR support for
the exact runtime hashes paired into a launcher build. The pinned Linux preview
runtime hash `691e4667ce082b10d82cd0814ee577a147895caaecd4961d54042c4d6165e114`
contains the upstream no-OpenXR build branch, so it is marked VR-blocked while
flat remains available. Windows defaults to OpenXR-capable; readiness still
requires its bundled loader and otherwise reports a blocker. These declarations
must be revisited with each supported runtime revision.

Run `launcher/run-issue10-qt612-container.sh [evidence-directory]` for the
noninteractive Qt 6.12.0/GCC 13.3.0 harness. By default it writes under
`/tmp/pt-launcher-issue10-evidence`. It configures and builds the launcher,
drives the actual Qt unknown/Try Play anyway, Try VR anyway and Retry VR
readiness controls, and runs controlled manifest, loader timeout, missing-loader, flat-independence,
saved-preference, process-argument, session and window checks. Fixture runtime
and loader files are disposable. This is Linux controlled-boundary evidence;
Windows runtime/loader behavior, native headset presentation, and a physical
device remain unverified. Ticket #11 launch outcome/fallback and ticket #12
Linux loader packaging are not included.

## Dump preparation acceptance (ticket #5)

Run `launcher/run-issue5-acceptance.sh [evidence-directory]` from a
noninteractive shell. The default evidence directory is
`/tmp/pt-launcher-issue5-evidence`. The script provisions shared Qt 6.12.0 with
`aqtinstall` when absent, then builds with GCC 13 in the pinned launcher preview
build container and runs all launcher CTest checks. It returns nonzero on setup,
build, or test failure. Override `PT_QT612_ROOT` or
`PT_LAUNCHER_BUILD_IMAGE` to use an existing local copy. No game data or account
is used.

`launcher_preparation` exercises the public `PrepareGame` and
`RecoverPreparation` operations against disposable synthetic archive headers.
It covers successful dump copy/adoption, repeat preparation without reprocessing,
source and player/unowned file preservation, update-check default adoption,
source mutation during copy, cancellation, invalid source, Unicode/spaced paths,
and initialization-journal recovery. Those fixtures are recognition fixtures;
they are not real or playable game data. The GUI routes dump selection, progress,
cancel confirmation and recovery through these same public operations. The
suite exercises the Qt event-loop/session/window tests but does not automate
a native human input session for the preparation prompt.

Evidence must distinguish the exercised Qt offscreen/X11 controlled fixtures
from Windows, native Wayland/SteamOS, and clean packaged runtime behavior. Those
environments are outside this ticket harness and remain pending unless separate
platform evidence is recorded.

### Ticket #5 evidence (2026-10-17)

`launcher/run-issue5-acceptance.sh /tmp/pt-issue5-evidence` passed on Ubuntu
24.04 in `pt-launcher-preview-build-env-complete:latest`
(`sha256:763f56146b3927b7e85b7b7197e0539fbddbb0856bb430020c54def74a5fa549`),
with GCC 13.3.0, CMake 3.28.3, and shared Qt 6.12.0. All 5 CTest cases passed:
`launcher_inspection`, `launcher_session`, `launcher_window`,
`launcher_preparation`, and `launcher_preparation_ui`. Full configure/build/test
logs are saved in `/tmp/pt-issue5-evidence/`.

The public preparation checks include changed-source rejection during copy,
mid-copy cancellation, one-time update-policy adoption, source and bundle
preservation, same-operation re-adoption after moving to a Unicode path,
interrupted initialization cleanup, rollback of a partially committed fixture,
and a real Linux permission-denied lock attempt with a repairable error. The Qt
UI check runs under Xvfb/xcb and verifies the safe focused default, unchanged
focus during progress, and dismissal of the obsolete prompt after completion.
All game archive contents are synthetic recognition fixtures, never playable
data.

The escalation harness `launcher_preparation_process` now runs actual public
preparation in disposable children and sends SIGKILL during copying, immediately
after archive placement, after the settings backup rename, and after settings
placement. A Linux-only LD_PRELOAD test library stops at successful rename
boundaries; production has no fault-injection environment switches. Each case
attempts concurrent Play/preparation/guarded writes while preparation holds the
bundle lock, then uses public recovery and retry, asserting original source,
previous archives/metadata/settings, player data, and unowned bytes survive.

A real non-dumpable controlled process denies `/proc/PID/exe` inspection while
leaving cmdline readable. This exposed a fail-open Linux activity check; the
regression now verifies Uncertain and blocked Play/preparation/recovery/writes.
A syscall fixture also reproduces ESRCH during procfs cmdline reading, found
during a full-suite run, and verifies inspection safely handles process exit.
The space check runs separately with `TMPDIR` on a Docker-owned 64 MiB tmpfs,
asserts actual `std::filesystem::space` is below 3 GiB, and verifies the public
preflight rejects preparation without changing source/player data or staging.
No large allocation or host mount modification is needed.

Committed escalation logs: `launcher/evidence/issue5-r2/`.
External escalation evidence: `/tmp/pt-issue5-r2-red/ctest.log` records the uninspectable
process regression failing before the source fix;
`/tmp/pt-issue5-r2-proc-race-red/ctest.log` records the procfs race regression; `/tmp/pt-issue5-r2-final/`
records all six CTest cases plus the separate real-space preflight. The exact
noninteractive command is
`launcher/run-issue5-acceptance.sh /tmp/pt-issue5-r2-final`.
These source-level Linux controlled-process results do not establish packaged
GUI/helper termination, Windows, native Wayland or SteamOS gates (#24/#26/#27).

Red/green observations for the two defects found by these checks are recorded
in `/tmp/pt-issue5-evidence/red-green.md` alongside the final run logs.

## fPKG preparation acceptance (ticket #6)

Run `launcher/run-issue6-acceptance.sh [evidence-directory]`. The default is
`/tmp/pt-launcher-issue6-evidence`. The script provisions a build-only .NET
10.0.402 SDK under `/tmp` if needed, checks out LibOrbisPkg at pinned revision
`643477263b2644e0803e0f58b8726ea4e3f3b7d4`, and uses
`tools/ci/publish_extractor.py` to build self-contained Linux x64 and Windows
x64 helpers with LGPL notices and corresponding source. It stages each helper
under the portable bundle's `extractor/` path, generates disposable synthetic
fPKGs with `installer/Tests/Program.cs`, executes both staged helpers, compares
the extracted archive bytes, then runs the Qt/GCC launcher suite with the Linux
staged helper through public `PrepareGame`.

The fixtures cover US, European and Japanese identities, archive relocation,
invalid/unrelated/retail refusal, output preservation, helper failure and
cancel, journal recovery, stale preparation prompt dismissal, containment,
space preflight, and source/player/unowned file preservation. The process
failure/hang controls are disposable copies of the test executable. The actual
helper runs use synthetic packages only; none are playable game data. Evidence
does not claim SteamOS, native Windows launcher GUI, or physical-device tests.
The final merged run's environment, helper hashes, per-platform results, full
CTest output and space-check output are committed under
`launcher/evidence/issue6/`; see `reproduction.md` for the exact environment and
coverage boundaries.

### Linux Gaming Mode session

Use the explicit entry point `pt-launcher --gaming-mode /absolute/path/to/bundle`.
The same executable runs a Qt Core local session server without constructing a
GUI application. Its disposable GUI is a child; closing or crashing that GUI
leaves a live game running. Running the entry point again reconnects to the
bundle's existing supervisor and opens a GUI only if one is absent. The root
exits when the GUI, game and registered work have finished. SIGTERM/SIGINT/SIGHUP
or the IPC `shutdown` operation terminate the GUI and supervised game session.
Do not use SIGKILL as graceful session shutdown.

To add a non-Steam shortcut once, select the shipped launcher executable in
Steam's **Add a Non-Steam Game** dialog, set Launch Options to
`--gaming-mode "/absolute/path/to/bundle"`, and select the standard Steam Gamepad
controller profile. Recreate the shortcut after moving the bundle. The launcher
does not edit Steam's library or infer Gaming Mode from the distribution.
Steam-client window switching and physical Deck behavior are not established by
our controlled-process checks.

The root preserves its original environment for games. Only its GUI child gets
private Qt/plugin and launcher SDL settings. Linux kernel pidfd support is required; startup fails explicitly if unavailable.
The per-user Unix socket is keyed
by canonical bundle path; an exclusive bundle lock prevents duplicate roots.
Protocol v1 uses bounded newline-delimited JSON requests and responses. For
example:

```
pt-launcher --session-command /path/to/bundle '{"v":1,"op":"status"}'
```

Operations are `status`, `reconnect`, `play`, `play-vr`, `stop-game` (requires the
current start identity), `shutdown`, `job-begin` and `job-end` (a nonempty `job`
token). Managed GUI mutations register around the existing public bundle guard.
Normal GUI closure waits for preparation cancellation/completion; a GUI crash
releases its job registrations and leaves existing journals for the established
recovery path. Registration is a lifetime contract, not permission to bypass
bundle locking or update trust. Update transactions must use the same lifetime
contract when that operation is implemented.

Run the maintained noninteractive checks with:

```
launcher/tests/run-controller-test.sh "$HOME/.cache/pt-issue16-acceptance"
```

It builds in the maintained Qt 6.12 container environment, generates disposable
archive and executable fixtures, runs the full launcher CTest suite including
`launcher_supervisor`, fails on errors, and saves environment/build/test logs and
artifact hashes in the specified directory. Python 3 is supplied by that build
image. Supervisor tests use real offscreen Qt GUI processes, local IPC and shell
game fixtures, with no game data, account, signing secrets or device. These are
source-build lifecycle checks, not packaged target/compositor or Steam coverage.

### Scoped desktop acceptance (#11)

`launcher/run-acceptance.sh <evidence-directory>` builds the launcher and runs
inspection, readiness, session, preparation, process/cancellation and existing
settings safety checks. Controller navigation, window-focus and Gaming Mode
supervision tests remain in CTest but are outside this default release gate.
Desktop use does not activate controller polling; developers can opt in with
`PT_LAUNCHER_CONTROLLER=1`. The native package dialog and existing extracted
folder/bundle adoption remain available. VR launch reports an unverified
headset outcome and recognizes only the runtime's explicit `vr: off for this
run:` fallback marker in the first 256 KiB of the current game log. Other
outcomes remain unknown; Open game log provides the runtime's details.
