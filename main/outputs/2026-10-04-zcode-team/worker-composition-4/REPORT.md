# REPORT — worker-composition-4 (composition-4 recomposition)

**Verdict: COMPOSITION FAILED at a delivered-script gate; STOPPED per contract (documented, not forced). Packaging step never reached. No composition-4 runtime object exists.**

Task step 7 rule applied verbatim: "If composition or packaging fails, document the exact error and stop — do not force."

## 1. What succeeded (all pre-compose verification)

### Step 0 base state — all four hashes EXACT
- composition-3 object `…ue4ss-native-supplier-composition-3/run-1/coherent-tick-file-witness-runtime.o`
  = `e9679aa5b3bc3a38140c4dd4a63b566158bded3eb0a4387d4cc0e073b2d86350`, 117712568 bytes (verified, matches APPLY-ORDER).
- pinned spike initializer `…ue4ss-native-tick-initializer-retry-1/src/UnrealInitializer.cpp`
  = `66691a88…` (untouched on disk throughout; still byte-identical after the run).
- staging initializer `outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp`
  = `933d08a2…`.
- build script `integration-patch/build/build_runtime_composition4.py` = `7a83cb38…`.

### Receipt attestation — all 10 replacement/new-unit source hashes EXACT
Every `files` entry of `composition4-replace-receipt.json` and `composition4-extra-receipt.json`
that exists as a deliverable source matched on readback: NativeQueryHarness.{hpp,cpp}
(`5e3abfa7…`/`1b9cec49…`), runtime_owner.{hpp,cpp} (`5428f8c3…`/`54c56c49…`),
activated_mod_source.hpp `e7d92aaf…`, ue4ss_backend.{hpp,cpp} (`7f932a58…`/`1d58f9a5…`),
modforge_activation.h `4d79eb05…`, runtime_ack_supplier.{cpp,hpp} (`b2c8dda7…`/`509037a2…`),
staging UnrealInitializer.cpp `933d08a2…`.

### Diffs reviewed before applying
`patches/01-unreal-initializer-s2-arm.diff` (122 lines: __PROSPERO__ harness include,
QueryHarnessEngineTickDispatch, ArmLayoutGate after PublishStagedNativeLayoutMaps, two
disarm-on-throw try/catch wrappers, persistent EngineTickPreCallback registration) and
`composition4-vs-composition3-build-script.diff` (70 lines: --replace-runtime-unit + rr
fallback in the extra-unit dep loop).

### Step 1 header staging — done exactly as ordered
Five flat headers copied into `…ue4ss-native-tick-file-owner-1/include/` (all five verified
absent beforehand; post-copy hashes identical to receipt pins): NativeQueryHarness.hpp,
runtime_owner.hpp, activated_mod_source.hpp, ue4ss_backend.hpp, modforge_activation.h.
These are the ONLY files this worker added to the spike tree.

## 2. The failure — exact error and root cause

Command: the verbatim APPLY-ORDER Step 2 one-shot (full argv preserved in
`compose-log.txt` and in the run's `commands.json`).

```
Traceback (most recent call last):
  File "…/build_runtime_composition4.py", line 178, in <module>
    if __name__=='__main__':main()
  File "…/build_runtime_composition4.py", line 78, in main
    if p not in pins:raise ValueError('Unpinned fresh dependency: '+p)
ValueError: Unpinned fresh dependency: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp
```

Root cause (readback of the delivered script, hash-attested `7a83cb38…`):

- Line 40: `pins` = sealed `native-coherent-runtime-1/build_runtime.py::validate_inputs()`.
- Line 48: pins += each base unit's source. Line 52: pins += `W/include/*`, `W/src/*`
  (W = the file-owner spike tree) — snapshotted at script start.
- Lines 74–79 (fresh-unit loop): after recompiling an affected unit from
  `new_sources`, EVERY `.d` dependency must be in `pins` (line 78).
- Lines 83–89: the replace-receipt map `rr` — which pins the staging initializer
  (`933d08a2…`, receipt `files` entry) — is built only AFTER this loop, and is consulted
  only in the extra-unit dependency loop (line 104) and re-built for the bootstrap
  section (line 117).

The `--replace-runtime-unit` source therefore can never pass line 78 unless it lives
under `W/src/` or `W/include/`. The delivered design consumes the staging copy from
`outputs/…/worker-query-s2-arm/staging/` (deliberately keeping the spike tree clean),
so the delivered patch set cannot compose end-to-end as shipped. This is an ordering
defect in the generated script, not a drift or a content failure: the staging source is
attested and compiles clean (below).

Note: the receipt's provenance claim is accurate but narrower than end-to-end: the
initializer is "compile-proven under the exact affected-unit argv" — which this run
reproduced (exit 0). The untested part was precisely the post-compile pin walk.

## 3. Positive evidence salvaged from run-1 (…ue4ss-native-supplier-composition-4/run-1/)

The failure occurred after exactly ONE compile invocation (the initializer is first in
the affected order). That unit is genuine, attested S2-arm output:

- `unreal-unrealinitializer.o`: sha256 `1a8bed2a95452d57e47f08eb1eab2f5e164dee1fe9d098f3fce58dc521ae316a`,
  1859160 bytes, compile exit 0, 6 pre-existing-style warnings on stderr, no errors.
- Undefined imports prove the wiring landed: `RC::Platform::QueryHarness::Global()`,
  `Harness::ArmLayoutGate(LayoutGateFacts const&)`, `Harness::DisarmLayoutGate()`,
  `Harness::OnEngineTickDispatch()` (5 QueryHarness-referencing symbols total). These
  are exactly the symbols the harness + ack-supplier units must satisfy at final link.
- `commands.json`, `unreal-unrealinitializer.{d,o,stderr,stdout}` preserved in run-1
  (raw provenance untouched).

## 4. Why no retry, relocation, or in-place fix was attempted

- The build script is hash-attested (`7a83cb38…`); editing it breaks the attestation
  chain and is outside this worker's write scope (existing files elsewhere read-only).
- `out.mkdir(exist_ok=False)`: run-1 now exists; a rerun requires a fresh run dir —
  blind replay against run-1 is impossible and re-running under a new name would hit
  the identical line-78 gate.
- Relocating the staging initializer under `W/src/` would satisfy line 52's glob but
  diverges from the delivered replace-receipt attestation story (receipt names the
  outputs staging path) — that is a design decision for the script owner, not a
  worker-side bypass. APPLY-ORDER: "STOP and reconcile on any other result — never bypass."
- The pinned original initializer (`66691a88…`) was never touched; no frozen receipt,
  pinned file, or composition-3 artifact was modified.

## 5. Required reconciliation (for controller / worker-query-s2-arm)

Minimal, attestation-preserving fix in a NEW generated script copy (composition-5 or
fixed composition-4, new hash, new receipt note):
1. Hoist the `rr` construction (current lines 83–89) above the fresh-unit loop (line 74).
2. At line 78, accept `rr` as a fallback: `if p not in pins: if p in rr: pins[p]=rr[p]; else: raise …`
   mirroring the existing extra-unit fallback at line 104.
3. Re-run into a fresh run dir (e.g. `run-2`); run-1 is a preserved failure artifact.
Alternative design (owner's choice): stage the initializer copy under `W/src/` so the
line-52 glob pins it — but then update the replace receipt prose accordingly.

## 6. Steps NOT executed (blocked by section 2)

- Composition completion / run receipt status check (expected
  `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, 368 units, 15 fresh).
- Final runtime object hash/bytes and undefined-delta verification
  (expected added `[sceKernelGetModuleInfo, clock_gettime, getpid]`, nm-verified).
- PRX packaging `--link` against composition-4 and the 20/20 static checks.
- In-game evidence path (APPLY-ORDER Step 4) — untouched, controller duties unchanged.

## 7. Artifacts produced by this worker (write scope honored)

- `outputs/2026-10-04-zcode-team/worker-composition-4/CLAIM.json` (written first)
- `outputs/2026-10-04-zcode-team/worker-composition-4/compose-log.txt` (full stdout+stderr incl. traceback)
- `outputs/2026-10-04-zcode-team/worker-composition-4/REPORT.md` (this file, last)
- Five include/ header copies in the spike tree (Step 1, hash-verified) and the
  `…ue4ss-native-supplier-composition-4/run-1/` partial build dir (script-written).
