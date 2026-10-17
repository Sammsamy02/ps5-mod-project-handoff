# Townfall launch card — morning of 2026-10-12

Goal: one clean launch with ALL mods (including the minimap), self-test once.
Townfall is done after 2 clean launches.

## Before the launch (Claude does these; each needs your OK, game CLOSED)

1. You: turn on the PS5 and jailbreak as usual. Make sure Townfall is **closed**.
   Tell Claude "game is closed".
2. Claude, one step at a time with your OK:
   a. read-only check of the console (nothing changes)
   b. upload the 12 self-test files + the new loadlist, read each one back
      (`deploy_mirror.py --upload`), then list and (with your OK) remove any
      old self-test files that are not in the new set
   c. create the per-mod writable folders (`deploy-precreate-writable.py`)
   d. back up the current runtime PRX, upload the new one, read it back with
      decrypt OFF (must show `4f153d1d` and the exact sha), check all 30
      loadlist rows, reload the plugin loader, start the kernel log capture
      (`prep_modded_launch.py`)
3. Claude says "ready". Nothing else changes after that.

## The launch (you)

1. Start Townfall once. Load your save. Wait about a minute after you are in
   the world.
2. Check without pressing anything:
   - **Minimap** is on screen.
   - **Infinite stamina**: sprint; the stamina bar should not run out.
   - Walk around for a minute: note any **stutter** (pauses or hitching).
3. **Item spawner**: press **L3 + R3** (both sticks in) to open the menu.
   D-pad **up/down** moves, **left/right** changes page, **Cross** spawns.
   Spawn one item, close the menu (L3 + R3 again).
4. **Self-test**: press **L1 + R1 once** (if you don't, it starts by itself
   3 minutes after the mods load). It runs only once per launch (pressing
   again is ignored). Keep playing normally for about 2 minutes while it runs.
5. Try the spawner menu once more after the self-test (it used to stick).
6. Quit the game normally (PS button -> close game). Tell Claude "game closed".

Buttons not mapped on the controller: the minimap's own keys (F5 menu, F6 full
map, Insert, Delete) and the developer console key. Mapping them would be a
config change; ask if you want it.

## After the launch (Claude, read-only)

Read the logs with the game closed, decode them, write the report (per-mod
verdict with this session's log lines, self-test PASS/FAIL/SLOW + GAPS table,
lookup timings, memory lines).

If the game crashes or the spawner breaks: the earlier labelled builds are kept
(B3, B2b, B1, 1e49188b) so the next launch can step back one build.
