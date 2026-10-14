# MOD-COMPAT — UE4SS Lua mod compatibility analysis for the PS5 runtime port

Worker: worker-mod-compat · 2026-10-05 · read-only analysis + design. No code changed, no
console contact, no game/runtime execution, no completion or quality claims.

**Question answered:** what does it take for "all mods to work" — not just the Item Spawner —
on this PS5 UE4SS port?

**One-paragraph answer.** The port's registered Lua surface is effectively complete: the
c92c92ff-lineage `LuaMod.cpp` registers **83 live global functions** plus **~20 global
tables/classes/constructors**, and the full 30-unit LuaType tree (per-object methods, TArray/
TMap/TSet, FName/FText/FString, UClass/UStruct/UWorld, delegates) is present. Almost nothing a
mod imports at load time is missing. What separates "mods that work" from "mods that silently
degrade" is **execution gating**: the PS5 no-exec law (walkguard GATE-1 exec-read refusal +
owned-memory soft-refusal of every live game-text touch) defers all initializer-class detour
registrations in the R3 FuncPtr-marshal world, which means **eight callback families are
accepted by `register_function` but never fire** (LoadMap, InitGameState/BeginPlay/EndPlay,
game-side NotifyOnNewObject, custom events, ULocalPlayerExec, CallFunctionByNameWithArguments,
ProcessConsoleExec/console-command handlers, BP-function RegisterHook). The unlock path is not
more API registrations; it is (a) proving the R3 marshal pump in-game, and (b) re-implementing
the deferred event families on top of legal mechanisms — the already-working UFunction
FuncPtr-swap (RW heap store, zero exec writes) and data-read polling inside the marshal drain.

---

## 1. API inventory (c92c92ff lineage)

Source of truth: `outputs/2026-10-04-zcode-team/worker-luamod-lineage/offline-proof/c92c92ff-current-headers/LuaMod.cpp`
(8342 lines; byte-identical to `worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp`).
All line numbers below are that file.

### 1.1 Registered global functions — 83 live

56 in `setup_lua_global_functions_internal` (mod state; called per mod and once for the
console executor at :7228):

| # | Function | Line | Notes |
|---|---|---|---|
| 1 | `print` | 1551 | plus full stdlib via `lua.open_all_libs()` :6802 |
| 2 | `LoadExport` | 1552 | LuaLibrary.cpp:112 (`loadexport` alias in Signatures.cpp:67) |
| 3 | `CreateInvalidObject` | 1554 | |
| 4 | `StaticFindObject` | 1559 | both overloads (string; class/outer/name/exact) |
| 5 | `FindFirstOf` | 1662 | |
| 6 | `FindAllOf` | 1697 | |
| 7 | `IsKeyBindRegistered` | 1765 | |
| 8 | `RegisterKeyBindAsync` | 1827 | port addition |
| 9 | `RegisterKeyBind` | 1945 | keydown via `m_program.register_keydown_event`; keys 0–255 |
| 10 | `UnregisterHook` | 2062 | |
| 11 | `DumpAllObjects` | 2155 | writes file (Windows `\` path join — port nit) |
| 12 | `GenerateSDK` | 2166 | writes CXXHeaderDump |
| 13 | `GenerateLuaTypes` | 2177 | writes Mods/shared/types |
| 14 | `GenerateUHTCompatibleHeaders` | 2188 | |
| 15 | `DumpStaticMeshes` | 2194 | posts to internal GUI thread — no GUI on PS5 |
| 16 | `DumpAllActors` | 2199 | posts to internal GUI thread — no GUI on PS5 |
| 17 | `DumpUSMAP` | 2204 | |
| 18 | `DumpJMAP` | 2214 | |
| 19 | `StaticConstructObject` | 2230 | FStaticConstructObjectParameters route |
| 20 | `RegisterCustomProperty` | 2340 | |
| 21 | `ForEachUObject` | 2597 | data-read iteration |
| 22 | `NotifyOnNewObject` | 2618 | appends to StaticConstructObject-callback list |
| 23 | `RegisterCustomEvent` | 2683 | |
| 24 | `UnregisterCustomEvent` | 2723 | |
| 25 | `RegisterLoadMapPreHook` | 2741 | |
| 26 | `RegisterLoadMapPostHook` | 2768 | |
| 27 | `RegisterInitGameStatePreHook` | 2795 | |
| 28 | `RegisterInitGameStatePostHook` | 2823 | |
| 29 | `RegisterBeginPlayPreHook` | 2851 | |
| 30 | `RegisterBeginPlayPostHook` | 2879 | |
| 31 | `RegisterEndPlayPreHook` | 2907 | |
| 32 | `RegisterEndPlayPostHook` | 2935 | |
| 33 | `IterateGameDirectories` | 2963 | expects Win64 layout; returns nil + warning otherwise |
| 34 | `CreateLogicModsDirectory` | 3297 | |
| 35 | `ExecuteAsync` | 3368 | port async thread |
| 36 | `ExecuteWithDelay` | 3389 | |
| 37 | `LoopAsync` | 3420 | |
| 38 | `RegisterProcessConsoleExecPreHook` | 3452 | |
| 39 | `RegisterProcessConsoleExecPostHook` | 3473 | |
| 40 | `RegisterCallFunctionByNameWithArgumentsPreHook` | 3494 | |
| 41 | `RegisterCallFunctionByNameWithArgumentsPostHook` | 3515 | |
| 42 | `RegisterULocalPlayerExecPreHook` | 3536 | |
| 43 | `RegisterULocalPlayerExecPostHook` | 3557 | |
| 44 | `RegisterConsoleCommandGlobalHandler` | 3578 | |
| 45 | `RegisterConsoleCommandHandler` | 3613 | |
| 46 | `LoadAsset` | 3648 | UAssetRegistry reflected route (3 ProcessEvent-class calls) |
| 47 | `FindObject` | 3696 | |
| 48 | `FindObjects` | 3861 | |
| 49 | `GetCurrentThreadId` | 4020 | port addition |
| 50 | `GetMainModThreadId` | 4031 | port addition |
| 51 | `GetAsyncThreadId` | 4043 | port addition |
| 52 | `GetGameThreadId` | 4055 | port addition |
| 53 | `IsInMainModThread` | 4066 | port addition |
| 54 | `IsInAsyncThread` | 4078 | port addition |
| 55 | `IsInGameThread` | 4090 | port addition |

27 in `setup_lua_global_functions_main_state_only` (main mod thread state):

| # | Function | Line | Notes |
|---|---|---|---|
| 56 | `RegisterHook` | 4860 | native branch = UFunction FuncPtr swap; script branch = ProcessInternal-class (deferred) |
| 57 | `ExecuteInGameThread` | 4988 | L11 enqueue-first; EngineTick/ProcessEvent/FuncPtrMarshal pumps |
| 58 | `ExecuteInGameThreadWithDelay` | 5192 | |
| 59 | `RetriggerableExecuteInGameThreadWithDelay` | 5386 | |
| 60 | `ExecuteInGameThreadAfterFrames` | 5541 | requires EngineTick resolve |
| 61 | `LoopInGameThreadWithDelay` | 5601 | |
| 62 | `LoopInGameThreadAfterFrames` | 5729 | |
| 63 | `ResetDelayedActionTimer` | 5790 | port-added delayed-action family |
| 64 | `SetDelayedActionTimer` | 5854 | |
| 65 | `PauseDelayedAction` | 5924 | |
| 66 | `UnpauseDelayedAction` | 6003 | |
| 67 | `CancelDelayedAction` | 6053 | |
| 68 | `IsValidDelayedActionHandle` | 6100 | |
| 69 | `IsDelayedActionActive` | 6142 | |
| 70 | `IsDelayedActionPaused` | 6184 | |
| 71 | `GetDelayedActionTimeRemaining` | 6226 | |
| 72 | `GetDelayedActionTimeElapsed` | 6309 | |
| 73 | `GetDelayedActionRate` | 6394 | |
| 74 | `ClearAllDelayedActions` | 6452 | |
| 75 | `MakeActionHandle` | 6489 | |
| 76 | `RestartCurrentMod` | 6500 | |
| 77 | `UninstallCurrentMod` | 6514 | |
| 78 | `RestartMod` | 6529 | |
| 79 | `UninstallMod` | 6546 | |
| 80 | `FString` (constructor) | 6708 | |
| 81 | `FUtf8String` (constructor) | 6723 | |
| 82 | `FAnsiString` (constructor) | 6738 | |
| 83 | `FName` call-constructor + `FText` call-constructor | 6693–6703 | FName/FText globals with MetaMethod::Call |

(A further 6 `register_function("RegisterHook"/"NotifyOnNewObject", …)` texts at :1051–:1108 are
inside dead comment blocks — not registered.)

### 1.2 Globals: tables, classes, constants

| Global | Line | Contents |
|---|---|---|
| `Key` | 644 | full key-code table (KeyDef.hpp, e.g. F7=0x76, RETURN=0x0D, arrows) |
| `ModifierKey` | 656 | |
| `EObjectFlags` | 702 | |
| `EInternalObjectFlags` | 722 | |
| `EFindName` | 732 | FNAME_Find / FNAME_Add |
| `PropertyTypes` | 1152 | 21 property-type tables (ObjectProperty…Utf8StrProperty) |
| `ue4ss_loaded_modules` | 1161 | custom module loader bookkeeping |
| `loadfile` / `dofile` | 1205–1211 | native-memory buffer route (no target-disk fallback) |
| `AsyncThread` / `MainThread` | 1072 / 1098 | lua_newthread handles (HookThread commented out) |
| `EGameThreadMethod` | 4975 | EngineTick / ProcessEvent / FuncPtrMarshal (port addition) |
| `EngineTickAvailable` | 4983 | capability boolean (`UEngine::TickInternal.is_ready()`) |
| `ProcessEventAvailable` | 4985 | capability boolean (`UObject::ProcessEventInternal.is_ready()`) |
| `UE4SS` | 6616 | `GetVersion()` (3 returns) |
| `UnrealVersion` | 6687 | GetMajor/GetMinor/IsEqual/IsAtLeast/IsAtMost/IsBelow/IsAbove |
| `FName` / `NAME_None` | 6694–6696 | constructor + constant |
| `FText` | 6703 | constructor |
| `FPackageName` | 6789 | |
| `ModRef` | 6810 | SetSharedVariable (:61) / GetSharedVariable (:170) / `type` (:240) in LuaModRef.cpp |
| `__OriginalReturnValue` | 6820 | inside hook-callback context |

### 1.3 Per-object surface (LuaType lineage — audited tree, menu-spawn 01-API-AUDIT.md)

30 LuaType units present (`native-coherent-runtime-1/source/port/ue4ss/src/LuaType/`):
LuaAActor, LuaCustomProperty, LuaFName, LuaFOutputDevice, LuaFSoftObjectPath, LuaFText,
LuaFURL, LuaFWeakObjectPtr, LuaModRef, LuaTArray, LuaThreadId, LuaTMap, LuaTSet,
LuaTSoftObjectPtr, LuaUClass, LuaUDataTable, LuaUEnum, LuaUInterface, LuaUScriptStruct,
LuaUStruct, LuaUWorld, + 9 XProperty wrappers.

`LuaUObject` per-instance methods: `CallFunction, get/Get, GetAddress, GetClass, GetFName,
GetFullName, GetOuter, GetProperty, GetPropertyValue, GetWorld, HasAllFlags, HasAnyFlags,
HasAnyInternalFlags, IsA, IsAnyClass, IsClass, IsValid, ProcessConsoleExec, Reflection,
set/Set, SetPropertyValue, type` — plus generic UFunction dispatch (MetaMethod::Index →
`handle_unreal_property_value` → `call_ufunction_from_lua` with the PS5 `__PROSPERO__`
protected-call guard, LuaUObject.cpp:135–151) and typed property read/write.

**Inventory count: 83 live global function registrations + 20 global tables/classes/constants +
22 per-object methods + 30 LuaType units.** Six additional registration texts are dead code
(commented out).

---

## 2. Capability tiers (what actually fires on PS5)

Registration is not the bottleneck; **execution law** is. Three tiers:

**TIER A — works, no detour dependency** (registration audited by worker-menu-spawn 01-API-AUDIT;
mechanism = data reads, RW-heap writes, port-owned threads):
print/stdlib/require/shared paths/loadfile/dofile; RegisterKeyBind(+Async)/IsKeyBindRegistered/
Key/ModifierKey; StaticFindObject/FindObject/FindObjects/FindFirstOf/FindAllOf/ForEachUObject/
CreateInvalidObject; StaticConstructObject; LoadAsset (reflected registry route);
RegisterCustomProperty; UFunction invocation + property read/write via reflection; FName/FText/
FString ctors; ModRef shared variables; ExecuteAsync/ExecuteWithDelay/LoopAsync (port async
thread); thread-ID APIs; RestartMod/UninstallMod; UE4SS:GetVersion; UnrealVersion; dumpers that
write files (with the GUI-thread pair degraded — see 4.1).

**TIER B — pump-dependent: works iff the R3 FuncPtr-marshal game-thread pump is armed**
(the launch-11 primary path; enqueue-first is in the audited bytes, arm/drain logic at
LuaMod.cpp:4153–4180, 4553–4622, 4725+; **in-game pump proof is still pending — launch-10 never
landed, launch-9 loaded zero mods**): ExecuteInGameThread and the whole game-thread family
(WithDelay/AfterFrames/Loop variants, 19 delayed-action timer functions), plus the drain of
queued NotifyOnNewObject callbacks for objects constructed through the port's own
StaticConstructObject. Every closure lands in a queue that nothing drains until a pump exists —
under a refused arm the queue stalls loudly (":5178/:5182 stall-honesty logs").

**TIER C — accepted but never fire in the R3 world (detour-class, deferred).**
`LuaMod::on_program_start()` (:7397) arms all initializer-class engine detours inside one block
guarded by `if (Unreal::R3MarshalPumpPending())` (:7451). In the R3 world the block is skipped
with ONE loud warning (:7453) that names exactly what stays unregistered for the process
lifetime (no re-arm path): **LoadMap/InitGameState/BeginPlay/EndPlay pre+post,
StaticConstructObject-post (the engine-side driver of NotifyOnNewObject), ULocalPlayerExec
pre+post, CallFunctionByNameWithArguments pre+post, ProcessConsoleExec (hence
RegisterConsoleCommandHandler/GlobalHandler), and custom-event script hooks.** The Lua-side
register calls still succeed (callbacks are appended to their lists) — mods load cleanly but
callbacks never fire. The composition rationale is pinned in the :7408–7450 comment: every PLH
touch of live game text soft-refuses at the owned-memory layer (zero syscalls, crash-free) —
each arm would be futile; deferral trades ~19 per-arm failure pairs for one honest warning.
Reachability note (:7447–7449): the gate is only reached in worlds where the initializer's
resolve walks were admitted (walkguard fold-1 or seeded); at fold-0 startup fails closed before
`on_program_start`.

**TIER D — plausible but unproven: `RegisterHook` on native UFunctions.** The native branch
(:4913–4927) registers via `UFunction::RegisterPreHook/RegisterPostHook` → GetFunctionHookData →
`SetFuncPtr(&UnrealScriptFunctionHook)` — the SAME one-aligned-pointer-store-into-RW-heap
mechanism as the R3 marshal carrier (:4553 comment; "zero syscalls"), NOT a PLH exec-text
detour, and NOT part of the deferred on_program_start block. The branch check needs
`ProcessInternalInternal.get_function_address()` (reachable only in fold-1/seeded worlds where
resolves succeeded). This tier is the biggest unexploited unlock; it needs an explicit
verification task and a loud refusal (not silent misroute) for script/BP functions while the
ProcessInternal-class route stays deferred.

---

## 3. Common mod patterns vs the port

Evidence base: four real Townfall mods in
`.superpowers/native-observations/nexus-real-mods-2026-10-01/` + the canonical Item Spawner
(`worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua`), plus the
UE4SS public API knowledge. Per-mod API-use counts were extracted by grep from the actual
main.lua bytes (MiniMap 196,743 B; ModMenu 128,009 B).

### Pattern 1 — Key-bind menus (Item Spawner style)
APIs: `RegisterKeyBind`, `Key`, `ExecuteInGameThread`, `FindFirstOf/FindAllOf`,
`StaticFindObject`, `StaticConstructObject`, `LoadAsset`, UMG-through-reflection, `FName`,
`FText`, `RegisterLoadMapPreHook` (pcall-wrapped state reset).
Status: **the proven pattern** — every API in its load/menu/spawn phases is registered
(01-API-AUDIT verdict: NO BLOCKERS; remaining risk is game-side object existence). LoadMap
reset degrades to TIER C (stale caches between maps; the mod tolerates it via pcall).

### Pattern 2 — LoadMap hooks (map-load callbacks)
APIs: `RegisterLoadMapPreHook/PostHook`.
Used by: ModMenu (3 + 2), ItemSpawner (1 each, pcall-wrapped).
Status: **TIER C — registered, never invoked** in the R3 world (:7453 names this loss
explicitly; consequence for the Item Spawner documented at :7444–7446: per-map state reset
lost). This is the highest-frequency silent degradation across the real mod set.

### Pattern 3 — ProcessEvent hooks (function interception)
APIs: `RegisterHook` (+ `UnregisterHook`).
Used by: ModMenu (11 — mix of BP paths like
`/Game/Townfall/AI/TheWidows/BTT_WidowAttackDecision_C:ReceiveExecuteAI` and native paths like
`/Script/Engine.PlayerController:ClientRestart`, `/Script/Townfall.TownfallPlayerCharacter:
IsCheatHiddenFromAIPerception`, ammo/durability getters; all pcall-wrapped), MiniMap (1).
Status: **split.** Native-UFunction hooks = TIER D (mechanism legal, unproven, needs the
script/native branch check to be resolvable and honest). BP/script-function hooks = TIER C
(their firing route is the ProcessInternal-class registration the deferral names). Mods that
need behavior *interception* (cheat menus, AI tweaks) are the big losers here.

### Pattern 4 — Object finding/spawning
APIs: `FindFirstOf/FindAllOf/FindObject(s)/StaticConstructObject/LoadAsset/ForEachUObject` +
`NotifyOnNewObject` as the "something spawned" callback.
Used by: everything (MiniMap: FindAllOf×7, StaticConstructObject×21, NotifyOnNewObject×2;
ModMenu: FindAllOf×12, NotifyOnNewObject×8 on `/Script/Townfall.TownfallPlayerCharacter`,
BP_MotionlessTrigger_C, BP_TheMotionless_C, TownfallEnemyCharacter).
Status: **finding/spawning = TIER A (works).** `NotifyOnNewObject` for game-constructed objects
= **TIER C** (its engine-side driver, the StaticConstructObject post-callback, is inside the
deferred block — the queue at :7718 is only filled by that never-registered callback).
Polling substitutes exist (see 5).

### Pattern 5 — Deferred/async execution
APIs: `ExecuteInGameThread`, `ExecuteInGameThreadWithDelay`, `Retriggerable…`,
`ExecuteInGameThreadAfterFrames`, `LoopInGameThreadWithDelay/AfterFrames`, `ExecuteAsync`,
`ExecuteWithDelay`, `LoopAsync`, the delayed-action timer family.
Used by: ModMenu (10+18+6+4+2+1), MiniMap (4), ItemSpawner (6), Stamina (LoopAsync×1 +
ExecuteInGameThread).
Status: **TIER B.** Async-thread trio works unconditionally; the game-thread family works iff
the R3 marshal pump is armed and drained in-game (pending proof). Frame-based variants
additionally need the EngineTick resolve (`:5556` throws honestly otherwise).

---

## 4. Critical gaps

### 4.1 GUI/console — structurally absent, mostly survivable
There is **no Lua GUI API** in upstream UE4SS either — the GUI toolkit is C++-side; Lua menus
are UMG-through-reflection, which this port supports (TIER A, proven pattern). What dies on PS5:
- `RegisterConsoleCommandHandler/GlobalHandler` and the console-exec pre/post hooks — double
  dead: their engine detour is TIER C-deferred AND a shipping PS5 game has no console UI to
  type into. Mods whose primary interface is a console command are dead-on-arrival unless they
  also ship a keybind.
- `DumpStaticMeshes`/`DumpAllActors` post to the internal GUI thread (:2195/:2200) — no GUI
  event loop exists on PS5; treat as degraded/unavailable.
- File dumpers that write directly (:2155–:2190, :2204, :2214) are functional in principle but
  join paths with `"\\"` (Windows separator) — on POSIX this creates a literal-backslash
  filename; a real (small) port defect to flag, unverified at runtime.

### 4.2 ProcessEvent detouring — the walkguard gate (dominant)
The port's law (pinned across :7408–7450, engine-pump-chain REPORT/CLAIM): live game text is
never in the owned-memory registry, so every PLH copy/disassemble/protect/write of it
soft-refuses with zero syscalls; the initializer's own resolve walks additionally need the
walkguard RX verdict (GATE-1). Net effect: the entire initializer-class detour family is
permanently unregistered in the R3 world (TIER C), and only the UFunction FuncPtr-swap class
(marshal carrier; TIER D candidate) is legal against live code. This single law is what splits
"mods that work" from "mods that load and do nothing."

### 4.3 File watching — absent, and mostly irrelevant here
No filesystem-watcher API exists in the port (or in upstream UE4SS's Lua API — hot-reload is a
UE4SS-core feature watching `Mods/` on PC). On PS5 the mod tree lives on a writable data path
but there is no inotify-equivalent wired into the runtime, and loadfile/dofile intentionally
read from the retained native buffer, not target disk. Mods do not call this API (zero uses in
the four real mods); polling with `LoopAsync` is the universal substitute. Not worth building.

### 4.4 Network — absent, low demand
No networking API exists upstream or here (no LuaSocket, no HTTP). Zero uses in the real mod
set. Any mod needing network would need a port-side native extension — out of scope; document
as unsupported rather than build it.

---

## 5. Mod compatibility matrix

Legend: **FULL** = works today (TIER A) · **PUMP** = works once the R3 marshal pump is proven
in-game (TIER B) · **DEFERRED** = accepted, never fires in R3 world (TIER C) · **UNPROVEN** =
mechanism legal, needs explicit verification (TIER D) · **ABSENT** = no API anywhere.

| Capability (APIs) | ItemSpawner | MiniMap | ModMenu | InfiniteStamina | Generic mod class |
|---|---|---|---|---|---|
| Load/require/print/stdlib | FULL | FULL | FULL | FULL | FULL |
| Keybind menus (RegisterKeyBind/Key) | FULL | FULL | FULL | FULL | FULL |
| Object find (Find*/ForEachUObject) | FULL | FULL | FULL | FULL | FULL |
| Spawn/construct (StaticConstructObject/LoadAsset) | FULL | FULL | FULL | n/a | FULL |
| Reflection calls + property R/W | FULL | FULL | FULL | FULL | FULL |
| Async trio (ExecuteAsync/WithDelay/LoopAsync) | FULL | FULL | FULL | FULL | FULL |
| Game-thread exec family | PUMP | PUMP | PUMP | PUMP | PUMP |
| Delayed-action timers (19 fns) | n/a | PUMP | PUMP | n/a | PUMP |
| LoadMap hooks | DEFERRED (pcall-tolerant) | n/a | DEFERRED | n/a | DEFERRED |
| InitGameState/BeginPlay/EndPlay hooks | n/a | n/a | n/a | n/a | DEFERRED |
| NotifyOnNewObject (game-constructed) | n/a | DEFERRED (×2) | DEFERRED (×8) | n/a | DEFERRED |
| RegisterHook — native UFunctions | n/a | n/a (×1 target unverified) | UNPROVEN (majority of ×11) | n/a | UNPROVEN |
| RegisterHook — BP/script functions | n/a | DEFERRED (×1) | DEFERRED (×2 BP paths) | n/a | DEFERRED |
| Custom events (Register/UnregisterCustomEvent) | n/a | n/a | n/a | n/a | DEFERRED |
| Console commands (handlers + exec hooks) | n/a | n/a | n/a | n/a | DEFERRED + no console UI (double-dead) |
| ULocalPlayerExec / CallFunctionByName hooks | n/a | n/a | n/a | n/a | DEFERRED |
| GUI toolkit / debug GUI | ABSENT (not a Lua API) | — | — | — | ABSENT |
| File watching / hot-reload | ABSENT | — | — | — | ABSENT (poll instead) |
| Networking | ABSENT | — | — | — | ABSENT |

Per-mod verdict with current bytes:
- **TownfallItemSpawner**: FULL experience modulo PUMP proof; LoadMap reset loss tolerated by
  pcall (documented :7444–7446).
- **TownfallMiniMap**: loads; loses both NotifyOnNewObject spawns-callbacks and its single
  RegisterHook; keeps finding/construction/loop-delay core — degraded to manual-refresh unless
  additions 3/2 land.
- **TownfallModMenu**: loads with everything pcall-wrapped; loses 8 NotifyOnNewObject + BP hooks
  + LoadMap resets; native hooks unproven; its ExecuteInGameThreadWithDelay polling skeleton is
  the only thing keeping it alive — degraded but partially functional after PUMP proof.
- **InfiniteStamina**: FULL (pure reflection + LoopAsync + ExecuteInGameThread).

---

## 6. Top 5 additions ranked by mods-unlocked

Ranked by (number of real mods un-blocked) × (feature weight) ÷ (legal-mechanism risk). All five
stay inside the port's law: no exec-page read/write of live game text; every new mechanism is
either an RW-heap FuncPtr store (proven class) or a data-read poll inside the already-draining
marshal pump.

**1. In-drain LoadMap/world-change shim (re-arm RegisterLoadMapPreHook/PostHook semantics).**
Design: inside the marshal drain (funcptr_marshal_hook, :4553 — already game-thread-published
and already draining), compare the current world/level identity (data reads via retained
engine/world pointers, e.g. WorldContext world object pointer + streaming-level name) against
the previous drain tick; on change, run the m_load_map_pre/post callback lists with the same
RemoteUnrealParam+FURL marshalling the deferred block already implements (:7458–:7506).
Pre-hook semantics degrade to "just before next tick" (cannot block the actual LoadMap) — the
documented loss is acceptable for state-reset use, which is what every real mod uses it for.
Unlocks: ItemSpawner (correctness), ModMenu (×5), every map-aware mod. Risk: minimal — pure
data reads on objects the pump already touches. No initializer detour involved.

**2. Native-UFunction RegisterHook activation + verification (TIER D → proven).**
Design: keep the :4913–4927 native branch exactly as composed (it is already the marshal
carrier's legal mechanism); add (a) an explicit arm-time precheck that refuses — loudly, with a
named log — any target whose GetFunc() equals ProcessInternal when the script route is
deferred, so a BP target can never silently misroute; (b) a `NativeHookAvailable` capability
global next to :4983–:4985; (c) one frozen in-game verification task proving
`/Script/Engine.PlayerController:ClientRestart`-class hooks fire. Unlocks: ModMenu's native
hook set (ClientRestart, IsCheatHiddenFromAIPerception, ammo/durability getters — the
inventory/health behavior layer), the common "hook a C++ gameplay function" mod class.

**3. Per-UFunction FuncPtr-swap route for BP/script hooks.**
Design: for a script-function target, do NOT use the global ProcessInternal registration; swap
that one UFunction's FuncPtr (RW heap, per-object — ProcessInternal is saved as the original and
called through it), reusing GetFunctionHookData/m_script_hook_callbacks. Honest caveat to
document: BP pre-hook parm visibility differs from PC UE4SS behavior (params may be
uninitialized in pre-callbacks; post-callbacks are reliable) — mods must be told via the
capability global. Unlocks: ModMenu's BP-path hooks (×2–4), MiniMap's hook (×1), the
"suppress/observe a BP event" mod class. Risk: medium — first in-game proof of the swap on a
non-carrier script function; keep it behind the same loud-refusal discipline as #2.

**4. NotifyOnNewObject polling shim (FUObjectArray diff-scan in the drain).**
Design: in the same marshal drain, every N ticks iterate the object array (the same data-read
iteration ForEachUObject already performs, :2597) and fire queued callbacks for class matches
newly present since the previous scan, reusing the :7634–:7720 marshalling and
pending-callback plumbing unchanged. Cost-bounded (N tunable; scan is O(objects)). Unlocks:
ModMenu (×8: player-character/enemy/motionless tracking), MiniMap (×2), the entire
"do X when Y spawns" mod class — the single largest callback family in the real set after
keybinds. Risk: low; worst case is late delivery (next tick) and scan cost.

**5. Console-command → keybind/shared-variable shim (honest, last).**
Design: keep RegisterConsoleCommandHandler registrations accepted; expose a documented
PS5-side virtual command bus (ModRef shared variables or a small `ps5.virtual_command(name,
args_table)` port addition) so a paired companion UI or another mod can invoke handlers; the
handlers themselves already exist in registration lists (:3578/:3613). Do NOT pretend console
text input exists. Unlocks: only mods whose primary interface is a console command — rare in
the Lua set (zero uses in the four real mods), hence rank 5. Alternative: skip building it and
publish the requirement "PS5 mods must ship a keybind path."

Explicitly NOT recommended: any exec-page detour route (violates the walkguard law), networking
(no demand), file watching (no demand, no mechanism), GUI toolkit (not a Lua API even upstream).

---

## 7. Honest boundaries of this document

- Registration facts are byte-pinned to the c92c92ff snapshot and its LuaType tree; **no runtime
  behavior was executed or observed** for this report. "FULL" claims inherit the load-time +
  registration evidence of worker-menu-spawn 01-API-AUDIT and worker-luamod-l11-dryrun; the
  game-thread family is **PUMP** — in-game marshal-drain proof does not exist yet (launch-9
  loaded zero mods; launch-10 never completed; per README-RETURN §1 the console is untouched
  since launch-9).
- Per-mod API counts are static greps of the four real main.lua files; a grep cannot see dynamic
  registration (e.g., hook paths built by string concatenation — ModMenu builds some paths via
  `Runtime.hookPaths`), so DEFERRED/UNPROVEN counts are floors, not ceilings.
- TIER D feasibility (native RegisterHook firing in the R3 world) is a source-reading argument
  from the shared mechanism with the marshal carrier (:4553–:4622), not a measured result; it is
  precisely what addition #2's verification task must establish.
- The `"\\"` path-join observation in the file dumpers (:2161, :2173, :2184) is a static read of
  upstream-derived code, unverified on the PS5 filesystem layer.
