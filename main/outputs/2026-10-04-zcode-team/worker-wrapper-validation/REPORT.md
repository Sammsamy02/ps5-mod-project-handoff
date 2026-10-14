# REPORT — worker-wrapper-validation

Validation of the stage-aware delta reconciliation wrapper
(`_verify_lineage_stage_aware`, driver
`outputs/2026-10-04-zcode-controller/launch10-build/build_runtime_composition6_l10.py`
lines 600-645) ahead of the launch-10 full pass. Verdict: **NO-GO** — one exact
one-line defect (missing `LLVM_CONFIG` env on the pre-proof compile), everything
else proven correct on the real artifacts. Details: GO-NOGO.md; per-case data:
RUN-RECEIPT.md; machine-readable: CLAIM.json, harness-results.json.

## 1. Harness route and faithfulness (task 1)

Route: exec `package_phase` wholesale, made safe by three bounded substitutions:

1. The driver file is exec'd verbatim (its `main()` guarded off via
   `__name__='driver_under_test'`). All top-level constants (ROOT, PKG_FIXED,
   pins) are the driver's own; preflight re-measures them.
2. `importlib` in the driver namespace is shimmed so `package_phase`'s
   `spec_from_file_location('package_prx_fixed', PKG_FIXED)` →
   `module_from_spec` → `exec_module` sequence receives a module that is a
   FRESH REAL importlib load of the same sha-pinned frozen `package_prx.py`.
   Only two attributes differ, on the private instance: `main` → sentinel
   (records invocation; asserted never called — proves zero pipeline
   execution), and for canned-evidence cases only `verify_lineage` → a canned
   function that deep-copies the REAL pass-12 evidence
   (prx-candidate-l10-1/verification-evidence/lineage-verification.json,
   33 checks, exactly one failing) and reproduces the real `check()` gap
   recording (package_prx.py lines 271-276). `Build`, `CLANG_FLAGS`, `PIN`,
   `BOOTSTRAP_UNITS` are the real frozen objects, so the driver's in-memory
   patches (`_cpp20_run` at lines 560-566, BOOTSTRAP_UNITS entry/bridge/diag
   edits at lines 568-587, PIN adjustments at 646-652) land exactly as in
   production.
3. `MY` is redirected into /tmp scratch so `package_phase`'s sole write (the
   transport-ledger mirror, line 551) never touches worker-comp6-driver.

`package_phase` is called with the REAL run dir
(`…/ue4ss-native-supplier-composition-6/run-l10-1`), the REAL overlay
(`worker-comp6-driver/staging/lineage-overlay-2`) and a pre-created scratch out
dir, so it executes its real preflight (sha gates, receipt status check,
ledger read), loads/patches the real module, installs the ACTUAL wrapper
(closure over real `m`, real `overlay` param), and then exits at its own
`out.exists()` guard (line 654) — strictly after wrapper installation and
strictly before `sys.argv` mutation / `m.main()` (lines 656-660). The harness
asserts per case: sentinel not invoked, wrapper identity replaced
(`m.verify_lineage is not original`), guard message, `sys.argv` untouched.

The wrapper is then driven through the real code path: either the REAL frozen
`Build` (real subprocesses — P1/P1b/P2/P2b) or a `FakeBuild` subclass of the
real Build that records argv, replicates `Build.run`'s record/raise semantics
(package_prx.py lines 220-234) AND the driver's `-std=c++20` injection
(lines 560-565) with canned rc (negatives).

Why faithful: the executed wrapper text is the driver's own; it is installed by
the driver's own code with real closure state; the module it closes over is a
real load of the frozen packaging script carrying the driver's real patches.
The only fabrications are (a) which module object receives those patches, (b)
the sentinel main that is provably never reached, (c) the redirected mirror
write, (d) explicitly-labeled canned evidence/rc in the cases that need control.

Reuse of prior-run artifacts (no rebuilds): pass-12 run
`run-l10-1/undefined-delta.json` (added = 3 spanpub accessors +
`sceKernelGetModuleInfo`; `expected_bootstrap_resolutions` = the 3 accessors),
failed candidate `prx-candidate-l10-1/verification-evidence/lineage-verification.json`,
overlay `lineage-overlay-2` (bridge sha 663e7ce9… = repaired
`final-units/runtime_start_bridge_r14_diag_spanpub.cpp`; `SeedValidation.hpp`
staged).

## 2. Results (tasks 2-3)

10/10 harness cases pass; verdicts in RUN-RECEIPT.md. Highlights:

- POSITIVE (P1b/P2b): with the toolchain env corrected, the real
  `prospero-clang++ -std=c++20 $CLANG_FLAGS -I<overlay×4> -c <overlay
  bridge>` runs clean and `llvm-nm --extern-only --defined-only --format=posix`
  shows `bridge_capture_envelope T`, `bridge_readable_span_at T`,
  `bridge_readable_span_count T`. The wrapper then sets
  `checks[undefined_delta_added_matches_checkpoint_claim] =
  {pass: True, superseded: True, detail: {original…, stage_aware…, policy…}}`
  — both results recorded — and clears the gap (`b.gaps` emptied). Wall-clocks:
  8.9s full-real (includes the real frozen verify_lineage over the 117MB
  composed object + readelf + compile + nm), 2.8s canned-evidence.
- NEGATIVES: failed-nm and failed-compile decline with the gap retained;
  second-failing-check, unexpected-4th-added-symbol, empty
  expected_bootstrap_resolutions and bridge-missing decline WITHOUT any compile
  attempt (asserted: zero b.run invocations). No case ever superseded; every
  declined case keeps `lineage check failed: undefined_delta_added_matches_checkpoint_claim`
  in `b.gaps` exactly as the original check left it.

## 3. Order confirmation (task 4) — claim holds

- `package_prx.py` `main()`: `evidence = verify_lineage(b)` at line 1016 →
  evidence written at 1017 → link only under `if args.link:` with no gaps,
  `packaging = link_pipeline(b)` at line 1024. First compilation is link
  stage 3.1 (lines 645-671; compiler `b.run` at line 667). `verify_lineage`
  itself only invokes llvm-readelf (line 415 `b.run([reader, "-h", obj],
  allow_fail=True)`; line 425 `readelf_undefined`). So the wrapper's one-TU
  compile (driver lines 627-633), running inside verify_lineage, is the FIRST
  compile of the build — the pre-build proof is genuinely pre-build.
- PRX existence: bare-original (lines 720-724), bare-sdk (743-747), candidate
  (750-753) all written after verify_lineage → the wrapper runs strictly
  before the PRX exists. Driver entry: `package_phase` ends
  `raise SystemExit(m.main())` at line 660.
- Flags: wrapper line 631 `*m.CLANG_FLAGS`; bootstrap line 667 `*CLANG_FLAGS`
  — the same frozen list (lines 140-142, deliberately `-std`-free), and BOTH
  paths get `-std=c++20` injected by the driver's `_cpp20_run` patch
  (lines 560-566; both argv[0]s end `prospero-clang++` and contain `-c`).
- Includes: bootstrap (lines 665-666) = `PIN['lineage']` generated / probe /
  lifecycle / root + `PIN['lua_include']`; wrapper (line 629) = the same four
  overlay dirs WITHOUT `lua_include`; `PIN['lineage'] = overlay` (driver line
  646) is the same directory the wrapper hardcodes at line 624 in the current
  flow. Divergences: `lua_include` omission (immaterial — the bridge's include
  list, verified, contains no Lua headers; safe/fail-closed direction) and the
  `LLVM_CONFIG` env omission (the blocker — see GO-NOGO.md).

## 4. Exception behavior + evidence requirement (task 5)

The wrapper swallows every exception in the reconciliation block into
`recon = False` (driver lines 636-637: bare `except Exception`). As a fail-closed
policy this is acceptable for the GO: any surprise (missing file, tool failure,
assert) can only prevent supersession, never cause it — N-a/N-b/N-f demonstrate
it live, and P1 demonstrates it for a real toolchain failure.

However, the decline is completely silent: in every case measured,
`decline_record_present=False` — the returned evidence keeps only the original
failing check, and the build receipt's GAPS.json would show the bare lineage
gap with zero trace that stage-aware reconciliation was attempted or why it was
declined. That violates the evidence-records-the-decline requirement. Proposed
(NOT applied) one-line addition, inserted immediately before driver line 638:

```python
            ev['checks'][_CHECK].setdefault('detail', {})['stage_aware_reconciliation'] = {'attempted': True, 'accepted': bool(recon and resolved_ok), 'others_failed': others_failed, 'delta_matched': bool(recon), 'preproof_compile_verified': bool(resolved_ok)}
```

Placement rationale: control flow reaches this point only when the delta check
failed (the outer guard at line 606), so `attempted: True` is always truthful,
and the four booleans separate every decline cause (other failures / delta
mismatch / pre-proof failure). A compile exception currently loses its stderr —
if the controller wants the reason too, appending
`'error': repr(sys.exc_info()[1])` via an `except Exception as _exc` rebind is
the minimal variant, but the one-liner above satisfies the requirement as stated.

## 5. Latent findings (no GO impact)

- `_lin` (driver line 623) is dead code.
- Wrapper compiles the overlay-PARAM source (line 625) against the HARDCODED
  overlay include dirs (line 624) — identical today because `build_overlay`
  pins exactly `MY/staging/lineage-overlay-2` and refuses reuse (driver lines
  466-468, 511-517); a future divergence could only spuriously fail the
  pre-proof or compile against stale headers — never a wrong supersession
  (the real bootstrap compile still gates the link).
- Wrapper overhead per pass: one extra -O1 -Werror bridge compile + one nm
  (≈2-6s measured), serialized with everything else.

## 6. Isolation

Writes confined to /tmp/wrapper-validation-scratch and
`outputs/2026-10-04-zcode-team/worker-wrapper-validation/`. No writes to any
worker dir, run dir, candidate dir, staging overlay, or frozen source. Real
artifacts (117MB composed object, 30MB frozen PRX, receipts, contract) accessed
read-only. No console contact, no target execution.
