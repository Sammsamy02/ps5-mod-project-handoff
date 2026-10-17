# LUA-API-MATRIX.md — worker-itemspawner-lua-audit

2026-10-04. Mod bytes audited (unchanged):
`outputs/2026-10-04-zcode-team/worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua`
(21415 B, sha256 `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`, manifest match;
below cited as IS:<line>) and `.../bundle/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua`
(9962 B, sha256 `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7`; cited as UH:<line>).

## Runtime sources audited (two compositions exist — both cited)

- **PORT (composed console runtime)** = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss` (the coherent UE4SS PS5 port composed into `coherent-tick-file-witness-runtime.o`, 360 runtime + 3 bootstrap units; per worker-3 INTEGRATION-MAP §4). Cited `PORT/...:line`.
- **HOST (bounded host-trial composition)** = `outputs/ps5-mod-bridge/native-host/lua/*.c` + trial runner `.superpowers/native-dependency-spike/item-spawner-runtime-trial-1/callback_runner.c`. This is the only composition that has ever executed the mod.
- Pristine upstream snapshot (reference): `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss`.

Legend: **IMPLEMENTED** = registration + implementation exist in the cited source; any further gate noted. **PRESENT-BUT-STUBBED/GATED** = code present in PORT but no live engine backend or no evidence it functions; **MISSING** = no implementation in that composition.

## 1. Lua-visible globals used by main.lua

| API (IS:line) | PORT (console) | HOST (trial) | Notes / gate |
|---|---|---|---|
| `print` (IS:6,189,194,335,342,501,636,643,746) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:1521` (`LuaLibrary::global_print`) | IMPLEMENTED — `callback_runner.c:142-170` (bounded_print) | Console log routing percomposition; OBSERVABLES path-agnostic |
| `require("UEHelpers")` (IS:15) | IMPLEMENTED — real luamod file functions (INTEGRATION-MAP §1 Stage 5; UE4SSProgram.cpp:1742-1748 mod list) | IMPLEMENTED — `outputs/ps5-mod-bridge/native-host/lua/module_loader.c:185` (`lua_setglobal(L,"require")`, approved-inventory only) | Host trial: UEHelpers in approved inventory; "no require error" is NOT proof it resolved (UH pcall at IS:15) |
| `RegisterKeyBind(Key.*, cb)` (IS:694-729) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:1915`; `Key`/`ModifierKey` tables `PORT/src/Mod/LuaMod.cpp:614,626`; upstream dispatch `Input::Handler` | IMPLEMENTED — `outputs/ps5-mod-bridge/native-host/lua/input_registry.c:74` | 6/6 registrations host-proven (findings.md). **Delivery gate: pad provider has no hardware run; bundle stages NO `ue4ss/PS5Pad.ini`** (bundle.json files list) |
| `ExecuteInGameThread(fn)` (IS:695,700,705,710,716,725) | PRESENT-BUT-GATED — registered `PORT/src/Mod/LuaMod.cpp:4690`; EngineTick queue `:4478-4489`; ProcessEvent refuses non-game-thread `:4421`; availability `PORT/src/UE4SSRuntime.cpp:10` | **MISSING (empirically nil)** — F7 callback died at IS:695 `attempt to call a nil value (global 'ExecuteInGameThread')` (findings.md; RUNTIME-REQUIREMENTS baseline) | THE single observed hard gate. Needs genuine `UGameEngine::Tick` identity publish (worker-3 S3) |
| `RegisterLoadMapPreHook(fn)` (IS:731, pcall-guarded) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:2711` | MISSING (pcall silently declines — harmless) | Optional; resets UI/items on map change |
| `FindFirstOf(class)` (IS:21,64) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:1632` → `Unreal::UObjectGlobals::FindFirstOf` | MISSING | Needs resolved GUObjectArray (worker-1 live module + published layouts) |
| `FindAllOf(class)` (UH:72) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:1667` | MISSING | UEHelpers only |
| `StaticFindObject(path)` (IS:44,208,241,283,286,294,499; UH:22) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:1529` | MISSING as a real lookup (name exists only as a test fixture label, `native-host/lua/owned_binding_tests/legacy_bool_driver.c:210`) | `Default__` CDOs and `/Game/...` classes need FName pool + GObjects |
| `StaticConstructObject(class, outer, FName)` (IS:50,516) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:2200` | MISSING | Engine callable; needs worker-2 address/offsets |
| `FName(string[, EFindName])` (IS:50,516; UH:252-270) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:6191`; `EFindName` `:698-702`; `NAME_None` `:6193`; native ctor ABI corrected (checkpoint `fname_native_constructor_abi`) | MISSING | NamePool facts from worker-2 |
| `FText(string)` (IS:434,469) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:6200` | MISSING | Backing text construction engine-gated |
| `LoadAsset(path)` (IS:285) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:3618` (UAssetRegistry route; throws off-game-thread `:3624`) | MISSING | Fallback only; 6 AlwaysCook classes likely resident without it |
| `CreateInvalidObject` (UH:15,35,45,...) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:1524` | IMPLEMENTED — `outputs/ps5-mod-bridge/native-host/lua/invalid_object.c:128` | Null UObject metatable (IsValid→false etc.) |
| `ModRef:Get/SetSharedVariable` (UH:18,23) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:6306-6307` + `PORT/src/LuaType/LuaModRef.cpp:61,170` | MISSING | UEHelpers only |
| `UnrealVersion:IsBelow` (UH:198,200) | IMPLEMENTED — `PORT/src/Mod/LuaMod.cpp:6184` (check fns `:6073-6095`) | MISSING | UEHelpers only |
| `ExecuteAsync/ExecuteWithDelay/LoopAsync` | IMPLEMENTED upstream — `PORT` retains (`LuaMod.cpp:3618`-region; upstream `LuaMod.cpp:3053,3074,3105`) | IMPLEMENTED — `outputs/ps5-mod-bridge/native-host/lua/async_queue.c:107-109` | **Not used by this mod** (modreqs §E). Caller-pumped; NOT an ExecuteInGameThread substitute |
| `NotifyOnNewObject` | IMPLEMENTED — upstream `LuaMod.cpp:808/834/860/2303` (retained in PORT) | MISSING | **Not used by this mod** |

## 2. Reflection / object services used by main.lua

| Capability (IS:line) | Classification | Gate |
|---|---|---|
| `obj:IsValid()` (IS:10; UH:19...) | IMPLEMENTED (PORT null/object metatables; HOST `invalid_object.c:29-34`) | Genuine validity needs live objects; null leaves always false |
| Property read incl. nested (`pc.Pawn` IS:31; `pickup.PickupInventoryItem.ItemId` IS:313; `Engine.GameViewport` UH:59) | PRESENT-BUT-GATED — upstream LuaUObject property walking retained in PORT | Needs FProperty offsets for SHIPPING UE5.6 (worker-2); unknown widths rejected (`NativeLayoutWidths.hpp`) |
| Property write (`PickupAmount` IS:325, `bAutoDestroyOnPickUp` IS:326, `WidgetTree`/`RootWidget` IS:525/528, `Font.Size` IS:470, `ShadowOffset` IS:483) | PRESENT-BUT-GATED — same | Same + struct-field write support |
| UFunction invocation 0-12 args (`GetPawn/K2_GetPawn` IS:32-33, `K2_GetActorLocation` IS:233, `GetControlRotation` IS:234, `GetComponentByClass` IS:208, game DB fns IS:163,168, UMG setters IS:364-561, `SetEnabled` IS:327, `SetActorHiddenInGame` IS:329, `SetActorEnableCollision` IS:330, `K2_DestroyActor` IS:345, `SetIgnoreLook/MoveInput` IS:450-451, `ToString` IS:59) | PRESENT-BUT-GATED — ProcessEvent route retained in PORT; dispatch thread-guarded (`LuaMod.cpp:4421`) | Callable addresses unproven live (modreqs B4); ProcessEvent params need struct layouts |
| **Out-param marshalling** `LineTraceSingle(...,hit,...)` then `hit.ImpactPoint/.Location/.ImpactNormal` (IS:245-269) | PRESENT-BUT-GATED / highest-risk item | modreqs B5: out-param lifetime work exists, no engine-backed implementation ever executed |
| Lua-table→engine struct (Vector IS:247-275, LinearColor IS:243/365/479/547/557, Transform IS:273-277, Vector2D IS:483/540, Margin IS:460/544, Anchors IS:534, ignored-actor array `{pawn}` IS:251) | PRESENT-BUT-GATED | modreqs B6: exact widths 55 stageable / 8 NO_WIDTH / 9+1 NO_PREDICATE (worker-layout-batch3 dryrun) |
| Engine-struct field reads (`.Yaw` IS:234, `.X/.Y/.Z` IS:237-238) | PRESENT-BUT-GATED | modreqs B7 |
| FName→Lua string (`str()` IS:57-61; settings `DefaultFNameToStringMethod=Scan`) | PRESENT-BUT-GATED | modreqs B8: Scan route + Kismet `Conv_NameToString` fallback retained, unproven live |
| Weak-ptr `.Get()` (`HitResult.Actor:Get()` UH:199-203) | PRESENT-BUT-GATED | UEHelpers only; versioned FHitResult field (UnrealVersion gates 5.0/5.4 variants) |

## 3. UI mechanism (answers "GUI/console calls": there are NONE)

The mod uses **no GUI module, no console object, no cooked widget asset, no popup notification API**. Its entire UI is a UserWidget constructed at runtime by reflection: `WidgetBlueprintLibrary:Create` (IS:510) or `StaticConstructObject(UserWidget)` fallback (IS:516), a hand-built `WidgetTree/CanvasPanel/Border/VerticalBox/TextBlock` tree (IS:524-628), made visible with `AddToViewport(9999)` (IS:631) + `SetVisibility(0)` (IS:655; 0=Visible, 1=Collapsed used to hide IS:632,660), anchored top-right (IS:534-541). Prerequisites: live PlayerController (IS:493), `/Script/UMG.Default__WidgetBlueprintLibrary` (IS:499), the six `/Script/UMG.*` classes, and working FText + struct setters.

## 4. UEHelpers.lua verdict

Shared dependency is **API-compatible with the PORT** (every global it touches is registered there; see rows above) and is safely inert on the HOST composition except `CreateInvalidObject`/`require`/`IsValid` (null leaves). Its runtime value is zero until live object lookups work. Its pcall-heavy callers in main.lua (IS:15-19) mean a non-functional UEHelpers degrades silently to `FindFirstOf("PlayerController")`. Note: `require("UEHelpers")` must match an approved-inventory module name on the host path (`module_loader.c:94-105`); the staged layout `Mods/shared/UEHelpers/UEHelpers.lua` is the retained distro asset (bundle.json source path) and the inventory already approved it in the trial (findings.md).
