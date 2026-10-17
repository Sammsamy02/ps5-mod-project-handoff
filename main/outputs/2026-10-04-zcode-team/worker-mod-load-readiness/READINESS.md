# READINESS — can launch-10 actually load and run the Townfall Item Spawner?

Worker: worker-mod-load-readiness · 2026-10-04 · source-level trace, local only, no canonical edits.
Mod bytes (unchanged, staged v3 package): `outputs/2026-10-04-zcode-team/worker-settings-v3-package/package-v3/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` (21,415 B) + `.../Mods/shared/UEHelpers/UEHelpers.lua` (9,962 B) + `enabled.txt` (0 B) + `UE4SS-settings.ini` v3 + `PS5Pad.ini` (2,960 B) + `loadlist.txt`.

Composed runtime examined: the 368-unit composition (`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/receipt.json`; l10r3 twins `run-a/run-b` under `outputs/2026-10-04-zcode-team/worker-launch10-r3-armed/`). All units named below are IN the composed unit list (`selected_runtime_units`): `mod`, `luamod`, `cppmod`, `native-mod-source`, `program` (comp6-program UE4SSProgram.cpp), `settings`, `lua-library`, all 40 LuaRaw .c VM units, 24 LuaType units, `output`, `input-handler`, `native-pad-input`, bootstrap `runtime_owner`, `ue4ss_backend`, `module-runtime-ack-supplier`.

## Bottom line

**Nothing in the composed runtime is missing for LOADING this mod.** Every Lua global, every callback registration, the module loader, the settings keys, and the input path that main.lua and UEHelpers.lua need are present in the composed sources, with file:line evidence in CLAIM.json. There are **zero load-time API blockers**.

The only conditional blocker is variant-dependent and sits AFTER load:
- **readpath-guard initializer (the witness-verified launch-10 staged input):** EngineTick pump registered at init (`RegisterEngineTickPreCallback` at `UnrealInitializer.cpp:1844/1855`), so `ExecuteInGameThread` has a functioning game-thread drain path. Menu/popup is then an in-game UNKNOWN, not a structural blocker.
- **l10r3 R3-armed fold-1 candidate (BOTH_FAIL matrix):** the initializer's EngineTick registrations are compiled out (`UnrealInitializer.cpp:1847-1862`), `pump_pending` is set, and the designated drain consumer (LuaMod DIFF 1) was proven not composable — nothing in this candidate drains the pump (owning REPORT §5). Mod still loads and logs; pad presses still fire Lua callbacks; but every `ExecuteInGameThread` body never runs ⇒ the menu can never open.

## 1. Mod load path (post-engine-ready), unit by unit

```
module_start (host bridge)
  └ StartActivatedRuntimeWithAck            runtime_ack_supplier.cpp:68-95
     └ PrepareActivatedUE4SSRuntime         ue4ss_backend.cpp:84-119
        ├ context.mod_source = MakeActivatedUE4SSModSource   activated_mod_source.cpp:19-46
        │   (title match :21-23, framework ue4ss-ps5 :24-26, role filter lua_mod/config/script :34-39)
        │   └ NativeModSource ctor          NativeModSource.cpp:57-136
        │       • lua mod = ue4ss/Mods/<n>/Scripts/main.lua present   :78-105 (read :103)
        │       • "shared" skipped as a mod (but files stay readable) :100
        │       • enabled.txt presence -> startup_mod_names           :134-135
        ├ UE4SSBackend ctor (enable_buffered_mods=true)               ue4ss_backend.cpp:20-58
        │   ├ ConfigureRetainedPS5PadInput(ue4ss/PS5Pad.ini) :25-26   NativePadInput.cpp:10-26
        │   ├ UE4SSProgram ctor
        │   │   ├ settings deserialize from mod source key            SettingsManager.cpp:58-68
        │   │   ├ validate_native_services (input init, v3 gates)     comp6-program:728-757
        │   │   └ start_native_mods -> setup_mods(PROSPERO: mod_source list)
        │   │       + install/start Cpp mods                          :558-601, 1753-1769
        │   └ program_->start_native_mods()  (-- inventory verified, ack armed)
        └ owner->start() -> init()
            ├ setup_unreal (armed seeds; '[Seeds] REFUTED' = early terminal)
            ├ on_engine_initialized (:663-670) -> ack engine_ready + mods_started
            │   (NOTE: mods_started ack = inventory OK, NOT "main.lua ran")
            └ event loop jthread UE4SSProgram::update  :682
               └ update -> on_program_start :1532
                  ├ gate ShouldRunNativeModCallbacks(true, mods_started) :1496-1500
                  ├ install_lua_mods :1503
                  ├ LuaMod::on_program_start :1504 (LuaMod.cpp:6904: LoadMap/InitGameState/
                  │   BeginPlay/ProcessEvent/EngineTick/console callback registrations)
                  └ start_lua_mods :1506 -> start_mods<LuaMod>
                     └ PROSPERO branch: for name in source->startup_mod_names()
                        -> mod->start_mod()                    comp6-program:1959-1974
                        └ LuaMod::start_mod                    LuaMod.cpp:6450-6490
                           ├ Lua VM = LuaMadeSimple::new_state (ctor :902)
                           ├ prepare_mod :6456 -> globals/classes/ModRef/Key/EFindName :6305-6339
                           ├ main-state-only fns (ExecuteInGameThread, FName/FText/NAME_None/
                           │   UnrealVersion globals)          :6458 (:4573 block)
                           ├ async state + thread              :6459-6460
                           ├ native module loader on main_lua  :6466 (:1125-1183)
                           └ load_and_execute_script(".../Scripts/main.lua")
                              native read :6388-6392, chunk :6419-6420, lua_pcall :6430
                              => main.lua executes: require UEHelpers (buffer searcher,
                                 key ue4ss/Mods/shared/UEHelpers/UEHelpers.lua :874),
                                 6x RegisterKeyBind, 1x pcall(RegisterLoadMapPreHook),
                                 final print -> UE4SS.log oracle line (main.lua:746).
```

Interesting negative result: on PROSPERO the classic filesystem paths (`mods.txt` parse, `enabled.txt` directory walk in `start_mods` Part #1/#2, `std::filesystem` mods scan in `setup_mods`) are **dead code** — every one of them has a `mod_source` early return (`setup_mods` :1759-1769, `start_mods` :1964-1974, `setup_mod_directory_path` :852-857, `setup_paths` :794-802, settings :60-68). The loadlist-staged in-memory snapshot is the ONLY mod source.

## 2. Verdict table

| # | Stage | Verdict | Evidence / blocker / what resolves it |
|---|-------|---------|----------------------------------------|
| 1 | Mods scan (discovery) | **READY** | `NativeModSource.cpp:78-105` discovers exactly `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua`; staged keys match loadlist.txt:11-13. Limits (≤4096 files, ≤8 MiB/file, ≤64 MiB total) comfortably clear (NativeModSource.cpp:59-75). |
| 2 | Enabled gate | **READY** (one minor host-side unknown) | `NativeModSource.cpp:134-135` enables on `read(mod.path+"/enabled.txt")` presence; v3 stages it (0 B). Unknown: whether the host mfa preload hands a non-null `data` pointer for a 0-byte file — `activated_mod_source.cpp:40` throws "UE4SS file was not preloaded" on null. Launch-10 evidence: the runtime-status ack `activated` line proves this stage passed. |
| 3 | Lua state creation | **READY** | Full LuaRaw 5.4 VM composed (lapi/ldo/lvm/lstate/... in run-3 receipt); `LuaMod` ctor creates the state (LuaMod.cpp:902); `open_all_libs` incl. pcall/string/table/math used by the mod (prepare_mod :6309). |
| 4 | main.lua executes | **READY** source-side; **UNKNOWN** launch-side | Native chunk read + pcall (LuaMod.cpp:6381-6448). All load-time calls resolve (see §3). Contingent: engine-ready must pass — the armed seed gate can still end startup ("[Seeds] REFUTED" class, prebuild REPORT); launch-10 log evidence resolves. Oracle line: `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` (main.lua:746 → LuaLibrary.cpp:35). Failure shapes: `Error loading script: …` / `Error executing script: …` / `Failed to execute main script: …` (LuaMod.cpp:6423/6434/6475), `Mod is absent from native Lua source: …` (:911). |
| 5 | API calls resolve | **READY at load**; dynamic calls **UNKNOWN in-game**; `ExecuteInGameThread` **BLOCKED on R3 variant only** | §3 inventory: every global registered. Dynamic UFunction/property calls resolve via UClass reflection at runtime (LuaUObject.cpp:151-327) — in-game only. EngineTick drain: readpath-guard variant registers the pump (initializer:1844/1855); R3 fold-1 variant refuses it and nothing drains `pump_pending` (initializer-l10-r3:1847-1862, :2136-2192; REPORT §5). |
| 6 | User-visible popup (OPTIONS menu) | **UNKNOWN** in-game (structurally sound on readpath-guard variant); **BLOCKED by construction on R3 variant** | Needs: live PlayerController (main.lua:21/493), `/Script/UMG.Default__WidgetBlueprintLibrary` (:499), UMG class reflection + FText + struct setters (:505-633), game-thread drain for the queued `toggleMenu`. R3 variant: drain never runs ⇒ menu impossible. Launch-10 evidence that resolves: UE4SS.log `Menu created.` + captured frame showing the panel. |
| 7 | Spawn (CROSS on a row) | **UNKNOWN** in-game | Needs ItemDatabaseSubsystem (:64-69), `LineTraceSingle` out-param marshalling (:245-269), struct construction (:273-277), deferred spawn (:295-303). Source provides the routes (LuaUObject ProcessEvent path); no launch evidence exists. |

## 3. API inventory (everything main.lua + UEHelpers.lua call)

Registrations cited in the COMPOSED luamod unit `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/src/LuaMod.cpp` (= `LuaMod.cpp` below). Load-time calls are on the main state via `prepare_mod` (:6311) and `setup_lua_global_functions_main_state_only` (:6458, block starts :4573).

| API | Used at | Status | Definition |
|---|---|---|---|
| `print` | main.lua:6,189,194,335,342,501,636,643,746 | REGISTERED | LuaMod.cpp:1521; impl LuaLibrary.cpp:35 (`[Lua] ` prefix) |
| `require("UEHelpers")` | main.lua:15 | REGISTERED (native searcher) | LuaMod.cpp:1149-1164, 848-893 (5th candidate path :874 hits staged `shared/UEHelpers/UEHelpers.lua`) |
| `FindFirstOf` | main.lua:21,64; UH:41,51 | REGISTERED | LuaMod.cpp:1632 |
| `FindAllOf` | UH:72 | REGISTERED | LuaMod.cpp:1667 |
| `StaticFindObject` | main.lua:44,283,499; UH:22 | REGISTERED | LuaMod.cpp:1529 |
| `StaticConstructObject` | main.lua:50,516,524,527,530,550,560 | REGISTERED | LuaMod.cpp:2200 |
| `FName` / `NAME_None` / `EFindName` | main.lua:50,516; UH:252-270 | REGISTERED | LuaMod.cpp:6201, :6202-6203, :698-702 |
| `FText` | main.lua:434,469 | REGISTERED | LuaMod.cpp:6210 |
| `LoadAsset` | main.lua:285 | REGISTERED | LuaMod.cpp:3618 |
| `RegisterKeyBind` + `Key` table (F7/RETURN/UP/DOWN/LEFT/RIGHT_ARROW) | main.lua:694-729 | REGISTERED | LuaMod.cpp:1915; Key global :614 (F7 :553, RETURN :458, arrows :480-483) |
| `ExecuteInGameThread` | main.lua:695,700,705,710,716,725 | REGISTERED (main state) — drain gated | LuaMod.cpp:4700; queue :4774; drain `engine_tick_hook` :4467-4513 (identity gate :4475); availability UE4SSRuntime.cpp:10-16 |
| `RegisterLoadMapPreHook` | main.lua:731 (pcall-wrapped) | REGISTERED | LuaMod.cpp:2711; callback :6909-6912; engine gate `HookLoadMap` default true (SettingsManager.hpp:119) |
| `CreateInvalidObject` | UH:15,35,45,62,81,91,106,116,127,148,195 | REGISTERED | LuaMod.cpp:1524 |
| `ModRef:Get/SetSharedVariable` | UH:18,23 | REGISTERED | ModRef global LuaMod.cpp:6316-6317; methods LuaModRef.cpp:61/:170 |
| `UnrealVersion:IsBelow` | UH:198,200 | REGISTERED | LuaMod.cpp:6194 (global), IsBelow ≈:6172 |
| `obj:IsValid()` | main.lua:10; UH: throughout | REGISTERED (metatable) | port LuaUObject.hpp:185, :761 |
| dynamic UFunction calls / property read+write (`GetPawn`, `K2_GetPawn`, `K2_GetActorLocation`, `GetControlRotation`, `LineTraceSingle`, `BeginDeferredActorSpawnFromClass`, `FinishSpawningActor`, `GetComponentByClass`, DB getters :163/:168, UMG setters, `SetEnabled`, `SetActorHiddenInGame`, `SetActorEnableCollision`, `K2_DestroyActor`, `SetIgnoreLook/MoveInput`, `ToString`) | main.lua:32-33,59,163,168,208,233-234,245,295,303,325-330,345,364-561,450-451 | DYNAMIC REFLECTION (resolved vs UClass at runtime; nothing to register) | port LuaUObject.cpp:151-327 (`call_ufunction_from_lua` → `ProcessEvent` :327); `GetWorld` example LuaUObject.hpp:640 |
| `RegisterHook`/`RegisterCustomEvent`/script hooks | NOT USED | REGISTERED but **silently disabled by settings v3** (see §4) | LuaMod.cpp:7677-7686 |
| `RegisterConsoleCommand(GlobalHandler)`, `NotifyOnNewObject`, `LoopAsync`, `ExecuteWithDelay`, InitGameState/BeginPlay hooks, `IterateGameDirectories`, … | NOT USED | REGISTERED | LuaMod.cpp registrations block :1519-4573 |

**Absent APIs: none.** The full gap analysis (including the one silently degraded subsystem and exact error shapes) is in API-GAP.md.

## 4. Settings interaction (v3)

- **`HookProcessInternal=0` + `HookProcessLocalScriptFunction=0`** (ini :34-35) skip exactly one initializer block: the ExecuteUbergraph FuncPtr jmp-resolve + 164-byte Zydis walk that derives ProcessInternal/ProcessLocalScriptFunction (port `UnrealInitializer.cpp:1424-1484`). The only consumer of those addresses is the custom-events script hook, which is then silently not installed (`LuaMod.cpp:7677-7686`, no else branch). **The mod uses neither** — its interactivity rides on:
  - keybinds → `Handler` (pad), no hook dependency;
  - `ExecuteInGameThread` → `HookEngineTick` (default true, v3 untouched; address armed at initializer:1756-1861) + game-thread identity from `HookedEngineTick` (:172 `publish_current`);
  - Lua→engine calls → `HookUObjectProcessEvent` (default true; vtable-resolved initializer:1932-1938);
  - `RegisterLoadMapPreHook` → `HookLoadMap` (default true).
  ⇒ **No part of the mod-load path depends on the two disabled hooks.** Loss is limited to RegisterHook/custom-event users, which this mod is not.
- **Key drift: none.** All 13 v3 keys are parsed by the composed SettingsManager (CLAIM.json settings_interaction block). Keys the runtime reads but v3 omits keep defaults that are exactly what launch-10 needs (HookLoadMap/HookEngineTick/HookUObjectProcessEvent true, EngineTickResolveMethod Scan, EnableDebugKeyBindings false…). `[Overrides]` correctly absent — native activation refuses ModsFolderPath/ControllingModsTxt (SettingsManager.cpp:112-113).
- **`[EngineVersionOverride] 5.6`** is consumed (comp6-program:1011-1023; sets `Unreal::Version`, nulls the version scan) and is the assumed version in the armed design ("engine version via EngineVersionOverride settings", initializer:985).
- **Native services gate** (comp6-program:728-757): v3 passes every check (dumping 0, consoles 0, RenderMode ExternalThread, hot-reload unset). Any deviation hard-throws at ctor — v3 is exactly right here.
- **`DefaultExecuteInGameThreadMethod=EngineTick`** (ini :12 → comp6-program:1494) selects the `m_engine_tick_actions` queue that all six mod callbacks use.

## 5. Timing / input

PS5 has no keyboard path; the input path is DualSense-pad-only, and it is fully wired in the composition:
`ue4ss/PS5Pad.ini` (staged, :34-41: OPTIONS→F7, UP/DOWN/LEFT/RIGHT→arrows, CROSS→RETURN) → `ConfigureRetainedPS5PadInput` at backend ctor (NativePadInput.cpp:10-26; absent file = no pad bridge, failing provider = start throws) → `Handler::init` from `context.input_source` in `validate_native_services` (comp6-program:728-757) → polled on the UE4SS UpdateThread ~5 ms → `RegisterKeyBind` callbacks run **synchronously on the UpdateThread** → the mod wraps every callback in `ExecuteInGameThread`, deferring real work to the game-thread drain. Full hop-by-hop audit (with linked-object symbol proof) already exists and matches: `outputs/2026-10-04-zcode-team/worker-pad-chord-audit/PAD-DISPATCH-PATH.md`.

The pad path needs none of the disabled hooks; its only engine dependency is the EngineTick drain of the deferred bodies (§2 verdict 5/6).

## 6. UE4SS.log oracle (success path)

| Stage | Lines |
|---|---|
| ctor/inventory | `Setting up mods...`; `Native console, GUI, crash dumping and filesystem watching are unavailable`; `UE4SS - v…` banner |
| engine-ready | `[Seeds] Required-slot seed table armed` + `slot 3 gmalloc bound at` (armed lineage); absence of `[Seeds] REFUTED` |
| **mod executed** | **`[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`** — THE oracle line; plus zero occurrences of `Error loading script:` / `Error executing script:` / `Failed to execute main script:` / `Mod is absent from native Lua source:` |
| menu opened (needs save + OPTIONS) | `[TownfallItemSpawner] N spawnable pickups found. Pages=M` then `[Lua] [TownfallItemSpawner] Menu created.` |
| runtime-status.txt | `activated` → `engine-ready` → `mods-started`; note `mods-started` ack (ue4ss_backend.cpp:41-45) fires at engine-ready because `start_native_mods` succeeded in the ctor — it does NOT prove main.lua ran. The `[Lua] Loaded` line is the only mod-executed proof. |
