# Agent CU — IsA<UClass>() resolution inside FindXOf's exclusion filter

Date: 2026-10-08. READ-ONLY audit; this report is the only file written.
Port root audited (abbrev PORT): `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
Runtime that ran tonight (l25) = PORT overlaid by
`outputs/2026-10-08-l25-runtime/typescan-r4/candidate/` (CHANGES.json overlay chain; the
IsA-relevant files — UObjectGlobals.cpp, UObject.cpp/hpp, Class.hpp, UObjectMacros.hpp,
UStruct.cpp — are NOT in the overlay, so the PORT versions below are exactly what ran,
except TypeChecker.cpp which was overlaid with the l25 core-ready gate, cited below).

---

## 1. Full IsA<UClass> trace (task 1)

Chain, outermost first:

| Step | Site | What happens |
|---|---|---|
| Filter | PORT `unreal/src/UObjectGlobals.cpp:220-224` (`IsValidObjectForFindXOf`) | `!HasAnyFlags(RF_ClassDefaultObject\|RF_ArchetypeObject) && !IsA<UClass>() && !IsA<UFunction>() && !IsA<UPackage>() && !IsA<UEnum>() && !IsA<UScriptStruct>()` |
| Template | PORT `unreal/include/Unreal/UObject.hpp:83-87` | `IsA<T>()` → `IsA(T::StaticClass())` — i.e. `UClass::StaticClass()` |
| Runtime | PORT `unreal/src/UObject.cpp:103-110` | `IsA(UClass* Class)` → `GetClassPrivate()->IsChildOf(Class)`; ClassPrivate read at `UObjectBase+0x10` (generated accessor, MemberOffsets) |
| Chain walk | PORT `unreal/include/Unreal/CoreUObject/UObject/Class.hpp:177-189` (`IsChildOfInternal`) | **Pointer equality walk**: `do { if (CurrentStruct == Struct) return true; CurrentStruct = CurrentStruct->GetSuperStruct(); } while (CurrentStruct);` — no FName compare anywhere in IsA |
| SuperStruct | PORT `unreal/src/UStruct.cpp:198-210` | `GetSuperStruct()` → generated accessor at `UStruct+0x40` |

So "everything classified as UClass" requires the canonical `UClass::StaticClass()`
pointer to be a **universal ancestor** (e.g. the root `Object` class), or the SuperStruct
chain of every class to terminate on the canonical. It is NOT a name comparison that
could mismatch.

**Canonical value source — two paths, same string:**

- Storage declaration: `Class.hpp:790` `DECLARE_EXTERNAL_OBJECT_CLASS_WITH_CAST(UClass, CoreUObject, CASTCLASS_UClass)` → macro at PORT `unreal/include/Unreal/UObjectMacros.hpp:113-133` defines `static UClass* StaticClassStorage`, initialized at static-init to `STATIC_CLASS_INVALID == (UClass*)-2` (`UObjectMacros.hpp:104`).
- **Eager (the one that matters)**: `TypeChecker::store_all_object_types()` — PORT `unreal/src/TypeChecker.cpp:86-87`:
  `UClass::StaticClassStorage = StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Class"))`.
  NOT a seed slot, NOT a hardcoded offset — a full GUObjectArray walk: `StaticFindObject_InternalSlow` (PORT `unreal/src/UObjectGlobals.cpp:137-159`) iterates `ForEachUObject` comparing `GetFullName()` minus the leading `"Class "` against the target path (`String::iequal`). `GetFullName` (PORT `unreal/src/UObject.cpp:580-593`) = `GetClassPrivate()->GetName() + GetPathName(...)` — depends on ClassPrivate/NamePrivate/OuterPrivate offsets + `FName::ToString`.
- **Lazy fallback**: `IMPLEMENT_EXTERNAL_OBJECT_CLASS(UClass)` (PORT `unreal/src/UClass.cpp:9` → macro `UObjectMacros.hpp:149-167`): if storage is null or `-2`, re-run the same `StaticFindObject_InternalSlow("/Script/CoreUObject.Class")` (name derived as `STR(#ClassName)[1]` = `"Class"` — the 'U' is stripped, matching engine object naming); **if still not found it throws `std::runtime_error`** (`UObjectMacros.hpp:163`).

**When initialized:** inside `UnrealInitializer::Initialize` — PORT `unreal/src/UnrealInitializer.cpp:1418-1422` ("Initializing type system"), invoked from `UE4SSProgram.cpp:1265` during program startup, strictly **before** mods start (`setup_mods`/`start_native_mods` at `UE4SSProgram.cpp:394-405`). Live proof (harvest-l25-B-final-ue4ss.log): "Initializing type system" line 620 → `[TypeScan] scan-complete missing-keys=0 core-ready=true` line 621 (18:36:29) → TownfallItemSpawner mod start line 772 (18:36:29, after). Canonicals are resolved before mod start and never written again (the only other writer is the lazy `StaticClass()` itself).

## 2. Failure modes (task 2)

**(a) Circular — canonical initialized from the same broken FindXOf walk: DOES NOT EXIST.**
`StaticFindObject_InternalSlow` (UObjectGlobals.cpp:141-156) does NOT call
`IsValidObjectForFindXOf`, `FindFirstOf`, or `FindAllOf`; it iterates `ForEachUObject`
directly. There is no filter circularity. Shared machinery is real but shallower: both
paths use `ForEachUObject` (proven healthy — early-init walks found `PlayerController`,
log line 583; TypeScan found all core types, line 621) and `GetFullName`→`FName::ToString`
(broken there ⇒ canonical = nullptr ⇒ mode (c), a throw — not silent exclusion).
Init-order circularity is also absent: canonicals resolve at engine-init time, walks at
mod time, sequentially, no re-entrancy.

**(b) FName 'Class' vs 'UClass' mismatch: DOES NOT EXIST.** IsA never compares names —
it compares pointers along the SuperStruct chain. The canonical lookup string is the
engine-true `"/Script/CoreUObject.Class"` (no U prefix) in BOTH the eager path
(TypeChecker.cpp:86) and the lazy macro (prefix-strip of `STR(#ClassName)`), and
`GetFullName()`'s `"Class /Script/CoreUObject.Class"` is stripped to the same string by
the `find(" ")+1` in `StaticFindObject_InternalSlow`. Consistent by construction.

**(c) Uninitialized/stale at runtime: theoretically possible, empirically excluded, and
NOT silent.** The `-2` sentinel plus the throw-on-miss lazy path mean a broken canonical
produces a `std::runtime_error` raised inside the `ForEachUObject` callback, propagating
through the Lua C boundary — the Lua wrapper (`ue4ss/src/Mod/LuaMod.cpp:1731-1756`, l25
candidate :1731) has **no try/catch** — i.e. a hard crash/terminate, not a quiet
walk-finds-nothing. Tonight: zero crashes in the B/A/v1 sessions. Additionally the l25
build added an explicit gate that validates exactly this:
`outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/TypeChecker.cpp:975-1016`
checks `UClass/UFunction/UPackage/UEnum/UScriptStruct` (and more) storages for
null/`STATIC_CLASS_INVALID` and computes `core-ready`; the live log shows
`core-ready=true` (line 621) — all five canonicals were found, valid, before mod start.
Stale-after-GC is not a thing here: the canonicals are permanent `/Script/CoreUObject`
core objects, and nothing writes the storages post-init.

**(d) AA's FUObjectArray corrections: ZERO overlap.** The AA fix moved only
FUObjectArray listener-zone members (≥0x30: Create-listeners 0x58, Delete-listeners
0x68, PrimarySerial 0x90, bShouldRecycle 0x94 — see BuildProfile lines in the live log
citing agent-AA-offset-verify.md). IsA reads `UObjectBase+0x10` (ClassPrivate) and
`UStruct+0x40` (SuperStruct) — members of the *object*, not of FUObjectArray, pinned by
l19 static facts and PA-checked at boot: live log prints `UObjectBase::ClassPrivate = 0x10`
(line 644) and `UStruct::SuperStruct = 0x40` (line 648). The walk's array-zone members
(ObjObjects 0x10, NumElements 0x14, NumChunks 0x1C, item stride 0x18) were untouched by
AA (corroborated by agent CO's table).

**Which produce "everything excluded"?** Only a canonical pointing at a universal
ancestor — unreachable via the exact-path `iequal` match unless `GetFullName` were
corrupted engine-wide (which would have nulled the canonicals and failed TypeScan
instead). All four enumerated modes either don't exist (a, b, d) or manifest as a loud
crash and were explicitly gated green at init (c). **Ranked likelihood of causing
tonight's walk-finds-nothing: effectively zero.**

## 3. The other four IsA calls in the filter (task 3)

Identical machinery, identical source, all in `TypeChecker::store_all_object_types()`:

| T | Canonical assignment (PORT TypeChecker.cpp) | Lazy impl (throws on miss) |
|---|---|---|
| `UClass` | :86-87 ← `/Script/CoreUObject.Class` | UClass.cpp:9 |
| `UEnum` | :80-81 ← `/Script/CoreUObject.Enum` | UEnum.cpp:51 |
| `UPackage` | :110 ← `/Script/CoreUObject.Package` | UPackage.cpp:7 |
| `UFunction` | :117 ← `/Script/CoreUObject.Function` | UFunction.cpp:11 |
| `UScriptStruct` | :125-126 ← `/Script/CoreUObject.ScriptStruct` | UScriptStruct.cpp:7 |

(l25 overlay keeps the same lines at :159-/:165/:189/:196/:204.) All five storages are
checked by the same `core-ready` gate and passed together at 18:36:29. Same verdict for
all five: initialized correctly; a broken one would crash, not exclude.

## 4. Console-fit (task 4)

Tonight's init-time type scan is literally the canonical-resolving code: it uses
`StaticFindObject_InternalSlow` over the same `ForEachUObject` chunked walk that
`FindFirstOf`'s fallback uses (hash tables were OFF — live log: "[HashTables]
FUObjectHashTables unavailable, object lookups use GUObjectArray"; also `[Seeds] …
fuobject_hash_tables_get` unbound), and it ASSIGNS the five canonical storages the
filter later reads. The l25 `core-ready` gate then re-validates those exact storages.
`scan-complete missing-keys=0 core-ready=true` therefore directly certifies the IsA
canonicals — **yes, same canonicals, proven initialized correctly: this hypothesis is
weakened to death for tonight's failure.**

One asymmetry matters (agent CO §4, confirmed here): the type-scan's own lookups reach
`GetFullName`/`FName::ToString` and FNAME_Find-constructed names, while Lua
`FindFirstOf` constructs the CLASS-NAME argument via `FName(ClassName)` with **default
FNAME_Add** (`NameTypes.hpp:304-312`, warned "may create unexpected name table entries";
wrapper `LuaMod.cpp:1750` → `UObjectGlobals::FindFirstOf(std::string_view)` :496-499 →
name compare at :457). The unverified input tonight is that FName, not the IsA
canonicals (pointers resolved once at init and only compared thereafter). The port
authors' own comment at `UObject.cpp:642-647` shows they avoid IsA during *early* init
because StaticClass lazy-loads through FName::ToString — but by mod start the lazy path
never runs (storages already valid).

## 5. Verdict + fix shape (task 5)

**Verdict: NOT the cause.** The `IsA<UClass>` (and UFunction/UPackage/UEnum/UScriptStruct)
canonicals are provably initialized, valid, and correct before mod start (core-ready
gate, live log line 621); their failure mode is a loud throw through the Lua boundary,
not silent exclusion; no crash occurred; AA's corrections don't touch anything IsA reads.
Drop this from the ranked causes. The live cause remains the FindFirstOf class-name
FName construction (FNAME_Add via the wire-ABI ctor, GATE-1 family), consistent with
CO/CP — and the pending zzdiag-v4 in-game discriminator (db=valid in-game ⇒ filter fine)
will confirm independently.

**Fix shape if it ever were the cause (defensive only, not required tonight):**
1. Keep the core-ready gate as a hard boot requirement (already shipped in l25; consider
   making `store_all_object_types` return false on any null core canonical in the base
   port — the base-port version at TypeChecker.cpp:823 returns true unconditionally and
   null-checks nothing, the l25 overlay is what added the gate).
2. Micro-hardening (optional): resolve the five canonicals once per FindXOf walk into
   locals before iterating, so a mid-walk storage anomaly cannot re-enter StaticClass()
   per object (today it only re-checks a sentinel — cheap and safe, this is belt-and-suspenders).
3. The real l26 fix stays where CO/CP put it: FindFirstOf's string overloads should
   construct the ClassName FName FNAME_Find-first (never FNAME_Add for a pure lookup),
   per `outputs/2026-10-08-l26-runtime/typescan-r4/tests/actual_findxof_fname_fix.cpp`.
