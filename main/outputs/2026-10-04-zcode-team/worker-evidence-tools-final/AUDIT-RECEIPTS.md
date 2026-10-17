# AUDIT-RECEIPTS — final cross-format audit of the three evidence tools

Worker: worker-evidence-tools-final · 2026-10-04/05 · read-only audit + minimal fixes.
NO console contact; every check ran host-side in --from-dir fixture mode or in-process.
The three tools' owners had all delivered; this worker inherited the final bytes.

## 0. As-found / post-fix identity

| File | as-found sha256 (before this worker) | post-fix sha256 |
|---|---|---|
| worker-post-launch-tools/post_launch_check.py | `801258fc46498c3c847dac52bf23ccf7ac19d65eacac1be9253e115750057c23` | `e6cea9ee42d43519f7f932f392b616748dca480713f1aec806b691f3889499b8` |
| worker-console-batch-runner/console_batch.py | `456f9372f7c59a412f4db8247f82b424cc66b02efa2a19bbf7d679845f573734` (= worker-ninth-slot-ini-adoption copy, byte-identical) | `5ca8d1c70f4d3349d34bd3746b1cfa8647d56ffbc7b61fa42bdc5862dbd74563` |
| worker-rung-evidence-reader/rung_check.py | `287a26844ad2491181b85f19c0dc01fbd2c48675dd0349d3b939acaa63a59694` | UNCHANGED (same sha) |
| worker-rung-evidence-reader/host-test/run_host_tests.sh | (not re-pinned pre-edit; one function fixed, quoted in §F-3) | `bf7f6d9379c475f2035cf3db6996c5959664e6c3084fe05f812d15152b6a4538` |

Raw outputs: `runs/` (BEFORE/AFTER pairs preserved); machine summary `rerun-results.json`.

## 1. Cross-format audit matrix — every parser vs its REAL writer source

| # | Real writer (source read) | Actual emitted format | Tool parser | Verdict |
|---|---|---|---|---|
| A | r4 entry `write_ue4ss_log_line` (runtime_module_entry_r4.c:184-222) | `[t=<ms>ms pid=<pid>] modbridge module_start failed: <stage> code=<n> detail=<sanitized>` (256-B line cap, printable-ASCII sanitize) | post_launch_check FAILURE_LINE_RE (optional-prefix group, lines 82-85) | MATCH — verified on prefixed, legacy-unprefixed, empty-detail, special-char, truncated shapes (runs/r4-prefix-regex-check-BEFORE.txt) |
| A' | same line | same | console_batch classify_l_log `entry_failure_lines` substring findall | MATCH both prefix shapes (same receipt); agree with post_launch_check in every case |
| B | D1 diagnostic channel (worker-aligned-entry-observer/source/diagnostic.c:148-157 `diagnostic_event`, `hex()` fixed-width shift 28..0 lowercase) | `[PPSA33286] modbridge-runtime-aligned-entry/v1 build=<hex32> <tag> value=0x%08x` | post_launch_check OBSERVER_EVENT_RE; console_batch classify_d1_log (`value=0x00000000` literals, `value=0x([0-9a-f]+)`) | MATCH — 8-zero literals valid because hex() is ALWAYS 8 lowercase digits |
| C | runtime-status.txt grammar (modforge_activation.c `mfa_status_write`: `state\t<seq>\t<state>\t<code>\t<detail>`; put_text maps <0x20/0x7f to '?' so no tab/newline can shift columns; 512-B line) | tab grammar | post_launch_check parse_status, rung_check parse_status, console_batch cp_parse_status_s | MATCH in all three; Seeds detail truncation at 512-B line keeps the `[Seeds]` prefix intact |
| D | NQH1 record (worker-record-sink-addon/src/NativeQueryHarness.cpp:459-500 `FormatRecordJsonLine`) | `{"record":"native_query_harness","magic":"NQH1","status":..,"count_source":..,"game_thread_id_key":"<hex>","counts":{first,second,recorded},"delta","monotonic","identity_checks","threshold","ticks":{dispatched,accepted,refusals_between},"gate_source":..}` | rung_check classify_c2/rung2_verdict reads exactly these keys (ticks.accepted, refusals_between, counts.recorded/first/second, threshold==10000, gate_source, identity_checks==2, monotonic in equal/non_decreasing) | MATCH — field-by-field |
| E | cc1 census sink (worker-carrier-probe-build/src/carrier_driver.c:747-782 + module_entry.c:70-96 + record_sink.c:151 envelope) | `{"marker":"modbridge.ingamefacts",..,"build_id":"cc1-…","exp":"census_result",..,"verdict":"CENSUS:VALIDATED/REFUSED","reason_code",controls{None_id_zero,ByteProperty_ascii_ok},negative_hits{PostRender,ZZZNotARealFunctionQQ},census_truncated,budget_entries}`; startup carries polls=30/poll_s=4; summary carries write_failures/polls_run; ONE `CC1: build=cc1-… verdict=` UE4SS line | console_batch carrier_evidence_verdict (+_cc1_field top-level/fields) | MATCH — every oracle field traced to the emitter |
| F | xwp/fc1 probe sinks | build_id igd1-6ebab07f2111 / fc1pool1-78508a2e2940, exp/verdict/checks_passed | console_batch probe_evidence_verdict | MATCH (ninth-slot receipts pinned; constants re-read in source dict §230-280) |
| G | tick evidence line | `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)` (CORRECTION-REGISTER corrected fact) | console_batch GAME["tick_line"] → CP2 | MATCH — tool carries the corrected 0x33ac8c0 |
| H | ninth-slot ini law | 9 entries / 8 registered; EXACTLY 8 `OK (<ms>): <file>` / ZERO FAIL | FINAL_INI registered_expected_final=8, post_launch_ok_lines_expected=8, canonical 440 B fd9c7393 | MATCH |
| I | freshness oracle | PATH_TEST monotonic-ns 8866785633337 → ms threshold | 8866785 in ALL THREE tools (post_launch_check:100, rung_check:87, console_batch:1563) | MATCH — identical constant, identical direction (no PASS without a datable fresh session) |

## 2. Seeds-refusal case (launch-9 expected terminal T-A) — verdict per tool

Fixture `fixtures/seeds_refusal_launch9/from-dir/` built ONLY from real source shapes:
UE4SS.log = 3-line baseline + UE4SS startup banners + the exact `[Seeds] Required-slot
seed table incomplete: … missing [gmalloc]. …` throw text (UnrealInitializer.cpp:898-905);
runtime-status.txt = fresh session 9534000 + `activated` + `failed 1 [Seeds] …` (ack path
runtime_owner.cpp:111-118 via RuntimeAckSink::failed → emit("failed",1,what), owner
ue4ss_backend initialize → program_->init() throw); D1 = entry grammar incl.
`runtime_start_ok value=0x00000000` + `module_start_return value=0x00000000`
(owner->start() returns before the init thread throws — entry r4 MRS_OK path).

| Tool | Classification | Per the brief? |
|---|---|---|
| post_launch_check | verdict **FAILED_STATUS**; failure_lines = 0 ([Seeds] is NOT a `modbridge module_start failed:` line per the fixed regex — correct); [Seeds] line visible in new_lines; status parsed (failed, code=1) | CORRECT verdict; guidance was the FINDING (§F-2) |
| rung_check | RUNG1 conditions record `session failed (code=1 detail='[Seeds] …')`; overall honest CANNOT_ASSERT exit 3 (no driver evidence — UNVERIFIED_DRIVER is honest, G2; absence of rung evidence ≠ launch failure) | CORRECT (brief §4.3 CP7 table: 3 = CANNOT_ASSERT honest) |
| console_batch CP gates | entry_failure_lines EMPTY (regex agreement with post_launch_check); D1 fully parsed; CP0 PASS + CP1 PASS (PRX ran, activation consumed) + CP2/CP3 FAIL(no engine-ready / no mods-started; F7/F8 shapes) + CP4 FAIL(F9/no banner); overall INCOMPLETE, hint = runbook §6 triage S→D1→L | CORRECT — exactly the brief §4.2 CP3-F7 row: `activated` then `failed 1 <what>` = init-thread throw, read `what` |

## 3. Findings and fixes

### F-1 (label-only, FIXED): ge.2 said "eight evidence sinks", fetches SEVEN
`build_game_evidence` fetch_fn fetches exactly 7 distinct console files (S, L, D1, C2,
XWP, FC1, CC1) and lists 7 curl commands; the ninth-slot revision bumped the step title
seven→eight (the CONTROLLER-DECISION-BRIEF §4.1 list double-counts C2 as "the igf facts
jsonl" — same path /data/modbridge-ingame-facts.jsonl). Fix: title "eight"→"seven"
(console_batch.py:3838). No behavioral surface; selftest 405/0 and fake drive 92/0
unchanged. The same slip remains in the brief text and worker-ninth-slot REPORT (their
deliverables — NOT edited by this worker; reconcile by reading the code).

### F-2 (real misroute, FIXED): post_launch_check sent the Seeds refusal to bridge
CONVERSION triage
`stage_hint` (GAP 2 fix) only recognized "PS scan timed out"/"AOB scans" as init-thread
signatures. A status `failed(1, <what>)` row with any other init-thread throw detail
(code 1 ∈ BRIDGE_GUIDANCE) fell through to `runtime_start_failed` → BRIDGE_GUIDANCE[1]
CONVERSION ("loader-facts capture/conversion refused (VMMAP-first + vm-scan)") — the
exact routing GAP 2 forbids, applied to launch-9's predicted terminal.
BEFORE evidence: runs/seeds_BEFORE_post_launch_check.json (NEXT ends "bridge stage 1
CONVERSION …"). Fix (minimal, two hunks in post_launch_check.py):
  1. stage_hint: `"[Seeds]" in detail` → new stage `seeds_refused`; `"GMalloc is
     uninitialized"` / `"initializer callback registration failed"` → init_thread_failed
     (launch-9 terminals T-A/T-REF/T-GM/T-WR per LAUNCH10-MATRIX §4 box 2; all ack-path
     `failed(1,<what>)` rows, never a bridge failure line).
  2. STAGE_GUIDANCE["seeds_refused"]: seeds-refusal triage (seed-table derivation;
     NEW launch for a new window; never bridge CONVERSION vm-scan triage).
AFTER: runs/seeds_AFTER_post_launch_check.json + stage_hint-regression-AFTER.txt —
T-A/T-REF → seeds_refused; T-GM/T-WR/outcome#8 → init_thread_failed; s03 bridge lease
(code 2) → runtime_start_failed CONTEXT; entry mfa (-2) → mfa_activate_failed; bridge
stage 3 → START. All prior routes byte-equivalent. post_launch host tests 20/20.

### F-3 (test-harness shell bug, FIXED): rung_check host-test lost status rows under bash
run_host_tests.sh mk_status used `"$(( $+1 ))"` — an invalid arithmetic token: hard
syntax error under bash/sh (state rows silently LOST → `rung1-pass` and `placeholders
ALL` failed 12/2), silent 0 under zsh (every seq 0 — how it passed before). Fix:
proper `seq` counter. After: 14 passed / 0 failed under BOTH bash and zsh. rung_check.py
itself untouched. (Honest note: the fixtures' `session 298 8866900000000` is ns-scale,
not ms — it satisfies the O2 fresh gate as fabricated "fresh"; left as-is, flagged here.)

### Non-findings (checked, agree)
- r4-prefix regex (controller fix) correct against the entry's real byte shapes.
- Both UE4SS.log failure-line classifiers (post_launch_check regex, console_batch
  substring) agree on every probe shape incl. prefix presence/absence.
- Freshness gates agree across all three tools (same constant, same no-PASS-without-
  fresh-session direction); probe-repeat oracle (PATH_TEST/ACTIVATION_TEST/DIAG
  prefixes) unchanged and exercised by s05/probe tree (3 repeats each).
- CP5/CP6 L-diff classifier and CP7 delegation unchanged (s01 corrected tree: CP5 PASS,
  CP6 PASS, CP7 exit 0 PASS).
- census oracle, probe-sink oracles, NQH1 reader, D1 reader, status parsers: all match
  their writers field-for-field (matrix §1).

## 4. Re-rehearsal with the FINAL tool bytes (runs/rerun-results.json)

Harness: run_rerun_final.py (step-for-step mirror of worker-evidence-pipeline-
rehearsal/run_rehearsal.py: post_launch_check --json + --baseline-file variant; rung_check
ALL + --discover on s03 + --rung RUNG2 on s01; console_batch's own classifiers for
CP0-CP4/CP5-6). It writes ONLY under worker-evidence-tools-final/ — the committed
rehearsal worker's fixtures/, runs/ and rehearsal-results.json are byte-preserved
(rehearsal-results.json md5 6bc8e743…; s01 fixture md5 23456f69…).

Two trees:
- corrected/ — regenerated by build_fixtures_corrected.py (build_fixtures.py with ONLY
  the output dir repointed; the generator already emits the corrected 0x33ac8c0 tick
  line per CORRECTION-REGISTER A5) + verbatim probe_gap4_stale_driver. Diff vs
  committed = tick-address lines in s01/s04 UE4SS.log files ONLY (diffs/fixture-tick-delta.diff).
- committed/ — the original fixtures (s01/s04 still carry the superseded 0x37ac8c0).

| Scenario | corrected tree | committed tree | Discriminates |
|---|---|---|---|
| s01_full_success | plc MODS_STARTED · rung PASS/0 · CP overall PASS · CP5/CP6/CP7 green | same except CP2 = PARTIAL(engine-ready yes, Tick line absent — G7), overall INCOMPLETE | the documented tick-correction delta (CORRECTION-REGISTER "the stale value no longer grounds a PASS"); NOT a tool regression |
| s02_ps_scan_timeout | plc FAILED_STATUS · CP0 PASS, CP2/CP3 FAIL · rung CANNOT_ASSERT/3 | identical | separated from s01 at CP2/CP3 and from s03 at plc stage guidance (init_thread_failed) |
| s03_lease_gate_refusal | plc FAILED_runtime_start_failed · CP0 FAIL(F3/F4/F5 runtime_start_failed: ['00000002']) · entry_failure_lines caught by BOTH tools · --discover exit 0, C2 honestly absent | identical | GAP-1b label intact |
| s04_script_error_trap | plc MODS_STARTED · CP4 FAIL(F9/no banner + named lua error) · overall INCOMPLETE | identical (same CP2 PARTIAL note as s01) | trap caught at CP4 exactly as the original rehearsal table |
| s05_stale_launch | plc NOT_STARTED + 3 probe repeats · rung INCONCLUSIVE-STALE exit 5 · CP0/1/2/3 INCONCLUSIVE-STALE, zero PASS cells | identical | BOTH freshness refusals fire |
| probe_gap4_stale_driver | rung exit 5 (GAP-4 refusal); RUNG2 honest FAIL | identical | stale driver can never PASS |

Both freshness refusals still discriminate; every scenario keeps its original
discriminating cell on the corrected tree.

## 5. Owner suites re-run against the final bytes

| Suite | Result |
|---|---|
| console_batch.py selftest | **405 ok, 0 failed** (same count as the ninth-slot delivery — label change shifted nothing) |
| tests/fake_console_drive.py | **92 green, 0 failed** (rc 0) |
| post_launch_check host tests | **20/20 PASS** (was 20/20 before fix; routes unchanged) |
| rung_check host tests (bash) | 2 failed BEFORE → **14 passed, 0 failed AFTER** (F-3) |
| rung_check host tests (zsh) | **14 passed, 0 failed** |
| py_compile all three tools | OK; bash -n / zsh -n on the harness OK |

## 6. Honesty bounds

Everything here is host/fixture evidence. Nothing certifies the console, rungs, or any
acceptance; the Seeds fixture is a source-derived prediction of launch-9's terminal
(LAUNCH9-PREDICTION §4-#2 / LAUNCH10-MATRIX T-A), not an observed launch. The committed
rehearsal tree's s01 CP2 PARTIAL under the corrected tool is the documented consequence
of the tick correction, preserved as evidence, not "fixed" by overwriting fixtures.
