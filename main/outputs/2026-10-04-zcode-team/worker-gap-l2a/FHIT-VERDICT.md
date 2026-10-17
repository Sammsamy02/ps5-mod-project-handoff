# FHIT-VERDICT — GAP-L2a: FHitResult out-param readback (LineTraceSingle) on PS5

Worker: worker-gap-l2a, 2026-10-05. Static evidence only (Ghidra 12.1.4 headless, project
`/tmp/wrfp_ghidra_proj/fnamepool`, program `/decrypted-eboot.bin`, imageBase=0). No console contact.
All eboot addresses are FILE vaddrs; runtime = file vaddr + 0x400000.

## Verdict summary

| Question | Verdict |
|---|---|
| Does the PS5 FHitResult member layout shift vs what the port's readback delivers? | **MATCH — by construction** (the port assumes no FHitResult layout; the one shared assumption, `FProperty::Offset_Internal` @ +0x44, is byte-verified in the eboot) |
| Is the KismetSystemLibrary ProcessEvent vtable slot 0x260 (Win64-derived) correct on PS5? | **SHIFTED +8 → 0x268** (UObject-family virtuals carry an Itanium 2-slot destructor prefix) |

## 1. What the port actually does on the readback path

The mod (`main.lua:245-269`) calls `system:LineTraceSingle(...)` with an out table `hit`, then reads
`hit.ImpactPoint` / `hit.ImpactNormal`. The port chain (`native-coherent-runtime-1/source/port`):

- `LuaUObject.cpp` `__PROSPERO__`: `Invocation.StoreOutTableReference(param_next)` (~:283), then the
  out-param loop :330-369 re-reads each out param at
  `uint8_t* data = &dynamic_unreal_function_data.data[param->GetOffset_Internal()]`,
  dispatching to `push_structproperty` (:816) → `convert_struct_to_lua_table` (:727+), which reads
  **every FHitResult member at `field->GetOffset_Internal()`** of the live child FProperties of the
  live `UScriptStruct`.
- `port/unreal/include/Unreal/FHitResult.hpp` is an **empty struct** ("Temporary empty implementation
  for FHitResult"). **The port hardcodes no FHitResult member offsets.** The mission premise ("the
  FHitResult layout the port assumes is Win64-PDB-derived") does not apply at the member level.

So the entire question reduces to: are the offsets the port reads from **live reflection** the same
offsets the game itself uses? That hinges on one Win64-PDB-derived assumption:
`FProperty::MemberOffsets["Offset_Internal"] = 0x44`
(`5_06_MemberVariableLayout_DefaultSetter_FProperty.cpp:23`, FProperty total 0x70, FField 0x30).

## 2. Eboot verification (ground truth)

### 2a. The +0x44 read is byte-verified in game code — MATCH

The game's own exec-thunk helper (FFrame::StepCompiledInRef-equivalent) at **0x4577d0**:

```
0x4577d0: PUSH RBP ; MOV RBP,RSP
0x4577d4: TEST byte ptr [RDX + 0x39], 0x1        ; RDX = live FProperty* (flag test)
0x4577da: MOV  RAX, qword ptr [RDI + 0x28]       ; RDI = FFrame* -> data base
0x4577de: MOVSXD RCX, dword ptr [RDX + 0x44]     ; <-- Offset_Internal read AT +0x44
0x4577e2: ADD  RCX, RAX                          ; data_ptr = base + Offset_Internal
0x4577e5: MOV  qword ptr [RDI + 0x38], RCX       ; Frame.MostRecentPropertyAddress
...
0x4577f6: CALL qword ptr [RCX + 0x118]           ; property virtual (Copy/Init value)
```

The game's own thunks read `Offset_Internal` at **+0x44** from the same live FProperty objects the
port reads. Both sides therefore compute identical locations for (a) the HitResult slot inside the
LineTraceSingle params buffer (UFunction param offset) and (b) every FHitResult member (child
property offset). **MATCH, by construction, statically proven at the mechanism level.**

### 2b. PS5 FHitResult envelope (corroboration, not member names)

38 inlined FHitResult ctor sites found via a store-pair scan (`mov dword[reg+0xA4],-1` within 0x50
bytes of `mov dword[reg+0xD0],-1`). Complete profile at 0x247c410 (a 2-vector trace thunk stepping
Start/End then a `HitResult` REF, writing the 0xF0-byte result to the out pointer):

- size ≥ 0xF0 (last write 0xE0..0xF0); ctor writes `{+0x00: 0.0f, +0x04: 1.0f}` (qword
  `0x3f80000000000000`), `{+0xA4: -1.0f}`, `{+0xD0: -1.0f}`, zeroes elsewhere;
- two 24-byte **double** vectors copied into `+0x70` and `+0x88` of the result (the trace
  Start/End vectors recorded into the hit);
- the exec thunk of LineTraceSingle (0x31436f0, see §4) default-constructs its `P_GET_STRUCT_REF`
  local with the same +0x04/+0xA4/+0xD0 writes — same struct.

Notably this envelope (TraceStart/End at +0x70/+0x88) does **not** match classic UE4-era Win64
layouts — a hand-transcribed FHitResult layout would have been wrong. The port avoiding any
hardcoded layout is what makes the readback robust here.

### 2c. What is NOT decided

Absolute per-name offsets of ImpactPoint/ImpactNormal were **not** pinned: UE5.6 stores reflected
property offsets in an encoded .rodata table (not a plain stride array — scan for
`{0x18,0x30,0x48,0x50,...}` at strides 4–32 found nothing), and ctor sites write zeroed members
without names. Deciders, in order of cost:
1. **Runtime log of the first ground-trace call** — the port marshals whatever live reflection
   declares; sane `ImpactPoint`/`ImpactNormal.Z` values in the log close GAP-L2a end-to-end.
2. Decoding the UE5.6 property-table encoding in .rodata (separate bounded task).

## 3. ProcessEvent vtable slot — SHIFTED +8 (0x260 → 0x268)

- The Win64-derived map (`5_06_VTableOffsets_UObject_FunctionBody.cpp`): `__vecDelDtor=0x0`,
  `ProcessEvent=0x260`, `GetFunctionCallspace=0x268`, `CallRemoteFunction=0x270`.
- Static vtable survey: 5,571 runs of ≥96 text-pointer slots in .rodata. **PS5 vtables carry a
  two-slot destructor prefix**: slot 0/1 hold adjacent paired functions per class family
  (0x29c800/0x29c810 ×943 runs, 0x1370fd/0x1370fe ×638, 0x222169/0x22216a ×188) — the Itanium
  complete/deleting-destructor pair replacing MSVC's single `__vecDelDtor`. Every UObject-family
  virtual from slot 1 on is +8 vs Win64.
- **Slot 77 (0x268)** dominant value **0x459830** (×2193 runs) is ProcessEvent by content:
  `[this+0x8]` ObjectFlags `TEST 0x40000000` guard; `TEST byte [ufunction+0xB1], 0x4` =
  FUNC_Native; virtual calls `[vtbl+0x270]` and `[vtbl+0x278]` — which are
  GetFunctionCallspace/CallRemoteFunction (Win64 0x268/0x270, i.e. exactly the same +8); then
  UFunction native-func pointer read at `+0xC8` and FFrame construction. Internal consistency:
  the function at PS5 slot 77 calls the functions at PS5 slots 78/79.
- **Slot 76 (0x260)** — the slot the port would call — is dominated by **0xb290 ×2514**: a
  near-universal empty stub (`PUSH RBP; MOV RBP,RSP; POP RBP; RET`). The port's live-check
  (`UnrealInitializer` `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UObject, ProcessEvent, Object)`)
  would resolve that stub; every UFunction call through `calling_context->ProcessEvent`
  (LuaUObject.cpp:327) would silently no-op.

Consequence: the same +8 shift applies to the other UObject-family slots the composed runtime may
hook (AActor::Tick, UGameViewportClient::Tick, ProcessConsoleExec, UStruct::Link) — **each needs its
own individual verification before enabling those hooks**; not claimed here.

## 4. LineTraceSingle recovery (how it was located)

- Reflected names are UTF-16LE in segment_1: `LineTraceSingle` ×5 (0x75f3b36, 0x76ae936, 0x78bfd46,
  0x79ac4a0, 0x7a28330), `KismetSystemLibrary` ×19, `ImpactPoint` 0x7a10640, `ImpactNormal`
  0x75d89ca, `FHitResult` ×4. All flow through the known FName ctor anchor 0x23d590.
- **0x31436f0 = execLineTraceSingle** (high confidence): 12 sequential Frame-Step reads matching the
  signature order (WorldContextObject, Start, End, TraceChannel, bTraceComplex, ActorsToIgnore,
  DrawDebugType, HitResult-REF-with-default-local, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime),
  one native call with the locals, bool result stored to the caller's out bool*. Its HitResult
  default local shows the §2b ctor writes; Start/End locals are 24-byte double vectors.
- Static FName slots: 0x952c2b0/b8, 0x952c338, 0x952c3b0/b8 (per-TU copies); FHitResult member-name
  table slots 0x95c8c88 (ImpactPoint) / 0x95c8c90 (ImpactNormal), Engine TU at 0x5d51c10.
- Game callers reading ImpactPoint were not individually pinned (see §2c); ground truth was
  established via the game's own reflection read path (§2a) and the ctor envelope.

## 5. Staged fix (bounded, proposal-level)

`staged/VTableLayout.ini` — identical to the shipped identity-only file
(`worker-loadlist-final/final-package/ue4ss/VTableLayout.ini`,
sha256 `9c20f7c9…bad86`) plus:

```ini
[UObject]
ProcessEvent=0x268
```

Mechanism: `NativeLayoutBinding.cpp` `StageNativeLayoutFiles` → `PublishStagedNativeLayoutMaps`
swaps the entry into the real `UObject::VTableLayoutMap` before caching; the 5_06 default setters
are find-guarded and will skip the published key. Constraints (also in DIFF.md):
must be staged before `InitializeVersionedContainer` (existing Stage24 seam); other UObject keys
keep wrong-but-dormant Win64 defaults (e.g. GetFunctionCallspace=0x268 now semantically collides
with PS5 ProcessEvent's slot — keep unverified hooks disabled); `offset % 8 == 0` satisfied.

**No readback source change** — verdict MATCH; `LuaUObject.cpp`, `FHitResult.hpp`, and the
FProperty member-offset defaults stay untouched. Not runtime-validated (console untouched).
