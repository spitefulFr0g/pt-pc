# Define readiness and VR failure behavior

Type: grilling
Labels: wayfinder:grilling
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: 01, 04, 09

## Question

What does Ready mean for a flat launch versus a VR launch, and how should the launcher help when something is missing? Decide which missing prerequisites block each launch action and define repair/retry behavior. Carry the user's hardware context into the release/validation ticket, which owns the exact test configurations.

Include missing or damaged game archives, unsuitable graphics support, no OpenXR loader/runtime, no connected headset, and VR initialization failing after a successful preflight. Decide whether an explicit VR request may silently become a flat game; the existing game currently falls back to flat mode.

## Comments

- Prerequisites: [Choose the installation and package model](01-installation-and-packages.md), [Establish the native Linux VR prerequisites](04-native-linux-vr.md).
- User input during charting: currently plays VR on Windows only; describes OpenXR and often launches using Steam Link. The headset and active OpenXR runtime still need confirmation. Steam Link is a streaming path, not itself the name of the OpenXR runtime. Linux VR is a planned target with validation not yet arranged.
- Additional prerequisite: [Reconcile Linux VR with upstream compatibility](09-linux-vr-compatibility.md); readiness must reflect the actual compatible runtime's capabilities, not require changing the game beneath the GUI.
- Claimed for the next live discussion. Proposed policy, not yet approved: independent readiness for flat and VR; explain proven blockers with a repair/setup action and Retry; treat an inconclusive check as unknown with an explicit Try anyway option, rather than claiming success or enforcing a guessed blocker.
- Proposed VR failure policy, not yet approved: preserve upstream's fallback to flat when it occurs, report that outcome accurately, and let the player continue flat or close and retry VR. Do not promise a universal fallback: renderer initialization may fail outright. Unknown outcomes stay unknown; do not require an engine patch or terminate a running flat fallback merely to enforce a launcher preference.
- Hardware information still needed: the user's actual Windows headset model. OpenXR and Steam Link do not identify that model. Linux remains a planned platform with physical validation not yet assigned.

## Answer

The user approved separate readiness and the proposed upstream-preserving VR failure behavior, then identified the headset as **Meta Quest 3**. Current target context is Windows, Quest 3, often through Steam Link; the active OpenXR runtime remains unconfirmed. Linux VR stays planned and unverified.

- Display readiness independently for flat and VR. Missing game/runtime files or a proven incompatible prerequisite blocks the affected action, with a plain explanation, a relevant setup/repair action, and Retry. VR-only problems must not disable an otherwise usable flat launch.
- Distinguish a proven blocker from an inconclusive check. Unknown results show a warning and allow an explicit Try anyway action; neither call them ready nor infer unsupported hardware from a failed probe.
- Preflight evidence is not proof of a working in-headset session. Report launch outcome only when it can be determined; a successful process start is insufficient. Keep unknown outcomes explicit.
- If upstream falls back to flat after a failed VR initialization/session start, explain what happened and allow the player to continue flat or close the game and retry VR. Preserve that behavior without patching the engine or forcibly terminating a working flat session.
- Some failures, such as renderer initialization failure, may exit instead of falling back. Show the actual failure and relevant log/retry actions; do not promise universal fallback. Retrying must not create a second competing game instance.

[Agree on the first-run and everyday launcher screens](07-first-run-and-home.md) owns how these states and actions are presented. [Choose the GUI and upstream integration boundary](06-launcher-boundary.md) owns exact probes, time bounds, log/outcome detection, and platform adapters. [Define release and validation acceptance](08-release-and-validation.md) owns the hardware/runtime matrix and physical evidence; no headset test has yet been performed.


Final acceptance-policy clarification: [Define release and validation acceptance](08-release-and-validation.md) records the user's explicit later instruction that all required checks are automated and agent-run, with no mandatory physical or human tests. Any earlier physical/headset validation references describe evidence limits or optional future observations, not a release gate. Gaming Mode/controller scope is being specified separately.
