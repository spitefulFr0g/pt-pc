# Ticket #8 graphics preset checks

Run from the repository root (the evidence destination should have ample space):

```sh
mkdir -p /home/watermelon/.cache/pt-issue8/tmp
TMPDIR=/home/watermelon/.cache/pt-issue8/tmp \
  launcher/run-issue8-graphics-container.sh /home/watermelon/.cache/pt-issue8/verified
```

The wrapper runs the complete #7 disposable-bundle Qt 6.12/container CTest suite, returns its failure status, and saves build/test output, image identity, source commit, result and hashes even on failure. Requirements are the existing Docker image and unchanged Qt SDK described by the #7 harness. No game files, renderer, device, sign-in or release secrets are required.

Public Settings operations cover all four deliberate presets with unknown, unsupported and supported RT capability; their full effective graphics/ray-tracing fields must equal unchanged upstream application. Unknown capability never enables RT. Tests cover legacy ray-quality migration (2 to 3; 5 to 2), clamps, anisotropy buckets, invalid/duplicate numbers, finite fallback, uppercase booleans, quoted tokens and compatibility defaults for missing lens-ghost/deband keys.

The oracle is the actual unchanged `src/engine/platform/settings.cpp` (`LoadAppSettings` and `SaveAppSettings`) plus `graphics_presets.cpp` (`ApplyGraphicsPreset` and `DetectGraphicsPreset`), linked only into the operations test. The sole link shim discards logging through `LogWrite`; platform path conversion is upstream's inline function. No parser/preset/renderer mock is used. Production retains a bounded byte adapter faithful to the pinned parser and links only the unchanged preset helper. No game/engine source changes.

Additional operation checks cover Custom-to-Custom conflicts without writes, invalid enum rejection, complete keyset writes, unrelated/unknown content, duplicate-key semantics, mixed newlines, platform separation, the shared save-directory enhanced-cache sentinel, unknown-runtime replacement and running/uncertain activity gates. Existing #7 containment, size, no-op, conflict, permission and atomic-save checks remain active.

Real Qt controls cover Low/Original/High/Ultra selection, disabled read-only Custom, explicit current-preset activation, reload/discard, conflict reconciliation retaining a dirty preset and refreshing clean display fields, and Custom readback after game exit. A boolean's inline note moves to a standalone comment when that key is deliberately patched: upstream boolean parsing does not accept an inline comment after a true token. Unedited lines retain their original bytes/endings.

See [repair evidence](evidence/issue8-r2/README.md) for exact red/green results and limits. These are source-build Qt offscreen event-loop and controlled-process checks on Linux. Native Windows/package deployment, physical display/RT hardware and gameplay are not established here. Uncooperative writers can still race the final recheck/replacement; this is not filesystem compare-and-swap.
