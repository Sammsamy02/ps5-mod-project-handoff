# MOD-CENSUS.md — representative retained mods, classes, exact API calls, config inputs, destinations

worker-modreqs, 2026-10-04. All facts below come from retained files (hashes in CLAIM.json). Line numbers
are from the cited file as retained on disk. Nothing was executed. "IS" = Item Spawner retained script
`.superpowers/native-observations/item-spawner-real-mod-intake-1/member-00000000.bin`
(zip member `TownfallItemSpawner/Scripts/main.lua`, 746 lines, sha256 f586043a…658).

Corpus = the eight original Nexus ZIPs in `outputs/2026-10-04-zcode-team/worker-6-containers/evidence/zip-roundtrip.json`
(all 8 present, roundtrip_pass 8/8) plus their retained extracted copies and intake receipts.

---

## 1. Townfall Item Spawner v1.0 — UE4SS Lua (first-visible-milestone target)

- Source: nexusmods.com/silenthilltownfall/mods/95 file 218 (`intake.json`). Class: **UE4SS Lua script**, no assets, no binaries.
- Archive: `original.zip` sha256 a9f7ed00…7fb, 2 members: `TownfallItemSpawner/Scripts/main.lua` (21415 B) + empty `TownfallItemSpawner/enabled.txt`.

### Exact engine APIs / reflection calls invoked (IS file + line)
Global Lua-visible UE4SS API:
| API | Lines |
|---|---|
| `print` | 6, 82(log wrapper), 189, 342, 636, 746 |
| `require("UEHelpers")` → `UEHelpers.GetPlayerController()` | 15–18 |
| `FindFirstOf("PlayerController")` | 21 |
| `FindFirstOf("ItemDatabaseSubsystem")` | 64 |
| `StaticFindObject(path)` (classes + `/Game/...` asset paths) | 44, 283, 286, 499 |
| `StaticConstructObject(class, outer, FName(name))` | 50, 516 |
| `FName(name)` / `FText(text)` constructors | 50, 434, 469, 516, etc. |
| `LoadAsset(path)` | 285 |
| `RegisterKeyBind(Key.F7 / UP_ARROW / DOWN_ARROW / RETURN / LEFT_ARROW / RIGHT_ARROW, cb)` | 694, 698, 703, 708, 713, 722 |
| `ExecuteInGameThread(fn)` (every keybind callback) | 695, 700, 705, 710, 716, 725 |
| `RegisterLoadMapPreHook(fn)` (optional, pcall-guarded) | 731 |

UObject member/reflection patterns:
| Pattern | Lines |
|---|---|
| `obj:IsValid()` | 10 |
| `pc.Pawn` property, `pc:GetPawn()`, `pc:K2_GetPawn()` | 31–33 |
| `pc:GetControlRotation().Yaw` (returned-struct field) | 234 |
| `db:GetItemIDFromDeprecatedEnum(entry.value)` (UFunction, game subsystem) | 163 |
| `db:GetInteractablePickupIDActorPath(id)` (UFunction) | 168 |
| `v:ToString()` | 59 |
| `actor.PickupInteractable` property | 202 |
| `actor:GetComponentByClass(class("/Script/Townfall.PickupInteractable"))` | 208 |
| `pawn:K2_GetActorLocation()` | 233 |
| `system:LineTraceSingle(pawn, Vec, Vec, 0, false, {pawn}, 0, hit, true, color, color, 0)` — 12-arg Kismet call, Lua-table `hit` used as **out-parameter** (`.ImpactPoint`, `.Location`, `.ImpactNormal` read back at 264–269) | 245–258 |
| `gameplay:BeginDeferredActorSpawnFromClass(pawn, pickupClass, transform, 2, pawn, 0)` | 295–297 |
| `gameplay:FinishSpawningActor(actor, transform, 0)` | 303 |
| `pickup.PickupInventoryItem.ItemId` (nested property read) | 313 |
| `pickup.PickupAmount = n`, `pickup.bAutoDestroyOnPickUp = true` (property writes) | 325–326 |
| `pickup:SetEnabled(true)` | 327 |
| `actor:SetActorHiddenInGame(false)` / `actor:SetActorEnableCollision(true)` | 329–330 |
| `actor:K2_DestroyActor()` (failure cleanup) | 345 |
| `pc:SetIgnoreLookInput(lock)` / `pc:SetIgnoreMoveInput(lock)` | 450–451 |

UMG construction/display:
| Pattern | Lines |
|---|---|
| `widgetLib:Create(pc, userWidgetClass, pc)` on `/Script/UMG.Default__WidgetBlueprintLibrary` | 499, 510 |
| Fallback `StaticConstructObject(userWidgetClass, pc, FName(...))` for `/Script/UMG.UserWidget` | 506, 516 |
| Construct `/Script/UMG.WidgetTree`, `.CanvasPanel`, `.Border`, `.VerticalBox`, `.TextBlock`; `ui.root.WidgetTree` / `ui.tree.RootWidget` property writes | 524–528, 530, 550, 560, 455 |
| `parent:AddChild(w)` → returned slot `:SetPadding/:SetAnchors/:SetAlignment/:SetPosition/:SetSize` | 456, 460, 531–541 |
| `panel:SetContent(w)` / `SetPadding` / `SetBrushColor` | 544–551, 554–561 |
| `w:SetText(FText(...))`, `w:SetColorAndOpacity({SpecifiedColor={R,G,B,A},ColorUseRule=0})`, `w:SetShadowColorAndOpacity(...)`, `w.ShadowOffset={X,Y}`, `w.Font.Size`, `w:SetAutoWrapText(false)` | 364–367, 434, 469–483 |
| `ui.root:AddToViewport(9999)`, `ui.root:SetVisibility(0/1)` | 631, 632, 655, 660 |

Struct-by-Lua-table marshalling observed: FVector {X,Y,Z}; FTransform {Rotation,Translation,Scale3D}; FLinearColor+ColorUseRule; FMargin {Left,Top,Right,Bottom}; anchors {Minimum,Maximum}; FHitResult out-table; font/margin nested writes.

Class-path strings referenced: `/Script/Townfall.PickupInteractable`, `/Script/Engine.Default__KismetSystemLibrary`, `/Script/Engine.Default__GameplayStatics`, `/Script/UMG.{TextBlock,UserWidget,WidgetTree,CanvasPanel,Border,VerticalBox,Default__WidgetBlueprintLibrary}`, six `/Game/Townfall/AlwaysCook/Items/...` pickup BluePrint paths (IS 73–78).

### Config / registration inputs
- `enabled.txt` (empty) — UE4SS per-mod enable marker.
- No `mods.txt` entry of its own shipped; registration happens through the UE4SS Mods directory scan
  (distro `mods.txt`/`mods.json` pattern, see §7).
- No declared prerequisite in the archive beyond "requires UE4SS" (header comment IS 1–3).

### File destinations
`<UE4SS mods root>/TownfallItemSpawner/{enabled.txt, Scripts/main.lua}` (zip is already laid out as the mod folder).

---

## 2. Infinite Stamina v1 — UE4SS Lua

- Source: `infinite-stamina-main.lua` (106 lines, sha256 7667fed7…335), zip members `Infinite Stamina/enabled.txt` + `Infinite Stamina/Scripts/main.lua` (unzip -l verified).
- Class: **UE4SS Lua script**, always-on loop pattern; no keybinds, no UI.

Exact calls (file `nexus-real-mods-2026-10-01/infinite-stamina-main.lua`):
| API | Lines |
|---|---|
| `require("UEHelpers")` | 7 |
| `UEHelpers.GetPlayer()` | 14 |
| `UEHelpers.GetKismetSystemLibrary()` / `UEHelpers.GetWorldContextObject()` | 57–58 |
| `FindFirstOf("TownfallPlayerCharacter")` | 20 |
| `FindFirstOf("TFPlayerGaitComponent")` / `FindFirstOf("UTFPlayerGaitComponent")` | 38, 41 |
| `Player.MapComponent.GaitComponent` (nested property chain) | 30–31 |
| `Player:IsValid()` etc. | 16, 22, 33, 40, 44 |
| `KismetSystem:PrintString(WorldContext, Message, true, false, TextColor, 3.5, FName("TownfallStaminaBanner"))` | 70–78 |
| `ExecuteInGameThread(fn)` | 56, 92 |
| `LoopAsync(1500, fn) return true` (recurring caller-pumped scheduler) | 89–107 |
| `GaitComp.bStaminaEnabled` property read + **write to false** | 98–99 |

Factual note: in the retained v1 source `InfiniteStaminaActive` is initialized `false` (line 11) and no toggle path
appears in the 106 lines, so the periodic write is gated off until something sets that local; the runtime requirement
(LoopAsync + ExecuteInGameThread + property write) is unchanged.

Config/registration: `enabled.txt` only. Destination `<UE4SS mods root>/Infinite Stamina/…`.

---

## 3. TownfallModMenu v6 — UE4SS Lua

- Retained extracted: `…/townfall-ue4ss-real-script-audit-1/mod-menu-v6/TownfallModMenu/Scripts/main.lua` (3198 lines, sha256 2ff1fa53…883) + empty `enabled.txt` (source-receipt.json).
- Class: **UE4SS Lua script** (cheat menu: invincibility, one-hit-kill, speed, fog, invisibility; UMG UI).

Exact calls (main.lua lines):
| API | Lines |
|---|---|
| `require("UEHelpers")` (cached) | 37–42 |
| `FindFirstOf("PlayerController" / "TFPlayerGaitComponent" / "UTFPlayerGaitComponent" / "ItemDatabaseSubsystem" / "BP_Bill_C" / "WBP_SimonOS_C")` | 45, 337–338, 1631, 1677, 1866 |
| `StaticFindObject(path)` | 67, 1505, 3035 |
| `StaticConstructObject(class, outer, FName(...))` | 73, 3048 |
| `prop:GetFName():ToString()` (property iterator chain) | 386 |
| `ExecuteInGameThreadWithDelay(ms, fn)` with capability test `type(...) == "function"`, fallback `ExecuteAsync(ms, function() ExecuteInGameThread(fn) end)` | 884–887, 1002–1005, 2429–2444, 2450–2453, 2563–2566, 2641–2644, 2698–2705 |
| `ExecuteWithDelay(ms, fn)` | 2051, 2721–2725 |
| `LoopInGameThreadWithDelay(100, gameplayTick)` with `LoopAsync(100,…)` fallback | 2429–2444 |
| `NotifyOnNewObject(className-or-object, cb)` — `II_SimonOSClass`, `/Script/Townfall.TownfallPlayerCharacter`, `BP_MotionlessTrigger_C`, `BP_TheMotionless_C`, `/Script/Townfall.TownfallEnemyCharacter` | 2037, 2650, 2663, 2677, 2689 |
| `RegisterLoadMapPreHook` / `RegisterLoadMapPostHook` (pcall-wrapped) | 2039/2045, 2753/2773, 3153 |
| UMG: `widgetLib:Create`, `StaticConstructObject(UserWidget)`, `SetText(FText(...))` | 3035, 3048, 2987, 3015 |
| `RegisterKeyBind(Key.BACKSPACE / UP_ARROW / DOWN_ARROW / RETURN, cb)` + `ExecuteInGameThread` | 3175–3188 |
| Boot hooks `ExecuteInGameThreadWithDelay(3000/7000, installPaceHooks)` | 3194–3196 |

Config/registration: `enabled.txt` only. Destination `<UE4SS mods root>/TownfallModMenu/…`.
Notable: version-degraded fallbacks mean this mod runs on a reduced API set but prefers
`ExecuteInGameThreadWithDelay`/`LoopInGameThreadWithDelay` when present (both currently unimplemented — see RUNTIME-REQUIREMENTS).

---

## 4. TownfallMiniMap v3.1 — UE4SS Lua

- Retained extracted: `…/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua` (4271 lines, 196743 B, sha256 d8f8d64f…00f) + `enabled.txt` (source-receipt.json).
- Class: **UE4SS Lua script** (full UMG minimap HUD + world polling).

Exact calls (main.lua lines):
| API | Lines |
|---|---|
| `o:IsValid()` | 217 |
| `StaticFindObject(path)` (cached class table) | 237 |
| `FindAllOf("PlayerController")` + `pc:IsLocalController()` | 307–308 |
| `FindAllOf("InteractComponent" / "MapCornerBounds" / "TownfallEnemyCharacter" / "Interactable" / "HidingPlaceInteractable" / "PickupInteractable")` | 345, 1450, 2579, 2745, 2880, 3000 |
| `StaticConstructObject(class, tree)` (CanvasPanel/Image/TextBlock widgets) | 604, 612, 658, 1062, 1088, 1125, 1132, 1354, 1958, 3132, 3193, 3201–3203, 3331, 3339, 3342, 3348, 3618, 3623, 3652, 3715 |
| `label:SetText(FText(...))` | 1181, 1236, 1986, 3104, 3242, 3254, 3353, 3382, 3630, 3752, 3780, 3790, 3816 |
| `x:GetFName()`, `x:GetClass():GetFName()` chains | 1401, 1578, 2291, 2632, 2656, 2680, 2704, 2785, 2941, 4138 |
| `component:GetOwner()`, `component:GetAttachParent().StaticMesh:GetFName()` | 2704, 2941 |
| `component.Enabled` property read | 2785 |
| `FName(k[2])` / `FName("MouseWheelAxis")` | 3269, 3966–3967, 4040 |
| `wt.Widget` property access | 4138 |
| `LoopInGameThreadWithDelay(1, M.frame)` (per-frame UI) | 4032–4034 |
| `LoopInGameThreadWithDelay(TICK_MS, safeTick)` (world tick) | 4267–4268 (guarded `if LoopInGameThreadWithDelay`) |
| `NotifyOnNewObject("/Script/Townfall.Interactable", cb)` | 4248–4249 |

Config/registration: `enabled.txt` only; ships pre-nested under `ue4ss/Mods/`. Destination `<UE4SS root>/Mods/TownfallMiniMap/…`.
Heaviest scheduler consumer of the corpus: two recurring in-game-thread loops are its primary startup dependency.

---

## 5. PlayerMovementSpeed 2x — IoStore PAK asset mod

- `movement-speed-real-mod-intake-2026-10-01/intake.json`: members `ReadMe.txt` + `Townfall/Content/Paks/~mods/PlayerMovementSpeed_2x_P.{pak(347 B),ucas(1938 B),utoc(462 B)}` (per-member sha256 recorded).
- Class: **PAK asset (IoStore utoc/ucas)**. No scripts, no config registration, no runtime activation needed.
- Destination: `<game>/Townfall/Content/Paks/~mods/` (the `~mods` loose-pak directory, `_P` suffix = patch priority).

## 6. Universal Performance PAK — IoStore PAK + Windows INI fragment

- `performance-pak-real-mod-intake-1/intake.json` (14 entries): `MOD/Townfall/Content/Paks/~mods/zzz_UniversalPerformance_P.pak` (IoStore) plus a separate Windows-only variant
  `FG ENABLER/%LOCALAPPDATA%/Townfall/Saved/Config/Windows/Engine.ini` whose entire retained content (78 B) is:
  `[SystemSettings]` / `r.FidelityFX.FI.Enabled=1` / `r.FidelityFX.FSR3.UseNativeDX12=1` (member-00000006.bin).
- Class: **PAK asset + INI config**. The INI member targets a Windows `%LOCALAPPDATA%` path that does not exist on PS5; it is an installer-adaptation input, not a runtime execution input.
- Destination: pak → `Content/Paks/~mods/`; ini → Windows `Saved/Config/Windows/Engine.ini` (PS5 equivalent path unresolved → gap, not guessed).

## 7. Leon 2K v1.1 — IoStore PAK texture pack

- `leon-download-receipt.json`: `zzz_Leon_Townfall_2K_P.{pak(347 B), utoc(50364 B), ucas(185446608 B)}` + README.
- Class: **PAK asset (IoStore)**, pure content replacement; same `~mods` destination. User-confirmed gameplay effect per checkpoint `representative_family_milestones.asset_containers`.

## 8. ue4ss-v1 distro — the retained native/PE DLL + example Lua mods + runtime config

Archive `ue4ss-v1.zip` (43 members). Retained extracted under `townfall-ue4ss-real-script-audit-1/ue4ss-v1/ue4ss/`.

### 8a. PE DLL members (the retained native DLL evidence; provenance `windows-image-original-inputs-1/inputs.json`)
- **`dwmapi.dll`** (279552 B, sha256 4e3e5324…c9a) — **PE DLL proxy loader** (AMD64, `IMAGE_FILE_DLL`). 143 exports; imports 135 symbols / 13 libraries. KERNEL32 set (25): `AreFileApisANSI, CloseHandle, CreateFile2, ExitProcess, FindClose, FindFirstFileW, FormatMessageA, FreeLibrary, GetCommandLineW, GetCurrentProcessId, GetCurrentThreadId, GetFileAttributesExW, GetFileInformationByHandleEx, GetLastError, GetLocaleInfoEx, GetModuleFileNameW, GetProcAddress, GetSystemDirectoryW, GetSystemTimeAsFileTime, InitializeSListHead, LoadLibraryW, LocalFree, MultiByteToWideChar, QueryPerformanceCounter, WideCharToMultiByte`. Also USER32 `MessageBoxA/W`, SHELL32 `CommandLineToArgvW`, VCRUNTIME140(+_1) incl. `_CxxThrowException, __CxxFrameHandler4, __C_specific_handler`, full MSVCP140 iostream/locale facet block (61), CRT filesystem/heap/locale/math/runtime/stdio/string.
- **`ue4ss/UE4SS.dll`** (20572160 B, sha256 94c2ebd3…52a) — the UE4SS runtime itself as **PE DLL**. 634 imports / 30 libraries; **non-zero static TLS directory** (tls_rva 17832576); entry+load-config+exceptions present. Full service-family import lists are in SERVICES-BACKLOG.md.

### 8b. Example Lua mods shipped in the distro (retained extracted)
| Mod | Exact calls (file:line) |
|---|---|
| LineTraceMod | `require("UEHelpers")` (2), `RegisterKeyBind(Key.F3, GetObjectName)` (79) |
| SplitScreenMod | `FindAllOf("PlayerController") or FindAllOf("Controller")` (23), `ExecuteInGameThread` (39/67/85), `RegisterKeyBind(Key.Y,{ModifierKey.CONTROL},…)` (106–110) |
| ConsoleCommandsMod | `require("summon_unloaded_assets")`, `require("set")`, `require("dump_object")` (10–12) |
| BPModLoaderMod | `StaticFindObject("/Script/AssetRegistry.Default__AssetRegistryHelpers"/"…Default__AssetRegistryImpl")` (241,249), `RegisterKeyBind(Key.INS,…)+ExecuteInGameThread` (269–270), `RegisterLoadMapPostHook(function(Engine, World))` (292), `FindFirstOf("Actor")` (297) |
| BPML_GenericFunctions | `StaticFindObject("/Script/CoreUObject.Class")` (50), `FindFirstOf("GameInstance")` (58), `StaticConstructObject` 11-arg form (60) |
| CheatManagerEnablerMod | `StaticFindObject("/Script/Engine.CheatManager")` (15), `StaticConstructObject(CheatManagerClass, PlayerController)` (23) |
| ConsoleEnablerMod | `StaticFindObject("/Script/Engine.Default__InputSettings")` (22), `UEHelpers.GetEngine()` (47), `StaticFindObject("/Script/Engine.Console")` (50), `StaticConstructObject(ConsoleClass, GameViewport)` (57), `ExecuteInGameThread(CreateConsole)` (75) |
| Keybinds (built-in) | `RegisterKeyBindAsync(Key, ModifierKeys, Callable)` (26) |

`UEHelpers.lua` prerequisite surface (23 functions, lines 31–265): GetEngine, GetGameInstance, GetGameViewportClient,
GetPlayerController, GetPlayer, GetWorld, GetPersistentLevel, GetGameModeBase, GetGameStateBase, GetWorldSettings,
GetWorldContextObject, GetAllPlayerStates, GetAllPlayers, GetActorFromHitResult, GetGameplayStatics,
GetKismetSystemLibrary/Math/String/Text, GetGameMapsSettings, FindFName/AddFName/FindOrAddFName, GetUEHelpersVersion.

### 8c. Runtime config/registration inputs (retained)
- `UE4SS-settings.ini`: `DefaultExecuteInGameThreadMethod = EngineTick`, `DefaultFNameToStringMethod = Scan`, `bUseUObjectArrayCache = false`, `SecondsToScanBeforeGivingUp = 120`, `ConsoleEnabled = 1`, empty `ModsFolderPath`/`ControllingModsTxt` overrides.
- `Mods/mods.txt` + `Mods/mods.json`: enable/disable registry for the eight example mods incl. pinned built-in `Keybinds : 1`.
- `UE4SS_Signatures/StaticConstructObject.lua`: signature/AOB-based function resolution script (engine-binding input).
- `BPModLoaderMod/load_order.txt`.

---

## Census gaps (explicit, per hard rules)

1. **No third-party native gameplay DLL mod is retained.** The only retained PE DLLs are the two members of
   `ue4ss-v1.zip` (its own `dwmapi.dll` proxy and `ue4ss/UE4SS.dll`). Windows-DLL *mod* requirements therefore
   rest on the UE4SS distro's own import census, not on a downloaded gameplay DLL mod.
2. Movement-speed/performance/Leon IoStore containers are retained as archives (and two extracted performance
   members); no loose-file extraction of their `.ucas/.utoc` payload exists — container content facts belong to
   worker-6's container handlers, not this census.
3. The file `claude-takeover-43a96969/source/examples/example-lua-mod/files/ItemSpawner/scripts/main.lua` is a
   built example, NOT Nexus mod 95; it must not be cited as the representative mod (the real script is the hash-verified
   member-00000000.bin above).
4. `ReadMe.txt` members of movement-speed/performance zips are retained inside the archives only (hashes in intake
   receipts); their text was not extracted this pass — no requirement depends on them.
5. No retained Skyrim/other-engine mod corpus exists yet; engine-adapter breadth claims cannot cite this census.
