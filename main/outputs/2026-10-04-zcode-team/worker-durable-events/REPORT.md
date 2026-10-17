# REPORT — worker-durable-events

Date: 2026-10-05 · Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what` ·
LOCAL ONLY; no console contact, no FTP, no edits outside this worker directory.
`evidence.sh` (owner: worker-evidence-gap-fix-2, in flight) was READ for vocabulary only,
never edited; the canonical copy read is
`outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh` (its header
documents the gap-fix-2 revision contract).

## What was delivered

1. **`tools/epoch_view.py`** — the complementary per-epoch evidence reader. Given the
   accumulated supplier-diag jsonl (+ optionally the UE4SS mirror and the runtime-status
   file), it emits ONE table row per GAME EPOCH (pid) with: build_id (only via `--build-id`;
   the sdiag grammar carries none — verified), session (the status session row, applied to
   its pid alone), first/last t_ms + span, the first-seen stage chain in BOTH the coarse
   evidence.sh tag vocabulary (ENTRY/ACT/VMMAP/lease/anchors/conversion/cfg/START/pad/
   harness/STOP) and the exact evidence.sh S-stage names, the terminal event (refusal text
   + int32 code + verbatim r4 mirror line, with 0x-hex pad error decode), the predicted
   pair (anchor_count / intervals_found / lease_spans / lease_reg / extent / cfg / spanpub),
   E4-activated state (literal only via the LATEST-STATE-ONLY session match, else
   evidence-equivalence with the caveat), buffered-Lua harness green, and the verdict class
   in the oracle vocabulary (E2 START / pad phase-6 / B7 MFA / engine-ready / mods-started
   plus D6/D7/C12/E1/STARTED) with the MINIMAL-ACCEPT-grade quartet qualifier. `--json`
   emits the full machine document; `--baseline-bytes` implements the byte-exact append cut
   as a per-epoch NEW label (history always shown — the accumulated file IS the evidence).
   Robustness: unparseable lines skipped and counted; pid reuse across boots -> occurrence
   #k; t_ms going backwards -> per-epoch BOOT-GAP note (console reboot between epochs);
   truncated epoch -> INCOMPLETE_WINDOW, never guessed; missing file -> exit 2; empty input
   -> NO-EPOCHS.

2. **LATEST-STATE-ONLY discipline (the user directive, in code):** the status file is read
   as one session row + last state and applied ONLY to the epoch with the matching pid —
   never as history, never raced. The tool prints the directive banner and, when the status
   session pid has no epoch in the file, says so explicitly instead of guessing.

3. **`VALIDATION.txt`** — the 4-epoch table from the REAL accumulated file
   (`launch10-prep/sdiag-baseline-run5.jsonl`, 2139 records) plus the requirement and
   cross-check ledger. Headlines:
   - ALL FOUR epochs recovered: pid 317 (run1, E2 START, "PS5Pad.ini must be ASCII…"),
     pid 341 (run2, B7 MFA, code −8/0xfffffff8, zero bridge tags), pid 355 (run3, pad
     phase-6, code=3, error 0x80920004), pid 370 (run4, pad phase-6 REPRO — the run-4
     outcome; status session 370 corroborates as its only legal application).
   - Predicted pair on the conversion-passing epochs: 7/1/6 with reg=6 and extent
     [0x400000,0xc0602c8) on pid 317 AND pid 355 (the two passes LAUNCH10-RESULTS §3
     confirmed) — and pid 370 adds a third identical quartet (honest extension of the
     run-4 repro; spanpub totals 119/127/123 differ per boot, cap=16 saturation on all).
   - Cross-checks: run1/run3/run4 per-run files are value-identical slices (programmatically
     asserted, PASS); the byte cut at run3's size marks exactly pid 370 NEW; r4-vs-sdiag
     t_ms agree exactly for 317/341/355 and 1 ms apart for 370 (recorded); LAUNCH10-RESULTS
     values (vmentry 22761/10028, spanpub 119/127, mfa_activated 36564/36566, stat_ownmod=1
     with exepath_source=0) all re-verified from the table.

4. **`PROCEDURE.md`** — the one-page RUN-N reading procedure: pin the baseline cut ->
   fetch once (LAUNCH-DAY-10 §3 order, no status race) -> read the DURABLE per-epoch
   evidence FIRST with epoch_view -> evidence.sh one-pager as the drift guard/cross-check
   -> controller declares the rung -> pin the next baseline cut + save `epochs.json`.

## Key findings while validating

- The accumulated file is byte-identical to run4-sdiag.jsonl; the run-5 baseline cut
  (318910 B) is therefore already pinned, and the pid-370 records begin exactly at the
  run3 file's byte end (237211) — the cut is a byte-exact epoch discriminator.
- pid 341 (B7) has NO bridge/conversion tags at all — the per-pid split makes the B7
  "nothing downstream ran" clause visible without any aggregation, which the old
  whole-file SDIAGSUM could silently blur (LAUNCH10-RESULTS G2).
- The run-4 sdiag `runtime_start_failed` t_ms (62842696) vs its r4 mirror line (62842697)
  differ by 1 ms — runs 1-3 agreed exactly; recorded as observed writer skew, no impact.
- `bridge_spanpub_env_low/high` are a 64-bit split (3997696/17 for pid 370), consistent
  with the G9 note; carried in the JSON per epoch.

## Honest bounds

- build_id is not derivable from the sinks; `--build-id` must be pinned by the operator
  from the staging receipt. No build identity is asserted for the four epochs beyond the
  controller's records.
- No verdict above "MINIMAL-ACCEPT-grade conversion (quartet met, no D6)" is claimed or
  automatable: E4 `activated` is not literally on file for ANY epoch (terminal-write
  O_TRUNC), and rung declarations remain the controller's act per ORACLE §2.
- The tool never writes outside `--out`; no network; standard library only (posix python3).
- evidence.sh compatibility is by vocabulary, not by invocation: the two tools are
  complementary scopes (window one-pager vs per-epoch durable history) and are both run
  in PROCEDURE.md steps 2-3.
