# Agent CA — UFunction-call nullptr trace (read-only source analysis, 2026-10-08)

Scope: pin the console error behind `db:GetItemIDFromDeprecatedEnum("Bandage")`
failing, trace the full Lua→UFunction resolution chain, and rank root causes.
Sources: port root (authoritative, sha-verified against the l25 build pins),
l22 diagnostics copies (the units the l25 console build actually compiled),
l25 typescan-r4 overlay, and the raw zzdiag-v2 console session log.

## 0. Headline correction (evidence-first)

**The console error is NOT a UFunction-null error.** The verbatim console text is
the *UObject-instance*-null error. Raw proof from
`evidence/zzdiag-v2-session.log` (byte-swapped UTF-16, decoded):

- Line 30 (preview, un-chunked): `[Lua] [zz] property: Tried calling a member function bu`
- Chunk E1 survivors decode to `n but the UO` — the wide chars are
  `唀`=U+5500('U'), `佛`=U+4F00('O'): **"UO"** = "UObject". ("UFunction" would
  show `唀䘀` = "UF".)
- E2 `nullptr\nsta(ck traceback:)` matches the source string's trailing `\n`
  plus luaL_traceback's `stack traceback:` banner; the trailing
  `in method 'GetItemIDFromDeprecatedEnum'` (E3/E4) is luaL_traceback's standard
  rendering of the `db:Method()` call frame — it is *not* part of any C++ string.

No string matching "Tried calling <name> but the UFunction … nullptr" exists
anywhere in the port root, the l22 diagnostics copies, or the l25 tree (grepped:
"Tried calling", "but the UFunction", "is a nullptr", "in method" → 0 relevant
hits). The three real UFunction-family texts are listed in §1.

Consistency consequence: in this codebase, a *found-object-but-missing-function*
failure produces Lua's own `attempt to call a userdata value` (see §3, path N2/N3)
— a *different* error than the one observed. The observed text is only
producible by calling a **null-wrapper** (§3, path N1), which means `db` itself
was a null wrapper, which means **FindFirstOf returned nullptr** for the probe
session. The "FindFirstOf works / UFunction resolution fails" asymmetry that
framed this investigation is an artifact of a false-positive validity check
(§4).

## 1. The error site (Task 1)

Console build unit: `outputs/2026-10-08-l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp`
(verified: l25-A COMPOSITION-RECEIPT lists
`outputs/2026-10-08-l22-runtime/diagnostics/src/LuaUObject.cpp` as the compiled
unit; the l25 CHANGE-SELECTION only swaps LuaMod.cpp; the l22 hpp differs from
the port root only by logging wrappers).

- **Error site (console build)**:
  `outputs/2026-10-08-l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp:518`
  ```cpp
  L22Diagnostics::throw_lua_error(lua, "Tried calling a member function but the UObject instance is nullptr\n", "property/UObject");
  ```
- **Error site (port root, identical text/logic)**:
  `.../source/port/ue4ss/include/LuaType/LuaUObject.hpp:517`
  ```cpp
  lua.throw_error("Tried calling a member function but the UObject instance is nullptr\n");
  ```
  Full format string: `"Tried calling a member function but the UObject instance is nullptr\n"`
  (constant — no `{}` placeholders; the method name never appears in it).

Where it lives: the `MetaMethod::Call` handler registered by
`UObjectBase::setup_metamethods` (port hpp:499; Call registered at hpp:512-520).
It fires when any UObject-family userdata **whose remote pointer is REAL nullptr**
is called. (`special_invalid_ptr()` = `UINTPTR_MAX-0x1000` is *not* null —
`first/…/LuaMadeSimple/LuaObject.hpp:32-35` — and does *not* trigger this error;
see §3.)

Error plumbing: `lua.throw_error` → `RC::LuaMadeSimple::throw_error`
(`first/src/LuaMadeSimple/LuaMadeSimple.cpp:1071-1086`, luaL_error at :1084),
with `handle_error` at :1048-1050 calling `luaL_traceback(lua_state, lua_state,
msg, 0)` — that is what appends `\nstack traceback: … in method
'GetItemIDFromDeprecatedEnum'` to the console text.

The other "Tried calling" strings (for disambiguation; none matched console):
- `[UFunction::call_ufunction_from_lua] Tried calling function without both UFunction and calling context` — port `src/LuaType/LuaUObject.cpp:196` / l22 `:205`
- `Tried calling UFunction without a registered handler for parameter…` — port cpp:316 / l22 :363
- `Tried calling UFunction without a registered handler 'Out' param…` — port cpp:369 / l22 :423; return-value variant port cpp:400 / l22 :454

## 2. Resolution chain, backward from the error (Task 2)

Forward chain for `db:GetItemIDFromDeprecatedEnum("Bandage")` (port-root line
numbers; l22 diagnostics equivalents in parens — logic identical, verified by
full diff):

1. `db.X` → UObject metatable `__index` (registered port hpp:502-505) →
   `prepare_to_handle(Operation::Get, lua)` — port hpp:791 (l22 hpp:792).
2. `object = lua_object.get_remote_cpp_object()` — hpp:794; **null gate hpp:800**
   (if null → N1 below).
3. Name→FName: `Unreal::FName(member_name, Unreal::FNAME_Find)` — hpp:821
   (l22 :824). Constructor: `construct_with_string` —
   `unreal/include/Unreal/NameTypes.hpp:239-255`; delegates to the engine's
   wide-char ctor `FName::ConstructorInternal` (bound in
   `unreal/src/UnrealInitializer.cpp:844` from the `fname_ctor_wchar` scan,
   verified via post-hook at :878-937). **Silent early-return if not ready →
   ComparisonIndex stays 0 → FName(0,0)** (NameTypes.hpp:241).
4. Custom-property lookup: `LuaCustomProperty::…property_list.find_or_nullptr(object, member_name)` — hpp:822 (l22 :826); miss.
5. `obj_as_struct->FindProperty(property_name)` — hpp:836 (l22 :840); UFunctions
   are not FFields in UE5.06 → miss (the `GFunctionName` bit_cast branch at
   cpp:2321/l22:2385 is the pre-4.25 compat path).
6. `handle_unreal_property_value(Get, lua, object, property_name, field=nullptr)` —
   hpp:839 → port cpp:2303 (l22 :2365, wrapped in try/catch that only logs).
7. `if (!field && property_name != FName(0u,0u))` → **`func =
   base->GetFunctionByNameInChain(property_name)`** — port cpp:2316 (l22 :2380).
   Else-branch: `func = std::bit_cast<UFunction*>(field)` = nullptr.
8. `GetFunctionByNameInChain` — `unreal/src/UObject.cpp:660-672`: iterates
   `TFieldRange<UFunction>(Struct, IncludeInterfaces|IncludeSuper)` over the
   class chain, comparing `Function->GetNamePrivate() == FunctionName`
   (FName `operator==` = full index+number compare, NameTypes.hpp:322-325;
   vs `Equals` = index-only, NameTypes.hpp:345-348 — for plain names these
   coincide). Iteration machinery: `TFieldRange` headers
   `unreal/include/Unreal/CoreUObject/UObject/Class.hpp` /
   `UnrealType.hpp` / `UObjectMacros.hpp`.
9. Found → `push_functionproperty` (port cpp:1508-1511) →
   `UFunction::construct(lua, base, func)` —
   `ue4ss/src/LuaType/LuaUFunction.cpp:33-55`; `__call` bound at :68-73 →
   `call_ufunction_from_lua` (port cpp:151/l22 :160; null-func/context guard at
   port cpp:194-197 / l22 :203-206).
10. **Not found → `LuaMadeSimple::Type::RemoteObject<UObject>::construct(lua,
    special_invalid_ptr())`** — port cpp:2332 (l22 :2397). That is the *trivial*
    "TrivialObject" metatable (LuaObject.hpp:121-140 + setup at :156-203) which
    has **no `__call`** — so calling it raises Lua's own
    `attempt to call a userdata value`, not the observed text.
11. **N1 (observed)**: object==nullptr at hpp:800 → `SelfType::construct(lua,
    static_cast<DerivedType*>(nullptr))` — hpp:808 → full UObject-family
    metatable *with* `__call` (hpp:512) → call → `!get_remote_cpp_object()`
    (hpp:515) → **throw at hpp:517** (l22 :518).

FindFirstOf side (the other half of the asymmetry):
- `LuaMod.cpp:1632-1665` registers it; calls
  `Unreal::UObjectGlobals::FindFirstOf(ensure_str(...))` at LuaMod.cpp:1651 →
  `FindFirstOf(FName)` — `unreal/src/UObjectGlobals.cpp:434-479`.
- The string→FName here uses the **default `FNAME_Add`** (NameTypes.hpp:307-312),
  not FNAME_Find.
- Console session took the **linear path**: hash tables unbound by design
  (session log: "FUObjectHashTables unavailable, object lookups use
  GUObjectArray"; `ShouldUseHashTables()` — UnrealInitializer.cpp:108-111) →
  `ForEachUObject_IndirectArray` — UObjectGlobals.cpp:747-776.
- Match test: `Class->GetNamePrivate().Equals(ClassName)` (index-only,
  UObjectGlobals.cpp:457) + super-struct walk :466-473; candidate filter
  `IsValidObjectForFindXOf` — UObjectGlobals.cpp:220-224 — **excludes
  RF_ClassDefaultObject/RF_ArchetypeObject and all class-like objects**.
- Miss → nullptr → `LuaType::auto_construct_object(lua, object)` (LuaMod.cpp:1655)
  → **`UObject::construct(lua, nullptr)`** — port cpp:438-444 → null-wrapper
  userdata that is **non-nil in Lua**.

## 3. Null paths and which one the console hit (Task 3)

| # | Path | Trigger condition | Resulting Lua error | Matches console? |
|---|------|-------------------|---------------------|------------------|
| N1 | hpp:800→808→__call:515-517 | `db` wrapper holds REAL nullptr (FindFirstOf returned nullptr; auto_construct_object cpp:441-444; also stale/GC'd wrapper nulled by `FLuaObjectDeleteListener`, hpp:33-40) | **"Tried calling a member function but the UObject instance is nullptr"** | **YES — verbatim, incl. the preview line "property: Tried calling a member function bu"** |
| N2 | NameTypes.hpp:241 silent return (ctor not ready) or pool miss for FNAME_Find → property_name==FName(0,0) → cpp:2378 false → bit_cast(nullptr) | FName construction fails for the runtime-built name | `attempt to call a userdata value` (trivial wrapper, no __call) | No |
| N3 | cpp:2316 GetFunctionByNameInChain miss (function absent from class chain, or Children/Super iteration offset defect) | FName OK but no child UFunction equals it | `attempt to call a userdata value` | No |
| N4 | call_ufunction_from_lua func/context guard (port cpp:194-197) | UFunction found but context null | "[UFunction::call_ufunction_from_lua] Tried calling function without both…" | No |
| N5 | param/return handler texts (port cpp:316/369/400) | unsupported param/return types | distinct texts | No |

Discriminator detail (as tasked): FindFirstOf's comparison is *also* index-based
(`Equals`, UObjectGlobals.cpp:457) — it is not a string compare that could
succeed where the UFunction path fails. The claimed works/fails asymmetry
between the two resolvers is not real; what actually differs is the *witness*:
the mod IsValid-gates FindFirstOf (main.lua:8-12, 63-69:
"Item database not ready. Load a save first."), while zzdiag v1/v2/v3 gated
with `db == nil` (zzdiag-v3/main.lua:34) — a check a null-wrapper always passes
(non-nil userdata). "A3 ok" therefore never proved the object was found.

Two corroborations that db was a null-wrapper in that session:
- The mod's own `Scan failed:` line comes from the OUTER pcall
  (main.lua:146,192-195); the GetItemID call is shielded by an inner pcall
  (:162-164) that only skips the entry. So the mod's scan failure was
  `database()`/IsValid failing — i.e., FindFirstOf returned nullptr — not a
  UFunction failure.
- zzdiag v2's crash (CE-108255-1) on `db:GetClass():FindFunction(...)`:
  `GetClass` is a table member fn → `construct_uclass` — port cpp:490-494 —
  derefs `get_remote_cpp_object()->GetClassPrivate()` **with no null check** →
  null-deref on the same null-wrapper. (`FindFunction` does not exist anywhere
  in the port — 0 grep hits — so that probe was doubly doomed; it was not
  class-children corruption.)

## 4. ParamsSize/flags reads vs exec-only-text guards (Task 4)

No. The call path's engine reads — `func->GetNumParms()` (l22 cpp:246),
`GetReturnValueOffset()`, `HasChildren()`, `GetParmsSize()` (l22 added a bounds
guard at ~:211-240 with its own distinct error texts), the TFieldRange walk and
`ProcessEvent` — are plain in-process loads of engine-heap objects; there is no
refusal layer in this path. The session log's "exec-only text; no read" lines
refer to init-time signature scanning (text segment walked without data reads),
not runtime member access; the l25 `NativeTypeScanPolicy.hpp`
`checked_pointer` reader is layout-scan-only. Moot for this bug anyway: the
failure occurred at the wrapper-null gate before any UFunction pointer existed.
The R3-era "marshal arm refused: carrier UFunction is null" (LuaMod.cpp lineage,
~:5001) is the hook-event carrier selector — a different subsystem, not in the
Lua member-call path.

## 4b. Coordinator/Claude hints checked: FuncMap / FindFunctionByName / walk offsets

Checked FIRST as requested; all four hints resolved against source + the l25
profile:

1. **FuncMap is NOT on the path.** `UClass::FuncMap` / `SuperFuncMap` exist only
   as layout declarations (`unreal/include/Unreal/CoreUObject/UObject/Class.hpp:820-822`;
   generated accessors `unreal/generated_include/MemberVariableLayout_HeaderWrapper_UClass.hpp:57-85`)
   with **zero call sites** anywhere in the port (grep: no reads of FuncMap for
   lookup). The staged l25 native layout
   (`CompiledNativeLayoutDeclarations.hpp` "[NativeLayout]" block) indeed has no
   FuncMap entry — but nothing needs it.
2. **`FindFunctionByName` is not bound and not called.** Zero references in the
   entire port tree. The engine-native FuncMap-backed lookup is NOT the port's
   route, so "native FindFunctionByName not bound on PS5" cannot explain the
   behavior.
3. **The port already uses the "safe route"**: Children + SuperStruct walk —
   `GetFunctionByNameInChain` (`unreal/src/UObject.cpp:660-672`) via
   `TFieldIterator` (`unreal/include/Unreal/CoreUObject/UObject/UnrealType.hpp:2998-3243`):
   entry `GetChildFieldsFromStruct<UField>` → `UStruct::GetChildren()`
   (UnrealType.hpp:2919-2923); advance `UField::GetNext()`
   (UnrealType.hpp:2948-2952); parent `GetSuperStruct()`
   (UnrealType.hpp:3227-3236); interfaces via `UClass::GetInterfaces()`
   (:3214-3225); per-child filter `UFieldClass->HasAllCastFlags(T::StaticClassCastFlags())`
   (:3178-3187).
4. **Every offset the walk depends on is in the audited l25 profile** (values
   match Claude's): `[UStruct] Children=0x48 SuperStruct=0x40 ChildProperties=0x50`,
   `[UField] Next=0x28`, `[UClass] ClassCastFlags=0xd8 Interfaces=0x1d8`,
   `[UObjectBase] NamePrivate=0x18`, `[FFieldClass] CastFlags=0x10`,
   `[UFunction] Func=0xd8 FunctionFlags=0xb0 NumParms=0xb4 ParmsSize=0xb6
   ReturnValueOffset=0xb8`. The generated accessors are MemberOffsets-keyed and
   **throw** `std::runtime_error{"Tried getting member variable 'UStruct::Children' …"}`
   on a missing key (`MemberVariableLayout_SrcWrapper_UStruct.hpp:42-59`) — a
   missing offset cannot silently yield nullptr; it would surface loudly
   (the l22 catch in handle_unreal_property_value logs "property access/set";
   nothing of the kind in the console log).
   Residual silent-failure modes in the walk itself, per Claude's taxonomy:
   - *0 UFunctions visited* would require a wrong (but staged) offset — most
     plausibly ClassCastFlags 0xd8 filtering every child out, or Children 0x48
     pointing at a null/incorrect list. Both offsets are l19-audited
     (REFLECTION-FACTS/UCLASS-FACTS), and the same session successfully walked
     the adjacent FField child list (log "[Lua] Exposing size of
     'N2RC6Unreal12FProperty…'" lines), so chain walking demonstrably works
     in-session for properties; UFunction children share the same UStruct base.
   - *Many visited, no match* = name-comparison problem — note the comparison
     here is `operator==` (index+Number, NameTypes.hpp:322-325) vs FindFirstOf's
     `Equals` (index-only, :345-348); for plain unnumbered names like
     "GetItemIDFromDeprecatedEnum" they coincide. Case handling is identical on
     both paths (same engine wide-char ctor, engine-side case-insensitive pool),
     so no asymmetry is available there.
   - Recommended instrumentation (l26): extend the existing l22
     `record_function_lookup` hook — currently SUCCESS-branch only (l22
     `diagnostics/src/LuaUObject.cpp:2390`) — to the miss branch inside
     `GetFunctionByNameInChain`: log receiver class FName + requested FName,
     count UFunctions visited, first few child names. Claude's
     KismetStringLibrary known-function vector fits the same hook.
   - Reminder from §0/§3: in the captured failing session this walk was never
     reached (db was a null wrapper; the error text is the UObject-instance
     string), so the FuncMap/walk question is live only for the
     in-game-with-save retest.

## 5. Ranked root causes (Task 5)

1. **FindFirstOf("ItemDatabaseSubsystem") returned nullptr; the diag's
   `db == nil` check passed the resulting null-wrapper as "found"; calling
   through the null-wrapper raised hpp:517.** Evidence: verbatim error text
   (only N1 produces it); mod's outer-pcall scan failure; v2 crash = unchecked
   `construct_uclass` null-deref; error fired ~19s after boot (mod load
   19:09:47, probes 19:09:53) — main-menu timing. Most likely sub-driver:
   **no save loaded / subsystem not instanced**, so only the CDO exists and
   `IsValidObjectForFindXOf` (UObjectGlobals.cpp:220-224) correctly excludes it.
   A linear-scan miss (layout) is less likely — the same session verified live
   engine-array reads (listener header Data/Num/Max, NotifyUObjectDeleted with
   real indices) and the FName pool + ctor ("slot 1 fname pool… admitted",
   "ByteProperty probe i==0 pool id 0x3", "behaviorally verified").
2. **Stale/GC'd wrapper nulled by FLuaObjectDeleteListener** (hpp:33-40) —
   same terminal error; possible for the mod's long-lived `db` between scan and
   call, but not needed to explain the zzdiag sequence (FindFirstOf→call were
   adjacent in one ExecuteInGameThread closure).
3. **FName(FNAME_Find) → FName(0,0) or pool mismatch (GATE-1 family)** —
   plausible latent risk for the real in-game path, but it produces the
   *trivial-wrapper* "attempt to call a userdata value" error, not the observed
   text; in-session pool/ctor probes passed.
4. **GetFunctionByNameInChain miss (children-chain offsets)** — same objection
   (wrong error text); the v2 crash was a null-deref, not chain corruption.
5. **Call-time guards / param metadata / R3 / exec-only-text refusals** — all
   have distinct texts and were never reached.

## 6. Implications for l26 (report-only suggestions)

- Fix the diag validity gate: use `IsValid` (pcall-wrapped, as the mod does),
  not `db == nil`; re-run the chain **in-game with a save loaded** before
  concluding a resolution defect. v3's A-step still has the false-positive check.
- Null-harden `construct_uclass`/`construct_fname` (port cpp:484-494) — they
  deref without checks and turned a benign null into CE-108255-1.
- Optional: make `handle_unreal_property_value`'s miss branch (cpp:2332/l22
  :2397) push something that raises an actionable "UFunction not found" error
  instead of a bare special_invalid_ptr wrapper (today that branch surfaces as
  Lua's opaque "attempt to call a userdata value").
- The UFUNCTION-NULLPTR-FINDING.md framing ("nullptr-UFunction verdict is
  unambiguous", "FName pool path suspect") should be amended per §0/§5: the
  verdict text is the UObject-instance error, and the null originated in
  FindFirstOf's result, before any UFunction resolution ran.

File:line references are to the port root
`.../native-coherent-runtime-1/source/port/` unless prefixed with an outputs
path. Console-build (l22 diagnostics) equivalents: hpp:517→518, cpp:196→205,
cpp:2303→2365, cpp:2316→2380, cpp:2332→2397.
