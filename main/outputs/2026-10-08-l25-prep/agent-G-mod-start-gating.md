# Agent G — Lua mod-start gating analysis (L25 prep, 2026-10-08)

READ-ONLY source trace. Sources analyzed (all absolute paths):

- Current runtime source (l24): `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l24-runtime/typescan-r3/candidate/src/` — `UnrealInitializer.cpp` (sha prefix 78c16fb-lineage successor; contains the exact `[R3]` strings seen in the live log at :2227-2228, confirming this is the unit that runs).
- Current program/mod units: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-runtime/thread/src/UE4SSProgram.cpp` (sha 030407d7351e) and `.../thread/src/LuaMod.cpp` (sha fc9589dbb81e) — pinned by `typescan-r3/CHANGES.json → source_selection → l23-build-r2/SOURCE-SELECTION.json`.
- Composed lineage: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l24-package/lineage/` (`lifecycle/runtime_module_entry.c`, `Platform/NativeProgramContext.hpp`).
- Upstream composition source referenced by those trees: `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/` (`bootstrap/ue4ss_backend.cpp`, `port/unreal/src/UObjectArray.cpp`, `port/unreal/src/UObject/UFunction.cpp`, `port/unreal/include/Unreal/Core/Containers/Array.hpp`, audit copy of `FMemory.cpp`, `port/first/include/DynamicOutput/FileDevice.hpp`).
- Reference artifacts: l21-audit source-context `UE4SS-settings.ini` (b980cea75c46), `runtime-status.txt` (d813b7ab2036), `L22Diagnostics/Logging.hpp` (l22-runtime).

Line numbers below are from these exact files.

---

## 1. Exact chain: "Adding shutdown listener" → Lua mod start

Boot chain (who decides, who waits):

1. `module_start` → `StartActivatedRuntimeWithAck` (lineage `lifecycle/runtime_module_entry.c:17`, flow comment :22-27) → `PrepareActivatedUE4SSRuntime(...)` + `owner->start()`.
2. `PrepareActivatedUE4SSRuntime` constructs the backend with `enable_buffered_mods=true` and calls **`program_->start_native_mods()` BEFORE `init()`** (`bootstrap/ue4ss_backend.cpp:62-68`, ctor call :33-38). `start_native_mods` (`UE4SSProgram.cpp:591-634`) runs `setup_mods()` (prints "Setting up mods...", builds the mod inventory from the activation's `mod_source->lua_mods()`, `UE4SSProgram.cpp:1839-1859`), `install_cpp_mods()`, then sets **`m_native_mods_started=true`** (:624). This must precede `init()` or it is rejected (`CheckNativeModStartup`, `Platform/NativeProgramContext.hpp:44-54` → "Native mods must be enabled before UE4SS initialization").
3. `owner->start()` runs `UE4SSBackend::initialize` → **`UE4SSProgram::init()`** (`ue4ss_backend.cpp:44`) on the serialized init thread ("UE4SS-InitThread", `UE4SSProgram.cpp:644`).
4. `init()` (:642) → `setup_unreal()` (:661) → **`Unreal::UnrealInitializer::Initialize(config)`** (:1349).
5. `Initialize()` (`typescan-r3/candidate/src/UnrealInitializer.cpp:1816`) waits for, in order: ≥10000 GUObjectArray elements (:1874-1888); `KismetStringLibrary`, `Conv_NameToString`, and its CDO (:1892-1935); required-objects constructed (:2050-2057); CDO lookups for Object/Struct/GameEngine/GameModeBase|GameMode/Actor/GameViewportClient (:2071-2118). It resolves hook addresses incl. the R3 refusal block (:2214-2230, the two `[R3]` lines in the live log), the type system (`store_all_object_types()` :2343-2347 → "[TypeScan] scan-complete" `TypeChecker.cpp:897`, "FText size detected" :902, "FSoftObjectPath layout" :915), then calls **`PostInitialize`** (:2439).
6. `PostInitialize` (:461): "Post-initialization: GMalloc: ..." (:468) → searcher-pool block skipped because `bUseUObjectArrayCache=false` (ini) → **"Adding shutdown listener" (:527)** → `UObjectArray::AddUObjectDeleteListener(&FShutdownDeleteListener...)` (:528) → "Starting callback garbage collector!" (:542) → StaticConstructObject post-callback registration (:546+) → FVector/FQuat/FFieldVariant/FText/FName size prints → returns.
7. Back in `init()`: "Unreal Engine modules (...)" (:663), `fire_unreal_init_for_cpp_mods()` (:690), `setup_unreal_properties()` (:691), `share_lua_functions()` (:694), `m_is_program_started=true` + `on_engine_initialized` callback (:696-703), then **`m_event_loop = std::jthread{&UE4SSProgram::update, this}`** (:715).
8. `update()` (:1584) on the new UE4SS-UpdateThread → **`on_program_start()`** (:1598). This is the function that decides mod loading. It waits for NOTHING game-side — no game thread, no GEngine instance, no world, no first tick.
9. `UE4SSProgram::on_program_start()` (:1449): on PROSPERO the only gate is **`allow_mod_callbacks = RC::Platform::ShouldRunNativeModCallbacks(true, m_native_mods_started)`** (:1562-1565; `Platform/NativeProgramContext.hpp:37-41` returns `native_mods_started` because `native_platform=true`). If true: **`install_lua_mods()` (:1569) → `LuaMod::on_program_start()` (:1570) → `fire_program_start_for_cpp_mods()` (:1571) → `start_lua_mods()` (:1572)**.
10. `start_lua_mods()` (:2233) → `start_mods<LuaMod>()` (:2050). On PS5 the native-source branch (:2054-2064) starts exactly the mods in `mod_source->startup_mod_names()` (no mods.txt/enabled.txt parsing on this path — those are the disk fallback :2072+).
11. `LuaMod::start_mod()` (`LuaMod.cpp:7527`): async thread, loads `main.lua` through the native chunk path (m_native_source branch :7511-7516), and on success prints **"[l22] Lua mod started: {} (main script returned)" (:7557)** then queues the L23 game-thread self-test (:7560 → :130-148).

Summary: mod start is decided by `UE4SSProgram::on_program_start` on UE4SS's own update thread, gated only by (a) `init()` having completed and (b) `m_native_mods_started` (set by the bootstrap before init). The engine-side waits are all inside `Initialize()` (object count + CDO existence) — there is no wait for game thread, GEngine, world, map load, or first tick anywhere between "Adding shutdown listener" and mod start.

## 2. Does mod start depend on HookLoadMap / HookInitGameState? **NO.**

What each hook actually gates:

- **`bHookLoadMap`** gates only the resolution+assignment of `UEngine::LoadMapInternal` (`UnrealInitializer.cpp:2120-2128`). Its consumers are exclusively the LoadMap detour and Lua `RegisterLoadMapPreHook/PostHook` callback delivery (`lua_load_map_pre/post_callback`, registered at `LuaMod.cpp:8112-8114`). Under R3 marshal mode those eager registrations are skipped anyway (`LuaMod.cpp:8052-8066`) and replaced by a one-shot deferred arm `try_deferred_loadmap_arm` (:4820-4838, triggered from the marshal drain :4895); a refused arm is loud and its own Error line says "mod loading and every other surface are unaffected" (:4838). Even in the hypothetical non-R3 world, a failed `RegisterLoadMapPreCallback` returns ERROR_ID which "this TU ignores" (:8033 comment) — still no mod-start impact.
- **`bHookInitGameState`** gates only `AGameModeBase::InitGameStateInternal` resolution (`UnrealInitializer.cpp:2255-2263`). Its consumers are the InitGameState pre/post Lua callbacks — which under R3 "stay unregistered for this process lifetime" per the deferral warning text itself (`LuaMod.cpp:8064`).

Neither setting is consulted by `start_native_mods` (:591), `setup_mods` (:1839), `install_lua_mods` (:1907/1950), `start_mods` (:2050), or `LuaMod::start_mod` (:7527).

**Consequence for the A/B test:** both hooks disabled CAN still produce a mod start — "[l22] Lua mod started" remains reachable. If v2 (both off) shows "no crash + no mod start", the cause is NOT the hooks being off; experiment-matrix outcome C's premise ("one of the two hooks is REQUIRED for mod boot") is not supported by this source. The only things those keys change in this build are two address-assignment blocks (:2120-2128, :2255-2263) and, downstream, whether Lua LoadMap/InitGameState callbacks can ever deliver.

## 3. R3 marshal pump: attach point, thread, ExecuteInGameThread

- With `UE4SS_R3_MARSHAL_MODE=1` (in-file default, `UnrealInitializer.cpp:197-199`), the initializer compiles OUT both EngineTick detour registrations and prints the two `[R3]` lines via `R3MarshalGate::refuse_registration_site` (:201-217, call sites :2227-2230), setting `pump_pending=true`.
- The pump does **not** attach to a detour. `arm_funcptr_marshal_drain` (`LuaMod.cpp:5032-5056`) registers the drain `funcptr_marshal_hook` as a **post-hook on a carrier UFunction**; `UFunction::RegisterPostHook` (`port/unreal/src/UObject/UFunction.cpp:78-101`) saves the current FuncPtr and `SetFuncPtr(&Internal::UnrealScriptFunctionHook)` (:86) — one aligned pointer store into already-RW heap memory, no code write.
- Carrier selection: `r3_marshal_calibration_consumer` runs at the top of `LuaMod::on_program_start` (`LuaMod.cpp:8001`, def :5200-5238). Ranked native candidates "Actor:ReceiveTick"/"AnimInstance:BlueprintUpdateAnimation" are refused by `r3_marshal_carrier_precheck` (requires FUNC_Native, :4998-5021 — BP events lack it), then `r3_marshal_arm_event_carriers` (:5096-5170) scans GUObjectArray for Blueprint OVERRIDE UFunctions of `/Script/Engine.Actor:ReceiveTick`, `/Script/Engine.AnimInstance:BlueprintUpdateAnimation`, `/Script/UMG.UserWidget:Tick` (:5110-5114) and arms them (script overrides; Func = VM entry; the engine-driven ProcessEvent→Invoke is intercepted while the original Func still runs the BP body).
- **Thread:** `funcptr_marshal_hook` (:4847-4933) runs on whatever thread the engine uses to invoke the armed carrier — the game thread for engine-fired BP events. The first accepted invocation publishes the game-thread identity through the initializer bridge `R3MarshalPublishGameThreadIdentity` (`UnrealInitializer.cpp:2508-2549`; first-published ID is immutable), later invocations gate on `IsInGameThreadRaw()` (:4883) and drain `m_game_thread_actions`, `m_delayed_game_thread_actions`, and pending NotifyOnNewObject callbacks (:4903-4930). `R3MarshalQueryHarnessDispatch` (:2551-2562) rides the same invocations.
- **ExecuteInGameThread without any detour: YES.** `LuaMod.cpp:5472-5477`: whenever `R3MarshalPumpPending()` is true, the requested method is overridden to `kFuncPtrMarshalExecutor` and the closure queues onto the marshal queue — even before any carrier arms (lazy semantics; the warning "the closure will await arming" is at :5628; armed-but-dead-pump refusal logic :5709-5718). Queueing needs zero detours; only *delivery* needs a carrier that actually fires.

## 4. Where "[l22]" markers and "Lua mod started" come from

All `[l22]` markers are in `LuaMod.cpp` and print unconditionally via `L22Diagnostics::observe` (which never suppresses — `l22-runtime/diagnostics/include/L22Diagnostics/Logging.hpp:12-15`):

- :2035, :2070, :2126, :2128 — RegisterKeyBind registration/callback entry;
- :2428 — StaticConstructObject post-hook (widget/object constructed);
- :4362, :4367 — ExecuteInGameThread drained (callback entry/return);
- :5617 — "[l22] ExecuteInGameThread queued method=... ";
- **:7557 — "[l22] Lua mod started: {} (main script returned)"** — the mod-start marker, emitted by `LuaMod::start_mod()` only after `main.lua` loads AND executes without error (load errors print "Error loading script"/"Error executing script" :7518-7528; missing main.lua prints "Main script 'main.lua' not found..." :7567).

Follow-on `[l23]` markers (game-thread self-test): queued :141, enqueue-failed :144, ran/refused :114-117, "NOT DRAINED within 30000ms" :154.

There is **no on-screen notification path in these sources** — the PS5-visible signal is the L3+R3 mod menu (PS5Pad rebind, outside these trees); everything else is UE4SS.log lines.

## 5. Single most likely blocker after "Adding shutdown listener"

**`UnrealInitializer.cpp:528` — `UObjectArray::AddUObjectDeleteListener(&FShutdownDeleteListener::ShutdownDeleteListener)` — the init thread dies or deadlocks in the first state-mutating call into live game memory, so `init()` never returns and the event loop / `on_program_start` / mod start never run.**

Evidence, ranked:

1. **Exact log-stop window.** The last line is :527's print; the next expected print is :542 ("Starting callback garbage collector!"). The ONLY statement between them is :528. `Output::send` does one file write per message (`FileDevice.hpp:89` `write_string_to_file` per send), so buffered-tail loss is unlikely — absence of :542 means execution genuinely never reached it.
2. **What :528 does.** `UObjectArray::AddUObjectDeleteListener` (`port/unreal/src/UObjectArray.cpp:379-389`) reads the game's delete-listener TArray (`Contains`) and then `Append`s onto the GAME-OWNED array inside `GUObjectArray` — array growth calls into the engine allocator (`GMalloc->Realloc` virtual call via `IMPLEMENT_UNREAL_VIRTUAL_WRAPPER`, audit `FMemory.cpp:32-35`) and writes new data-pointer/ArrayNum back into `GUObjectArray` — executed on the UE4SS init thread. This is the first *write/virtual-call into live game state* in the whole startup; everything before it was read-only scanning through leased owned images.
3. **Precedent.** The l21-era `runtime-status.txt` (audit `d813b7ab2036-runtime-status.txt`) records a prior terminal failure of exactly this call class: "state 2 failed 1 Virtual FMalloc::QuantizeSize is unavailable" — an FMalloc vtable call killed the runtime before. A fault/deadlock at :528 is the same family (and would present as CE-108255-1 / silent log truncation).
4. **Explains every missing marker at once.** Stall at :528 ⇒ no "Starting callback garbage collector!", no struct-size prints, no "Unreal Engine modules", no event-loop thread, no `[R3] LoadMap detour registrations deferred` warning (`LuaMod.cpp:8064`), no carrier-arm lines, no `[l22]`/`[l23]`, no "[l22] Lua mod started", no mod list. The same call class repeats at `LuaMod.cpp:7995` (`LuaMod::on_program_start` adds the FLuaObjectDeleteListener first) — unreachable today.
5. **Console corroboration to pull:** `runtime-status.txt` under the mod root should show no terminal 'started' ack (the RuntimeAckSink only reports after `init()` progresses past `on_engine_initialized`, `UE4SSProgram.cpp:696-703`), and `UE4SS.log` ends exactly at the :527 line on every arm.

Ruled out or secondary: `allow_mod_callbacks` gate false would require `start_native_mods` to have failed — but then `UE4SSBackend`'s ctor throws BEFORE `init()` (`ue4ss_backend.cpp:34-38`) and none of the scan/init log lines would exist; the observed log rules this out. A crash slightly later with lost tail would still place init() as not-completed — same conclusion, weaker line-precision.

**A/B-test implication:** because the stall precedes every hook-gated block (:2120/:2255 assignments happen BEFORE PostInitialize, but their consumers all run AFTER the event loop starts), all arms — loadmap-only, initgs-only, both-on, both-off — should log identically up to "Adding shutdown listener". If v2 shows "no crash + no mod start", do not attribute it to the hooks (outcome C); the next probe should target :528's GMalloc/listener-array write path (e.g., temporary guard or diagnostic around `AddUObjectDeleteListener`, or confirm via runtime-status ack state).
