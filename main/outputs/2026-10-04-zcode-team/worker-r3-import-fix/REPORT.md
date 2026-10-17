# REPORT — worker-r3-import-fix (2026-10-06)

Mode: build-system analysis + fix verification. LOCAL ONLY. One-TU compiles with
the pinned production argv (only `-o`/`-MF`/source redirected into this worker's
dir) and local `ld -r` pair probes as analysis artifacts. No shared-lineage edits,
no production composition run, no packaging, no console contact, no game claims.
All deliverables under `outputs/2026-10-04-zcode-team/worker-r3-import-fix/`.

## 1. Root cause (with hard evidence)

The composed-object U-symbol observation is REAL but belongs to a specific
lineage window, and it is an **initializer lineage mismatch**, not a preprocessor
or link-order problem in the current tree.

**Failing artifacts (forensics, unchanged):**
`ue4ss-native-supplier-composition-6/run-l11-1-aside-{1791261226,1791261432,
1791261796→1791261596,1791261854}` — four aside runs whose composed
`coherent-tick-file-witness-runtime.o` show exactly

```
_ZN2RC6Unreal20R3MarshalPumpPendingEv              U
_ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv U
```

(extern-only undefined view; measured in all four). Their `unreal-unrealinitializer.o`
(sha1 `69125dcb…`) contains **ZERO** R3Marshal symbols. The aside-1226 receipt
records the cause precisely:

- initializer source sha256 `d245bffb…` — the EDITIONS-edit-19 IO-controls
  initializer, derived from the `4a2b9400` base which **lacks the entire R3
  marshal implementation** (controller edit `21_initializer_r3_repair` documents
  exactly this);
- luamod source sha256 `facf993a…` (the r3-ladder+gate5 merge) whose TU carries
  the two consumer references;
- the run's `undefined_delta.providers` maps BOTH R3Marshal symbols to
  `<linker-provided>` — i.e. that run's script edition had been deflected by
  adding the R3Marshal names to `linker_provided`, which allowed a composed
  object with unresolved imports to produce a receipt (runtime `cd5c7bc2…`).

With no definition in any linked TU, `ld -r` correctly keeps the references
undefined. `UFunction::GetFuncPtr` never appeared as composed-U because the
untouched unreal-ufunction unit defines it.

**Mechanism confirmed by negative control (this worker):** compiling the CURRENT
initializer with `-DUE4SS_R3_MARSHAL_MODE=0` produces an object with **0**
R3Marshal symbols (`verify/unreal-unrealinitializer.DISARMED.o`), and the
disarmed pair-link reproduces the incident byte-shape — both R3Marshal symbols U
(`verify/nm-pair-disarmed-und.txt`). Any initializer TU without the armed fold
(wrong lineage OR disarming define) reproduces the failure; the LuaMod side is
unconditional `__PROSPERO__` code, so the pairing is linker-enforced by design
(ARM-DISARM 2.3).

## 2. Why the current tree is NOT broken (guards verified)

- `launch11-combined/units/UnrealInitializer.cpp` (sha256 `782dce71…`, the
  capture-first fix on the edit-21 repaired lineage) **arms in-file**:
  `#ifndef UE4SS_R3_MARSHAL_MODE / #define UE4SS_R3_MARSHAL_MODE 1` at lines
  178-180. No `-D` is required; the production argv (190 args, run-l11-1
  `commands.json`, name `unreal-unrealinitializer`) defines none — correct.
- The definitions exist: `R3MarshalPublishGameThreadIdentity` (:2543),
  `R3MarshalQueryHarnessDispatch` (:2559), `R3MarshalPumpPending` (:2597), under
  `#if defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE` (:2532).
- `__PROSPERO__` is predefined by the target (probe:
  `prospero-clang++ -E -dM` → `#define __PROSPERO__ 1`;
  `verify/prospero-macro-probe.txt`), matching worker-l11-marshal-integration's
  probe receipt.
- The initializer `.o` **is** an `ld -r` input: the fixed edition builds
  `inputs` from the base unit list (`originals`), applies the
  `replacements` map for recompiled units, and links
  `ld.lld -m elf_x86_64 -r *inputs -Map=… -o coherent-tick-file-witness-runtime.o`
  (build_runtime_composition6_fixed_l11.py:269-275). run-l11-1's
  `input-hashes.json` includes the initializer object; its `runtime.map` lists
  the initializer archive member.

## 3. Fix

The fix is **already landed and verified**: keep the initializer on the
R3-carrying lineage (controller edit `21_initializer_r3_repair` → capture-first
`782dce71`). No source change is needed or made by this worker. What this worker
adds:

1. **End-to-end proof for the merged LuaMod** (`facf993a`) against the CURRENT
   initializer — the exact pairing that unlocks the next candidate — see §4.
2. **Fail-closed guard confirmed on the CURRENT scripts**: the fixed edition's
   `linker_provided` set contains NO R3Marshal names
   (build_runtime_composition6_fixed_l11.py:283), so a R3-less initializer
   compose now dies at the undefined-delta gate
   (`Unexpected native import change` / `Extra-unit imports not declared by any
   pinned SDK library`) instead of shipping a U-symbol object; and
   `launch11_one_pass.py::consumer_und_gate` (:228-258) independently requires
   the three consumer UNDs to resolve and dies on ANY residual `R3Marshal*` UND
   (`PAIRING-LAW GATE FAILED`). The deflected `<linker-provided>` tagging that
   let the failing asides through exists only in those aside runs' script
   editions; the on-disk current scripts are clean (grepped).
3. Recommendation (no edit made; controller-owned file): never re-add
   `R3Marshal*` to `linker_provided`; these are internal bridges, not PRX
   imports — their presence in the composed UND set must always be fatal.

## 4. Verification evidence (nm output, T not U)

All commands used the production argv pinned in run-l11-1 `commands.json`
(deterministic compiles; only source/-o/-MF redirected).

**(a) Initializer one-TU compile (782dce71, production argv):** exit 0,
6 pre-existing warnings, 0 errors. Object `verify/unreal-unrealinitializer.r3fix.o`
is **byte-identical** to the production object (sha256
`5296d39a4c10123c212a8a0ec82d61226b322f4eaf257bd419b244e9fdacc2f6`, equal to
run-l11-1/run-l11-2 receipt pins):

```
0000000000000000 b _ZN2RC6Unreal17UnrealInitializer13R3MarshalGateL12pump_pendingE
0000000000000000 b _ZN2RC6Unreal17UnrealInitializer13R3MarshalGateL18refused_site_countE
0000000000000000 t _ZN2RC6Unreal17UnrealInitializer13R3MarshalGateL24refuse_registration_siteEPKw
0000000000000000 T _ZN2RC6Unreal20R3MarshalPumpPendingEv
0000000000000000 T _ZN2RC6Unreal29R3MarshalQueryHarnessDispatchEv
0000000000000000 T _ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv
0000000000000000 b _ZZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEvE18logged_unavailable
```

**(b) Merged LuaMod one-TU compile (facf993a, pinned luamod argv):** exit 0,
23 pre-existing warnings, 0 errors. Object sha256 `37eece2d0047d0db…` —
**byte-identical to the EDITIONS-edit-14 recorded comparator for facf993a**
(`verify/nm-luamod-facf993a-r3marshal.txt`):

```
                 U _ZN2RC6Unreal20R3MarshalPumpPendingEv
                 U _ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv
                 U _ZN2RC6Unreal9UFunction10GetFuncPtrEv
```

**(c) Relocatable pair compose (production link recipe
`ld.lld -m elf_x86_64 -r initializer.o luamod.o`):**
`verify/pair-composed.r3fix.o`:

```
0000000000000000 T _ZN2RC6Unreal20R3MarshalPumpPendingEv
0000000000000000 T _ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv
0000000000000000 T _ZN2RC6Unreal29R3MarshalQueryHarnessDispatchEv
                 U _ZN2RC6Unreal9UFunction10GetFuncPtrEv   <- pair only; see (e)
```

Gate view (`llvm-nm --extern-only --undefined-only`): **0** R3Marshal entries.
Reverse link order (luamod first) is identical — link order is not a factor.
GetFuncPtr stays U in the two-object probe because its defining unit
(unreal-ufunction) is absent from the pair by construction.

**(d) Negative control (disarmed fold):** `-DUE4SS_R3_MARSHAL_MODE=0`
initializer object: 0 R3Marshal symbols; pair link shows both bridges as U —
reproduces the failing asides exactly (`verify/nm-pair-disarmed-und.txt`).

**(e) Production composed objects (current on-disk runs, c92c92ff luamod):**
run-l11-1 and run-l11-2 `coherent-tick-file-witness-runtime.o` are
byte-identical to each other and to their receipts (sha256
`6f215d89216105e9546a668d26f4d4b28acff604a60b4f3817c0f9e347dfb137`, the
"GO ×2, determinism 6f215d89" state) and show `R3MarshalPumpPending`,
`R3MarshalPublishGameThreadIdentity`, `R3MarshalQueryHarnessDispatch` as **T**,
`UFunction::GetFuncPtr` as **T**, with 0 R3Marshal UND — and
`launch11-combined/receipts/consumer-und-resolution.json` records
`unresolved: [], r3marshal_undefined: []` for run-l11-2.

## 5. Boundaries

- No edits outside `outputs/2026-10-04-zcode-team/worker-r3-import-fix/`.
- Shared sources read as-is (hashes recorded in `verify/VERDICT.json`); the two
  staging LuaMods re-hash to their pinned values (`c92c92ff…`, `facf993a…`).
- The pair-composed objects in `verify/` are analysis artifacts of a two-unit
  `ld -r`, not the production composed runtime; the next production admission of
  facf993a remains the controller's serialized compose (which now demonstrably
  resolves: this exact initializer object + this exact merged LuaMod object are
  the inputs proven in (c), and the full unit set resolves GetFuncPtr per (e)).
- No console, no packaging, no game-behavior claims of any kind.
