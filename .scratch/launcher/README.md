# Launcher planning archive

The [decision map](map.md) and all ten linked decision tickets are resolved. This branch contains planning and inspection evidence, not production launcher implementation. The next step is `$to-spec` using the map, followed by `$to-tickets`.

## Primary sources

- [Completed decision map](map.md) and [domain glossary](../../GLOSSARY.md).
- [Accepted visual/controller prototypes](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/README.md), preserved separately on `prototype/launcher-ui-accepted` at `a24c10d89a1ad77f92698db19b510ed9635c9380` and tag `archive/launcher-prototypes-2026-10-17`. Download the self-contained controller HTML from that archive or check out the prototype branch to run both studies. Do not merge its throwaway HTML as production code.
- [Native Linux VR investigation](https://github.com/spitefulFr0g/pt-pc/blob/779ab50a40d05fa9fb57a401d6c55cbd8dc216a1/.scratch/launcher/research/native-linux-vr.md), retained on `research/launcher-linux-vr`. Its proposals remain constrained by the final unchanged-game boundary and later acceptance decisions.

The planning snapshot is retained by tag `archive/launcher-planning-2026-10-17`. Tags preserve the work independently of any later deletion of working branches or worktrees.

## Resume constraints

Read the final `Answer` sections of the decisions, including later cross-references, before acting. Historical comments retain earlier proposals that later user instructions superseded. Keep the game/engine and player-data formats compatible with upstream. All required tests are automated and agent-run; there is no required human, physical Deck/controller or headset test. No native launcher implementation, build, packaged release or hardware validation has been completed. Browser prototype checks are interaction evidence only.

Use only models authenticated through the user's subscriptions. API-priced models/providers require explicit authorization for that work. Verify the billing route before delegation; if verification or subscription capacity fails, wait/report the blocker without API fallback. For codex-implement, enforce its billing guard before resolving or launching workers. Preserve this requirement in every handoff.

The user authorized committing/pushing this archive first. Branch/worktree cleanup requires their later explicit go-ahead; publishing this snapshot does not authorize cleanup or merging.
