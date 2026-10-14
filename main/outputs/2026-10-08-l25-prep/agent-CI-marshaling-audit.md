# Agent-CI — L26 pre-audit: parameter/return marshaling for the mod's call chain

READ-ONLY audit; the only file written is this report. Scope: the layer AFTER
UFunction resolution — once `db:GetItemIDFromDeprecatedEnum` resolves, the call
must still convert Lua values into the reflected param frame and convert
returns/out-params back, and the runtime carries a console-specific
"fname_tostring unbound by design" decision on the FName→string leg.

Primary sources (all line numbers re-verified in this audit, 2026-10-08):

- MOD = `outputs/2026-10-04-zcode-team/worker-menu-chain/fixture/main.lua` (Townfall Item Spawner v1.0)
- PORT = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss` (port root, upstream-shaped bindings)
- UNREAL = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal` (unreal ABI layer)
- SEL = `outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp` (the SELECTED LuaUObject for the l24→l26 composition; sha 48aa5605354a649462f090615546d043dc268c0b06ec0a7dd226964e5a1a0638)
- INIT = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp` (current composed initializer)
- CLMOD = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp` (current composed LuaMod, R3 marshal)
- UEHelpers = `outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/UEHelpers.lua` (9,962 B — matches task hint)

All paths below are relative to the worktree root
`/WORKSPACE/.codex/worktrees/translation-core/what/`.

---

## 0. How a UFunction call marshals (the machinery under every row)

One function does all Lua↔engine conversion for calls:
`call_ufunction_from_lua` — PORT/src/LuaType/LuaUObject.cpp:151-410 (Prospero
repackaged at :135-149/:412-435 with a fixed out-param collector, capacity
error at :120-126).

1. Exact param-count check (:218-232): `GetNumParms()` minus return must equal
   the Lua stack size or `throw_error` — loud.
2. Param fill loop (:236-321): per `CPF_Parm` property, dispatch by the
   property CLASS FName's comparison index through
   `StaticState::m_property_value_pushers` (map declared at
   PORT/include/LuaType/LuaUObject.hpp:356; registered in
   PORT/src/UE4SSProgram.cpp:1702-1733 — Object/Class/Int8/16/32/64/Byte/
   UInt16/32/64/Struct/Array/Set/Map/Float/Double/Bool/Enum/WeakObject/Name/
   Text/Str/SoftObject/SoftClass/Interface/Delegate/Multicast*).
   No handler → `throw_error` "parameter ... not supported" (:311-319) — loud.
3. Out-params (`CPF_OutParm`, non-const, non-return) store a Lua table ref
   (:269-296); non-struct/non-array outs skip the Set pusher, struct/array outs
   still marshal the input table in.
4. `calling_context->ProcessEvent(func, frame)` (:327) — the engine call.
5. Out tables refilled (:332-377, `GetNonTrivialLocal`, `reuse_same_table` for
   struct/array/set), then return value pushed (:379-405). Unhandled return
   type → `throw_error` (:397-402) — loud.

The frame is a FIXED 512-byte buffer `DynamicUnrealFunctionData::data` (size
stated in `outputs/2026-10-08-menu-marshal-bounds/REPORT.md`; the selected
file guards it: SEL:199-235 refuses `GetParmsSize() > capacity` and any
parameter whose offset/extent escapes the declared frame BEFORE any pusher or
ProcessEvent — this bounds preflight IS integrated in the selected unit).

Property get/set (non-call) goes through `__index`/`__newindex` →
`prepare_to_handle` (PORT/include/LuaType/LuaUObject.hpp:502-510, :791-840)
which resolves the member name via `FName(name, FNAME_Find)` (:821) and lands
in the same pusher map via `handle_unreal_property_value`
(PORT/src/LuaType/LuaUObject.cpp:2303-2370). Nested struct fields chain through
LuaUScriptStruct (`__index`/`__newindex` PORT/src/LuaType/LuaUScriptStruct.cpp:58-66;
`prepare_to_handle` :206-221 — returns nil, not an error, when the FName
lookup yields NAME_None at :213-218).

Key behavioral facts verified this pass:

- `get_userdata<T>` performs NO metatable type check
  (PORT/first/include/LuaMadeSimple/LuaMadeSimple.hpp:752-785): wrong-type
  value at the index → invalid internal-type marker → `std::runtime_error`
  ("See traceback", :777-784). So passing a plain Lua string where FName
  userdata is required is a LOUD error (pcall-visible), never silent garbage.
- `push_nameproperty` Set REQUIRES `LuaType::FName` userdata
  (PORT/src/LuaType/LuaUObject.cpp:1777-1781). There is no string branch.
- `push_strproperty` Set accepts a plain Lua string OR FString userdata
  (:1837-1852).
- `convert_lua_table_to_struct` leaves fields DEFAULT (zero) when the Lua
  table key doesn't match a reflected field — silent, no error
  (:671-676 + :695-705 skip-with-verbose-log).
- Pusher integer/bool/enum Set paths accept plain Lua numbers/booleans
  (push_integer PORT/include/LuaType/LuaUObject.hpp:974-1001;
  push_boolproperty PORT/src/LuaType/LuaUObject.cpp:1590-1594;
  push_enumproperty Set :1666-1688).

---

## 1. Task 1 — Every engine-boundary conversion in the mod's chain

### 1.1 db scan (the proving-ladder goal)

- `db:GetItemIDFromDeprecatedEnum(entry.value)` — MOD:162-165. Param: plain
  Lua integer (4..212) → ByteProperty or IntProperty or EnumProperty (actual
  reflected type UNMEASURED — see §2/§5 #28). Marshals via
  push_integer<uint8_t>/int32 or push_enumproperty Set — all accept integers.
- Return: id → if StrProperty: FString userdata; if NameProperty: FName
  userdata; if TextProperty: FText userdata. MOD wraps it with `str()`
  (MOD:57-61): string passes through, userdata → `:ToString()`.
- `db:GetInteractablePickupIDActorPath(id)` — MOD:167-169. Param: `id` is a
  PLAIN Lua string after `str()`. If the reflected param is StrProperty →
  push_strproperty Set string branch (bound). If it is NameProperty →
  runtime_error (see §0 get_userdata) → pcall at MOD:167 catches → entry
  dropped. If TextProperty → push_textproperty Set needs FText userdata
  (PORT/src/LuaType/LuaUObject.cpp:1807-1810) → wrong-type runtime_error →
  same silent drop.
- Return: path → `str()` → plain string; only used in Lua (normalizeClassPath,
  keys). No engine read-back.

### 1.2 player/ground math (spawn prelude)

- `pc.Pawn` / `pc:GetPawn()` / `pc:K2_GetPawn()` — MOD:30-39. ObjectProperty
  Get (push_objectproperty :515-518) / UObject* return
  (auto_construct_object :438-482).
- `pawn:K2_GetActorLocation()` — MOD:233. StructProperty return →
  convert_struct_to_lua_table (:727-814) → `{X,Y,Z}` plain numbers
  (FloatProperty pusher :1513-1540 GetNonTrivialLocal).
- `pc:GetControlRotation().Yaw` — MOD:234. Same path (FRotator table).
- `system:LineTraceSingle(pawn, {X,Y,Z}, {X,Y,Z}, 0, false, {pawn}, 0, hit,
  true, transparent, transparent, 0)` — MOD:245-258. 12 params exact
  (count check :218-232): ObjectProperty (WorldContext=pawn), 2× StructProperty
  (FVector tables), byte/enum ints (TraceChannel, DrawDebugType), BoolProperty
  (bTraceComplex), ArrayProperty (ActorsToIgnore `{pawn}` → table→FScriptArray
  :954-1021), byte/enum int, FHitResult OUT table (struct out path :269-296 +
  refill :332-377), BoolProperty (bIgnoreSelf), 2× FLinearColor tables
  (StructProperty), float (TraceTime). Return bool (:1584-1589).
  `hit.ImpactPoint/Location/ImpactNormal` are NESTED struct reads from the
  refilled out table (Lua tables after GetNonTrivialLocal) — Lua-only.

### 1.3 class load + spawn

- `StaticFindObject(path)` — MOD:44, :283-287, :499. Overload #1
  (PORT/src/Mod/LuaMod.cpp:1529-1558) → `UObjectGlobals::StaticFindObject`
  with a string path; per-segment FName lookups (FNAME_Find class) + readable
  pool (slot 0). Returns most-derived userdata (auto_construct_object;
  UClass::construct for classes :449-452).
- `LoadAsset(path)` fallback — MOD:285-287. PORT/src/Mod/LuaMod.cpp:3618-3660:
  game-thread gate (:3624-3627), `FName(path, FNAME_Add)` (pool ADD),
  AssetRegistry via StaticReflectedFunction
  (UNREAL/include/Unreal/UAssetRegistryHelpers.hpp:19-23 —
  GetAssetRegistry/GetAsset reflected statics) → engine ProcessEvent class.
- `gameplay:BeginDeferredActorSpawnFromClass(pawn, pickupClass, transform, 2,
  pawn, 0)` — MOD:294-297. ObjectProperty, ClassProperty (UClass userdata —
  push_classproperty Set :555-563), StructProperty (FTransform table), enum
  int, ObjectProperty, enum int. Return AActor* → userdata (:515-518).
- `gameplay:FinishSpawningActor(actor, transform, 0)` — MOD:303. Object,
  Struct, enum int; no return.
- Pickup config — MOD:305-330: `actor.PickupInteractable` (ObjectProperty
  Get), `actor:GetComponentByClass(class(...))` (ClassProperty param + object
  return), `pickup.PickupInventoryItem` (StructProperty Get → UScriptStruct
  userdata) `.ItemId` (nested StrProperty Get → FString userdata → `str()` →
  `:ToString()` local, LuaUnrealString.hpp:68-72 + :198-210),
  `pickup.PickupAmount = int` (IntProperty Set via __newindex),
  `pickup.bAutoDestroyOnPickUp = true` (BoolProperty bitfield Set :1590-1594),
  `pickup:SetEnabled(true)`, `actor:SetActorHiddenInGame(false)`,
  `actor:SetActorEnableCollision(true)` (BoolProperty params), failure cleanup
  `actor:K2_DestroyActor()` (MOD:345).

### 1.4 widget creation (menu UI)

- `UEHelpers.GetPlayerController()` — MOD:15-19; UEHelpers.lua:69-83:
  `FindAllOf("PlayerController")`/`("Controller")` +
  `Controller:IsValid()` + `Controller:IsPlayerController()` /
  `IsLocalPlayerController()` (reflected bool calls). Fallback
  `FindFirstOf("PlayerController")` (MOD:21; PORT/src/Mod/LuaMod.cpp:1632-1661).
- `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")` — MOD:499.
- `widgetLib:Create(pc, userWidgetClass, pc)` — MOD:509-511: 3 params (object,
  class, object) + UUserWidget* return. Reflected
  (WidgetBlueprintLibrary::Create = CreateWidget entry).
- Fallback `StaticConstructObject(userWidgetClass, pc, FName("TownfallItemSpawnerRoot"))`
  — MOD:516; binding PORT/src/Mod/LuaMod.cpp:2200-2308 (Name param takes FName
  userdata :2232-2236, integer :2237-2240, else default 0). Engine call =
  slot 4 seed (INIT:1229-1245).
- `construct(path, outer, name)` = StaticConstructObject(class, outer,
  `FName(name)`) — MOD:49-55 for WidgetTree (:524), CanvasPanel (:527),
  Border (:530, :550), VerticalBox (:560), TextBlock (:455). The `FName(name)`
  Lua ctor (PORT/src/LuaType/LuaFName.cpp:48-101, default FNAME_Add :64) is
  the GATE-1-adjacent pool-ADMISSION path.
- Widget wiring — MOD:525-561: `ui.root.WidgetTree = ui.tree`,
  `ui.tree.RootWidget = ui.canvas` (ObjectProperty Set :519-534);
  `canvas:AddChild(panel)` / `box:AddChild` (ObjectProperty param + PanelSlot
  object return); slot `SetAnchors/SetAlignment/SetPosition/SetSize`
  (FAnchors/FVector2D struct tables); `panel:SetPadding` (FMargin table),
  `SetBrushColor` (FLinearColor table), `SetContent` (object param).
- Text styling — MOD:454-486: `w:SetText(FText(marker..label))` (FText ctor
  from string + TextProperty param — both engine Conv_StringToText,
  UNREAL/src/FText.cpp:147-156 + push_textproperty Set
  PORT/src/LuaType/LuaUObject.cpp:1807-1810); `w.Font.Size = 13`
  (nested struct field IntProperty Set via LuaUScriptStruct);
  `w:SetColorAndOpacity({SpecifiedColor={R,G,B,A}, ColorUseRule=0})` and
  `w:SetShadowColorAndOpacity({...})` — FSlateColor-SHAPED tables
  (MOD:361-369, :477-482); `w.ShadowOffset = {X=1,Y=1}` (FVector2D struct
  Set); `w:SetAutoWrapText(false)` (bool).
- `ui.root:AddToViewport(9999)` — MOD:631 (int param); `SetVisibility(0/1)`
  — MOD:632/:655/:660 (enum int).
- Input lock — MOD:446-452: `pc:SetIgnoreLookInput/SetIgnoreMoveInput(bool)`.

### 1.5 entry globals

- `RegisterKeyBind(Key.F7/UP_ARROW/DOWN_ARROW/RETURN/LEFT_ARROW/RIGHT_ARROW,
  fn)` — MOD:694-729. Binding PORT/src/Mod/LuaMod.cpp:1915-2010 (key must be
  integer 0-255); `Key` global table at :614; F7=0x76 at
  PORT/first/src/Input/KeyDef.cpp:117.
- `ExecuteInGameThread(fn)` — MOD:695-728. Port-root binding
  PORT/src/Mod/LuaMod.cpp:4690-4740 (hook-state + EngineTick/ProcessEvent
  methods); console composition replaces the pump with the R3 FuncPtr marshal
  (CLMOD:4262-4339: lazy arming, `R3MarshalPumpPending`, carrier UFunction
  post-hook drain; default method constant EngineTick at
  PORT/include/Mod/LuaMod.hpp:169).
- `RegisterLoadMapPreHook(fn)` — MOD:731-744, pcall-wrapped; binding exists
  (PORT/src/LuaMod.cpp:2711+). Missing/nil → pcall swallows.
- `print`/`IsValid` chain — pure Lua + IsValid bindings.

---

## 2. Task 2 — Console status of each conversion

Console ground truth for this section comes from the composed initializer and
composed LuaMod of the CURRENT lineage (INIT / CLMOD):

- Seeds: slot 0 GUObjectArray (INIT:1063-1077), slot 3 GMalloc (INIT:1083-1128,
  V1+V2 verified), slot 1 FName pool READ admission (INIT:1137-1165 — the
  GATE-1 copyin-oracle blocker), slot 2 FName ctor thunk (INIT:1174-1224,
  behavioral ByteProperty roundtrip probe BEFORE bind at :1197-1212), slot 4
  StaticConstructObject (INIT:1229-1245 — "statically_verified_empty_callback;
  native success not observed" at :1242-1244).
- `fname_tostring` is UNBOUND BY DESIGN: INIT:1317 "[Seeds] Unbound by design:
  fname_tostring (Kismet fallback owns it), gnatives, fuobject_hash_tables_get;
  engine version via EngineVersionOverride settings".
- The Kismet fallback is REQUIRED at init: KismetStringLibrary class,
  Conv_NameToString UFunction, and the CDO are located in infinite retry loops
  (INIT:1966-2015) — i.e. `FName::ToString` cannot silently come up dead; if
  the objects never appear, init never finishes (visible stall, not garbage).
- ToString dispatch (UNREAL/src/NameTypes.cpp:127-142): Scan method only if
  `FNameToStringMethod == Scan` AND `ToStringInternal.is_ready()` — on console
  `fname_tostring` is unbound so this branch is skipped; else
  `Conv_NameToStringInternal` (ProcessEvent on the Kismet CDO, :69-108);
  else `throw std::runtime_error("FName::ToString was not ready but was
  called anyway")` (:140). With init as composed, the Conv fallback is the
  ONLY FName→string path.
- The composed LuaMod itself treats "engine ProcessEvent as an unproven
  engine-call class": CLMOD:5043-5063 changed 7 R3 log sites away from
  `GetFullName()` precisely because it routes FName::ToString →
  Conv_NameToString + a GMalloc-backed FString allocation, and "no run in
  this lineage has completed one".
- Host-only proven: worker-menu-chain ran the full Lua-side chain
  (F7 → keybind dispatch → closure → ExecuteInGameThread enqueue → game-thread
  drain → first engine-object lookup gate) 41/41 PASS
  (`outputs/2026-10-04-zcode-team/worker-menu-chain/results/PASS-receipt.txt`),
  with REAL Input/Handler + QueueInputSource + KeyDef + vendored Lua 5.4.7 +
  byte-exact LuaMod regions — but stopped at the first engine-object lookup,
  so NO engine marshaling is proven even at host.
- The l21 audit already recorded all 31 mod-called reflected methods as
  status "unknown", `console_proven_execution: false`
  (`outputs/2026-10-08-l21-audit/mod-api/reflected-methods.json`).

Status of each conversion leg (port-root binding site → console status):

| # | Conversion | Binding site | Console status |
|---|---|---|---|
| 1 | Lua int → Byte/Int/EnumProperty param | LuaUObject.hpp:974-1001; LuaUObject.cpp:1607-1688 | BOUND; logic host-exercised (menu-marshal-bounds positive controls); console UNPROVEN (no live UFunction call yet) |
| 2 | Lua bool → BoolProperty (bitfield) param | LuaUObject.cpp:1569-1605 | BOUND; host-exercised arithmetic; console UNPROVEN |
| 3 | Lua string → StrProperty param | LuaUObject.cpp:1837-1843 | BOUND; console UNPROVEN; NEEDED only if ItemId param is FString (unmeasured) |
| 4 | Lua string → NameProperty param | LuaUObject.cpp:1777-1781 + LuaMadeSimple.hpp:752-785 | BOUND-STRICT; plain string = loud runtime_error (never silent garbage); mod only hits this if ItemId param is FName (unmeasured) |
| 5 | Lua string → TextProperty param | needs FText userdata, LuaUObject.cpp:1807-1810 | BOUND-STRICT; wrong type = loud error; mod never passes raw string to FText param |
| 6 | `FName(string)` Lua ctor (pool ADD) | LuaFName.cpp:48-101 → NameTypes.hpp:239-255 | Slot-2 thunk bound ONLY after GATE-1 admission passes (INIT:1215-1217). If ctor ever unbound, `construct_with_string` SILENTLY returns → NAME_None object names (NameTypes.hpp:241) — silent-failure design worth a loud guard |
| 7 | FName return → FName userdata | LuaFName.cpp:13-33 (pusher :1773-1776) | BOUND; console UNPROVEN |
| 8 | FName → Lua string (`:ToString`) | LuaFName.cpp:119-125 → NameTypes.cpp:127-142 | fname_tostring UNBOUND BY DESIGN (INIT:1317); Kismet Conv_NameToString fallback is init-required (INIT:1984-2015) but is an engine ProcessEvent class NEVER executed on console (CLMOD:5043-5063 says exactly this). **What happens today if a Lua call's return is FName and you call :ToString(): a ProcessEvent on the KismetStringLibrary CDO with an 8-byte FName param frame — first execution of its kind; failure modes (fault, stall, garbage return) all unobserved.** If BOTH Conv and Scan were unavailable it throws `runtime_error` (NameTypes.cpp:140) — but with the composed init that state can't be reached silently |
| 9 | FString return → FString userdata → string | LuaUnrealString.hpp:26-46, :68-72, :198-210 | BOUND and ENGINE-FREE after the data read (local UTF-16→UTF-8); host-exercisable |
| 10 | `FText(string)` ctor | LuaFText.cpp:45-77 → UNREAL/src/FText.cpp:147-156 | BOUND via engine `UKismetTextLibrary::Conv_StringToText` (UNREAL/include/Unreal/UKismetTextLibrary.hpp:10-11) — console UNPROVEN (engine-call class) |
| 11 | FText userdata → TextProperty param | LuaUObject.cpp:1807-1810 (SetString → ANOTHER Conv_StringToText) | BOUND; engine-routed copy/destroy via FTextProperty ValueOpsProperty (FText.hpp:30-32, FText.cpp:40-43, :62-131); console UNPROVEN. Net: one `SetText` = ctor Conv + Set-Conv = 2 nested ProcessEvents |
| 12 | struct table → StructProperty param | LuaUObject.cpp:637-717 (silent per-field defaults :671-676) | BOUND; console UNPROVEN; silent-default hazard §3 |
| 13 | StructProperty return/out → Lua table | LuaUObject.cpp:727-814 (+out refill :332-377) | BOUND; console UNPROVEN (FHitResult out is the only out-param in the chain) |
| 14 | UObject/UClass userdata → Object/ClassProperty param | LuaUObject.cpp:506-578 | BOUND; console UNPROVEN; StaticFindObject yields proper UClass userdata via auto_construct_object (:449-452) so the ClassProperty strictness is satisfied |
| 15 | table → ArrayProperty param | LuaUObject.cpp:954-1021 | BOUND; placement-new FScriptArray + `AddZeroed` via GMalloc (slot 3) — engine-heap allocation; frame is never destructed after the call → small engine-heap leak per non-empty-array call (1 per LineTraceSingle) |
| 16 | Object*/struct/bool/int returns → Lua | LuaUObject.cpp:438-482, :515-518, :1584-1589, :985-988 | BOUND; console UNPROVEN |
| 17 | UObject property Get/Set (`Pawn`, `PickupInteractable`, `PickupAmount`, `bAutoDestroyOnPickUp`, `WidgetTree`, `RootWidget`, `ShadowOffset`, `Font.Size`, `PickupInventoryItem.ItemId`) | LuaUObject.hpp:502-510, :791-840; LuaUScriptStruct.cpp:58-66, :206-221 | BOUND; console UNPROVEN; UScriptStruct returns nil (not error) when FNAME_Find misses (:213-218) |
| 18 | StaticFindObject/FindFirstOf/FindAllOf(string) | LuaMod.cpp:1529, :1632, :1671+ | BOUND; FNAME_Find lookups + GUObjectArray iteration — dependent on GATE-1 readable pool upstream |
| 19 | StaticConstructObject(UClass,Outer,FName) | LuaMod.cpp:2200-2308 | Slot 4 bound with "native success not observed" (INIT:1242-1244) — bound-but-unproven by the initializer's own log |
| 20 | LoadAsset(string) | LuaMod.cpp:3618-3660 | BOUND; game-thread gate + FNAME_ADD + AssetRegistry reflected statics — console UNPROVEN |
| 21 | RegisterKeyBind + Key.* | LuaMod.cpp:1915-2010, :614; KeyDef.cpp:117 | BOUND and HOST-PROVEN dispatch (worker-menu-chain); console pad provider defers cleanly (composed UE4SSProgram.cpp:782-810) — but PS5 has no F7/arrows: mission-level pad-chord rebind (l12) maps the mod's keys |
| 22 | ExecuteInGameThread | LuaMod.cpp:4690+ (root) / CLMOD:4262-4339 (R3 marshal) | Queue mechanics HOST-PROVEN (worker-menu-chain items 3-4); console carrier arming/calibration UNPROVEN live |
| 23 | 512-byte param frame bounds | SEL:199-235 | Guard integrated in the selected unit; all mod calls' frames must fit — LineTraceSingle is the largest and its ParmsSize is UNMEASURED on the real image |

Seeds/refusal interactions summarized: every string-carrying leg either (a)
is engine-free after a data read (FString, #9), or (b) funnels into exactly
two engine-call classes — ProcessEvent-on-CDO Kismet conversions (FName #8,
FText #10/#11) and ProcessEvent-on-target UFunction calls (#1-#5 params, all
returns). GATE-1 (pool admission) refuses ALL of it terminally before mods
load (seed_refused → nothing bound, INIT:1158-1160), so marshaling cannot
degrade below "not running at all" — the audit's statuses assume GATE-1 and
UFunction resolution land.

---

## 3. Task 3 — The WIDGET path from Lua

What `buildUI` (MOD:488-645) actually needs, in marshaling terms:

1. A PlayerController (UEHelpers FindAllOf → reflected IsValid +
   IsPlayerController bool calls; or FindFirstOf).
2. `/Script/UMG.Default__WidgetBlueprintLibrary` CDO via StaticFindObject
   (string path → FName find per segment).
3. `widgetLib:Create(pc, userWidgetClass, pc)` — a normal reflected static:
   3 params (Object, Class, Object) + UUserWidget* return; the ENGINE's
   CreateWidget native does the Slate-side work. Console hazard: it runs the
   game-thread-only UMG pipeline inside our ProcessEvent — inside the R3
   marshal drain, which IS the game thread by construction (mod complies:
   every widget action is inside `ExecuteInGameThread`).
4. Fallback/foundation: `StaticConstructObject` (slot 4, "native success not
   observed") for UserWidget root, WidgetTree, CanvasPanel, Border×2,
   VerticalBox, TextBlocks — each with `FName(name)` (FNAME_ADD pool
   admission via the engine's own ctor through the slot-2 thunk).
5. Wiring via reflected property sets and child-adds (all pushers), text via
   `FText` ctor + `SetText` (2 engine Conv calls per SetText), colors via
   struct tables.

Console-specific hazards (beyond "everything is the unproven engine-call
class"):

- **UMG module presence is an init requirement, not a runtime surprise**: the
  initializer waits for `/Script/UMG.Widget` and `/Script/UMG.ComboBoxString`
  as required objects (INIT:2058-2059). Good: if UMG never cooks, init stalls
  visibly instead of the mod failing later.
- **AddToViewport needs a live GameViewport** — UEHelpers.lua:59 checks
  `Engine.GameViewport`; the mod opens the menu only after save load (status
  string "Load a save, then open with F7"), which is the right frame.
- **The FSlateColor-shape hazard**: MOD passes
  `{SpecifiedColor={R,G,B,A}, ColorUseRule=0}` to `SetColorAndOpacity`
  (MOD:364-368) and `SetShadowColorAndOpacity` (MOD:478-482). If the
  reflected param is FLinearColor (UWidget::SetColorAndOpacity(FLinearColor)
  in common UE5 builds), `convert_lua_table_to_struct` silently zeroes every
  field (no matching keys — LuaUObject.cpp:671-676) → opaque black text and
  zero-opacity shadow with NO error. The l21 audit flagged this exact
  signature as "not measured" (reflected-methods.json, SetShadowColorAndOpacity
  row). This is the one place the menu can "succeed" while rendering wrong.
- **Font.Size / ShadowOffset sets** ride the nested-struct chain
  (LuaUScriptStruct) — if `Font`'s FNAME_Find misses (ctor unready/pool
  unreadable), `.Size` read yields nil → `w.Font.Size = 13` becomes
  "index a nil value" inside pcall — silently unstyled text.
- **The pump**: F7 → keybind → `ExecuteInGameThread` → R3 FuncPtr-marshal
  drain. Host-proven mechanics; console needs the carrier armed (lazy,
  CLMOD:4314-4328). The menu ALSO needs repeated drains for navigation
  (up/down/enter arrows each enqueue a closure) — sustained carrier
  invocation, not a one-shot.

What the mod's UMG menu needs beyond the pump: nothing exotic — no Slate
direct calls, no custom materials; every widget op is a reflected call or
property set. The gap is purely that NONE of this engine-call class has ever
completed on console (CLMOD's own comment).

---

## 4. Task 4 — The SPAWN path (StaticLoadObject/SpawnActor equivalents)

The mod's equivalents (there is no direct StaticLoadObject in main.lua):

- Load: `StaticFindObject(path)` first; on miss `LoadAsset(path)` then
  re-find (MOD:283-287). LoadAsset marshaling: Lua string →
  `FName(path, FNAME_Add)` → AssetRegistry `GetAssetByObjectPath` (FAssetData
  by value — FAssetData marshaling via the unreal layer's own param structs)
  → `GetAsset` → returns UObject* (three values returned to Lua: object,
  was_found, did_load — LuaMod.cpp:3618-3660). FAssetData is a nontrivial
  struct param/return — this is the single most structurally complex
  marshaling in the whole chain, and it only runs for pickup classes NOT
  already resident (AlwaysCook entries should all be resident, making
  LoadAsset the rare path).
- Spawn: `BeginDeferredActorSpawnFromClass` + `FinishSpawningActor`
  (MOD:294-303) — the modern deferred-spawn pair (NOT the deprecated
  SpawnActor), all param types covered by §2 rows 1/12/14; returns actor
  userdata (row 16).
- Ground placement: LineTraceSingle with the FHitResult OUT table (rows
  12/13/15) — the only OUT-param conversion in the chain and the largest
  frame (FHitResult alone ≈ 0xE8 bytes; whole frame est. 350-400 B < 512 B
  guard, but ParmsSize is UNMEASURED — the integrated preflight at SEL:199-235
  refuses loudly if it ever exceeds 512).
- Post-spawn config/verification: nested `PickupInventoryItem.ItemId` read
  (row 17) compared against the requested id (MOD:311-323) — meaning a
  marshaling-induced wrong id (e.g. a garbage FString) turns into a CLEAN
  rollback (K2_DestroyActor) rather than a bad spawn. Good containment.
- Failure paths: every stage is pcall-wrapped with `busy` reset and destroy
  cleanup (MOD:226-347); marshaling errors surface as "Failed at <stage>" in
  the status line + UE4SS.log — loud enough for the ladder.

Same treatment conclusion: spawn-path marshaling is fully covered by bound
pushers; the risk concentration is (a) the first-ever live ProcessEvent
class, (b) FNAME_ADD admission in LoadAsset (rare path), (c) FHitResult
out-refill correctness on first live execution.

---

## 5. Task 5 — L26 MARSHALING REQUIREMENTS (fix-completeness checklist)

Legend: GREEN = bound and host-proven (no engine dependency) · AMBER = bound,
console-unproven (engine-call class never completed in this lineage) · RED =
blocked, unmeasured-with-break-risk, or silent-by-design hazard.

| # | Conversion | Needed-by (call, MOD line) | Current status | Fix needed before/with L26 |
|---|---|---|---|---|
| 1 | Lua int → Byte/Int/Enum param | GetItemIDFromDeprecatedEnum:163; LineTraceSingle:248-257; Begin/Finish:296,303; SetVisibility:632; AddToViewport:631 | GREEN (bound, host-exercised) | none — assert via first live-call log |
| 2 | Lua bool → BoolProperty bitfield param | LineTraceSingle:249,254; SetEnabled:327; SetActorHiddenInGame/EnableCollision:329-330; SetIgnore*Input:450-451; SetAutoWrapText:476 | GREEN (bound, host-exercised) | none |
| 3 | Lua string → StrProperty param | GetInteractablePickupIDActorPath:168 (only if ItemId param is FString) | AMBER (bound:1837-1843) | measure the reflected param type (see #27) |
| 4 | Lua string → NameProperty param | same call if ItemId param is FName | AMBER-STRICT (bound:1777-1781; plain string → loud runtime_error) | if type=FName: mod-side `FName(id)` wrap OR accept-strings branch in pusher; else none |
| 5 | FText userdata → TextProperty param | SetText:434,469 | AMBER (bound:1807-1810, engine-routed) | first live verification |
| 6 | `FName(string)` Lua ctor — FNAME_ADD pool admission | construct():50; widget names:516+; LoadAsset internal | RED (slot-2 thunk binds only after GATE-1 admission; ctor-unbound silently yields NAME_None, NameTypes.hpp:241) | GATE-1 fix (upstream); ADD a loud Lua-visible error when constructing FName from string with ctor unready, so silent "None" names can't flow into SCO |
| 7 | FName return → FName userdata | db pair returns if NameProperty | AMBER (bound LuaFName.cpp:13-33) | measure return type (#27) |
| 8 | FName → Lua string via Kismet Conv_NameToString | str() of any FName return:59; R3 logging class | AMBER-CRITICAL (fname_tostring UNBOUND BY DESIGN, INIT:1317; Conv fallback init-required INIT:1984-2015 but engine ProcessEvent class NEVER executed on console — CLMOD:5043-5063) | schedule Conv_NameToString as the FIRST instrumented engine call after resolution lands; log + fault-contain it |
| 9 | FString userdata → Lua string | str() of db returns/ItemId:59,313 | GREEN (engine-free local conversion, LuaUnrealString.hpp:198-210) | none |
| 10 | `FText(string)` ctor → engine Conv_StringToText | SetText:434,469 | AMBER (bound FText.cpp:147-156; engine call) | same first-live verification as #8 |
| 11 | FText engine-correct copy/destroy (ValueOpsProperty) | every TextProperty param + FText return | AMBER (bound FText.hpp:30-32, FText.cpp:62-131; TypeChecker must have set StaticSize+ValueOpsProperty) | verify TypeChecker init populated ValueOpsProperty before mods run |
| 12 | struct table → StructProperty param | LineTrace vectors/colors:246-257; transform:273-277; SetPadding:544; SetBrushColor:547; slot anchors/align/pos/size:534-541 | AMBER (bound:637-717) | none in marshal; add optional "no field matched" warning log |
| 13 | FSlateColor-shaped table vs FLinearColor param | SetColorAndOpacity:364-368; SetShadowColorAndOpacity:478-482 | RED (silent zero-default render, LuaUObject.cpp:671-676; signature unmeasured per l21 audit) | measure reflected param type; if FLinearColor, fix MOD to pass {R,G,B,A} |
| 14 | StructProperty return → Lua table | K2_GetActorLocation:233; GetControlRotation:234 | AMBER (bound:727-814) | none |
| 15 | FHitResult OUT param → table refill | LineTraceSingle hit:252,264-270 | AMBER (bound:269-296+:332-377; only out-param in chain) | first-live verification; check nested ImpactPoint/ImpactNormal tables |
| 16 | UObject userdata → ObjectProperty param | pawn/pc args:245-303; Create:510-511; SetContent:551 | AMBER (bound:506-543) | none |
| 17 | UClass userdata → ClassProperty param | Begin...:296; Create:510 | AMBER (bound:545-578; StaticFindObject returns UClass userdata:449-452) | none |
| 18 | table → ArrayProperty param (engine heap) | ActorsToIgnore {pawn}:252 | AMBER (bound:954-1021; GMalloc slot 3; per-call engine-heap leak — placement-new FScriptArray never destructed) | accept for prototype; add frame teardown for param arrays later |
| 19 | Object*/bool/int returns → Lua | GetPawn:31-33; GetComponentByClass:208; Begin actor:295; Create widget:510; AddChild slot:531; LineTrace bool:245 | AMBER (bound:438-482,1584-1589,985-988) | none |
| 20 | UObject property Get/Set + nested struct chain | Pawn:31; PickupInteractable:202; PickupAmount:325; bAutoDestroyOnPickUp:326; WidgetTree:525; RootWidget:528; Font.Size:470; ShadowOffset:483; PickupInventoryItem.ItemId:313 | AMBER (bound LuaUObject.hpp:502-510,791-840; LuaUScriptStruct.cpp:206-221) | none — note nil-return-on-miss semantics (:213-218) already pcall-contained by mod |
| 21 | StaticFindObject/FindFirstOf/FindAllOf | class():44; database():64; widgetLib:499; UEHelpers.lua:69-83 | AMBER (bound LuaMod.cpp:1529,1632,1671; FNAME_Find + GUObjectArray) | rides GATE-1 readable pool |
| 22 | StaticConstructObject (slot 4) | widget fallback:516; WidgetTree/panels/TextBlocks via construct():455-560 | AMBER (bound INIT:1242-1244 "native success not observed") | first live SCO is a named L26 checkpoint |
| 23 | LoadAsset + AssetRegistry statics + FAssetData marshal | class-load fallback:285-287 | AMBER (bound LuaMod.cpp:3618-3660; game-thread gate; rare path if AlwaysCook resident) | verify only if StaticFindObject misses |
| 24 | ExecuteInGameThread → R3 FuncPtr marshal | ALL keybind work:695-728 | AMBER (queue mechanics host-proven worker-menu-chain 41/41; console carrier arming unproven, CLMOD:4262-4339) | carrier calibration live; sustained-drain soak (navigation) |
| 25 | RegisterKeyBind + Key.* dispatch | F7/arrows/return:694-729 | GREEN (host-proven dispatch; Key global LuaMod.cpp:614, KeyDef.cpp:117) | console input mapping: pad-chord rebind (mission l12) — input-layer, not marshaling |
| 26 | 512-byte frame bounds preflight | every UFunction call | GREEN (integrated SEL:199-235; menu-marshal-bounds controls) | measure LineTraceSingle ParmsSize once reflection reads are live |
| 27 | Reflected types of the db pair (param+return) | GetItemIDFromDeprecatedEnum:163; GetInteractablePickupIDActorPath:168 | RED (UNMEASURED — l21 reflected-methods.json "actual native reflected type unmeasured"; FName/Text flavor would break #3/#4/#5 loudly-inside-pcall → silent item loss) | one static reflection dump of both UFunctions' param/return property classes (doable the moment pool+reflection reads are live, before any call) |
| 28 | fname_tostring direct scan seed | (replaces #8's fallback cost) | UNBOUND BY DESIGN (INIT:1317) — not a defect | optional perf item only; never required for correctness given #8 |

Counts: 28 rows — GREEN 5 (#1, #2, #9, #25, #26) · AMBER 20 (#3, #4, #5, #7,
#8, #10, #11, #12, #14, #15, #16, #17, #18, #19, #20, #21, #22, #23, #24,
#28) · RED 3 (#6 pool-admission-with-silent-None, #13 FSlateColor silent
render, #27 db-pair types unmeasured). (#28 is unbound-by-design/optional and
counted amber-class rather than red because #8 functionally covers it.)

Ordering note for L26: #27 (static type dump) is free once reflection reads
are live and unlocks #3/#4/#13 decisions; #8/#10 share one engine-call class
so one instrumented Conv execution covers both; #6's silent-None guard is a
5-line Lua-visible throw and closes the only silent path into SCO.

---

## 6. Single riskiest marshaling gap

**#8 + #27 combined: the db scan's string legs.** `str()` of the
GetItemIDFromDeprecatedEnum return and the plain-string pass into
GetInteractablePickupIDActorPath sit at the top of the proving ladder, are
entirely pcall-wrapped (MOD:162-179), and fail in OPPOSITE directions:
FName-flavored types fail loudly-but-swallowed (runtime_error → entry dropped
→ menu silently degrades to the 6 static extras), while the FName→string leg
that DOES run requires the Kismet Conv_NameToString ProcessEvent class that
no console run in this lineage has ever completed (the composed LuaMod
explicitly avoids calling it in its own logging, CLMOD:5043-5063). Worst
case: the ladder reads as "game data missing" when it is actually the first
unproven engine-call class — burning a launch test on a misdiagnosis. The
cheap insurance is #27's static reflection dump (types before calls) plus
making the Conv_NameToString probe the first instrumented engine call after
UFunction resolution lands.
