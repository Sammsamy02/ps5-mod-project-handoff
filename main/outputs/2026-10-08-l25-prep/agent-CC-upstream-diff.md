# Agent CC — upstream UE4SS 3.0.1 vs PS5 port diff, Lua object-call / UFunction-resolution path (2026-10-08)

Read-only analysis. Sources compared (all sha-of-content by direct `diff`, not by
label):

- **Upstream UE4SS v3.0.1** (the version the console banner reports:
  `UE4SS - v3.0.1 - Git SHA #e3ba1016562d6c08`, zzdiag-v2 session log line 2):
  `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/`
  (abbreviated **UE/** below). Its Unreal API submodule is empty in the archive;
  the authentic upstream for that layer is
  `.superpowers/native-dependency-spike/ue4ss-unreal-source-access-1/upstream/`
  (Re-UE4SS/UEPseudo @ `38e7171e9e8c4a871ff84765848f0290ca34a44e`, receipt
  `source-receipt.json`; abbreviated **UEP/**). The RE-UE4SS mirror in
  `ue4ss-binding-research-1/upstream/RE-UE4SS` is textually identical for the
  units used here (LuaUObject.cpp GetFunctionByNameInChain call at its :2220).
- **Port root**: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
  (**PORT/**). The l24-build-r2 `SOURCE-SELECTION.json` (the runtime the l25-A
  console package consumed) pins `PORT/ue4ss/src/LuaType/LuaUObject.cpp` and
  `PORT/ue4ss/include/LuaType/LuaUObject.hpp` as the compiled units, with the
  l22-diagnostics overlay for LuaMod/LuaUObject logging
  (`outputs/2026-10-08-l22-runtime/diagnostics/…`, abbreviated **L22/**).
- **Console build's UnrealInitializer/FName machinery**:
  `outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp`
  (**CAND/**) + `outputs/2026-10-08-l25-package-A/lineage/NativeFNameAdapter.hpp`,
  `pool_admission.h`, `PoolAdmissionEngine.hpp`, `SeedTable.h`.
- Console evidence: `outputs/2026-10-08-l25-prep/evidence/zzdiag-v2-session.log`,
  `CONSOLE-RESULTS-20261008.md`, `evidence/UFUNCTION-NULLPTR-FINDING.md`,
  `zzdiag-v3/main.lua`, spawner `main.lua`
  (`outputs/2026-10-08-l21-audit/claude-review-20261008T151105Z/prior-console-evidence/main.lua`).
- Cross-check: parallel agent CA's `agent-CA-ufunc-trace.md` (independent
  derivation; agreement noted where relevant).

---

## Task 1 — upstream 3.0.1's Lua object-call path for UFunctions

For `db:GetItemIDFromDeprecatedEnum("Bandage")` upstream 3.0.1 does:

1. **`__index`**: UObject-family userdata metatable `Index` metamethod →
   `prepare_to_handle(Operation::Get, lua)` — **UE/include/LuaType/LuaUObject.hpp:791**
   (identical text at PORT hpp:791, L22 hpp:792).
2. **Null-object guard**: `object = lua_object.get_remote_cpp_object()`; if null →
   `SelfType::construct(lua, static_cast<DerivedType*>(nullptr))` — a *null-wrapper*
   userdata (metatable "UObject" **with** a `Call` metamethod) — UE hpp:795-812.
3. **Method-name → FName**: `Unreal::FName property_name = Unreal::FName(member_name, Unreal::FNAME_Find);`
   — **UE hpp:821**. The ctor `construct_with_string`
   (UEP/include/Unreal/NameTypes.hpp:239-255, public ctors :306-318) delegates to
   `FName::ConstructorInternal` — a **function pointer into the game's own
   `FName(const WIDECHAR*, EFindName)` constructor**, bound at init
   (UEP/src/UnrealInitializer.cpp:686-688
   `FName::ConstructorInternal.assign_address(results.fname_ctor_wchar)`; verified
   via the FNameConstructor post-hook, :698-760). Guard: if the pointer is not
   ready it **silently returns, leaving the FName as `FName(0,0)` (NAME_None)**
   (UEP NameTypes.hpp:241). `FNAME_Find` = lookup-only, never adds to the pool
   (comment at :306). No caching of UFunctions anywhere in this path — every
   `__index` redoes the lookup.
4. **Custom-property probe**: `LuaCustomProperty::StaticStorage::property_list.find_or_nullptr(object, member_name)`
   — UE hpp:822 (miss for engine-reflected members).
5. **Property search**: `obj_as_struct->FindProperty(property_name)` (walks the
   UStruct FField chain; UFunctions are not FFields in UE≥4.25) — UE hpp:824-837.
6. **Dispatch**: `handle_unreal_property_value(...)` —
   **UE/src/LuaType/LuaUObject.cpp:2191-2241**. Because `field==nullptr`:
   `if (!field && property_name != Unreal::FName(0u, 0u))` →
   **`func = base->GetFunctionByNameInChain(property_name);`** — **UE cpp:2220**.
   (Else-branch `func = std::bit_cast<UFunction*>(field)` is the pre-4.25
   compat shortcut; with `field==nullptr` and `property_name==None` it yields
   `func==nullptr`.)
7. **GetFunctionByNameInChain**: **UEP/src/UObject.cpp:653-663** —
   `ReceiverAsStruct(this)` then
   `for (UFunction* Function : TFieldRange<UFunction>(Struct, IncludeInterfaces|IncludeSuper))`
   comparing `Function->GetNamePrivate() == FunctionName` (FName index+number
   equality, UEP NameTypes.hpp:322-325). The walk uses the UClass `Children`
   linked list (`UField::Next`) and `SuperStruct` chain via the member-offset
   layout system (headers UEP include/Unreal/CoreUObject/UObject/Class.hpp +
   UnrealType.hpp — `TFieldRange`). No guards: a wrong offset faults; a miss
   returns nullptr.
8. **On hit**: `push_functionproperty` → `UFunction::construct(lua, base, func)`
   (UE/src/LuaType/LuaUFunction.cpp:33-55) whose `setup_metamethods` binds
   `MetaMethod::Call = call_ufunction_from_lua` (:68-73) — the `__call` at
   **UE/src/LuaType/LuaUObject.cpp:69-** (arg guard :134, "without both UFunction
   and calling context" at :134-136).
9. **On miss** (`func==nullptr`): `LuaMadeSimple::Type::RemoteObject<UObject>::construct(lua, special_invalid_ptr())`
   — UE cpp:2233-2238. That is the *trivial* "TrivialObject" metatable
   (UEP deps first/LuaMadeSimple/include/LuaMadeSimple/LuaObject.hpp:121-140)
   with **no `__call`** — so Lua itself later raises
   *`attempt to call a userdata value (method 'X')`*. It never produces the
   observed console text.
10. **The observed text's only upstream source**:
    `lua.throw_error("Tried calling a member function but the UObject instance is nullptr\n")`
    — **UE hpp:517** (PORT hpp:517, L22 hpp:518 with
    `L22Diagnostics::throw_lua_error(..., "property/UObject")`). It is the
    `MetaMethod::Call` handler of the UObject metatable and fires **only when a
    null-wrapper (remote pointer == real nullptr) is called**;
    `special_invalid_ptr()` (= `UINTPTR_MAX-0x1000`, LuaObject.hpp:32-35) does
    **not** trigger it.

Upstream FindFirstOf (the "works" half of the asymmetry):
`UE/src/Mod/LuaMod.cpp:1347-1366` → `UObjectGlobals::FindFirstOf(FName)`
(UEP/src/UObjectGlobals.cpp; same shape as PORT :434-479) — on miss it calls
`auto_construct_object(lua, nullptr)` → `UObject::construct(lua, nullptr)` → the
**non-nil null-wrapper**. So on PC, `FindFirstOf("…") == nil` is *always false*;
mods must use `IsValid()`.

---

## Task 2 — port divergences on this exact path

Full-file diffs run for every unit in the chain. The resolution logic is
**byte-identical to upstream**; the divergences are:

| # | Divergence | Upstream | Port / console build |
|---|---|---|---|
| D1 | **FName ctor binding.** Direct engine fn-ptr, bound unconditionally after the signature scan; callable on any thread. | UEP UnrealInitializer.cpp:686-688 (+ PORT root unreal/src/UnrealInitializer.cpp:844 keeps this as the non-Seeds path) | Console (CAND UnrealInitializer.cpp): `FName::ConstructorInternal.assign_address(&native_fname_thunk)` at **:1216**; `native_fname_thunk` **:1025-1035** → `rc::fnameabi::invoke` (lineage/NativeFNameAdapter.hpp:37-50) re-encodes to UTF-16 and calls the engine ctor through a **wire ABI (RDI=out, RSI=UTF16, EDX=mode; RAX ignored)**, rejecting `mode>1` and bad UTF-16. The bind is **fail-closed behind Seeds admission**: pool readability (PoolAdmissionEngine.hpp `coverage/read/retry`), ctor XOM signature capture, `ByteProperty` behavioral probe + pool roundtrip + id==pool check (CAND :1141-1214). Any refusal = `seed_refused` → ctor never bound → `construct_with_string` silently yields `FName(0,0)` (NameTypes.hpp:241, unchanged). |
| D2 | **FindFirstOf backend.** Hash-table object lookup when `FUObjectHashTables` resolves. | UEP UObjectGlobals.cpp (hash path first) | Console: `fuobject_hash_tables_get` "Unbound by design" (session log line 24) → `ShouldUseHashTables()` false → **linear `ForEachUObject` walk**, PORT unreal/src/UObjectGlobals.cpp:449-479; CDO/archetype/class exclusion `IsValidObjectForFindXOf` PORT :220-222 (same rule as upstream). Lua binding identical: PORT ue4ss/src/Mod/LuaMod.cpp:1632-1657 (L22 logging aside). |
| D3 | **`call_ufunction_from_lua` protection.** Direct body. | UE LuaUObject.cpp:69- | PORT cpp:95-155 + :202-437 `__PROSPERO__`: `FNativeUFunctionCall` RAII + `lua_pcall` frame so the C-compiled VM's `longjmp` cannot skip C++ unwinding of out-param registry refs. Resolution-neutral. |
| D4 | **Marshal-bounds pre-checks** (l22 diagnostics only): parameter-frame capacity/offset/extent checks with their own distinct error texts. | none | L22 LuaUObject.cpp:211-250 ("parameter frame exceeds…", "invalid reflected bounds", …). Resolution-neutral. |
| D5 | `FName::ToString` frame alloc `_malloca` → guarded `MakePayloadParameterFrame` + empty-frame throw. | UEP NameTypes.cpp:68-99 | PORT unreal/src/NameTypes.cpp:74-104. Not in the resolution path (ToString only). |
| D6 | Cosmetics: `DynamicOutput/Output.hpp` include, `Unreal::TCHAR` qualification, `#include <type_traits>`/`<exception>`. | — | PORT LuaUObject.cpp:1-8, hpp:10, etc. |

Identical-to-upstream (verified 0-diff or trivially cosmetic): `prepare_to_handle`
(PORT hpp:791-841), `handle_unreal_property_value` (PORT cpp:2303-2353, call at
:2316 = UE :2220), `UObject::GetFunctionByNameInChain` (PORT
unreal/src/UObject.cpp:660-672 = UEP :653-663), `TFieldRange`/Class.hpp/
UnrealType.hpp, `auto_construct_object` (PORT cpp:437-…), the hpp:517 error text,
FindFirstOf Lua binding semantics, and LuaUFunction construct/metamethods
(consumed from the vendored upstream header unmodified — PORT/ue4ss/include/LuaType
contains only LuaUObject.hpp + LuaUnrealString.hpp; the rest resolve from the
vendored UE/ tree).

Console runtime further proved D1 **succeeded live** in every session:
`[Seeds] slot 1 fname pool 0x9765EC0: admitted`, `'ByteProperty' id 0x3`,
`[Seeds][L18] FName constructor signature matched`, `[Seeds][L15] FName output
verified`, and `KismetStringLibrary:Conv_NameToString located` — the last one is
the *same* `FName(STR("Conv_NameToString"), FNAME_Find)` +
`GetFunctionByName` machinery (CAND UnrealInitializer.cpp:1988) resolving a
UFunction by name on a native class, on console, at init.

---

## Task 3 — which divergence produces the console symptom

**None of them.** The symptom is not a UFunction-resolution divergence at all;
it is upstream-identical behavior hitting a *null wrapper*:

1. **The chunked console text reassembles** (E0-E11, zzdiag-v2 log 19:09:53) to
   `"Tried calling a member function but the UObject instance is nullptr\n"` +
   Lua 5.4 `luaL_traceback` tail `stack traceback: … in method
   'GetItemIDFromDeprecatedEnum' … in 'pcall'` — produced by
   `handle_error`'s `luaL_traceback` (PORT
   first/src/LuaMadeSimple/LuaMadeSimple.cpp:1048-1050, 1071-1086). The `[l22]
   property:` preview is L22's error-callback tag for category
   `"property/UObject"` (L22 hpp:518).
2. That text is emitted **only** by the UObject `Call` metamethod when the
   userdata's remote pointer is a real nullptr (Task 1 §10). The two genuine
   resolution-miss outcomes produce *different* errors: FName→None or
   GetFunctionByNameInChain→nullptr both fall into the `special_invalid_ptr`
   TrivialObject branch (PORT cpp:2331-2338) which has no `__call` → Lua's
   `attempt to call a userdata value (method 'X')`. **So the observed error
   cannot come from a failed name→UFunction lookup on a valid object.**
3. The only way `db:GetItemIDFromDeprecatedEnum(...)` reaches hpp:517 is
   `__index` running on a db wrapper that is **already null** →
   `prepare_to_handle`'s null-object branch (PORT hpp:800-812) returns another
   null-wrapper, whose call throws hpp:517. I.e. **FindFirstOf returned
   nullptr** and the diag's `db == nil` witness passed the non-nil null-wrapper
   as "found" (upstream semantics, LuaMod.cpp — miss →
   `auto_construct_object(lua, nullptr)`, PORT cpp:441-444). zzdiag v3 repeats
   this false-positive gate (`zzdiag-v3/main.lua:34`).
4. Corroboration in the same session: the spawner's `Scan failed:` came from the
   **outer** pcall (main.lua:146,192-195) — the GetItemID calls are shielded by
   **inner** pcalls (:162-164, :169-171) that only skip entries — so the scan
   failure was `database()`/`IsValid` failing, i.e. FindFirstOf found nothing,
   not a UFunction failure. The probes fired at 19:09:53, ~19 s after boot, at
   the main menu with no save loaded; `IsValidObjectForFindXOf` (PORT
   UObjectGlobals.cpp:220-222, upstream rule) correctly excludes the CDO, so a
   not-yet-instanced `ItemDatabaseSubsystem` yields nullptr — matching the mod's
   own status text "Item database not ready. Load a save first."
5. The zzdiag-v2 crash is the same null wrapper: `db:GetClass()` →
   `construct_uclass` derefs `get_remote_cpp_object()->GetClassPrivate()` with
   **no null check** (PORT LuaUObject.cpp:490-494) → CE-108255-1. Not chain
   corruption; `FindFunction` doesn't even exist in the port (0 grep hits).
6. Console evidence *against* the GATE-1/FName theory for this symptom: pool
   admitted, ctor signature-matched and behaviorally verified (ByteProperty
   id 0x3 == pool), and Conv_NameToString resolved by name (§Task 2 end). D1's
   fail-closed refusal would also degrade *every* reflected call, not just
   this one.

(Agent CA's independent trace — `agent-CA-ufunc-trace.md` — reaches the same
conclusion set: error-site hpp:517/518, null-wrapper provenance, pcall-shielding,
`construct_uclass` crash mechanism, and the `db == nil` false positive. My diff
adds: the byte-identity proof of the whole resolution chain against upstream
3.0.1 + UEPseudo@38e7171, and the D1-D6 inventory.)

**Latent risk worth one line in l26 (not this bug):** if Seeds admission ever
refuses (D1), `FName(str, FNAME_Find)` silently becomes NAME_None
(NameTypes.hpp:241, unchanged from upstream) and every reflected method call
degrades to the opaque `attempt to call a userdata value` — the current console
witness cannot distinguish that from other misses.

---

## Task 4 — recommended minimal port-side actions

Nothing in the name→UFunction path should be reverted — it is already
upstream-identical, and its one console divergence (D1) is verified working.
Minimal, safety-model-consistent actions:

1. **Fix the witness, not the runtime**: zzdiag v3 (and any future diag) must
   gate on `pcall(function() return db:IsValid() end)` like the mod does, and
   the chain must be re-run **in-game with a save loaded** before any
   UFunction-resolution defect is declared. (`zzdiag-v3/main.lua:34` currently
   repeats the `db == nil` false positive.)
2. **Null-harden `construct_uclass`/`construct_fname`** (PORT
   LuaUObject.cpp:484-494): throw the standard Lua error on null remote before
   deref, exactly like the Call metamethod does — this converts a repeatable
   CE-108255-1 into a catchable Lua error. Pure guard, no engine access added.
3. **Optional upstream-parity improvement** (small, contained): in
   `handle_unreal_property_value`'s miss branch (PORT cpp:2331-2338 / L22
   :2397), push an error-carrying wrapper (or log `[Lua] UFunction '<name>' not
   found on <class>`) instead of the bare `special_invalid_ptr` wrapper, so a
   real resolution miss stops surfacing as opaque `attempt to call a userdata
   value`. Keep the wrapper itself for upstream chaining compatibility.
4. **One-line diagnosability for D1**: log once (not per-call) if
   `FName::ConstructorInternal` is unready when `construct_with_string` runs,
   so a future Seeds refusal is visible instead of silently degrading to
   NAME_None.

No changes to `GetFunctionByNameInChain`, `TFieldRange`, `prepare_to_handle`,
the Seeds admission flow, or the exec-only/text-guard model are indicated by
this symptom.

---

### Divergence count summary

- Path-relevant divergences found: **6** (D1-D6 above; D3-D6 behavior-neutral,
  D2 backend-only, D1 gated-binding).
- Culprit for the console symptom: **none of the divergences** — the symptom is
  upstream-identical null-wrapper behavior from a FindFirstOf miss (subsystem
  not instanced at main menu), misread by a `db == nil` check that can never
  detect it; the crash was the unchecked `construct_uclass` deref on the same
  null wrapper.
