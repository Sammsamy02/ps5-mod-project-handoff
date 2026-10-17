# Agent CG — FindFunction-from-Lua crash (CE-108255-1) vs internal nullptr resolution

READ-ONLY analysis. Sources: `evidence/zzdiag-v2-session.log`, `evidence/UFUNCTION-NULLPTR-FINDING.md`,
`evidence/zzdiag-v3/main.lua` (successor mod documenting v2's removed F1/F2 probes), port root
`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`, and the deployed l25-A
build selection (`outputs/2026-10-08-l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json`:
`lua-luauobject` = l22-diagnostics `LuaUObject.cpp`, `unreal-uobject` = port-root `UObject.cpp`,
guard = `typescan-r4` candidates).

## VERDICT (one issue or two)

**ONE issue, two faces.** Both the internal resolution (the `db:GetItemIDFromDeprecatedEnum`
member-call path that errors cleanly) and the Lua `db:GetClass():FindFunction(...)` probe funnel
into the SAME unguarded traversal — `UObject::GetFunctionByNameInChain` → `TFieldIterator<UFunction>`
→ raw `UStruct::Children` / `UField::Next` linked-list reads. The asymmetry (null vs crash) is NOT
different guarding of the traversal; it is an accidental gate UPSTREAM of it:

- internal/A-probe: `FName("GetItemIDFromDeprecatedEnum", FNAME_Find)` fails to produce a nonzero
  index (the GATE-1/runtime-FName family failure already documented as THE functional blocker), so
  `handle_unreal_property_value` skips the traversal entirely at its `property_name != FName(0u,0u)`
  gate (l22-diagnostics `LuaUObject.cpp:2378`), builds a `special_invalid_ptr` userdata, and calling
  it throws the clean Lua error ("Tried calling a member function but the UObject instance is
  nullptr" + Lua traceback "in method 'GetItemIDFromDeprecatedEnum'"). Refused → nullptr. No raw walk.
- F-probe: the member name "FindFunction" is engine-common and resolves to a nonzero FName index,
  the gate passes, the RAW UFunction children traversal runs for the first time ever on console
  (no UFunction-list walk has ever completed there — every prior lookup died at the FName gate),
  and one of its unguarded reads faults → CE-108255-1.

So: the nullptr root cause (FName find refuses/fails for runtime names) and the crash root cause
(the children traversal is raw and unsafe) are two defects in ONE resolution subsystem; fixing only
the FName side will convert every future mod call into the same crash. Fixing only the traversal
keeps every call at nullptr. l26 must fix the traversal AND the name resolution together.

## 1. The Lua-exposed GetClass():FindFunction path (file:line, memory touched)

There is NO `FindFunction` Lua binding anywhere in the port (checked `LuaUClass.cpp` — only
GetCDO/IsChildOf/type; `LuaUStruct.cpp`, `LuaUObject.*`: none). The call rides the generic member
machinery. Verified against the exact deployed sources:

1. `db:GetClass()` — registered binding, `LuaUObject.hpp:558` → `construct_uclass`
   (`LuaUObject.cpp:552`) → **raw read** `db->GetClassPrivate()` (versioned member wrapper =
   `*(void**)(db + offset)`; `TObjectPtr` is a plain pointer wrapper, `TObjectPtr.hpp:17-53`,
   no handle resolution). Returns a `LuaType::UClass` userdata wrapping the UClass object.
2. `:FindFunction(name)` — "FindFunction" not in any member table → `__index` metamethod. The
   winning `__index` for every UObject-derived userdata is `UObjectBase<Unreal::UObject>`'s
   (`LuaUObject.hpp:503`, created last in the construct chain — UClass/UStruct `setup_metamethods`
   are no-ops), so `DerivedType = Unreal::UObject` in `prepare_to_handle` (`LuaUObject.hpp:795-845`):
   - `FName("FindFunction", FNAME_Find)` — calls the GAME's live FName constructor
     (`NameTypes.hpp:239-255` → `ConstructorInternal`, address assigned+verified at
     `UnrealInitializer.cpp:844,894-931`). GATE-1 admission family.
   - custom-property lookup (no game memory), then since `is_base_of_v<UStruct, UObject>` is false:
     `obj_as_struct = object->GetClassPrivate()` = **the METACLASS** (class-of-the-class)
     (`LuaUObject.hpp:838`) → `FindProperty(FName("FindFunction"))` (`UStruct.cpp:155`):
     `TFieldRange<FProperty>` raw walk of the metaclass FProperty chain + supers
     (**raw reads**: `ChildProperties` head, `FField::Next`, `FField::ClassPrivate`,
     `FFieldClass::CastFlags`, `FProperty` FName, `SuperStruct`).
   - `handle_unreal_property_value` (`LuaUObject.cpp:2365-2447`): field==null and name nonzero →
     `func = base->GetFunctionByNameInChain(property_name)` (`LuaUObject.cpp:2380`), base = the
     UClass object itself → `ReceiverAsStruct` (`UObject.cpp:596-604`: raw ClassPrivate +
     ClassCastFlags read; a UClass IS a UStruct → returns SELF, not the metaclass) →
     `TFieldRange<UFunction>(theClass, IncludeInterfaces|IncludeSuper)`.

   NOTE the receiver inconsistency: the property scan uses the metaclass, the function scan uses the
   class itself — same as upstream UE4SS, but relevant when unifying.

### The UFunction-children traversal and its reads (UE 5.6 = `IterateToNext_425Plus`, `UnrealType.hpp:3167-3243`)

| # | Read | Where | Raw? |
|---|------|-------|------|
| R1 | `UStruct::Children` head (`GetChildFieldsFromStruct<UField>`) | `UnrealType.hpp:2919-2923` → `MemberVariableLayout_SrcWrapper_UStruct.hpp:42` | RAW offset deref |
| R2 | per-child `UField::Next` (`GetNextField<UField>`) | `UnrealType.hpp:2947-2952` | RAW |
| R3 | per-child `ClassPrivate` then `UFieldClass->HasAllCastFlags(CASTCLASS_UFunction)` | `UnrealType.hpp:3180-3183` | RAW — **null-check MISSING** (pre-4.25 path has it at 3104; the 4.25+ path derefs `UFieldClass` unconditionally) |
| R4 | `Function->GetNamePrivate() == FunctionName` per child (comparison index + number words) | `UObject.cpp:665` | RAW |
| R5 | `SuperStruct` per hop | `UnrealType.hpp:3229` | RAW |
| R6 | `Interfaces` TArray `.Num()` + `operator[]` (unique to IncludeInterfaces, which only GetFunctionByNameInChain uses) | `UnrealType.hpp:3217-3222` | RAW |

Also exposed to Lua and equally raw: `LuaUStruct.cpp:65-94` "ForEachFunction" and 96-125
"ForEachProperty" (direct `TFieldRange` over the remote struct).

## 2. What can fault on console, and who is guarded

- **l23/l24/l25 guard family (`NativePropertyGuard`)**: `typescan-r4/include/NativePropertyPointerGuard.{hpp,inl}`
  — safe reads via installed backends `PropertyReadBytes` (fault-ledger `bridge_copyin_range_faulted`
  + budgeted VQ admission coverage + re-check) and `PropertyImageClassRange` (`bridge_image_range`
  spanpub fallback + coverage) — `NativePropertyReadBackend.inl:20-46`, installed at
  `typescan-r4/candidate/src/UnrealInitializer.cpp:1719`. BUT the entire guard is
  **FField-only** (`FField::Next`, `FField::ClassPrivate`, FFieldClass identity) and lives in an
  **anonymous namespace inside the TypeChecker TU** (`.inl` included only by `TypeChecker.cpp:39`;
  the build's `.d` files confirm one consumer). No UField/UObject-side member is covered; no other
  TU can even link it.
- **Who uses the guard**: only the startup `TypeChecker` FProperty bootstrap scan
  (`TypeChecker.cpp:254-301`, `__PROSPERO__` only). It never touches `UStruct::Children` or
  `UField::Next`.
- **Who guards the Lua path**: nothing. `FindProperty`, `GetFunctionByNameInChain`,
  `ForEachFunction`, `ForEachProperty` all deref directly. The only "protection" ever demonstrated
  is the accidental `FName(0,0)` skip. Exec-only text guards / the owned-memory refusal layer
  (R3 detour refusals in the session log) govern TEXT writes/detour arming, not these data reads.
- **Which pages fault**: any stale/garbage child, wrong-offset `Children`/`Next` value, null
  `ClassPrivate` (R3 reads near 0 → instant fatal on Prospero), or an `Interfaces`/`SuperStruct`
  value landing in an unmapped hole. The game's allocator regions are only proven readable through
  the admission-coverage/spanpub backends — which the Lua path never consults. Crucially, the
  UField-side offsets have NEVER been validated on console: every earlier function lookup failed at
  the FName gate before reaching R1. The zzdiag v1/v2 A-probe DID complete `FindProperty` over db's
  whole class chain (ChildProperties + `FField::Next` + SuperStruct walked clean — A/B/E markers
  printed), so the FFIELD-side offsets are console-proven; the crash isolates to the UFIELD-side
  structures (R1-R6), first exercised by the F-probe.

## 3. Hypothesis check

"Internal nullptr = traversal refused; Lua FindFunction = raw traversal → fault" — **TRUE in
effect, with one correction**: the refusal is not the l24 safe-reader (which does not exist on this
path at all) but the `property_name != FName(0u,0u)` gate at `LuaUObject.cpp:2378`, fed by the
failed runtime FName find (the documented GATE-1 blocker). The F-probe's name ("FindFunction",
engine-common) resolved nonzero, passed the gate, and ran the raw walk. One traversal, one
memory-safety hole; nullptr and crash are the same defect surfaced through two different gate
outcomes. Secondary aggravator found during the map: the 4.25+ iterator's missing
`UFieldClass` null check (`UnrealType.hpp:3183`) is a concrete near-null deref that alone can
produce exactly this CE-108255-1.

## 4. l26 shape — one fixed traversal for both paths

1. Generalize `NativePropertyGuard` beyond FField: add checked readers for `UStruct::Children`,
   `UField::Next`, child `ClassPrivate`(+null/alignment/identity via UClass CastFlags),
   `NamePrivate`, `SuperStruct`, `Interfaces.Num()` — all routed through the ALREADY-INSTALLED
   backends (`PropertyReadBytes` = fault ledger + admission coverage; `PropertyImageClassRange` =
   bridge_image_range spanpub). Move the guard out of TypeChecker's anonymous namespace into a
   shared TU so LuaUObject/UObject TUs can link it (TypeChecker keeps its scan-mode wrappers).
2. One iterator used by both paths: `SafeFunctionScan(struct, FName, flags)` (and a matching
   safe FindProperty) with: per-node validation before every deref; cycle set + bounded depth
   (max nodes/struct, max super hops, global cap); FName compare as pure comparison-index integer
   compare (pool-find result), never ToString in-loop; fail-closed → nullptr on ANY refusal, with
   a loud refusal log line (reason taxonomy like the l24 scan refusals). Replace the bodies of
   `GetFunctionByNameInChain`/`GetFunctionByName` and `FindProperty`, which automatically covers
   `handle_unreal_property_value`, the generic index machinery, and any future Lua-exposed
   `UClass:FindFunction(name)` binding (add it as a real binding returning the UFunction userdata
   or nil — no invalid-object call needed). Gate `LuaUStruct` "ForEachFunction"/"ForEachProperty"
   behind the same iterator.
3. Fix the 4.25+ null-check regression (`UnrealType.hpp:3183`): restore the pre-4.25 path's
   `!UFieldClass ||` guard.
4. Make the FName(0,0) skip LOUD (log which name refused) and pair it with the GATE-1/FNAME_Find
   fix so game names resolve — the traversal fix removes the crash face, the FName fix removes the
   nullptr face. Also reconcile the receiver inconsistency (metaclass for FindProperty vs self for
   GetFunctionByNameInChain) inside the one traversal so both scans of a struct receiver use self.

## Residual unknowns

- No kernel backtrace exists for the crash (no klog tap post-reboot), so WHICH of R1-R6 faulted
  first is unproven; R3's near-null deref and R1/R2 wrong-offset wild pointers are the leading
  candidates. A one-shot guarded-traversal refusal log (item 2) will name it on the first run.
- Whether `FNAME_Find("FindFunction")` succeeded via the real pool or a startup-pinned cache is
  not distinguishable from this log; either way the gate outcome split is what the session shows.
- The zzdiag v2 F-probe source itself is not preserved; v3's header comment documents it
  ("old F1/F2 ... crashed the console"), and the log's truncated tail is consistent with the crash
  occurring while evaluating the F-probe after the E-chunk print in the same callback.
