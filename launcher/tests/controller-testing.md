# Controller #13 acceptance

From the repository root:

```sh
launcher/tests/run-controller-test.sh "$HOME/.cache/pt-issue13/acceptance"
```

The command builds the standalone launcher in Release with assertions enabled in tests, using `pt-launcher-preview-build-env-complete`, Qt 6.12.0 at `/tmp/launcher-qt-6.12.0`, g++13 and SDL's pinned `release-3.4.16`. Override the SDK, build tools and image using `PT_QT612_ROOT`, `PT_BUILD_TOOLS`, `PT_LAUNCHER_BUILD_IMAGE`. Build tools default to the shared preview CMake3.31 installation. Use an ext4 evidence directory with space for builds and executable disposable fixtures; shared `/tmp` may be full or non-executable. The checkout and SDK are mounted read-only. No game data, account or physical device is required.

It runs **all launcher CTests**, propagating configure/build/test failure through pipefail, and saves `controller-acceptance.log`, `configure.log`, `build.log`, `ctest.log`, `environment.txt`, `image-id.txt`, `artifact-sha256.txt` and the build. Plain `QT_QPA_PLATFORM=offscreen ctest --test-dir BUILD --output-on-failure` also works: the Linux controller, launch, window and preparation UI tests each provision their own Xvfb/Openbox/xcb display. CTest derives the plugin path from its configured Qt kit. Xvfb, Openbox, xauth and xprop are supplied by the build image; missing tools fail the test.

`launcher_controller` uses real SDL virtual gamepad events and Qt widgets/event loops. It covers dead zone (±8000), immediate directional navigation, 360ms initial delay/110ms repeats, disabled controls, actual scroll-to-focus, native combo selection, spin adjustment, draft selection, hints, modal focus restoration, foreground/ownership suppression, disconnect/reconnect, idempotent handle release and neutral-before-rearm. A 4096-event flood leaves work after a Qt turn, proving the 128-event pump bound. The actual Settings dialog is edited, kept open through its safe-default unsaved-changes prompt, and applied through SDL input.

UI actions are queued after the polling timer returns, with foreground/session checks again at delivery. This lets B work inside an A-opened `QDialog::exec()`; held A cannot activate its default. Edges/repeat state are consumed before any UI callback. The local SDL override disables raw Deck HIDAPI even if the parent enabled it; it does not change the environment. X selects the native draft and emits Qt's `RequestSoftwareInputPanel` event, an entry seam for #15. Combo X opens native choices. This slice does not supply the #14 picker, #15 keyboard, or #17 supervisor/Gaming Mode integration.

`launcher_controller_launch` drives the actual launcher Play and Try VR anyway buttons through a virtual gamepad against disposable executable fixtures. A controlled slow loader inventory holds VR launch pending, proving handle release at request time. It checks held-A suppression during play/return, neutral rearming, exactly two deliberate starts, flat/VR arguments, and child environment preservation without launcher-added SDL hints or an invented Steam App ID. It also catches the game-exit transition restarting readiness indefinitely.

## Repair evidence

Repair branch starts at integration `d1b06d6`, merges candidate `a3595b8`. The unchanged candidate passed 10/10 using the known-good fresh-container harness; the earlier worker's 3/10 result was not reproduced. No conservative process/activity guard was weakened. Repair logs live under `/home/watermelon/.cache/pt-issue13/r2` (initially `/tmp/pt13-r2`, moved after tmpfs exhaustion).

Observed red → green checks: empty combo numerical exception; stale ownership click; repeated-open SDL reference leak; no native draft selection; unusable combo popup; missing spin adjustment; inherited raw-Deck hint defeating local policy; B unable to close an A-opened nested modal; game-exit Play never re-enabling; controller handle retained during pending VR. `*-red.log` and corresponding `*-green.log`/full-suite logs preserve the verdicts. The initial held-A-only modal check passed before repair; its stronger B-before-timeout assertion supplied the actual nested-loop red. An early launch probe used a noexec tmpfs and was corrected to ext4 before drawing product conclusions.

This is controlled Linux/X11/virtual SDL evidence. It does not establish native Windows controller behavior, Steam Input delivery, Steam client, Gamescope/Wayland or physical hardware behavior. Native package/compositor gates remain separate and unrun here; no full product/release acceptance is claimed.

For a combined run including #6's optional real extraction cases, set `PT_LAUNCHER_STAGED_EXTRACTOR` and `PT_LAUNCHER_TEST_PKG` to its already-generated helper directory and synthetic fixture package. The harness mounts both read-only and forwards their container paths; it fails if only one is supplied. These are synthetic fixtures, never player files.

Final merged Release run: **11/11 passed**, including #6 conditional real helper/package cases, exit 0 in 22.78 seconds. Committed evidence: `launcher/evidence/issue13-r2/`; full external logs/build: `/home/watermelon/.cache/pt-issue13/r2/`. This is full applicable launcher-suite evidence, not full cross-platform release acceptance.
