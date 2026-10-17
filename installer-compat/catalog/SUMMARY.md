# UE4SS Lua API — Global Function Catalog

**Scope:** machine-readable catalog of every registered UE4SS Lua **global function**, built
Mac-only / host-tested (static source analysis only — no FTP, no payloads, no launches, no
console contact, nothing outside the module's own directory written; the authoring-time
original of this module is preserved at `catalog/src-catalog-globals/`).

**Output:** `globals.json` (array of objects, sorted by category then name; fields:
`name`, `signature`, `category`, `registration_sites`, `ue4ss_version`, `pc_only_behavior`,
`ps5_runtime_status` (always `""` — filled in later), `needs_live_game`, `notes`).

## Primary source

PS5/Prospero port of UE4SS at
`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/` under
`/WORKSPACE/.codex/worktrees/translation-core/what/`:

- `src/Mod/LuaMod.cpp` (7781 lines) — main registration file. In this codebase the
  upstream `RegisterLuaFunction`-style macros appear as `LuaMadeSimple::Lua::register_function()`
  (which is `lua_setglobal` of a C closure underneath, `first/src/LuaMadeSimple/LuaMadeSimple.cpp:701`)
  plus direct `lua_setglobal` / `Table::make_global` calls.
- `src/Signatures.cpp` — separate, temporary **signature-scan Lua state** globals.
- `src/LuaLibrary.cpp` / `src/LuaLibraryWindows.cpp` — binding bodies for `print`,
  `LoadExport`, `DerefToInt32`.
- `src/UE4SSProgram.cpp`, `src/SettingsManager.cpp`, `src/LuaType/*` — checked: **no**
  additional Lua global-function registrations (LuaType files only contain commented-out
  `make_global` lines; SettingsManager exposes nothing to Lua).

Cross-check baseline: vendored upstream copy at
`.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/`
with `generated_src/version.cache` = **3.0.1.0.0** (UE4SS 3.0.1). The port's
`register_function` name set is **identical** to that vendored 3.0.1 tree (verified by diff);
the only global-function surface the port adds is the `loadfile`/`dofile` native-source
override (see Site D).

## Registration sites

| Site | Where | Which Lua state gets it |
|---|---|---|
| A | `LuaMod.cpp:1519` `setup_lua_global_functions_internal` | Every true Lua-mod state (`:6301`) **and** the GUI console-executor state (`:6725`) |
| A-gated | same function, `is_true_mod == Yes` block (`:1733-2198`) | True Lua mods only — keybinds + dump/generate functions (NOT the console executor) |
| B | `LuaMod.cpp:4563` `setup_lua_global_functions_main_state_only` | Main Lua state of true mods only (`start_mod`, `:6448`) — RegisterHook + game-thread/delayed-action/mod-lifecycle family |
| C | `LuaMod.cpp:6101` `setup_lua_classes_internal` | Every true mod (`:6302`) and console executor (`:6726`) — string constructors + global class tables |
| D | `LuaMod.cpp:1181` `setup_custom_module_loader` | Only mods with a **native mod source** configured (PS5/port path) — replaces stdlib `loadfile`/`dofile` |
| E | `Signatures.cpp:62-67` `scan_from_lua_script` | Temporary sandbox state for `UE4SS_Signatures/*.lua` AOB scan scripts only (never mod `main.lua`) |

Commented-out registrations (NOT live): `RegisterHook`/`NotifyOnNewObject` stubs inside the
`/* ... */` blocks at `LuaMod.cpp:1021-1030`, `1047-1056`, `1073-1082`.

Multi-site registrations: `print` (Sites A + E, plus alias `Print` in E), `LoadExport`
(Sites A + E, plus alias `loadexport` in E), `DerefToInt32`/`dereftoint32` (E only).

## Counts per category (98 entries total)

**UPDATE 2026-10-07 (v4):** the live `catalog/globals.json` now holds **106**
entries (98 here + 6 Rev-2 conformance additions in v3 + the 2 v4 `Key` /
`ModifierKey` TYPE entries, which are constant tables rather than functions —
`input` therefore counts 3 functions + 2 type entries on disk; see
`docs/COUNTS.md` for the authoritative recount). The tables and list below
describe the original 98-function build and are kept for provenance.

| Category | Count |
|---|---|
| object-management | 9 |
| reflection | 1 |
| math | 0 |
| string | 3 |
| file-io | 4 |
| input | 3 |
| hooking | 21 |
| threading | 29 |
| debug | 20 |
| settings | 0 |
| other | 8 |

No math or settings globals are registered (SettingsManager is C++-side only; no math
helpers exist in this API).

## Complete alphabetical function list (98)

CancelDelayedAction, ClearAllDelayedActions, CreateInvalidObject, CreateLogicModsDirectory,
DerefToInt32, dereftoint32, dofile, DumpAllActors, DumpAllObjects, DumpJMAP, DumpStaticMeshes,
DumpUSMAP, ExecuteAsync, ExecuteInGameThread, ExecuteInGameThreadAfterFrames,
ExecuteInGameThreadWithDelay, ExecuteWithDelay, FAnsiString, FindAllOf, FindFirstOf, FindObject,
FindObjects, ForEachUObject, FPackageName.IsShortPackageName, FPackageName.IsValidLongPackageName,
FString, FUtf8String, GenerateLuaTypes, GenerateSDK, GenerateUHTCompatibleHeaders, GetAsyncThreadId,
GetCurrentThreadId, GetDelayedActionRate, GetDelayedActionTimeElapsed, GetDelayedActionTimeRemaining,
GetGameThreadId, GetMainModThreadId, IsDelayedActionActive, IsDelayedActionPaused, IsInAsyncThread,
IsInGameThread, IsInMainModThread, IsKeyBindRegistered, IsValidDelayedActionHandle,
IterateGameDirectories, LoadAsset, LoadExport, loadexport, loadfile, LoopAsync,
LoopInGameThreadAfterFrames, LoopInGameThreadWithDelay, MakeActionHandle, NotifyOnNewObject,
PauseDelayedAction, print, Print, RegisterBeginPlayPostHook, RegisterBeginPlayPreHook,
RegisterCallFunctionByNameWithArgumentsPostHook, RegisterCallFunctionByNameWithArgumentsPreHook,
RegisterConsoleCommandGlobalHandler, RegisterConsoleCommandHandler, RegisterCustomEvent,
RegisterCustomProperty, RegisterEndPlayPostHook, RegisterEndPlayPreHook, RegisterHook,
RegisterInitGameStatePostHook, RegisterInitGameStatePreHook, RegisterKeyBind, RegisterKeyBindAsync,
RegisterLoadMapPostHook, RegisterLoadMapPreHook, RegisterProcessConsoleExecPostHook,
RegisterProcessConsoleExecPreHook, RegisterULocalPlayerExecPostHook, RegisterULocalPlayerExecPreHook,
ResetDelayedActionTimer, RestartCurrentMod, RestartMod, RetriggerableExecuteInGameThreadWithDelay,
SetDelayedActionTimer, StaticConstructObject, StaticFindObject, UE4SS.GetVersion,
UninstallCurrentMod, UninstallMod, UnpauseDelayedAction, UnrealVersion.GetMajor,
UnrealVersion.GetMinor, UnrealVersion.IsAbove, UnrealVersion.IsAtLeast, UnrealVersion.IsAtMost,
UnrealVersion.IsBelow, UnrealVersion.IsEqual, UnregisterCustomEvent, UnregisterHook

(Class-table methods are listed with their qualified names: `UE4SS.GetVersion`,
`UnrealVersion.*` (7), `FPackageName.*` (2). Bare-table globals such as `UE4SS`,
`UnrealVersion`, `FPackageName`, `Key`, `EObjectFlags` are non-function values and are
covered in the section below.)

(`Register` and `OnMatchFound` are NOT registered by C++ — they are globals the
signature-scan scripts must *define* themselves, `Signatures.cpp:112-115`.)

## Non-function globals observed (context only — not in globals.json)

`ModRef` (LuaModRef userdata, `:6307`), `AsyncThread`/`MainThread` (Lua threads, `:1042/:1068`),
`EGameThreadMethod` (enum table, `:4677`), `EngineTickAvailable`/`ProcessEventAvailable`
(booleans, `:4685/:4687`), `FName` + `NAME_None` (preconstructed userdata, `:6191/:6193`),
`FText` (userdata table, `:6200`), `__OriginalReturnValue` (nil, `:6317`), `Key` /
`ModifierKey` (Windows VK tables, `:614/:626`), `EObjectFlags` / `EInternalObjectFlags` /
`EFindName` (enum tables, `:672/:692/:702`), `PropertyTypes` (property-class tables, `:1122`),
`ue4ss_loaded_modules` (table, `:1131`), `OutputDeviceRef` (light userdata set from
`LuaLibrary::set_outputdevice_ref`, `src/LuaLibrary.cpp:32`). Dynamic UEnum tables are also
exported as globals lazily when first encountered (`src/LuaType/LuaUObject.cpp:1632`).

## Key PC-only findings (detail per function in globals.json)

1. **Input family** (`RegisterKeyBind`, `RegisterKeyBindAsync`, `IsKeyBindRegistered`) —
   Windows VK key codes; on this Prospero port `UE4SSProgram::register_keydown_event` /
   `is_keydown_event_registered` throw `"Native input service is unavailable"` unless an
   injected platform input source is active (`UE4SSProgram.cpp:2882-2925`).
2. **Dump/generate family** (`DumpAllObjects`, `GenerateSDK`, `GenerateLuaTypes`,
   `GenerateUHTCompatibleHeaders`, `DumpStaticMeshes`, `DumpAllActors`, `DumpUSMAP`,
   `DumpJMAP`) — write files **outside the mod dir**; several build paths with literal
   Windows backslash separators (`STR("\\")`, `LuaMod.cpp:2131/2143/2154`).
3. **Filesystem family** (`IterateGameDirectories`, `CreateLogicModsDirectory`) —
   `std::filesystem` over the game install tree with a hardcoded
   `<RootGamePath>/Game/Binaries/Win64` layout assumption (`LuaMod.cpp:2943`);
   `CreateLogicModsDirectory` writes `<Content>/Paks/LogicMods`; the `IterateGameDirectories`
   `__files` accessor can also create the LogicMods directory as a fallback.
4. **`LoadExport`** — Win32 `GetProcAddress` across loaded modules on PC; native
   `ModuleSource->FindExport` on the port (`port/unreal/src/UnrealInitializer.cpp:200-228`).
5. **`DerefToInt32`/`dereftoint32`** — Windows-only implementation
   (`LuaLibraryWindows.cpp` fully guarded by `#ifdef _WIN32`, pointer validated against
   `GetCurrentProcess`); no non-Windows definition exists in this port snapshot.
6. **`print`/`Print`** — UE4SS console/GUI output on PC; optionally mirrors into the game's
   `FOutputDevice` via the `OutputDeviceRef` global.
7. **`loadfile`/`dofile`** — PS5 port addition: replaced with native-source-backed closures
   so scripts never touch the target disk; PC builds keep the stdlib versions.

## Version attribution honesty

`version.cache` (3.0.1.0.0) and the local trees only prove presence in the vendored
**UE4SS 3.0.1** base. The `ue4ss_version` field therefore distinguishes:
- "part of the long-documented UE4SS Lua API (1.x/2.x era)" for classic globals, and
- "3.x-era API (not verified against older local sources)" for the delayed-action /
  mod-lifecycle / string-constructor family,
- "PS5 port addition" for `loadfile`/`dofile`.
All 98 entries were verified registered in the port's own source (line numbers cited in
`registration_sites`).

## Reproduce

`python3 catalog/src-catalog-globals/build_catalog.py` (regenerates `globals.json`;
all facts hard-coded from the cited source lines, no console or network access).
Note: the frozen original script still writes to its original absolute staging
output path; update its output `open()` to the project-relative
`catalog/globals.json` before running it outside that original layout.
