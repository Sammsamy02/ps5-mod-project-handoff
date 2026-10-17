# RUNTIME-REQUIREMENTS.md — intersection set the runtime must provide for Item Spawner (unchanged)

worker-modreqs, 2026-10-04. Scope: exactly what the retained, hash-verified script
`TownfallItemSpawner/Scripts/main.lua` (sha256 f586043a…658, 746 lines) invokes, and nothing more.
No per-mod rewriting; every item below must be a generic runtime capability.

Status markers (worker roles per TEAM_PLAN.txt):
- **available** — already implemented and evidenced on the host path.
- **depends-on-worker-1** — live module facts supplier (native loading / live base addresses).
- **depends-on-worker-2** — validated Unreal build/layout facts (struct widths, offsets, callable addresses).
- **depends-on-worker-3** — game-thread/Lua delivery adapter + reflection call path (consumes 1+2).
- **missing-new** — no owner yet; new component required beyond the named workers.

Baseline facts (checkpoint `item_spawner_trial` + `.superpowers/native-dependency-spike/item-spawner-runtime-trial-1/findings.md`):
unchanged host startup passed, 6 keybinds registered, first F7 callback failed at script line 695
`attempt to call a nil value (global 'ExecuteInGameThread')`; no menu/database/trace/spawn ran.

## A. Lua-visible globals

| # | Requirement | Script evidence | Status |
|---|---|---|---|
| A1 | `print(msg)` with `\n`-terminated lines | IS:6 | **available** (host Lua) |
| A2 | `require("UEHelpers")` module loading | IS:15 | **available** (mb_lua_install_require; UEHelpers inventory unchanged, 23 functions lines 31–265) |
| A3 | `FindFirstOf(className)` — live GUObject/array instance lookup for `PlayerController`, `ItemDatabaseSubsystem` | IS:21, 64 | **depends-on-worker-3** (reflection provider) + worker-2 offsets; object-array backing store **depends-on-worker-1** live module |
| A4 | `StaticFindObject(path)` — named-object lookup incl. `Default__` CDOs and `/Game/...` classes | IS:44, 283, 499 | **depends-on-worker-3**; needs worker-2 FName/GObjects offsets |
| A5 | `StaticConstructObject(class, outer, FName)` — reflected object construction | IS:50, 516 | **missing-new** engine callable (worker-2 must supply the address; worker-3 the call path) |
| A6 | `FName(string)` / `FText(string)` constructors with engine pool semantics | IS:50, 434, 469… | FName: worker-2 width/pool facts **missing**; FText: **missing-new** |
| A7 | `LoadAsset(path)` fallback when a class is not resident | IS:285 | **missing-new** (asset loading service) |
| A8 | `RegisterKeyBind(Key.<K>, cb)` for F7, UP/DOWN/LEFT/RIGHT_ARROW, RETURN | IS:694–729 | registration **available** (portable registry, 6/6 registered; pinned F7 edge 118 dispatched); **real device input source missing-new** (keyboard input on PS5 unproven; prior controller-only test) |
| A9 | `ExecuteInGameThread(fn)` — run callback on the real game thread | IS:695 (the exact observed failure), 700, 705, 710, 716, 725 | **depends-on-worker-3** (delivery adapter; EngineTick route per settings `DefaultExecuteInGameThreadMethod = EngineTick`); the async queue is caller-pumped and is NOT a substitute (findings.md) |
| A10 | `RegisterLoadMapPreHook(fn)` (optional; resets UI/items on map change) | IS:731 (pcall-guarded) | **missing-new** (map lifecycle hook; optional for first visible milestone) |

## B. Reflection / UObject services

| # | Requirement | Script evidence | Status |
|---|---|---|---|
| B1 | `obj:IsValid()` | IS:10 | **available** (null/invalid-object leaves, genuine invalid values) |
| B2 | Property read incl. nested chains (`pc.Pawn`, `pickup.PickupInventoryItem.ItemId`) | IS:31, 313 | **depends-on-worker-2** (validated property offsets) + worker-3 access path |
| B3 | Property write (`pickup.PickupAmount`, `bAutoDestroyOnPickUp`, `ui.root.WidgetTree`) | IS:325–326, 525 | same as B2 |
| B4 | UFunction (reflected method) invocation, 0–12 args: `GetPawn/K2_GetPawn`, `K2_GetActorLocation`, `GetControlRotation`, `GetItemIDFromDeprecatedEnum`, `GetInteractablePickupIDActorPath`, `GetComponentByClass`, `ToString`, UMG setters (§D) | IS:32–33, 233–234, 163, 168, 208, 59, 364–483 | **depends-on-worker-3**; callable function addresses **depend-on-worker-2** (static survey has candidates, none live-proven) |
| B5 | **Out-parameter marshalling**: `LineTraceSingle(..., hit, ...)` fills a Lua table later read as `hit.ImpactPoint/.Location/.ImpactNormal` | IS:245–258, 264–269 | **missing-new** (out-param lifetime work exists `ue4ss-native-outparam-lifetime-1` but no engine-backed implementation; native runtime never executed — checkpoint `native_lua_mod_path_lifetime`) |
| B6 | Lua-table→engine struct conversion: FVector, FTransform, FLinearColor(+rule), FMargin, anchors, array-of-actors `{pawn}` | IS:243–257, 273–277, 364–367, 460–465, 534–541 | **depends-on-worker-2** (exact widths; 304/419 accessor widths compiled, Townfall offsets unvalidated) + worker-3 converters |
| B7 | Engine-struct field reads on returned values (`.Yaw`, `.X/.Y/.Z`) | IS:234, 237–238 | same as B6 |
| B8 | Game-subsystem database functions returning FName/string (`GetItemIDFromDeprecatedEnum`, `GetInteractablePickupIDActorPath`) + FName→Lua-string (`ToString`, FName→String method `Scan` per settings) | IS:59, 163–168 | **missing-new** (FName→string: SkySphereMesh anchor absent in survey; Kismet Conv_NameToString fallback retained but unproven live) |

## C. World / spawn services (the actual gameplay payload)

| # | Requirement | Script evidence | Status |
|---|---|---|---|
| C1 | `KismetSystemLibrary.Default__` + `LineTraceSingle` (collision trace, ignore-actor array) | IS:241–258 | **depends-on-worker-3**; callable address worker-2 |
| C2 | `GameplayStatics.Default__` + `BeginDeferredActorSpawnFromClass` + `FinishSpawningActor` with FTransform + collision method 2 | IS:294–303 | **depends-on-worker-3**; **missing-new** spawn ownership/GC lifetime facts (ownedActors list at IS:332 is Lua-side only) |
| C3 | Actor state setters `SetActorHiddenInGame`, `SetActorEnableCollision`, `K2_DestroyActor` (failure cleanup path) | IS:329–330, 345 | worker-3 call path |
| C4 | Input lock during menu: `SetIgnoreLookInput/SetIgnoreMoveInput` on live PlayerController | IS:450–451 | worker-3 call path |
| C5 | Valid live `PlayerController`/`Pawn` objects obtainable after save load (timing: mod useful only post-save) | IS:14–41, 66, 228–231 | **depends-on-worker-1** (live instances need the live module/GUObject source) |

## D. UMG services (the visible milestone surface)

| # | Requirement | Script evidence | Status |
|---|---|---|---|
| D1 | `WidgetBlueprintLibrary.Create` (with StaticConstructObject fallback) for UserWidget root | IS:499–521 | **missing-new** (UMG family) |
| D2 | Construct WidgetTree/CanvasPanel/Border/VerticalBox/TextBlock under an outer | IS:524–560, 455 | **missing-new** |
| D3 | Widget/ slot methods: AddChild, SetContent, SetPadding, SetBrushColor, SetAnchors, SetAlignment, SetPosition, SetSize, SetText(FText), SetColorAndOpacity, SetShadowColorAndOpacity, SetAutoWrapText, Font/ShadowOffset property writes | IS:456–483, 531–561 | **missing-new** |
| D4 | `AddToViewport(9999)`, `SetVisibility` | IS:631–632, 655, 660 | **missing-new** |
| D5 | Widget lifetime across GC/map change (widget table nulled on map hook; pcall guards everywhere) | IS:731–744 | **missing-new** (ties to A10) |

## E. Timing / ordering / config inputs

| Requirement | Evidence | Status |
|---|---|---|
| No recurring scheduler needed — only per-key `ExecuteInGameThread` (contrast: minimap needs `LoopInGameThreadWithDelay`) | findings.md comparison; IS has no LoopAsync/LoopInGameThread | (none required beyond A9) |
| Mod startup after engine/world exist; first useful action requires a loaded save (ItemDatabaseSubsystem ready) | IS:66 error path "Load a save first", IS:118 | **depends-on-worker-1** live state; ordering enforced by UE4SSProgram deferred mod startup in retained bootstrap (source port) |
| `enabled.txt` marker + Mods-folder scan registration | zip member 2; distro mods.txt/mods.json pattern §8c census | **available** on host intake; console-side activation via asset-installer route (checkpoint `ue4ss_current_runtime_readiness.asset_installer_route_available`) |
| Key constants `Key.F7` etc. pinned | 6 registrations verified in trial | **available** |
| UE4SS-settings inputs that this mod's path depends on: `DefaultExecuteInGameThreadMethod = EngineTick`, `DefaultFNameToStringMethod = Scan` | settings ini (retained, §8c) | EngineTick delivery = A9; FName Scan = B8 |

## F. Summary against the four named missing-fact families

- **Live module supplier** (worker-1): blocks A3/A4 backing store, C5, and every live object/offset read (A3–C4 all consume it once workers 2/3 land their slices).
- **Validated layouts** (worker-2): blocks B2/B3/B6/B7 struct+property widths, B4/B8/C1/C2 callable addresses, A6 FName width/pool.
- **Callable engine functions** (worker-2 addresses + worker-3 path): B4, B8, C1–C4 — static survey has candidate families, none live-validated; no guessed addresses permitted.
- **Game-thread delivery** (worker-3, consumes 1+2): A9 `ExecuteInGameThread` is the single hard gate already observed failing at IS:695; D1–D5 and C1–C2 execute inside it.
- **Missing-new (new scope items)**: A7 LoadAsset, A8 real input event source, A10 map hooks, B5 out-param engine marshalling, B8 FName→string live path, C2 spawn/GC ownership, D1–D5 UMG family.

Nothing in Item Spawner requires `LoopAsync`/`LoopInGameThreadWithDelay`/`NotifyOnNewObject`/`ExecuteWithDelay`
(those belong to stamina/mod-menu/minimap — MOD-CENSUS §2–4); that keeps the first-visible-milestone intersection minimal:
**A9 + A3/A4 + B1–B8 + C1–C5 + D1–D5, bootstrapped by worker-1 live facts and worker-2 layouts.**
