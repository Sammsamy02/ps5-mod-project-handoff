# IN-MINIMAP-STUTTER — TownfallMiniMap v3.1 50 ms tick: what is expensive

Agent IN, 2026-10-11. Analysis + Lua prep only — no builds, no console, no minimap
edits. Source audited in full:
`.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua`
(196,743 B, 4,271 lines, sha d8f8d64f… per STATIC-SCAN §6).

## 0. Executive summary

- The tick driver is `LoopInGameThreadWithDelay(TICK_MS, safeTick)` at **:4268**
  (`TICK_MS = 50` at **:9**) → `safeTick` **:4241** → `tick()` **:4062–4239**. On this
  port every LIGWD callback runs on the **R3-marshal game-thread pump**
  (launch2:909, round4:855), so every millisecond of tick body is a millisecond
  taken out of the frame budget (16.6 ms at 60 fps).
- The PlayerController is **already cached** (`S.pc`, :304–310). The
  "FindFirstOf every tick" hypothesis is FALSE for steady state; the cache drops
  only on invalidation (map/pawn change, no-pawn retry every 20 ticks :4090,
  key-poll failure :4045).
- The **1-second stutter signature is the 20-tick (1.0 s) phase-aligned burst**:
  `FindAllOf("TownfallEnemyCharacter")` (ENEMY_RESCAN = 20, :16, fired at
  `tickN % 20 == 0`, :4195) **plus** `R.refresh()`'s 23 reflected
  `GetNumItemsOfTypeInInventory` calls (R.CHECK_TICKS = 20, :418, burst at :454)
  land on the same tick. Pickup scans add a second spike at 0.5 s
  (PICKUP_RESCAN = 10, :20); interaction/hide scans at 2 s (RESCAN = 40, :22/:31).
- Why a single FindAllOf is expensive HERE: **launch2:764 / round4:763 —
  `[HashTables] FUObjectHashTables unavailable, object lookups use
  GUObjectArray`**. Every FindAllOf/FindFirstOf takes the non-hash path
  (port `UObjectGlobals.cpp:565–589`): a full GUObjectArray walk; per object a
  class-name FName::Equals, a validity-flag read, **and for every non-instance a
  full SuperStruct-chain walk with one FName::Equals per level**. That is
  ~40K objects × (1 + ~4–8 chain links) ≈ 200–500K compares + 40K flag reads
  **per scan**, ×4 scans/s, all on the game thread.
- `GetWorld` (task question): the mod has exactly ONE GetWorld — `drawInto` :526,
  reached only from `copySnapshot` (map open, 4×/s) and `renderPanel` crop
  redraws. The UE4SS Lua `UObject:GetWorld` binding is an outer-chain walk
  (OuterPrivate until UWorld), NOT a `FindFirstOf("World")` scan — that
  FindFirstOf pattern is zzdiag's own code (zzdiag-v11 `mn()`), never the
  minimap's. GetWorld is cheap and not the stutter source.
- Widget work is **diff-gated** (position-string compares before every
  SetPosition/SetSize: :2049–2052, :2077, :2109–2110, :2180–2181, :2201–2202,
  :2222–2223); SetText fires only on toast transitions (:3104). The 1,372
  StaticConstructObject burst (launch2: 990 in s 16:27:17 + 211 in 16:27:18) is
  the one-time `buildWidget` (:1117+) — a first-open hitch, NOT the 1 Hz stutter.
  Later clusters of 4–10 are `M.keepTexture` Image constructions (:3132).

## 1. The tick body — every engine/Lua API call per tick, ranked

Steady state (minimap loaded, HUD hidden, map closed) — runs EVERY tick:

| # | Call(s) | Site(s) | Frequency | Class |
|---|---------|---------|-----------|-------|
| 1 | `pc:IsValid()` (cache check) | :305→:217 | 1× | UE4SS binding |
| 2 | `pc:GetAddress()` | :4070 | 1× | UE4SS binding (address math) |
| 3 | `pc:IsInputKeyDown({KeyName=FName})` | :4043 (in `pollKeys` :4037) | **8×** | **UFunction + FKey struct param marshal** |
| 4 | `pc.Pawn` + IsValid | :313–314 | 1× | reflected property read |
| 5 | `pawn:GetAddress()` | :4095 | 1× | binding |
| 6 | `GetLastInteractedInteractable()` | :365 (`pollInteracted` :363) | 1× | UFunction |
| 7 | `CurrentInteractableIsMaxedPickup()` + `GetCurrentInteractable()` | :372–373 (`pollMaxedPickup` :369) | 2× | UFunction |
| 8 | `S.widget:IsValid` + `IsInViewport()` | :4117 | 2× | binding + UFunction |
| 9 | `comp:IsMapOpen()` | :4136 | 1× | UFunction |
| 10 | mapTexture chain: `.WidgetTextures` + `GetArrayNum` + `wts[1]` + `.Material` + IsValid + `.MaterialTextureName` + `K2_GetTextureParameterValue()` | :1501–1506 | ~7 ops | property reads + **UFunction** |
| 11 | mapKey: `wt.Widget:GetClass():GetFName()` + `str()`→`ToString()` | :4138 | 3× | UFunction + FName bridge |
| 12 | `lib:GetGameState(pc)` (+ cached StaticFindObject + IsValid) | :2546–2548 | 2–3× | UFunction |
| 13 | `pc:GetHUD():GetHUDDisplayState()` | :2553 | 2× | UFunction ×2 |
| 14 | areaMaps: `pawn.MapComponent` + `.CurrentRegionMapItems` + `GetArrayNum` + N×(`arr[i]` + ToString) | :2559–2561 | 2 + N | property + array reads + FName bridge |
| 15 | `ui.toastText:IsValid` / `S.widget:IsValid` | :3099 / :4056 | 2× | binding |

Every 20 ticks (1 s — **the stutter second**, phase 0):
- :4195 `scanEnemies` (:2577): **`FindAllOf("TownfallEnemyCharacter")`** (full
  GUObjectArray walk) + per-enemy `IsValid` + `GetHasBegunDeath()` (:2581).
- :4109 `R.refresh` (:446, epoch gate :447): `R.pawn.InventoryComponent`
  (:440) + **23 × `inv:GetNumItemsOfTypeInInventory(t, true)`** (:453–455).
- (when shown, both above run only if `show`; R.refresh runs whenever `R.on()`
  — ready/readyPulse settings — even with the map closed :4109.)

Every 10 ticks at phase 3 (0.5 s): :4198 `scanPickups` (:2997) —
**`FindAllOf("PickupInteractable")`** (:3000) + per-pickup `GetOwner` +
`Enabled` + 2×`GetAddress` + `K2_GetComponentLocation` + `itemKind` (first
sight only: `PickupInventoryItemType`, `.ItemName`, `GetAttachParent().StaticMesh:GetFName()`)
+ `pickupExists` burst (:2962–2984: up to 8 reflected calls per kept pickup,
recheck every 40 ticks :2990).

Every 40 ticks at phases 11 / 31 (2 s): :4202 `scanInteractions` (:2742) —
**`FindAllOf("Interactable")`** (:2745) unless the >64-cache survives
(cache dropped on region change :4150); batched 32 components/tick
(INTERACT_BATCH :28) with a 5 ms `os.clock` budget (:2761/:2831,
INTERACT_BUDGET_MS :28); per near component ~6–15 reflected calls.
:4206 `scanHides` (:2877) — **`FindAllOf("HidingPlaceInteractable")`** (:2880)
+ per-hide ~4 calls (classify cached per-address :2615).

Every 5 ticks while the in-game map is OPEN (250 ms): `copySnapshot` (:4159,
COPY_EVERY :9) → `drawInto` (:523): `pc:GetWorld()` (:526, outer-chain walk),
`BeginDrawCanvasToRenderTarget`, `K2_DrawTexture`, `EndDrawCanvasToRenderTarget`
(:533/:539/:543) + `SetBrushResourceObject` (:1521).

Per tick when the minimap HUD is SHOWN (`updateView` :2338, via :4210) — the
normal playing state inside a map area:
- :2365 `pawn:K2_GetActorLocation()`; :2366 `pc.PlayerCameraManager` +
  `GetCameraRotation()` (:1496).
- Per enemy (:2372–2399): IsValid + `K2_GetActorLocation` + `GetAlertState` +
  `GetAddress` + (`GetController()` + `.TownfallSight` +
  `GetPlayerPerceptionProgress()`) + per shown enemy `K2_GetActorRotation`
  (:2135) ≈ 7 calls each.
- Per pickup ≤ 48 (:2401–2425): 2×IsValid + 2×`GetAddress` + `.Enabled` +
  `.bHidden` + `K2_GetComponentLocation()` ≈ 6 each.
- Per interaction ≤ 48 (:2427–2486): door state (IsValid + every 4 ticks
  `IsOpen`+`IsLocked` :408–415) + IsValid×2 + `.Enabled` + `.bHidden` +
  `K2_GetComponentLocation` + `GetAddress` + `R.ready` → `IsItemIDInInventory`
  (epoch-cached :462–468) ≈ 5–8 each.
- Per hide ≤ 24 (:2488–2503) and per save (:2507–2520): ~4–5 each.
- `renderPanel` per face ×1–2 (:2530–2542): diff-gated widget writes
  (SetPosition/SetSize :589–592, SetRenderScale/Opacity/Angle,
  SetColorAndOpacity, SetVisibility) — only on change, but positions change
  most ticks while anything moves; crop redraw → `drawInto` on movement.

Cost ranking (likely per-tick game-thread time on this port):

1. **FindAllOf scans** (4×/s, full-array × SuperStruct-chain) — milliseconds
   each; the only single call capable of eating a 16.6 ms frame alone.
2. **R.refresh 23-call UFunction burst** — same 1 s boundary as the enemy scan.
3. **Per-entity UFunction storm when shown** (~300–700 reflected calls/tick at
   the caps = 6K–14K/s; each is pcall + closure + FindUFunction + param struct
   marshal + ProcessEvent).
4. **IsInputKeyDown ×8** (160 UFunction calls/s with FKey deep-struct marshal —
   STATIC-SCAN flags the struct-param shape :281–283).
5. mapTexture/mapKey/gameState/hudState/areaMaps constant chain (~15–20
   UFunction/property ops ×20 = 300–400/s).
6. String ops: mapKey `ToString` :4138, areaMaps per-element :2561,
   `str()` chains — ~5–15/s through the FName bridge; minor.
7. Widget writes: diff-gated; not the 1 Hz source.

## 2. Cost model (20 ticks/s)

With ~40K live UObjects and FUObjectHashTables unavailable (proven, §0):

- **FindAllOf scans**: 4 scans/s (1 enemy + 2 pickup + 0.5 interact + 0.5 hide)
  ≈ 4 × (1–8 ms) ≈ **4–32 ms/s of pure game-thread scan time**, concentrated in
  one 50 ms tick each — exactly an every-second hitch. (If the task's
  worst-case arithmetic is applied — 20 × 40K comparisons — that is the
  *PlayerController* pattern, which the mod does NOT do per tick; the real
  per-second comparison load is 4 × 40K × (1 + chain ≈ 5) ≈ 800K/s in the four
  scans, plus their per-entity follow-ups.)
- **R.refresh**: 23 UFunction calls at the same 1 s boundary.
- **Baseline idle** (map closed, HUD hidden): ~45–55 reflected/binding ops per
  tick ≈ 900–1,100/s + ~50 pcall+closures per tick ≈ 1,000/s of Lua overhead.
- **Shown state**: + ~300 (modest: 6 foes/20 pickups/20 interactions/8 hides)
  to 700+ (caps 48/48/24 + all enemies) reflected ops per tick =
  **7K–15K/s**. At 20–100 µs per marshaled call (PS5 core + bridge) that is
  0.15–1.5 ms per tick baseline and the scans still spike on top.
- **Map open**: + 4×/s full-canvas `drawInto` (RT draw implies an RHI flush).
- **First map build**: 1,372 SCOs in ~1–2 s (launch2 16:27:17–18) — one-time.

## 3. The diagnostic — `zzminimapdiag-v1/main.lua`

- File: `outputs/2026-10-08-l25-prep/zzminimapdiag-v1/main.lua`
  (3,073 B, `luac -p` clean; sha256
  `259833d5c4a7155a076e356a6147eb5540fdd07858f992fa54211062ae4b859d`).
  Over the 2 KB target by ~1 KB: the four phase-aligned FindAllOf sections +
  the 23-call inventory burst are the minimum that attributes the 1 Hz spike;
  still below the 3.9 KB zzdiag-v11 that rode the same slot.
- Deploy: `harvest/lua_fast_deploy.sh <path>` — it overwrites the zzdiag slot
  and fixes the loadlist size row automatically. The minimap mod itself is
  never touched.
- What it does: own 50 ms LIGWD loop (same cadence), `os.clock()` around each
  section, sections mirroring the tick body (noop baseline, FindAllOf PC on
  cache-miss only, `Keys8` ×8 IsInputKeyDown, `PawnLocRot`,
  `MapChain` = MapRenderComponent+IsMapOpen+WidgetTextures+K2_GetTextureParameterValue,
  `InvC` + `R23x` on the 20-tick boundary, `GSHUD`), plus the four phase-aligned
  FindAllOfs (enemy%20, pickup%10==3, interact%40==11, hide%40==31). Prints one
  `[INMD]` line per second: per-section n / avg ms / max ms / found-object count,
  plus worst single tick. All engine calls pcall-guarded; read-only.
- **A/B law (must follow)**: the diag's own FindAllOf calls stack with the
  minimap's. Session 1: current loadlist (minimap ON) — reproduces and
  attributes the stacked stutter second. Session 2: remove the TownfallMiniMap
  row from the loadlist (deploy-time loadlist edit, not a minimap edit) — clean
  per-call baselines. Compare the two; never absolute-read session 1.

Readout → decision table:

| Dominant row on the 1 s report | Culprit | Fix |
|---|---|---|
| `FAOEn` (and FAOPk/FAOIn/FAOHd) max ≫ others | full-array scans | port hash tables / FindAllOf cache (§4.1–4.2) |
| `R23x` max dominates | inventory UFunction burst | mod-side R.refresh gating; port UFunction path cost |
| `Keys8` avg ×8 dominates | FKey struct-param marshal | keybind redesign / subset polling |
| `MapChain`/`TexParam` avg high | per-tick reflected chain | cache mapKey + texture handle |
| all sections < 0.5 ms yet stutter persists | not the Lua tick (suspect canvas RT flush with map open, or non-Lua) | extend probe to drawInto timing |

## 4. Fix recommendation (ranked; none applied — no minimap edits allowed)

1. **PORT — enable FUObjectHashTables** (highest leverage, fixes every mod):
   the launch-2/round-4 logs prove lookups fall back to GUObjectArray walks
   because the hash tables are unavailable; the `fuobject_hash_tables_get`
   seed is already in the backlog (launch2:27 "Unbound by design"). With hash
   tables, FindAllOf uses FindObjectSearcher class-rooted iteration
   (UObjectGlobals.cpp:546–562) — the 40K×chain walk disappears.
2. **PORT — FindAllOf result cache** (if hash tables stay unbound): cache by
   class name, invalidate via NotifyOnNewObject + validity checks; the port
   already exposes NotifyOnNewObject (the minimap uses it :4248).
3. **MOD-side, for a later authorized minimap revision** (currently forbidden):
   stagger the 1 s alignment (move R.refresh off the enemy-scan phase),
   ENEMY_RESCAN 20→60, gate R.refresh to menu-open, replace the 8×
   IsInputKeyDown with RegisterKeyBind (port-supported) or 2 keys/tick
   rotation, cache the `mapKey` FName instead of recomputing :4138.
4. **NOT recommended**: tick 50→200 ms (smooths the baseline, keeps the 1 s
   spike, makes the minimap choppy); batching widget writes (already
   diff-gated); background thread (engine calls must stay game-thread).

## 5. Evidence appendix

- launch2-final-session-20261011.decoded.log — :835 minimap started 16:26:58;
  :764 `[HashTables] FUObjectHashTables unavailable, object lookups use
  GUObjectArray`; :909 R3 marshal owns the game-thread pump; 1,440
  `widget/object constructed` lines total, burst 990 @16:27:17 + 211 @16:27:18,
  later 4–10 clusters (keepTexture).
- round4-spawn-session-20261011.decoded.log — :763 same HashTables line;
  :816–835 that loadlist had NO TownfallMiniMap (ItemSpawner/ueselftest/zzdiag
  only), so round-4 carries no minimap tick evidence — and the mod logs
  nothing per tick by design (its only print is the old-UE4SS banner :4270).
- mod-ladder/STATIC-SCAN.md §6 — API census; `GetWorld` :526 BOUND-UNTESTED;
  LoopInGameThreadWithDelay :4034/:4268 predicted-first-failure (P7) — in
  launch-2 it demonstrably registered and ran.
- Port source — UObjectGlobals.cpp:506–615 (FindAllOf non-hash full walk +
  TSuperStructRange per object), UnrealInitializer.cpp:108 (ShouldUseHashTables
  gate), LuaMod.cpp:5124–5208 (LIGWD game-thread carrier).
