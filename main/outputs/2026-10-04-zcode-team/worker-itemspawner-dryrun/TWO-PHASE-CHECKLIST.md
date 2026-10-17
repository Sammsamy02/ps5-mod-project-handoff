# TWO-PHASE-CHECKLIST.md — Item Spawner main.lua: load-time vs first-keybind touches

worker-itemspawner-dryrun, 2026-10-04. Static analysis only; the mod was never executed.
Source: retained, hash-verified `TownfallItemSpawner/Scripts/main.lua` (intake member
`member-00000000.bin`, 21415 B, sha256 `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`,
746 lines — re-verified this session). Cites use the RUNTIME-REQUIREMENTS convention `IS:<line>`.

## Phase L — load-time touches (chunk execution at mod start, before any key)

Requirement A1 `print` | IS:6 (`log` defined as `print(MOD_TAG .. " " .. tostring(s) .. "\n")`), invoked at IS:746. **The only log call reachable in the load path.** Full inventory of log call sites: IS:189, 194, 335, 342, 501, 636, 643, 746 — all except 746 are inside functions first invoked from keybinds.

Requirement A8 `RegisterKeyBind` + `Key.*` | **Hard load-time dependency.** Six registrations: IS:694 (F7), 698 (UP_ARROW), 703 (DOWN_ARROW), 708 (RETURN), 713 (LEFT_ARROW), 722 (RIGHT_ARROW). A nil `RegisterKeyBind` global or nil `Key` field aborts the chunk at that line — before the IS:746 completion log.

Requirement A10 `RegisterLoadMapPreHook` | IS:731, wrapped as `pcall(RegisterLoadMapPreHook, fn)` — **soft** load-time dependency: a nil global is swallowed by the pcall, the reset callback simply never registers.

Pure-Lua load-time state ops (no engine contact): data tables `extraPickupClasses` IS:72–79, `legacyItems` IS:83–109; state locals IS:111–120 (`items`, `page`, `quantityIndex`, `quantities`, `PAGE_SIZE=8`, `rows`, `selectedIndex`, `status="Load a save, then open with F7."` IS:118, `ownedActors`, `busy`); `buildRows()` executed at IS:424 — constructs 11 row closures in Lua only.

Functions DEFINED but not executed at load: `isValid` IS:8, `getPlayerController` IS:14, `getPlayerPawn` IS:26, `class` IS:43, `construct` IS:49, `str` IS:57, `database` IS:63, `normalizeClassPath` IS:122, `addUnique` IS:138, `scanItems` IS:145, `getPickupComponent` IS:198, `spawnSelected` IS:215, `setTextColor` IS:361, `pageCount` IS:371, `buildRows` IS:375, `refreshText` IS:426, `setInputLock` IS:446, `addStyledText` IS:454, `buildUI` IS:488, `showMenu` IS:647, `toggleMenu` IS:670, `moveSelection` IS:674, `activateSelection` IS:684.

Explicitly NOT touched at load (lazy — first keybind only):
- `require("UEHelpers")` — IS:15, inside `getPlayerController`; `getPlayerController` is first called from `buildUI` IS:493 (F7 chain) and `spawnSelected` IS:227/230 and `setInputLock` IS:447.
- `FindFirstOf` — IS:21 (`"PlayerController"`), IS:64 (`"ItemDatabaseSubsystem"`).
- `StaticFindObject` — IS:44, 283, 286, 499.
- `StaticConstructObject` + `FName` — IS:50 (inside `construct`), IS:516 (UI root fallback).
- `ExecuteInGameThread` — IS:695, 700, 705, 710, 716, 725. First possible touch is the first F7 press (IS:695); the other five are guarded by `if not menuOpen then return end` (IS:699, 704, 709, 714, 723) so they return BEFORE `ExecuteInGameThread` while the menu is closed.
- `LoadAsset` — IS:285 (spawn path only). `FText` — IS:434, 469 (UI text only).

Settings keys the mod's path depends on (the Lua reads none itself; these are runtime inputs):
- `DefaultExecuteInGameThreadMethod=EngineTick` — the delivery route for every callback (A9; RUNTIME-REQUIREMENTS E; BUNDLE-SPEC 1.1).
- `DefaultFNameToStringMethod=Scan` — backs `ToString()` at IS:59 (B8).
- `enabled.txt` presence marker enables the mod at scan time (BUNDLE-SPEC 1.3; NativeModSource.cpp:134-135) — bundle-level, not a settings key.
- `ConsoleEnabled=1` — visibility of output on the console-side channel (BUNDLE-SPEC 1.1).

Phase-L checklist (all must hold for a clean load):
1. Chunk loads through the real luamod path; no syntax/load errors.
2. `RegisterKeyBind` × 6 succeed (registry evidence: baseline trial registered 6/6).
3. `pcall(RegisterLoadMapPreHook, ...)` returns without surfacing (ok either way).
4. Exactly one `print` emitted: the IS:746 completion line (see OBSERVABLES.md for byte-exact text).
5. No object lookups, no UI, no spawn, no game-thread work occurred.

## Phase K — first-keybind touches (F7 press, full chain)

K0. Keybind dispatch → callback IS:694–696 → **`ExecuteInGameThread(toggleMenu)` IS:695** — requirement A9, the single hard gate; the retained baseline trial failed exactly here (`attempt to call a nil value (global 'ExecuteInGameThread')`, RUNTIME-REQUIREMENTS baseline facts).

K1. `toggleMenu` IS:670 → `showMenu(true)` IS:647–657.

K2. `#items == 0` → `scanItems()` IS:651 → `database()` IS:147 → **`FindFirstOf("ItemDatabaseSubsystem")` IS:64** (A3; needs a loaded save per IS:66 and C5). Failure is pcall-captured IS:146/192 → status IS:193, log `"Scan failed: " .. err` IS:194.

K3. `buildUI()` IS:652 → reuse check IS:489 → **`getPlayerController()` IS:493** → `require("UEHelpers")` IS:15–18 (A2; pcall-guarded — a failed require silently degrades to the fallback) → **`FindFirstOf("PlayerController")` IS:21** (A3). No PC → `status = "Load a save first."` IS:495, silent `return false` (no log call on this path).

K4. **`StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")` IS:499** (A4). Missing → log `"WidgetBlueprintLibrary not found."` IS:501, `return false`.

K5. UMG family (all missing-new per RUNTIME-REQUIREMENTS D1–D5): `class("/Script/UMG.UserWidget")` IS:506 → `widgetLib:Create(pc, class, pc)` IS:510 (D1) → `StaticConstructObject` + `FName("TownfallItemSpawnerRoot")` fallback IS:516 (A5/A6) → `construct` of WidgetTree/CanvasPanel/Border/VerticalBox IS:524–560 (D2; each `construct` hits `StaticFindObject` + `StaticConstructObject` + `FName` via IS:44/50) → widget-tree property writes IS:525, 528 (B3) → `AddChild`/slot setters IS:531, 534–541, 544, 547, 551, 554–558, 561 and `addStyledText` setters IS:456–483 (D3) → `refreshText()` IS:630 with `SetText(FText(...))` IS:434 and `SetColorAndOpacity` IS:364–367 (A6 FText, D3) → **`AddToViewport(9999)` IS:631, `SetVisibility(1)` IS:632** (D4) → log `"Menu created."` IS:643. Any failure inside → caught IS:635 → log `"UI creation failed: " .. err` IS:636, ui table nulled IS:637–639, `return false`.

K6. `showMenu` tail: `refreshText()` IS:654, `SetVisibility(0)` IS:655, `setInputLock(true)` IS:656 → `SetIgnoreLookInput/SetIgnoreMoveInput` IS:450–451 (C4); `menuOpen = true` IS:657.

K7. Subsequent keys (inert until `menuOpen`, guards IS:699/704/709/714/723): `moveSelection` IS:674–682, `activateSelection` IS:684–692 (RETURN on a pickup row → `spawnSelected` IS:215), paging IS:716–719, 725–728 — each body runs inside `ExecuteInGameThread`. Spawn payload chain: `K2_GetActorLocation` IS:233, `GetControlRotation().Yaw` IS:234, `Default__KismetSystemLibrary` IS:241, `LineTraceSingle` with out-param `hit` IS:245–258 (B5), `hit.ImpactPoint/.ImpactNormal` reads IS:264–269, transform table IS:273–277, `StaticFindObject`/`LoadAsset` retry IS:283–286 (A7), `Default__GameplayStatics` IS:294, `BeginDeferredActorSpawnFromClass` IS:295–297 + `FinishSpawningActor` IS:303 (C2), pickup component IS:198–213/306, ID read IS:313 (B2/B8), property writes IS:325–326 (B3), `SetEnabled` IS:327, `SetActorHiddenInGame`/`SetActorEnableCollision` IS:329–330 (C3), ownership record IS:332; failure cleanup `K2_DestroyActor` IS:345.

Phase-K checklist (in order, each gated on the previous):
1. IS:695 `ExecuteInGameThread` callable and delivers on the game thread (A9).
2. IS:64 `FindFirstOf("ItemDatabaseSubsystem")` returns a live instance after save load (A3/C5) — without a save this phase is DESIGNED to stop gracefully with the IS:194 log line.
3. IS:15–21 player-controller acquisition (A2 require chain + A3).
4. IS:499/506+ UMG family (D1–D5) — expected first failure point once 1–3 work.
5. IS:245+ trace/spawn family (B5/C1/C2/A7/C3) — payload, last rung.
6. Physical key delivery itself (A8 real device input) — prerequisite for any of K0–K5 being observed at all.
