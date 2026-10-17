# Define downstream update ownership

Type: grilling
Labels: wayfinder:grilling
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: none

## Question

Should the first launcher include updates, and if so how are launcher and runtime versions paired, selected, and installed without replacing downstream additions with upstream binaries? Decide when the user is prompted, what explicit actions are required, what happens offline, and how existing in-game and installer update entry points behave.

The checkout's update endpoint defaults to upstream; its Git remote is the downstream. Consider repair, rollback, saves/mods preservation, a failed download, and the game running during an update. Do not assume a downstream release pipeline already exists.

## Comments

- User confirmed during charting: updates require an explicit action and install only downstream builds. Version pairing, the update/recovery flow, downstream release availability, and handling the existing upstream update entry points remain undecided.
- Compatibility constraint: preserve the underlying upstream game and formats. The existing `--no-update-check` option is one potential way to suppress competing upstream discovery on GUI launches without patching the engine; decide the complete policy, including direct launches, before choosing it.
- Claimed for the next user discussion. Proposed policy, awaiting confirmation: manual Check for updates followed by Install; a paired launcher/runtime release; preserve archives, saves, mods, and settings; block installation while the game is running; restore the prior version if installation fails. Disable the game's existing upstream update discovery using its supported setting so the ordinary bundle defaults to downstream updates without game/engine code changes. Exact release metadata, authenticity checks, and installer reuse must be checked before treating the policy as implementation-ready.

## Answer

The user approved this update policy:

- Updates are manual: **Check for updates**, inspect the offered downstream release, then explicitly **Install**. Do not automatically download or install updates at startup.
- Offer only releases of this personal downstream. Update the launcher and matching native runtime as a compatible release unit; do not substitute upstream binaries that discard launcher integration.
- Preserve the player's game archives, retained source package, saves, mods, and settings. Update only the release's managed program files. Existing platform data stays separate, as agreed in the installation decision.
- Refuse installation while the game is running. Stage and verify before replacing files; if installation fails, restore the previous working release. A failed check/download or offline state leaves the installed game available.
- Disable the game's upstream update discovery through its existing `[network] check_updates = 0` setting in the bundle, including adopted platform settings. Preserve other settings. GUI launch can also use supported `--no-update-check`. This avoids game/engine code changes and makes ordinary direct launches respect the bundle's update default; manually re-enabling the original setting remains a user-controlled upstream behavior.

This resolves update ownership and player-facing behavior, not the updater implementation. Source inspection found that the original updater verifies releases against the upstream public key, and that existing installer layout/manifest assumptions differ from the approved portable bundle. [Choose the GUI and upstream integration boundary](06-launcher-boundary.md) must choose a downstream release identity, verification/trust metadata, the launcher-owned integration around reusable updater/install primitives, and recovery/self-replacement mechanics on both platforms. Do not use test key overrides or bypass verification to make downstream releases install.

[Define release and validation acceptance](08-release-and-validation.md) must validate rollback, runtime/launcher pairing, data preservation, game-running checks, offline launch, and the default upstream-check setting. No downstream release pipeline, update installation, or recovery test has been implemented or exercised yet.
