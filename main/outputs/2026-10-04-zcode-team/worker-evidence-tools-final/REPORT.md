# REPORT — worker-evidence-tools-final (2026-10-04/05)

Final cross-format audit of the three evidence tools after every accumulated patch
(freshness gates, r4-prefix regex fix, ninth-slot adoption, tick correction). Read-only
audit + minimal fixes; owners had all delivered; NO console contact, no nested
subagents; all evidence host/fixture only.

**Verdict: the three tools now agree with EACH OTHER and with the real r4 entry /
bridge r13 / probe output formats, with three drift findings fixed (one real
misroute, one label slip, one test-harness shell bug). The full rehearsal suite
re-run with the final bytes: all five scenarios + both freshness refusals still
discriminate.**

## 1. Cross-format audit (receipt: AUDIT-RECEIPTS.md §1 matrix)

Nine parser-vs-writer rows, each traced to the emitter source (not to docs):

- **r4 entry failure line** (`runtime_module_entry_r4.c:184-222`): `[t=<ms>ms
  pid=<pid>] modbridge module_start failed: <stage> code=<n> detail=…`. The
  controller's optional-prefix regex in post_launch_check.py parses prefixed,
  legacy, empty-detail, special-char and capacity-truncated shapes; console_batch's
  `classify_l_log` substring findall agrees on every shape. No drift.
- **D1 diagnostic grammar**: `hex()` always emits 8 lowercase digits, so the
  `value=0x00000000` literals in `classify_d1_log` are valid; OBSERVER_EVENT_RE
  matches. No drift.
- **Status tab grammar**: all three parsers agree; `put_text` sanitization makes
  column-shifting tabs/newlines impossible; the 512-B line cap preserves the
  `[Seeds]` prefix of a truncated detail.
- **NQH1 records**: rung_check reads exactly `FormatRecordJsonLine`'s keys
  (counts.first/second/recorded, ticks.accepted/refusals_between, identity_checks,
  threshold, gate_source, monotonic). No drift.
- **cc1 census oracle**: every oracle field (verdict strings, controls,
  negative_hits, census_truncated, polls=30/poll_s=4, write_failures, the ONE
  `CC1: build=cc1-…` line) traced to the carrier probe emitter. No drift.
- **Freshness oracle** `8866785` identical in all three tools, same no-PASS
  direction. **Tick line** carries the corrected `0x33ac8c0`. **Ninth-slot ini
  law** (9 entries / 8 registered / EXACTLY 8 OK lines) matches the adopted law.

## 2. Findings and fixes

| # | Severity | Finding | Fix |
|---|---|---|---|
| F-1 | label-only | console_batch ge.2 title said "Fetch the **eight** evidence sinks"; fetch_fn fetches **seven** (S, L, D1, C2, XWP, FC1, CC1). The brief §4.1 list double-counts C2 as "the igf facts jsonl" — same path. | title → "seven" (one word). Selftest 405/0 and fake drive 92/0 unchanged. Brief + ninth-slot REPORT carry the same slip — flagged, not edited (other workers' deliverables). |
| F-2 | real misroute | post_launch_check's FAILED_STATUS guidance sent the launch-9 `[Seeds]` refusal to **"bridge stage 1 CONVERSION … vm-scan"** triage (stage_hint GAP-2 list only knew "PS scan timed out"/"AOB scans"; any other init-thread throw with code=1 fell into BRIDGE_GUIDANCE[1]). | stage_hint: `[Seeds]` → new `seeds_refused` stage; `GMalloc is uninitialized` / `initializer callback registration failed` → init_thread_failed (launch-9 terminals T-A/T-REF/T-GM/T-WR); dedicated STAGE_GUIDANCE entry. Regression table proves all prior routes (s03 lease→CONTEXT, mfa -2, stage 3) unchanged; host tests 20/20. |
| F-3 | test harness | rung_check's own host-test `mk_status` used `"$(( $+1 ))"` — syntax error under bash (status rows silently LOST → rung1-pass + placeholders failed, 12/2), silent seq=0 under zsh (why it had passed). rung_check.py itself was NOT drifted. | proper seq counter; now **14/0 under both bash and zsh**. |

Post-fix identities: post_launch_check `e6cea9ee…`, console_batch `5ca8d1c7…`,
rung_check UNCHANGED `287a2684…` (as-found hashes in receipts-*-sha256.txt).

## 3. Seeds-refusal case (launch-9 expected terminal) — all three tools correct

Fixture built only from real source shapes (throw text UnrealInitializer.cpp:898-905;
status `failed(1,<what>)` per runtime_owner.cpp:111-118; D1 per the entry-r4 MRS_OK
path). Result:

- **post_launch_check**: FAILED_STATUS; ZERO bridge failure lines — `[Seeds]` is NOT
  a `modbridge module_start failed:` line per the fixed regex (visible in new_lines);
  guidance now names the seeds family (was the F-2 misroute).
- **rung_check**: honest CANNOT_ASSERT exit 3; RUNG1 conditions record `session
  failed (code=1 detail='[Seeds] …')`; UNVERIFIED_DRIVER honest (G2) — matches the
  brief CP7 table.
- **console_batch CP gates**: `entry_failure_lines` empty (agrees with
  post_launch_check); CP0 PASS + CP1 PASS (PRX ran, activation consumed), CP2/CP3
  FAIL(no engine-ready / no mods-started; F7/F8 shapes), CP4 FAIL(F9/no banner),
  overall INCOMPLETE — exactly the brief §4.2 CP3-F7 row (init-thread throw; read
  `what`).

## 4. Re-rehearsal with the final tool bytes

`run_rerun_final.py` mirrors worker-evidence-pipeline-rehearsal/run_rehearsal.py
step-for-step (plc --json + --baseline-file; rung ALL + --discover s03 + --rung RUNG2
s01; console_batch's own CP classifiers incl. CP5/CP6 L-diff) and writes only under
this worker — the committed rehearsal artifacts stay byte-preserved. Two trees:

- **corrected/** (regenerated fixtures, tick line 0x33ac8c0 per CORRECTION-REGISTER
  A5; diff vs committed = tick lines only): s01 **all-green** (plc MODS_STARTED,
  rung PASS/0, CP overall PASS, CP5/CP6 PASS, CP7 exit 0); s02 vs s03 early-failure
  shapes separated (s03 CP0 `FAIL(F3/F4/F5 runtime_start_failed: ['00000002'])` —
  GAP-1b intact; s02 init-thread guidance); s04 trap caught at CP4 (named lua error);
  s05 NOT_STARTED + 3 probe repeats + rung **exit 5** + all CP cells
  INCONCLUSIVE-STALE; probe_gap4_stale_driver rung **exit 5** (GAP-4 refusal).
- **committed/** (originals): identical EXCEPT s01/s04 CP2 = `PARTIAL(engine-ready
  yes, Tick line absent — G7)` → overall INCOMPLETE — the documented consequence of
  the tick correction against the stale fixtures ("the stale value no longer grounds
  a PASS"), preserved as evidence, never overwritten.

Both freshness refusals still discriminate. Machine summary: rerun-results.json;
raw outputs runs/rerun-corrected/ and runs/rerun-committed/.

## 5. Suites re-run against the final bytes

console_batch selftest **405 ok / 0 failed**; fake_console_drive **92 green /
0 failed**; post_launch_check host tests **20/20**; rung_check host tests **14/14**
(bash and zsh, after F-3); py_compile + shell syntax checks clean.

## 6. Do-not-claim bounds

Nothing here certifies the console, rungs, engine-readiness, or any acceptance rung;
the Seeds fixture is a source-derived prediction of launch-9's terminal
(LAUNCH9-PREDICTION §4-#2 / LAUNCH10-MATRIX T-A), not an observed launch. Gameplay
remains a prototype; host/fixture evidence only.

Deliverables: CLAIM.json · AUDIT-RECEIPTS.md · this REPORT.md · runs/ · diffs/ ·
receipts-as-found/post-fix/final-sha256.txt · fixtures/ + fixtures-corrected/ ·
run_rerun_final.py + build_fixtures_corrected.py.
