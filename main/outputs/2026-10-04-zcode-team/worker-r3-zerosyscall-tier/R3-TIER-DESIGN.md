# R3-TIER-DESIGN.md — the zero-syscall interception tier (UFunction::SetFuncPtr), the Item Spawner's dependency map, and the R3-only verdict

Worker: worker-r3-zerosyscall-tier, 2026-10-04. PROPOSALS ONLY: nothing built, staged,
executed; no console contact; no nested subagents. Read-order contract honored:
`TEAM:worker-vtable-swap-design/VTABLE-SWAP-DESIGN.md` (VSD) read first; its R3 ranking
(VSD:39-41, VSD:129 row 6, VSD:289-294) is the contract this document deepens.

Path abbreviations (reused from VSD:11-16 plus two new):
- `TEAM=` `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/`
- `SRC=` `.../.superpowers/native-dependency-spike/native-coherent-runtime-1/source-reconstruction-final-4/` (canonical reconstruction, per `TEAM:worker-all-signatures-inventory/SIGNATURES-INVENTORY.md:8-9`)
- `SINI=` `TEAM:worker-query-s2-arm/staging/UnrealInitializer.cpp` (the staged initializer)
- `IS=` `TEAM:worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` (the spawner, 746 lines, sha `f586043a…658` per `TEAM:worker-itemspawner-dryrun/REPORT.md` "Inputs verified")
- `LUA=` `SRC:port/ue4ss/src/Mod/LuaMod.cpp`
- `RECIPE=` `.../.superpowers/native-dependency-spike/native-coherent-runtime-1/compile-recipes.json` (357 units, read this session)

---

## 0. Executive summary

1. **R3 is real, in-tree, and already platform-neutral.** `UFunction::SetFuncPtr`
   (`SRC:port/unreal/src/UFunction.cpp:33-36`) is ONE pointer-width store into a heap
   UFunction object at `MemberOffsets["Func"]` (default `0xD8` for 5.06+, RECIPE-derived
   setters). No mprotect, no vtable write, no exec write, no syscall. There are ZERO
   platform guards in `UFunction.cpp` / `UFunctionStructs.cpp` — the "Windows-only"
   hypothesis is FALSE; the Lua `RegisterHook` native branch already routes to it on the
   PS5 build (`LUA:4618-4632`).
2. **The R3 core needs NO source diff.** Every piece (hook install, original save,
   pre/post callbacks, out-parm copyback, ProcessInternal fallback) already exists
   (`UFunction.cpp:78-132`, `SRC:port/unreal/src/UFunctionStructs.cpp:163-333`). The
   composition work is confined to `LuaMod.cpp` (marshal method), the staged initializer
   (init re-route), and one settings string.
3. **The Item Spawner's ONLY interception dependency is A9 `ExecuteInGameThread`**
   (`IS:695,700,705,710,716,725`). Keybind is port-owned input (`LUA:1949-1956` →
   `SRC:port/first/src/Input/Handler.cpp:105`); reflection and spawn are direct
   port-API calls that need no interception at all. A10 (`RegisterLoadMapPreHook`,
   `IS:731`) is a second, optional hook dependency that dies under R3-only but is
   pcall-guarded.
4. **The real blocker is init, not the spawner:** under both-fail the composed runtime
   dies at `RegisterEngineTickPreCallback` → ERROR_ID → throw (`SINI:1383-1387`,
   analysis `TEAM:worker-engine-tick-fallback/TICK-FALLBACK-DECISION.md:59-71` §2) —
   before any Lua, before F7. R3-only therefore REQUIRES re-routing the
   GameThreadInitializer pump + query-harness dispatch (`SINI:1383-1397`) onto the R3
   marshal. This is the design's central new requirement (§4.4).
5. **Verdict (§6):** the acceptance ladder can NOT complete on R3 as composed today
   (init fatal). It CAN complete for the Item Spawner's visible-menu + spawn rungs with
   the §4/§5 diff set and IF the marshal calibration finds a viable carrier UFunction —
   an unproven in-game fact this design turns into a falsifiable protocol, not a claim.
   Rung A10 and any global-visibility feature stay dead on R3.

---

## 1. Task 1 — exactly what SetFuncPtr does (target of the write, consumers, port support)

### 1.1 The write itself

`UFunction::SetFuncPtr` (`SRC:port/unreal/src/UFunction.cpp:33-36`):

```cpp
auto UFunction::SetFuncPtr(UnrealScriptFunction NewFuncPtr) -> void
{
    GetFunc() = std::bit_cast<std::remove_reference_t<decltype(std::declval<UFunction>().GetFunc())>>(NewFuncPtr);
}
```

- `UnrealScriptFunction` = `void(*)(UObject* Context, FFrame& TheStack, void* RESULT_DECL)`
  (`SRC:port/unreal/include/Unreal/UFunctionStructs.hpp:13`) — plain SysV function pointer.
- `GetFunc()` (`SRC:port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp:356-359`)
  returns `std::function<...>*&` via the generated layout accessor `GetFunc419()`
  (`SRC:port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UFunction.hpp:42`),
  whose body resolves `MemberOffsets.find(STR("Func"))` and
  `ptr_cast<std::function<...>**>(this, offset)` — loud `std::runtime_error` if the offset
  is absent (`SRC:port/unreal/generated_include/MemberVariableLayout_SrcWrapper_UFunction.hpp:194-211`).
- **Net effect: `*(void**)((uint8_t*)this + FuncOffset) = (void*)NewFuncPtr`** — a single
  aligned pointer store inside the UFunction object. Version defaults registered per
  engine generation: `Func=0xD8` for 5_04/5_06/5_07/5_08
  (`SRC:port/unreal/generated_include/FunctionBodies/5_06_MemberVariableLayout_DefaultSetter_UFunction.cpp:45-49`,
  same value in 5_04:46-48 and 5_07:48; 4_x generations use `0xB0`, e.g.
  `4_20_…:46-48`). The `emplace` is guarded by a find-first idiom, so a real dump's
  offset (if ever loaded) wins over the default.

### 1.2 Where the written memory lives (the zero-syscall argument)

UFunction instances are heap UObjects allocated by the engine (GUObjectArray-managed
objects; the port iterates them data-side today, VSD:125 row 2 citing
`SIGNATURES-INVENTORY.md:123,132`). Heap-object writability CLASS is proven on this exact
target by igf4: 96 B read + same-value writeback at `0x97ba110` inside the prot=3 L7 tail
(`CON:igf6-results.jsonl` igf4 seq 6 `gua_read`, seq 7 `gua_writeback`
`"restored":1`, cited VSD:97-103). Unlike VSD's R1/R2/R4 tiers, **no page re-protection
is ever needed: the store lands in already-RW memory. Zero syscalls.** The retained
honesty gate carries over unchanged: the `[0x9568000,0x96dc000)` prot=1 hole inside L7
(VSD:105-111) makes a per-target fresh mapscan of the covering span before the first
FuncPtr store MANDATORY (same C-2 discipline, `EXEC-WRITABILITY-PROBE-DESIGN.md` C-2, via
VSD:88-89). GUA at `0x97ba110` sits in the prot=3 tail — a different, proven span; do not
infer UFunction spans from it.

### 1.3 What hooking it enables — the full in-tree machinery

`UFunction::GetFunctionHookData()` (`UFunction.cpp:78-89`):

- Looks up `Internal::HookedUFunctionMap` (a `std::unordered_map<const UFunction*,
  UnrealScriptFunctionData>`, `SRC:port/unreal/include/Unreal/UFunctionStructs.hpp:85`;
  storage `static Internal::HookedUFunctionMap GHookedScriptFunctions{}`,
  `UFunctionStructs.cpp:156-161`).
- On first touch: emplaces the entry saving the CURRENT FuncPtr as original
  (`UFunction.cpp:85`), then `SetFuncPtr(&Internal::UnrealScriptFunctionHook)`
  (`UFunction.cpp:86`) — **this is the entire "hook installation": one store.**
- Public registration surface: `RegisterPreHook` / `RegisterPostHook`
  (`UFunction.cpp:91-101`), `…ForInstance` variants with instance filtering
  (`:103-113`), `UnregisterHook` with mid-execution deferral
  (`UFunctionStructs.cpp:30-50`), `UnregisterAllHooks` which RESTORES the original
  pointer (`UFunction.cpp:121-132`).
- The hook body `Internal::UnrealScriptFunctionHook`
  (`UFunctionStructs.cpp:163-333`) per invocation:
  - Resolves the UFunction from `TheStack.CurrentNativeFunction()` (with a `Code()-8`
    fallback, `:168-178`).
  - Full fidelity path (requires `TheStack.Code() && GNatives_Internal`, `:210`):
    allocates a fresh FFrame, steps all params via `StepCompiledIn`, rebuilds the
    out-parm chain, fires pre-callbacks (`:278`), invokes the ORIGINAL
    (`:287`), fires post-callbacks (`:292`), copies out values back (`:300-312`),
    frees the frame (`:323`).
  - Degraded-but-safe path when `GNatives_Internal` is unbound: callbacks fire with the
    ORIGINAL stack (`NewStackPtr = &TheStack`, `:207`) and the original is invoked with
    it (`:287`) — no re-framing, no GNatives dependency.
  - Unknown-function fallback executes `ProcessInternal` via ExecuteUbergraph's FuncPtr
    found by `StaticFindObject` (`:198-202`) — a data-side lookup, the SAME pattern as
    `SINI:1425-1432` region's ExecuteUbergraph resolution and
    `SRC:port/unreal/src/UnrealInitializer.cpp:1432`.
- `GNatives_Internal` is declared `extern COREUOBJECT_API FNativeFuncPtr*`
  (`SRC:port/unreal/include/Unreal/Script.hpp:136`) and bound data-side at
  `SINI:897-899`; slot 8's own table address remains unbound
  (`SIGNATURES-INVENTORY.md:81`) — the hook body tolerates that (guard at `:210`),
  which is exactly why R3 does NOT inherit the GNatives address risk.

### 1.4 Who reaches the hooked pointer — coverage semantics

The engine's `UObject::ProcessEvent` builds an FFrame and calls through the UFunction's
Func field; therefore a SetFuncPtr'd UFunction intercepts **every reflected invocation of
that one function**, from:

- game-initiated calls (BP events, delegates, timers), and
- port-initiated calls: the port's `UObject::ProcessEvent` is an
  `IMPLEMENT_UNREAL_VIRTUAL_WRAPPER` (`SRC:port/unreal/src/UObject.cpp:448-451`) that
  virtual-dispatches into the GAME's ProcessEvent, which then reads Func — so even the
  spawner's own `gameplay:BeginDeferredActorSpawnFromClass` call would pass through a
  FuncPtr hook of that function. (The port never calls its own resolved
  `ProcessEventInternal` for invocations — its only consumers are the detour metadata
  `SRC:port/unreal/include/Unreal/Hooks/Internal/Metadata.hpp:153`, the init assign
  `SRC:port/unreal/src/UnrealInitializer.cpp:1381`, and the capability check
  `SRC:port/ue4ss/src/UE4SSRuntime.cpp:23`.)

What R3 does NOT cover (consistent with VSD:129 row 6, VSD:330-337):
- EngineTick (C++ virtual, not a UFunction) — VSD:41.
- Global ProcessEvent / ProcessInternal / ProcessLocalScriptFunction visibility —
  per-function only. Note the Lua **script-function** RegisterHook branch
  (`LUA:4633-4650`) registers into `m_script_hook_callbacks`, which is drained by
  `script_hook` registered on the ProcessLocalScriptFunction/ProcessInternal DETOURS
  (`LUA:7670-7675`, body `LUA:6741-6891`) — **that path dies under both-fail.** Script
  functions are only R3-reachable if their FuncPtr (== ProcessInternal) is ALSO swapped
  per-function, relying on the `:198-202` fallback — an extension this design scopes but
  does not require for the spawner (§4.6).
- Direct `E8` call sites (FName ctor, StaticConstructObject) — VSD:323-329; irrelevant
  to R3's role.

### 1.5 The port's existing support — route status: NATIVE, LIVE, not Windows-only

- `UFunction.cpp` / `UFunctionStructs.cpp` contain no `_WIN32`/ms_abi/`#ifdef` platform
  gates (whole-file reads, §1.1/§1.3); the only PROSPERO-conditional in the chain is the
  overlay note at `SRC:port/unreal/src/Hooks/Hooks.cpp:34-36` (shared_ptr atomics), which
  does not touch the FuncPtr path.
- Live consumers already compiled into the PS5 build:
  1. Lua `RegisterHook` native branch — `LUA:4618-4632`: if
     `func_ptr && func_ptr != UObject::ProcessInternalInternal.get_function_address() &&
     HasAnyFunctionFlags(FUNC_Native)` → `RegisterPreHook`/`RegisterPostHook`
     (`LUA:4624-4625`) → SetFuncPtr. **The R3 route for E4-style Lua hooks is already
     the shipped path for native functions** (VSD:289-294 proposed "routing"; the
     routing EXISTS in this reconstruction).
  2. C++ API `RegisterHook(UFunction*, …)` — `SRC:port/unreal/src/UObjectGlobals.cpp:1122-1133`
     same native branch.
- Degradation fact: both consumers discriminate native-vs-script by comparing against
  `ProcessInternalInternal.get_function_address()` (`LUA:4619,4633`). If the ProcessInternal
  address is unbound (FLAG-3 needs RX reads, `SIGNATURES-INVENTORY.md:231-236`), the
  compare degenerates; the native branch's `func_ptr &&` still passes for real native
  pointers, so **native RegisterHook keeps working R3-only even with FLAG-3 unresolved**;
  script RegisterHook falls to the loud error branch (`LUA:4651-4660`) instead of
  silently mis-hooking. This is the good failure mode.

---

## 2. Task 2 — the Item Spawner's ACTUAL interception dependencies

Step-by-step map of the F7→visible-menu→spawn chain (line cites `IS=`; requirement IDs
from `TEAM:worker-modreqs/RUNTIME-REQUIREMENTS.md:16-40,70-86`):

| # | Spawner step | Evidence | Needs interception? | Mechanism under R3-only |
|---|---|---|---|---|
| 1 | Load-time: 6× `RegisterKeyBind` (`IS:694-729`) | A8; registrations available, `TEAM:worker-itemspawner-dryrun/REPORT.md` findings 1 | **NO.** Routes to the port's OWN input pipeline: `LUA:1949-1956` `m_program.register_keydown_event` → `SRC:port/first/src/Input/Handler.cpp:105`. Callbacks fire on the port's input/poll thread — which is exactly why step 3 exists. (Real PS5 device delivery remains A8's standing caveat, out of R3 scope.) | Port-owned polling; no engine contact |
| 2 | `RegisterLoadMapPreHook` (`IS:731`) | A10 | **YES — and DEAD under R3-only** (LoadMap is a C++ virtual hook, `SINI:1315-1324` region; vtable/detour tier). pcall-guarded at `IS:731`, so the mod still loads; consequence: no auto-reset on map change → stale-UI/stale-`items` hazard after loading a new save while menu state exists (§6 honesty) | None; degrade + document |
| 3 | `ExecuteInGameThread(closure)` (`IS:695,700,705,710,716,725`) | A9 — the single observed hard failure (`attempt to call a nil value … 'ExecuteInGameThread'` at `IS:695`, `RUNTIME-REQUIREMENTS.md:16`) | **YES — THE hook dependency.** Today: EngineTick or ProcessEvent detour drain (`LUA:4743-4765`), default EngineTick (staged ini `TEAM:worker-itemspawner-bundle/bundle/ue4ss/UE4SS-settings.ini:8`; struct default `SRC:port/ue4ss/include/SettingsManager.hpp:45`; parse `SRC:port/ue4ss/src/SettingsManager.cpp:151-158`). Both detours die under both-fail → **R3-compatible replacement required: §4 FuncPtr-marshal** | R3 marshal (new) |
| 4 | `require("UEHelpers")`, `FindFirstOf("PlayerController"/"ItemDatabaseSubsystem")` (`IS:15-24,64`) | A2/A3 | **NO.** Object enumeration is data-side (GUObjectArray iteration proven); port-owned API | Direct port API |
| 5 | `StaticFindObject`, `StaticConstructObject`, `FName(name)` (`IS:44,50,283,499,506,516`) | A4/A5/A6 | **NO.** Lookup is data-side; StaticConstructObject is a port-initiated call INTO the engine (needs worker-2 facts, not interception — `RUNTIME-REQUIREMENTS.md:28,35-36`) | Direct port API |
| 6 | Property reads/writes (`pc.Pawn` `IS:31`; `pickup.PickupAmount = …` `IS:325-326`; `ui.root.WidgetTree` `IS:525`) | B2/B3 | **NO.** Offset-based memory access (worker-2 facts) | Direct port API |
| 7 | Reflected method calls — `K2_GetActorLocation`, `GetControlRotation`, `LineTraceSingle`, `GetItemIDFromDeprecatedEnum`, `GetInteractablePickupIDActorPath`, UMG setters (`IS:32-33,163-168,208,233-234,245-258,364-483,594-631`) | B4 | **NO.** Port-initiated calls through `IMPLEMENT_UNREAL_VIRTUAL_WRAPPER` (`UObject.cpp:448-451`) into the game's ProcessEvent. No interception required for THEM to work; (they would additionally pass through a FuncPtr hook of the same function, §1.4) | Direct port API |
| 8 | Spawn: `gameplay:BeginDeferredActorSpawnFromClass(...)` + `FinishSpawningActor` (`IS:294-303`) | C-rung engine callables | **NO.** Same B4 mechanism | Direct port API |
| 9 | `LoadAsset` fallback (`IS:285`) | A7 | NO (port service; missing-new per `RUNTIME-REQUIREMENTS.md:38`) | Direct port API |

**Answer to the acceptance question:** the spawner needs interception ONLY for step 3
(A9) — everything from keypress to a visible UMG menu and a spawned pickup is direct
port API on top of worker-1/worker-2 facts. Step 2 (A10) is a second, optional hook
dependency that R3 cannot carry and the mod already tolerates. There is no hidden
EngineTick need inside the spawner chain itself — but see §4.4: the LADDER's init phase
carries a non-negotiable EngineTick-shaped dependency that must be re-routed before the
spawner is ever reached.

---

## 3. Why the existing ProcessEvent cross-fallback is not an R3 answer (honest assessment)

The in-tree ProcessEvent method (`EGameThreadMethod.ProcessEvent`, `LUA:4673-4676`,
availability `UE4SSRuntime.cpp:18-24` = `ProcessEventInternal.is_ready()`) drains
`m_game_thread_actions` from `process_event_hook` (`LUA:4413-4461`), which is a callback
on the **EDetourTarget::ProcessEvent detour** (`SRC:port/unreal/src/Hooks/Hooks.cpp:91-96`
→ `Metadata.hpp:153`). Under both-fail that detour has no installable backend (PLH =
exec write; VSD R4 = D-1 mprotect; both dead). Two additional traps, stated so no future
worker trusts the flag:

- **`ProcessEventAvailable` is address-availability, not arming.** `is_ready()` only
  means the init scan assigned an address (`UE4SSRuntime.cpp:18-24`; lazy detour note
  `:15-17`); the throw happens later at `EnsureHooked` (`SRC:port/unreal/include/Unreal/Hooks/Internal/DetourInstance.hpp:158-163`,
  ERROR_ID `:287-296`). A Lua mod checking `ProcessEventAvailable` could see TRUE and
  still fail at first `ensure_process_event_hooked` (`LUA:4536-4547`). Same for
  `EngineTickAvailable` (`UE4SSRuntime.cpp:10-16`).
- The `WithDelay` siblings' auto-fallback (`LUA:4796-4841,4954-4996,5126+`) alternates
  between exactly these two dead candidates — the fallback ladder needs a third rung, not
  a preference swap (contrast `TICK-FALLBACK-DECISION.md` §3, which correctly kept
  EngineTick for the W-works world).

Hence a genuinely new R3 carrier is required: **§4.**

---

## 4. The R3-compatible marshal: "FuncPtr-marshal" (design to build-ready depth)

### 4.1 Concept

One native UFunction F ("carrier") is R3-hooked. Its hook body doubles as the
game-thread pump: it drains the SAME `m_game_thread_actions` queue via the SAME
`process_simple_actions` (`LUA:4113-4436` machinery), gated exactly like
`process_event_hook`'s PROSPERO prologue — `Unreal::IsInGameThreadRaw()` return-early
(`LUA:4418-4425`) and `NativeLuaActionDispatchScope` single-owner dispatch
(`LUA:4426-4430`). Everything downstream (`ExecuteInGameThread` enqueue `LUA:4742-4765`,
delayed actions, pending-queue swap `LUA:4441-4460`) is REUSED UNCHANGED; only the drain
site's registration changes.

Registration API: `F->RegisterPostHook(marshal_body, nullptr)` (`UFunction.cpp:97-101`) —
post-hook so the original function always executes first and unmodified; the marshal body
never touches parms, never alters control flow, returns normally. Original invocation is
the hook body's own `:287` call — semantics identical to today's forwarding detours but
with zero instruction patches.

### 4.2 Carrier selection — falsifiable calibration protocol (no per-frame UFunction is assumed to exist)

No UE title is guaranteed a per-frame NATIVE UFunction (per-frame work is C++-virtual,
unreflected). The design therefore treats "a viable carrier exists on Townfall" as the
tier's ONE open in-game fact (F-1) and specifies its measurement:

1. **Enumerate** candidate UFunctions data-side: GUObjectArray iteration (proven;
   VSD:125 row 2 citing `SIGNATURES-INVENTORY.md:123,132`), filter
   `HasAnyFunctionFlags(FUNC_Native)` (`LUA:4620` shape) and Func non-null.
2. **Arm trial hooks** with per-UFunction invocation counters in module .bss atomics:
   `GetFunctionHookData()` + a counting pre-callback (`UFunction.cpp:91-95`). Cap the
   trial set (bounded budget, e.g. 64) — each arm is one pointer store; the
   `HookedUFunctionMap` already saves each original (`UFunction.cpp:85`).
3. **Measure** during a loaded save for N frames-of-reference (port-clock seconds):
   invocations/second, thread-id stability via `NativeGameThreadIdentity.publish_current()`
   (the HookedEngineTick publication logic, `SINI:152-180` — reused, not re-invented).
4. **Select** the carrier: max invocations/second with ≥1 steady-state rate and
   game-thread-stable identity; refuse carriers whose invocation is player-action-gated
   (rate collapses when idle).
5. **Demote** all other trials: `UnregisterAllHooks()` restores originals
   (`UFunction.cpp:121-132`).
6. **Record** the outcome (carrier full name, rate, thread evidence) in the module's
   saved state — the calibration is itself the F-1 falsifier: zero qualifying carrier
   ⇒ F-1 false ⇒ R3-only spawner verdict becomes NO, loudly, with the recorded trial
   table as evidence.

Input-driven carriers (pad-routed input event UFunctions) are an acceptable secondary
class IF their hook bodies only run the marshal drain when a queued action exists — the
spawner's closures are per-keypress, so a keypress-correlated carrier actually matches
the need with lower overhead. The protocol above measures both classes identically.

### 4.3 Safety properties (carried over from the in-tree design + VSD discipline)

- **Write safety:** single aligned pointer store into mapped-RW heap memory (§1.2);
  per-target fresh mapscan gate before the FIRST store of the session (§1.2 mandatory
  gate). No exec, no vtable, no syscall anywhere in the tier.
- **Identity pre-check (R3 port of VSD §2.1):** before `GetFunctionHookData()` swaps,
  read back `GetFuncPtr()` (data read, prot includes R) and require:
  non-null; `!= ProcessInternalInternal.get_function_address()`; FUNC_Native set.
  Refusal ⇒ loud log + no store (mirrors `LUA:4618-4620`'s own gates — the R3 tier gets
  the PLH-missing semantic validation for free because the check is pure data).
- **Restore:** `UnregisterAllHooks` restores the exact saved original
  (`UFunction.cpp:129`); mid-execution unregistration is deferred
  (`UFunctionStructs.cpp:35-49`). Teardown honesty per VSD §4.5 applies: process-lifetime
  hooks, no unload-restore promises.
- **Reentrancy:** PROSPERO invocation scopes of the detour world do NOT apply here (this
  is not a DetourInstance); instead the machinery is the hook body's own `bIsMidExecution`
  flag (`UFunctionStructs.cpp:273,326`) and the `m_is_processing_actions` +
  pending-queue discipline (`LUA:4432-4434,4444-4451`). Marshal body must not enqueue;
  it only drains — the enqueue side (`LUA:4732-4765`) already takes
  `m_thread_actions_mutex`.
- **Thread honesty:** the drain runs on the carrier's invoker thread; the
  `IsInGameThreadRaw()` gate (`LUA:4418-4425` shape) plus first-publication identity
  (`§4.4a`) is what makes that "the game thread" rather than a hope. Until identity is
  published, the marshal must DRAIN NOTHING except the publication step itself.

### 4.4 What else must ride the marshal under both-fail (the ladder's hidden EngineTick dependencies)

Under both-fail, `RegisterEngineTickPreCallback` cannot arm, and BOTH init-time
registrations throw fatally (`SINI:1383-1387` GameThreadInitializer,
`SINI:1390-1397` QueryHarnessDispatch; consequences traced end-to-end in
`TICK-FALLBACK-DECISION.md:59-90` §2 — runtime-status `failed` before mods-started).
R3-only composition therefore re-routes:

- **(a) Game-thread identity publication** — the body of `HookedEngineTick`
  (`SINI:152-180`: `NativeGameThreadIdentity.publish_current()`, cancellation check,
  first-publication-immutability) moves into the marshal's first accepted invocation.
  Ordering fix: today registration happens at init when no UFunction carrier can exist
  yet; the R3 design DEFERS arming until the calibration's carrier is found (first
  possible point: after `setup_unreal()`'s object world exists — the same post-init
  region `SINI:1245+` already searches CDOs in). The runtime's
  `mods-started` must not gate on marshal arming (Lua can load; first
  `ExecuteInGameThread` call awaits the carrier — lazy, matching the spawner's own lazy
  A9 profile, `TEAM:worker-itemspawner-dryrun/REPORT.md` finding 1).
- **(b) Query-harness dispatch** — `QueryHarnessEngineTickDispatch` (`SINI:1390-1397`)
  re-registers onto the marshal. **Semantic change, honestly declared:** the harness's
  "two consecutive accepted reads" then counts CARRIER invocations, not engine ticks.
  Harness output labels must change (`tick`→`marshal-observation`); any prior rung
  evidence phrased as "ticks" is not comparable. This is a rung-instrument change and
  belongs in the controller's acceptance notes.
- **(c) FName verification detour** — cannot arm under both-fail regardless; VSD §5 C1
  already replaces it with the FC-1 read-only validator (`VSD:300-305`), killing the
  registration at `SINI:945-958` under a settings gate. No new R3 work; listed because
  the init path will otherwise still throw there (`SINI:954-958`).
- **(d) HookedEngineTick's remaining steps** (publication-verification retirement,
  `SINI:180+`) follow the same re-route as (a).

### 4.5 Lua/settings surface

- New enum value `GameThreadExecutionMethod::FuncPtrMarshal` (next to
  `LUA:4673-4676`'s table registration) and a `EGameThreadMethod.FuncPtrMarshal` global.
- `ExecuteInGameThread` overload #2 accepts it (`LUA:4702-4706` shape); availability
  predicate = "carrier armed" (module state), NOT an AOB `is_ready()` (the §3 trap).
- The `WithDelay`/`Loop` fallback ladders (`LUA:4796-4841,4954-4996,5126+`) gain the
  third rung in order: EngineTick → ProcessEvent → FuncPtrMarshal → throw.
- Settings: extend `SettingsManager.cpp:150-160`'s literal parse with
  `"FuncPtrMarshal"`; struct default in `SettingsManager.hpp:45` stays EngineTick for
  the W-works world (per `TICK-FALLBACK-DECISION.md` §1 recommendation — R3-only is a
  DIFFERENT composed world with its own ini, not a restage of the same bytes; the staged
  ini hash caveats of `TICK-FALLBACK-DECISION.md:119-141` apply to any byte change).

### 4.6 Explicitly out of scope (recorded, not designed)

- Script-function (Func==ProcessInternal) RegisterHook under R3: today's path dies with
  the global script_hook detours (`LUA:7670-7675`). A per-function FuncPtr extension is
  POSSIBLE (the `:198-202` fallback already handles "no map entry → ProcessInternal"),
  but the spawner uses no `RegisterHook` at all (`IS` has none), so it is deferred.
- Global "see every event" observers (the asset-loaded default hook note, `VSD:294`).

---

## 5. Composition integration — exact unit diffs + argv (PROPOSAL, nothing staged)

### 5.1 Units (RECIPE, 357 units; relevant pins read this session)

| unit | source (RECIPE pin) | R3 disposition |
|---|---|---|
| `unreal-ufunction` | `port/unreal/src/UFunction.cpp` (source_sha `84a718db…0823`) | **NO DIFF** — R3 core is complete (§1.1-1.3); recompiles only if headers it includes change (none proposed) |
| `unreal-flat-ufunctionstructs` | `port/unreal/src/UFunctionStructs.cpp` | **NO DIFF** (unless §4.6 extension is later taken) |
| `luamod` | `port/ue4ss/src/Mod/LuaMod.cpp` | **DIFF 1** (§5.2): marshal method, drain registration, availability predicate, enum/settings parse |
| initializer (via `--replace-runtime-unit`) | staging `SINI` (pattern proven by composition-4's receipt flow, `TEAM:worker-composition-4/REPORT.md:18-37,105-113`) | **DIFF 2** (§5.3): init re-route (a)-(c) of §4.4 |
| `unreal-hooks` | `port/unreal/src/Hooks/Hooks.cpp` | no diff |

### 5.2 DIFF 1 — LuaMod.cpp (proposal sketch, sites cited)

1. Enum + Lua table: add `FuncPtrMarshal` to `GameThreadExecutionMethod` and the
   `EGameThreadMethod` table at `LUA:4669-4678`.
2. Availability: replace `is_engine_tick_hook_available`/`is_process_event_hook_available`
   call sites in the ExecuteInGameThread/WithDelay/Loop bodies (`LUA:4715-4730`,
   `4795-4841`, `4954-4996`, `5126-5180`) with a backend-reality predicate:
   `marshal_armed() || TickInternal.is_ready()-detour-armed || ProcessEvent-armed` —
   three-state ladder per §4.5. The §3 trap fix (arming, not address) is part of this.
3. Drain: `funcptr_marshal_hook(UObject* Context, FFrame& Stack, void* RESULT_DECL)` —
   PROSPERO prologue copied from `process_event_hook` (`LUA:4418-4430`), then the
   identical drain block (`LUA:4431-4460`, `process_simple_actions(m_game_thread_actions)`
   + delayed + pending swap). First accepted invocation additionally performs §4.4(a).
   Registered via carrier `RegisterPostHook` once calibration selects (§4.2).
4. Calibration service: bounded candidate enumeration + trial hooks + counters (§4.2);
   runs post-`setup_unreal()`, before/parallel-with `mods-started`; writes its outcome
   record into saved state.

### 5.3 DIFF 2 — staged initializer

1. Gate the `#ifdef __PROSPERO__` EngineTick registrations (`SINI:1381-1400`) behind
   backend-armed truth; on both-fail do NOT throw — emit
   `"EngineTick detour unavailable; deferring game-thread pump to R3 marshal"` and set
   the pending-pump state DIFF 1 consumes. (Loud status remains: the composition's
   runtime-status must show the deferred-pump state, never silently proceed.)
2. Kill the FName detour registration under the FC-1 settings gate (VSD §5 C1;
   `SINI:945-958`).
3. Leave LoadMap/ProcessConsoleExec/UStruct::Link registrations dead under both-fail
   (not-on-path or degraded per VSD §2 row 4).

### 5.3.1 DIFF 3 (optional, deferred with §4.6) — UFunctionStructs.cpp script-hook bridge.

### 5.4 Argv — unchanged

All three touched units compile under their EXISTING pinned recipes; no new flags, no
new imports, no new link deps (the marshal uses only in-tree symbols:
`process_simple_actions`, `IsInGameThreadRaw`, `NativeGameThreadIdentity`,
`UFunction::RegisterPostHook`). Reference argv for the heaviest-relevant unit
(`unreal-ufunction`, RECIPE, verbatim shape): `prospero-clang++ -I …(8 include roots)…
-DRC_LUA_MADE_SIMPLE_BUILD_STATIC … -DOVERRIDE_PLATFORM_HEADER_NAME=Prospero -std=c++23
-fms-extensions -O0 -fno-lto -fPIC -ferror-limit=8 … -c …/UFunction.cpp -o
@OUTPUT@/unreal-ufunction.o -MD -MF @OUTPUT@/unreal-ufunction.d` — captured in full from
RECIPE this session. Since `unreal-ufunction` has NO diff, the practical recompile set
is `luamod` + the replaced initializer unit; everything else reuses prior objects under
the `build_runtime.py` pin discipline (`build_runtime.py:35-48`). One composition-order
lesson applies verbatim: replacement sources must be visible to the fresh-dependency
pin walk — stage under the spike tree's `W/src|W/include` glob or hoist the
replace-receipt map BEFORE the fresh-unit loop (the exact composition-4 failure and
fix, `TEAM:worker-composition-4/REPORT.md:39-52,105-113`).

### 5.5 What R3 costs vs the detour world (honest deltas)

- Menu latency becomes carrier-cadence-bound (usually next-carrier-invocation, not
  next-tick) — bounded by F-1's rate measurement; if the carrier is input-correlated,
  latency is sub-perceptual for keypress-driven closures.
- Query-harness semantics change (§4.4b) — rung instruments must be relabeled.
- No LoadMap lifecycle reset (A10) — spawner keeps a stale-menu hazard after map change;
  mitigation proposal (not designed): the marshal can also observe a cheap
  world-identity read data-side per invocation, but that is future work.
- GNatives_Internal unbound ⇒ degraded re-framing (§1.3): callbacks still fire with the
  original stack; out-parm copyback fidelity for COMPLEX out params is lower. The
  spawner's only out-parm consumer is `LineTraceSingle`'s hit struct (`IS:245-258`,
  blocker B5 already flagged by dryrun, `TEAM:worker-itemspawner-dryrun/REPORT.md` §3(5))
  — B5's risk is INDEPENDENT of the hook tier (it is call-marshalling, not
  interception), but the degraded path must be listed as a co-factor until
  GNatives binds.

---

## 6. The verdict — can the acceptance ladder complete on R3 alone?

Scope of "R3 alone": no exec writes, no mprotect (D-1 rejected), no vtable swaps; ONLY
heap FuncPtr stores + port-owned input/polling + direct port API calls.

**NO — not as composed today.** The ladder never reaches Lua: init throws at the
EngineTick registration (`SINI:1383-1387`; `TICK-FALLBACK-DECISION.md:64-71`), after the
FName detour would have thrown first under both-fail (`SINI:954-958`). Two diffs
(§5.2-5.3) are PREREQUISITES, not enhancements.

**Conditionally YES — for the Item Spawner's visible-menu + spawn rungs — with the diffs,
on three stated conditions:**

1. **F-1 (new in-game fact):** the calibration (§4.2) finds a carrier UFunction with
   acceptable game-thread cadence on Townfall. This is genuinely unproven — no retained
   evidence names ANY native UFunction's live invocation rate on this title. The protocol
   is the falsifier; its outcome record is the evidence either way.
2. **The re-routed pump satisfies the ladder's game-thread requirements** (identity
   publication `SINI:152-180` semantics; harness relabeling §4.4b accepted by the
   controller as an instrument change).
3. **Worker-1/worker-2 facts land** (layout bytes B0 — the dryrun's what-breaks-first
   ordering stands, `TEAM:worker-itemspawner-dryrun/BLOCKERS.md` via REPORT §3: the
   spawner dies before any hook tier matters until B0 closes).

**Permanently dead on R3 (do not budget):** global ProcessEvent/ProcessInternal
visibility; script-function RegisterHook as shipped (`LUA:7670-7675` path); A10
LoadMap lifecycle (`IS:731` degrades to no-op; stale-menu hazard recorded); EngineTick
as a hook. Any rung whose acceptance evidence requires "saw every event" is unratable
on R3 — consistent with the rubric's missing-evidence rule
(`docs/QUALITY-RUBRIC.md` discipline echoed in VSD §6.7).

**Bottom line:** R3 is a real, complete, zero-syscall interception tier whose spawner
coverage is exactly one service wide (A9) — and that is SUFFICIENT for the spawner's
chain (§2), because every other step was never an interception problem. The tier's
fate hangs on one measurable fact (F-1) and two small diffs, not on any memory-protection
gamble.

---

## 7. Explicit non-claims

- No claim any SetFuncPtr hook, marshal, or calibration has run in-game; F-1 is OPEN.
- No claim the composed runtime currently boots under both-fail (§6 says the opposite).
- No claim about UE 5.6-specific Func offsets beyond the RECIPE defaults (0xD8 for the
  5_04-5_08 setters, read this session); the accessor's loud-throw is the runtime guard.
- No claim that `RegisterHook` on the PS5 build has ever fired end-to-end; only that the
  routing exists in the reconstruction (`LUA:4618-4632`).
- Nothing staged, nothing executed, no console contact; all diffs are proposals under
  this worker's directory.
