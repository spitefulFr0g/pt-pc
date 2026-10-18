# Backend review: issue #32

Reviewed integration tip `abe6cae1edf90b601d0aeca855c190866d26b7c3` against recorded base `1c2f11578ecfa38abfa41983bd851fda552cd4bb`. Read current issue #32, REQUEST, SPEC, RELEASE-CONTRACT, TASKS, ACCEPTANCE, current RUNS receipts, reports and cleanup audit. No frontend review, publication, deletion or source repair performed.

## Findings

1. **Blocking finalization: acceptance gate ordering is unsatisfiable.** RELEASE-CONTRACT requires open-PR finish COMPLETE before merging, then stable rebuild/publication and cleanup. ACCEPTANCE marks `native-bundles`, `release`, and `cleanup` required now. `finish.py:84` calls `check_acceptance` without phase/defer support. Public merged-main release and post-release cleanup cannot honestly pass before that finish. Dispatcher must establish an explicit supported staged finalization procedure preserving required operational obligations; do not mark checks optional/passed merely to bypass this. This review does not authorize a helper bypass.
2. **Required evidence refresh:** acceptance checker reports `readme: relevant source changed` and `audit: relevant source changed`. README evidence predates the workflow/package changes it scopes; audit scopes all source but was recorded at base. Rerun against final integrated source and record genuine reports before finalization. Cleanup audit additionally remains a snapshot requiring live finalizer revalidation.
3. **Existing nonblocking defect:** `qt-deprecated-set-active-window` remains open in DEFECTS.md. Deprecated test API warnings do not invalidate successful tests; no unrelated source change made.

## Release implementation and compatibility

No actionable release-source defect found. Shared identity validates source base 0.1.0, stable v0.1.0 and existing preview namespace; filenames, archive roots, inventory channel/tag, installed version and draft prerelease flag agree. Packaging checks the installed version and runtime hash, creates matching internal/external inventories and archive checksum. Smoke validates checksum, exact inventory file set/hashes, runtime identity, archive/root identity and startup with cleaned environment. Five focused regression tests passed, including synthetic stable/preview ZIP creation; deployment is mocked in that test and it is not native bundle evidence.

The existing Windows/Linux matrix and dependency pins remain. Draft creation depends on successful native build/smoke jobs and explicit manual create_draft. Only macos.yml subscribes to published releases; both jobs exclude launcher-v* and exact v0.1.0, and manual macOS upload rejects these. Guards intentionally cover this first stable release, not future stable versions.

README matches runtime labels, entry points, region recognition, preparation/adoption, preservation and both launch modes' --no-update-check argument. Upstream credit/pin, user-supplied game data, unsigned/manual updates and gameplay/headset/compositor limits are explicit. No engine behavior was changed.

## Coverage, receipts and cleanup audit

All material request obligations have required manifest entries: workflow identity/guards, README, read-only audit, backend coverage, native bundles, publication and actual cleanup. No hardware verification is falsely required or claimed. Configured full checks remain separate from native package checks. The unresolved gate-order problem prevents a completion recommendation.

Current RUNS contains real taskId/clientRequestId and subscription billing records for each current task and this review; no old receipt was reconstructed. Audit records 35 worktrees, 50 tracking/local refs, six live remote branches, issue/PR state, tracked diffs, unique commits and file preservation proposals. It conservatively preserves ambiguous/unfinished work, stashes and active paths, distinguishes archived artifacts from patch equivalence, and exposes the unreadable root core file. It is suitable as a candidate inventory, not deletion proof. Recheck new review/child paths, current heads, terminal ownership, ignored files and saved hashes before any finalizer deletion; root stays present and must eventually be on main.

Evidence and detailed current-check results: notes `logs/review-backend.json` and `logs/review-backend-full.log`. Native stable Windows/Linux packaging, gameplay/headset presentation, actual release and cleanup were not performed by this reviewer.
