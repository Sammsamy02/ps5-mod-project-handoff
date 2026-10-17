# ItemSpawner API gaps in actual runtime order

Local report only, 2026-10-07. No runtime/mod implementation, console/account contact or changes. Source: the immutable TownfallItemSpawner v1.0 Nexus release (`mod95/file218`), l15 provenance, and its API census. The user owns Townfall on PS5 only; no same-game PC reference is available.

## Verified census and stopping point

The full `API-GAPS.json` was parsed locally and its counts recomputed:

| Census category | Core/global/type surfaces | Reflected methods |
|---|---:|---:|
| Total | 17 | 33 |
| Implemented + console-proven for this downloaded mod | 0 | 0 |
| Implemented, untested | 13 | 33 |
| Missing source binding | 0 | 0 counted as missing by this source census |
| Behavior differs | 4 | 0 separately classified |

All 17 `console_proof_this_mod` fields and all 33 reflected `console_proven` fields are false. A generic reflection route being present does **not** prove any particular live game method exists, has the expected signature, or marshals successfully. “0 missing source bindings” is not a console compatibility pass and is not a claim that every possible UE4SS API exists.

The four behavior differences are `require`, `FName`, `ExecuteInGameThread`, and `RegisterLoadMapPreHook`. Each used overload/path is itself untested for this downloaded mod. The l15 census records five selected source-hash checks against the l15 receipt; that is source attribution, not runtime proof.

The recorded l15 PRX hash is `1cb1b27a2771a9178b6e5725665d398b81b56e8e84685903a67aa407b5585acb`. `main.lua` is 21,415 bytes with SHA-256 `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`; the release and harvested console copy agree in the provenance manifest. The archive SHA-256 is `a9f7ed004dd053e02f6206d8ed6886174f9b5aaecf55244447a2c5fb9d6fa7fb`. Both release files remain immutable.

Pool admission passed in l15. A delayed fresh constructor-prologue sink `_openat` then returned -1/EPERM; no source bytes were copied and the constructor was not called. Runtime refused and stopped ItemSpawner before the release's Loaded print. Consequently, registration, F7 delivery, menu creation, reflected calls and spawn have no evidence from that run. The 88-byte pure Lua diagnostic is a separate script.

Evidence: [API census JSON](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/api-audit/API-GAPS.json), [API audit narrative](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/api-audit/API-GAPS.md), [release manifest](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/provenance/MOD-RELEASE-MANIFEST.json), [l15 console result](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/CONSOLE-REPORT.md).

## Registration happens before gameplay helpers

This order is derived from calls and callbacks, not function-definition line numbers. The release defines helpers early, but defining `getPlayerController`, `scanItems`, `spawnSelected` and `buildUI` does not run them.

1. Initialize Lua tables/state and call `buildRows()` at line424. This creates row closures using ordinary Lua; it performs no object lookup, scan, UMG construction or spawn.
2. Call `RegisterKeyBind` six times, in this order: F7 (694), UP_ARROW (698), DOWN_ARROW (703), RETURN (708), LEFT_ARROW (713), RIGHT_ARROW (722). The bodies containing `ExecuteInGameThread` are deferred callbacks; they do not run during these registrations. `Key` values and later controller translation must preserve the original keys.
3. Call `pcall(RegisterLoadMapPreHook, callback)` at731. Registering successfully does not prove the callback will fire. The protected call can hide a registration error from the final Loaded message.
4. Call the release's final `log(...)` at746, which calls `print` through the helper at6.

The first required native calls **from this mod's chunk** are therefore key registration, optional map-hook registration and print, not `require`, `FindFirstOf`, `FName` or actor spawning. Runtime readiness checks can independently call native APIs before the chunk; the l15 FName witness is such a check.

## The 17 surfaces, ordered by the mod's flow

U = implemented, untested. D = behavior differs in the full API; this release's used path remains untested. Branches can skip individual calls, so this is a flow-oriented census rather than a claim that every execution has one total order.

| First relevant stage | Surface | Status | Concrete unresolved behavior |
|---|---|---|---|
| Chunk registration | `RegisterKeyBind` | U | All six registrations, original Key constants, callback lifetime and translated PS5 delivery. |
| Chunk registration | `RegisterLoadMapPreHook` | D | Callback is stored; deferred LoadMap arm may permanently refuse delivery. Test actual map transition and UI/input cleanup. |
| Loaded / later status logging | `print` | U | No Loaded print from the downloaded mod in l15. Later scan/menu/spawn status/error logs also use it. |
| F7, navigation, Enter callbacks | `ExecuteInGameThread` | D | Prospero uses a FuncPtr-marshal carrier instead of the normal PC queue path. Prove carrier readiness, bounded refusal, ordering and actual engine-thread identity. |
| First F7 item scan | `FindFirstOf` | U | First finds ItemDatabaseSubsystem. Later controller fallback finds PlayerController. Real class/name/object-array traversal is untested. |
| Scan and every live-object operation | `UObject:IsValid` | U | Real game validity/lifetime, invalid-wrapper behavior and stale references. Nil checks can skip invocation. |
| Scan method lookup; UI and spawn properties | `UObject.__index / __newindex` | U | Reflected function lookup, reads and writes use exact game offsets and ownership. Needs live data validation. |
| Database item/path conversion; later inventory ID | `FName/FText/FString:ToString` | U | `str()` bypasses ToString for an ordinary Lua string; non-string native values need actual conversion proof. Pool anchor admission does not prove it. |
| F7 UI controller lookup, then repeated lookups | `require` | D | `pcall(require, "UEHelpers")` occurs inside `getPlayerController`, not at chunk load. Native loader keeps preload plus retained-buffer searcher and removes ordinary disk/C searchers. This import and package caching are untested. |
| First UEHelpers import / invalid-controller fallback | `CreateInvalidObject` | U | UEHelpers module initialization creates EngineCache and GameInstanceCache invalid wrappers. GetPlayerController can return another invalid wrapper. |
| UEHelpers.GetPlayerController | `FindAllOf` | U | Enumerates PlayerController; tries Controller only if the first result is falsy, not merely an empty table. Returned objects/controller selection are untested. |
| F7 UMG lookup and construction; later trace/assets | `StaticFindObject` | U | Local traversal/path matching rather than a verified native StaticFindObject function. Real UMG class/default-object and cooked asset lookup need proof. |
| UMG construction arguments | `FName` | D | Release uses default Add1. Corrected output/UTF16 ABI is host-tested and never called in l15. Full API accepts Replace2 in Lua but native adapter rejects it. |
| UMG tree/widgets and root fallback | `StaticConstructObject` | U | Parameters layout, outer/name handling and game constructor ABI. Root fallback is conditional; later WidgetTree/CanvasPanel/Border/TextBlock construction is required on the normal build path. |
| Controller collections, UMG parameters/properties, spawn trace | `UScriptStruct / array / OUT marshalling` | U | Array/controller paths, anchors, vectors, colors, margins, fonts and nested writable properties; later Rotator/Transform/HitResult and 12-argument trace OUT semantics. |
| UMG text creation / refresh | `FText` | U | Native construction, text ownership and SetText/property copying. |
| Spawn class absent from live objects | `LoadAsset` | U | Conditional asset-load fallback followed by another StaticFindObject; true game-thread and live asset-service behavior are untested. |

There are no missing source-binding entries to list. Any live absence or refused dispatch encountered later must be recorded as an actual failure rather than inferred from symbol presence.

## F7: scan first, then build and show the original UMG menu

The first F7 callback calls `ExecuteInGameThread(toggleMenu)`. With the initial `menuOpen=false`, `toggleMenu` calls `showMenu(true)`. When the item list is empty, **`scanItems()` runs before `buildUI()`**:

1. `database()` calls `FindFirstOf("ItemDatabaseSubsystem")` and checks IsValid. The scan adds the six cooked-class entries, then queries legacy enums through `GetItemIDFromDeprecatedEnum` and, only for a nonempty/non-None ID, `GetInteractablePickupIDActorPath`. Native results go through `str`/ToString; the Lua list is sorted and status is logged. A failed scan is protected and does not automatically prevent `buildUI` from being attempted.
2. `buildUI()` checks its existing UI state, then calls `getPlayerController()`. The first successful UEHelpers import executes its two cache initializers. GetPlayerController uses `FindAllOf`, IsValid, the `IsPlayerController` property/method and an OR fallback to `IsLocalPlayerController`; the release then validates the result. If the helper path fails, the release tries `FindFirstOf("PlayerController")`.
3. Find the WidgetBlueprintLibrary default object and UserWidget class. Try its reflected `Create(pc, class, pc)`. Only if that fails/returns invalid, construct the root with `StaticConstructObject(..., FName("TownfallItemSpawnerRoot"))`.
4. Build WidgetTree, CanvasPanel, frame Border, inner Border and VerticalBox; assign `ui.root.WidgetTree` and `ui.tree.RootWidget`; add children and set layout/content/style. `construct()` evaluates `class(path)` and `FName(name)` before calling StaticConstructObject. It is not enough to expose an alternative runtime-owned PS5 menu.
5. Repeated `addStyledText` calls construct TextBlocks, AddChild, set padding/text/font/color/wrapping/shadow. Build section labels and rows, refresh text, then `AddToViewport(9999)` and initial `SetVisibility(1)`. Log Menu created.
6. Back in `showMenu`, refresh again, `SetVisibility(0)`, look up the controller again, call `SetIgnoreLookInput(true)` and `SetIgnoreMoveInput(true)`, then set `menuOpen=true`.

Several styling, visibility and input-lock calls are wrapped in `pcall`. A Menu created print or `buildUI()` true result can coexist with silently failed optional calls. Acceptance must show the original menu visibly, readable labels and selection, correct input locking, close/reopen behavior, and no orphaned widgets/input locks.

UP/DOWN callbacks change selection and refresh. LEFT/RIGHT change pages and refresh. RETURN calls `ExecuteInGameThread(activateSelection)` while the menu is open. The quantity/page/refresh rows perform their own actions; **only a selected pickup row calls `spawnSelected(entry)`**. Empty pickups can return without spawning. Closing with F7 hides the root and clears the input locks.

## All 33 reflected methods, in flow order

Every method below is U. The generic UObject/UFunction dispatch exists in source; live method existence, parameter layouts, return values and successful invocation are untested. Repeated names appear once here at their first relevant stage; controller/helper methods and UI refresh recur later.

| Stage | Methods in call-flow order | Important branch/layout condition |
|---|---|---|
| First F7 database scan | `GetItemIDFromDeprecatedEnum`, `GetInteractablePickupIDActorPath` | Second call only after a valid ID; strings/path data must match the live database. |
| UI controller helper | `IsPlayerController`, `IsLocalPlayerController` | Property existence and Lua OR short-circuit determine which methods run. |
| UMG root | `Create` | Invalid/error result selects the StaticConstructObject fallback. |
| UMG panel tree/layout | `AddChild`, `SetAnchors`, `SetAlignment`, `SetPosition`, `SetSize`, `SetPadding`, `SetBrushColor`, `SetContent` | Child slot class, anchors/vectors/margins/colors and ownership must be correct. Later TextBlock additions reuse AddChild/SetPadding. |
| Text and row styling | `SetText`, `SetColorAndOpacity`, `SetAutoWrapText`, `SetShadowColorAndOpacity` | FText/color representation plus Font.Size/ShadowOffset property writes; pcall can hide style failure. |
| Display and input lock | `AddToViewport`, `SetVisibility`, `SetIgnoreLookInput`, `SetIgnoreMoveInput` | Expected visibility0/1, viewport order9999, actual controller state and clean unlock. |
| Spawn player/pawn lookup | `GetPawn`, `K2_GetPawn` | Try pc.Pawn property first; each method is a subsequent fallback only if earlier pawn candidates fail validation. |
| Spawn geometry and ground trace | `K2_GetActorLocation`, `GetControlRotation`, `LineTraceSingle` | Vector/Rotator read, 12 arguments, actor array and HitResult OUT table. Require a valid hit point and sufficiently flat normal. |
| Actor creation | `BeginDeferredActorSpawnFromClass`, `FinishSpawningActor` | Real game thread, complete Transform and successful native actor lifetime. |
| Pickup component fallback | `GetComponentByClass` | Try actor.PickupInteractable property first; call only if that is invalid. |
| Configure valid pickup | `SetEnabled`, `SetActorHiddenInGame`, `SetActorEnableCollision` | Nested inventory ID must be valid and match the requested database ID before these calls. |
| Failed-spawn cleanup | `K2_DestroyActor` | Conditional on an actor surviving failure with IsValid true. |

The grouped list contains all 33 names from the JSON census exactly once. Helper-only IsPlayerController/IsLocalPlayerController are included. Unrelated UEHelpers exports (`ModRef:GetSharedVariable`, `ModRef:SetSharedVariable`, `UnrealVersion:IsBelow`, weak-object helpers) are not reached by this mod's helper path and are not added to the count.

## Enter: exact spawn chain and cleanup

`activateSelection()` invokes the chosen row's closure and refreshes after it. A pickup closure indexes the current page's entry and calls `spawnSelected`. That function guards busy/empty entry, marks busy, and runs this protected chain:

1. `getPlayerPawn()` calls `getPlayerController()` and validates it, then tries `pc.Pawn`, `pc:GetPawn()`, `pc:K2_GetPawn()` in that order, stopping at the first valid pawn. It then looks up and validates the controller again.
2. Call pawn K2_GetActorLocation and controller GetControlRotation; read position X/Y/Z and yaw. Find KismetSystemLibrary and LineTraceSingle forward/downward with the pawn ignored. Read `hit.ImpactPoint or hit.Location`, optional ImpactNormal, and reject missing ground/steep surface.
3. Build Transform from the resulting ground point. Normalize the entry's cooked class path; StaticFindObject; if invalid, LoadAsset followed by StaticFindObject again; refuse if still invalid.
4. Find GameplayStatics; BeginDeferredActorSpawnFromClass, validate the actor, then FinishSpawningActor. Locate PickupInteractable directly or through GetComponentByClass.
5. Read `pickup.PickupInventoryItem.ItemId` and convert it. Refuse an empty/None ID or mismatch with a nonempty requested ID. Write `PickupAmount` and `bAutoDestroyOnPickUp`, then SetEnabled, SetActorHiddenInGame and SetActorEnableCollision. Retain the actor in ownedActors and log success.
6. Clear busy regardless of the protected call's result. On failure, log its stage and conditionally K2_DestroyActor. The outer selection action then refreshes the status text.

The map-prehook can fire at a later transition, outside the above linear path. It closes an open menu, unlocks input through `showMenu(false)`, clears UI/item/ownedActors references, and resets selection/busy/status. It does not individually destroy all retained actors. Test real callback delivery and safe map transition rather than equating registration with working cleanup.

## Validation order and limits

Keep the requested sequence: resolve the live witness refusal → off-by-default runtime tooling → runtime/config controller translation to the original PC keys → original mod UMG menu → an actual pickup spawn → conformance and more unmodified mods → deferred B/C/A phases. Do not edit the PC release or substitute a proxy menu to satisfy the ladder.

Conformance must record deterministic case IDs, normalized values, explicit failed/skipped reasons, real callback-thread proof, data/layout validation, and cleanup/revert for widget/map/spawn cases. A same-game PC comparison requires an otherwise authorized reference run; another UE5.6 title can examine generic APIs but cannot establish Townfall-specific database, cooked pickup or UMG parity. Host tests, bound symbols, source-hash checks and Ploader OK do not fill that gap. No conformance script or diff tool was installed or run for this report.
