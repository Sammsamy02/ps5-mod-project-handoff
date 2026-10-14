# GO-NOGO — launch-10 stage-aware delta reconciliation wrapper

## VERDICT: NO-GO (one exact, one-line defect — fix specified below; wrapper logic otherwise proven correct on the real artifacts)

Date: 2026-10-05. Worker: worker-wrapper-validation. Validation only; nothing
applied to the driver or any frozen source.

## The defect (measured, not inferred)

The wrapper's pre-build one-TU proof compile (driver lines 630-631) calls
`b.run([… prospero-clang++ …])` WITHOUT the `LLVM_CONFIG` environment that every
prospero toolchain invocation in the frozen pipeline passes
(`env_extra={"LLVM_CONFIG": str(PIN["llvm"] / "llvm-config")}` — package_prx.py
line 650, used at lines 667-668).

`prospero-clang++` is a bash script that execs `${SCRIPT_DIR}/clang++`
(line 45); `bin/clang++` derives the real binary via
`prospero-llvm-config --bindir` (line 21), which requires `$LLVM_CONFIG` (or a
PATH `llvm-config`). Without it the exec becomes `/llvm-clang++` and fails:

```
bin/clang++: line 45: /llvm-clang++: No such file or directory
exec: /llvm-clang++: cannot execute: No such file or directory   (rc=126)
```

`Build.run` raises on rc!=0 (package_prx.py lines 220-234); the wrapper's
`except Exception: recon = False` (driver lines 636-637) declines silently.

Consequence: under the production environment the wrapper can NEVER supersede.
The launch-10 full pass would fail at lineage with exactly the
prx-candidate-l10-1 gap (`undefined_delta_added_matches_checkpoint_claim`) and
never link. The failure direction is fail-closed (no wrong acceptance), but the
build stays blocked.

## Evidence

Harness (harness.py) drove the ACTUAL wrapper — installed by the driver's own
`package_phase` code — through the real code path with real artifacts:
10/10 cases pass, including the two that pin the defect:

- P1 (production env, REAL frozen verify_lineage, REAL tools, real run-l10-1
  delta, real overlay 663e7ce9…): compile rc=126, superseded=False, gap
  retained. Fail-closed confirmed. wall 4.76s.
- P1b (P1 + LLVM_CONFIG injected into the process env as a CONTROL): compile
  rc=0, `llvm-nm` shows all three accessors DEFINED global (`T`):
  superseded=True, gaps cleared, evidence records both original and stage_aware
  results. wall 8.9s. This control isolates the missing `env_extra` as the ONLY
  blocker — the reconciliation logic, nm parsing, gap clearing and supersession
  detail all work on the real artifacts.
- All six negative cases (failed-nm, failed-compile, second failing check,
  unexpected 4th added symbol, empty expected_bootstrap_resolutions, bridge
  source missing) decline correctly, retain the gap, and skip the compile where
  expected. wall ≤0.002s each.

Full per-case table + wall-clocks: RUN-RECEIPT.md. Raw: harness-results.json.

## The fix (one line, in the DRIVER — controller applies; I did not touch it)

Driver lines 630-631, the wrapper's compile call — add the same env the frozen
bootstrap compile uses:

```python
b.run([str(ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++'),
       *m.CLANG_FLAGS, *_inc, '-c', str(_src), '-o', str(_obj)],
      env_extra={'LLVM_CONFIG': str(m.PIN['llvm'] / 'llvm-config')})
```

(The `llvm-nm` call needs no env change.)

## Required with the fix (task 5 finding)

The wrapper swallows exceptions into `recon=False` — acceptable fail-closed
behavior — BUT it records nothing about attempted-and-declined reconciliation
(`decline_record_present=False` in every case; a declined build shows only the
bare lineage gap with zero trace of why). Required one-line evidence addition,
inserted immediately before driver line 638 (`if recon and resolved_ok:`) —
proposed, NOT applied:

```python
            ev['checks'][_CHECK].setdefault('detail', {})['stage_aware_reconciliation'] = {'attempted': True, 'accepted': bool(recon and resolved_ok), 'others_failed': others_failed, 'delta_matched': bool(recon), 'preproof_compile_verified': bool(resolved_ok)}
```

This single statement covers every decline path (others_failed, delta mismatch,
missing bridge assertion, compile/nm failure) and executes only when the delta
check failed, so `attempted` is always meaningful.

## Order-confirmation re-verified from source (task 4) — holds

- `verify_lineage` runs BEFORE any compilation of the build:
  package_prx.py line 1016 (`evidence = verify_lineage(b)`) precedes line 1024
  (`packaging = link_pipeline(b)`); the first compiler invocations are link
  stage 3.1 (lines 645-671, compiler `b.run` at line 667). verify_lineage itself
  only runs llvm-readelf (lines 415, 425). The wrapper's one-TU compile
  (driver lines 627-633) is therefore the first compile of the whole build.
- …and BEFORE the PRX exists: bare-original (lines 720-724), bare-sdk
  (743-747) and the candidate (750-753) are all written after verify_lineage.
- Flags/include comparison: same frozen `CLANG_FLAGS` object on both paths
  (driver line 631 vs package_prx.py line 667; list at lines 140-142), both get
  `-std=c++20` injected via `_cpp20_run` (driver lines 560-566). Include sets
  match (overlay generated/probe/lifecycle/root) EXCEPT the wrapper omits
  `PIN['lua_include']` — immaterial for this TU (the bridge includes no Lua
  headers; verified) and in the safe (fail-closed) direction. The env omission
  above is the one divergence that matters.

## Post-fix GO procedure

1. Controller applies the env_extra line (and, per policy decision, the
   evidence one-liner).
2. Re-run `python3 harness.py` with P1/P2 `expect_superseded` flipped to True
   (with the fix the driver carries LLVM_CONFIG itself, so the production-env
   cases must supersede). Expect `OVERALL: ALL-CASES-PASS` with superseded=True
   and gaps cleared in P1.
3. Only then proceed to the controller rebuild for the full pass.

## Latent notes (no action required for GO)

- `_lin` (driver line 623) is dead code (assigned, never used).
- The wrapper compiles the overlay-PARAM bridge source (line 625) against the
  HARDCODED overlay include dirs (line 624). Identical today
  (`build_overlay` pins exactly that path and refuses reuse, driver lines
  466-468). A future divergence could only make the pre-proof spuriously fail
  or compile against stale headers — it can never cause a wrong supersession,
  because the real bootstrap compile still gates the actual link.
- Package-phase peak cost of the wrapper: one extra -O1 -Werror compile +
  one nm (≈2-6s observed), acceptable inside the serialized 16GB-Mac budget.
