# GO-NOGO — worker-wrapper-validation-l11 (UPDATED wrapper, l11 driver)

- v1 vs driver `291d3b9a...` (2026-10-05T20:30:45Z, 12.32 s): **NO-GO** — 5/6 PASS; two
  wrapper-path evidence defects found.
- **v2 vs driver `af309a3a...` (controller supersede-detail fix; 2026-10-05T20:34:21Z,
  13.03 s): NO-GO — 5/6 PASS; the supersede-path defect is CONFIRMED FIXED (P1 passes with
  all reconciliation fields readable); the one remaining failure is the step-4
  `preproof_exception` decline-path wipe.**

## v2 criterion table

| # | criterion | result |
|---|---|---|
| 1 | POSITIVE supersedes: real l11 delta + real overlay + real toolchain through the ACTUAL wrapper; BOTH pre-proof compiles execute; rc captured from the run result object; all fields readable | **PASS** — superseded=true; supersede detail now carries `stage_aware_reconciliation` {attempted=true, accepted=true, others_failed=[], delta_matched=true, preproof_compile_verified=true}; tail rc `[prospero-clang++ 0, llvm-nm 0, prospero-clang 0, llvm-nm 0]`; provider-split nm proof (3 accessors in bridge defs; `diagnostic_event` only in diag defs); production env (wrapper's own `env_extra LLVM_CONFIG`, no harness injection) |
| 2 | N-a diag.c missing → diag proof fails, no supersede | **Decline semantics PASS** — real clang rc=1 ("no such file"), accepted=false, gap retained. Its step-4 sub-criterion FAILS (see #6) |
| 3 | N-b bridge swapped for baseline r13 (no accessors) → bridge proof fails | **PASS** — pure definition-miss: both compiles rc=0, r13 defines 0 accessors, accepted=false, no exception, `preproof_exception` correctly absent |
| 4 | N-c unexpected extra `added` symbol → decline | **PASS** — delta_matched=false, proof skipped |
| 5 | N-d second failing check → decline | **PASS** — others_failed=[composition_object_bytes], proof skipped |
| 6 | STEP 4: decline record carries `preproof_exception` on exception cases; absent on pure definition-miss | **FAIL** — still wiped on N-a under af309a3a: driver line 734 (except) writes it into `stage_aware_reconciliation`, line 735 then REPLACES that dict. Absent-on-definition-miss (N-b) holds. Supplementary N-e (5th resolution not in allowed names) passes, confirming change (1) still gates |
| 7 | Pipeline never runs (sentinel main, guard fires, argv untouched) | **PASS** — all 6 installs |

## Blocking defect (exact, minimal — the ONLY one left)

`build_runtime_composition6_l11.py` (af309a3a) lines 732-738, decline path only:

```python
except Exception as _ex:
    recon = False
    ev['checks'][_CHECK].setdefault('detail', {}).setdefault('stage_aware_reconciliation', {})['preproof_exception'] = repr(_ex)[:300]
ev['checks'][_CHECK].setdefault('detail', {})['stage_aware_reconciliation'] = {   # <-- REPLACES the dict above
    'attempted': True, 'accepted': bool(recon and resolved_ok),
    'others_failed': others_failed, 'delta_matched': bool(recon),
    'preproof_compile_verified': bool(resolved_ok)}
```

Minimal fix (either): make line 735 a `setdefault(...).update({...})` merge, or set
`preproof_exception` after line 735's assignment. Secondary fidelity nit, same lines: the
except's `recon = False` also flips the recorded `delta_matched` to false on exception cases
even when the delta set matched (N-a shows delta_matched=false).

## GO condition

Re-run this harness unchanged after a driver revision carrying the line-735 fix with a NEW
edition id; GO requires all 6 case verdicts PASS (step-4 included). Do not widen or re-rate
unchanged evidence (studio contract).
