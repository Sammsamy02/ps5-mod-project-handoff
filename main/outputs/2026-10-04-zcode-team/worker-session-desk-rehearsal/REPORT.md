# REPORT — worker-session-desk-rehearsal, 2026-10-04 (written last)

Task: rehearse the SESSION (not the tools — that was worker-evidence-pipeline-rehearsal):
the full Phase 0→3 return-session sequence against the fake-console drive, with the
FINAL artifact set and the corrected constants, then walk the CONTROLLER-DECISION-BRIEF
branch table mechanically with each failure shape injected at its phase.

## Deliverables (in order)

| file | what |
|---|---|
| `CLAIM.json` | scope, read-only inputs, declared mocks, bounds (written first) |
| `tests/fake_console_drive.py` | the extended drive: original tool-regression scenarios + full-session Phase 0–3 + branch-table walk. Their `worker-console-batch-runner/tests/fake_console_drive.py` was NOT edited (note: it is 698 lines on disk and already covers observer-coordination/rollback/law-negative — my core section re-proves the key ones in-scope) |
| `receipts/REHEARSAL-RECEIPTS.json` | per-phase + per-branch machine receipts with the live-verified constants table |
| `receipts/SESSION-GO-NO-GO.json` | the final checklist state |
| `runs/` | per-phase and per-branch run dirs (real `console_batch.py` receipts, verdicts, preserved pre-images) |
| `SESSION-REHEARSAL.md` | the rehearsal narrative (written before this file) |

**Drive result: 78 checks green, 0 failed, exit 0.**

## What was rehearsed

* **Core regression** (their scenarios, my run dirs): dll7, e2e (15 dispatches, 35-line
  ledger, rescue no-dispatch), legacy stage-qdrv1, game-evidence, dll8 PASS vs
  EXPECTED-DELTA-8FAM, dll8 stale-7fam halt, exactly-once dispatch law.
* **Phase 0**: Track-8 pre-image sha + parse; registered count 3 (igf mode-0 skipped).
* **Phase 1 (MOCK)**: stand-in composition PRX staged + readback identity; verify-only /
  gate-v2 GO simulated and labeled MOCK; `comp6_one_pass.py` never executed (it remains
  a pre-flight skeleton until the initializer lands — its on-disk sha recorded).
* **Phase 2 (real tool flows)**: dll8 (8fam ledger, records 37–40, memory
  114688→163840) → e2e (MODE=B, 15 dispatches, §7 reset scoped to `/data/modbridge/**`)
  → stage-ridealongs (SHT-strip law, 5 registered) → settings **v3** restage (REAL
  package-v3 bytes: 1448 B `d8faa4ec…` + 447 B `4d98ecd7…`, byte-exact readbacks, A-PI
  keys present) → IGW-1 armed probe staged (REAL artifact 20013 B `48949316…`
  igd1-6ebab07f2111, build-receipt cross-check) → fc1 MOCK staged → final ini
  **7 registered**, no duplicates, append-only law proven at every append.
* **Phase 3**: fixtures regenerated at **0x33ac8c0** (generator copied, never edited);
  committed s01 proven stale (0x37ac8c0 → CP2 PARTIAL, overall INCOMPLETE) vs
  regenerated (overall PASS); real `post_launch_check.py --from-dir` → MODS_STARTED;
  real `rung_check.py --from-dir` → exit 0 fresh / **exit 5 INCONCLUSIVE-STALE** on a
  stale session; full game-evidence flow green with CP5/CP6 PASS on corrected L.

## Branch walk (6 required + 2 bonus) — all resolve per the brief

B0 pre-image divergence → STOP/ABORT; B1 dll8 import mismatch → verdict FAIL, one
dispatch, no auto-dll7; B2 e2e NOT CONSUMED → halt, no re-send; B3 ini drift → edit
`stop`, console untouched, `.bak` first; B4 CP2 stale tick → PARTIAL/INCOMPLETE, CP5
handoff never reached, no address re-derivation; B5 CP4 lua error → FAIL(F9) naming the
error, ADAPT-content-not-ABORT; B6 rung stale → real exit 5; B7 CP5 shape-a → restage
anomaly classification. Details with next-commands in `REHEARSAL-RECEIPTS.json`.

## Findings and one-line fixes proposed (nothing applied)

1. `CONTROLLER-DECISION-BRIEF.md` §4.2 CP2 row and `RETURN-SESSION-MASTER-PLAN.md`
   item 7 still cite the retracted `0x37ac8c0`; correct constant is `0x33ac8c0`
   (the live tool already gates on it — verified).
2. Registered-count constants: with IGW-1 + FC-1 as extra `= 2` ridealongs the launch
   window registers **7**, not the pinned "exactly 5" (brief §3.5 gate; tool
   `RIDEALONG_HANDOFF` "EXACTLY 5 OK lines"). MAX_PLUGINS=10 not exceeded.
3. `worker-writability-probe-build/REPORT.md` armed-artifact pin is stale
   (pre-TRANSITIONS-injection); live on-disk identity (20013 B `48949316…`) matches the
   build receipt — the drive used the live identity per the re-hash-the-bytes law.
4. `worker-fc1-validator-probe` has NO built PRX (src + host tests only): fc1 staging
   is necessarily a MOCK; build before the session or drop the step.
5. `simulate_ploader_ini.py` ignores its argv path (hard-wired 6-entry variants), so it
   cannot oracle a 7-registered end-state; the drive used `console_batch` parser facts.
6. Tool shape-a hint cites only the v2-era sha `9f3fa127…`; after a v3 restage the
   live marker is `d8faa4ec…` (proposal-only; their files untouched).

## Go/no-go (final)

Phase 0 **GO** · Phase 1 **CONDITIONAL (mock only)** · Phase 2 **GO** (fc1 MOCK) ·
Phase 3 **GO on regenerated fixtures** · **overall CONDITIONAL GO** — blockers: the
comp6 initializer, the fc1 PRX build, and corrections 1–2 landing before the session.

## Honesty bounds

Fake console only (zero console contact, zero network); pacing/time windows are NOT
proven by an in-process drive (≥30 s dispatch gaps remain controller discipline);
Phase 1 + fc1 are declared mocks; everything else staged from REAL on-disk artifacts
whose identities the flows hash-verified. No file outside `worker-session-desk-rehearsal/`
was written; stale siblings were read, never modified. Nothing here is
console-proven or an acceptance claim.
