# API-GAP — absent/degraded UE4SS Lua APIs for the Townfall Item Spawner on launch-10

Worker: worker-mod-load-readiness · 2026-10-04. Companion to READINESS.md / CLAIM.json.
Scope: the UNCHANGED mod (`main.lua` 21,415 B + `UEHelpers.lua` 9,962 B, v3 staged package) against the 368-unit composed runtime (run-3 / l10r3 receipts) with settings v3.

## Headline

**There are NO absent APIs.** Every global, table, callback hook and metatable method that main.lua and UEHelpers.lua touch at LOAD time is registered in the composed `luamod` unit. Zero `EXPECTED LOAD BLOCKERS` of the "attempt to call a nil value" class exist for this mod at load.

What remains is one **silently degraded subsystem** (not used by this mod), one **variant-conditional drain blocker** (R3 fold-1 candidate only), and a set of **in-game-gated dynamic resolutions** that are registered-by-mechanism but unproven until a live capture.

## 1. Load-time surface — all present

Verified registrations (composed unit `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/src/LuaMod.cpp`, applied to the mod's main state via `prepare_mod` :6311 + `setup_lua_global_functions_main_state_only` :6458):

| Required at load | Registration |
|---|---|
| `print` | :1521 |
| `require` module loader (native buffer searcher) | :1149-1164 + :848-893 |
| `RegisterKeyBind` + `Key`{F7,RETURN,UP_ARROW,DOWN_ARROW,LEFT_ARROW,RIGHT_ARROW} | :1915; :614/:553/:458/:480-483 |
| `RegisterLoadMapPreHook` | :2711 |
| `StaticFindObject` / `StaticConstructObject` / `FindFirstOf` / `FindAllOf` / `CreateInvalidObject` | :1529 / :2200 / :1632 / :1667 / :1524 |
| `FName` / `NAME_None` / `EFindName`{FNAME_Find,FNAME_Add} / `FText` | :6201 / :6202-6203 / :698-702 / :6210 |
| `UnrealVersion`{IsBelow,…} | :6194 (IsBelow ≈:6172) |
| `LoadAsset` | :3618 |
| `ExecuteInGameThread` | :4700 (registered BEFORE main.lua runs: start_mod :6458 precedes :6473) |
| `ModRef`{GetSharedVariable,SetSharedVariable} | :6316-6317; LuaModRef.cpp:61/:170 |
| `IsValid` method on UObject userdata | port `include/LuaType/LuaUObject.hpp:185,761` |

Because main.lua is written defensively (its own `log()` uses plain `print`; every risky call is `pcall`-wrapped; `require` result is checked :15-19), even a hypothetical missing helper would degrade rather than abort the chunk.

## 2. Silently degraded by settings v3 (registered, but inert) — NOT a blocker for this mod

**Custom events / `RegisterHook` / script hooks.**
- Mechanism: the hook is installed at `LuaMod.cpp:7677-7686` ONLY if `ProcessLocalScriptFunctionInternal.is_ready()` (UE ≥4.22) or `ProcessInternalInternal.is_ready()` (<4.22). With v3's `HookProcessInternal=0` and `HookProcessLocalScriptFunction=0`, the initializer skips the derivation block (`port/unreal/src/UnrealInitializer.cpp:1424-1484`), so neither is ever ready and the `if/else-if` falls through **with no else and no log line**.
- What is lost: `RegisterHook(fn)` on script/BP functions, `RegisterCustomEvent`, and ProcessLocalScriptFunction-post-based callbacks.
- Mod impact: **none** — main.lua uses none of these (grep of main.lua + UEHelpers.lua: no RegisterHook/RegisterCustomEvent).
- If a future mod needed it, the error shape today is silent (callbacks simply never fire); the minimal fix is a settings profile with the hooks at 1 for mods that need them (launch-10 cannot do that — the flags exist precisely to avoid the init-time text reads), or a ScanOverrides-based address source.

## 3. Variant-conditional blocker: `ExecuteInGameThread` drain

Registered fine; the question is whether the queued action ever RUNS.

| Variant | Identity publication | EngineTick pump | Consequence for the mod |
|---|---|---|---|
| **readpath-guard** (witness-verified launch-10 staged initializer, `worker-launch10-readpath-guard/staging/UnrealInitializer.cpp` 1fc82e6b…) | `HookedEngineTick` → `NativeGameThreadIdentity.publish_current()` (:172, registration :1844) | REGISTERED (bHookEngineTick default true; v3 does not touch it) | drain path structurally complete: `engine_tick_hook` (LuaMod.cpp:4467) passes the `IsInGameThreadRaw()` gate (:4475) once the real UEngine::Tick publishes identity → `process_simple_actions(m_engine_tick_actions)` (:4488) runs `toggleMenu`/`moveSelection`/`activateSelection`. In-game UNKNOWN (needs a live Tick on the game thread), no structural blocker. |
| **l10r3 R3-armed fold-1** (BOTH_FAIL candidate, `worker-launch10-r3-armed/staging/initializer-l10-r3/UnrealInitializer.cpp` 8b0c7e99…) | initializer EngineTick registrations compiled OUT (:1847-1862 print `[R3] refused: …EngineTick pre-callback registrations…` + `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal`); `pump_pending` set :1861 | REFUSED — designated drain consumer is LuaMod DIFF 1, which was proven NOT composable through the frozen composition-6 script (REPORT §2 blocker 1) and is not in the candidate | `pump_pending` has zero consumers in the composed object (workspace grep: bridges defined only in the initializer; no references from luamod/program units). Pad presses still reach the Lua keybind callbacks on the UpdateThread, `ExecuteInGameThread` still queues (:4774) — and nothing ever dequeues. **Menu can never open; nothing can spawn.** Expected log evidence: the two `[R3] refused:`/deferral lines, then silence from the drain. |

Minimal fix candidates (in preference order):
1. Ship launch-10 on the readpath-guard lineage (already the witness-verified staged input) — no code change.
2. If the R3 mode is required, land DIFF-1 through a comp-7-class driver clone or an attested `RETAINED_REPLACEABLE` allow-list extension so the LuaMod-side marshal drain exists (worker-launch10-r3-armed REPORT §5 items i/ii).
3. Controller-attested replacement of ANY composed unit that consumes `R3MarshalPumpPending` from the game thread (e.g., a bridge in the query-harness unit) — new bounded task, new IDs.

## 4. Registered-by-mechanism but in-game-gated (UNKNOWN until capture, not gaps)

- **Dynamic UFunction calls and property read/write** (`GetPawn`, `LineTraceSingle`, UMG setters, DB getters, property writes like `pickup.PickupAmount = …`): resolved at runtime against live UClass reflection through `call_ufunction_from_lua` → `ProcessEvent` (port LuaUObject.cpp:151-327). Nothing can be "missing" at the registration level; success depends on engine-ready reflection facts (layouts, addresses) that launch-10's engine-ready stage produces.
- **Out-param marshalling** for `LineTraceSingle(..., hit, ...)` then `hit.ImpactPoint/.ImpactNormal` (main.lua:245-269): route exists; historically the highest-risk item (worker-itemspawner-lua-audit LUA-API-MATRIX §2). Failure shape: `Failed at ground trace. See UE4SS.log.` status text (main.lua:341) — mod stays alive.
- **`LoadAsset`** (main.lua:285): implemented (LuaMod.cpp:3618, AssetRegistry route); likely unnecessary for the six AlwaysCook classes; guarded by pcall/error path in the mod.
- **`FName`→string (`Scan` method from v3 ini :13)**: Scan route + Kismet `Conv_NameToString` fallback retained (settings :161-169); live proof pending.

## 5. Exact error shapes the runtime would emit (for the acceptance oracle)

| Failure | UE4SS.log shape | Source |
|---|---|---|
| Mod absent from snapshot (key drift between loadlist and discovery) | `Mod is absent from native Lua source: <path>` and mod not installable | LuaMod.cpp:911-912 |
| main.lua byte-syntax error | `Error loading script: <lua parse error>` then `Failed to execute main script: <path>` | LuaMod.cpp:6423, :6475 |
| main.lua runtime error at load (nil global, bad arg) | `Error executing script: …attempt to call a nil value (global 'X')…` | LuaMod.cpp:6434 (lua_pcall + error handler :6416) |
| main.lua `error()` at load (e.g. class() helper) | `Error executing script: <chunk>:<line>: Could not find class: <path>` | main.lua:45 via :6434 |
| require('UEHelpers') unresolvable | `Error executing script: …\n\tno native Lua file 'ue4ss/Mods/shared/…'` (5 candidate keys listed) | LuaMod.cpp:876-892 |
| Exception in C++ during script load | `Exception in load_and_execute_script: <what>` | LuaMod.cpp:6445 |
| ExecuteInGameThread with EngineTick unavailable | immediate Lua error at registration: `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)` | LuaMod.cpp:4729 |
| Keybind registered with no input source | startup throw `Native input service is unavailable…` / `Input registration unavailable: no active input source` | comp6-program:728-757; Handler.cpp:109,123 |
| Missing/invalid UE4SS-settings.ini under native activation | `Native activation is missing ue4ss/UE4SS-settings.ini` (no UE4SS.log yet; surfaces via startup error/ack `failed`) | SettingsManager.cpp:63 |
| 0-byte staged file with null preload pointer | `UE4SS file was not preloaded: ue4ss/Mods/TownfallItemSpawner/enabled.txt` | activated_mod_source.cpp:40-41 |
| R3 variant (expected, not an error) | `[R3] refused: EngineTick pre-callback registrations (GameThreadInitializer + QueryHarnessDispatch) (detour registration compiled out under R3 marshal mode)` + `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` | initializer-l10-r3:1859-1860 |

## 6. Minimal fixes summary

| Finding | Fix |
|---|---|
| No absent load-time APIs | none required |
| Custom-event hooks inert under v3 | accepted (unused by this mod); a separate profile for hook-dependent mods later |
| R3-variant pump with no consumer | prefer readpath-guard lineage for launch-10; else DIFF-1 via comp-7-class driver/allow-list (REPORT §5) |
| 0-byte enabled.txt preload semantics | verify host mfa preload of empty files yields non-null data (host-side check; or stage a 1-byte enabled.txt with a newline — semantically identical for the presence-based gate at NativeModSource.cpp:134-135) |
