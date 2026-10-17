# FIELDS track (round 10): the 15 grandfathered engine fields

All claims here are **Mac-only** (no console contact, no launch). Image = the saved decrypt-off
copy, sha256 `3b6cfad4…22fc` (`.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`).
Addresses are image addresses. Every image value is re-checked by
`evidence/tools/measure_fields.py` (40 asserted checks, `MEASURE PASS`), results in
`evidence/FIELD-EVIDENCE.json`, instruction excerpts in `evidence/FIELD-DISASSEMBLY.txt`.

"Generic compiler" = the l23 measurement of the official UE 5.6.1 headers compiled for the PS5
target (`outputs/2026-10-08-l23-layout-generic/TABLE.json`, `CLASS-SIZES.json`). "UE4SS 5.6
default" = UE4SS upstream's own 5.6 layout file (Windows build), used only as a third,
agreeing check, never as one of the two sources.

## The table

| field | exists in 5.6? | source 1 | source 2 | offset | action |
|---|---|---|---|---|---|
| AActor::Children | yes | – | – | – | **Not read.** False alarm: `func->HasChildren()` (LuaUObject.cpp:300/345) is `UStruct::HasChildren` on a UFunction, which reads the proven `UStruct::Children`. The gate now resolves the name through the class. Nothing added. |
| AActor::Owner | yes | – | – | – | **Not read.** False alarm: `SetOwnerVariant` (CustomProperty.cpp:44) is `FField::SetOwnerVariant` → proven `FField::Owner` (0x10). Gate fix only. (Round 2: the round-1 gate could not see FField.cpp at all, see below; the fixed gate now shows this path itself: `--explain SetOwnerVariant FField::Owner` → `FField::SetOwnerVariant -> FField::GetOwner`, gate-runs-final/explain-SetOwnerVariant.txt.) |
| FByteProperty::Enum | yes | generic compiler 0x70 (TABLE.json) | – (not measured) | (0x70) | **Not read.** The only site (LuaUObject.cpp:1806) casts to `FEnumProperty*` → proven `FEnumProperty::Enum` 0x78. Gate fix; nothing added (no reader). |
| FConsoleCommandBase::Flags | not checked (no reader) | – | – | – | **Not read.** Only appeared through the shared name `GetFlags` (FUObjectItem::GetFlags) spreading through `start_mod`/`generate_*`. Gate fix. |
| FConsoleVariableBase::Flags | not checked (no reader) | – | – | – | Same as above. |
| FDelegateProperty::SignatureFunction | yes | generic compiler 0x70 | image: "FDelegateProperty" vtable 0x7EEFFF0 slot 0x70 → 0x403380 reads `[this+0x70]` → its name | (0x70) | **Not read on PS5.** The one site (TypeChecker.cpp:727) is a `FMulticastInlineDelegateProperty`, which uses the multicast field below. Nothing added (no reader). |
| FMulticastDelegateProperty::SignatureFunction | yes | generic compiler 0x70 (UE4SS 5.6 default agrees) | image: "FMulticastDelegateProperty" vtable 0x7EF11D8 slot 0x70 → 0x4147A0 (GetCPPType): `mov rax,[rsi+0x70]`, then its name (+0x18), chops "__DelegateSignature"; Inline subclass slot 0x50 → 0x3FC2B0 copies +0x70 | **0x70** (class size 0x78: generic 120 B; image constructors allocate 0x78) | **Added** to the declaration. Read at startup by `TypeChecker::store_all_object_types` when ObjectProperty's class is not found yet. |
| FUObjectItem::ClusterAndFlags | **no** (UE 4.11–4.12 only; 5.6 has Flags 0x8 + ClusterRootIndex 0xC) | – | – | – | Reading code (UObjectArray.cpp GetFlagsInternal / SetFlags / UnsetFlags) is already behind explicit `Version::` if/else chains; on 5.6 the `IsAtLeast(4,13)` branch (`Flags`, proven) is taken. The new gate evaluates those chains for 5.6 and proves this read dead. No code change. |
| FUObjectItem::FlagsAndRefCount | **no** (5.7+) | – | – | – | Same chains: only under `Version::IsAtLeast(5, 7)`. Gate proves it dead on 5.6. No code change. |
| FUObjectItem::SerialNumber | yes | generic compiler 0x10 (UE4SS 5.6 default agrees) | image: FUObjectArray::AllocateSerialNumber 0x480F10 (found by `lock xadd [this+0x90]` = PrimarySerialNumber, plus the "serial numbers overflowed" fatal string): reads `[item+0x10]`, `lock cmpxchg [item+0x10]` | **0x10** | **Added.** Read by NotifyOnNewObject queueing (LuaMod.cpp:5329, 9836) and AllocateSerialNumber (UObjectArray.cpp:1068). This is the field whose absence broke launch 8. |
| FWorldContext::ThisCurrentWorld | yes (last member, not a UPROPERTY) | image reflection data: the UHT struct record "WorldContext" (FStructParams at 0x822BE68) says size 0x2C8, align 8; the official declaration puts the 8-byte world pointer last → 0x2C8−8 = 0x2C0 (reflected GameViewport 0x200 / OwningGameInstance 0x208 / ActiveNetDrivers 0x210 fit the declared order) | image engine code: UEngine::LoadMap 0x384D460 (GameEngine vtable slot 0x4F8) loads `[WorldContext+0x2C0]` 7× as the World, and its ExternalReferences loop (data +0x2B0, count +0x2B8) compares each entry with `[ctx+0x2C0]` | **0x2C0** (struct size 0x2C8: reflection SizeOf + TCppStructOps factory 0x2A6CBE0 stores Size 0x2C8) | **Added.** Read by the LoadMap pre/post callbacks and the `[FL-MAP] phase=loaded` marker (LuaMod.cpp:5848/5936/5991). The generic compiler could not measure this one (needs UHT output); UE4SS 5.6 default also says 0x2C0/0x2C8. |
| ICppStructOps::Size | yes (0x8) | – | – | – | **Not read.** All 60 sites are `FProperty::GetSize()` / `UScriptStruct::GetSize()` (same name, different class). Gate fix. Nothing added. |
| TUObjectArray::Chunks | **no** (UE 4.8–4.10 indirect array; 5.6 uses the chunked item array) | – | – | – | Readers (UObjectGlobals.cpp ForEachUObject_IndirectArray / …InRange) were only reached through callers' `IsAtMost(4,10)` dispatch. **Now each function has its own explicit version gate** (returns, with one `[FIELDS] … refused` log line, on 4.11+). Gate proves the read dead. |
| UEnum::Names | yes | generic compiler 0x40 (UE4SS 5.6 default agrees) | image: "FEnumProperty" vtable 0x7EDC468 slot 0xD0 → 0x360820: takes `Enum` from +0x78 (proven), reads count `[Enum+0x48]`, data `[Enum+0x40]`, loops 16-byte entries taking the 8-byte value at +8 (UEnum::GetMaxEnumValue) | **0x40** (class size 0x68: generic 104 B; image native class "Enum" registers 0x68, calibrated by Object 0x28 / Struct 0xB0 / Field 0x30) | **Added.** Read by Lua UEnum methods (LuaUEnum.cpp) and enum-property reads (LuaUObject.cpp:1833). |
| UGameViewportClient::World | yes | – | – | – | **Not read.** `object->GetWorld()` in LuaUObject.hpp is a UObject (virtual GetWorld or the outer-chain fallback). UGameViewportClient, its subclasses and any function returning one never appear in that file, so the gate drops it. Nothing added. |
| (16th) StructName::\<unresolved\> | – | – | – | – | Not a field: `OUTPUT_MEMBER_OFFSETS_FOR_STRUCT` / `SHIFT_MEMBER_OFFSETS` macros loop over offset tables. Gate lists these as "map iteration in macro". |

Newly surfaced by the stricter gate (not in the 15): **FUObjectItem::RefCount** (exists in 5.6,
generic compiler 0x14). Only `FUObjectItem::GetRefCountInternal()` reads it and nothing calls
that function. Marked `FIELD-GATE-OK: <reason>` at UObjectArray.cpp:261; not added. If a caller
ever appears, the accessor throws instead of reading a guessed offset.

## What changes where

- Declaration (staged, NOT applied): `apply_declaration.py` edits
  `outputs/2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp`
  (sha `bf9419dd…` → `ab8750c5…`, 74 → 81 member rows, vtables unchanged) and refreshes the
  matching `CHANGES.json` row. Staged result: `staged-profile/units/CompiledNativeLayoutDeclarations.hpp`;
  diffs `declaration.diff`, `declaration-CHANGES.json.diff`. The profile's own host tests
  (`profile/tests/run_tests.py`) PASS on the staged copy. Profile digest, build-facts JSON and
  the required-pin lists are unchanged (the new rows are not startup pins).
- Every new row has a schema row (NativeLayoutSchema.inc) and an accessor width
  (NativeLayoutWidths.cpp), and a positive class size covering it, as the binder requires.
- **Console ini: no change needed.** On PS5 the runtime stages the compiled declaration and does
  not read `ue4ss/MemberVariableLayout.ini` while one exists. (Side note, listed only: the pinned
  console ini still carries the old wrong listener offsets 0x68/0x78; harmless today, but it
  would be wrong if the compiled declaration were ever removed. Fixing it is a console action.)

## Round 2 (after the reviewer BLOCK)

- **Gate bug fixed.** Inside a multi-line `#define` (lines ending in `\`), the round-1 gate did
  not see the `{ ... }` after an `if (Version::...)`, so it blanked almost all of `FField.cpp`
  (and 77% of `UnrealCoreStructs.hpp`). It now joins those line endings before pruning (same
  line numbers). Result on the patched tree: 521 calls / 1391 pairs (was 494 / 1358) and 49
  fields read (was 46): `FField::Next`, `FField::Owner`, `FFieldClass::CastFlags` reappear,
  all three proven. PASS/FAIL results did not change.
- **New self-check.** Every blanked span must be one whole balanced block or statement inside
  its enclosing block, and `{`/`}` must drop by the same amount per file. Otherwise the gate stops
  with exit 2 and names the file. Proven: the fixed gate with only the line-ending fix turned off
  stops on `FField.cpp lines 8-790` (gate-runs-final/selfcheck-without-continuation-fix.txt).
  The gate now prints `pruning self-check OK: 558 files loaded, 41 with version-dead code blanked`.
- **Comment wording.** The `Chunks` guard comment no longer says `FFixedUObjectArray` (that is the
  4.11-4.19 non-chunked array); it now says "4.8-4.10 static indirect array". Comment-only.
- AActor::Owner / AActor::Children conclusions unchanged; both helper paths are now shown by the
  gate itself (`explain-SetOwnerVariant.txt`, `explain-HasChildren.txt`).
- The declaration edit (`apply_declaration.py`) is unchanged.

## Build integration (for the labelled build; nothing done here)

- `apply_fields.py` changes three pinned candidate files (UObjectGlobals.cpp, UObjectArray.cpp,
  UnrealInitializer.cpp). Their `typescan-r4/CHANGES.json` candidate_sha256 rows must be
  refreshed in the build step (same as the other tracks; fast_cycle.sh reports stale rows), or
  build_variants G2 refuses.
- Ship together, as one build: `apply_fields.py` + `apply_declaration.py` (real profile, no
  `--repoint`) + install the new `field_gate.py` into harvest/. The new gate FAILs on the real,
  unedited declaration (7 unproven sites), which is safe but blocks the build.
