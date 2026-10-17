# SESSION-REHEARSAL — full return-session desk run against the fake console

Worker: worker-session-desk-rehearsal, 2026-10-04.
Drive: `tests/fake_console_drive.py` (private extension of
`worker-console-batch-runner/tests/fake_console_drive.py`; their file untouched).
Run: `python3 tests/fake_console_drive.py` → **78 checks green, 0 failed, exit 0**.
Receipts: `receipts/REHEARSAL-RECEIPTS.json` + `receipts/SESSION-GO-NO-GO.json`.
NO console contact, NO network: every curl and USER gate is intercepted in-process;
the REAL `console_batch.py` (sha `fd21d1718d62…`) executes every flow.

## What this rehearsal is

The evidence-tool rehearsal proved the TOOLS. This one rehearses the SESSION:
the full Phase 0 → 3 sequence in one continuous fake console, with the FINAL
artifact set (8fam dll, e2e payload, combined ridealongs, settings v3, IGW-1
armed probe, fc1 placeholder, tick-corrected CP evidence) and the CORRECTED
constants (Tick `0x33ac8c0`, not the retracted `0x37ac8c0`).

## Phase-by-phase results

| Phase | Rehearsed content | Result |
|---|---|---|
| core | Original drive scenarios re-run (dll7, e2e, legacy qdrv1, game, dll8, dll8-stale-7fam, exactly-once dispatch law) | all green |
| 0 sanity | Track-8 pre-image RETR + sha + parse (1 header, 4 pinned lines, 3 registered; igf mode-0 skipped) | ADVANCE |
| 1 comp6 | **MOCK build outcome**: stand-in `modbridge_supplier_composition.prx` (sha `895f6326…`, 3456 B) + simulated verify-only/gate-v2 GO + stage STOR/readback identity; `comp6_one_pass.py` NEVER executed (pre-flight skeleton until the initializer lands) | staged; GATE→Phase 2 |
| 2.1 dll8 | Real flow, 8fam ledger: ALLFAM8 upload once, readback, ONE dispatch, verdict PASS vs EXPECTED-DELTA-8FAM (records 37–40, memory 114688→163840) | PASS |
| 2.2 e2e | 13/13 SHASUMS + payload `a21771f8…`/681888 B, MODE=B, 15 dispatches, 35-line verbatim ledger, §7 reset only inside `/data/modbridge/**` | PASS |
| 2.3 stage-ridealongs | SC-0..SC-7 on the SHT-strip law channel (§3 constants recomputed and cross-checked); combined ini end-state 6 entries / **exactly 5 registered**; both readback-law verdicts OK | PASS |
| 2.4 settings v3 | Freeze-check live pair → STOR **real package-v3 bytes** (ini 1448 B `d8faa4ec…`, loadlist 447 B `4d98ecd7…`) → readback byte-exact both (lockstep law); both A-PI keys present (`HookProcessInternal`/`HookProcessLocalScriptFunction`) | PASS |
| 2.5 IGW probe | REAL armed artifact staged: 20013 B `48949316…` (igd1-6ebab07f2111) — cross-checked against `build-receipt-armed.json` (determinism pair equal); append-only `= 2` line; registered 6 | staged |
| 2.6 fc1 | **MOCK stand-in** (no PRX is built in worker-fc1-validator-probe — src + host tests only); placeholder target `/data/plugins/modbridge_fc1_validator.prx`; registered **7 final** | MOCK staged |
| 3 prep | CP fixtures **regenerated at 0x33ac8c0** by copying (never editing) the evidence-pipeline-rehearsal generator into scratch: 5 scenario trees; committed s01 confirmed still stale at 0x37ac8c0 | done |
| 3.0 comparator | Stale committed s01 → CP2 **PARTIAL** + overall **INCOMPLETE** (never a PASS); regenerated s01 → overall **PASS**, all cells green | proven |
| 3.1/3.2 real tools | `post_launch_check.py --from-dir` → MODS_STARTED (exit 0); `rung_check.py --from-dir` → honest verdict recorded (exit 0) | PASS |
| 3.3 game-evidence | Full ge.0..ge.10 flow on tick-corrected canned L: CP0–CP4 PASS, CP5 PASS (menu), CP6 PASS (Spawned), honesty note in receipt | PASS |

## Branch table walk (CONTROLLER-DECISION-BRIEF, mechanically injected)

| # | Injected shape | What the drive actually did | Brief decision verified? |
|---|---|---|---|
| B0 | Phase-0 pre-image divergence (entry line flipped to mode 0) | sha mismatch detected; no flow runs | YES — STOP + preserve + reconcile, ABORT launch path |
| B1 | dll8 import mismatch (`consumer_imports` 90→88, SEH64 attribution shape) | verdict step FAIL, console receipt SKIPPED_HALT, exactly ONE dispatch, **no automatic dll7** | YES — optional dll7 fallback is explicit+budget-gated only |
| B2 | e2e NOT CONSUMED (cmd file left present, `.consumed` twin absent) | d01 gone/consumed gate FAIL → halt; 14 steps SKIPPED_HALT; NO re-send | YES — reconcile via cmd_gone + evid reads only |
| B3 | ini drift (composition line at rolled-back mode 0) | ride.6 edit returns `stop` → FAIL; ride.7 skipped; console ini byte-untouched; `.bak` written first; decision JSON preserved | YES — never blind-edit; reconcile first |
| B4 | CP2 stale tick (retracted 0x37ac8c0 in a FRESH session) | CP2 PARTIAL(tick line absent) → ge.3 FAIL → CP5 handoff NEVER reached | YES — honest result, no address re-derivation, director reconciliation |
| B5 | CP4 lua error (main.lua load error, banner absent, S still mods-started) | CP4 FAIL(F9/no banner: [lua error]) → halt before pad interaction | YES — ADAPT content offline (distinct revision), do NOT re-stage the PRX |
| B6 | CP7 rung stale (session ms 8865000 ≤ 8866785) — REAL `rung_check.py` | exit 5 INCONCLUSIVE-STALE | YES — re-run only in a fresh window; never re-rate |
| B7 (bonus) | CP5 shape (a): EngineTick-unavailable after mods-started | CP5 FAIL(shape a anomaly → restage check) | YES — reconcile staging provenance, never re-press |

All six required shapes plus two bonus branches resolve exactly as the brief
prescribes. Three one-line corrections surfaced (below).

## Corrections proposed (one line each; none applied — their files untouched)

1. **CONTROLLER-DECISION-BRIEF §4.2 CP2 row**: expected line still cites
   `vtable: 0x37ac8c0; scan: 0x37ac8c0` — the retracted base-convention value
   (worker-tick-correction-propagation). Fix: `0x37ac8c0` → `0x33ac8c0`.
   The live tool already gates on 0x33ac8c0 (verified).
2. **RETURN-SESSION-MASTER-PLAN.md Phase-3 item 7**: same stale constant
   (`Tick 0x37ac8c0`) → `0x33ac8c0`.
3. **Registered-count constants when IGW-1 + FC-1 ride**: brief §3.5 gate
   ("ini registers exactly 5") and the tool's `RIDEALONG_HANDOFF` ("EXACTLY 5
   `OK (<ms>)` lines") pin the tool-only end-state. With the IGW probe and fc1
   validator appended as `= 2` ridealongs the launch window registers **7**.
   Fix: state "5 tool-staged + 2 manual ridealongs = 7" in both places when the
   final artifact set includes IGW-1 + FC-1 (MAX_PLUGINS=10 is not exceeded;
   proven by the drive's parser-fact count at every step: 3 → 5 → 6 → 7).
4. (tool, proposal only) `cp56_verdict` shape-a hint cites only the v2-era
   live sha `9f3fa127…` as the restage marker; after a v3 restage the live sha
   is `d8faa4ec…` — the hint should say "sha != the just-restaged pair".
5. (sibling receipt) `worker-writability-probe-build/REPORT.md` artifact pin
   (19277 B `7b9e030c…` igd1-27618a58f062) predates the design worker's
   TRANSITIONS injection; the live armed artifact is 20013 B `48949316…`
   (igd1-6ebab07f2111) and the build receipt agrees. The drive used the live
   identity (the brief's re-hash-the-bytes lesson, applied).
6. (sibling tool) `simulate_ploader_ini.py` ignores its argv path — its
   variant set is hard-wired to the 6-entry layout, so it cannot count a
   7-registered end-state. Fix: accept `argv[1]` as an extra variant. The
   drive used `console_batch._parse_combined_ini` parser facts as the oracle.

## Session go/no-go checklist (final state)

| Item | State |
|---|---|
| Phase 0 sanity | **GO** — rehearsed; divergence branch aborts correctly |
| Phase 1 comp6 | **CONDITIONAL — MOCK ONLY**; real one-pass blocked on the initializer landing; never run the skeleton as if it built |
| Phase 2 batch | **GO** for dll8 + e2e + stage-ridealongs tool flows (all green); settings v3 restage verified against REAL bytes; IGW staged from the REAL armed artifact; **fc1 staging is MOCK** (no PRX built — build it before the session or drop the step) |
| Phase 3 game-evidence | **GO on regenerated fixtures** — committed CP fixtures are STALE (0x37ac8c0) and would honestly yield INCOMPLETE; regenerate at 0x33ac8c0 first |
| Branch table | 6 required + 2 bonus shapes exercised; all resolve per the brief; corrections 1–3 should land before the session |
| Overall | **CONDITIONAL GO** |

## Honesty bounds

* The fake drive intercepts curl/user gates in-process: it proves step
  ordering, gates, verdicts, exactly-once dispatch, halt semantics, and the
  append-only/readback laws. It does NOT prove wall-clock pacing (the ≥30 s
  dispatch gaps, 15 s dll8 wait, 60 s ridealong windows remain controller
  discipline at the real session) and nothing here is console-proven.
* Phase 1 and fc1 are MOCKS, labeled as such in every receipt. The IGW-1
  armed probe, settings v3 package, dll8/e2e payloads, and both ridealong PRXs
  are REAL on-disk artifacts whose pinned identities the flows hash-verified.
* The committed stale fixtures and the stale REPORT pin were read, never
  modified; no file outside `worker-session-desk-rehearsal/` was written.
