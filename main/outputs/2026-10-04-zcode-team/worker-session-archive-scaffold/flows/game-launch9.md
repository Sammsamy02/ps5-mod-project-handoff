# FLOW ENTRY — game-launch9 (expected in the return session)

Launch-window label: launches 1–8 are fail-open history (each root-caused one
blocker, fixed once); **launch 9 is the next window** (composition-6 =
configured roots). Archived under the game-evidence law of
`../worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md` §2 (game-evidence run
dir) and §3 (CP0–CP7 + rungs 1–5, fresh-window evidence law: INCONCLUSIVE-STALE
can never PASS).

Run dir: `RUNS/<YYYYMMDD-HHMMSS>-game-launch9/` — stamped fresh by
`../session_run.sh start --flow game-launch9 --run-root <canonical RUNS>`.

## Expected-artifact checklist (advisory; receipt.json is the index)

- [ ] `S-runtime-status.txt` — S sink copy (state lines)
- [ ] `UE4SS.log` — L sink copy
- [ ] D1 aligned-entry log copy (`*aligned*`)
- [ ] D2 observer log copy (`*observer*`)
- [ ] C2 / igf facts jsonl (`*igf*`)
- [ ] probe verdict sink 1 (`*xwp*` jsonl) and sink 2 (`*fc1*` jsonl)
- [ ] `probe-verdicts.json` — ge.3b freshness gate
- [ ] `cp0-cp4-verdict.json` — ge.3 (CP0 composed PRX ran … CP4 Item Spawner loaded)
- [ ] `cp5-verdict.json` — ge.5–ge.7 (menu opens — half the goal; G4 VISIBLE
      overlay + eye confirmation)
- [ ] `cp6-verdict.json` — ge.8–ge.9 (spawn works — half the goal; G5 visible
      pickup + log line + eye confirmation)
- [ ] `rung_check-RUNG1.json` — ge.4/4b strict baseline
- [ ] `rung_check-RUNG2.json` — terminal NQH1 record (optional deeper rung; never blocks)
- [ ] post_launch_check output (`*post_launch*`)
- [ ] `game-evidence-receipt.json` — ge.10, INCLUDING the two eye confirmations
      (menu visible, pickup visible): eyes facts are receipts, not memories
- [ ] `stability-gate-verdict.md` — SG window verdict (extends, never replaces)
- [ ] `spawn-count-table.csv` — press # → item id → frame timestamp, N ≥ 5,
      negative controls
- [ ] `events.log` — append-only step log (archive-law minimum)
- [ ] `receipt.json` — verdict receipt / artifact index (archive-law minimum)

## Session usage

    ./session_run.sh start --flow game-launch9 --run-root <RUNS>
    ./session_run.sh capture --run-dir <RD> --src <S> --src <UE4SS.log> --src <D1> ...
    ./session_run.sh close   --run-dir <RD> --verdict PASS|FAIL|INCONCLUSIVE|ABORTED \
                             --notes "launch 9 window; user actions 1-7; eye confirmations"

Honesty: only ONE real launch window's fresh evidence; host tests never advance
a CP/rung; the eye confirmations are written by the flow's handoff steps from
the user's answers, never synthesized by this scaffold. A row NOT-ATTEMPTED
keeps the acceptance honestly INCOMPLETE.
