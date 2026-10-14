# ACTIVATION-PATH.md — worker-itemspawner-lua-audit

2026-10-04. Startup sequence of the unchanged TownfallItemSpawner under the composed runtime, the exact UI mechanism, and why the user sees nothing.

## A. Runtime startup (before any mod code)

1. **Activation consumption** — `PrepareActivatedUE4SSRuntime` (`outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.cpp:84-123`) wraps the activation; `MakeActivatedUE4SSModSource` (`bootstrap/activated_mod_source.cpp:20-84`) accepts only `ue4ss/`-prefixed `lua_mod`/`config`/`script` files (line 38-59). This bundle is Lua-only (no `native_plugin`, so activation root not required, `activated_mod_source.cpp:62-67` — bundle.json note).
2. **Deferred policy** — `mod_startup_policy = Deferred` (`ue4ss_backend.cpp:27`); Lua mods start only after `on_engine_initialized`.
3. **Layout gate (today's hard stop)** — `UE4SSProgram::load_unreal_offsets_from_file()` on `__PROSPERO__` reads `ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini` from the mod source and **throws "Native activation requires both member and vtable layout files" when either is absent** (`PORT/src/UE4SSProgram.cpp:896-906`). The bundle's two layout keys are `awaiting-staged-pair` (bundle.json) and worker-staged-pair-emission REPORT states nothing promoted. Until they land, a console run ends inside `setup_unreal()` **before any Lua exists** — not even the mod's Loaded line.
4. **Pad input** — `ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source)` (`ue4ss_backend.cpp:26`) reads `ue4ss/PS5Pad.ini` through the mod source (NativePadInput.cpp:13, per worker-real-input-pad comment in `worker-final-staging-rehearsal/rehearsal-root/ue4ss/PS5Pad.ini`). **The itemspawner bundle does NOT stage PS5Pad.ini** (bundle.json lists exactly 4 files + 2 awaiting layout keys), so as staged there are no pad→key bindings: OPTIONS keeps its normal game meaning and no F7 can ever be produced for this mod.
5. **Engine bootstrap** — `UnrealInitializer::PreInitialize` owner-identity refusals, `ScanGame()` via owned ps_scan binding, engine 5.6 axes, FName constructor verify, bounded object-count waits (worker-3 INTEGRATION-MAP §1 Stage 4). On success: `fire_unreal_init_for_cpp_mods` → `setup_unreal_properties` → `share_lua_functions` → `on_engine_initialized` (`PORT/src/UE4SSProgram.cpp:652-661`), then deferred Lua startup: mods constructed from `mod_source->lua_mods()` (`PORT/src/UE4SSProgram.cpp:1742-1748`).
6. **Lua environment** — each LuaMod state receives the full API surface: `Key`/`ModifierKey` (`PORT/src/Mod/LuaMod.cpp:614,626`), `EFindName` (`:698-702`), `UnrealVersion` (`:6184`), `FName` (`:6191`), `NAME_None` (`:6193`), `FText` (`:6200`), `ModRef` (`:6306-6307`), and every function registered in LuaMod.cpp (matrix citations).

## B. Mod startup sequence (main.lua top level — what runs on load)

All of this is pure Lua + registration; it makes **zero engine calls** (dryrun TWO-PHASE-CHECKLIST agrees):

1. `buildRows()` (IS:424) — static menu row table; no engine contact.
2. Six `RegisterKeyBind` registrations (IS:694-729): `Key.F7`, `Key.UP_ARROW`, `Key.DOWN_ARROW`, `Key.RETURN`, `Key.LEFT_ARROW`, `Key.RIGHT_ARROW`. Each callback body is wrapped in `ExecuteInGameThread(...)` (IS:695,700,705,710,716,725). Non-F7 callbacks early-return while `menuOpen == false`.
3. `pcall(RegisterLoadMapPreHook, ...)` (IS:731-744) — pcall-guarded; silently declines if the hook is absent; resets menu/items/ownedActors on map change.
4. `log("Loaded v1.0...")` (IS:746 via IS:6 `print`).

**There are NO polling loops.** `FindFirstOf` is never called at load; every object query is on-demand inside a keybind callback. The mod idles at zero cost until a key arrives.

## C. First keypress → visible UI

1. Pad chord (per rehearsal profile: OPTIONS) → `PS5PadSource::poll` → `InputEvent{key=118 /*F7*/}` → upstream `Input::Handler` → LuaMod dispatches the F7 callback.
2. Callback calls `ExecuteInGameThread(toggleMenu)` (IS:695) → queues an EngineTick action (`PORT/src/Mod/LuaMod.cpp:4690-4760`, queue drained `:4478-4489`) → runs `toggleMenu` on the real game thread on a later tick.
3. `showMenu(true)` (IS:647-668): `scanItems()` if item list empty (IS:651) → `FindFirstOf("ItemDatabaseSubsystem")` (IS:64); pcall-guarded, failure logs `Scan failed: ...` and keeps going. Then `buildUI()` (IS:488-645).
4. `buildUI()`: `FindFirstOf`-based PlayerController (IS:14-24, with optional `require("UEHelpers")` at IS:15), `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")` (IS:499), `WidgetBlueprintLibrary:Create(pc, UserWidget, pc)` (IS:510) with `StaticConstructObject` fallback (IS:516), constructs WidgetTree/CanvasPanel/Border/VerticalBox (IS:524-561) and TextBlock rows (IS:455 via `addStyledText`), then `AddToViewport(9999)` (IS:631) and `SetVisibility(0)` (IS:655).
5. Menu navigation: UP/DOWN move selection (IS:698-706), RETURN activates (IS:708-711) — first row is "Quantity", so open+Enter changes quantity, it does NOT spawn (IS:378-396); LEFT/RIGHT page (IS:713-729). While open: `SetIgnoreLookInput/SetIgnoreMoveInput(true)` (IS:446-452).
6. Spawn path (RETURN on a pickup row): pawn location + control rotation (IS:233-234) → `KismetSystemLibrary:LineTraceSingle` with out-param `hit` (IS:245-258) → `StaticFindObject`/`LoadAsset` class resolve (IS:283-291) → `GameplayStatics:BeginDeferredActorSpawnFromClass` + `FinishSpawningActor` (IS:295-303) → configure `PickupInteractable` component (IS:198-213, 305-330) → failure cleanup via `K2_DestroyActor` (IS:340-346).

## D. What the player SEES, and why they currently see nothing

- The "popup" is the self-made UMG UserWidget (dark red/black 570x500 frame, top-right, red selected row). Nothing else: no GUI module, no console window, no notification, no cooked asset.
- **Baseline-observed failure**: in the only measured composition, first F7 died at IS:695 with `attempt to call a nil value (global 'ExecuteInGameThread')`; the top level had succeeded, printed the Loaded line, and registered 6/6 binds (`.superpowers/native-dependency-spike/item-spawner-runtime-trial-1/findings.md`; RUNTIME-REQUIREMENTS baseline). `toggleMenu` never ran → `buildUI` never ran → no widget was ever created. Log-only, screen unchanged.
- **"Options menu behaving normally" is the expected symptom, not a contradiction**: the mod never touches the game's menus. Its UI would be a separate viewport widget. And because the staged bundle lacks `ue4ss/PS5Pad.ini` (and the pad provider has no hardware run), pressing OPTIONS reaches the game's own menu exactly as vanilla — matching the user's report precisely.
- **Today's console reality is even earlier**: with the layout pair still `awaiting-staged-pair`, step A.3 throws before Lua, so on a real run not even the Loaded line appears.

## E. Minimal activation subset (mod stays unmodified; runtime must provide)

| Tier | What runs | Runtime prerequisites |
|---|---|---|
| **T0 — reach Lua at all** | nothing yet | staged `MemberVariableLayout.ini` + `VTableLayout.ini` (worker-2), `PublishStagedNativeLayoutMaps` wired (INTEGRATION-MAP S2 gap: no production caller), worker-1 live module facts |
| **T1 — harmless load (acceptance-rung NOW-adjacent)** | full top-level chunk unmodified: 6 registrations + pcall-guarded pre-hook + one Loaded log; zero engine effect until keypress | none beyond T0 (port `RegisterKeyBind`/`Key`/`print` already present); host-proven behavior |
| **T2 — F7 becomes interactive, still log-only** | toggleMenu executes on game thread; scanItems fails gracefully ("Scan failed: Item database not ready"); buildUI fails gracefully ("Load a save first.") | `ExecuteInGameThread` EngineTick route live against genuine `UGameEngine::Tick` identity (code present, unproven); pad delivery: add `ue4ss/PS5Pad.ini` to the activation bundle + pad provider hardware path |
| **T3 — menu VISIBLE** | UMG widget renders, navigable, quantity/page refresh works for the 6 hardcoded cooked classes | live `FindFirstOf`/`StaticFindObject` (GUObjectArray + NamePool), `StaticConstructObject`, FName/FText, full property/UFunction reflection (B2-B7), UMG module classes resident (game's own UI implies they exist) |
| **T4 — actually spawns items** | complete spawner | T3 + game UFunctions (IS:163,168), `LineTraceSingle` out-param marshalling (B5 — first predicted break), deferred spawn callables, property writes, FName→string (B8) |
| optional | `LoadAsset` fallback (IS:285) only for non-resident classes; `RegisterLoadMapPreHook` reset behavior (IS:731) | A7/missing-new, A10 — neither blocks T1-T4 |

**Harmless-now set (unmodified)**: T1 only. Everything the player can see or interact with requires T2-T4; there is no partial UI the mod could show without full UMG reflection, because it builds its UI purely through reflected calls.
