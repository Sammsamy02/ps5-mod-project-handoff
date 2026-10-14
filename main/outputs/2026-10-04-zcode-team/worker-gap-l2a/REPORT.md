# REPORT — worker-gap-l2a (GAP-L2a: FHitResult readback + ProcessEvent slot)

Worker: worker-gap-l2a, 2026-10-05. Static-only mission on the analyzed eboot project. Budget:
19 Ghidra headless postScript runs (`/tmp/wrfp_spawn_trace/GapL2A1..19.java`, artifacts +
run logs in `/tmp/wrfp_gap_l2a/`). No console contact, no game writes, no dispatches.

## Deliverables

- `CLAIM.json` — bounded claims with evidence addresses and non-claims.
- `FHIT-VERDICT.md` — full verdict + evidence.
- `staged/VTableLayout.ini` (sha256 `37a7d860…e9a8a5`) + `DIFF.md` — the bounded staged fix.

## Headline verdicts

1. **FHitResult out-param readback: MATCH (by construction).** The port assumes no FHitResult
   member layout at all (`FHitResult.hpp` is an empty stub); the readback is fully reflection-driven
   (`param->GetOffset_Internal()` for the out-param slot, child-property offsets for members). The
   one shared assumption — `FProperty::Offset_Internal` at +0x44 within the FField object — is
   byte-verified in the eboot: the game's own thunk helper at 0x4577d0 executes
   `MOVSXD RCX,[RDX+0x44]` against live FProperty objects (0x4577de). Game and port read the same
   field of the same objects, so hit.ImpactPoint/ImpactNormal land correctly **provided** the
   ProcessEvent call itself works — which it currently does not (verdict 2).
2. **ProcessEvent vtable slot: SHIFTED +8.** PS5 UObject vtables have an Itanium two-slot dtor
   prefix (verified: adjacent paired functions at slots 0/1 across 5,571 vtable runs). PS5
   ProcessEvent = slot 77 = **0x268** (0x459830, ×2193 runs, identified by content: ObjectFlags
   guard, FUNC_Native test at [ufunction+0xB1]&0x4, internal calls to [vtbl+0x270]/[vtbl+0x278] =
   the +8-shifted GetFunctionCallspace/CallRemoteFunction). The Win64-derived 0x260 holds a
   near-universal empty stub (0xb290 ×2514) — the port's live-check would resolve and call a no-op.
   This is the first static closure of the "no constant slot shift is established" gap flagged in
   outputs/2026-10-02-ue4ss-native-core-port.md ("Remaining native engine-layout gap").

## What was recovered about LineTraceSingle

- Reflected names are UTF-16LE in .rodata segment 1 (ASCII search finds nothing — this cost the
  first three scans). `LineTraceSingle` ×5, `KismetSystemLibrary` ×19, `ImpactPoint` 0x7a10640,
  `ImpactNormal` 0x75d89ca, `FHitResult` ×4; all FName constructions route through the known anchor
  0x23d590.
- `0x31436f0 = execLineTraceSingle` (high confidence): 12 sequential Frame-Step param reads in
  signature order with a `P_GET_STRUCT_REF` FHitResult default local and bool result written to the
  caller's out pointer. FHitResult default-ctor envelope on PS5 (38 inlined sites): 0xF0-byte
  struct, `{+0x00:0.0f, +0x04:1.0f}`, `-1.0f` at +0xA4 and +0xD0, and 24-byte double-vector copies
  at +0x70/+0x88 — an envelope that does NOT match classic UE4-era Win64 layouts (hand-transcribed
  layouts would have been wrong).
- Static FName slots for LineTraceSingle (0x952c2b0/b8, 0x952c338, 0x952c3b0/b8) and the FHitResult
  member-name table (ImpactPoint 0x95c8c88 / ImpactNormal 0x95c8c90, Engine TU 0x5d51c10) recorded
  for future workers.

## Honest limits

- **Absolute ImpactPoint/ImpactNormal offsets NOT pinned.** UE5.6 encodes reflected property
  offsets in a .rodata table that was not decoded here (a plain-stride scan for classic offset
  sequences found nothing), and ctor sites zero members without naming them. Deciders: (a) the
  runtime log of the first ground-trace call (cheapest — the mod already logs its spawn stages),
  (b) decoding the 5.6 property-table encoding. Claimed instead: the mechanism-level MATCH (§1) —
  the port reads exactly what the game's reflection declares.
- Game callers reading `hit.ImpactPoint` were not individually decompiled; outer `LineTraceSingle`
  name refs (0x47f058f, 0x4826113, 0x489d4bb, 0x489fb39) are FName-construction sites, not
  member-offset evidence. Ground truth came from the game's own reflection read path + ctor sites.
- The +8 shift is proven for the **UObject** family. AActor::Tick / UGameViewportClient::Tick /
  ProcessConsoleExec / UStruct::Link slots are expected to shift identically but were NOT
  individually verified — their hooks must stay off until per-slot verification.
- Nothing runtime-validated; console untouched. The staged ini is proposal-level and must flow
  through the existing Stage24 staging order and parser dry-run (see DIFF.md acceptance path).
- No connection is claimed between the 0x260-empty-stub finding and the observed launch-9/10
  OPTIONS-press silence — the spawn path has not executed end-to-end; the fix removes one
  demonstrated live-run unknown.

## Reusable facts for other workers

- UTF-16LE is the only correct probe for reflected names in this eboot; `findBytes` with the
  default string API silently returns nothing useful — brute-force byte scans work (scripts kept).
- vtable runs: scan both .rodata segments with a text-pointer filter that includes the first page
  of .text (tiny functions at 0xe0/0x120 are real slot values; a 0x1000 lower bound silently
  fragments runs).
- FFrame field offsets observed in game code (for future thunk work): +0x18 Code view, +0x20/+0x28
  position/Locals, +0x38 MostRecentPropertyAddress, +0x88 compiled-in property chain.
- UFunction field reads seen inside ProcessEvent (0x459830): FunctionFlags byte 1 at +0xB1
  (FUNC_Native=bit 0x4 of that byte), native Func ptr at +0xC8, +0x58/+0x5c/+0xb6 Script fields.
- Prior worker corroboration: the synthetic-validated emission pipeline
  (worker-staged-pair-emission) already derived FField total 0x30 / FProperty total 0x70 and left
  VTableLayout.ini identity-only pending vtable evidence — this worker supplies exactly that
  evidence for UObject::ProcessEvent.
