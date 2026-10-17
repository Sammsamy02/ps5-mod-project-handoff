# SMOKE-RESULTS.md — worker-lua-mod-smoke

2026-10-04. Verdict: **SMOKE PASS — 90/90 checks, 0 failures.** No predicted console
failure at mod LOAD. One conditional F7-press gate identified (EngineTick availability),
plus engine-side data risks that are outside Lua-level scope (see REPORT.md).

## Environment

- VM: **Lua 5.4.7 compiled from the port's own dependency tree**
  `ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw`
  (the exact sources the port embeds), plus `lua_lock_shim.c` (pthread stand-in for the
  port's Windows-only `luauser.c` lock hooks, `luaconf.h:796-800`). No port source modified.
- Host: `harness/ue4ss_host.lua` registers **exactly** the globals the port registers
  (82 functions from `register_function` in port `src/Mod/LuaMod.cpp` + tables
  `Key` [165 entries, values extracted from `KeyDef.hpp`], `ModifierKey`, `EObjectFlags`,
  `EInternalObjectFlags`, `EFindName`, `PropertyTypes`, `UE4SS`, `UnrealVersion`, `FName`,
  `NAME_None`, `FText`, `FString/FUtf8String/FAnsiString`, `FPackageName`, `ModRef`,
  `EGameThreadMethod`, `EngineTickAvailable`, `ProcessEventAvailable`, `ue4ss_loaded_modules`,
  `__OriginalReturnValue`) with port-matching return shapes (lookups return invalid-object
  wrappers, never nil, except `FindAllOf` → nil; keybind/hook/queue functions return zero values).
- Mod bytes under test: byte-identical staged copies (sha256 `f58604…9658` main.lua,
  `5a8f66…15a7` UEHelpers.lua — match worker-itemspawner-lua-audit LUA-API-MATRIX.md).
- Chunk name mirrors the port (`@Mods/TownfallItemSpawner/Scripts/main.lua`,
  LuaMod.cpp:784); require paths mirror `setup_lua_require_paths`
  (`;Mods/shared/?.lua;Mods/shared/?/?.lua`, LuaMod.cpp:1387-1392).

## Phase results (all PASS)

| Phase | Checks | Result | Key evidence |
|---|---|---|---|
| 0 env | 11 | PASS | Lua 5.4 VM; Key table 165/165 entries; F7/RETURN/4 arrows present; capability globals present; port shared require patterns in package.path |
| 1 load | 25 | PASS | main.lua loads with **zero errors**; **exactly 6 keybinds**: F7(0x76), UP_ARROW(0x26), DOWN_ARROW(0x28), RETURN(0x0D), LEFT_ARROW(0x25), RIGHT_ARROW(0x27), no modifier chords; RegisterLoadMapPreHook accepted (pcall did not decline); banner printed |
| 2 f7-nosave | 10 | PASS | F7 → 1 ExecuteInGameThread queued → drained: **"Item database not ready. Load a save first."** guard path, "Scan failed:" logged, menu NOT created, no UMG lookups, no errors |
| 2b f7-gate | 4 | PASS | With EngineTick hook unavailable: ExecuteInGameThread throws the port's exact error, **caught by keybind dispatch**, nothing queued, menu can never open |
| 3 f7-save | 35 | PASS | Save-loaded world: 7 pickups resolved; "Menu created."; full UMG tree built (WidgetBlueprintLibrary:Create → WidgetTree/CanvasPanel/Border×2/VerticalBox/12×TextBlock); AddToViewport(9999); SetVisibility 1→0; DOWN×3/UP/RIGHT nav clean; **CROSS spawn path end-to-end** (LineTraceSingle out-params → BeginDeferredActorSpawnFromClass → FinishSpawningActor → PickupAmount/bAutoDestroyOnPickUp writes → SetEnabled/SetActorHiddenInGame/SetActorEnableCollision → "Spawned Antibiotics x1 ID=DeprecatedItem"); load-map hook + clean reopen |
| 4 gt-gate | 5 | PASS | LoadAsset throws off game thread (port LuaMod.cpp:3626) and does not throw inside drained game-thread closure |

Captured output: `harness/PHASE-LOGS/all-phases.txt` (full 90-check run),
`load-dump.txt`, `f7-nosave-dump.txt`, `f7-gate-dump.txt`, `f7-save-dump.txt` (941
recorded calls), `gt-gate-dump.txt`. Reproduce: `sh harness/run_all.sh`.

Sample captured mod output (f7-nosave):
```
[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.
[Lua] [TownfallItemSpawner] Scan failed: Mods/TownfallItemSpawner/Scripts/main.lua:66: Item database not ready. Load a save first.
```
Gate variant (f7-gate), caught by the port's keybind dispatch lambda (LuaMod.cpp:1944-1947):
```
ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)
```

## Checklist items requested by the task

- **Syntax errors**: none (loads clean under Lua 5.4.7).
- **Nil-index at load**: none — strict pre-check confirmed every global main.lua touches
  at load exists in the port's registered set; load executed with zero errors.
- **Require-path mismatch**: none — `Mods/shared/UEHelpers/UEHelpers.lua` resolves via the
  port's second shared pattern `Mods/shared/?/?.lua`. Note: main.lua defers
  `require("UEHelpers")` to the first `getPlayerController()` call (main.lua:15), i.e.
  first F7 press, not mod load; at that point UEHelpers executes its two module-scope
  `CreateInvalidObject()` calls cleanly (verified in f7 phases).
- **Keybind registrations**: exactly 6, exact keys above, matching PS5Pad.ini chords
  OPTIONS→F7, UP/DOWN/LEFT/RIGHT→arrows, CROSS→RETURN (staged PS5Pad.ini [Bindings]).
- **pcall-guarded map hook**: `RegisterLoadMapPreHook` is registered by the port
  (LuaMod.cpp:2711), accepted the callback, and fired without error on simulated map change.
- **Print output**: captured per phase (see above).
- **F7 no-save**: does NOT error; mod's own "Load a save first." status path taken; menu
  never opens; second F7 also safe.
- **F7 save-loaded**: buildUI's UMG calls (Create/StaticConstructObject/AddChild/
  SetPadding/SetAnchors/SetAlignment/SetPosition/SetSize/SetContent/SetText/
  SetColorAndOpacity/SetAutoWrapText/SetShadowColorAndOpacity/Font.Size/ShadowOffset/
  AddToViewport/SetVisibility) all reach stubs without Lua errors; full spawn flow clean.
