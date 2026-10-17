# Choose the installation and package model

Type: grilling
Labels: wayfinder:grilling
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: none

## Question

What does the player download, and which installation does the launcher manage? Decide whether launcher and matching native runtime ship together or the launcher downloads the runtime, whether an existing installation can be adopted, whether the first version manages one installation or several, and where game archives and user data live.

Use fresh install, adopting an existing install with saves/mods, moving the installation, and launching without a network connection as concrete scenarios. Source compilation is already outside the launcher flow.

## Comments

- User confirmed during charting: bundle launcher and matching runtime so setup/play work offline with their own dump/fPKG. Existing-install adoption, installation count, and storage remain undecided; keep this ticket open until the whole question is resolved.
- Claimed to inspect the real Windows Desktop export offered by the user and use that evidence in the installation discussion. Inspection is read-only; game files must not be copied into planning artifacts or the repository.
- Evidence: [Windows Desktop export example](../windows-export-example.md). The example is an existing portable bundle with extracted archives, both native runtimes, shared saves/mods, separate platform settings by documented intent, and launch scripts using explicit paths. Relevant archive identity probes passed; neither runtime nor cross-platform save compatibility was tested. Preserving/adopting this layout remains a recommendation for the user to decide.
- User subsequently approved preserving/adopting this layout and clarified that everything underneath the GUI should stay the same to retain upstream compatibility. Shared archive/save/mod locations and platform-specific settings are approved; existing game formats and direct/script launch remain usable. The installation-selection model (one portable bundle at a time versus a library of several) remains open, so this ticket is not yet resolved.

## Answer

The user approved one portable folder at a time, completing the installation/package model decision.

- Distribute the launcher with its matching native game runtime and dependencies. Setup and play work offline once the player supplies their own supported dump/fPKG and the system prerequisites are installed. Source compilation is outside the normal launcher flow.
- The unit managed by the GUI is one portable bundle, following the user's existing `pt-pcport` layout. There is no multi-installation library in the first version; separate copies are independent.
- Support adopting the existing bundle in place, preserving its extracted game archives, saves, and mods. Already extracted recognized archives do not require re-extracting the retained source package merely to adopt them.
- Preserve `game/CUSA01127/`, shared `saves/` and `mods/`, and platform runtimes under `runtime/linux/` and `runtime/windows/`. Settings, logs, and ordinary caches remain separate under each runtime's existing `data/` location; the user later approved upstream enhanced-texture caches sharing `saves/` as recorded in [Choose the GUI and upstream integration boundary](06-launcher-boundary.md). Retain the original package when it is already present; it is not required for everyday launch from an intact extracted installation.
- Fresh setup from a supported dump/fPKG prepares the same portable structure. Player-owned game archives are supplied locally, not included in the distributed runtime package. Folder movement must retain the bundle-relative relationships used by the current scripts.
- The GUI remains optional. Existing runtime flags, game/data formats, and direct/script launch paths stay usable. The shared-save layout is approved, but actual save compatibility across the specific runtime builds still needs validation; approval of the layout is not test evidence.

Evidence and example: [Windows Desktop export example](../windows-export-example.md). [Agree on the first-run and everyday launcher screens](07-first-run-and-home.md) owns folder-selection and setup interaction details. [Define release and validation acceptance](08-release-and-validation.md) owns concrete package/shortcut choices and validation evidence.
