# SPAWN-READINESS — the rung AFTER the menu (worker-re-spawn-chain)

Worker: worker-re-spawn-chain · 2026-10-05 · local only, read-and-derive + Ghidra 12.1.4 on the
ANALYZED `/tmp/wrfp_ghidra_proj/fnamepool` project (decrypted-eboot.bin, imageBase=0). No console
contact, no target execution. Companion: `CLAIM.json` (every pin), `REPORT.md` (limits).

Mod: TownfallItemSpawner v1.0, canonical bytes `worker-settings-v3-package/package-v3/.../main.lua`
(21,415 B, MD5 `4f15f2c79955c636eeaa38d0bf49433d`). All line pins below are exact for that file.

---

## 0. What actually runs when the user presses CROSS on an item row

`RegisterKeyBind(Key.RETURN, …)` (main.lua:708-711) wraps **every** activation in
`ExecuteInGameThread(activateSelection)`; `activateSelection` (main.lua:684-692) dispatches
`row.activate()` → `spawnSelected(entry)` (main.lua:215-348), guarded by the `busy` flag
(main.lua:216-219). Everything below runs inside ONE drained game-thread closure. Five ordered
stages, each with its own error string — these are exactly the mod's failure-taxonomy stages:

```
player → ground trace → load pickup class → spawn actor → configure pickup
```

| # | Stage | What the mod executes | UE4SS API(s) invoked | Pin |
|---|---|---|---|---|
| 1 | **player** | `getPlayerPawn()` → `getPlayerController()`: tries `UEHelpers.GetPlayerController()` first (UEHelpers.lua:69, which itself does `FindFirstOf("Engine")`/`FindFirstOf("GameInstance")`/PC, UEHelpers.lua:41-91), falls back to `FindFirstOf("PlayerController")`; pawn via property read `pc.Pawn` or UFunction `pc:GetPawn()` / `pc:K2_GetPawn()` (first that validates). Then `pawn:K2_GetActorLocation()`, `pc:GetControlRotation().Yaw`, placement = 150 units ahead along camera yaw | `FindFirstOf`, reflected property read, 2-3 UFunction calls | main.lua:14-41, :227-238 |
| 2 | **ground trace** | CDO lookup `class("/Script/Engine.Default__KismetSystemLibrary")` (StaticFindObject, main.lua:43-47), then `system:LineTraceSingle(pawn, {Z+50}, {Z-400}, 0, false, {pawn}, 0, hit, true, transparent, transparent, 0)`; reads back `hit.ImpactPoint or hit.Location` and `hit.ImpactNormal.Z`; rejects no-hit / steep surfaces | 1 CDO query + 1 ProcessEvent UFunction call with **struct in-params AND a struct OUT-param** (`FHitResult`) + bool return | main.lua:240-271 |
| 3 | **load pickup class** | `normalizeClassPath(entry.path)` (strip `'…'`, append `.<asset>`, append `_C`); `StaticFindObject(path)`; if invalid → `LoadAsset(path)`; re-query `StaticFindObject(path)`; error `"Pickup class could not be loaded: <path>"` if still invalid | `StaticFindObject` ×2 (+ `LoadAsset` only as fallback) | main.lua:122-136, :279-291 |
| 4 | **spawn actor** | CDO `class("/Script/Engine.Default__GameplayStatics")`; `gameplay:BeginDeferredActorSpawnFromClass(pawn, pickupClass, transform, 2, pawn, 0)` with `FTransform{Rotation quat, Translation ground+8, Scale3D 1}` (main.lua:273-277); then `gameplay:FinishSpawningActor(actor, transform, 0)` — Finish is what runs construction + BeginPlay and makes the actor real | 2 ProcessEvent UFunction calls, one with **FTransform by value + TSubclassOf** params, returning the new AActor | main.lua:293-303 |
| 5 | **configure pickup** | `getPickupComponent(actor)`: property read `actor.PickupInteractable`, fallback `actor:GetComponentByClass(class("/Script/Townfall.PickupInteractable"))`; reads `pickup.PickupInventoryItem.ItemId` (+ mismatch guard for DB entries, main.lua:321-323); writes `pickup.PickupAmount`, `pickup.bAutoDestroyOnPickUp`; calls `SetEnabled(true)`, `SetActorHiddenInGame(false)`, `SetActorEnableCollision(true)`; appends to `ownedActors` (Lua-side only). On ANY stage failure: status line + `actor:K2_DestroyActor()` (main.lua:344-346) | 1 CDO query, ~6 property reads/writes, 4-5 UFunction calls | main.lua:305-347 |

Class-path strings the spawn path loads (main.lua:72-79, :241, :294, :208):
- `/Script/Engine.Default__KismetSystemLibrary` (main.lua:241)
- `/Script/Engine.Default__GameplayStatics` (main.lua:294)
- `/Script/Townfall.PickupInteractable` (main.lua:208)
- `/Game/Townfall/AlwaysCook/Items/Weapon_Items/BP_Weapon_Axe`, `…/BP_Weapon_Pistol3000`,
  `…/Collectable_Items/BP_Item_Antibiotics`, `…/BP_Item_Empty_IV_Bag`, `…/BP_Item_Filled_IV_Bag`,
  `…/BP_Item_Enhanced_IV_Bag` → normalized to `<path>.<Asset>_C` (main.lua:122-136)
- plus live-DB paths from `db:GetInteractablePickupIDActorPath(id)` (main.lua:168) — same normalization.
  The DB getters themselves (`GetItemIDFromDeprecatedEnum` :163, `GetInteractablePickupIDActorPath`
  :168) are menu-rung-2 calls, re-run only via the Refresh row.

---

## 1. PS5-side readiness per dependency

### (a) `LoadAsset` — what the port's Lua LoadAsset maps to

- Registered `LuaMod.cpp:3618` (composed unit
  `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/src/LuaMod.cpp`).
- **Game-thread gate** (`IsInGameThread()`, LuaMod.cpp:3626-3629) — satisfied: the whole spawn runs
  inside the drained `ExecuteInGameThread` closure.
- Chain: `UAssetRegistryHelpers::GetAssetRegistry()` (port
  `native-coherent-runtime-1/source/port/unreal/src/UAssetRegistryHelpers.cpp:37-46`) →
  `asset_registry->GetAssetByObjectPath(FName)` (`UAssetRegistry.hpp:18`,
  `/Script/AssetRegistry.AssetRegistry:GetAssetByObjectPath`) → `UAssetRegistryHelpers::GetAsset()`
  (`UAssetRegistryHelpers.cpp:48-56`).
- **Every hop is a `StaticReflectedFunction`/`InstancedReflectedFunction` call = ProcessEvent**
  (`ReflectedFunction.hpp:22-33`: `Owner::Self->ProcessEvent(...)` / `Instance->ProcessEvent(...)`,
  function resolved by path from live reflection at first use, `ReflectedFunction.cpp:15-23`).
- **PAK mounting: the port does none.** LoadAsset does not mount anything; it asks the LIVE asset
  registry of the shipping game (which has already mounted its own paks and populated the registry
  at startup). It needs the registry's UFunctions to resolve and the registry object to be live —
  both reflection facts the engine-ready stage must already provide for ANY ProcessEvent call.
- Mod usage: fallback only (main.lua:283-287). The mod **ignores LoadAsset's 3 return values** and
  re-queries `StaticFindObject`. For the six AlwaysCook classes the classes are expected to be
  resident already (API-GAP.md §4: "likely unnecessary for the six AlwaysCook classes").

### (b) `BeginDeferredActorSpawnFromClass` → `StaticConstructObjectInternal` (slot 4, 0x490aa0, call-only)

Route to the engine: `gameplay:BeginDeferredActorSpawnFromClass(...)` is a Lua UFunction call →
`call_ufunction_from_lua` (LuaUObject.cpp:153) marshals 6 params from the UFunction's live property
list (ObjectProperty context+owner, **StructProperty FTransform**, ByteProperty collision-method
enum, ObjectProperty owner, ByteProperty split-screen) via the registered pushers
(UE4SSProgram.cpp:1704-1732), then `calling_context->ProcessEvent(func, params)`
(LuaUObject.cpp:327). Port `UObject::ProcessEvent` (UObject.cpp:448-450) reads the real function
pointer from the live object vtable at `VTableLayoutMap["ProcessEvent"]` = **0x260**
(generated `5_06_VTableOffsets_UObject_FunctionBody.cpp:571-578`; Win64-PDB-derived — see GAP-L2b)
and calls it (UnrealVirtualBaseVC.hpp:43-58). The initializer independently resolves and PRINTS the
same address at engine-ready (`Finding ProcessEvent` / `ProcessEvent address 0x<hex>`,
staged UnrealInitializer.cpp:1909-1915) — that log line is the live correctness check of the slot.

Inside the engine, the UFunction's native thunk runs `UGameplayStatics::BeginDeferredActorSpawnFromClass`
→ `UWorld::SpawnActorDeferred` → `StaticAllocateObject` → **`StaticConstructObjectInternal`** — the
seeded slot-4 address (linked 0x490aa0, runtime base+0x490aa0; the seed table's own derivation:
dominant final call pair `params-ctor 0x493530 → SCO 0x490aa0` in 744/1918 xref windows,
`SeedTable.launch10.h:34`). Key facts my Ghidra session adds:

- **Identity corroboration**: bytes at 0x490aa0 in the analyzed eboot are
  `55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec` — byte-identical to the slot-4 prologue anchor.
- **Body**: 0x490aa0..0x490c74 (469 bytes), 8 direct calls
  (`0xbe760 0x368090 0x480fc0 0x490fb0 0x491db0 0x491fe0 0x4b51c0 0x7471e60`) and one RELRO
  pointer read `[0x9131f08]` — a coordinator, consistent with the 4.26+ signature
  `UObject* StaticConstructObjectInternal(const FStaticConstructObjectParameters&)` that the port
  binds (`GlobalState::StaticConstructObjectInternal426`, UObjectGlobals.cpp:21, bound :23-27,
  invoked :70-77).
- **What the call NEEDS beyond the address**: within a 3-level, 140-function DFS from SCO's callees
  there are NO direct rip-relative references to the seeded GUA (0x93ba110) / GMalloc (0x930add8) /
  FName pool (0x9365ec0) labels — the construction subsystem reaches those deeper than 3 levels and
  through RELRO-indirect globals (e.g. the unnamed `[0x9131f08]` pointer both SCO and the
  0x493580 wrapper read). This is expected engine structure, not a port gap: all engine-internal
  references resolve against the engine's own load base. The seed set that already arms the
  launch-10 chain (GUA slot 0 — in-game-validated; GMalloc slot 3 — V1/V2 validator-gated;
  FName pool + ctor slot 2 — bound at initializer :765/:910; SCO slot 4 — bound call-only,
  initializer :895-913) is exactly the population evidence; a GUA ref site from the derivation
  (0x2b1e57 `LEA R13,[0x93ba110]`) was re-verified directly in the analyzed project.
- **Crucially: on the spawn path SCO is called BY ENGINE CODE with engine-consistent ABI.** The
  port's unverified-ABI concern for slot 4 applies only to the port's OWN SCO calls — which the
  MENU already exercises: every UI element goes through the Lua `StaticConstructObject`
  (main.lua:50 `construct()`, used ~11× in `buildUI` main.lua:455-560, plus the fallback
  main.lua:516). If `Menu created.` prints, port→SCO ABI is proven live BEFORE any spawn.

**Where does the CLASS come from?** Not from LoadAsset's return. From **`StaticFindObject(path)`**
(main.lua:283, re-query :286) — a live GObjects query over the seeded GUObjectArray. LoadAsset
exists only to materialize the class into GObjects when StaticFindObject misses; the UClass pointer
then travels as the `TSubclassOf`/ObjectProperty param through ProcessEvent into the engine impl.

### (c) Player-object query (`FindFirstOf` path name)

`FindFirstOf("PlayerController")` (main.lua:21) → `LuaMod.cpp:1632` →
`UObjectGlobals::FindFirstOf` (UObjectGlobals.cpp:434). On this target the hash-table fast path is
OFF BY DESIGN: `fuobject_hash_tables_get` is unbound (staged UnrealInitializer.cpp:981-983
"graceful (UObjectHashTables.cpp:133)"), so `ShouldUseHashTables()` (initializer :120-123) is false
and the query iterates **all UObjects through the seeded GUObjectArray** (`ForEachUObject`,
UObjectGlobals.cpp:449+; `GUObjectArray` bound from slot 0 at initializer :765, whose seed is the
ONLY slot with in-game validation). Pawn then comes from property read `pc.Pawn` or the
`GetPawn`/`K2_GetPawn` UFunctions (main.lua:30-34). The identical mechanism is already on the MENU
path (`getPlayerController` in `buildUI` main.lua:493, `FindFirstOf("ItemDatabaseSubsystem")`
main.lua:64) — menu rungs 1-2 prove it.

### (d) `LineTraceSingle` — is it in the composed API surface or ProcessEvent?

**It routes through ProcessEvent — this is the L2 gap, precisely.** There is no composed
LineTraceSingle-specific code; it is a static native UFunction invoked on the KismetSystemLibrary
CDO through the generic `call_ufunction_from_lua` → `ProcessEvent` route (LuaUObject.cpp:327).
Param census vs registered pushers (UE4SSProgram.cpp:1704-1732) — every type is covered:

| Param | Property type | Pusher |
|---|---|---|
| WorldContextObject = pawn | ObjectProperty | push_objectproperty |
| Start / End {X,Y,Z} | StructProperty (FVector) | push_structproperty (live UScriptStruct members) |
| TraceChannel = 0, DrawDebugType = 0 | ByteProperty | push_byteproperty |
| bTraceComplex = false, bIgnoreSelf = true | BoolProperty | push_boolproperty |
| ActorsToIgnore = {pawn} | ArrayProperty (ObjectProperty elements) | push_arrayproperty → LuaTArray |
| **OutHit = `hit` table** | **StructProperty FHitResult, CPF_OutParm** | **push_structproperty, out-readback `GetNonTrivialLocal` with `reuse_same_table` (LuaUObject.cpp:339-341, 344-364)** |
| TraceColor / TraceHitColor | StructProperty (FLinearColor) | push_structproperty |
| DrawTime = 0 | FloatProperty | push_floatproperty |
| return hasHit | BoolProperty | push_boolproperty |

Out-param readback exists in code (`__PROSPERO__` branch: `Invocation.OutParameters` /
`StoreOutTableReference`, LuaUObject.cpp:112-116, :283; writeback loop :330-369). Struct member
offsets (ImpactPoint/ImpactNormal) come from the LIVE `FHitResult` UScriptStruct children
(LuaUScriptStruct.cpp recursion) — no hardcoded layout. What is NOT proven is that any of it has
ever executed in-game in this candidate: zero live ProcessEvent captures exist (the L2 gap),
and the FHitResult out-param is the historically highest-risk instance
(RUNTIME-REQUIREMENTS.md B5, API-GAP.md §4, POST-POPUP.md §3 discrimination row).

---

## 2. SPAWN-READINESS table

Legend — READY: structurally present + would be (or is) exercised by earlier rungs;
DEPENDS-ON: named prerequisite; GAP: the named open item. "R3" = the menu rungs of
POST-POPUP.md (drain + object query + reflected access), reachable only on a launch-11-armed
drain candidate.

| Step | Verdict | Evidence | DEPENDS-ON | GAP |
|---|---|---|---|---|
| 0. CROSS → drained game-thread closure (`ExecuteInGameThread`, main.lua:710) | **READY conditional on launch-11** | registered LuaMod.cpp:4700; queue/drain LuaMod.cpp:4467-4488; v3 keeps `HookEngineTick` default-true (UE4SS-settings.ini:28-35) | launch-11 R3 FuncPtr-marshal pump arming (SPAWN-CAPABILITY.md "working paths" 1) | **GAP-DRAIN**: launch-10 candidate has NO drain consumer — first-OPTIONS-press silence is EXPECTED; nothing below can run |
| 1. player query (`FindFirstOf("PlayerController")`, `pc.Pawn`, `K2_GetActorLocation`, `GetControlRotation`) | **READY conditional on R3 rungs 1-3** | FindFirstOf over seeded GUA (slot 0, in-game-validated; hash-table path off by design, initializer :981-983); UEHelpers fallback chain; identical mechanism on menu path (main.lua:64, :493) | R3 rung 2 (object query) + reflected property reads (rung 3) | none beyond GAP-DRAIN; property-read route is rung-3-proven if menu renders |
| 2. ground trace (`LineTraceSingle`, main.lua:245-258) | **DEPENDS-ON** | full route present: call_ufunction_from_lua → ProcessEvent (LuaUObject.cpp:327); all 12 param types have registered pushers; FHitResult out-param readback implemented (LuaUObject.cpp:330-369, `__PROSPERO__` StoreOutTableReference :283) | PE-LIVE (below) + live UScriptStruct member offsets for FVector/FLinearColor/FHitResult | **GAP-L2a (out-param)**: FHitResult struct OUT-param readback never executed live; historically highest-risk; failure shape = `Failed at ground trace.` (main.lua:341), mod survives |
| 3. load pickup class (StaticFindObject → LoadAsset → StaticFindObject, main.lua:283-287) | **READY conditional on R3 rung 2** (StaticFindObject part) | StaticFindObject = GObjects query (same as menu CDO lookups main.lua:499, :506); LoadAsset implemented (LuaMod.cpp:3618) via AssetRegistry UFunctions (all ProcessEvent); six AlwaysCook classes likely resident | PE-LIVE for the LoadAsset fallback only | **GAP-A7**: LoadAsset fallback never exercised; if the live registry lacks the FAssetData the stage fails with `Pickup class could not be loaded:` (main.lua:290) — mod survives. Low-probability for the 6 cooked classes |
| 4. spawn actor (`BeginDeferredActorSpawnFromClass` + `FinishSpawningActor`, main.lua:295-303) | **DEPENDS-ON** (structurally complete) | ProcessEvent slot resolved (PE 0x260 PDB-derived + engine-ready log check initializer :1909-1915); FTransform/ObjectProperty pushers registered; engine side self-consistent: SpawnActorDeferred → SCO slot 4 (engine→engine ABI); seeds GUA/GMalloc/FName-pool armed + SCO bound call-only (initializer :895-913); port→SCO ABI itself proven by menu `construct()` if rung 3 passes | PE-LIVE + GAP-L2b (below) + FinishSpawningActor (same route; BeginPlay/registration are engine-internal) | **GAP-L2b (PE slot)**: ProcessEvent vtable slot 0x260 is Win64-PDB-derived; PS5 slot-shift caveat recorded (core-port :617); the engine-ready `ProcessEvent address` line + rung-3 UMG calls are the live checks. **GAP-SPAWN**: engine spawn internals (allocation/registration/BeginPlay) never executed under the port — no known structural blocker, zero live evidence |
| 5. configure pickup (property reads/writes + SetEnabled/SetActorHiddenInGame/SetActorEnableCollision, main.lua:305-331) | **READY conditional on PE-LIVE + a successful stage 4** | same routes as rung-3 UMG reflected access (dozens of calls); property writes use the same live-reflection offsets; failure auto-destroys the actor (main.lua:344-346) | stage 4 must have returned a valid actor | nested property read `PickupInventoryItem.ItemId` (main.lua:313) + game-specific `PickupInteractable` component name (main.lua:202, :208) are unverified game-content facts, not API gaps |

Seed/ABI sub-facts behind the table:
- GUA slot 0 `0x93ba110` — seeded, read-path-guarded, **in-game-validated** (SeedTable.launch10.h:31).
- GMalloc slot 3 `0x930add8` — seeded + V1/V2 arming gate (SeedTable.launch10.h:15-22).
- FName pool `0x9365ec0` + FName ctor slot 2 `0x23d590` — seeded, bound (initializer :765/:910 phase).
- SCO slot 4 `0x490aa0` — seeded, `CodeCallOnly` enforced (:901-908), bound :910; prologue re-verified
  byte-exact in the analyzed eboot; body 469 B / 8 calls / 1 RELRO-ptr read; no direct seed-label
  refs within 3 levels (engine reaches GUA/GMalloc/pool deeper — population evidence: 1565 GUA
  ref sites, sample `0x2b1e57 LEA R13,[0x93ba110]` re-verified).

---

## 3. Verdict (honest)

**In the CURRENT combined candidate (the staged launch-10 rebuild): a successful menu-open cannot
happen, therefore a spawn cannot happen.** The drain blocker precedes this entire rung
(SPAWN-CAPABILITY.md: "LOAD-CAPABLE, NOT SPAWN-CAPABLE"); first-OPTIONS-press silence is the
PREDICTED outcome and nothing in my table is reachable. Visible spawn is not a launch-10 verdict
input.

**Given a launch-11-armed candidate where the menu opens (POST-POPUP rungs 1-3 pass): the spawn
chain is structurally complete — no missing APIs, no unbound seeds, no absent routes — and a
successful menu-open has ALREADY proven, live: GObjects queries (FindFirstOf/StaticFindObject over
the seeded GUA), reflected property reads/writes, real ProcessEvent UFunction calls (the UMG
setters), and the port→SCO call ABI (every menu widget is built through seeded slot 4 via Lua
StaticConstructObject).** What menu success does NOT prove, and what therefore remains between
"menu opens" and "actor appears":

1. **GAP-L2a** — struct-heavy in-params + the `FHitResult` OUT-param readback (`ground trace`
   stage). Highest-risk known instance; the mod fails soft (`Failed at ground trace.`).
2. **GAP-L2b** — the ProcessEvent vtable slot itself (0x260, Win64-PDB-derived) is only checked
   live by the engine-ready log line + rung-3's UMG calls; a PS5 slot shift would surface at the
   FIRST ProcessEvent call (rung 3), not at spawn.
3. **GAP-SPAWN** — the engine-side spawn path (SpawnActorDeferred → engine-internal SCO,
   allocation, registration, FinishSpawningActor construction/BeginPlay) has zero live evidence
   under the port, but is engine-code-calling-engine-code; the only port inputs it consumes are
   already-seeded addresses.
4. **GAP-A7** — the LoadAsset fallback (ProcessEvent-based AssetRegistry route) — likely unused
   for the six AlwaysCook classes; fails soft (`load pickup class` stage).

So: **menu-open ≠ spawn.** Menu-open converts this rung from "structurally blocked" to "three
named live-evidence instances pending (L2a, SPAWN, A7), each with a distinct mod-visible failure
shape and none a known structural gap." The cheapest discriminator remains the mod's own status
line + UE4SS.log: the first CROSS press on an item row will name the exact failing stage, and
POST-POPUP.md §3's discrimination table already maps `Failed at ground trace.` to GAP-L2a.
