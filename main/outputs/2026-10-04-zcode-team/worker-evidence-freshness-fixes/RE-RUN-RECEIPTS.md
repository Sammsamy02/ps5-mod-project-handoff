# RE-RUN-RECEIPTS — evidence-pipeline re-rehearsal after freshness fixes

worker-evidence-freshness-fixes, 2026-10-04. Host/fixture mode ONLY (no console
contact). Fixtures consumed unmodified from worker-evidence-pipeline-rehearsal/
fixtures/. Raw outputs: runs/<scenario>/; machine summary: rerehearsal-results.json
(runner: run_rerehearsal.py — the original rehearsal worker's artifacts were not
overwritten). BEFORE evidence of both false-PASS bugs: runs/BEFORE_*.

## Fixed tools at run time
- worker-rung-evidence-reader/rung_check.py sha256 287a26844ad2… (GAP 4 O2 gate)
- worker-post-launch-tools/post_launch_check.py sha256 cc75d00788b9… (GAP 2+3)
- worker-console-batch-runner/console_batch.py sha256 92b56889cbfe… (GAP 1a+1b
  via diffs/console_batch.freshness-gate.patch applied IN PLACE after the owner
  worker-batch-runner-8fam-patch delivered their terminal REPORT.md; as-found
  snapshot c691cdd4cb8b and their delivered bytes 8c3031ebf34c preserved under
  diffs/; owner selftest after patch: 162 ok, 0 failed)

## Per-scenario verdict table (after fixes)

| Check | s01 full success | s02 PS-scan timeout | s03 lease-gate refusal | s04 script-error trap | s05 stale launch | probe_gap4_stale_driver |
|---|---|---|---|---|---|---|
| post_launch_check (exit) | MODS_STARTED (0) | FAILED_STATUS (0) | FAILED_runtime_start_failed (0) | MODS_STARTED (0) | NOT_STARTED (0) | NOT_STARTED (0) |
| plc guidance | rung-4 print check, 4 hits | status-is-truth + **init_thread_failed hint** (GAP 2 fixed; was CONVERSION vm-scan) | bridge stage 2 CONTEXT **+ r11 lease-gate text** (GAP 3 fixed) | MODS_STARTED text, 0 hits (hook intact) | stale/mid-launch or F0 | stale/mid-launch or F0 |
| probe repeats counted | 0 | 0 | 0 | 0 | 3 (never "new") | 0 |
| rung_check (exit / overall) | PASS (0) | CANNOT_ASSERT (3) | CANNOT_ASSERT (3) | CANNOT_ASSERT (3) | **INCONCLUSIVE-STALE (5)** | **INCONCLUSIVE-STALE (5)** |
| RUNG1 / RUNG2 | PASS / PASS | UNVERIFIED / CANNOT_ASSERT | UNVERIFIED / CANNOT_ASSERT | UNVERIFIED / CANNOT_ASSERT | **INCONCLUSIVE-STALE** / CANNOT_ASSERT | **INCONCLUSIVE-STALE** / FAIL |
| status_session_fresh cond | PASS (9435000) | PASS (9512000) | PASS (9534000) | PASS (9577000) | **FAIL (8812000 <= 8866785)** | **FAIL (8812000)** |
| CP0 (patched in-tree) | PASS | PASS (conversion really succeeded) | **FAIL(F3/F4/F5 runtime_start_failed: ['00000002'])** (GAP 1b fixed; was F1) | PASS | **INCONCLUSIVE-STALE** (was PASS — GAP 1a fixed) | n/a (not run for probe) |
| CP1 / CP2 | PASS / PASS | PASS / FAIL(no engine-ready) | FAIL(entry line) / FAIL | PASS / PASS | **INCONCLUSIVE-STALE / INCONCLUSIVE-STALE** (was PASS/PARTIAL) | n/a |
| CP3 / CP4 | PASS / PASS | FAIL / FAIL(F9) | FAIL / FAIL(F9) | PASS / **FAIL(F9: exact Lua error surfaced — the trap)** | **INCONCLUSIVE-STALE** / FAIL(F9) | n/a |
| CP overall | PASS → "gate CP5: user must load a save and press OPTIONS once" | INCOMPLETE | INCOMPLETE | INCOMPLETE (trap caught at CP4) | INCOMPLETE + stale hint, **zero PASS/PARTIAL cells** | n/a |
| --baseline-file variant | same verdict | same | same | same | same | — |
| CP7 (--rung RUNG2, s01) | PASS (0) | — | — | — | — | — |
| --discover (s03) | — | — | exit 0; /data candidates listed; C2 correctly absent | — | — | — |
| CP5/CP6 L-diff (s01 t1→t2→t3) | PASS(1 new pickups line + Menu created.) / PASS(Spawned Antibiotics x1 ID=DeprecatedItem) | — | — | — | — | — |

In-tree patched console_batch output agrees byte-for-byte with the
shadow/console_batch.py reference copy in every cell ("agree with shadow: True"
for all scenarios in rerehearsal-results.json).

## The two false-PASS bugs: before → after
- GAP 4 (rung_check stale RUNG1 PASS): runs/BEFORE_probe_gap4_stale_driver.RUNG1.txt
  = "VERDICT: PASS", exit 0 on session ms 8812000 evidence with advancing NQH1
  ticks 5→7. AFTER: exit 5, "VERDICT: INCONCLUSIVE-STALE", condition
  status_session_fresh FAIL with "session ms 8812000 <= threshold 8866785:
  PRIOR-launch evidence"; s01 still PASS exit 0
  (runs/AFTER_s01.rung_check.txt). Exit code 5 is distinct from
  PASS 0 / FAIL-UNVERIFIED 1 / usage 2 / CANNOT_ASSERT 3 / FETCH_FAILED 4.
- GAP 1a (cp01234_verdict stale PASS cells): before, s05 yielded CP0=PASS +
  CP3=PASS + CP2=PARTIAL contradicting post_launch_check NOT_STARTED. AFTER
  (in-tree): CP0/CP1/CP2/CP3 = INCONCLUSIVE-STALE, overall INCOMPLETE with the
  stale hint; no PASS/PARTIAL cell survives on stale evidence. The O1 delta
  guard also holds: s01 with d1_entered_before=1 (no NEW entry this launch)
  → "FAIL(F0 no NEW module_start_entered this launch …)".

## Discrimination: PASS (preserved and sharpened)
All five trees still produce distinct signatures: s01 all-green; s02
FAILED_STATUS/silent-init-death (CP0 PASS — conversion really succeeded — with
the corrected init-thread guidance) vs s03 FAILED_runtime_start_failed (CP0 now
correctly F3/F4/F5); s04 shares s01's MODS_STARTED by design and is still caught
at CP4 (exact Lua error) + RUNG4 count 0; s05 is NOT_STARTED + 3 probe repeats
and is now refused at BOTH the rung layer (exit 5 INCONCLUSIVE-STALE) and the CP
layer (all stale cells INCONCLUSIVE-STALE) instead of false-PASSing.

## Honesty
Fixture-mode host verification only. Nothing here advances a rung or certifies
the console (runbook do-not-claim guard). The freshness threshold is the same
shared oracle as post_launch_check.py (MONO_NS_AT_PATH_TEST // 1e6 = 8866785 ms);
s01-s04 session ms values (9435000/9512000/9534000/9577000) are all above it, so
no fresh-scenario verdict changed.
