# Agent CO — FindFirstOf linear GUObjectArray walk pre-audit (pre-v5)

Date: 2026-10-08. READ-ONLY; the only file written is this report.
Port root audited: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
(abbreviated PORT/). Ground truth: `agent-AA-offset-verify.md` (engine-code disassembly),
l26 profile overlay `outputs/2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp`
+ `RuntimeBuildProfile.hpp` pins, live `harvest-l25-B-final-ue4ss.log` BuildProfile PA lines,
official vendored UE 5.6.1 source (`outputs/2026-10-08-l23-layout-generic/official-source/.../ObjectMacros.h`).

---

## 1. The exact linear walk (task 1)

Entry chain: Lua `FindFirstOf(name)` (PORT ue4ss/src/Mod/LuaMod.cpp:1632-1657)
→ `UObjectGlobals::FindFirstOf` (PORT unreal/src/UObjectGlobals.cpp:434-479).
Hash tables off (D2; live log line 725 "[HashTables] FUObjectHashTables unavailable,
object lookups use GUObjectArray") → fallback body at **:454-476** runs
`UObjectGlobals::ForEachUObject` (**:980-1000**). Version 5.6 → **`ForEachUObject_Chunked` (:882-911)**.

Iteration formula (equivalent of `Objects[i >> 16][i & 0xFFFF]`, chunked form):

```
GUObjectArray (bound at 0x97BA110, [Seeds] slot 0, engine-code verified by AA §2)
 └ ObjObjects            = GUObjectArray + 0x10          (FUObjectArray::ObjObjects)
    ├ Objects (chunks**) = ObjObjects + 0x0              (TUObjectArray::Objects)
    ├ NumElements        = ObjObjects + 0x14             (NOT used as a bound by this walk)
    └ NumChunks          = ObjObjects + 0x1C

for (ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)            // :896
    for (ItemIndex = 0; ItemIndex < NumElementsPerChunk; ++ItemIndex) // :898, = 64*1024
        item  = bit_cast<FUObjectItem*>( Objects[ChunkIndex] + ItemIndex * 0x18 )  // :900-901
        obj   = item->Object                                         // FUObjectItem +0x0
        if (item->Flags & (1<<28 /*Unreachable*/) || !obj) continue; // :902-903, Flags at item +0x8
        ... FindFirstOf predicate ...
```

- **Constants**: `NumElementsPerChunk = 64 * 1024` (UObjectArray.hpp:88); item stride
  `FUObjectItem::UEP_TotalSize() = 0x18` (UObjectArray.hpp:53 + profile). Both binary-verified
  by AA §2 (engine code uses `shr eax,0x10` = /65536 chunking and ×0x18 stride).
- **Start/stop**: chunk 0 slot 0 through NumChunks×65536. The **only** bounds are `NumChunks`
  (outer) and `NumElementsPerChunk` (inner). It does NOT use NumElements, MaxElements,
  ObjFirstGCIndex, ObjLastNonGCIndex, or MaxObjectsNotConsideredByGC — I verified
  `GetObjFirstGCIndex`/`GetObjLastNonGCIndex` have **zero call sites** in UObjectGlobals.cpp.
  Slots ≥ NumElements in the last chunk are engine-zeroed on chunk allocation
  (FChunkedFixedUObjectArray placement-default-constructs every item), so they hit the
  `!obj` skip — covered, not missed.
- **Early exit**: only the caller's `LoopAction::Break` on first match (:906-908).
- **No stale pointer**: `ObjObjects.GetObjects()` (the chunk-pointer array) is re-read
  inside the inner loop (:900) — wasteful but never stale. `GUObjectArray` null → early
  return finding nothing (:886-890) — a null global degrades to "nothing found", not a crash.
- Per-object predicate in FindFirstOf (:454-476): direct `Class->GetNamePrivate().Equals(ClassName)`
  (ClassPrivate = UObjectBase+0x10; NamePrivate = +0x18) + `IsValidObjectForFindXOf` (:220-224),
  else super-struct chain walk `TSuperStructRange` (UnrealType.hpp:3270-3330; SuperStruct =
  UStruct+0x40) matching ancestor class names.

## 2. Layout assumptions vs profile + binary reality (task 2)

The AA correction moved ONLY listener-zone members (≥ 0x30): ObjObjectsCritical 0x30,
ObjAvailableList 0x48, Create 0x58, Delete 0x68, DeleteCrit 0x78, PrimarySerial 0x90,
bShouldRecycle 0x94, sizeof 0x98. **No object-array-zone member shifted**:

| Walk consumer | Profile (l26 overlay, AA-corrected) | 5.6 defaults (5_06 setters) | Binary (AA §2/§3) | Verdict |
|---|---|---|---|---|
| FUObjectArray::ObjObjects | 0x10 | 0x10 | 0x10 (GO+0x10 RIP-refs ×20) | OK |
| TUObjectArray::Objects | 0x0 | 0x0 | +0x10 abs, ubiquitous | OK |
| TUObjectArray::NumElements | 0x14 | 0x14 | +0x24 abs | OK (unused as bound anyway) |
| TUObjectArray::NumChunks | 0x1C | 0x1C | chunked-array coherence | OK |
| FUObjectItem size | 0x18 | 0x18 | ×0x18 stride | OK |
| FUObjectItem::Object / Flags | 0x0 / 0x8 | 0x0 / 0x8 | {0x0 Obj, 0x8 Flags, 0x10 Serial} | OK |
| UObjectBase ClassPrivate/NamePrivate/ObjectFlags | 0x10/0x18/0x8 | same | [obj+0x18] NamePrivate, [obj+0xC] InternalIndex | OK |
| UStruct::SuperStruct | 0x40 | — | static-facts pinned | OK |

The live l25-B build enforced every one of these as a PA BuildProfile check at boot
(harvest log: TUObjectArray Objects=0/PreAlloc=8/Max=16/Num=20/MaxChunks=24/NumChunks=28,
FUObjectItem 24/0/8, UObjectBase 16/8/12/24/32, SuperStruct 64). The walk consumes **no stale
member**; the AA fix could not have broken it.

Version dispatch check: 5.6 → ForEachUObject_Chunked (Version::IsAtMost(4,19) false);
VC selection → UObject425/Flags425 (Container.cpp:5.6 branch), whose flag conversion is
identity for RF_ClassDefaultObject(0x10)/RF_ArchetypeObject(0x20) — matches official 5.6.1
EObjectFlags. `EInternalObjectFlags::Unreachable = 1<<28` (UnrealFlags.hpp:184) == official
5.6.1 ObjectMacros.h:640. (Aside: port `PendingKill = 1<<29` but official 5.6 has
`RefCounted = 1<<29` — PendingKill was removed in 5.5. Only affects IsPendingKill/IsValid
diagnostics, NOT the walk, which tests Unreachable only.)

## 3. Failure modes that find nothing / skip live objects (task 3)

Enumerated and checked:

1. **Wrong element/chunk-size constants for 5.6** — ruled out (§1: 0x18 and 64K both binary-confirmed).
2. **Iterating only [FirstGCIndex, LastNonGCIndex)** — does not exist in this walk (no GC-index consumers).
3. **Stale cached Objects pointer** — does not exist (re-read per item). A *torn* read if the engine
   reallocates the chunk-pointer array mid-walk is a crash risk, not a find-nothing mode, and no
   crash occurred in v4.
4. **Off-by-one/skip pattern** — none: full [0, NumChunks)×[0, 65536) coverage; freed slots have
   Object=nullptr (engine RemoveUObjectAtIndex) and are skipped, never live slots.
5. **IsValidObjectForFindXOf rejecting everything** — ruled out as a total-blocker:
   - `HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)` reads ObjectFlags at +0x8, bits
     0x30, identity-mapped for 4.25+ (Flags425.hpp:20-21) — only rejects CDO/archetypes by design.
   - `IsA<UClass/UFunction/UPackage/UEnum/UScriptStruct>` — StaticClass storages were resolved by
     the init type-scan; missing one would *throw* (UObjectMacros.hpp:163) through the Lua C
     boundary → crash/terminate, not a quiet null; v4 logged nulls without crashing.
   - ClassPrivate offset is 0x10 (unchanged by the AA fix — the correction began at 0x30).
6. **The FName the walk compares against** — the one input NOT verified end-to-end. See §5.

Live proof the walk itself works at init (harvest-l25-B-final-ue4ss.log):
- line 583 `Already constructed: ["/Script/Engine", "PlayerController"]` — the early-init
  name-parts walk (`StaticFindObject_InternalNoToStringFromNames`, same ForEachUObject) found the class;
- line 621 `[TypeScan] scan-complete missing-keys=0 core-ready=true` — the type-scan
  (`StaticFindObject_InternalSlow` × dozens, same ForEachUObject + GetFullName) found all
  /Script core classes.

## 4. The type-scan asymmetry (task 4)

The type-scan did **not** use a different iteration — it uses the **same**
`UObjectGlobals::ForEachUObject` chunked walk, via
`StaticFindObject_InternalSlow` (UObjectGlobals.cpp:141-156, GetFullName string compare) and
`StaticFindObject_InternalNoToStringFromNames` (:175-205, FName parts compare).
The difference is the **name-construction mode and timing**:

- Init/type-scan path: `Hooks.cpp:511-518 AddRequiredObject` constructs each name part with
  **FNAME_Find first**, falling back to FNAME_Add only on NAME_None — with the embedded comment:
  *"This is because of a bug with FNAME_Add (perhaps wrong constructor found) that causes a new
  name to be created with a number instead of retrieving the existing one."*
- Seeds admission probe (CAND UnrealInitializer.cpp Phase 4) likewise invoked the bound ctor
  with **FNAME_Find explicitly**: *"FNAME_Find EXPLICITLY: the default is FNAME_Add, which
  would mutate the pool on a miss"*. The FNAME_Add path through the wire-ABI thunk was
  **never behaviorally verified**.
- FindFirstOf's string overloads (UObjectGlobals.cpp:481-504) construct `FName(ClassName)`
  with the **default FNAME_Add** (NameTypes.hpp:307/312, warned at :304 "may create
  unexpected name table entries with default FNAME_Add").
- Lua `__index` method lookup uses FNAME_Find (LuaUObject.hpp:821) — so a known-function
  discriminator can pass while FindFirstOf fails, cleanly separating the two suspects.

So "walk worked for the type-scan" is really "walk + FNAME_Find names worked at init";
no FindFirstOf call has ever provably succeeded on console (the v1/v2 "WORKS" was the
`db == nil` false positive — agent CA amendment).

## 5. Ranked bug list (task 5) + v5 predictions

Using: **P1** = PlayerController FindFirstOf probe, **P2** = ItemDatabaseSubsystem probe,
**D1** = known-function/FName discriminator (FNAME_Find-based).

### #1 (HIGH) — FNAME_Add name construction feeds the walk a non-matching ComparisonIndex
Where: UObjectGlobals.cpp:481-504 (`FName(ClassName)` default FNAME_Add) →
NameTypes.hpp:312 → wire-ABI thunk (never verified for Add; documented duplicate-entry bug in
Hooks.cpp:512 and NameTypes.hpp:304). `Equals` compares ComparisonIndex only (NameTypes.hpp:345-348);
a duplicated pool entry ⇒ different index ⇒ every `GetNamePrivate().Equals(ClassName)` fails ⇒
walk returns nullptr **for every type, including PlayerController, with a mechanically perfect walk**.
Matches ALL evidence: init Find-mode successes, v4 null objects in-game with save loaded, no crash.
v5 prediction: **P1n + P2n + D1y** (D1 uses the FNAME_Find path and should still pass).
Fix (few lines, mirrors Hooks.cpp:514): in each FindFirstOf string overload —
```cpp
auto FindFirstOf(StringViewType ClassName) -> UObject*
{
    auto Name = FName(ClassName, FNAME_Find);
    if (Name == NAME_None) { Name = FName(ClassName, FNAME_Add); }
    return FindFirstOf(Name);
}
```
(and the same Find-first guard in `FindAllOf(StringType…, bUseCache=false)` at :506-508,
whose `FName(InClassName)` also defaults to Add). A class that exists must already be in the
pool, so Find-first cannot regress legitimate lookups.

### #2 (MEDIUM) — Subsystem-not-instanced / late-init semantics (walk fine, name fine)
ItemDatabaseSubsystem instance may not exist even with a save (or exists as a BP subclass whose
super-chain match should work via TSuperStructRange). v5 prediction: **P1v + P2n**.
Fix: none in the walk; diag-side retry after save-load settles / FindAllOf with IsValid filter.

### #3 (LOW) — Walk-broken (any mechanical failure: chunks, stride, flags, GUObjectArray null)
Everything the walk consumes is binary/profile/live-PA verified (§2), and the same walk
succeeded at init in the same session (§3). For this to be the bug, something must differ
in-game (e.g., probe thread ≠ game thread reading a freshly-grown array, or the EIGT executing
before GUObjectArray rebind). v5 prediction: **P1n + P2n + D1n** (every GUObjectArray-derived
probe dead; a D1 that fetches its object via FindFirstOf also fails).
Fix: none identified — no stale member, no wrong constant, no GC narrowing exists in the code;
if this pattern materializes, instrument NumChunks/NumElements/first-slot pointer in the diag
before touching the walk.

### Also-noted (not on the FindFirstOf path)
- Port EInternalObjectFlags::PendingKill(1<<29) vs official 5.6 RefCounted(1<<29) — affects
  IsPendingKill/IsValid(false)/FindObjects BannedFlags only.
- ForEachUObject_Chunked walks all 65536 slots of the last chunk and re-reads GetObjects()
  per item — perf-only; Townfall is single-chunk at ~35.5K objects (Index=35550 in tonight's log).
- FUObjectArray::UEP_TotalSize corrected 0xB8→0x98 is consumed only by diagnostics
  (`GetObjectArraySize`), not by the walk.

## 6. Bottom line for the v5 readout

The linear walk is the cleanest-verified component in this chain (offsets engine-code-pinned,
constants binary-matched, no GC-zone narrowing, no stale-pointer or off-by-one mode). If v5
shows **P1n with D1 passing**, the bug is #1 (FNAME_Add duplicate at FindFirstOf's name
construction — apply the Find-first fix); if **P1v**, the walk and names are both fine and the
subsystem query was the artifact (#2); only an across-the-board **P1n+P2n+D1n** would put the
walk itself back under suspicion (#3), and that outcome would contradict tonight's init-time
walk successes unless the in-game context (thread/timing) differs in a way no current code
suggests.

— agent CO (ZCode), 2026-10-08, read-only; only this file written.
