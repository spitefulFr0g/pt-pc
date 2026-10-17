# Graphics #8 repair evidence

Candidate: `49108474da40867518daf3d88619d9a8d68b8f9d`, merged into the already bound `t3/repair-graphics-preset-candidate` branch based on `d1b06d6`. Local integration `b6907be3400873b6267ee91c81bda8b789dfcafb` was subsequently merged. Subscription verification returned `Verified: Codex subscription authentication (chatgpt)`. No delegation/provider fallback/API billing.

## Behavioral red

- `red-normalization.log`: original candidate with the new public-operation legacy Ultra regression, before the repair. 8/9 passed; `launcher_settings` aborted on expected Ultra detection from `ray_quality=2`. Full command: `launcher/run-issue8-graphics-container.sh /tmp/pt-issue8-r2-red-normalization`.
- `red-conflict.log`: replay of the candidate's enum-only conflict predicate with the new public-operation Custom A -> Custom B regression. Assertion requiring Conflict/no-write failed.
- `red-enum.log`: replay of the candidate's Custom-only enum validation, with the other repairs retained. Invalid-enum Failed/no-write assertion failed.
- `red-custom.log`: replay of the candidate's enabled Custom row. Real Qt assertion requiring the row to be disabled failed.
- `red-bool-comment.log`: new regression against the repaired adapter before fixing affected boolean inline-note handling. Original preset did not read back as Original from `bloom = 1 ; note`.

Predicate/row replays were temporary local edits, restored before committing. They exercise real operations/widgets, not linkage failures or mocked internals. The original candidate's prior 9/9 was insufficient; its earlier missing-symbol/test-expectation failures are not counted as behavioral red here.

The first full repaired run after merging #6 also revealed an overly strict new assertion: it expected an unedited comment to remain adjacent to a section header even though missing deliberate keys must be inserted there. The assertion was corrected to verify each unedited line's exact bytes independently. The rebuilt suite passed 9/9 in 9.85s. Final wrapper result is saved alongside this file.

## Environment and limits

Ubuntu 24.04.5 container, g++-13, Qt 6.12.0, offscreen Qt controls plus the existing Xvfb/Openbox process/window tests. Exact container identity, source hashes, configuration and final logs are committed here; full build artifacts are under `/home/watermelon/.cache/pt-issue8/`.

Shared `/tmp` reached 100% usage during a fresh configure. That infrastructure attempt failed before compilation and was rerun on ext4; no other worker's evidence or Qt SDK was deleted.

No game/engine source, serializer or player-data format changes. Production links the unchanged preset helper; differential tests link the unchanged full settings parser with only a no-op logging shim. Native Windows execution/package checks and physical GPU/VR/gameplay validation were not run by this slice. They remain package/environment gates, not inferred successes. Existing source snapshot and immediate precommit rechecks reduce conflicts but cannot provide compare-and-swap against an uncooperative external writer.
