# REPORT — worker-session-handoff-readme (2026-10-04)

## Deliverable
`/WORKSPACE/.codex/worktrees/translation-core/what/README-RETURN.md` — the single return-handoff README at the workspace root, 39 lines (limit: 60).

## Contents delivered (task items a-f)
- (a) One-line status: both candidates built and gate-approved locally; console homebrew servers DOWN at close; PS5 slot still holds r11; first act = reachability check, then staging. (Quoted from CURRENT-STATE-DIGEST-FINAL.md "THE status, one line".)
- (b) Reading order with exact paths: digest-final (`worker-index-final-edition/CURRENT-STATE-DIGEST-FINAL.md`) -> cheat-sheet rev 2026-10-04c (`2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md`, header confirms `rev 2026-10-04c`) -> EVE-STATE (`worker-session-eve-coherence/EVE-STATE.md`) -> the two matrices (`worker-launch10-matrix-prep/LAUNCH10-MATRIX.md`, `worker-launch11-integration-matrix/LAUNCH11-MATRIX.md`).
- (c) First five commands verbatim, quoted from EVE-STATE §"First five commands at session open" (curl reachability 8084, ploader.ini pre-image sha gate, `comp6_one_pass.py`, `console_batch.py dll8`, `console_batch.py e2e`), with the game-CLOSED constraint and the never-9223 rule.
- (d) User's four game actions (cheat-sheet Phase 2 + USER-LAUNCH-GUIDE): arm ploader; launch Townfall (expect exactly 8 OK lines, zero FAIL); load a save (no OPTIONS yet); press OPTIONS once only after CP0-CP4 green. Includes the one-attempt-per-launch rule.
- (e) Do-not list, session-earned: no re-dispatch on ambiguity (STOP + preserve + reconcile; the down-console window produced zero dispatches); no upload before gates (reachability + pre-image sha gate first; armed `d8cb4a76…` only per LAUNCH10-MATRIX row); freshness gates (re-verify identities at open; 12/12 hashes are a 2026-10-04 recomputation, CP2 tick `0x33ac8c0` not `0x37ac8c0`, byte-exact readbacks); no assumed tests (launch-9 loud stop is a prediction; Item Spawner activation never observed in-game).
- (f) Pointers: knowledge base `worker-knowledge-consolidation/KNOWLEDGE-BASE.md` ("READ THIS FIRST"); restore paths from the cheat-sheet Rollbacks section (`rollback-ridealongs` for ini, v2 freeze pair for settings, `C/console/composition4-console-backup.prx` r11 chain for the comp6 slot).

## Path verification
All 14 cited paths were tested for existence on disk before writing and re-tested after writing from the workspace root: 14/14 exist, zero MISS. The list is recorded in CLAIM.json `paths_cited_and_verified_on_disk`.

## Choices made
- "First five commands" taken verbatim from EVE-STATE (the digest's 6-line list is a summary; EVE-STATE is cited by the digest as the source of "the exact first-five commands"). The post-five steps (`stage-ridealongs`, `stage-settings`, `game-evidence`) are named in one pointer line, not counted as commands.
- "Cheat-sheet rev c" matched to the on-disk header `rev 2026-10-04c` of RETURN-SESSION-COMMANDS.md.
- "The two matrices" = LAUNCH10-MATRIX.md (launch-10 router, referenced by digest and cheat sheet) + LAUNCH11-MATRIX.md (post-launch-10 integration router), in that order.

## Honesty
Documentation-only synthesis of existing worker docs; no console contact, no nested subagents, nothing executed against the console or game. All hashes/identities are quoted from the cited documents, not independently re-verified against the console. No completion or >8-quality claim.
