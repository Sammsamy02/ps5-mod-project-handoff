# BLOCKERS.md — Item Spawner blockers vs the acceptance ladder, with what-breaks-first

worker-itemspawner-dryrun, 2026-10-04. Static analysis; nothing executed.
Ladder (ZCODE-FORK-HANDOFF.md "CRITICAL PATH STATE"):
**game-thread callback → object query → reflected access → unchanged Item Spawner → visible spawn via real input.**
Statuses reuse RUNTIME-REQUIREMENTS markers (`depends-on-worker-N`, `missing-new`).
Source cites `IS:<line>` against the pinned 746-line main.lua.

## Blocker register

| # | Ladder rung | Blocker | Mod contact point | Status |
|---|---|---|---|---|
| B0 | rung 0 (pre-Lua) | Layout bytes for bundle keys 5+6 absent (BLOCKED-ON-LAYOUT-DATA) | none — run dies in `setup_unreal()` before Lua (UE4SSProgram.cpp:899-904) | worker-2 |
| B1 | rung 1 | `ExecuteInGameThread` game-thread delivery (A9) | IS:695 (first F7), 700/705/710/716/725 (guarded, menu-open only) | depends-on-worker-3 (EngineTick route); observed baseline failure exactly at IS:695 |
| B2 | rung 2 | Live object-array source: `FindFirstOf` (A3) / `StaticFindObject` (A4) need the real GUObject array; mapped_start(L7) for GUA 0x93ba110 (LOAD7 BSS) NOT found; v3 probe in flight/orphaned | IS:21, 64 (query), IS:44, 283/286, 499 (named lookup) | depends-on-worker-1 + worker-2; **no guessed offsets** (handoff honesty rules) |
| B3 | rung 2.5 | FName construction "its own gap (P2)"; `FText(string)` likewise | IS:50 (every `construct`), IS:516, IS:434, 469 | missing-new (worker-2 width/pool facts + worker-3 path) |
| B4 | rung 3 | Reflected method invocation 0–12 args (B4), out-param marshalling for `LineTraceSingle` hit table (B5), struct conversions (B6/B7), FName→string (B8) | IS:32–33, 59, 163–168, 233–234, 245–269, 295–303, 313, UMG setters IS:456–483, 531–561 | B5 out-params missing-new; call path depends-on-worker-3, addresses depends-on-worker-2 (none live-proven) |
| B5 | rung 4 | `LoadAsset` (A7) + deferred spawn family (C2) incl. spawn ownership/GC lifetime | IS:285, 295–297, 303; ownedActors IS:332 is Lua-side only | missing-new |
| B6 | rung 4.5 | UMG family D1–D5 + widget lifetime across map change (D5, ties to optional A10 map hook IS:731) | IS:499–645 (buildUI), 631–632/655/660 (viewport/visibility) | missing-new |
| B7 | rung 5 | Real device input source for the registered keys (A8) — keyboard on PS5 unproven (prior test was controller-only) | delivery of F7/arrows/RETURN into the six callbacks | missing-new |
| B8 | cross | Property read/write chains (B2/B3) incl. nested `pickup.PickupInventoryItem.ItemId` IS:313, `pc.Pawn` IS:31 | IS:31–33, 202, 313, 325–326, 470, 483, 525, 528 | depends-on-worker-2 + worker-3 |

## What breaks first — honest prediction, ordered by condition

0. **Today, bundle as specified: the run never reaches Lua.** B0 fires first: activation/
   setup throws on the missing layout keys. Any "first run" claimed without worker-2 layout
   data would be the placeholder file-load path, not a mod run. (This is the current
   first-breaker and it is ABOVE the ladder's rung 1.)
1. **Assuming B0 resolved and the runtime starts mods:** the LOAD phase should complete.
   Evidence: unchanged host startup passed with 6/6 binds registered in the retained trial,
   and in-game probes v1/v2 proved module loading + all imports (module quiesces inert).
   Expected load observable: exactly the IS:746 completion line (OBSERVABLES.md).
2. **First F7 press: B1 breaks at IS:695** — `attempt to call a nil value (global
   'ExecuteInGameThread')`, the exact baseline-observed failure. The callback dies
   unprotected: `toggleMenu` never runs, so NO mod log line follows the press (the error
   surfaces runtime-side, as the baseline findings captured). This remains the predicted
   first break until worker-3's delivery adapter lands.
3. **If B1 lands but B2 does not:** F7 → `showMenu(true)` → `scanItems` IS:651.
   Two sub-cases, both graceful: `FindFirstOf` present but no world/save → IS:194 log
   `"Scan failed: Item database not ready. Load a save first."` (error text IS:66);
   `FindFirstOf` itself missing → IS:194 log with the nil-global error text instead. Then
   `buildUI` → `getPlayerController` (IS:15 require chain, IS:21 fallback); without a PC it
   returns false SILENTLY at IS:494–497. Menu never opens; the mod stays inert. Phase-1
   still looks fully healthy in the log — the ladder stalls quietly.
4. **If B1+B2 land (save loaded, PC found): break moves to B6/B3 inside `buildUI`.**
   Predicted observable: log `"UI creation failed: <err>"` IS:636 (UMG D1–D5 missing-new;
   `FName`/`FText` gap B3 hits at IS:50/516/434 inside the same pcall). Menu still never
   becomes visible.
5. **If UMG somehow builds:** RETURN on a pickup row → `spawnSelected` IS:215. Predicted
   first break: B4/B5 at the `LineTraceSingle` call IS:245–258 — either the 12-arg UFunction
   invocation or the out-param `hit` marshalling read at IS:264 — BEFORE `LoadAsset`
   IS:285 and `BeginDeferredActorSpawnFromClass` IS:295 can be judged. Failure is
   pcall-captured: `"Failed at ground trace. See UE4SS.log."` IS:341–342.
6. **Standing caveat across all rungs: B7.** The six binds registering is proven; a real
   keyboard F7 arriving on PS5 is not. If no key event ever delivers, the entire Phase-K
   chain is unobservable and "nothing happened in the log" is ambiguous between "key never
   fired" and "died at IS:695" — see OBSERVABLES.md for the discriminator.

Per-item mapping to the four named missing-fact families (RUNTIME-REQUIREMENTS §F) is
unchanged by this analysis; this file adds only the ladder ordering and the break-point
predictions above. No blocker above may be closed by guessing addresses or widths.
