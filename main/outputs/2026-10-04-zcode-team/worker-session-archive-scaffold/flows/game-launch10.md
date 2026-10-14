# FLOW ENTRY — game-launch10 (expected in the return session)

Second expected launch window (contingency / second window after launch 9 —
e.g. for the SG stability window, a retry after a failed launch 9, or the
fresh re-proof window; each window is its OWN flow run with fresh evidence;
never re-rate a stale window). Archived under the game-evidence law of
`../worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md` §2–§3.

Run dir: `RUNS/<YYYYMMDD-HHMMSS>-game-launch10/` — stamped fresh by
`../session_run.sh start --flow game-launch10 --run-root <canonical RUNS>`.

## Expected-artifact checklist (identical to game-launch9 / game-evidence law)

- [ ] `S-runtime-status.txt` — S sink copy
- [ ] `UE4SS.log` — L sink copy
- [ ] D1 aligned-entry log copy (`*aligned*`)
- [ ] D2 observer log copy (`*observer*`)
- [ ] C2 / igf facts jsonl (`*igf*`)
- [ ] probe verdict sinks (`*xwp*` jsonl, `*fc1*` jsonl)
- [ ] `probe-verdicts.json` — ge.3b freshness gate
- [ ] `cp0-cp4-verdict.json` / `cp5-verdict.json` / `cp6-verdict.json`
- [ ] `rung_check-RUNG1.json` / `rung_check-RUNG2.json`
- [ ] post_launch_check output (`*post_launch*`)
- [ ] `game-evidence-receipt.json` — incl. the two eye confirmations
- [ ] `stability-gate-verdict.md` + `spawn-count-table.csv` (SG window)
- [ ] `events.log` + `receipt.json` (archive-law minimum)

## Session usage

    ./session_run.sh start --flow game-launch10 --run-root <RUNS>
    ./session_run.sh capture --run-dir <RD> --src ...
    ./session_run.sh close   --run-dir <RD> --verdict PASS|FAIL|INCONCLUSIVE|ABORTED

Honesty: a second window EXTENDS the record, never replaces launch 9's evidence;
if launch 10 is never needed, this entry simply stays un-stamped (a flow entry
that is NOT-ATTEMPTED is recorded as such — INCOMPLETE is a legitimate terminal
state and is never back-filled from host evidence).
