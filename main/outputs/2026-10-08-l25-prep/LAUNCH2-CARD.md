# LAUNCH-2 CARD — final stack 4d3f5422 — ladder v2 + self-test

Setup: ploader → OK → launch Townfall → load same save → get IN-WORLD.

## Ladder mods — what to press and look for (in-game, any order)
1. CHEAT MANAGER (CheatManagerEnabler): loads at save-load. LOOK: no key;
   works silently. Proof = the next mod's console (below) responding.
2. CONSOLE ENABLE (ConsoleEnabler): TILDE (~) or F10 opens the game
   console. LOOK: console overlay appears, you can type. This proves the
   value/array-write mod class end-to-end.
3. MINIMAP (TownfallMiniMap): LOOK: a map overlay on screen while playing
   (its 50ms tick = the sustained-tick class). If the map draws and
   updates while you walk: PASS.
4. STAMINA (InfiniteStamina): LOOK: stamina never depletes while
   sprinting/fighting. (Engine-level, always on.)
5. ITEM SPAWNER (F7): the proven menu — browse, spawn an item, pick it up.

## Self-test LAST: press L1+R1 together
- Runs ~10-30 seconds; 168 calls each logged.
- STARTS: a line "[ST2] RUN start 1..167" appears in the log (you won't
  see it on screen — it just takes a few seconds).
- FINISHED when: the game responds normally again and the log ends with
  "[ST2] SUMMARY <N> of 168 PASS" + "[ST2] done".
- Nothing visible should change; no menu, no side effects.

## zzdiag auto-runs (no buttons needed)
- Every 6s a probe line; [MAP] name changes at map loads; at +18s the
  pickup probe (P0-P8) runs automatically, including the new fname-eq
  (PRB-7) and UEHelpers.GetWorld (PRB-8) checks.

Clean exit to dashboard when done. Report any CE code + when it happened.
