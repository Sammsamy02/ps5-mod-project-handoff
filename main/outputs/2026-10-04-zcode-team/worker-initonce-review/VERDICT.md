# VERDICT — worker-initonce-review (2026-10-04)

## PASS — all-clear; no actionable mismatches

Independent evidence-based review of the controller's InitOnce integration
(build 1a6b37b7). Every check was verified by direct file reads with line
citations; no claim below rests on the controller's own summaries.

## Check results

| # | Check | Result |
|---|-------|--------|
| 1 | Opt-in isolation with MB_OWNED_INIT_ONCE=0 | PASS |
| 2 | Pattern fidelity vs the sync slice (ordering, stages 37/38) | PASS |
| 3 | Fixture semantics: documented behavior only, no adapter-policy pins, LastError contract | PASS |
| 4 | Builder invariants: census 2 exports, 46/10 vs 44/10 formulas, guard chain, on/off assertions | PASS |
| 5 | Console evidence consistent with fixture assertions and receipt; scope not overstated | PASS |
| 6 | Promoted files byte-identical to worker-4 originals except the one expected include fix | PASS |

## Key evidence

- All 8 `#if MB_OWNED_INIT_ONCE` pairs in owned_runtime.c and all 6 in
  tests/worker-fixture.c are balanced; the two unguarded edits (report field
  write at owned_runtime.c:212 and the `+0` import terms) are provably inert
  with the flag off.
- Session begin/end is exact reverse (init_once begins last, ends first);
  owner close order srw(34)->aw(35)->critical(36)->init_once(38) matches the
  stated design "init_once closes AFTER sync owners" and is dependency-safe
  (only the domain is pinned; it closes at stage 16 afterwards).
- fail(37)/fail(38) appear nowhere else in the candidate tree.
- worker-initonce.h pins only documented codes 997/87; duplicate-complete is
  asserted as failure with no error-code pin; the finish hook runs after the
  -18 GetLastError check and after sync_finish (exact reverse of prepare).
- Census 'mb_win_init_once_abi':2 = two WIN64 definitions = def file's two
  exports; console log directly evidences 46/10 (init-once) vs 44/10
  (6d295f56 sync-only).
- Result-copy sha256 matches the receipt; final record fields match the
  fixture's assertions and the receipt's final_record; scope explicitly
  standalone-diagnostic.

## Non-defect observations (recorded, no action required)

1. The console result copy holds 16 records (4 builds x 4); the briefing's
   "4 records" refers to the 1a6b37b7 slice (final four). The receipt's
   result_copy_sha256 is for the whole accumulated file and matches it.
2. worker-fixture.c CreateThread-failure path (104-108) finishes sync but not
   init-once. Harmless: once_b is never begun if the thread never started,
   once_a's sidecar is COMPLETE, and its re-zeroed storage is never touched
   again, so owner close still succeeds. Unreachable in asserted profiles.
3. Owner-level close is not strict LIFO across all service owners (init_once
   opened last, closes last among them) - this is the specified design, and
   the sync triple itself remains strict LIFO.

Raw detail: findings.json and REPORT.md in this directory.
