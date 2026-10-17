# RUN-RECEIPT — wrapper validation, launch-10 stage-aware delta reconciliation wrapper

- worker: worker-wrapper-validation
- date: 2026-10-05 (local)
- object under test: `_verify_lineage_stage_aware(b)` nested in `package_phase(...)`,
  `outputs/2026-10-04-zcode-controller/launch10-build/build_runtime_composition6_l10.py`
  (driver sha256 `ee154e7dd1b4ee74eba74da38463bd5ffef77c395586cea2cc09da214fd18d9d`),
  installed at driver lines 600-645, wrapping frozen `verify_lineage` from
  `outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py`
  (sha256 `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313`,
  = driver's own `PKG_FIXED_SHA` pin, verified).
- harness: `harness.py` (same directory). Raw machine output: `harness-results.json`,
  `harness-run.log` (copies of /tmp/wrapper-validation-scratch/results.json +
  harness-run.log). Scratch: /tmp/wrapper-validation-scratch only.
- verdict: **NO-GO** (one exact, one-line defect; see GO-NOGO.md). All 10 harness
  cases PASS — including the two that pin the defect.

## Harness route (task 1)

"Exec `package_phase` wholesale" route, made safe by three bounded substitutions,
all inside the harness process:

1. Driver source exec'd verbatim in a fresh namespace (`__name__='driver_under_test'`).
2. `ns['importlib']` shimmed AFTER the exec so `package_phase`'s
   `spec_from_file_location('package_prx_fixed', PKG_FIXED)` sequence receives our
   module object — which is itself a FRESH REAL importlib load of the same frozen
   sha-pinned `package_prx.py`, with only `main` swapped for a sentinel (records
   invocation; proves the pipeline never ran) and, for canned-evidence cases only,
   `verify_lineage` swapped for a canned function faithful to the real `check()`
   gap semantics (package_prx.py lines 271-276). `Build`, `CLANG_FLAGS`, `PIN`,
   `BOOTSTRAP_UNITS` remain the real frozen objects, so the driver's in-memory
   patches (`_cpp20_run`, BOOTSTRAP_UNITS edits, PIN adjustments) land exactly as
   in production.
3. `ns['MY']` redirected into scratch, so `package_phase`'s only filesystem write
   (transport-ledger mirror) lands in /tmp, never in worker-comp6-driver.

`package_phase` is then called with real inputs; it installs the ACTUAL wrapper
(closure over real `m`, real `overlay` param) and raises SystemExit at its own
`out.exists()` guard (driver line 654) — after wrapper installation (line 645),
before `sys.argv` mutation and `m.main()` (lines 656-660). Asserted per case:
sentinel never invoked, wrapper identity replaced, guard message, `sys.argv`
untouched. The wrapper is then driven via `m.verify_lineage` with either the real
frozen `Build` (real subprocesses) or `FakeBuild` (records argv, replicates
`Build.run` raise semantics from package_prx.py lines 220-234 plus the driver's
`-std=c++20` injection from lines 560-565, canned rc).

## Preflight (all green)

- driver's `PKG_FIXED_SHA` == measured sha of frozen package_prx.py: OK
- diagnostic sink staging sha `f5e96c41…` == driver pin: OK
- overlay bridge sha `663e7ce9…` == repaired r14 spanpub bridge (EDITION.json): OK
- overlay `SeedValidation.hpp` present: OK
- real `prospero-clang++` and `llvm-nm` present: OK
- run-l10-1 receipt status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`: OK
- real delta `added` == {3 spanpub accessors + sceKernelGetModuleInfo},
  `expected_bootstrap_resolutions` == the 3 accessors: OK
- failed candidate prx-candidate-l10-1: 33 checks, exactly ONE failing:
  `undefined_delta_added_matches_checkpoint_claim`: OK

## Per-case results (final run; wall-clock = wall of the wrapped call)

| case | inputs | expected | observed | verdict | wall |
|---|---|---|---|---|---|
| P1-production-env-full-real | REAL verify_lineage, REAL Build (real tools), real run-l10-1 delta, real overlay, production env | must NOT supersede (defect manifestation; fail-closed) | compile rc=126 (`/llvm-clang++: No such file or directory`), superseded=False, gap retained | PASS | 4.764s |
| P1b-control-env-full-real | P1 + LLVM_CONFIG injected into process env | superseded=True, gap cleared | compile rc=0, nm rc=0, superseded=True, gaps=[] | PASS | 8.889s |
| P2-production-env-canned-evidence | canned pass-12 evidence + real tools, production env | must NOT supersede | compile rc=126, superseded=False, gap retained | PASS | 0.285s |
| P2b-control-env-canned-evidence | P2 + LLVM_CONFIG env | superseded=True | compile rc=0, nm rc=0, superseded=True, gaps=[] | PASS | 2.754s |
| N-a-failed-nm | canned evidence, FakeBuild nm rc=1 | no supersede, gap stays, compile attempted | exactly that (compile canned rc=0, nm rc=1 raises) | PASS | 0.002s |
| N-b-failed-compile | canned evidence, FakeBuild clang rc=1 | no supersede, gap stays, compile attempted | exactly that | PASS | 0.002s |
| N-c-second-failing-check | evidence with 2 failing checks | no supersede, both gaps stay, NO compile | exactly that | PASS | 0.001s |
| N-d-unexpected-4th-added | mutated delta: added gains `harness_totally_unexpected_symbol` | no supersede, gap stays, NO compile | exactly that | PASS | 0.001s |
| N-e-empty-expected-resolutions | mutated delta: `expected_bootstrap_resolutions=[]` | no supersede, gap stays, NO compile | exactly that | PASS | 0.001s |
| N-f-bridge-missing | overlay param without lifecycle/runtime_start_bridge.cpp | no supersede, gap stays, NO compile | exactly that (assert swallowed by wrapper) | PASS | 0.001s |

OVERALL: ALL-CASES-PASS (10/10). Install phase (driver exec + wrapper
installation through the real `package_phase` code): 0.02-0.26s per case.

P1b superseding check entry (evidence records BOTH results), verbatim from the
returned evidence:

```json
{
 "original": "raw runtime-object delta != composition-2 checkpoint",
 "stage_aware": "raw added == checkpoint + expected_bootstrap_resolutions ['bridge_capture_envelope', 'bridge_readable_span_at', 'bridge_readable_span_count']; all three verified DEFINED by a pre-build one-TU compile of the bridge source against the staged overlay (the packaging bootstrap compile will bind them); post-link resolution in the final PRX verified separately by the runner",
 "policy": "user directive 2026-10-05: packaging expectation reflects build stage; raw delta preserved; fail-closed otherwise",
 "superseded": true, "pass": true
}
```

`decline_record_present` = False in EVERY case (including the rc=126 defect
cases): the wrapper records NOTHING when it declines. See REPORT.md task 5 for
the proposed one-line evidence addition (not applied).

## Real tools used (positive cases)

- compile: `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
  with the driver's exact argv (CLANG_FLAGS + 4 overlay includes + `-std=c++20`
  injection), bridge = overlay `lifecycle/runtime_start_bridge.cpp` (sha 663e7ce9…)
- nm: `outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm
  --extern-only --defined-only --format=posix` — shows all three accessors as
  global DEFINED (`T`): `bridge_capture_envelope T`, `bridge_readable_span_at T`,
  `bridge_readable_span_count T` (name-first posix format confirmed live)

## Isolation statement

Writes: /tmp/wrapper-validation-scratch only (+ this output dir). No writes to
worker-comp6-driver, any run dir, candidate dir, staging overlay, or the frozen
sources. All real artifacts accessed strictly read-only.
