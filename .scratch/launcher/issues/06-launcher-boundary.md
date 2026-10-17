# Choose the GUI and upstream integration boundary

Type: grilling
Labels: wayfinder:grilling
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: 01, 02, 03, 04, 05, 07

## Question

Which GUI approach and module boundaries support the agreed package model, basic settings, readiness behavior, and minimal engine divergence on Linux and Windows? Choose how the launcher invokes or reuses installation, extraction, update, settings, and process-launch functionality, and which narrow platform adapters are needed.

Ground alternatives in the existing C++/CMake setup and the UI prototype. Consider whether setup must open when the game renderer cannot initialize. Identify any external toolkit research needed before deciding; do not select a stack merely because it is familiar. Define the limited engine/build changes justified by Linux VR and how future upstream changes will be reconciled.

For the approved manual downstream updater, choose the release identity/endpoint and authenticity metadata, runtime/launcher version pairing, self-replacement/recovery mechanism, and the boundary around reusable existing installer/updater primitives. The existing game updater pins the upstream signing key and assumes an upstream installation layout. Account for the portable layout and its external shared saves/mods without changing game/engine code, replacing the game's key, or enabling test overrides in production. Record what remains possible with unmodified upstream runtimes.

For the approved settings surface, specify how to reuse upstream preset semantics and safely update only edited settings, preserving unknown options and in-game changes. Avoid a competing write while the game is running and avoid depending on the game renderer merely to edit a configuration file. The installed supported runtime's capabilities govern the controls offered.

## Comments

- Prerequisites: [Choose the installation and package model](01-installation-and-packages.md), [Define downstream update ownership](02-downstream-updates.md), [Define the launcher's basic settings](03-basic-settings.md), [Establish the native Linux VR prerequisites](04-native-linux-vr.md), [Define readiness and VR failure behavior](05-readiness-and-vr-behavior.md), [Agree on the first-run and everyday launcher screens](07-first-run-and-home.md).
- User clarified the integration boundary: everything underneath the GUI should remain the same and compatible with upstream. Use supported command-line/settings interfaces, retain the portable layout and existing direct/script launches, and preserve archive/save/mod formats. Do not require a patched engine to use the launcher. [Reconcile Linux VR with upstream compatibility](09-linux-vr-compatibility.md) records the approved Linux build/packaging boundary; its answer constrains this architecture decision.
- Architecture discussion: user approved using `spitefulFr0g/pt-pc` for releases, with a dedicated launcher channel selecting matching downstream bundles. Exact signed metadata and channel selection remain to settle.
- User approved keeping the launcher open during play and allowing it to close independently without stopping the game. Process ownership must support that behavior; reopening must recheck the bundle's game activity before enabling updates/settings writes.
- User approved a standalone C++ application using Qt 6 Widgets, with required Qt libraries bundled. Preserve the selected visual design; the HTML remains a throwaway specification. Build with CMake independently of game renderer initialization.
- Read-only fact audit (subscription route verified as `codex/chatgpt`) confirmed standalone installer linkage at `CMakeLists.txt:160–201`, reusable archive/payload primitives in `installer/Native/setup_core.h`, and renderer-independent presets in `src/engine/platform/graphics_presets.cpp`. Existing install/update orchestration assumes a different layout and cannot be reused unchanged. No build or game launch was performed.
- Audit found two compatibility qualifications for live discussion: `SaveAppSettings` rewrites a fixed known-schema INI (`settings.cpp:280,484`), so launcher edits can preserve unknown keys but the unmodified game's saves may discard them. With `--save-dir`, upstream enhanced-texture caches are stored with saves (`main.cpp:7576–7593`); ordinary pipeline cache stays in platform data. These qualify earlier preservation/cache statements rather than authorizing engine patches.
- Toolkit evidence: [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html) supports C++/CMake and styled desktop controls; [Windows deployment](https://doc.qt.io/qt-6/windows-deployment.html) and [Linux deployment](https://doc.qt.io/qt-6/linux-deployment.html) require bundling the appropriate libraries/plugins and accounting for system dependencies. [Tauri webview documentation](https://v2.tauri.app/reference/webview-versions/) records its platform webview dependency. Qt selection is the user-approved tradeoff; no exact Qt version or deployment platform has yet been validated.

- User approved preserving the unchanged game's own INI serialization, qualifying unknown-key preservation to the launcher's edits.
- User approved the existing enhanced-texture-cache placement under shared `saves/`, with other caches following upstream per-platform behavior.
- User approved a separate signed downstream updater and replacement/recovery helper, preserving the game's original trust key.

- User explicitly accepted the complete integration plan, including supported-runtime adoption and recoverable explicit alignment. This resolves the architecture decision; release/validation acceptance remains separate.

## Answer

### Application and integration boundary

Use a separate C++/CMake executable with Qt 6 Widgets. Recreate the accepted G design with the local licensed forest photo and gothic font; the HTML is reference material. Bundle required shared Qt libraries, plugins, and notices. Do not depend on initializing the game's Vulkan/D3D/OpenXR renderer to prepare files or edit settings. Toolkit system/display prerequisites still apply; package formats, supported OS baselines, exact dependency versions, and validation belong to Define release and validation acceptance.

Keep launcher code and its build target separate from game/engine sources. Game builds and direct/script launches remain usable without building or installing Qt. Preserve the portable layout and unchanged game/archive/save/mod formats. Native Linux VR integration remains limited to approved build/packaging adjustments.

Keep the UI thin over launcher-owned operations for bundle inspection/preparation, settings, readiness/session monitoring, and updates. Narrow Windows/Linux adapters handle process identity/liveness, platform runtime discovery, and replacement behavior. Long extraction/network operations run off the UI thread, with progress/cancellation delivered back to it. Serialize bundle mutations with an interprocess bundle lock; recheck activity immediately before writing. Upstream global progress hooks require one install/extraction operation at a time.

### Installation and settings

Reuse upstream source/archive recognition, extraction helper, containment, payload/hash verification, HTTP/cryptography utilities, and transaction primitives where they fit. Compile reusable upstream sources unchanged into launcher targets/adapters. Do not call the existing full RunInstall/ApplyGameUpdate orchestration unchanged: it assumes a different installation layout and upstream update trust. Launcher-owned orchestration maps these operations onto the approved separate game/runtime/player-data directories, with staging, cancellation, and recovery.

Reuse the matching runtime revision's AppSettings/preset logic without initializing the game. Patch the current INI text rather than calling the upstream whole-file serializer: reread before commit, preserve unedited/unknown entries and formatting, update only the user's edited keys (including all upstream keys affected by an explicitly selected preset) plus the approved upstream-update policy, and atomically replace. Detect concurrent file changes and reload/reconcile instead of blindly overwriting. No settings writes while the bundle's game is running or activity cannot be determined. Unknown graphics capability remains unknown; do not assume ray-tracing support for Ultra.

Unknown keys are preserved by launcher writes; the unchanged game's own saves may discard them. Enhanced-texture caches follow upstream --save-dir behavior and may share saves/. These are user-approved qualifications to earlier settings/cache descriptions, not engine changes.

### Runtime compatibility and sessions

User-approved adoption policy: identify supported runtime builds using release metadata and file hashes. Offer full settings/preset/readiness integration only for explicitly supported matching builds. Recognized game archives and player data can be adopted independently of runtime identity. An unknown runtime is reported as unverified; do not guess its setting semantics or claim compatibility. Offer an explicit action to align managed program files with the bundled matching runtime, preserving player data and retaining a recoverable prior program version. Never silently replace an adopted runtime. Existing direct/script launch remains available.

Invoke the supported runtime with its existing explicit game/save/mod/settings/log paths, --no-vr or --vr, and --no-update-check. Launch independently so closing the launcher leaves the game running; keep the launcher open by default. Track process identity and start time as well as PID, and inspect bundle activity again on reopening, including direct/script launches. The launcher's own lock alone does not prove the game is stopped. Uncertain activity blocks destructive updates and settings writes.

Use a fresh per-launch log and version-specific parsing for known VR-fallback/error messages. Process creation is not successful gameplay or headset presentation. Missing/unrecognized logs remain unknown. Follow the approved continue-flat/retry flow without killing a working fallback. Archive header/identity checks establish recognition, not complete integrity or a successful game session.

### Downstream updates and upstream maintenance

Discover releases from the GitHub releases API for spitefulFr0g/pt-pc, filtering the dedicated launcher channel (tags launcher-v<version>, excluding drafts/prereleases for the normal channel). Do not use an unfiltered latest release. Publish launcher-manifest-v1.json and its detached Ed25519 signature with paired native payloads. The signed document identifies repository/channel, release ID, platform/architecture, launcher/runtime versions and upstream revision, package/file hashes and sizes, and supported integration capabilities. Verify the exact manifest bytes against a downstream public key pinned in the launcher before trusting fields; verify downloaded payloads and containment against that signed inventory. No test-key override in production. The unchanged game retains its upstream key.

One downstream release identity pairs launcher and runtime; the updater rejects mixed/incomplete or unsupported pairs. The first installation's bootstrap trust comes from the downloaded release bundle; signed updates protect subsequent checks. Signing-key provisioning is implementation/release setup, not performed during planning. A v1 key replacement requires a manually installed trusted bundle unless a separate authenticated rotation design is later approved.

Use a small launcher-owned helper outside the files being replaced. After explicit Check then Install, stage on the destination filesystem, verify, check no game is active, and journal the transaction before closing/replacing the launcher and managed runtime files. Recheck activity immediately before commit; preserve game archives, source package, settings, saves, mods, and unowned files. Back up managed files and restore on incomplete/failed replacement; recover an interrupted transaction at next start through a stable helper entry point. A new-launcher startup acknowledgement plus installed pair verification completes the transaction; this is not a gameplay-health claim. Test crash/power-loss recovery and Windows in-use files before acceptance. No whole-bundle destructive swap and no claim of a multi-directory atomic rename.

Pin integration adapters and presets to supported upstream revisions; advance them deliberately with each paired downstream release and rerun compatibility tests. Keep launcher-specific policies outside game/engine code. Additional adapter/build changes that exceed the approved boundary must reopen the decision. Concrete packages, shortcuts, target environments, physical VR evidence, and release/recovery acceptance remain in Define release and validation acceptance.

Evidence: source audit above; installer paths at setup_core.h:164,203,347,485,999; versioned preset logic at graphics_presets.cpp:3; settings serializer at settings.cpp:280,484; explicit-key manifest verification at update_check.cpp:216; fallback logs at main.cpp:7364,7613. [Qt detached process documentation](https://doc.qt.io/qt-6/qprocess.html#startDetached) confirms independent lifetime and warns about PID reuse. These are source/design findings; no production launcher, packaging, build, update, or hardware check has been executed.


Final acceptance-policy clarification: [Define release and validation acceptance](08-release-and-validation.md) records the user's explicit later instruction that all required checks are automated and agent-run, with no mandatory physical or human tests. Any earlier physical/headset validation references describe evidence limits or optional future observations, not a release gate. Gaming Mode/controller scope is being specified separately.


Accepted Gaming Mode extension: [Design Steam Deck Gaming Mode and controller navigation](10-steam-deck-controller-ui.md) adds a standalone SDL3 input adapter and Linux session supervisor. For Steam Gaming Mode, the supervisor owns the game/session and uses a same-PID exec-style takeover during its own update; the GUI's detached lifetime alone does not establish session survival under Gamescope. This extends the launcher boundary without changing game/engine code.
