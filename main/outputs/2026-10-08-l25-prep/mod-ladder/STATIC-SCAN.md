# STATIC-SCAN — per-mod UE4SS API census vs the composed PS5 port

Agent-FT, 2026-10-10. ANALYSIS ONLY (Mac on battery; no builds, no console, no tree
edits other than this file). Inputs: every one of the 12 candidate mods read in full
or grep-censused with line numbers (TREE-B LF + TREE-A CRLF; hashes re-verified
against MANIFEST.md this pass), the resident shared libs (`UEHelpers.lua`,
`jsbProfi.lua`), plus 2 extra mod sources found beyond the 12 (see §E). Port
registration surface verified by direct grep of the composed console runtime
candidate tree `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (L26-R4,
post-FL/FM/FK/FR/FW edits — line numbers below are CURRENT tree lines, which have
drifted from FO's manifest cites) PLUS the port-tree translation units the build
actually composes (verified via `2026-10-08-l25-build-variants/l25-A/pass1/
COMPOSITION-RECEIPT.json`: `lua-luaxproperty`, `lua-luafoutputdevice`,
`lua-luamodref`, `lms-luamadesimple`, `luaraw-liolib/loslib/linit` etc. resolve to
`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/...`
and `ue4ss-stdio-consumer-adaptation-1/patched/liolib.c`).

Agent-FS's `API-PARITY-TABLE.md` was NOT present at scan time (poll-read twice);
per task order this scan's own grep of the registration surface is the parity
census. FL's `FL-CONVERSION-INVENTORY.md` (+ its postscript) was consumed.

## 0. Mapping vocabulary

- **SUPPORTED** — console-proven tonight by TownfallItemSpawner (package-v3).
  Evidence set: FindFirstOf/FindAllOf, StaticFindObject (long + short names),
  RegisterKeyBind + ExecuteInGameThread game-thread dispatch, reflected
  `obj:Fn(...)` calls (incl. `WidgetBlueprintLibrary:Create` spawner:510,
  `K2_GetActorLocation` FVector return spawner:233,
  `BeginDeferredActorSpawnFromClass` spawner:295 + `FinishSpawningActor`
  FTransform param spawner:303, `GetItemIDFromDeprecatedEnum` FName return
  spawner:163), StaticConstructObject(Class,Outer,FName) spawner:50/516,
  FName/FText constructors, SetText(FText) spawner:434/469, IsValid, LoopAsync,
  print, GetFullName.
- **BRIDGED-UNTESTED** — implemented and routed through the FL/FD/FR/FW hand
  converters (fname/fstring/ftext marshaling families) but with zero console
  firings.
- **BOUND-UNTESTED** — registered in the composed runtime, never run on console.
- **MISSING** — not in the composed runtime at all (global, member, or overload).
- **CRASH-LAW** — FindFunction/bpfn/class-children enumeration. Re-verified this
  pass across all 18 source files (12 mods + sidecars + shared libs + extras):
  **zero hits** → no mod takes the auto-SKIP on crash-law grounds.
- **PAD-GAP** — not a binding gap: keybind whose PS5Pad.ini chord row is absent.

## 0.1 Registration-surface facts established this pass (corrections to FO)

FO's manifest grepped `LuaMod.cpp` only and an older tree revision. Against the
CURRENT composed runtime:

1. **FO (c2) claims that are actually BOUND** (live in LuaType TUs FO never
   grepped; all compiled per COMPOSITION-RECEIPT):
   - `IsClass()` — LuaUObject.hpp:638 (set.lua:25 works).
   - `Reflection():GetProperty(name)` — LuaUObject.hpp:582-616, with the `[LFB]`
     bridged property-name key (FNAME_Find).
   - `Property:ImportText(...)` — port
     `native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaXProperty.cpp:215`.
   - `Property:ContainerPtrToValuePtr(...)` — same file :186 (returns
     lightuserdata).
   - `Property:IsValid()` — null-safe on the RemoteObject base (LuaMadeSimple
     `LuaObject.hpp:171`), so the null-property chain in set.lua is guarded.
   - All seven `Keybinds` tool globals are REGISTERED as real (not stub)
     registrations: DumpAllObjects LuaMod.cpp:2420, GenerateSDK :2431,
     GenerateLuaTypes :2442, GenerateUHTCompatibleHeaders :2453, DumpStaticMeshes
     :2459, DumpAllActors :2464, DumpUSMAP :2469, DumpJMAP :2479. FO's "other five
     absent" is stale for this tree.
2. **FO (c2) claim that is actually BOUND**: BPModLoaderMod does NOT use a
   `Dirs.*` global (FO misread); it calls `IterateGameDirectories()` (main.lua:115,
   LuaMod.cpp:3270) and `CreateLogicModsDirectory()` (main.lua:122, :3604). Also
   `RegisterBeginPlayPostHook` main.lua:275 = LuaMod.cpp:3186, and
   `RegisterLoadMapPostHook` main.lua:292 = :3075 — both bound.
3. **Stdlib is fully OPEN**: port `LuaMadeSimple.cpp:683` calls `luaL_openlibs`;
   `linit.c` opens base, package+require, coroutine, table, **io**, **os**, string,
   math, utf8, **debug**. `io` is the Sony-native adapted lib
   (`ue4ss-stdio-consumer-adaptation-1/patched/liolib.c`). So `io`/`os`/`debug`
   are BOUND-UNTESTED (not MISSING); the open question is console path semantics
   and writability, which is exactly P0's `type(io)` + the settings-write probe.
   No mod uses `json` (verified: zero hits).
4. **Property-read shape (2026-10-10 FR parity revert)**: NameProperty/TextProperty/
   StrProperty reads return **FName/FText/FString USERDATA** (upstream shape),
   with `:ToString()` bridged (LuaFName.cpp:169, LuaFText.cpp:223,
   LuaUnrealString.hpp via push_fstring_to_lua) and FName/FText `__tostring`
   landed (LuaFName.cpp:56, LuaFText.cpp:76). FString userdata `__tostring` is
   STILL MISSING (empty setup_metamethods — agent-FW TODO; `:ToString` is fine).
   Set branches accept plain Lua strings (FD-landed, bridged).
5. **`IsA(string ObjectFullName)` overload exists** — LuaUObject.cpp:2482-2511
   (bridged StaticFindObject path). MiniMap :381/:386 and ModMenu :1516 use it.
6. **New seam finding (feeds FW)**: `ImportText`'s buffer conversion is
   `ensure_str(lua.get_string())` (port LuaXProperty.cpp:226) — the one remaining
   unrouted Lua-visible wide conversion in the set.lua path. ASCII values work;
   non-ASCII is the seam. Not in FL's inventory (FL's scope was the candidate tree
   + selected port headers; LuaXProperty.cpp was outside both).
7. **UObject `__eq`** (uobject_equal_implementation, LuaUObject.cpp:2515+) and
   FName `Equal` metamethod exist — object identity and FName equality compares
   are BOUND/BRIDGED-UNTESTED respectively.
8. **Signature mismatch (latent)**: port `ExecuteAsync(LuaFunction)` only
   (LuaMod.cpp:3675, upstream-identical). ModMenu's fallback branches call
   `ExecuteAsync(delayMs, fn)` (2-arg, the UE4SS-experimental signature) at
   :887/:1005/:2453/:2566/:2644 — would throw if ever reached. Dead while
   `ExecuteInGameThreadWithDelay` is registered (it is, :6093). `ExecuteWithDelay`
   (LuaMod.cpp:3696) DOES match (integer, LuaFunction).
9. **Key table coverage**: every key referenced by the mods verified present
   (F3, Y, U, I, INS, NUM_THREE, NUM_FIVE..NUM_NINE, J, H, BACKSPACE, UP_ARROW,
   DOWN_ARROW, RETURN). `ModifierKey` = SHIFT/CONTROL/ALT only (LuaMod.cpp:851).

## Per-mod scans

Line numbers: TREE-B LF copies for mods 1-5, 8-12; TREE-A CRLF for 6-7 (hashes
re-verified: MiniMap d8f8d64f…, ModMenu 2ff1fa53… — identical to MANIFEST rows).

### 1. LineTraceMod — RUN-1

| item | mod cite | mapping |
|---|---|---|
| `require("UEHelpers")` | main.lua:2 | resident lib (loadlist row 9962) — SUPPORTED (lib load path) |
| UEHelpers.GetKismetSystemLibrary/MathLibrary/PlayerController | :5-7 | resident lib: CreateInvalidObject (LuaMod.cpp:1754), ModRef:Get/SetSharedVariable (port LuaModRef.cpp:170+), StaticFindObject (proven), FindAllOf (proven), IsValid (proven) — SUPPORTED |
| `UnrealVersion:IsBelow(5,0/5,4)` | :26/:28 | BOUND-UNTESTED (table LuaMod.cpp:7689 region, pure-Lua-side method) |
| property reads `Pawn`/`PlayerCameraManager` | :38-39 | SUPPORTED (same read family as spawner) |
| `GetCameraLocation`/`GetCameraRotation`/`GetForwardVector`/`Multiply_VectorInt`/`Add_VectorVector` | :40-42 | SUPPORTED (reflected calls; FVector return/param family proven spawner:233) |
| `LineTraceSingle` 12-arg + **HitResult struct OUT table** + bool return | :56-71 | **BOUND-UNTESTED (probe-first P5)** — out-param table keys routed `fname-narrow` (LuaUObject.cpp:385-417) but struct-OUT fill never fired on console |
| `HitResult.HitObjectHandle.ReferenceObject:Get()` weak-object unwrap | :27/:29/:31 | BOUND-UNTESTED (port LuaFWeakObjectPtr.cpp:52 `Get`) |
| `IsValid`/`GetFullName`/`print` | :12/:14/:16/:18/:73 | SUPPORTED |
| `RegisterKeyBind(Key.F3)` | :79 | SUPPORTED binding; **PAD-GAP** (needs PS5Pad.ini chord row from free set) |
| std-lib: `string.format` | :14/:18/:73 | stdlib open — SUPPORTED (pure Lua) |

**PREDICTED FIRST FAILURE**: `LineTraceSingle` at main.lua:56 — first struct-OUT
marshal on console. If the OUT table is not populated, the return-bool path may
still "work" but `GetActorFromHitResult` (:72) then indexes `HitResult.HitObjectHandle`
on an empty table → uncaught Lua error in the keybind callback (no crash; error
text names the line). Offline fix: none in mod (never edited) — P5 probe step is
the gate; if P5 shows unfilled OUT tables, the fix is in the candidate's
out-param table construction (LuaUObject.cpp:385-417 region) before RUN-1.

### 2. CheatManagerEnablerMod — RUN-2

| item | mod cite | mapping |
|---|---|---|
| `RegisterHook("/Script/Engine.PlayerController:ClientRestart", fn)` | :3 | **BOUND-UNTESTED** (LuaMod.cpp:5705; hook-path name bridged per Claude item 2; P6) |
| `self:get()` param unwrap | :4 | BOUND-UNTESTED (RemoteUnrealParam get, LuaUObject.cpp:2744) |
| property reads `CheatManager`/`CheatClass` | :6/:11 | SUPPORTED (read family) |
| `IsValid` | :6/:12/:18/:24 | SUPPORTED |
| `StaticFindObject("/Script/Engine.CheatManager")` short name | :15 | SUPPORTED (spawner used both forms) |
| `StaticConstructObject(Class, Outer)` 2-arg | :23 | BOUND-UNTESTED (binding :2495; 3-arg FName form is the proven one; 2-arg = same overload minus optional args) |
| `GetAddress` | :25 | SUPPORTED |
| **object-reference property Set** `PlayerController.CheatManager = …` | :27 | **BOUND-UNTESTED** (push_objectproperty Set, LuaUObject.cpp:576-605) |
| `print`, `string.format` | :7/:13/:19/:25/:28 | SUPPORTED |

**PREDICTED FIRST FAILURE**: the `RegisterHook` call itself at :3 — it runs at
script load, so a hook-state registration failure errors S1 before anything else
(P6 probe gates this rung). If registration succeeds, the next risk is the
object-ref property Set at :27 when ClientRestart fires. Offline fix: P6 first;
Set machinery needs no offline change unless P3/P4 family probes expose a
push_objectproperty Set defect.

### 3. ConsoleEnablerMod — RUN-3

| item | mod cite | mapping |
|---|---|---|
| `require("UEHelpers")` | :5 | resident — SUPPORTED |
| `UEHelpers.FindFName("Tilde"/"F10")` → `FName(Name, EFindName.FNAME_Find)` | :12-13 (UEHelpers.lua:251-252) | BRIDGED-UNTESTED (FName ctor fully bridged, LuaFName.cpp:96-135; spawner proved the plain ctor form; FNAME_Find variant never fired — P3) |
| `NAME_None` compare | :27 | BRIDGED-UNTESTED (global LuaMod.cpp:7698 + FName Equal metamethod) |
| `StaticFindObject("/Script/Engine.Default__InputSettings")` | :22 | SUPPORTED |
| `ConsoleKeys` ArrayProperty read | :25 | BOUND-UNTESTED (TArray Index metamethod, port LuaTArray.cpp:59) |
| `#ConsoleKeys` | :29/:41 | BOUND-UNTESTED (Length metamethod :69 → Num()) |
| FName `==` compare | :30 | BRIDGED-UNTESTED (FName Equal) |
| **array growth write** `ConsoleKeys[#ConsoleKeys+1].KeyName = FName` | :36 | **BOUND-UNTESTED — deepest write in the ladder** (TArray NewIndex :64 + struct-field Set, LuaUObject.cpp:796-818/912-934 custom-property/struct-field key machinery) |
| `KeyName:ToString()` | :42 | BRIDGED-UNTESTED (LuaFName.cpp:169, FL-landed) |
| `Engine.GameViewport`/`ViewportConsole` reads | :51-52 | SUPPORTED family |
| `StaticConstructObject(ConsoleClass, GameViewport)` 2-arg | :57 | BOUND-UNTESTED (2-arg form) |
| object-ref Set `GameViewport.ViewportConsole = CreatedConsole` | :60 | BOUND-UNTESTED (push_objectproperty Set) |
| `IsValid`/`print` | :23/:42/:48/:52/:56/:58/:64 | SUPPORTED |
| `ExecuteInGameThread(CreateConsole)` at load | :75 | SUPPORTED (proven dispatch) |
| `RegisterHook` + `UnregisterHook(path, Pre, Post)` unhook-after-first-fire | :79-87 | BOUND-UNTESTED (RegisterHook :5705; UnregisterHook :2324 signature matches (string,int,int); P6 covers register+unregister) |

**PREDICTED FIRST FAILURE**: `ConsoleKeys[#ConsoleKeys + 1].KeyName = KeyName`
at :36 — the array-growth + struct-field Set. An error here aborts RemapConsoleKeys
mid-way (the `for` at :26 has no pcall) and the mod's own acceptance lines (:42
keymap print) never appear. Offline fix: extend the P3/P4 probe template to cover
one growth-write before RUN-3 (the machinery is composed upstream code; no port
edit identified pre-probe). Note: constructing UConsole may have no visible effect
on a PS5 build — acceptance is the :42 print + :60 assignment log, not UI.

### 4. ConsoleCommandsMod — RUN-4 (main + 3 sidecars)

main.lua: `Log()` helper with `type(GlobalAr)=="userdata"` + `GlobalAr:type()=="FOutputDevice"`
(:5) and `GlobalAr:Log(Message)` (:6) — FOutputDevice userdata BOUND-UNTESTED
(port LuaFOutputDevice.cpp:54 `Log`); `require` of the 3 sidecars (:10-11) —
package/require stdlib open. `print` :4 SUPPORTED.

set.lua:
| item | mod cite | mapping |
|---|---|---|
| `RegisterConsoleCommandHandler("set", fn)` | set.lua:1 | BOUND-UNTESTED (LuaMod.cpp:3929; never fired) |
| `EObjectFlags.RF_ClassDefaultObject \| RF_ArchetypeObject` | :11 | BOUND-UNTESTED (table LuaMod.cpp:897; Lua 5.4 integer bitor — stdlib fine) |
| `FindObject(nil, name, nil, flags)` | :18 | BOUND-UNTESTED (LuaMod.cpp:4020) |
| `ObjectOrClass:IsClass()` | :25 | **BOUND** (LuaUObject.hpp:638) — correction vs FO |
| `Reflection():GetProperty(PropertyName)` | :26/:40 | **BOUND** (LuaUObject.hpp:582-616, bridged name key) — correction vs FO |
| `Property:IsValid()` | :27/:41 | **BOUND** (RemoteObject base IsValid, null-safe) — correction vs FO |
| `FindObjects(0, class, nil, nil, flags, false)` | :28 | BOUND-UNTESTED (LuaMod.cpp:4192) |
| `Property:ImportText(NewValue, Property:ContainerPtrToValuePtr(Object), 0, Object)` | :30/:42 | **BOUND** (port LuaXProperty.cpp:215/:186) — correction vs FO. CAVEAT: ImportText buffer still `ensure_str` (LuaXProperty.cpp:226) — unrouted seam for non-ASCII values (new FW-queue item) |
| `#Parameters`, `Parameters[i]`, `string.format` | :3/:7-9 | stdlib — SUPPORTED |

summon_unloaded_assets.lua: `RegisterConsoleCommandHandler("summon", …)` :1
BOUND-UNTESTED; `LoadAsset(Parameters[1])` :5 BOUND-UNTESTED (LuaMod.cpp:3970).

dump_object.lua (runs at require time):
| item | mod cite | mapping |
|---|---|---|
| `StaticFindObject` ×4 (incl. `/Script/CoreUObject.Class`) | :30-33 | SUPPORTED — **executes at load** |
| `PropertyTypes.*` (17 entries) | :42-129 | BOUND-UNTESTED (table LuaMod.cpp:1310-1347, all referenced entries present) |
| `Property:IsA(PropertyTypes.X)` | :42+ | BOUND-UNTESTED (LuaXProperty.cpp:136) |
| `Object[Property:GetFName():ToString()]` dynamic read | :43+ | BRIDGED-UNTESTED chain (GetFName → FName userdata → :ToString) + property read by string key (SUPPORTED family) |
| `Property:GetInner()` / `:GetStruct()` / `:GetEnum()` / `:GetFieldMask()` / `:GetByteMask()` / `:GetPropertyClass()` | :68/:72/:109/:122-126/:186 | BOUND-UNTESTED (port LuaXArrayProperty.cpp:55, LuaXStructProperty.cpp:57, LuaXEnumProperty.cpp:55, LuaXBoolProperty.cpp:57-75, LuaXObjectProperty.cpp:57) |
| `Value:GetArrayNum()/GetArrayMax()/ForEach` | :78-91 | BOUND-UNTESTED (port LuaTArray.cpp:87/:95/:121) |
| `Elem:get()` | :89 | BOUND-UNTESTED (RemoteUnrealParam get) |
| Name/Str/Text property reads + `:ToString()` | :54-62/:110-112 | BRIDGED-UNTESTED (userdata reads restored by FR revert; :ToString bridged) |
| `FPackageName.IsShortPackageName(ObjectName)` | :137 | BOUND-UNTESTED (LuaMod.cpp:7827; `ensure_str` forward — narrow-safe for ASCII) |
| `ObjectIn:IsA(UClassStaticClass)` userdata overload | :149 | BOUND-UNTESTED (LuaUObject.cpp:2482 overload #1) |
| `Class:ForEachProperty(...)` / `GetSuperStruct()` | :157/:163/:183/:196/:212/:216 | BOUND-UNTESTED (port LuaUStruct.cpp:96/:57) |
| `Object:ForEachName(...)` + `GetNameByValue(...):ToString()` | :199/:109 | BOUND-UNTESTED (port LuaUEnum.cpp:72/:54) + BRIDGED-UNTESTED ToString |
| `io.open(Parameters[1], "w+")` + `File:write/close` | :309-311 | BOUND-UNTESTED (patched liolib; only on `.txt` save) |

**PREDICTED FIRST FAILURE**: load-time is low-risk (the four StaticFindObjects at
:30-33 are the proven family; a miss returns an invalid object, and the first
`IsA`/`ForEachProperty` use is inside handler bodies). First exercised failure =
`RegisterConsoleCommandHandler` registration for "set"/"summon"/"dump_object"
(set.lua:1, summon:1, dump_object:283) — bound, never fired; then on dispatch,
`GlobalAr:Log` (main.lua:6, port LuaFOutputDevice.cpp:54). Offline fix: P0 env
report (includes `type(io)` + Ar userdata) and the P2 `[P2] q`/`[P2] D` dispatch
pair gate this rung; the documented set-deep-path gap (FO S5 partial) shrinks to
the ImportText `ensure_str` seam only.

### 5. SplitScreenMod — RUN-5

| item | mod cite | mapping |
|---|---|---|
| `require("UEHelpers")` | :1 | resident — SUPPORTED |
| **bool property Set** `bUseSplitscreen`/`bOffsetPlayerGamepadIds` | :13-14 | **BOUND-UNTESTED** (bool Set pusher; runs at load via Init :31) |
| bool property read-back | :15-16 | SUPPORTED family |
| `FindAllOf("PlayerController"/"Controller")` | :23 | SUPPORTED |
| `PlayerController.Player` read + `:IsValid()` | :25 | SUPPORTED |
| `HasAnyInternalFlags(EInternalObjectFlags.PendingKill)` | :25 | BOUND-UNTESTED (LuaUObject.hpp:711 + enum table LuaMod.cpp:917) |
| `Player.ControllerId` read | :26 | SUPPORTED family |
| `GetGameplayStatics():GetFullName()` | :38/:61 | SUPPORTED |
| `ExecuteInGameThread` | :39/:67/:85 | SUPPORTED |
| `CreatePlayer(...)`/`RemovePlayer(...)` reflected GameStatics | :40/:68 | BOUND-UNTESTED (side-effectful reflected calls; single pad on console — acceptance is the :44 "Player %s created." marker) |
| `Pawn`/`RootComponent` reads; `K2_GetComponentLocation/Rotation` | :86-88 | SUPPORTED family (FVector/FRotator returns per spawner:233 precedent) |
| `K2_SetActorLocationAndRotation(..., HitResult, ...)` struct OUT | :92 | BOUND-UNTESTED (P5 family) |
| `RegisterKeyBind(Key.Y/U/I, {ModifierKey.CONTROL})` | :106-110 | SUPPORTED binding; **PAD-GAP** (CTRL chord has no PS5Pad.ini mapping — keys never fire; each PS5Pad.ini edit re-pins its config row) |

**PREDICTED FIRST FAILURE**: the bool property Set at :13 — the first novel write
executes at script load (Init :31), so a Set-machinery defect fails S1 before any
keybind. Offline fix: P3/P4 text/property Set probes gate it; no port edit
identified. Second: the CTRL-chord PAD-GAP (silent no-op, not an error) — the
pad-chord decision (Y/U/I from the free chord set) must be made before launch or
the rung produces no evidence.

### 6. TownfallMiniMap v3.1 — RUN-6 capstone (TREE-A CRLF; 4,271 lines)

Self-contained (no UEHelpers). Census (unique member-call counts in parentheses):

Globals: `StaticFindObject` :237 (SUPPORTED), `FindAllOf` :307/:345/:1450/:2579/
:2745/:2880/:3000 (SUPPORTED), `StaticConstructObject` ×21 :604/:612/:658/:1062/
:1088/:1125/:1132/:1354/:1958/:3132/:3193/:3201/:3203/:3331/:3339/:3342/:3348/
:3618/:3623/:3652/:3715 (2-arg UMG forms — BOUND-UNTESTED for the 2-arg variant;
UMG tree content = same machinery the spawner proved), `pcall(RegisterHook, path,
…)` :515 (BOUND-UNTESTED, guarded), `NotifyOnNewObject` :4248-4249 (BOUND-UNTESTED
LuaMod.cpp:2918, guarded), `LoopInGameThreadWithDelay(1, M.frame)` :4034 and
`(TICK_MS, safeTick)` :4268 (BOUND-UNTESTED LuaMod.cpp:6564 — **P7**; both call
sites check only global existence, not call success — an error thrown inside
propagates uncaught at load), `FName(k[2])` ctor :3269/:3966-3967 and
`FName("MouseWheelAxis")` :3967 (SUPPORTED ctor family — spawner-proved),
`FText(...)` ctor ×15 :1181/:1236/:1986/:3104/:3242/:3254/:3353/:3382/:3630/:3752/
:3780/:3790/:3816 (SUPPORTED), `print` (SUPPORTED).

Reflected member calls (all pcall-wrapped except widget-build internals):
SetVisibility ×90, GetAddress ×38, SetColorAndOpacity ×27, SetText ×13,
K2_GetComponentLocation ×10, GetFName ×10, GetClass ×8, SetRenderTransformPivot ×7,
SetBrushResourceObject ×7, K2_GetActorLocation ×7, SetRenderTransformAngle ×5,
SetRenderScale ×5, SetRenderOpacity ×4, SetFont ×4 (FSlateFontSet struct param
:1139/:3196/:3334/:3626), SetAnchors ×4, AddChildToCanvas ×4, SetZOrder ×3,
SetSize ×3, SetJustification ×3, SetGamePaused ×3 (:3827/:3930/:3946),
IsInputKeyDown ×3 + WasInputKeyJustPressed ×2 (`{ KeyName = FName }` FKey struct
param :3271-3272/:3972-3973/:4043 — deep struct-param marshal, pcall-guarded with
graceful degrade `S.pc = nil` :4044-4046), SetInputMode_GameOnly ×6 /
SetInputMode_GameAndUIEx ×1, GetOwner ×6, GetController ×2 (:1805/:2388 +
`.TownfallSight` read), AddToViewport :1340, RemoveFromParent :583, GetDesiredSize
:1539, SetPosition :590, AddChild :595, GetViewportSize/GetViewportScale ×2 each
(:3837/:4126), SetBrush :2067, SetAutoWrapText, GetSize, IsVisible, `K2_DrawTexture`
:539 (struct params), `BeginDrawCanvasToRenderTarget` :533 / `EndDrawCanvasToRenderTarget`
:543 (canvas/context OUT tables — BOUND-UNTESTED struct-OUT shape beyond P5),
`CreateCanvasRenderTarget2D` :548-549 (reflected static — BOUND-UNTESTED),
`ClearColor = {…}` struct property Set :550. Mapping: reflected-call family =
SUPPORTED precedent for returns/params; every genuinely new SHAPE above
(FKey struct param, font struct param, CRT OUT) = BOUND-UNTESTED, all pcall-wrapped.

`str()` helper :226-229 — `pcall(n:ToString())` with `tostring(n)` fallback →
BRIDGED-UNTESTED (both paths FL-landed: :ToString LuaFName.cpp:169 + __tostring
:56; zero console firings). Chains at :423/:508/:1401/:1435/:1578/:1621/:2291/
:2561/:2591-2592/:2632/:2648/:2656/:2680/:2684/:2704/:2785/:2939/:2941.
`IsA("/Script/Townfall.TownfallDoor")` string overload :381/:386 — BRIDGED-UNTESTED
(LuaUObject.cpp:2504-2511). `GetWorld` :526 — BOUND-UNTESTED (LuaUObject.hpp).
Property reads (SUPPORTED family): `.ItemId` :423/:1435, `.PickupInventoryItem.
ItemName` :2939, `.Verb.DialocTag/DialocLineContent` :2591/:2648/:2684, `.Enabled`
:2785, `.StaticMesh` :2941, `.MapCornerBounds` corners :1424-1426, `.UpperLeftCorner`
struct fields, `.RenderTarget` :542, `arr[i]` array read :2561, `#interactComponents
+ 1` array growth write :4257 (BOUND-UNTESTED growth family), `.SizeX` :536-537.

Stdlib: `debug.getinfo(1,"S")` :242 (guarded `debug and …` — open),
`io.open(settingsPath,"r")` :251 (BOUND-UNTESTED — patched liolib; path built
:241-245 by appending `"\\settings.txt"` — a literal backslash on PS5 likely
fails the open → nil → settings disabled, NON-FATAL on read), `io.open(…,"w")`
:293 assert-guarded (only reached on user save — documented S5 owner),
`os.clock` :2761/:2831 (guarded), `string.*`/`table.*`/`math.*` throughout
(102 pcall sites total).

**PREDICTED FIRST FAILURE**: `LoopInGameThreadWithDelay(TICK_MS, safeTick)` at
:4268 (and the earlier `LoopInGameThreadWithDelay(1, M.frame)` :4034) — the tick
driver is called unguarded at load (existence-checked only). A registration-time
error kills S1; a silent failure leaves the mod inert. P7 gates RUN-6. Offline
fix: P7 probe; if P7 fails, the LIGWD carrier (LuaMod.cpp:6564) is the owner.
Second-tier: the widget-tree SCO + SetText at first frame (2-arg SCO form +
SetText proven-family) and the `str()` → `:ToString()` bridged chains — all
P3-adjacent.

### 7. TownfallModMenu v0.66 — RUN-7 (TREE-A CRLF; 3,198 lines)

`pcall(require,"UEHelpers")` :36 (resident). Globals: `FindFirstOf` :45/:337-338/
:1631/:1677/:1866 (SUPPORTED), `FindAllOf` pcall'd :267/:276/:854/:973/:1294/
:1310/:1328/:2327/:2329 (SUPPORTED), `StaticFindObject` :67/:1505/:3042 (SUPPORTED),
`StaticConstructObject(class(path), outer, FName(name))` :73/:3048 (SUPPORTED —
exact spawner form), `ExecuteInGameThread` :887/:1005/:2440/etc. (SUPPORTED),
`ExecuteInGameThreadWithDelay` :884/:1002/:2048/:2429/:2450/:2563/:2641/:2698
(BOUND-UNTESTED :6093 — P7), `ExecuteAsync(delayMs, fn)` fallback :887/:1005/:2453/
:2566/:2644 (**MISSING SIGNATURE** — port takes (LuaFunction) only; dead branch
while EITWD exists — latent), `ExecuteWithDelay(500, fn)` fallback :2051/:2722
(BOUND-UNTESTED :3696, signature matches), `LoopInGameThreadWithDelay(100,
gameplayTick)` :2431 (BOUND-UNTESTED — P7; type-checked only, call unguarded),
`LoopAsync(100, …)` fallback :2437 (SUPPORTED), `RegisterHook` ×9 sites :1163/
:1234/:1250/:1539/:1559/:2120/:2143/:2602/:1933 — **ALL pcall-guarded** (verified
in the hash-pinned file; FO's "unguarded :1234/:1250/:1539+" cites do not match
this revision — correction), `NotifyOnNewObject` :2037/:2648/:2661/:2675/:2687
(all pcall-guarded; BOUND-UNTESTED), `ForEachUObject` :1515 (pcall-wrapped;
BOUND-UNTESTED :2897), `RegisterKeyBind(Key.BACKSPACE/UP_ARROW/DOWN_ARROW/
RETURN)` :3175-3186 (SUPPORTED binding; co-fires with spawner nav binds — RUN-7
must be a spawner-free boot; pad mapping from free chord set).

Members: `GetAddress` ×23, `get()` ×12 / `set()` ×2 (hook-param unwrap + write
:2124-2149 — BOUND-UNTESTED RemoteUnrealParam set), `ToString` ×10 (BRIDGED-
UNTESTED chains: `GetInventoryItemNameFromID(item):ToString()` :1642 FName return;
`entry.ItemId:ToString()` :1702/:1794/:1807/:1826-1827/:1839-1844 NameProperty
reads; `GetEquippedItemID/GetPreviouslyEquippedItemID` :1905-1906), `GetFullName`
×4/:2082, `GetClass` ×8, `GetWorld` ×3, `IsValid` (via helpers), `IsA(string)`
:1516 (BRIDGED-UNTESTED overload #2), `ProcessConsoleExec("r.Fog 0", nil, pc)`
×9 :2265-2277 (BOUND + FW-bridged cmd — LuaUObject.hpp:735-760, matches the
3-arg shape exactly; zero console firings), `cls:ForEachProperty` :386 (BOUND —
port LuaUStruct.cpp:96) with `prop:GetFName():ToString()` (BRIDGED-UNTESTED)
and dynamic `gait[propName]` read :391 (SUPPORTED family), UMG: `widgetLib:Create
(pc, userWidgetClass, pc)` :3042 (SUPPORTED — exact spawner call), SetText ×2 +
`FText(marker .. label)` :2987/:3015 (SUPPORTED ctor), SetColorAndOpacity/
SetPadding/SetContent/SetBrushColor, `ui.root.WidgetTree = ui.tree` :3053 /
`ui.tree.RootWidget = ui.canvas` :3055 (BOUND-UNTESTED object-ref Set), AI-family
reflected calls (SetAlertState/SetAlertness/ForgetAllPerceptionStimuli/
ResetAttack/StopMovement/LoseAwareness/ResetRunningSense/Deactivate/ClearFocus/
K2_ClearFocus/SetCollisionEnabled/SetGenerateOverlapEvents/SetComponentTickEnabled
— reflected family, BOUND-UNTESTED as never-fired shapes, mostly pcall-wrapped),
fog hooks with `Value:get()`/`Value:set(reduced)` :2124-2149 (BOUND-UNTESTED),
`:ForEach` ×3 (BOUND-UNTESTED port LuaTArray.cpp:121), `II_SimonOS:Construct`
hook via string concat :1933 (RegisterHook string path — bridged).

**PREDICTED FIRST FAILURE**: `LoopInGameThreadWithDelay(100, gameplayTick)` at
:2431 (startGameplayLoop) — like MiniMap, the unguarded load-time loop
registration; P7 gates RUN-7 too. If P7 is green, the first runtime failure is
whichever `RegisterHook` machinery path fires first (P6) — all sites pcall-guarded
so failures degrade to logged misses, not aborts. Offline fix: none pre-probe;
the ExecuteAsync 2-arg fallbacks are documented-dead (do NOT "fix" by adding the
overload without a task order — it's unreachable while EITWD is registered).

### 8. Keybinds — LOAD-ONLY

`Key.*`/`ModifierKey.*` table reads :14-20 (BOUND tables — verified entries J/H/
NUM_FIVE..NUM_NINE); `IsKeyBindRegistered(key, mods)` :25 (BOUND-UNTESTED
LuaMod.cpp:2002); `RegisterKeyBindAsync(key, mods, fn)` :26 (BOUND-UNTESTED
:2064); the seven tool callables :30-36 — **ALL SEVEN REGISTERED** (LuaMod.cpp:
2420-2479, real registrations calling dump/generate machinery — correction vs
FO's "five absent"). Loads clean (S1/S2/S3 achievable). PAD-GAP: CTRL+J/H/NUM5-9
chords need PS5Pad.ini rows.

**PREDICTED FIRST FAILURE**: none at load. On key press: `DumpAllObjects` →
`UE4SSProgram::dump_all_objects_and_properties(output_dir + "\\" + file)` —
Windows path separator + console output-dir semantics (LuaMod.cpp:2423-2425);
DumpStaticMeshes/DumpAllActors route to GUI::Dumpers (headless port behavior
unverified). S5 remains SKIP-by-gap for FUNCTIONALITY, but the binding gap FO
cited is closed in this tree. Offline fix (optional): PS5 path join in the dump
targets, or keep documented-broken.

### 9. BPML_GenericFunctions — LOAD-ONLY

`RegisterCustomEvent("PrintToModLoader")` :29 / ("ConstructPersistentObject") :45
(BOUND-UNTESTED LuaMod.cpp:2986); handlers contain `ParamMessage:get()` :31 +
`Message:type()` :36 + `Message:ToString()` :41 (FString userdata members —
BOUND/BRIDGED-UNTESTED, unreachable without a caller); `ParamContext:get():
GetClass():GetFullName()` :39 (chains of bound members); `StaticFindObject(
"/Script/CoreUObject.Class")` :50 (SUPPORTED); `FindFirstOf("GameInstance")` :58
(SUPPORTED); `Class:IsA(UClassStaticRef)` :53 (BOUND-UNTESTED overload #1);
`StaticConstructObject(Class, GameInstance, 0, 0, 0x0E000000, false, false, nil,
nil, nil)` 10-arg EObjectFlags form :60 (BOUND-UNTESTED — first >3-arg SCO on
console; the :2495 binding's overload handles it but it has never fired);
`OutParam:get()`/`OutParam:set(PersistentObject)` :54/:66 (BOUND-UNTESTED
RemoteUnrealParam set — unreachable without a caller); `Explode` string helper
:14-27 (pure Lua); `print` :10 (SUPPORTED).

**PREDICTED FIRST FAILURE**: none at load — both RegisterCustomEvent calls
register quietly (S1 pass). Handlers can never fire on console (no BP loader, no
custom-event caller). S5 SKIP-by-no-caller stands.

### 10. BPModLoaderMod — SKIP (corrected mechanism)

`package.path` append with Windows separators :10-11 (pure string op — harmless);
`io.open(file,"rb")` :29 / `io.lines(file)` :44 / `dofile(ModFile.__absolute_path)`
:132 (BOUND-UNTESTED patched liolib + stdlib dofile); `UEHelpers.FindOrAddFName`
:21/:135/:199/:203-204 (BRIDGED-UNTESTED FName ctor FNAME_Find/Add pair);
**`IterateGameDirectories()`** :115/:123 (BOUND-UNTESTED LuaMod.cpp:3270 —
correction vs FO); **`CreateLogicModsDirectory()`** :122 (BOUND-UNTESTED :3604);
`Dirs.Game.Content.Paks.LogicMods` table walk :119/:124/:127 (depends on the
IterateGameDirectories result shape); `AssetRegistryHelpers:GetAsset(AssetData)`
:208 + `GetAssetRegistry()` :245 (reflected — will fail: no LogicMods assets on
console); `World:SpawnActor(ModClass, {}, {})` :223 (reflected spawn — c1);
`Actor.PreBeginPlay`/`PostBeginPlay` function-property read + call :228-231/
:281-284 (c1); `RegisterKeyBind(Key.INS)` :269 (SUPPORTED binding; PAD-GAP);
`RegisterBeginPlayPostHook` :275 (BOUND-UNTESTED :3186); `RegisterLoadMapPostHook`
:292 (BOUND-UNTESTED :3075); `ExecuteInGameThread` :270/:296 (SUPPORTED);
`FindFirstOf("Actor")` :297 (SUPPORTED); `UnrealVersion.IsBelow(5,1)` :197
(BOUND-UNTESTED); `IsValid`/`GetFullName`/:ToString() :209-212/:227/:278-280
(SUPPORTED/BRIDGED-UNTESTED).

**PREDICTED FIRST FAILURE**: `IterateGameDirectories()` at :115 — the game-dir
walk through the port's PosixFile layer on PS5. Returns nil → clean error :117;
returns a table → the `Dirs.Game.Content.Paks.LogicMods` nil-chain →
`CreateLogicModsDirectory()` (mkdir in a read-only app dir?) → error :125. Either
way the mod aborts with its own error message. Core function stays unreachable
(no BP paks on console; IoStore retail content) — SKIP verdict unchanged, but
the mechanism is BOUND-UNTESTED IO, not an absent `Dirs` global.

### 11. ActorDumperMod — SKIP (safety law, unchanged)

`RegisterCustomProperty{Name="Actors", Type=PropertyTypes.ArrayProperty,
BelongsToClass="/Script/Engine.Level", OffsetInternal=0x98, …}` :1-9 (BOUND
LuaMod.cpp:2630 — but the hardcoded 0x98 offset is ENGINE-BUILD-SPECIFIC and
unverified for the console Townfall build); `RegisterKeyBind(Key.NUM_THREE,
CTRL)` :11 (SUPPORTED binding; PAD-GAP); `FindFirstOf("Level")` :12 (SUPPORTED);
`Level.Actors` custom-property read :13 (BOUND-UNTESTED custom-property dispatch
— walks the unverified offset); `Actors:ForEach(fn)` :15 (BOUND-UNTESTED port
LuaTArray.cpp:121); `ElemWrapper:get()` :16 (BOUND-UNTESTED);
`GetAddress`/`GetFullName` :18 (SUPPORTED); `print`/`string.format` (SUPPORTED).

**PREDICTED FIRST FAILURE** (if ever run): `Level.Actors` read at :13 — a wrong
offset walks garbage memory → native crash (zzdiag-class), not a Lua error. This
is the offset law, identical to agent-EF finding 6; stock mods.txt ships it
disabled. Never run.

### 12. jsbLuaProfilerMod — SKIP (unchanged verdict, corrected detail)

main.lua is all comments + `require "jsbProfi"` :1. jsbProfi.lua: `io.open(file,
mode)` :69-73 — **nil-safe** (`if not File then return end`), and the load-time
`jsb.write(… session header …)` :115 therefore no-ops silently if the console
path fails → the mod LOADS as a no-op even with broken io (correction to the
risk picture; FO's "loads as a no-op" holds for a different reason than silence);
`os.clock` :95-122/:226-274, `os.date` :98/:109/:115/:183/:189, `debug.getinfo`
:146/:168/:214 (stdlib — OPEN, unverified behavior irrelevant while unused);
`debug.sethook` machinery (only when profiling is actually invoked — hook-budget
risk). shared/Types.lua = pure SDK stub table.

**PREDICTED FIRST FAILURE**: none at load. A Lua-VM profiler on the console
proves nothing about UE4SS generality and risks the hook budget if invoked —
SKIP stands.

## E. Extra mod sources found beyond the 12

Searched every `Mods/` dir and mod-shaped tree under the two staging trees and
`.superpowers/native-dependency-spike/`:

1. **Stamina mod — REAL 13th mod candidate**:
   `real-mod-lua-runner-1/inputs/stamina_main.lua` (106 lines, "Silent Hill:
   Townfall - Infinite Stamina", author Framcecore). API surface:
   `require("UEHelpers")` :7 (resident); `UEHelpers.GetPlayer()` :14;
   `FindFirstOf("TownfallPlayerCharacter"/"TFPlayerGaitComponent"/
   "UTFPlayerGaitComponent")` :20/:38/:41 (SUPPORTED); `IsValid` ×many
   (SUPPORTED); nested property reads `Player.MapComponent.GaitComponent` :30-31
   (SUPPORTED family); `ExecuteInGameThread` :56/:92 (SUPPORTED);
   `UEHelpers.GetKismetSystemLibrary()/GetWorldContextObject()` :57-58;
   `KismetSystem:PrintString(WorldContext, Message, true, false, TextColor, 3.5,
   FName("TownfallStaminaBanner"))` :70-78 — reflected call with a **plain-Lua-
   string FString param** (BRIDGED-UNTESTED — the FD-landed fstring-param push),
   LinearColor struct table, float, FName ctor (SUPPORTED) + FName reflected
   param; `LoopAsync(1500, …)` :89 (SUPPORTED); bool property read+Set
   `GaitComp.bStaminaEnabled` :98-99 (BOUND-UNTESTED bool Set — same family as
   SplitScreen's :13). **PREDICTED FIRST FAILURE**: `PrintString`'s FString
   param from a plain Lua string at :70 (bridged, never fired — same family the
   spawner exercised only via FString userdata/property paths). Lowest-risk real
   mod in the pool — a natural RUN-0.5 if the ladder wants a bridge-family
   cheap win before RUN-1.
2. **ExampleItemSpawner (stub, not a real mod)**:
   `claude-takeover-43a96969/source/examples/example-lua-mod/files/ItemSpawner/
   scripts/main.lua` (17 lines) — print/tostring only, keybind left as a stub.
   Exercises nothing beyond the proven print path; packaging-pipeline fixture.
3. `real-mod-lua-runner-1/inputs/menu_main.lua` / `minimap_main.lua` — sha256
   IDENTICAL to the TREE-A ModMenu/MiniMap copies (verified this pass) — same
   revisions, not new mods. `smoke.lua` — one-line VM probe
   (`print('safe-vm', 2+3, io, os, package, require)`), not a mod.
4. No other `Mods/` directories exist under the staging trees (full
   `find` sweep; the three known trees are the only ones).

## F. AGGREGATE GAP LIST — the offline fix queue

Union of all MISSING / UNTESTED items, ordered by how many mods hit them.
"S/P" column = covered by probe-template step (P0 env, P2 dispatch, P3+P4
text/name roundtrip, P5 struct-OUT, P6 hooks, P7 delay family).

| # | gap | class | mods hit (count) | offline action |
|---|---|---|---|---|
| 1 | FName/FText/FString userdata `:ToString()` + `__tostring` chains | BRIDGED-UNTESTED | ConsoleEnabler:42, dump_object:56/109/112, MiniMap str()×20+, ModMenu×10, BPML:41 (5) | P3 gates; FString `__tostring` still MISSING (FW TODO — no direct caller in the 12, but BPML:41-adjacent) — attach via luaL_getmetatable+lua_setfield pattern |
| 2 | RegisterHook (registration + fire + param unwrap) | BOUND-UNTESTED | CheatManager:3, ConsoleEnabler:79, MiniMap:515, ModMenu×9 (4) | P6 gates; reconcile make_hook_state (LuaMod.cpp:5705+) if P6 red |
| 3 | Struct-OUT param tables (LineTraceSingle HitResult; K2_SetActorLocationAndRotation; CRT canvas/context OUT) | BOUND-UNTESTED | LineTrace:56-69, SplitScreen:92, MiniMap:533 (3) | P5 gates LineTrace shape; CRT OUT shape is one notch deeper (document; consider a P5b in template if RUN-6 stalls there) |
| 4 | Array metamethods: Length `#arr`, Index read, growth NewIndex write, `:ForEach` | BOUND-UNTESTED | ConsoleEnabler:29/36/41, UEHelpers:165 (resident), MiniMap:2561/4257, ModMenu:1794/1826/1839 + :ForEach×3 (4) | P3/P4 template should add ONE growth-write (`arr[#arr+1].field = X`) before RUN-3; no port edit pre-probe |
| 5 | io.* on console (patched Sony-native liolib; path semantics, writability) — incl. `dofile` | BOUND-UNTESTED | MiniMap:251/293, BPModLoader:29/44/132, dump_object:309, jsbProfi:71/115 (4) | P0 `type(io)` + a settings-write mini-probe; io SHIM DECISION: lib exists — the open question is path separators/backslash builds (MiniMap:244 appends `"\\settings.txt"`) and mod-dir writability. Cheapest offline fix if needed: none in port; document + accept non-fatal degradation |
| 6 | RemoteUnrealParam `get()`/`set()` (hook/callback params) | BOUND-UNTESTED | CheatManager:4, MiniMap:502, ModMenu×14 (3) | P6 covers get via the hook fire; `set` only exercised by ModMenu fog hooks — note as RUN-7 acceptance item |
| 7 | Object-reference property Set (`obj.X = obj`) | BOUND-UNTESTED | CheatManager:27, ConsoleEnabler:60, ModMenu:3053/3055 (3) | P3/P4 property-Set family; no port edit pre-probe |
| 8 | LoopInGameThreadWithDelay + ExecuteInGameThreadWithDelay (+ ExecuteWithDelay fallback) | BOUND-UNTESTED | MiniMap:4034/4268, ModMenu:2431 + 8 EITWD sites (2) | P7 gates BOTH big-mod rungs; both mods call it unguarded-at-load (existence check only) — a P7 red means RUN-6/RUN-7 S1 fails at exactly those lines |
| 9 | NotifyOnNewObject | BOUND-UNTESTED | MiniMap:4248, ModMenu×5 (2) | P6-adjacent; all sites pcall-guarded → degrade to missed events, not aborts |
| 10 | FName ctor FNAME_Find variant + FName `==`/NAME_None Equal | BRIDGED-UNTESTED | ConsoleEnabler:12-13/27/30 (via UEHelpers FindFName), BPModLoader:21/135/199-204 (via FindOrAddFName) (2) | P3 roundtrip gates |
| 11 | FWeakObjectPtr `:Get()` | BOUND-UNTESTED | LineTrace:27/29/31, UEHelpers:199-203 (resident) (2) | rides P5 (HitResult path) |
| 12 | IsA(string) overload #2 | BRIDGED-UNTESTED | MiniMap:381/386, ModMenu:1516 (2) | P3-adjacent; both pcall-guarded |
| 13 | StaticConstructObject 2-arg / 10-arg forms | BOUND-UNTESTED | CheatManager:23, ConsoleEnabler:57, MiniMap×21 (2-arg); BPML:60 (10-arg) (4) | same overload family as proven 3-arg; no offline action |
| 14 | RegisterConsoleCommandHandler + handler dispatch (Ar/FOutputDevice:Log) + LoadAsset + FindObject/FindObjects | BOUND-UNTESTED | ConsoleCommands (1 mod, 4 entry points) | P0 (Ar) + P2 dispatch pair gate |
| 15 | Reflection/ImportText/ContainerPtrToValuePtr/IsClass path | BOUND (was FO's "missing") + ImportText buffer `ensure_str` SEAM | ConsoleCommands set.lua (1) | FW queue: route ImportText buffer through the Bridge (port LuaXProperty.cpp:226) — the last unrouted Lua-visible wide conversion in a runnable mod path |
| 16 | ProcessConsoleExec member (FW-bridged) | BOUND-UNTESTED | ModMenu:2265-2277 (1) | no offline action; document as RUN-7 fog-feature dependency |
| 17 | ForEachUObject | BOUND-UNTESTED | ModMenu:1515 (1, pcall-wrapped) | P0 env/optional |
| 18 | IterateGameDirectories / CreateLogicModsDirectory | BOUND-UNTESTED | BPModLoader:115/122/123 (1, SKIP mod) | no action (SKIP) |
| 19 | ExecuteAsync 2-arg (delayMs, fn) overload | **MISSING** (latent, dead branch) | ModMenu:887/1005/2453/2566/2644 (1) | document; do NOT add the overload without a task order (branch unreachable while EITWD registered) |
| 20 | Dump/generate output paths (Windows `\\` joins, GUI dumpers headless) | BOUND but console-broken paths | Keybinds:30-36 (1, LOAD-ONLY) | optional FW cleanup: PS5 path join in LuaMod.cpp:2423-2425/2436/2447 or keep documented-broken |
| 21 | RegisterCustomProperty w/ hardcoded offset | BOUND + SAFETY (offset law) | ActorDumper:1-9 (1, SKIP) | never run; no action |
| 22 | Custom-event machinery (RegisterCustomEvent) | BOUND-UNTESTED | BPML:29/45 (1, LOAD-ONLY) | no action (no caller can exist) |
| 23 | stdlib debug/os use on console (debug.getinfo, os.clock/date; debug.sethook if profiling) | BOUND-UNTESTED (libs open) | MiniMap:242/2761, jsbProfi (2) | P0 `type(io)`/`type(os)`/`type(debug)` rows |
| 24 | PAD-GAP: PS5Pad.ini chord rows for F3; CTRL+Y/U/I; CTRL+J/H/NUM5-9; CTRL+NUM_THREE; BACKSPACE/UP/DOWN/RETURN; INS | launch-config gap (not a binding) | LineTrace, SplitScreen, Keybinds, ActorDumper, ModMenu, BPModLoader (6) | pad-chord budget decision BEFORE launch; ModMenu binds must be spawner-free-boot; no-chord-contains-chord law |

Crash-law re-verification: zero `FindFunction`/`bpfn`/`GetDefaultClass`/
`GetAllClasses` hits across all 18 source files (this pass, per-file grep). No
mod takes the auto-SKIP on crash-law grounds; the three SKIP verdicts
(BPModLoaderMod, ActorDumperMod, jsbLuaProfilerMod) stand on
core-function-unreachable / offset-law / no-generality-value grounds
respectively — with the mechanism corrections recorded in their sections.

## G. Bottom line for the ladder

The port surface is far more complete than FO's manifest represented: the only
outright MISSING item referenced by any mod is the dead ExecuteAsync(delay, fn)
overload. The load-bearing risks are (in order): P7 (LIGWD — kills both capstone
rungs at load if red), P6 (RegisterHook — rungs 2/3 and both big mods), P5
(struct-OUT — rungs 1/5), the array growth-write (rung 3), and the bridged
ToString chains (every rung). The io question is bounded: two of its four
consumers are SKIP mods, MiniMap degrades non-fatally, and only dump_object's
`.txt` save actually depends on writable io.
