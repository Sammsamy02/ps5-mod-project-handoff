# REPORT — worker-evidence-pipeline-rehearsal

Date: 2026-10-04. Status: COMPLETE (host rehearsal only; zero console contact).

## What was done

1. **Synthesized five complete fake session evidence trees** under
   `fixtures/s01..s05/` (each: `from-dir/` flat files consumed by the tools'
   `--from-dir` mode + `console-tree/data/…` mirror of the console paths;
   `data/plugins/UE4SS.log`, `data/modbridge/roots/PPSA33286/runtime-status.txt`,
   `data/townfall_runtime_aligned_entry.log`,
   `data/townfall_runtime_aligned_observer.log`,
   `data/modbridge-ingame-facts.jsonl`). Content is fabricated but the line
   shapes are the real ones: the 3-line baseline verbatim from
   post_launch_check's embedded baseline, the runbook §1 grammars, the exact
   banner/tick/mod/menu/spawn strings from console_batch's GAME dict and the
   runbook, the launch-8 ledger lease signature ("Owned read range includes
   unregistered bytes", bridge code=2), the ENGINE-READY-PATH "PS scan timed
   out" → `failed(1,<msg>)` shape, and the rung1-pass fixture NQH1 record
   layout. Scenarios: (a) full success through CP7, (b) configured-roots
   conversion success then PS-scan timeout (outcome #8), (c) lease-gate refusal,
   (d) mods-started + main.lua script error (the status-stays-mods-started
   trap), (e) stale launch (repeat oracle probe lines only, prior-session
   everything).
2. **Ran the full chain per fixture**: post_launch_check `--from-dir` (plus the
   `--baseline-file` variant on all five, plus `--json`), rung_check
   `--from-dir` (plus `--discover` on the C2-absent tree and `--rung RUNG2` on
   the full-success tree), and the runbook CP0-CP7 checks via console_batch.py's
   own unmodified classifier functions (`cp_parse_status_s`, `classify_l_log`,
   `classify_d1_log`, `cp01234_verdict`, `cp56_verdict`), including the CP5/CP6
   L-diff rehearsal on staged t1→t2→t3 re-fetch slices. 17 raw tool outputs are
   preserved under `runs/<scenario>/`; combined machine summary in
   `rehearsal-results.json`.
3. **Verified discrimination and verdict routing** (REHEARSAL-RECEIPTS.md §1).

## Result: the chain DISTINGUISHES all five scenarios — with one real tool bug found

- s01 full success: `MODS_STARTED` / RUNG1+RUNG2 PASS (exit 0) / CP0-CP6 PASS /
  CP7 delegated PASS. Unique all-green signature.
- s02 PS-scan timeout: `FAILED_STATUS` (silent init-thread death: status failed,
  NO L failure line) / rungs honestly CANNOT_ASSERT / CP2-CP4 FAIL. Distinct
  from s03 exactly as the verdict precedence intends.
- s03 lease-gate refusal: `FAILED_runtime_start_failed` (L failure line present,
  sinks agree) / CP0-CP4 FAIL / `--discover` correctly lists `/data` candidates
  with C2 absent.
- s04 script-error trap: post_launch stays `MODS_STARTED` **by design** (status
  ladder is truth); the trap is caught by the chain — CP4 FAIL surfaces the
  exact `Error loading script:` line, RUNG4 count 0, "0 found" in NEXT text.
- s05 stale launch: post_launch `NOT_STARTED` with 3 probe repeats correctly
  never counted as new — **but the console_batch CP classifier false-PASSES
  CP0/CP3 and PARTIALs CP2 from stale prior-session evidence** (tool bug, below).

## Tool gaps found (documented only; no other worker's tool was edited)

1. **BUG — console_batch.py `cp01234_verdict` has no freshness/delta gate**: on
   the stale tree it returns CP0=PASS/CP3=PASS from prior-session D1/status,
   contradicting post_launch_check's `NOT_STARTED` and runbook O1/O2; on console
   this could green-light the CP5 pad gate after a launch that never ran the
   PRX. Fix: require the pre-launch D1 `module_start_entered` delta ≥ 1 (CP0)
   and session-ms freshness (CP1/CP3). Related label-only bug: CP0 fallback
   mislabels an s03-shaped bridge failure as "F1 probe lifecycle".
2. **post_launch_check.py `stage_hint`** maps status-failed code∈{1..4} to
   BRIDGE_GUIDANCE, so outcome #8 ("PS scan timed out", conversion already
   succeeded) reads as "bridge stage 1 CONVERSION" — hypothesis-level mislabel;
   verdict + "report detail verbatim" still route correctly.
3. **post_launch_check.py `BRIDGE_GUIDANCE[2]`** predates the launch-8 lease
   discovery: code=2 with "unregistered bytes" detail gets "verify the PRX path"
   advice that would send the controller to re-stage a good PRX.
4. **rung_check.py has no O2 freshness gate (DEMONSTRATED false-PASS)**:
   `status_engine_ready_or_later` passes on a stale session. With the derived
   probe tree `fixtures/probe_gap4_stale_driver/` (stale status + stale NQH1
   records with advancing ticks), `rung_check --rung RUNG1` returns overall
   `VERDICT: PASS`, exit 0, on evidence from a session that never happened
   (raw output preserved in `runs/`).
5. **Designed handoff (not a bug)**: s04 proves post_launch_check alone cannot
   name F9 — the CP4 check is mandatory before any rung-4 claim.
6. **Runner note**: console_batch's `game-evidence` flow on disk matched its
   documentation everywhere exercised (Step A/B commands, CP classifier, CP5/CP6
   diff logic); the interactive runner itself was not executed (live FTP + user
   gates), so no runner-level delivery gap beyond the label wording above.

Each gap carries a proposed one-line fix in REHEARSAL-RECEIPTS.md §2; the
verdict table, the confirmed real-session command sequence (§3), and the
real-session expectations (UNVERIFIED_DRIVER honesty; CP4 mandatory; the two
known failure-detail signatures) are in the same file.

## Honesty limits

- Host rehearsal on fabricated trees: validates plumbing and verdict routing
  only. No rung advanced, no console fact established, nothing claimed >host-test.
- s01's C2 stages a driver add-on record set that does NOT exist on console
  today (gap G2) purely to exercise RUNG1/RUNG2 PASS routing; the real-session
  expectation remains UNVERIFIED_DRIVER.
- D1 `module_start_return` on the s03 failure path was left absent (abnormal
  return shape unobserved in any real artifact); this only affects the
  (already-mislabeled) CP0 FAIL cell, not the tool verdicts.
- Two bugs in MY OWN first-draft runner (wrong dict stored for CP5/CP6;
  --baseline-file variant run on dirs lacking baseline.txt) were found by the
  rehearsal itself and fixed in my dir before final runs; final results above
  are from the fixed runner.

## Deliverables

- `CLAIM.json` (first), `fixtures/` (5 scenario trees × from-dir + console-tree,
  plus the derived `probe_gap4_stale_driver/` proof tree),
  `build_fixtures.py`, `run_rehearsal.py`, `runs/` (18 raw outputs),
  `rehearsal-results.json`, `REHEARSAL-RECEIPTS.md`, this `REPORT.md` (last).
  All writes confined to
  `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-evidence-pipeline-rehearsal/`.
