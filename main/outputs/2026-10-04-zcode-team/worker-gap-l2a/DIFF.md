# DIFF.md — worker-gap-l2a staged fix (PROPOSAL-LEVEL)

## Scope

Exactly one staged artifact: `staged/VTableLayout.ini`. **No source diff** — the FHitResult
readback verdict is MATCH; `LuaUObject.cpp`, `FHitResult.hpp`, and the 5_06 member-offset defaults
are intentionally untouched.

## Baseline

`outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/ue4ss/VTableLayout.ini`
(the runtime VTableLayout pair shipped with the loadlist-final package; launch-10-final-b freshens
{cppmod,luamod,settings}, not this file):

```
sha256 9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86
```

Content (identity-only, 10 lines):

```ini
[NativeLayout]
Format=1
Target=prospero-x86_64-sysv
PointerSize=8
EngineMajor=5
EngineMinor=6
NonShipping=0
Stats=0
EngineRevision=5.6.1-0+++UE5
BuildId=04bb4330a7d71dcefe1cc2c4d19769d5
```

## Staged candidate

`staged/VTableLayout.ini`:

```
sha256 37a7d86036106d1042c10c10e11f66edef4ba7c2b2a2948c8aaeea719ee9a8a5
```

```diff
 [NativeLayout]
 Format=1
 Target=prospero-x86_64-sysv
 PointerSize=8
 EngineMajor=5
 EngineMinor=6
 NonShipping=0
 Stats=0
 EngineRevision=5.6.1-0+++UE5
 BuildId=04bb4330a7d71dcefe1cc2c4d19769d5
+
+[UObject]
+ProcessEvent=0x268
```

(+4 lines, +1 entry.) Identity header preserved byte-exact.

## Why 0x268

Win64-PDB-derived `ProcessEvent=0x260` assumes MSVC's single `__vecDelDtor` slot at vtable+0x0.
The eboot's UObject-family vtables (5,571 runs analyzed) carry the Itanium two-slot
complete/deleting destructor pair at slots 0/1 (adjacent paired functions per family, e.g.
0x29c800/0x29c810 ×943 runs), shifting every later virtual +8. The function at PS5 slot 77
(byte offset 0x268, address 0x459830, ×2193 runs) is ProcessEvent by content (ObjectFlags guard,
FUNC_Native test at `[ufunction+0xB1]&0x4`, internal virtual calls at `[vtbl+0x270]`/`[vtbl+0x278]`
= GetFunctionCallspace/CallRemoteFunction — themselves the +8-shifted Win64 0x268/0x270).
The Win64 slot 0x260 holds a near-universal empty stub (0xb290 ×2514 runs) on PS5.

## Parser/validator constraints (from ue4ss/src/NativeLayoutBinding.cpp)

- `StageNativeLayoutBundle` requires: `format=1`, `target=prospero-x86_64-sysv`,
  `pointer_size=8`, `engine_major>=4`, non-empty `engine_revision`/`build_id`;
  vtable entries need `offset % 8 == 0` (0x268 ✓) and `offset <= INT32_MAX` ✓.
- `CheckEmpty`: the real `UObject::VTableLayoutMap` must still be empty when staged — stage BEFORE
  `InitializeVersionedContainer` (`set_virtual_offsets()`), i.e. through the existing Stage24
  staging seam ordering.
- `PublishStagedNativeLayoutMaps` cross-checks identity vs resolved engine facts
  (5.6, non_shipping=0, stats=0 — matches the header).
- The 5_06 default setters are `find`-guarded: a published `ProcessEvent` key is skipped, so the
  corrected value survives `set_virtual_offsets()`.

## Known residuals after this fix (documented, NOT fixed here)

- All other UObject vtable keys keep Win64 defaults: wrong-but-dormant while their hooks are
  disabled. `GetFunctionCallspace` default 0x268 now numerically collides with PS5 ProcessEvent's
  slot — `bHookProcessConsoleExec` and any GetFunctionCallspace/CallRemoteFunction consumer must
  stay off until their PS5 slots are individually verified (expected +8: 0x270/0x278 — consistent
  with the calls observed inside 0x459830, but not individually pinned by a slot histogram here).
- The same +8 shift presumably applies to AActor::Tick / UGameViewportClient::Tick /
  UStruct::Link slots consumed by the initializer — NOT verified per-slot; do not enable.
- Full UObject vtable map correction is a separate bounded task.

## Acceptance path (not run by this worker)

1. Parser dry-run of the staged pair through the vendored `parser_verify` harness
   (worker-staged-pair-emission/vendored/worker-layout-batch3/dryrun/).
2. Emission-gate integration via `assemble_staged_pair_v2.py` flow or direct staged-pair
   replacement at the next gated rebuild (launch-10 rebuild rules apply; this file must NOT be
   staged to the console outside the fixed order).
3. Runtime proof: first `[Lua]` ground-trace call log after the fix lands (also closes the
   remaining absolute-offset unknown, see FHIT-VERDICT §2c).
