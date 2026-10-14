# agent-IN-minimap — MiniMap 50 ms tick analysis + diagnostic prep

2026-10-11. Analysis + Lua prep only — no builds, no console, no minimap edits.
Deliverables:

1. `outputs/2026-10-08-l25-prep/IN-MINIMAP-STUTTER.md` — full analysis: tick-body
   call inventory with line numbers, cost model, diagnostic runbook, fix ranking.
2. `outputs/2026-10-08-l25-prep/zzminimapdiag-v1/main.lua` — profiler mod
   (3,073 B, luac-clean, sha256 259833d5c4a7155a076e356a6147eb5540fdd07858f992fa54211062ae4b859d),
   deployable via harvest/lua_fast_deploy.sh (rides zzdiag slot).

## Compact summary — cost per tick

- Tick = `LoopInGameThreadWithDelay(50, safeTick)` main.lua:4268 → tick():4062,
  20×/s, on the R3-marshal GAME THREAD (launch2:909) — tick ms = frame ms.
- Idle (map closed): ~45–55 reflected/binding ops/tick (~1,000/s) — 8×
  IsInputKeyDown (:4043), pollInteracted/pollMaxedPickup UFunctions (:365/:372),
  IsMapOpen+mapTexture chain (:4136–4138,:1501–1506), gameState/hudState
  (:2546–2553), areaMaps array walk (:2559).
- Shown (normal play): + ~300–700 reflected ops/tick (6K–14K/s) across enemies
  ≤N×7, pickups ≤48×6, interactions ≤48×7, hides ≤24×5, plus diff-gated widget
  writes (renderPanel :2003–2229 — NOT the stutter; positions change-gated).
- **The 1 Hz stutter second** = tickN%20==0 burst: FindAllOf("TownfallEnemyCharacter")
  (:4195, full-array scan) + R.refresh 23× GetNumItemsOfTypeInInventory (:454),
  same boundary. Second spikes: pickups %10==3 (:4198), interact %40==11 /
  hide %40==31 (:4202/:4206).
- Why scans are ms-scale: launch2:764/round4:763 prove
  `FUObjectHashTables unavailable → GUObjectArray` fallback; port
  UObjectGlobals.cpp:565–589 walks ALL ~40K objects × SuperStruct chain with an
  FName::Equals per link ⇒ ~200–500K compares per scan, ×4/s.
- PlayerController IS cached (:304–310) — the "FindFirstOf every tick" theory is
  disproven. GetWorld appears once (drawInto :526, outer-chain walk, not a scan).
- 1,372 SCOs = one-time buildWidget (launch2 16:27:17–18), not the 1 Hz loop.

## The fix

1. PORT: bind FUObjectHashTables (seed `fuobject_hash_tables_get` already in
   the unbound backlog) — FindAllOf switches to class-rooted searchers; kills
   the scans for every mod. 2. PORT alt: FindAllOf name→result cache with
   NotifyOnNewObject invalidation. 3. MOD (later, currently forbidden):
   stagger enemy-scan/R.refresh phases, ENEMY_RESCAN 20→60, keybind instead of
   8× IsInputKeyDown, cache mapKey. 4. NOT: 200 ms tick (spike survives),
   widget batching (already diff-gated), background thread (game-thread only).

## Diagnostic readout path

Deploy zzminimapdiag-v1; A/B: session 1 minimap ON (stacked spike), session 2
loadlist row OFF (baselines). `[INMD]` 1 s report ranks sections by avg/max ms;
`FAOEn` dominant → hash tables; `R23x` → inventory burst; `Keys8` → key-poll
marshal. Round-4 log note: that loadlist had no minimap and the mod prints
nothing per tick — launch-2 is the minimap evidence session.

— agent IN (ZCode), 2026-10-11.
