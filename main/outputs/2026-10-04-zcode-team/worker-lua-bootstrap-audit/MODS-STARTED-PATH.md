# MODS-STARTED-PATH — composition-5: from `RuntimeOwner::start()` to the `mods-started` ack

worker-lua-bootstrap-audit, 2026-10-04. Static trace of the saved composition-5 sources and
objects. All line numbers verified by reading the files listed below. No execution, no relink.

## Path legend (all paths relative to `/WORKSPACE/.codex/worktrees/translation-core/what`)

| Tag | Path |
|---|---|
| `ENTRY` | `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/staging/lineage-overlay-1/lifecycle/runtime_module_entry.c` (byte-copy of `worker-module-start-wiring/runtime_module_entry_r2.c`, sha256 97283afe…, per `overlay-manifest.json`) |
| `BRIDGE` | `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/staging/lineage-overlay-1/lifecycle/runtime_start_bridge.cpp` (= `worker-module-start-wiring/runtime_start_bridge_r2.cpp`, sha256 a7546bf1…) |
| `ACKSUP` | `outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp` (composed as `module-runtime-ack-supplier`, `build_runtime_composition5.py:162`) |
| `BACKEND` | `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp` — the exact unit composition-5 links (`--replace-bootstrap-unit ue4ss_backend=…`, `build_runtime_composition5.py:167`). Differs from the older `outputs/ps5-mod-bridge` copy only in the 4-arg `PrepareActivatedUE4SSRuntime` signature. |
| `OWNER` | `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp` — byte-identical to `worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp` (the composition-5 `runtime_owner` replacement unit, `build_runtime_composition5.py:166`) |
| `PORT` | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss` |
| `UNREAL` | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal` |
| `ACTSRC` | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/bootstrap/activated_mod_source.cpp` |
| `NMS` | `PORT/src/Platform/NativeModSource.cpp` |
| `PAD` | `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/NativePadInput.hpp` |
| `NPC` | composition-5 overlay `Platform/NativeProgramContext.hpp` (`worker-composition-5-rebuild/staging/lineage-overlay-1/`) |

## 1. Ordered step table: `start()` → `mods-started`

Note on thread identity: steps 1–7 all run on the **loader thread inside `module_start`**.
`RuntimeOwner::start()` (step 8) only *spawns* the init thread and returns; steps 9–10 run on
that **init thread**.

| # | Thread | Step | Cite |
|---|---|---|---|
| 1 | loader (`module_start`) | `module_start` entered; entry_guard serialization; diagnostic channel; probe constructor run | `ENTRY:294-333` (`module_start`), guard `ENTRY:297`, constructors `ENTRY:316-318` |
| 2 | loader | `start_composed_runtime()` once-guard (`runtime_started`), `diagnostic_event("runtime_wiring_begin")` | `ENTRY:230-234` |
| 3 | loader | `mfa_activate(&config, &activation, &error)` — reads/validates the activation from `/data/modbridge/roots/PPSA33286` **once**, while `/data` is reachable; default preload roles `lua_mod, config, script` | `ENTRY:250` (call), `ENTRY:246-249` (root/title/roles), failure path `ENTRY:251-269` (writes diag + `UE4SS.log` line + `failed` status, returns 0 → game runs unmodded) |
| 4 | loader | `modbridge_runtime_start_with_activation(...)` → lmf capture (VMMAP-first) → `TryMakeNativeLoaderFactsFromSupplier` → `MakeNativeProgramContext` (both module + exe paths must exist *now*) | `BRIDGE:124-227` (call `ENTRY:281-283`), capture `BRIDGE:155-158`, conversion `BRIDGE:169-176`, context `BRIDGE:183-184` |
| 5 | loader | `StartActivatedRuntimeWithAck(context, mods, title, mod_root)`: (a) `MakeRuntimeAckConfig` — status_root = exact mfa root, pid = `getpid()`, now_ms = `CLOCK_MONOTONIC` ms; (b) `QueryHarness::Global().InstallDefaultEngineBindings()` (once, before start); (c) `PrepareActivatedUE4SSRuntime(...)`; (d) `owner->start()` | `ACKSUP:68-95`; config `ACKSUP:44-54`; bindings `ACKSUP:82`; start `ACKSUP:93` |
| 6 | loader | `PrepareActivatedUE4SSRuntime`: `MakeActivatedUE4SSModSource(activation, title)` — title match, `unreal` engine + `ue4ss-ps5` framework required, `ue4ss/`-prefixed files with roles `lua_mod/config/script` only → builds `NativeModSource` snapshot. **NativeModSource ctor enumerates Lua mods and computes the startup list** (see §2). Then `RuntimeAckSink::Open` opens `<mod_root>/runtime-status.txt` (single sink). | `BACKEND:84-104` (source bind `BACKEND:90`); `ACTSRC:16-46`; sink open `OWNER:130-153` |
| 7 | loader | `UE4SSBackend` ctor: `ConfigureRetainedPS5PadInput` (opens the genuine pad provider, assigns only after it reports available) → force `mod_startup_policy = Deferred` → install the `on_engine_initialized` ack chain → construct `RC::UE4SSProgram`: `ValidateNativeProgramContext` → `setup_paths` (root = module parent `/data/plugins`; native branch log dir = working dir → `UE4SS.log` at `/data/plugins/UE4SS.log`; `NewFileDevice` bound) → settings → `validate_native_services` (**input handler initialized with the retained provider here, before any mod starts**) → publish `s_program` → `setup_mod_directory_path` (logical `ue4ss/Mods`) → **Deferred ⇒ constructor does NOT autostart mods**. Back in backend ctor: `program_->start_native_mods()` (one-shot gate) → `setup_mods()` + `install_cpp_mods()` + `m_native_mods_started=true` + `start_cpp_mods(Yes)`. Native start branch starts **Lua** mods from `source->startup_mod_names()`. Each `LuaMod::start_mod()`: `prepare_mod` (libs/require-paths/globals/classes/ModRef/input globals/property types) → `make_main_state` → async state+thread → **`m_is_started = true`** → custom module loader → **`load_and_execute_script(scripts/main.lua)`** (native buffer read → `lua` chunk load → `lua_pcall`). Then `native_startup_error_message()` checked; `mods_startup_verified_ = true`; `ack->activated()` written. | `BACKEND:20-58` (pad `:26`, Deferred `:27`, chain `:33-46`, program `:47`, `start_native_mods` `:53`, verified `:56`), `PORT/src/UE4SSProgram.cpp:205-222` (ctor), `:218` setup_paths, `:250` validate, `:256-257` log device, `PORT:749-771` (`setup_paths` native log dir), `PORT:718-744` (`validate_native_services`, input init `:733`), `PORT:397` publish, `PORT:399` + `PORT:832-837` mods dir, `PORT:402-403` no autostart, `PORT:549-592` (`start_native_mods`, gate `:552-577`), `PORT:1735-1751` (`setup_mods` native branch, "Setting up mods…" `:1739`), `PORT/src/Mod/LuaMod.cpp:897-916` (LuaMod ctor; **Lua VM created** `LuaMadeSimple::new_state()` `:900-902`; scripts_path from snapshot `:906-916`), `PORT LuaMod.cpp:6440-6480` (`start_mod`; `m_is_started` `:6452`; main.lua exec `:6459-6463`), `PORT LuaMod.cpp:6371-6438` (`load_and_execute_script`), `PORT:1941-1956` (`start_mods<LuaMod>` native branch), `BACKEND:103` (`ack->activated()`), `OWNER:171` (emit `"activated"`) |
| 8 | loader → returns | `owner->start()`: state → `Starting`, `std::thread(&RuntimeOwner::initialize)` spawned, **returns immediately** (module_start returns 0, game continues) | `OWNER:23-42` (spawn `:31`), `ENTRY:284-287` |
| 9 | **init thread** | `RuntimeOwner::initialize` → `UE4SSBackend::initialize` → `program_->init()`: one-shot guard; `setup_unreal()` — offsets, `UnrealInitializer` with `ModuleSource` = retained loader snapshot + `NativeScanSource`, `StartupWait` with cooperative-cancel check and deadline `now + DefaultStartupWaitTimeout` (**5 minutes**); module log; `fire_unreal_init_for_cpp_mods()`; `setup_unreal_properties()`; `share_lua_functions()`; `m_is_program_started = true` | `OWNER:84-128`, `BACKEND:59-71` (init `:65`), `PORT:600-618` (init guards), `PORT:619` + `PORT:930-957` (`setup_unreal`, ModuleSource `:946`, StartupWait `:948-956`), `UNREAL/include/Unreal/StartupWaitControl.hpp:102` (5-min default), `PORT:648-655` (cpp-mod init / properties / lua share / started flag) |
| 10 | **init thread** | **`mods-started` write site**: still inside `init()`, under `m_engine_ready_callback_mutex` and a not-shutting-down check, `m_native_context.on_engine_initialized()` fires the composition chain: previous callback → `signal->ready` (RuntimeOwner lambda flips status to `EngineInitialized`) → `ack_->engine_ready()` (writes `"engine-ready"`) → **if `mods_startup_verified_`: `ack_->mods_started()` writes `"mods-started"`** (via `mfa_status_write` into `runtime-status.txt`; one mutex-protected emit; fd<0 ⇒ recorded no-op). Only after the callback returns does `init()` start the event loop (`std::jthread{update}`) and block in `join()` until shutdown. | `PORT:654-661` (call site), `BACKEND:33-46` (chain, `engine_ready` `:43`, `mods_started` `:44`), `OWNER:92-100` (ready lambda), `OWNER:154-177` (`emit`/`engine_ready`/`mods_started`), `PORT:673-678` (event loop starts **after** the ack) |

Status-line order on `runtime-status.txt` for a fully successful session:
`activated` (step 7, loader thread) → `engine-ready` then `mods-started` back-to-back (step 10,
init thread). `failed` replaces the tail on any failure (prepare `BACKEND:110-121`, init
`OWNER:111-124`, wiring `ENTRY:221-226`); `stopped` is written once by `owner->stop()` after the
init thread joins (`OWNER:43-78`, sink close `OWNER:193-203`).

## 2. Gating answer: is the Lua mod load before any game-thread tick, or gated?

**The Lua mod load is not on the init thread at all and is gated on nothing.** With
`mod_startup_policy = Deferred` (`BACKEND:27`, `NPC:70-73`) the constructor skips autostart
(`PORT:402-403`), and the backend constructor explicitly calls `start_native_mods()` while still
inside `PrepareActivatedUE4SSRuntime` — i.e. on the **module_start (loader) thread**, before
`RuntimeOwner::start()` has even created the init thread (`ACKSUP:87` runs before `ACKSUP:93`).
The Lua VM per mod is created in the `LuaMod` constructor during `setup_mods`
(`LuaMod.cpp:900-902`) and `main.lua` is loaded and executed inside `start_mod()` in the same
call chain (`LuaMod.cpp:6440-6480`). No tick, no engine readiness, no hook is involved; at that
point `GObjects`/name-pool/engine bindings are **not** established yet (they only come from
`setup_unreal()` in step 9).

**The `mods-started` status line, however, is gated — on engine-ready, not on a tick.** It is
emitted only from the `on_engine_initialized` chain (`BACKEND:41-45`), which the program invokes
exactly once from `init()` (`PORT:654-661`) after `setup_unreal()` + `setup_unreal_properties()`
+ `share_lua_functions()` complete on the init thread — *before* the event loop thread is
launched (`PORT:673`), so it never waits for a game-thread tick of the loop. Two more
conditions: `mods_startup_verified_` must be true (the module_start-time `start_native_mods()`
succeeded — otherwise the session already failed with a `failed` line, `BACKEND:51-57`,
`BACKEND:110-121`) and no shutdown may have been requested (`PORT:658`).

**Expected latency between `start()` and `mods-started`:** init-thread scheduling plus
`UnrealInitializer` work over the retained module snapshot (module setup, signature scans,
property layout, Lua sharing). There is no wait for a game frame; the only bound is the
cooperative `StartupWait` deadline of **5 minutes** (`UNREAL/include/Unreal/StartupWaitControl.hpp:102`,
installed at `PORT:954-956`, cancel check wired to the owner's shutdown flag `PORT:950-953`),
which on expiry throws `StartupCancelled` → `shutdown_native_program()` → `failed` ack, not a
hang. If `init()` completes without the callback ever firing, `RuntimeOwner::initialize`
records `failed: "Native runtime init returned without a stop request"` (`OWNER:101-109`).

## 3. Lua implementation linkage (LuaRaw question)

**One single Lua implementation is linked: LuaRaw = vendored PUC-Rio Lua 5.4, statically linked.**

- Dependency tree: `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/` carries the full C sources (`lapi.c … lparser.c … lua.c`) plus `include/lua.h` with `LUA_VERSION_MAJOR "5"`, `LUA_VERSION_MINOR "4"`, `LUA_RELEASE "Lua 5.4.x  Copyright (C) 1994-2024 Lua.org, PUC-Rio"` (`lua.h:19-28`). `LuaMadeSimple` sits beside it under `deps/first/`.
- The composition's compile commands include `-I …/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include` (`worker-composition-5-rebuild/staging/lineage-overlay-1/build-1/compile-commands.json`).
- The packaged object `build-1/combined-full.o` **defines** the Lua core, not just headers: `llvm-nm --defined-only` shows `lua_newstate`, `luaL_newstate`, `lua_close`, `lua_newthread`, `lua_pcallk`, `luaD_call`, `luaV_execute` (+`luaV_execute.disptab`), `luaS_new/luaS_newlstr/luaS_newudata`, `lua_tolstring` — 145 defined `lua_*`/`luaL_*` text symbols total. So the aligned-entry probe's own passing test (`lua_call_result value=0x00000000` etc.) ran against a real, statically linked Lua core.
- The probe is `outputs/ps5-mod-bridge/native-runtime-startup/probe/memory_lua.c`: it includes `<lua.h>/<lauxlib.h>`, static-asserts `sizeof(lua_Integer) == sizeof(long long)` (line 6), then `luaL_newstate` → `luaL_loadbufferx` → `lua_pcall` → `lua_close` (lines 24-53), emitting `lua_state_begin`, `lua_load_result`, `lua_call_result`, `lua_result_match`, `lua_memory_check_passed`.
- **The UE4SS port uses the same implementation:** the composed runtime object itself (composition-4 run-3 `coherent-tick-file-witness-runtime.o`) already defines `lua_newstate`, `luaL_newstate`, `lua_pcallk`, `luaV_execute` (llvm-nm), i.e. the port's `LuaMadeSimple::new_state()` resolves against the same statically linked LuaRaw core in the same single object — there is no second Lua, no system lua, and no LuaInLua. `PORT LuaMod.cpp:900-902` (`m_lua(LuaMadeSimple::new_state())`) is the port-side entry into it.

## 4. `main.lua` load-time failure modes

Default-verbosity log lines go to `/data/plugins/UE4SS.log` (log device bound at
`PORT:256-257`; native log dir = module parent, `PORT:768-771`).

| Failure at load time | Loud or silent | Exact observable |
|---|---|---|
| Script **syntax error** | **Loud** (Error level), non-fatal | `Error loading script: {lua error}` (`LuaMod.cpp:6413`) then `Failed to execute main script: {path}` (`LuaMod.cpp:6465`); `start_mod` returns normally, `m_is_started` stays true (`:6452`) → `mods-started` is still written later |
| Script **runtime error** (Lua exception in main.lua) | **Loud**, non-fatal | `Error executing script: {lua error}` (`LuaMod.cpp:6424`, error handler pushed `:6406`) + `Failed to execute main script: {path}` (`:6465`); same non-fatal behavior |
| **Missing UEHelpers** (`require("UEHelpers")` fails) | **Loud**, non-fatal | Native module searcher pushes `"module 'UEHelpers' not found:…"` (searcher at `LuaMod.cpp:~1160-1180`, not-found string at `:1345-1348`); `require` raises → pcall catches → `Error executing script: … module 'UEHelpers' not found:…` (`:6424`) + `Failed to execute main script` (`:6465`) |
| `RegisterKeyBind` **before pad provider** | **Cannot happen at load time** — provider is activated during `validate_native_services` in the program constructor (`PORT:731-741`, init `:733`) *before* `start_native_mods`. If the provider fails, startup fails earlier and loudly: constructor throw `Native supplied input source is unavailable or failed activation` (`:734`) → `ack->failed` (`BACKEND:110-114`) → `StartActivatedRuntimeWithAck` → `MRS_ERR_START` → `modbridge module_start failed: runtime_start_failed code=…` to `UE4SS.log` + `failed` status (`ENTRY:289-290`, `:166-192`). `RegisterKeyBind` itself (`LuaMod.cpp:1915-2030`) only validates args and enqueues into `Input::Handler` key data (`Handler.hpp:68-71`); no provider check, no log. Lua errors inside the keybind callback later log `Error: {msg}` (`:1944-1947`). | Loud-at-construction / silent-at-registration |
| `NotifyOnNewObject` at load time (engine bindings not yet installed) | Quiet (Verbose-level), by design deferred | Registration only queues a `PendingNotifyOnNewObjectCallback` into `m_static_construct_object_lua_callbacks` (`LuaMod.cpp:2639-2646`) and logs `[NotifyOnNewObject] Registered notification for {class}` at Verbose (`:2648`). No engine bindings are touched at registration; delivery happens later via the engine hooks, gated by `ShouldRunNativeModCallbacks` (`NPC:38-42`). Caveat (unverified): `FName(…, FNAME_Find/FNAME_Add)` at `:2627-2637` runs before `setup_unreal` establishes the name pool; no guard visible in `LuaMod.cpp` |
| `FindFirstOf` at load time (GObjects not yet set) | **Silent** — returns `nil` | `LuaMod.cpp:1632-1651` calls `UObjectGlobals::FindFirstOf`; pre-init, `ShouldUseHashTables()` is false (`FUObjectHashTables::IsAvailable()` false, `UNREAL/src/UnrealInitializer.cpp:108-110`) and `ForEachUObject*` early-returns on null `GUObjectArray` (`UNREAL/src/UObjectGlobals.cpp:752-754`, `:780-784`, `:805-808`) → nullptr → Lua `nil`. No log line |
| `main.lua` missing from the mod | **Loud**, non-fatal | Native path: `read()` returns nullopt → `load_and_execute_script` false → `Failed to execute main script: {path}` (`LuaMod.cpp:6381`, `:6465`). Disk path (not native): `Main script 'main.lua' not found in scripts directory: {path} -- Ensure your script file uses the correct casing.` (`:6471-6473`) |
| `prepare_mod` exception (bindings/classes setup) | **Loud**, non-fatal to startup | `Exception during mod preparation: {what}` (`LuaMod.cpp:6321`/`:6326`) rethrown → caught by `start_mod` → `Exception during mod startup: {what}` (`:6478`) |
| Whole-inventory failure (`setup_mods`/`start_cpp_mods` sets program error) | **Loud**, fatal to session | `start_native_mods` throws `Native C++ mod startup reported a program error` (`PORT:584-585`) → backend ctor throw → `ack->failed(1, "Native UE4SS inventory rejected: …")` path (`BACKEND:54-55`, `:110-121`) → `runtime_start_failed` lines (`ENTRY:289-290`) |
| **Successful mod load** | **Quiet** — no per-mod success line on the native path | Only `Setting up mods...` (`PORT:1739`) and, for the first started mod only, `[Lua] Exposing size of '{property}' as 0x{size}` (`LuaMod.cpp:956-958`; flag cleared `:1120`). The disk-branch `Starting Lua mod '{name}'` (`PORT:2054`) is **not** reached on the native snapshot path. Success is observable as: no `Error` lines for the mod + `runtime-status.txt` progressing `activated` → `engine-ready` → `mods-started` |

## 5. One-sentence summary

Item-Spawner's `main.lua` (and its 6 `RegisterKeyBind`s) execute on the loader thread during
`module_start`, long before the init thread or any tick; the `mods-started` line they make
observable is written later on the init thread, immediately after `setup_unreal`/properties/
Lua-sharing complete and before the event loop starts, with a 5-minute cooperative deadline and
no game-frame dependency.
