# ENGINE-READY-PATH — composition-5 (r9): from "context created successfully" to the `engine-ready` ack

worker-engine-ready-walkthrough · 2026-10-04 · static trace of saved sources; no execution, no relink, no console contact.
All paths relative to `/WORKSPACE/.codex/worktrees/translation-core/what`. Every line number below was read back from the named file in this walkthrough.

## 0. Source legend and linkage proof

| Tag | Path | Role |
|---|---|---|
| `OWNER` | `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp` | composition-5 `runtime_owner` replacement unit (204 lines). `:172 engine_ready()`, `:173 mods_started()`, `:154-170 emit()` via `mfa_status_write`. |
| `BACKEND` | `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp` | the `ue4ss_backend` unit composition-5 links (`build_runtime_composition5.py:163`). Ack chain `:33-46`; `:41-45` = `if (ack_) { ack_->engine_ready(); if (mods_startup_verified_) ack_->mods_started(); }`. |
| `PROG` | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp` | UE4SS program port. sha256 `dfdd5014a0bf74663152fa74a7f5d2b245517252788e6bc30225cdc71e3ee907` — verified byte-identical to `native-coherent-runtime-1/frozen-source-hashes.json` AND to `program.source_sha256` in `ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/receipt.json` (the object composition-5 reuses). |
| `UI` | `outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp` | the `--replace-runtime-unit unreal-unrealinitializer` unit composition-5 links (1627 lines). |
| `UI0` | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UnrealInitializer.cpp` | port base of `UI` (1543 lines). `UI` = `UI0` + NativeQueryHarness tick dispatch + layout-gate arming (diff reviewed; all shared hops cited in both). |
| `SWC` | `.superpowers/.../port/unreal/include/Unreal/StartupWaitControl.hpp` | deadline/cancel control (unchanged by overlay). |
| `NPC` | `.superpowers/.../port/ue4ss/include/Platform/NativeProgramContext.hpp` | context fields + `ValidateNativeProgramContext`. |
| `SET` / `SIG` / `NLB` / `LM` | `port/ue4ss/src/SettingsManager.cpp`, `Signatures.cpp`, `NativeLayoutBinding.cpp`, `src/Mod/LuaMod.cpp` | settings parse, Lua scan overrides, layout publication, mod start. |
| `ACKSUP` | `outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp` | `StartActivatedRuntimeWithAck` (loader-thread tail before `start()`). |

Thread identity: steps P1–P7 run on the **loader thread inside `module_start`** (precondition phase, summarized). Steps 1–9 run on the **init thread** spawned by `RuntimeOwner::start`. Step 10 runs on the **event-loop thread** (post-ack; included because task 3 asks for the ordering reconciliation).

## 1. Precondition phase (loader thread — "context created successfully" and its gate)

| # | Step | Cite | Inputs consumed | FS / network | Failure → exact string |
|---|---|---|---|---|---|
| P1 | `ValidateNativeProgramContext(context)` | `NPC:83-101`; called `BACKEND:24` (ctor) and `:93` (prepare) | `context.module_file_path`, `context.game_executable_path`, `context.module_source` | `is_regular_file` on BOTH paths; `file_size(game exe)` (`NPC:97-100`) | `std::invalid_argument`: "Native program paths must be absolute normalized file paths without NUL" / "Native program requires an owning loader module snapshot" / "Native module and executable paths must name existing regular files" |
| P2 | `StartActivatedRuntimeWithAck` — ack config birth, engine-seam bindings | `ACKSUP:44-58` (`MakeRuntimeAckConfig`), `:82` (`QueryHarness::Global().InstallDefaultEngineBindings()`), `:87-93` | mfa `root` string, `getpid()`, `CLOCK_MONOTONIC` | none | — |
| P3 | `PrepareActivatedUE4SSRuntime(context, activation, title, ack)` | `BACKEND:84-123` | `ActivatedMods`, expected title; `context.mod_source` must be empty (`:89-90`) | — | `std::invalid_argument`: "Native UE4SS context already has a mod source" |
| P4 | `UE4SSBackend` ctor: pad provider, Deferred policy, ack chain, program construction | `BACKEND:20-58`: pad `:25-26`, Deferred `:27`, chain `:33-46`, program `:47-48`, checks `:49-50`, `start_native_mods()` `:53`, checks `:54-56`, verified `:56`, `ack->activated()` `:107` | full `NativeProgramContext` (`NPC:59-81`): `module_file_path`, `game_executable_path`, `module_source`, `scanner_source`, `mod_source`, `mod_startup_policy=Deferred`, `on_engine_initialized`, `input_source` | `ConfigureRetainedPS5PadInput` (provider activation); UE4SS ctor writes `UE4SS.log` (`PROG:256-257`); diagnostic `file_size` `PROG:427` | "Native UE4SS construction rejected: <msg>" (`BACKEND:50`); "Native UE4SS inventory rejected: <msg>" (`:55`); preparation failures acked `failed(1,<what>)` then rethrown (`BACKEND:110-121`) |
| P5 | UE4SS ctor (loader thread): `setup_paths` + settings + services + mods-dir | `PROG:205-222` (validate `:215`, publish gate `:216-217`, `setup_paths` `:218`), `setup_paths` `:749-818` (mod_source early return `:777-784`), settings `:227` → `SET:53-73` (logical `ue4ss/UE4SS-settings.ini`), `validate_native_services` `:250` → `:719-746`, `UE4SS_MODS_PATHS` guard `:259-263`, `setup_mod_directory_path` `:399` → `:832-839`, Deferred ⇒ **no autostart** `:401-403` | settings bytes from `mod_source` logical keys; `input_source`; env `UE4SS_MODS_PATHS` | `UE4SS.log` NewFileDevice `:256-257`; diagnostic `file_size(game exe)` `:427` | "Native activation is missing ue4ss/UE4SS-settings.ini" (`SET:60`); "Native UE4SS settings contain NUL" (`SET:62`); "Native activation does not support UE4SS_MODS_PATHS" (`PROG:263`); "Native activation does not support filesystem mod directory overrides" (`:2722`); service strings in §4; input strings `:734/:737/:741` |
| P6 | `start_native_mods()` (one-shot, loader thread) | `PROG:549-592`: gate `:551-577`, `setup_mods` `:580` (native branch `:1741-1751` creates LuaMod objects + Lua VMs, `LM:897-916`), `install_cpp_mods` `:581` (CppMod-only `:1836-1839`), flag `:582`, `start_cpp_mods` `:583` → `start_mods<CppMod>` native branch `:1946-1956` returns `{}` **without starting Lua mods** | `source->startup_mod_names()` (read but NOT consumed for Lua here), `m_error_object`, shutdown flag | none (logical keys only) | "Native mods require a published UE4SS program" / "Native mods cannot start after a UE4SS startup error" / "Native mods cannot start during shutdown" / "Native mods must be enabled before UE4SS initialization" / "Native mod startup is one-shot" (`PROG:563-571`); "Native C++ mod startup reported a program error" (`:585`) |

`context created successfully` = P1 passes and P3 binds `mod_source`. Everything below is the init-thread sequence my scope owns.

## 2. Ordered step table: `RuntimeOwner::start()` → `engine-ready` (+`mods-started`)

| # | Thread | Step | Cite | Inputs consumed | FS / deadline | Failure → exact string → ack effect |
|---|---|---|---|---|---|---|
| 1 | loader→returns | `start()`: state `Prepared`→`Starting`, spawn init thread, return | `OWNER:23-42` (state `:28-29`, spawn `:31`) | `signals_` state | thread spawn | spawn throw → `Failed`, `ack_->failed(1, e.what())` (`OWNER:32-39`); non-`Prepared` state → `false`, no ack |
| 2 | init | `RuntimeOwner::initialize`: record tid, call `backend_->initialize(readyLambda)` | `OWNER:84-100` (readyLambda `:93-99` flips `Starting`→`EngineInitialized`) | `signals_` | — | all exceptions → `Failed` + `ack_->failed(1, …)` (`OWNER:111-124`) |
| 3 | init | `UE4SSBackend::initialize`: store ready, `program_->init()`, clear ready, rethrow stored error | `BACKEND:59-71` | `signal_->ready`, program | whole init | `throw std::runtime_error(<stored msg>)` (`:70`) → OWNER `:111-118` |
| 4 | init | `UE4SSProgram::init` one-shot + shutdown guards | `PROG:600-618` (mutex `:609`, `m_native_init_started` `:610-612`, published-check `:614`, cancel `:616-617`) | `s_program`, `m_error_object`, `m_shutdown_requested` | — | "Native UE4SS initialization is one-shot" (`:611`); "Native UE4SS program cannot initialize after rejected startup or shutdown" (`:615`); `StartupCancelled` → clean shutdown, no error (`:686-690`) → then OWNER `:102-109` "Native runtime init returned without a stop request" |
| 5 | init | `setup_unreal()` — offsets staging, Config assembly, overrides | `PROG:619` → `:930-1321`: layout staging `:936` (`:896-908`), Config `:944-956` (`ModuleSource` `:946`, `NativeScanSource` `:947`, `StartupWait` `:948-956`), `CachePath` `:958`, scan threads `:966-989`, version override `:991-1014`, Lua scan overrides `:1017` → `SIG:171-176` + `SIG:40-47`, hooks `:1222-1240`, early scan `:1241-1261`, `Initialize` `:1265` | `mod_source` logical `ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini`; settings fields; `context.module_source/scanner_source`; `m_shutdown_requested` | layout staging is in-memory (no disk under mod_source); **5-min deadline set `:954-955`** | "Native activation requires both member and vtable layout files" (`:904`); "Was unable to override engine version from ini file; The number in the ini file must be in range of a uint32" (`:1004`); early-scan rethrows `StartupCancelled`/`StartupDeadlineExceeded` `:1249-1256`, swallows other runtime_errors `:1257-1260` |
| 6 | init | `UnrealInitializer::Initialize(config)` → `PreInitialize` → `ScanGame` → `VerifyFNameConstructor` → readiness waits | `UI:1020-1111` (`PreInitialize` `:1023` / `UI0:969-974`; gates `UI:993-998` / `UI0:943-948`; `SetupUnrealModules` `UI0:282-291` uses `source->Views()`; `ScanGame` `UI:1032`/`UI0:981`; `VerifyFNameConstructor` `UI:1046`; ≥10000 objects `UI:1060-1072`; Kismet `UI:1075-1111`) | `StaticStorage::GlobalConfig` (ModuleSource, NativeScanSource, StartupWait, scan threads, overrides), ps_scan results | every retry/loop iteration calls `CheckStartupWait` (`UI:67-75`) / `WaitForRetry` (`UI:77-89`) against the 5-min deadline | "Native module source is not configured"; "Native mapped scan source must retain the configured module owner" (`UI0:944/947`); "FName constructor verification callback registration failed" (`UI0:907`); "Unreal initialization cancelled" (`SWC:20`); "Unreal initialization startup deadline exceeded" (`SWC:26`) |
| 7 | init | `ScanGame` internals: ps_scan loop, AOB output, layout axes, GUObjectArray bind, versioned container | `UI:576-940`: ps_scan loop `:607-632` (`throw "PS scan timed out"` `:630`), AOB aggregate throw `:635-658`, axes `UI0:809-833`, `SetupGUObjectArrayAddress(results.g_uobject_array)` `:895`/`UI0:852`, versioned container + layout gate `:861-880`, hash tables bind `UI0:858-861` | `NativeScanSource` readable ranges (mapped snapshot), `ScanOverrides`, `SecondsToScanBeforeGivingUp` (default coerced 30→120, `SET:136-144`), settings DebugBuild/Stats | scan retries bounded by both the settings give-up and the 5-min deadline | "PS scan timed out" (`UI:630`); "AOB scans could not be completed because of the following reasons:\n<errors>" (`UI:638-658`); layout gate: "Native layout files have not been staged" / "Native layout build axes require resolved or explicitly supplied facts" / "Native layout declarations disagree with resolved engine facts" (`NLB:177-183`) |
| 8 | init | `PostInitialize`: pools, listeners, `IsInitialized()=true`, hooks | `UI:385-490` (`GMalloc` `:388-390`, pools `:405-449`, `IsInitialized()=true` `:454`, GC `:467`, StaticConstructObject hook `:471ff`) | `GMalloc`, `bUseUObjectArrayCache` | — | "UnrealInitializer::PostInitialize: GMalloc is uninitialized." (`UI:389`) |
| 9 | init | ProcessInternal/TypeChecker/required objects (still inside `Initialize`) | `UI:1476-1512` (`never constructed` log `:1482` + empty-message throw `:1485`, TypeChecker `:1494`, ExecuteUbergraph `:1503`) | GUObjectArray contents, hook settings (`bHookProcessInternal`, `bHookProcessLocalScriptFunction`) | deadline-checked | EMPTY `std::runtime_error{""}` after "Fatal error! The following objects were never constructed:" (`UI:1482-1485`); "TypeChecker: Was unable to find some or all of the required core objects" (`:1494`); "Was unable to find locate ProcessInternal because '/Script/CoreUObject.Object:ExecuteUbergraph' wasn't found in GUObjectArray" (`:1503`) |
| 10 | init | back in `init()`: module log, cpp-mod init event, `setup_unreal_properties`, asset registry, `share_lua_functions` | `PROG:621-652` (`fire_unreal_init_for_cpp_mods` `:648`, properties `:649` → `:1702-1733`, `share_lua_functions` `:652` → `:1323-1330`) | settings (`Memory.MaxMemoryUsageDuringAssetLoading` `:650`) | none; both setup fns are pure in-memory maps/pointers | — |
| 11 | init | **ack block**: `m_is_program_started=true`; under `m_engine_ready_callback_mutex`, if not shutting down → `m_native_context.on_engine_initialized()` | `PROG:654-666` (flag `:655`, mutex `:657`, shutdown check `:658`, call `:660`, recheck `:662-666`) | `m_native_context.on_engine_initialized`, shutdown flag | — | shutdown race → `shutdown_native_program()` + return (`:662-666`) → OWNER `:102-109` failed-as-above |
| 12 | init | BACKEND chain: `previous()` → `signal->ready()` (OWNER flips to `EngineInitialized`) → `ack_->engine_ready()` → if `mods_startup_verified_` `ack_->mods_started()` | `BACKEND:33-46` (`:34`, `:35`, `:41-45`) → `OWNER:171-173` → `emit` `:154-170` (`mfa_status_write`) | `mods_startup_verified_` (P4), ack sink fd | status file write only | fd<0 ⇒ recorded no-op (`OWNER:159-163`); emit never throws (`:167-169`) |
| 13 | init | event loop starts AFTER the acks; init thread parks in `join()` | `PROG:670-683` (jthread `:673`, join `:678`) | — | loop body deadline-free | loop exception → `copy_error_into_message` + `request_shutdown` (`PROG:1691-1699`) |

**Status-line order for a successful session:** `activated` (P4) → `engine-ready` then `mods-started` back-to-back (step 12, same init-thread callback) → `stopped` (owner `stop()`, `OWNER:43-78`).

## 3. Inputs each step consumes (consolidated view)

| Context field / fact | Producer | Init-thread consumers |
|---|---|---|
| `module_file_path` | loader facts capture (`BRIDGE`, audit legend) | `ValidateNativeProgramContext` (`NPC:85-92`); `m_root_directory`/`m_module_file_path` (`PROG:752-753`); log root (`:781`, `:256-257`); `CachePath` string (`:958`, no consumer in linked `UI`) |
| `game_executable_path` | loader facts capture | `ValidateNativeProgramContext` (`NPC:85-100`); `m_legacy_root_directory`/`m_game_executable_directory`/`m_game_path_and_exe_name` (`PROG:761-773`); ctor diagnostic `file_size` (`:427`); getters `:2573/:2579` (Lua/SDK only) |
| `module_source` (retained snapshot) | lmf capture → context | `Config.ModuleSource` (`PROG:946`); `SetupUnrealModules` views (`UI0:282-291`); all AOB scans (`UI0:81-83`, `:146-148`); `LoadExport` (`UI0:221-230`) |
| `scanner_source` | owned mapped scan source | `Config.NativeScanSource` (`PROG:947`); PreInitialize owner-equality gate (`UI0:946-948`) |
| `mod_source` (activated snapshot) | `MakeActivatedUE4SSModSource` (`BACKEND:94`) | logical keys: settings (`SET:58-66`), layouts (`PROG:899-905`), signature Lua (`SIG:174-176`), mods dir `ue4ss/Mods` (`PROG:835-839`), `setup_mods` inventory (`:1742-1751`), `startup_mod_names` (`:1950`, `:2606`) |
| `input_source` (pad) | `ConfigureRetainedPS5PadInput` (`BACKEND:25-26`) | `validate_native_services` init (`PROG:730-738`); `m_input_handler.process_event()` on update loop (`:1650`) |
| `mod_startup_policy=Deferred` | forced `BACKEND:27` | ctor skips autostart (`PROG:401-403`); `ShouldRunNativeModCallbacks` allows later Lua start (`:1478-1485`) |
| `on_engine_initialized` | BACKEND chain (`:28-46`) | single call site `PROG:660` |
| settings (from logical ini) | staged `ue4ss/UE4SS-settings.ini` | hooks/hotkeys/scan config (`PROG:1222-1240`), services gate (`:719-729`), `SecondsToScanBeforeGivingUp` (`SET:136-144`), `DoEarlyScan` (`:1241`) |
| `m_shutdown_requested` | program member (`PROG` hpp `:130`) | cancel lambda (`:949-953`), `:614-617`, `:658`, `:662` |

## 4. Failure-mode rows (exact strings, thread, effect on ack)

| Site (thread) | Trigger | Exact message / effect | Ack line written |
|---|---|---|---|
| `OWNER:28` (loader) | `start()` when state ≠ `Prepared` | returns `false`, no status change | none |
| `OWNER:31-39` (loader) | `std::thread` construction throws | state `Failed`, message = `e.what()` | `failed(1,<system msg>)` |
| `PROG:611` (init) | second `init()` | "Native UE4SS initialization is one-shot" | `failed(1,<msg>)` via `BACKEND:67-70` → `OWNER:111-118` |
| `PROG:615` (init) | program rejected/shutdown before init | "Native UE4SS program cannot initialize after rejected startup or shutdown" | `failed(1,<msg>)` |
| `PROG:616-617` + `SWC:20` (init) | stop requested during waits | `StartupCancelled` ("Unreal initialization cancelled") → clean `shutdown_native_program()`, no stored error (`PROG:686-690`) | if a real stop was requested: `stopped`; if not: `failed(1,"Native runtime init returned without a stop request")` (`OWNER:106-108`) |
| `SWC:26` (init) | 5-minute `StartupWait` deadline exceeded anywhere in waits/scans | "Unreal initialization startup deadline exceeded" | `failed(1,<msg>)` |
| `UI:630` (init) | ps_scan unsuccessful longer than `SecondsToScanBeforeGivingUp` (120 s default) | "PS scan timed out" | `failed(1,<msg>)` |
| `UI:638-658` (init) | fatal AOB scan errors | "AOB scans could not be completed because of the following reasons:\n<joined errors>" | `failed(1,<msg>)` |
| `UI0:944/947` (init) | PreInitialize source gates | "Native module source is not configured" / "Native mapped scan source must retain the configured module owner" (`std::logic_error`) | `failed(1,<msg>)` |
| `NLB:177-183` (init) | layout gate at versioned-container publication | "Native layout files have not been staged" / "Native layout build axes require resolved or explicitly supplied facts" / "Native layout declarations disagree with resolved engine facts" | `failed(1,<msg>)` |
| `UI0:907` (init) | FName verification hook registration failure | "FName constructor verification callback registration failed" | `failed(1,<msg>)` |
| `UI:389` (init) | `GMalloc` null at PostInitialize | "UnrealInitializer::PostInitialize: GMalloc is uninitialized." | `failed(1,<msg>)` |
| `UI:1482-1485` (init) | required hook objects never constructed | logs "Fatal error! The following objects were never constructed:" then `throw std::runtime_error{""}` — **empty message** (status detail will be blank) | `failed(1,"")` |
| `UI:1494` (init) | TypeChecker misses core objects | "TypeChecker: Was unable to find some or all of the required core objects" | `failed(1,<msg>)` |
| `UI:1503` (init) | ExecuteUbergraph not found (when ProcessInternal hooks enabled) | "Was unable to find locate ProcessInternal because '/Script/CoreUObject.Object:ExecuteUbergraph' wasn't found in GUObjectArray" | `failed(1,<msg>)` |
| `PROG:711` (init) | non-std exception in init | "Native UE4SS initialization failed with an unknown exception" | `failed(1,<msg>)` |
| `BACKEND:49-50` (loader) | ctor stored an error | "Native UE4SS construction rejected: <msg>" | prepare-time `failed` (`BACKEND:110-121`) |
| `BACKEND:54-55` (loader) | `start_native_mods` stored an error | "Native UE4SS inventory rejected: <msg>" | prepare-time `failed` |
| `PROG:427` (loader) | game exe vanished between validation and ctor log | `std::filesystem::filesystem_error` message copied into program error | construction-failure path |

## 5. Task 2 verdict — directory derivations under active `mod_source`

Derivations (`PROG:749-818`): `m_root_directory` = module parent (`:752`); `m_working_directory` = root (`:758`, `:770`; log alias `:781`); `m_legacy_root_directory` = **game-exe parent** (`:768`); `m_game_executable_directory` = **game-exe parent** (`:771`); `m_game_path_and_exe_name` = game exe (`:773`).

Used by `setup_unreal`/`ScanGame`/properties on the init thread with mod_source active:

| Derivation | Init-thread consumers | Behavior if `game_executable_path` became a configured root |
|---|---|---|
| `m_legacy_root_directory` | **none**. All consumers are fenced off: game-specific scan `:791-808` and legacy settings `:814-817` are after the mod_source early return (`:783`); legacy Mods fallback `:856-858` is after the `:835-839` early return; getter `:2729-2731` is diagnostics/Lua-only | none on the init path; value changes purely cosmetically |
| `m_game_executable_directory` | **none** on init path. Only getter `:2579-2582` → Lua helpers `IterateGameDirectories` (`LM:2939`), LogicMods reconstruction (`LM:3134`), `CreateLogicModsDirectory` (`LM:3274`); they assume `<exe>/Binaries/Win64`-shaped parents | those Lua helpers silently change meaning (a root that is not a real exe parent breaks their `parent_path().parent_path()/Content` derivation, and they fail closed with a warning/nil/throw) |
| `m_working_directory` | **no functional use** on init path: log alias `:781/:810`; `setup_lua_scan_overrides` argument `:1017` is replaced by logical `"ue4ss"` (`SIG:175`); `VTableLayout.ini` read `:1024` is `#ifndef __PROSPERO__`; UHT/SDK dump roots (`:2748`, `LM:2142/2153`) are GUI/Lua-triggered only | none on the init path |
| `m_game_path_and_exe_name` | `ValidateNativeProgramContext` `is_regular_file` + `file_size` (`NPC:97-100`, **pre-construction**); ctor diagnostic `file_size` (`PROG:427`); getter `:2573` → SDK generator (`UEHeaderGenerator.cpp:4172`) | **the one real constraint**: the configured root must still be an existing regular FILE with a filename (absolute, normalized); a directory-style root is rejected before any init-thread work. ScanGame/properties behavior is otherwise identical because everything reads `module_source` views, not paths |
| `m_root_directory` | `CachePath` (`:958`) — declared but never consumed by the linked `UI` (grep: `CachePath` appears only in `UnrealInitializer.hpp:130`); settings-path string (`:772`, `:782`) — content read from logical key, path unused; log device `:256-257` writes `<module parent>/UE4SS.log` | log placement keyed to the MODULE path, unaffected by exe-root change |

Net: swapping `game_executable_path` to an explicit configured root changes **zero** init-thread (context→engine-ready) behavior, provided the configured value still passes `ValidateNativeProgramContext` (file, absolute, normalized, has filename). The consumers that would change are mod-runtime Lua directory helpers and the SDK generator — none of them gate the acks.

## 6. Task 3 verdict — loader-thread main.lua claim vs engine-ready ordering (CORRECTION)

`worker-lua-bootstrap-audit` C1 claimed main.lua executes on the loader thread inside `start_native_mods()` before the init thread exists. The linked source contradicts the required link in that chain:

- `start_native_mods` (`PROG:580-583`) runs `setup_mods` + `install_cpp_mods` + `start_cpp_mods(Yes)`.
- `setup_mods` native branch only **creates** LuaMod objects (and Lua VMs in the ctor, `LM:897-916`) — it does not start them (`PROG:1741-1751`).
- `install_cpp_mods` → `install_mods<CppMod>` — LuaMods are skipped by the `dynamic_cast` (`PROG:1797-1834`).
- `start_cpp_mods` → `start_mods<CppMod>()`; the native branch starts Lua mods **only** under `if constexpr (std::is_same_v<ModType, LuaMod>)` (`PROG:1946-1956`) and otherwise returns immediately. No Lua mod starts here.
- The only reachable `start_lua_mods()` sites: `on_program_start` (`PROG:1488`) — which runs on the event-loop thread (`update` → `PROG:1507-1514`), and `reinstall_mods` (`PROG:2295`).
- Linkage proof: `ue4ss-native-supplier-composition-4/run-3/receipt.json` records `program.source_sha256 = dfdd5014…`, byte-identical to the file cited above (hash re-verified).

Therefore the true order is:

1. `on_engine_initialized()` fires on the **init thread** (`PROG:660`) → `engine-ready` ack, then `mods-started` ack (`BACKEND:41-45`) — both BEFORE any main.lua runs.
2. The event-loop jthread starts after the acks (`PROG:673`); `update` → `on_program_start` (`PROG:1514`) → `install_lua_mods` + `LuaMod::on_program_start` + `start_lua_mods` (`PROG:1486-1489`) → `LuaMod::start_mod` (`LM:6440`) → `load_and_execute_script(scripts/main.lua)` (`LM:6459-6463`).
3. So "`mods-started`" certifies the mod **inventory** (setup + one-shot + cpp-start attempt), not script execution; and `mods-started` does NOT imply main.lua has run yet. The audit's C2 (write site) and C3 (enablement from snapshot) remain correct; C1's thread placement and C5's "FindFirstOf silently nil at load time" scenario do not apply to this composition.

**What the Item Spawner F7 (OPTIONS) handler needs, and when it gets it:**

| Need | Provided at | Relative to engine-ready |
|---|---|---|
| `GUObjectArray` bound (`FindFirstOf` iterates it, `UObjectGlobals.cpp:434-478`) | `ScanGame` second pass `UObjectArray::SetupGUObjectArrayAddress(results.g_uobject_array)` (`UI:895` / `UI0:852`), plus the ≥10000-element wait (`UI:1060-1072`) | BEFORE engine-ready (same init thread, earlier step) |
| FName pool verified + Kismet helpers | `VerifyFNameConstructor` (`UI:1046`), Kismet location (`UI:1075-1111`) | BEFORE engine-ready |
| `IsInitialized()` true (event loop refuses to run otherwise, `PROG:1577-1583`) | `PostInitialize` (`UI:454`) | BEFORE engine-ready |
| main.lua registered `RegisterKeyBind(Key.F7, …)` (main.lua:694 per CONFIG-VERDICT) | `LuaMod::start_mod` on the event-loop thread (`LM:6440/6459-6463` via `PROG:1488`) | AFTER engine-ready (shortly) |
| Input pump dispatching keydown to Lua binds | `m_input_handler.process_event()` in the loop (`PROG:1650`); handler initialized with the pad provider during construction (`PROG:733`) | initialized before; pumped after |
| Non-nil `FindFirstOf` for the menu target class | engine-ready state + the game having constructed instances | guaranteed bound; content-dependent |

Residual observer gap: a main.lua **load failure** logs `"Failed to execute main script: <path>"` (`LM:6465`) or `"Exception during mod startup: <what>"` (`LM:6478-6480`) and is otherwise non-fatal; `runtime-status.txt` still ends at `mods-started` — there is no negative signal, and F7 simply never registers.

## 7. Checklist — what must be true for OPTIONS to work, from the moment context creation succeeds

1. Context validation held: both paths absolute/normalized/with filename and **existing regular files**; `file_size(game exe)` succeeds (`NPC:85-100`, `PROG:427`).
2. Activation consumed: title matched, `unreal` + `ue4ss-ps5` declared, role-filtered files preloaded (`BACKEND:89-107`); `startup_mod_names()` non-empty — otherwise no main.lua ever runs and OPTIONS is impossible by construction.
3. Activation payloads present: `ue4ss/UE4SS-settings.ini` (no NUL, `SET:58-66`), `ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini` (`PROG:899-905`), `ue4ss/Mods/<mod>/scripts/main.lua` in the snapshot.
4. Settings keep native-rejected services off: CrashDump disabled, Simple/Debug console off (`RenderMode=ExternalThread`), hot-reload and auto-reload off (`PROG:719-729`); `UE4SS_MODS_PATHS` unset (`:259-263`); no `ModsFolderPaths` +/- entries (`:2720-2723`).
5. Pad provider activated and accepted: `ConfigureRetainedPS5PadInput` OK (`BACKEND:25-26`), `m_input_handler.init(input_source)` true, `InputSource` setting matches provider (`PROG:730-738`); PS5Pad.ini maps OPTIONS→F7 (verified GO in worker-config-final-review).
6. `start_native_mods` one-shot passed on the loader thread (preconditions `PROG:552-577`), `mods_startup_verified_=true` (`BACKEND:56`), `activated` written (`:107`).
7. Owner `start()` from `Prepared`; init thread spawns; no stop request during init.
8. ps_scan succeeds over the retained snapshot within `SecondsToScanBeforeGivingUp` (120 s) and the shared 5-minute StartupWait deadline.
9. No fatal AOB errors; layout gate satisfied (staged layout identity == resolved engine axes — Stats must be explicit or detectable, `UI0:752-763`).
10. Engine constructs ≥10000 UObjects; KismetStringLibrary + `Conv_NameToString` + CDO found; FName verification callback registers and a real FName construction occurs — all before the deadline.
11. `GMalloc` non-null; all required hook objects constructed; TypeChecker complete; ExecuteUbergraph resolvable if ProcessInternal hooks are enabled.
12. Shutdown check at `PROG:658` passes → `engine-ready` + `mods-started` written (ack sink fd valid, else silent no-op).
13. Event loop starts (`PROG:673`), reaches `on_program_start` (`:1514`) → `start_lua_mods` (`:1488`) → main.lua executes and registers F7; loop keeps pumping `m_input_handler.process_event()` (`:1650`) so the pad's OPTIONS chord reaches the Lua callback; the callback's `ExecuteInGameThread` path runs against the already-bound GUObjectArray.
14. Nothing between steps 8–13 requested shutdown or stored a program error (an error would divert to `shutdown_native_program` and end the session `failed`).
