# Reconcile Linux VR with upstream compatibility

Type: grilling
Labels: wayfinder:grilling
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: 04

## Question

How should the planned native Linux VR option respect the user's requirement that everything underneath the GUI stay the same and remain compatible with upstream? Are Linux build/packaging adjustments acceptable while game code and player-data formats remain unchanged, or must the launcher wait for a suitable upstream-provided runtime?

Define the boundary between launcher integration and runtime changes. Carry its consequences into the existing readiness and validation decisions, which own unavailable-capability UI and evidence requirements. Do not silently remove Linux VR from the destination or silently authorize engine changes to meet it.

## Comments

- Prerequisite: [Establish the native Linux VR prerequisites](04-native-linux-vr.md).
- User explicitly required upstream compatibility after approving the existing portable layout. Research identifies build/loader packaging gaps, but no fixes or runtime tests were performed.
- Claimed for the follow-up design question: may Linux build/packaging be adjusted while preserving unchanged game code and data formats? Recommendation: yes; this is not yet the user's decision.

## Answer

The user explicitly approved build and packaging adjustments for Linux VR while keeping game/engine code and data formats unchanged. This preserves the original project's game beneath the optional GUI; it does not authorize a launcher-specific engine fork or data conversion.

Allowed integration includes selecting existing build options, supplying the proper platform dependencies, staging libraries/notices, and arranging loader discovery through build/package configuration or the launched process's environment. These are boundaries, not a claim that an unchanged-code Linux VR build has been demonstrated. Source compilation remains part of producing releases, outside the normal player flow.

The research artifact also proposed an executable-relative loader edit as one possible approach. That game/engine source edit is outside this decision's approved boundary; evaluate configuration/packaging alternatives first. If working Linux VR requires game/engine code changes, report that conflict and reopen the boundary decision rather than treating this approval as permission for those changes.

Linux VR remains a target, not a verified capability. [Define readiness and VR failure behavior](05-readiness-and-vr-behavior.md) owns how unsupported/missing capabilities appear and how an attempted VR launch fails or recovers. [Define release and validation acceptance](08-release-and-validation.md) owns the required build, simulated, and physical-headset evidence. Neither was resolved by this answer.


Final acceptance-policy clarification: [Define release and validation acceptance](08-release-and-validation.md) records the user's explicit later instruction that all required checks are automated and agent-run, with no mandatory physical or human tests. Any earlier physical/headset validation references describe evidence limits or optional future observations, not a release gate. Gaming Mode/controller scope is being specified separately.
