# FAILURE-TAXONOMY — every error path the unchanged mod can take, diagnosable from UE4SS.log alone

Worker: worker-post-popup-evidence · 2026-10-05. Pins are into the canonical main.lua
(`worker-settings-v3-package/package-v3/.../TownfallItemSpawner/Scripts/main.lua`, MD5 `4f15f2c79955c636eeaa38d0bf49433d`;
identical in all staged trees). Every spawn-time failure renders as
`[Lua] [TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>` (main.lua:341-342); the table
gives `<stage>` and each `<err>` payload verbatim. All failures are pcall-caught: **the mod never
crashes the game and never dies itself** — `busy` resets (main.lua:338), an invalid partial actor is
destroyed via `K2_DestroyActor` (main.lua:344-346), and the menu stays usable.

## A. Load-time (pre-OPTIONS; completeness — owned by READINESS/API-GAP)

| Symptom in UE4SS.log | Meaning | Source |
|---|---|---|
| `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` | SUCCESS marker (not a failure) | main.lua:746 |
| `Error loading script: <lua parse error>` | main.lua byte-corrupted | LuaMod.cpp:6423 |
| `Error executing script: <msg>` | runtime error at load (nil global etc.) or a load-time `error()` | LuaMod.cpp:6434 |
| `Failed to execute main script: <path>` | chunk/pcall wrapper failure | LuaMod.cpp:6475 |
| `Main script 'main.lua' not found in scripts directory: …` | staging broken | LuaMod.cpp (playbook F9) |
| `Mod is absent from native Lua source: <path>` | loadlist/discovery key drift | LuaMod.cpp:911-912 |
| `Exception in load_and_execute_script: <what>` | C++ exception during load | LuaMod.cpp:6445 |
| `Error executing script: … no native Lua file 'ue4ss/Mods/shared/…'` | UEHelpers require unresolvable (5 candidate keys listed) | LuaMod.cpp:876-892 |
| `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)` | EngineTick hook unavailable — **fires at the FIRST OPTIONS press in this mod** (main.lua:695 is the mod's first call), not at load | LuaMod.cpp:4729 (verified in composed source) |

None of these remove `mods-started` (ORACLE G3): the session lives; only the mod is dead.

## B. Menu-open / scan failures (OPTIONS pressed; rungs 1-3)

| Exact log line | Trigger | Pin | Diagnosis |
|---|---|---|---|
| `[Lua] [TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.` | `FindFirstOf("ItemDatabaseSubsystem")` invalid — main menu / pre-save | main.lua:194 + :66 | expected pre-save; press OPTIONS again in a loaded save |
| `[Lua] [TownfallItemSpawner] Scan failed: <other>` | a DB getter threw (`GetItemIDFromDeprecatedEnum` :163 or `GetInteractablePickupIDActorPath` :168) or DB object resolved but reflected call failed | main.lua:194 | reflection-level DB call problem; `<other>` names the Lua error |
| `[Lua] [TownfallItemSpawner] WidgetBlueprintLibrary not found.` | `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")` invalid | main.lua:501 | UMG CDO missing; menu never builds; press again after full engine init |
| `[Lua] [TownfallItemSpawner] UI creation failed: Could not find class: /Script/UMG.UserWidget` | `class()` helper threw inside buildUI | main.lua:636 + :45 (via :506) | UMG UClass not resolvable |
| `[Lua] [TownfallItemSpawner] UI creation failed: Unable to create root UserWidget.` | `Create` failed AND `StaticConstructObject` fallback invalid | main.lua:636 + :520 | UserWidget creation route dead |
| `[Lua] [TownfallItemSpawner] UI creation failed: Could not construct: /Script/UMG.<T> as TownfallItemSpawner<Tree/Canvas/Frame/Inner/VBox/Section<N>/Row<N>>` | widget-tree construction threw (TextBlock/WidgetTree/CanvasPanel/Border/VerticalBox) | main.lua:636 + :52 (via :524,527,530,550,560,455) | partial UI torn down (:637-639); next OPTIONS retries the build |
| `[Lua] [TownfallItemSpawner] UI creation failed: <reflected-setter error>` | any UMG setter/FText call threw mid-build | main.lua:636 | names the exact failing reflected call |
| (SILENT — no log line, no menu) | PlayerController invalid in buildUI | main.lua:493-497 | **the only silent menu failure**: status row never even renders (menu absent). Distinguish from drain stall via POST-POPUP.md §3 (a drain stall also silences the scan line; a PC miss still logs the scan line) |

## C. Spawn failures by stage (CROSS on a real item row; rung 4)

Stage prefix is exact (main.lua:224/240/279/293/305). Log shape:
`[Lua] [TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>`

### C1. stage `player`

| `<err>` payload | Trigger | Pin |
|---|---|---|
| `Load a save first.` | pawn invalid (PC.Pawn/GetPawn/K2_GetPawn all nil/invalid) | main.lua:228 |
| `Player controller unavailable.` | PC invalid after UEHelpers + FindFirstOf fallback | main.lua:231 |
| `Could not find class: /Script/Engine.Default__KismetSystemLibrary` — surfaces under stage `ground trace` | Kismet CDO query failed | main.lua:45 via :241 |

### C2. stage `ground trace`

| `<err>` payload | Trigger | Pin |
|---|---|---|
| `No ground ahead. Move to an open, flat area.` | LineTraceSingle returned no hit (facing a pit/void or trace blocked) | main.lua:261 |
| `Ground trace returned no position.` | hit struct missing ImpactPoint AND Location | main.lua:266 |
| `Surface too steep.` | hit.ImpactNormal.Z < 0.5 (wall/slope) | main.lua:270 |
| `Could not find class: /Script/Engine.Default__KismetSystemLibrary` | Kismet CDO query failed | main.lua:45 via :241 |
| `<Lua error from LineTraceSingle call / hit-out-param read>` | **historically highest-risk path (API-GAP.md §4)**: out-param marshalling of `hit` (:242,:264-269) or the 11-arg UFunction call itself (:245-258) | main.lua:342 carries it |

### C3. stage `load pickup class`

| `<err>` payload | Trigger | Pin |
|---|---|---|
| `Pickup has no actor class.` | normalized path nil (entry.path empty/"None"/unparseable) | main.lua:281 |
| `Pickup class could not be loaded: <path>` | `StaticFindObject` invalid → `LoadAsset` retry (:285) → second `StaticFindObject` still invalid | main.lua:290 (route :283-287) |

### C4. stage `spawn actor`

| `<err>` payload | Trigger | Pin |
|---|---|---|
| `Could not find class: /Script/Engine.Default__GameplayStatics` | GameplayStatics CDO query failed | main.lua:45 via :294 |
| `Actor creation returned invalid.` | `BeginDeferredActorSpawnFromClass` returned invalid actor | main.lua:300 |
| `<Lua error from BeginDeferredActorSpawnFromClass / FinishSpawningActor>` | deferred-spawn UFunction threw (transform struct marshalling :273-277,296,303) | main.lua:342 carries it |

### C5. stage `configure pickup`

| `<err>` payload | Trigger | Pin |
|---|---|---|
| `Spawned actor has no PickupInteractable.` | `actor.PickupInteractable` invalid AND `GetComponentByClass("/Script/Townfall.PickupInteractable")` failed/invalid (the class() throw inside is pcall-swallowed at :207-209) | main.lua:308 (route :198-213) |
| `Spawned pickup has no valid inventory ID.` | `pickup.PickupInventoryItem.ItemId` read empty/"None" (read is pcall-wrapped :312-314) | main.lua:317 |
| `Pickup mismatch: expected <id>, got <id2>` | DB-resolved entry's ItemId differs from the spawned actor's — wrong actor class resolved for that item | main.lua:322 |
| `<Lua error from property writes / setters>` | `PickupAmount=` (:325), `bAutoDestroyOnPickUp=` (:326), `SetEnabled` (:327), `SetActorHiddenInGame` (:329), `SetActorEnableCollision` (:330) | main.lua:342 carries it |

Note: in C4/C5 the actor may already exist; the catcher destroys it when invalid (:344-346). A
`Spawned` line NEVER coexists with a `Failed at` line for the same press (success path returns at :336
before the catcher).

## D. Silent no-op presses (not failures — do not misdiagnose)

| Situation | Mod behavior | Pin |
|---|---|---|
| CROSS on an empty slot row (`---`) | `spawnSelected(nil)` returns with NO log, NO status change | main.lua:220 |
| CROSS while a spawn is in flight | STATUS row shows `Wait for the previous spawn.` — NO log | main.lua:216-219 |
| OPTIONS #2+ on an already-built menu | no scan (items>0), buildUI early-true → NO new log lines at all | main.lua:651, :489-491 |
| UP/DOWN/LEFT/RIGHT/Quantity/Page | never log anything; screen-only evidence | main.lua:698-729, :378-396 |
| F7 with menu already closed + items empty + PC invalid | scan logs `Scan failed: …`, then buildUI fails silently (PC miss) — one scan line, no menu | main.lua:651→652→495 |
| Map load during any state | RegisterLoadMapPreHook resets UI/items/actors; status `Load complete. Open with F7.`; NO log line | main.lua:731-744 |

## E. Runtime-level drain/input failures (NOT mod errors; runtime-owned)

| Signature | Meaning | Source |
|---|---|---|
| UE4SS.log init shows `[R3] refused: EngineTick pre-callback registrations (GameThreadInitializer + QueryHarnessDispatch) (detour registration compiled out under R3 marshal mode)` AND `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal`; then OPTIONS is totally silent forever | R3-armed fold-1 variant: `pump_pending` has zero consumers — menu can never open, nothing can spawn. Expected on that variant, not new info | API-GAP.md §3/§5; initializer-l10-r3:1859-1860 |
| `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)` at first press | EngineTick hook genuinely unavailable (call-time check in this mod's usage pattern) | LuaMod.cpp:4729 |
| First-OPTIONS produces zero mod lines and NO `[R3]` init lines and no call-time throw | input-source problem (pad bridge absent/failed — `Native input service is unavailable…` / `Input registration unavailable: no active input source` at startup would already have thrown) | comp6-program:728-757; Handler.cpp:109,123 |
| Menu opened, then ENTIRE menu freezes (no marker movement, no close) | mid-session pump death: all six callbacks share one `m_engine_tick_actions` queue — partial stall impossible at queue level | LuaMod.cpp:4774-4790 |

## F. Decision shortcut for the controller

- `Spawned … ID=` line → rungs 4 (+5 with a frame) CLOSED.
- `Failed at <stage>` line → rung 4 executed but failed; read `<stage>` + payload against §C; mod alive.
- `Scan failed` / `UI creation failed` / `WidgetBlueprintLibrary not found` → rungs 1-3 partial; read §B.
- Nothing after OPTIONS-press #1 → POST-POPUP.md §3 table before concluding anything.
- Any `---`-row or busy-gate or repeat-press silence → §D, not a failure.
