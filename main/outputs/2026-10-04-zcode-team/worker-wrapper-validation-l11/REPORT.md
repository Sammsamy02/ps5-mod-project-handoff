# REPORT — worker-wrapper-validation-l11

Worker: wrapper-validation-l11 · 2026-10-05 · scope: LOCAL ONLY (this dir + harness dir;
everything else read-only; writes in /tmp/wrapper-validation-l11-scratch)

## Mission and outcome

Exercise the UPDATED stage-aware wrapper (installed by the l11 combined driver) through the
existing validation-harness route with the REAL pinned inputs, then report the verdict. Two
recorded runs:

- **v1 vs driver `291d3b9a...`** (pre-fix): 5/6 PASS; found (a) the supersede path dropped the
  whole `stage_aware_reconciliation` record, and (b) the decline path wiped the instrumented
  `preproof_exception`. Reported to the controller.
- **v2 vs driver `af309a3a...`** (controller supersede-detail fix, re-run on request):
  **5/6 PASS — P1 now passes with ALL reconciliation fields readable, and all negative cases
  still decline correctly — but the verdict remains NO-GO solely on the mission's step-4
  check: `preproof_exception` is still absent from the evidence on the exception case (N-a).**
  Criterion tables: GO-NOGO.md; run facts: RUN-RECEIPT.md; machine results: harness-results.json.

## Object under test and its three changes (all verified present, preflight text pins)

`_verify_lineage_stage_aware` inside `package_phase` of
`outputs/2026-10-04-zcode-controller/launch11-combined/build_runtime_composition6_l11.py`
(v1 `291d3b9a...` → v2 `af309a3a...`; wrapper block lines 681-753, byte-carried from l10
f4103572 per `diff_driver_l10-to-l11.diff` except the v2 supersede-detail fix):

1. allowed-names tuple includes `diagnostic_event` (line 689);
2. DUAL pre-proof: bridge TU via `prospero-clang++` AND staged `diagnostic.c` via
   `prospero-clang`, each with `env_extra={'LLVM_CONFIG': <llvm-config>}`
   (exactly 2 occurrences);
3. PROVIDER-SPLIT resolution: `resolved_ok = (set(res) - {'diagnostic_event'}) <= defs and
   _diag_ok` (line 731).

Plus the instrumentation under step-4 scrutiny: `preproof_exception` written in the
reconciliation except (line 734).

## Harness route (unchanged across both runs)

Exec the driver verbatim (`__name__='driver_under_test'`), shim `ns['importlib']` so
`package_phase` receives a fresh REAL importlib load of the frozen `package_prx.py`
(sha == driver pin `e6fbe6c5...`) with sentinel `main`, redirect `ns['MY']` to scratch.
`package_phase` installs the ACTUAL wrapper and exits at its own `output dir exists` guard —
after wrapper install, before `sys.argv` mutation and `m.main()`. The harness then calls
`m.verify_lineage(real Build)` — the REAL frozen pipeline + REAL tools. No canned evidence,
no FakeBuild anywhere in this edition (the original harness's LLVM_CONFIG control-env cases
are obsolete: the UPDATED wrapper passes `env_extra` itself, which P1 proves under the
production environment).

GO-POSTFIX lineage honored: the l10 directory's post-fix cycle was unresolved on disk
(GO-POSTFIX-RECEIPT.txt records a crashed run on `NameError: failed_checks`); this l11
edition supersedes it; the l10 harness was not modified.

## Case results — v2 run (all real inputs; rc from Build.records ==
subprocess.run().returncode — never a piped $?)

- **P1-l11-full-real PASS** — run-l11-1 delta (added = sceKernelGetModuleInfo + 4 resolutions
  incl. diagnostic_event), real overlay (bridge d96cfe70), real staged diagnostic.c
  (f5e96c41), real tools, production env. Both pre-proof compiles executed rc=0
  (`[prospero-clang++ 0, llvm-nm 0, prospero-clang 0, llvm-nm 0]`); provider-split proven:
  bridge nm defines exactly the 3 accessors (and NOT diagnostic_event), diag nm defines
  diagnostic_event; check superseded; **the v2 fix confirmed: the supersede detail carries
  `stage_aware_reconciliation` {attempted=true, accepted=true, others_failed=[],
  delta_matched=true, preproof_compile_verified=true} — all fields readable on the success
  path**. Wall: ~0.08 s install + ~2.5 s wrapper path.
- **N-a-diag-source-missing** — `ns['ROOT']` re-pointed (after install) to a /tmp symlink farm
  lacking `worker-diagnostic-sink/staging/diagnostic.c`; the wrapper's diag pre-proof hits a
  genuinely missing file: real clang rc=1, wrapper declines (accepted=false), no supersede.
  Decline semantics PASS; **step-4 FAIL: `preproof_exception` absent from the evidence**
  (expected present) — unchanged by the v2 fix, which touched only the supersede path.
- **N-b-baseline-r13-bridge PASS** — /tmp overlay copy with the bridge swapped for the r13
  baseline (`worker-module-start-wiring/runtime_start_bridge_r13.cpp`, sha 10385e22;
  standalone pre-verified compiles rc=0 with 0 accessors). Pure definition-miss: both
  compiles rc=0, r13 defines 23 symbols but zero accessors, diag pre-proof still proves
  diagnostic_event, accepted=false, and `preproof_exception` correctly ABSENT (no exception).
- **N-c-unexpected-extra-added PASS** — added gains a 6th symbol: delta_matched=false,
  proof skipped, decline recorded.
- **N-d-second-failing-check PASS** — mutated receipt runtime_bytes (+1) over a full symlink
  farm of run-l11-1: orig verify fails exactly `composition_object_bytes` besides the delta
  check → others_failed non-empty → decline, proof skipped.
- **N-e-unexpected-5th-resolution PASS (supplementary, gates change 1)** — resolutions gain
  `harness_extra_resolution` with `added` extended to match, so ONLY the allowed-names filter
  rejects → decline. Confirms the widened list still gates unexpected names.

Total wall-clock v2: 13.03 s. Install facts (sentinel main never called, wrapper installed,
guard fired, argv untouched) held in all 6 cases in both runs; the wrapper propagated no
exception in any case (fail-closed inside).

## The remaining step-4 finding (sole NO-GO cause under af309a3a)

Empirically confirmed on N-a in both runs and explained exactly by the driver text: line 734
(in the except) writes `detail.stage_aware_reconciliation.preproof_exception = repr(_ex)[:300]`,
and line 735 then unconditionally ASSIGNS a fresh `stage_aware_reconciliation` dict
(attempted/accepted/others_failed/delta_matched/preproof_compile_verified), replacing — and
thereby erasing — the exception detail on every decline path that reaches it. The v2 fix
(af309a3a) added the reconciliation record to the SUPERSEDE detail (confirmed working via P1)
but did not touch the 734/735 interplay. The physical cause still survives in `Build.records`
(the rc=1 clang stderr: "no such file or directory: …/diagnostic.c"), but the evidence decline
record does not carry it. Secondary nit: the except's `recon = False` also flips the recorded
`delta_matched` to false on exception cases even though the delta set matched.

Minimal fix for the driver author (not applied by this worker; the driver is read-only here):
merge instead of replace at line 735 (`setdefault('stage_aware_reconciliation', {}).update({
...})`) or move the `preproof_exception` write after that assignment; optionally preserve the
pre-exception `recon` in a separate variable so `delta_matched` stays truthful. Then re-run
this harness unchanged under a NEW edition id; GO requires all 6 case verdicts PASS.

## Honest scope

- This validates the wrapper's evidence/decision logic and the dual pre-proof + provider-split
  mechanics end-to-end with real tools. It does NOT package a PRX (sentinel main; the
  packaging pipeline never ran) and makes no claim about PRX-level quality/parity.
- P1's supersede mirrors the production precedent recorded in
  `prx-candidate-l11-1/verification-evidence/lineage-verification.json` (same check,
  superseded=true, diagnostic_event in the stage_aware detail).
- The runtime_object/candidate evidence, overlay, driver, frozen pipeline, r13 bridge and
  staged diagnostic.c were consumed read-only; the l10 harness directory is byte-untouched.
