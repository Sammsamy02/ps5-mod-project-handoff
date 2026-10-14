# REPORT — worker-evidence-freshness-fixes (terminal)

2026-10-04. All work host/fixture-mode; NO console contact; no nested subagents.
CLAIM.json first, REPORT.md last, per dispatch.

## What was done
1. **rung_check.py (worker-rung-evidence-reader/, no active owner) — GAP 4 O2
   freshness gate, fixed in place.**
   Added `FRESH_SESSION_MS_THRESHOLD = 8866785` (same oracle/constant as
   post_launch_check.py), `status_session_fresh()`, a `status_session_fresh`
   condition in RUNG1, and an `INCONCLUSIVE-STALE` verdict with a NEW exit code 5
   (distinct from PASS 0 / FAIL-UNVERIFIED 1 / usage 2 / CANNOT_ASSERT 3 /
   FETCH_FAILED 4). A status file dating the session at/below the threshold — or
   with no datable session line — can never ground a rung PASS.
   Proof: `runs/BEFORE_probe_gap4_stale_driver.RUNG1.txt` (stale NQH1 ticks 5→7,
   VERDICT PASS exit 0 — the documented bug) vs `runs/AFTER_…` (exit 5,
   INCONCLUSIVE-STALE); s01 still PASS exit 0.
2. **post_launch_check.py (worker-post-launch-tools/, no active owner) — its two
   documented fixes, in place.**
   GAP 2: `stage_hint` now returns `init_thread_failed` for init-thread
   signatures (`PS scan timed out` / `AOB scans`) BEFORE the bridge-code lookup,
   with a matching `STAGE_GUIDANCE["init_thread_failed"]` entry (s02 guidance no
   longer sends triage to bridge CONVERSION vm-scan; verdict FAILED_STATUS
   unchanged). GAP 3: `BRIDGE_GUIDANCE[2]` now carries the launch-8 read-lease
   r11 text ("do not re-stage on this signature alone"; s03).
   Scope note: the dispatch's third item ("CP0 fallback mislabel") is GAP 1b,
   which REHEARSAL-RECEIPTS.md locates in console_batch.py's `cp01234_verdict`
   (verified — post_launch_check.py has no F1/CP0 label); it shipped with the
   console_batch fix below.
3. **console_batch.py (worker-console-batch-runner/) — GAP 1a freshness gate +
   GAP 1b CP0 fallback label, ownership-safe.**
   While worker-batch-runner-8fam-patch was mid-flight (file hash moved
   c691cdd4cb8b → 8c3031ebf34c during preparation; receipts empty) I did NOT
   touch it: I snapshotted both states, verified the cp01234 region
   byte-identical across them, built and behavior-tested the fix on a shadow
   copy, and delivered `diffs/console_batch.freshness-gate.patch` +
   `PATCH-BRIEF.md`. The patch: shared freshness constant + `cp_session_fresh()`;
   `cp01234_verdict(..., d1_entered_before=0)` requiring a fresh session AND
   `>=1 NEW module_start_entered` (O1) for CP0, stale CP1-CP3 cells labelled
   `INCONCLUSIVE-STALE` (no PASS/PARTIAL from a prior launch), and
   `FAIL(F3/F4/F5 runtime_start_failed: …)` instead of the F1 mislabel. Overall
   contract (PASS/INCOMPLETE + hint) unchanged; hint is stale-aware.
   The owner then delivered (selftest/TEST-RECEIPT/terminal REPORT.md 22:01-22:04);
   per the dispatch condition I applied the patch IN PLACE (result sha256
   92b56889cbfe…; their dll7/dll8 sections untouched; their selftest after patch:
   162 ok, 0 failed).
4. **Full re-rehearsal against the fixed tools** — `run_rerehearsal.py` re-runs
   the original five-scenario chain + the probe tree, consuming the original
   fixtures unmodified and writing only under this worker's directory (the
   rehearsal worker's artifacts are byte-preserved). Results:
   `RE-RUN-RECEIPTS.md` + `rerehearsal-results.json` + raw `runs/<scenario>/`.
   All five scenarios still discriminate exactly as the original table (s01
   all-green incl. CP5/CP6/CP7; s02 FAILED_STATUS vs s03
   FAILED_runtime_start_failed separated; s04 trap caught at CP4 with the exact
   Lua error + RUNG4 count 0; s05 NOT_STARTED + 3 counted probe repeats), AND
   both false-PASS bugs now refuse: s05/probe are INCONCLUSIVE-STALE exit 5 at
   the rung layer and carry zero PASS/PARTIAL CP cells at the CP layer. In-tree
   patched console_batch agrees with the shadow reference in every cell;
   `--baseline-file` variant same-verdict everywhere; s03 `--discover`, s01 CP7
   RUNG2 delegation unchanged.

## Deliverables (this directory)
CLAIM.json, RE-RUN-RECEIPTS.md, run_rerehearsal.py, rerehearsal-results.json,
PATCH-BRIEF.md, diffs/ (patch + as-found snapshots + .orig of both in-place
tools), shadow/console_batch.py (reference bytes the owner's file was proven
equal to), runs/ (BEFORE/AFTER proofs + per-scenario raw outputs).

## Honest limits
Fixture-mode host verification only; nothing advances a rung or certifies the
console (runbook do-not-claim guard). The freshness gate can only refuse
(never fabricate a PASS); it assumes the status file's session line is the
launch identity — if a future writer changes that grammar, both tools' oracles
need re-derivation. The owner's dll7/dll8 flows were not re-exercised beyond
their selftest; on-target behavior of all three tools remains [TARGET] until the
user-authorized session.
