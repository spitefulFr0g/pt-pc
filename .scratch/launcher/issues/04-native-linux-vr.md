# Establish the native Linux VR prerequisites

Type: research
Labels: wayfinder:research
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: none

## Question

What build, packaging, runtime-selection, and verification work is required for native Linux VR launch, and which Windows VR prerequisites should the launcher also account for?

Investigate the repository plus current primary OpenXR/runtime documentation. Distinguish existing code paths, simulated checks, and actual hardware validation. Identify the smallest plausible changes outside the launcher needed to enable Linux VR, without implementing them. Explain which readiness checks can run without entering the game and which capabilities must be tested through game launch or a headset.

The existing Linux loader path is not proof of working native Linux VR. Record uncertainty and platform constraints explicitly. Do not choose the user's headset or runtime for them.

## Comments

- Research must run on a verified subscription route. Capture findings on the throwaway `research/launcher-linux-vr` branch, push that branch, and open no PR. It is not merged into `launcher`.
- Claimed before delegation. Billing guard verified `subscription` via `codex/chatgpt` for `codex/gpt-6.1-sol`; verify again at worker startup. Parent context: [Launcher starting points](../context.md). Findings path on the research branch: `.scratch/launcher/research/native-linux-vr.md`.
- The background research worker is running in an isolated worktree. Its receipt is [Research task receipt](../research-task.json). It must return evidence before this ticket is resolved.
- The user currently plays VR only on Windows, describes OpenXR, and often launches through Steam Link. Actual headset and active OpenXR runtime are unconfirmed; do not treat Linux VR or any particular hardware/runtime combination as validated.

## Answer

Research completed on the verified `codex/chatgpt` subscription route. The findings were committed as `779ab50a40d05fa9fb57a401d6c55cbd8dc216a1` and pushed on the throwaway research branch. No PR was opened and the branch is not merged.

Canonical evidence: [Native Linux VR prerequisites and Windows launcher checks](https://github.com/spitefulFr0g/pt-pc/blob/779ab50a40d05fa9fb57a401d6c55cbd8dc216a1/.scratch/launcher/research/native-linux-vr.md). It contains pinned repository evidence and primary-source citations. Read that artifact for loader discovery rules, backend requirements, readiness-probe limits, proposed packaging work, and the pending validation matrix.

The native Linux host has an OpenXR loader path, but current configuration defaults support off and gates support/staging on a Windows DLL package. Linux needs an appropriate loader/build/package strategy and reliable discovery of `libopenxr_loader.so.1`; the test-runtime manifest also assumes a DLL filename. These are identified gaps, not implemented or validated fixes. Existing `--vr` and `--no-vr` arguments support launch-mode selection. File presence and a successful process exit do not prove a VR session started. Neither native Linux nor the user's exact Windows physical-headset route was tested by this investigation.

The user then strengthened the requirement to preserve the underlying project for upstream compatibility. Recommendations in the research artifact are proposals only: game/engine modifications are not authorized by the research outcome. [Reconcile Linux VR with upstream compatibility](09-linux-vr-compatibility.md) now decides what build/packaging integration is acceptable and how unsupported runtime capabilities should appear in the launcher.


Final acceptance-policy clarification: [Define release and validation acceptance](08-release-and-validation.md) records the user's explicit later instruction that all required checks are automated and agent-run, with no mandatory physical or human tests. Any earlier physical/headset validation references describe evidence limits or optional future observations, not a release gate. Gaming Mode/controller scope is being specified separately.
