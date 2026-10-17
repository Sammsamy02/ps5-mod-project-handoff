# APPLY-ORDER — comp6 DRY BUILD composition + packaging (worker-comp6-drybuild)

2026-10-04. Rehearsal of REPLACE-MECHANICS.md §2 with the D2 scaffold
(`outputs/2026-10-04-zcode-controller/comp6-integration/`, 21:11 state). All
paths below are relative to the workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what`. Run from the root.

## Step 0 — staging (staging-d1/, 9 files, shas recorded in STAGING-DIFFS.md)

| staged file | source | delta vs source |
|---|---|---|
| `lmf_to_native_loader_facts.cpp` | D2 scaffold (Diff A + Resolution B) | + appended `RC::Platform::comp6_drybuild::adapter_staging_marker()` (R3 marker pattern) |
| `lmf_to_native_loader_facts.hpp` | D2 scaffold (Resolution B option field) | `<NativeLoaderContext.hpp>` -> quoted + 2 `#error` include-resolution guards |
| `PsMappedImageCaller.hpp` | D2 scaffold (Diff B) | + `#define WCD_DRYBUILD_STAGING_PS_HEADER 1` + `extern const char WCD_PS_DIFF_B_STAGING_MARKER[]` |
| `OwnedMappedImageCaller.hpp` | D2 scaffold (Diff C) | `<PsMappedImageCaller.hpp>` -> quoted + `#define WCD_DRYBUILD_STAGING_OWNED_HEADER 1` + `WCD_OWNED_DIFF_C_STAGING_MARKER[]` |
| `NativeLoaderContext.hpp` | frozen `ue4ss-native-tick-file-owner-1/include` | 1 angle->quoted include conversion |
| `OwnedMappedScanSource.hpp` | frozen `ue4ss-native-tick-file-owner-1/include` | 2 angle->quoted conversions |
| `OwnedMappedCandidateSelection.hpp` | frozen `ue4ss-native-tick-file-owner-1/include` | 1 angle->quoted conversion |
| `OwnedMappedResolverCollector.hpp` | frozen `ue4ss-native-mapped-candidate-selection-1/include` | 1 angle->quoted conversion |
| `OwnedSerializedElfSource.hpp` | frozen `ue4ss-native-tick-file-owner-1/include` | 1 angle->quoted conversion |

Intercept chain (quoted hop marked `->`):

```
staging lmf_to_native_loader_facts.cpp
  -> lmf_to_native_loader_facts.hpp [staged; quoted NativeLoaderContext + guards]
  -> NativeLoaderContext.hpp [staged]
  -> OwnedMappedScanSource.hpp [staged]
       -> OwnedMappedCandidateSelection.hpp [staged]
            -> OwnedMappedResolverCollector.hpp [staged]
                 -> OwnedMappedImageCaller.hpp [STAGED D2 Diff C]
                      -> PsMappedImageCaller.hpp [STAGED D2 Diff B]
       -> OwnedSerializedElfSource.hpp [staged]
            -> OwnedMappedImageCaller.hpp [STAGED D2 Diff C, pragma-once no-op]
```

D2 headers NOT staged (configured-roots deferral, need Procedure B):
`NativeLoaderContext.cpp/.hpp` (scaffold versions), `UE4SSProgram.cpp`,
`Platform-NativeProgramContext.hpp`.

## Step 1 — driver copy (Procedure B)

`build_runtime_composition6_dry1.py` = byte-identical copy of
`outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/build_runtime_composition5.py`
(sha256 `aaf47bdaf752e62ff845e37828cda4a89a0a612d47809ab2eb99aa5a6dcd9904`)
with exactly 6 documented edits (see its docstring); copy sha256
`50e8d16c23d33e38e7989a87c0d0b9978e3e3a3eeaab0227a0f3eabae0513cbc`.
The pinned composition script
`outputs/2026-10-04-zcode-team/worker-composition-4-fix/build_runtime_composition4_fixed.py`
(sha `36cc8199…`) is executed UNMODIFIED (driver Gate 1 re-verifies it).

## Step 2 — Run A (fresh composition + packaging)

```
python3 outputs/2026-10-04-zcode-team/worker-comp6-drybuild/build_runtime_composition6_dry1.py \
  --composition-run outputs/2026-10-04-zcode-team/worker-comp6-drybuild/ue4ss-native-supplier-composition-6-dry1/run-1 \
  --out outputs/2026-10-04-zcode-team/worker-comp6-drybuild/prx-candidate-composition6-dry1-a
```

Defaults in effect: entry `worker-module-start-wiring/runtime_module_entry_r3.c`
(sha `51771ab5…`, r4 stays parked), bridge
`worker-module-start-wiring/runtime_start_bridge_r11.cpp` (sha `ebef44bf…`),
overlay `worker-comp6-drybuild/staging/lineage-overlay-dry1`.

Phase A argv = composition-5 `COMPOSE_ARGV_TAIL` verbatim except:

```
--extra-unit lmf-loader-adapter=outputs/2026-10-04-zcode-team/worker-comp6-drybuild/staging-d1/lmf_to_native_loader_facts.cpp
```

(the baseline pointed at `.superpowers/.../ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp`).
Everything else — receipts, defines, `--extend-undefined-baseline`,
`--extra-receipt`, `--replace-receipt`, bootstrap replacements — stays verbatim.

Checkpoint: `zcode_supplier_composition_6_dry1` (bumped so the real
composition-5 ledger entry is untouched; the mirror is written under THIS
worker dir only — the real transport ledger is read-only here).

## Step 2.5 — bridge r11 overlay closure (REQUIRED, new vs composition-5)

The r11 bridge TU includes far more than the composition-5-era bridge: its
packaging compile (overlay-root-only `-I`) needs 21 additional headers
flat-staged via the driver's existing `--stage-header REL=SRC` mechanism (no
driver edit). The exact 21 args are recorded verbatim in
`staging/extra-stage-headers.args` and with sha256s in
`staging/extra-stage-headers.json`. Discovery was mechanical: compile the
bridge TU syntax-only with the recorded packaging argv, stage each
`file not found` header from its spike source (port-of-record first:
`native-coherent-runtime-1/source/port/ue4ss/include` for
`Platform/*`), repeat to convergence, then confirm `-Wall -Wextra -Werror`
syntax-clean. NOTE `Platform/NativeProgramContext.hpp` has two vintages; the
PORT copy (sha `66499615…`) is the one the composition resolves and the one
staged here (NOT the older `ps-scan-provider-1` copy, sha `5ebad2f1…`).

## Step 3 — Run B (determinism double packaging, D1)

```
python3 outputs/2026-10-04-zcode-team/worker-comp6-drybuild/build_runtime_composition6_dry1.py \
  --reuse-composition-run \
  --composition-run outputs/2026-10-04-zcode-team/worker-comp6-drybuild/ue4ss-native-supplier-composition-6-dry1/run-1 \
  --overlay-dir outputs/2026-10-04-zcode-team/worker-comp6-drybuild/staging/lineage-overlay-dry1-b \
  --out outputs/2026-10-04-zcode-team/worker-comp6-drybuild/prx-candidate-composition6-dry1-b
```

(Same composition run re-verified + re-packaged with an independent overlay —
the composition5-1/-2 precedent.)

## Step 4 — gate (full 8 checks)

```
python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py \
  --build-dir outputs/2026-10-04-zcode-team/worker-comp6-drybuild/prx-candidate-composition6-dry1-a \
  --compare-dir outputs/2026-10-04-zcode-team/worker-comp6-drybuild/prx-candidate-composition6-dry1-b \
  --json outputs/2026-10-04-zcode-team/worker-comp6-drybuild/gate-receipt-composition6-dry1.json
```

## First-attempt failures (recorded honestly)

1. Background launch with relative paths — shell cwd is not the workspace root;
   re-run with absolute paths.
2. Missing intercept: `OwnedSerializedElfSource.hpp` (the 4th direct includer
   of `<OwnedMappedImageCaller.hpp>`) was not staged on the first attempt ->
   frozen twin redefinition at compile. Fixed by staging it; composition run
   dir from the failed attempt consciously deleted (partial, own dir).
3. Marker namespace braces: D2 headers close `namespace
   RC::Unreal::Platform` themselves; the appended marker blocks initially
   added an extra `}`. Caught by a syntax-only pre-flight (exact run-3
   lmf-loader-adapter argv, `-fsyntax-only`) BEFORE the re-run. The pre-flight
   pass is part of this procedure for any future revision.
4. Bridge header pin drift: the live driver's `BRIDGE_H_MEASURED_SHA`
   default (`c47f7184…`) predates the finalized `runtime_start_bridge.h`
   (18:28, `ecada77e…`). The real composition-5 build had staged the current
   content via an explicit `--stage-header` override (see
   `worker-composition-5-rebuild/staging/overlay-manifest.json`), so edit 7
   re-pins the measured current delivery in this copy only.
5. First packaging attempt failed at the bridge TU: the r11 bridge include
   closure is wider than the composition-5 `DEFAULT_STAGE_HEADERS` list
   (`live_module_base_scan.hpp` not found). Solved with Step 2.5; the failed
   candidate dir is preserved as `prx-candidate-composition6-dry1-a-failed1/`
   (GAPS.json, build-receipt.json with the failing bridge argv).
