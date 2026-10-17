# Personal Linux and Windows launcher

Labels: wayfinder:map
Status: resolved

## Destination

An implementation-ready specification for a friendly GUI launcher in this personal downstream: select the player's dump/fPKG, install, check readiness, choose basic settings, and launch the native game in flat or VR mode on Linux and Windows.
The map is complete when implementation can proceed without unresolved product, integration, packaging, or validation decisions.

## Notes

- Feature branch: `launcher`. Charting and decisions belong here; this effort is planning, not launcher implementation.
- Confirmed with the user: native Linux and Windows; flat and VR launch on each; source compilation stays outside the normal launcher flow. The launcher is an optional GUI over the existing project, preserving upstream compatibility and the underlying game behavior and formats.
- Preserve the approved portable layout and existing direct launch paths. Use existing game arguments/settings; avoid a launcher-specific game, archive, save, or mod format. The Linux integration boundary is recorded in [Reconcile Linux VR with upstream compatibility](issues/09-linux-vr-compatibility.md).
- Consult `wayfinder`, `grilling`, and `domain-modeling` for decision sessions; use `research` for external investigations and `prototype` for the UI discussion.
- Terminology: [Project glossary](../../GLOSSARY.md). Repository evidence: [Launcher starting points](context.md).
- Tracker: local Markdown in this directory, with one child decision file per ticket. Use the conventions below; `/setup-matt-pocock-skills` can configure a different tracker if desired.
- Current canonical specification and active tracker: [Personal P.T. launcher for Windows and native Linux — GitHub issue #1](https://github.com/spitefulFr0g/pt-pc/issues/1). Its existing specification has been reconciled with this completed map. The local map/decision files are resolved evidence only. Future `$to-spec` updates that issue in place; do not create a second spec issue or local spec. `$to-tickets` uses that canonical issue when requested.
- Children are individual files in `issues/`. Scan open, unassigned children whose `Blocked by` tickets are all resolved, ordered by filename, to find the frontier. Claim by setting `Status: claimed` and `Assignee:` before working. Append resolutions under `## Answer`, then set `Status: resolved` and index the answer below. Use only `wayfinder:` labels.
- Billing is subscription-only. API-priced models/providers require explicit authorization for that work. Verify the route before delegation and preserve this restriction in every handoff. If capacity or verification fails, wait or report the blocker; do not fall back to API billing.
- Charting responses about packaging, updates, and current VR use are captured in their child tickets. Those inputs do not settle the remaining questions in each ticket.
- Windows/Quest 3/Steam Link remains the intended VR use context; registered Windows OpenXR configuration currently points to Virtual Desktop. Linux targets are SteamOS on Steam Deck and Omarchy on the user's Surface (launcher-only on Surface). Gaming Mode/controller navigation is required in the first Linux preview. All required release tests are automated and agent-run: no mandatory human or physical-device/headset checks. Record actual automation environments and evidence without claiming unperformed hardware tests.

## Decisions so far

- [Choose the installation and package model](issues/01-installation-and-packages.md): one portable bundle at a time, with bundled runtime, existing-folder adoption, shared game/saves/mods, and separate platform settings.
- [Define downstream update ownership](issues/02-downstream-updates.md): manual downstream-only updates pair launcher/runtime, preserve player data, block while playing, and roll back on failure; the bundle disables upstream checks using existing settings.
- [Define the launcher's basic settings](issues/03-basic-settings.md): expose basic display/graphics/audio/voice/VR comfort controls, reuse per-platform settings, preserve in-game edits, and select flat/VR per launch.
- [Establish the native Linux VR prerequisites](issues/04-native-linux-vr.md): Linux build/loader packaging and simulator wiring need attention; physical VR compatibility remains unverified.
- [Define readiness and VR failure behavior](issues/05-readiness-and-vr-behavior.md): separate flat/VR readiness, actionable blockers, explicit unknown/Try anyway, and visible upstream flat fallback with continue/retry choices.
- [Agree on the first-run and everyday launcher screens](issues/07-first-run-and-home.md): G's split layout with freely licensed spooky forest photography, green palette, gothic title, stable Play label, and concise status/error copy.
- [Reconcile Linux VR with upstream compatibility](issues/09-linux-vr-compatibility.md): Linux VR may use build/packaging adjustments while game/engine code and player-data formats remain unchanged.
- [Choose the GUI and upstream integration boundary](issues/06-launcher-boundary.md): standalone C++/Qt 6 Widgets launcher; versioned upstream adapters, explicit runtime alignment, independent game sessions, and a separately signed downstream updater with recovery.
- [Define release and validation acceptance](issues/08-release-and-validation.md): Windows ZIP/Linux tar.gz; Windows 11, SteamOS Gaming Mode and Omarchy targets; required Deck controller functionality and entirely automated agent-run acceptance, with no physical/human release gate.

- [Design Steam Deck Gaming Mode and controller navigation](issues/10-steam-deck-controller-ui.md): accepted A/B/X controller flow, built-in keyboard/file picker, explicit input handoff, and a Linux session helper that preserves a running game across GUI closure; agent-run automation throughout.

## Not yet specified

- None. All decision tickets are resolved and their specification is already synthesized in [the canonical GitHub issue](https://github.com/spitefulFr0g/pt-pc/issues/1). Repeated `/to-spec` invocations update that issue in place; the next build-planning step is `/to-tickets` from that issue when requested. No production implementation or native package/runtime validation has been completed by this map.

## Out of scope

- Implementing the launcher as part of charting this map.
- Compiling the game from source through the normal launcher GUI.
- macOS and a Proton launch route in this first launcher effort.
- Obtaining game archives for the player; the source remains the player's own dump/fPKG.
- Public service infrastructure such as accounts, a hosted game library, or a public mod marketplace; this destination is a personal-use launcher.
- Launcher-specific changes to game behavior or archive/save/mod formats, and a requirement to use the GUI to play an otherwise working upstream-compatible installation.
