# RUN-RECEIPT — worker-wrapper-validation-l11

Two recorded runs of the same harness (identical case plan and pinned inputs):

| run | driver under test | date (UTC) | wall-clock | result |
|---|---|---|---|---|
| v1 | `291d3b9a...` (pre-fix) | 2026-10-05T20:30:45Z | 12.32 s | 5/6 PASS; step-4 findings (below) |
| v2 | `af309a3a8a36420b7cd202a6d67338bc568859608097319a74896a79003ea0fa` (controller supersede-detail fix) | 2026-10-05T20:34:21Z | 13.03 s | **5/6 PASS; N-a still FAILs on the step-4 preproof_exception check → NO-GO** |

- runner: worker-wrapper-validation-l11 (this worker only; LOCAL ONLY scope honored —
  writes confined to this directory + /tmp/wrapper-validation-l11-scratch)
- harness: `harness.py` (v2 expectation set; P1 requires the supersede detail to carry the
  reconciliation record with accepted=true fields)
- results: `harness-results.json` (v2 run) sha256 `41d3ccefeef540f906a67395e30b3004ff1952cb61a3648a1d495b42d2d9968e`

## Object under test

- driver: `outputs/2026-10-04-zcode-controller/launch11-combined/build_runtime_composition6_l11.py`
  — v1 sha `291d3b9a0b70d768109c49094eca55a15f8a8beac46738008f86b03e24dc965f`,
  v2 sha `af309a3a8a36...` (UPDATED wrapper at driver lines 681-753; wrapper byte-carried
  from l10 f4103572 per `diff_driver_l10-to-l11.diff` except the v2 supersede-detail fix)
- frozen pipeline: `outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py`
  sha256 `e6fbe6c5...` == driver's `PKG_FIXED_SHA` pin (verified both runs)
- route (identical both runs): driver exec'd verbatim (`__name__='driver_under_test'`),
  `ns['importlib']` shimmed to a fresh REAL load of package_prx.py with sentinel `main`,
  `ns['MY']` redirected to scratch. Sentinel proved `m.main()` never ran; the driver's own
  `output dir exists` guard fired in all 6 installs per run; `sys.argv` untouched.

## Pinned real inputs (preflight-verified, zero drift, both runs)

| input | value |
|---|---|
| run dir | `.superpowers/.../ue4ss-native-supplier-composition-6/run-l11-1` (status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`; object sha `afc1fb7dcfe878dc...` verified against receipt, 117,975,808 B) |
| delta `added` | `bridge_capture_envelope, bridge_readable_span_at, bridge_readable_span_count, diagnostic_event, sceKernelGetModuleInfo` |
| `expected_bootstrap_resolutions` | the 4 accessors incl. `diagnostic_event` |
| overlay bridge | `worker-comp6-driver/staging/lineage-overlay-2/lifecycle/runtime_start_bridge.cpp` sha `d96cfe70` (pinned, unchanged) |
| staged diagnostic.c | sha `f5e96c41...` == driver `_diag_sha` pin |
| r13 baseline bridge | `worker-module-start-wiring/runtime_start_bridge_r13.cpp` sha `10385e22...`, 0 accessor mentions (standalone pre-verified: compiles rc=0, defines no accessors) |
| candidate lineage evidence | `launch11-combined/prx-candidate-l11-1/verification-evidence/lineage-verification.json`: delta check `pass=true, superseded=true`, stage_aware detail names `diagnostic_event`; no hard-failing checks |
| toolchain | prospero-clang++, prospero-clang, llvm-nm, llvm-config, llvm-readelf all present (real executions) |
| env | production: `LLVM_CONFIG` NOT set in harness env; the wrapper supplied `env_extra={'LLVM_CONFIG': ...}` itself on BOTH pre-proof compiles (text pin count == 2) |

## Per-case outcomes — v2 run (all real: no canned evidence, no FakeBuild)

| case | verdict | wrapper sub-process rc (Build.records == `subprocess.run().returncode`, in-process) | superseded | reconciliation record (from the evidence) |
|---|---|---|---|---|
| P1-l11-full-real | **PASS** | clang++ 0, nm 0, clang 0, nm 0 | TRUE | **readable on the supersede path (v2 fix): attempted=true, accepted=true, others_failed=[], delta_matched=true, preproof_compile_verified=true** |
| N-a-diag-source-missing | FAIL (only step-4 index 13) | clang++ 0, nm 0, **clang 1** (real clang error: no such file or directory …/diagnostic.c) | false | attempted=true, accepted=false, others_failed=[], delta_matched=false, preproof_compile_verified=false; **preproof_exception ABSENT** |
| N-b-baseline-r13-bridge | PASS | clang++ 0, nm 0, clang 0, nm 0 | false | attempted=true, accepted=false, others_failed=[], delta_matched=true, preproof_compile_verified=false; preproof_exception correctly absent (pure definition-miss) |
| N-c-unexpected-extra-added | PASS | (none — proof skipped) | false | attempted=true, accepted=false, others_failed=[], delta_matched=false, preproof_compile_verified=false |
| N-d-second-failing-check | PASS | (none — proof skipped) | false | attempted=true, accepted=false, others_failed=[composition_object_bytes], delta_matched=false, preproof_compile_verified=false |
| N-e-unexpected-5th-resolution | PASS | (none — proof skipped) | false | attempted=true, accepted=false, others_failed=[], delta_matched=false, preproof_compile_verified=false |

Provider-split evidence (P1, unchanged by the fix): bridge pre-proof nm defines exactly the
3 accessors and NOT `diagnostic_event`; diagnostic.c pre-proof nm defines
`diagnostic_event`. N-b mirror: r13 bridge defines 23 symbols incl. r13-era
`modbridge_runtime_start_with_activation`/`_stop` but ZERO accessors; diag pre-proof still
proves `diagnostic_event`.

## Step-4 result across both runs (preproof_exception)

- v1 (driver 291d3b9a): two wrapper-path defects observed:
  (a) supersede path dropped the whole `stage_aware_reconciliation` record — controller
  FIXED in af309a3a (detail now includes it explicitly);
  (b) decline path: driver line 734 (inside `except`) writes
  `detail.stage_aware_reconciliation.preproof_exception`, then driver line 735 UNCONDITIONALLY
  REPLACES `stage_aware_reconciliation` with a fresh dict, erasing it — NOT fixed in af309a3a.
- v2 (driver af309a3a): (a) confirmed FIXED by P1 PASS with all fields readable;
  (b) STILL PRESENT: N-a shows `preproof_exception` ABSENT from the evidence (expected
  present on an exception case); absent-on-definition-miss (N-b) holds. The rc=1 clang
  stderr survives only in `Build.records`, not in the evidence decline record.

## Integrity

- No writes outside this directory and `/tmp/wrapper-validation-l11-scratch`.
- The l10 harness directory (`worker-wrapper-validation/`) untouched; the driver, frozen
  pipeline, overlay, run-l11-1, candidate evidence, r13 bridge and staged diagnostic.c
  read-only throughout.
- Harness-side edits between runs were expectation-only (supersede-path record now required
  by P1 per the v2 fix); the harness never modified the driver or wrapper. Single recorded
  run per driver edition; no failed-run replay.
