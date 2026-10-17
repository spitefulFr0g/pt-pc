# Define the launcher's basic settings

Type: grilling
Labels: wayfinder:grilling
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: none

## Question

Which settings must be available before pressing Play, and which remain in the game? Define the smallest useful surface for display/graphics, audio/microphone, and VR comfort, including how flat and VR launches share or separate preferences.

Use changing settings in-game and returning to the launcher as an edge case. Decide whether Play/Play in VR are explicit per-launch choices and what persists. Preserve settings the launcher does not expose; do not duplicate every game option by default.

## Comments

- Claimed for the next live discussion. Proposed starting surface: resolution/window mode, graphics preset, volume, microphone or supported voice-key fallback, and basic VR comfort settings. Exact controls and supported options must match the selected upstream runtime; this recommendation is not yet approved.
- Proposed persistence model: continue using the existing per-platform `pt.ini`, read back in-game edits, preserve unexposed options, and choose flat/VR per launch through existing flags. Do not introduce a separate flat/VR profile system without a user decision.

## Answer

The user approved both the basic settings surface and the existing per-platform settings model.

- Expose resolution and window mode, the upstream graphics preset, volume, microphone selection or the upstream voice-key fallback, and basic VR comfort controls including turning mode and resolution scale. Advanced options remain available in-game.
- Use each platform runtime's existing `data/pt.ini`; Linux and Windows settings remain separate. Read current values when opening settings and after the game exits, so changes made in-game are reflected.
- Preserve settings the launcher does not expose, including unknown keys from supported upstream versions, when the launcher writes. The user accepted that the unchanged game may discard unknown keys during its own saves; see [Choose the GUI and upstream integration boundary](06-launcher-boundary.md). Apply changes only to the settings the player edits; do not rewrite unrelated preferences through a launcher-specific configuration model. While the game is running, avoid competing writes to its settings file; precise interaction belongs in the UI prototype.
- **Play** and **Play in VR** are per-launch actions using existing `--no-vr` and `--vr` behavior. The explicit flat action must override a saved VR-enabled preference. Do not introduce separate flat/VR profiles or persist a different saved VR-enabled value merely because one launch action was chosen.
- Match control values, preset semantics, and availability to the installed supported upstream runtime and platform. Existing custom graphics settings should appear as Custom rather than being overwritten just by opening the launcher. Do not add unsupported options or game/engine code changes to satisfy a GUI control.

This settles product scope and preference ownership. [Choose the GUI and upstream integration boundary](06-launcher-boundary.md) must specify safe settings access/preset reuse and runtime-version compatibility; [Agree on the first-run and everyday launcher screens](07-first-run-and-home.md) owns control layout and interaction. No settings were changed in the user's actual portable bundle.
