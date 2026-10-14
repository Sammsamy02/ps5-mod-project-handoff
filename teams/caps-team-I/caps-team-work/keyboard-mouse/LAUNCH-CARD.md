# LAUNCH CARD: USB keyboard and mouse (Team I2). K1 probe, then K2 capability

## In one sentence
K1 is a read-only Lua test mod. It shows whether Townfall's engine sees a USB
keyboard and mouse plugged into the PS5. It needs no build and changes nothing the
game runs.

## What it is, plainly
- Every console step below needs the user's OK for **that** step.
- About 30 times a second, the test mod asks the game's own player object: "is F8,
  A, Space, Enter, Shift, Ctrl, left click, right click, Cross down?"
- It writes one log line each time a key goes down or up, and a summary every
  10 seconds.
- It does not bind any key, does not open the keyboard and does not change the
  controller setup (`PS5Pad.ini` stays as it is).
- It can be its own launch, or ride along with the next launch that happens anyway
  (the integrator decides). If it rides along, the user must do the key steps below
  during that launch.

## What the integrator does (K1)
1. Copy `probe/zzkbprobe/` to the local mod staging as `ue4ss/Mods/zzkbprobe/`. It is
   an exact mirror: `Scripts/main.lua` and `enabled.txt` only.
2. Add two loadlist rows with exact sizes:
   ```
   file	lua_mod	ue4ss/Mods/zzkbprobe/Scripts/main.lua	2193
   file	lua_mod	ue4ss/Mods/zzkbprobe/enabled.txt	0
   ```
   main.lua sha256 must be
   `2e7fdd8e88303a4e46aa1d47d81d074b70d4113893f7ca2621a5bff66120ce29`
   (zzkbbind main.lua: `45604aa20090ba22ad61392b105b9092ff12136491a4c1c17a2eeb71f02d4551`). Run the normal
   loadlist row-check.
3. Upload (user OK), then read back with `cmp`. Then the normal pre-launch checks.
   No PRX change, so the runtime stays the current rollback build.

## What the user does (K1)
1. **Before starting the game**, plug a USB keyboard and a USB mouse into the PS5.
2. Start Townfall and get into normal gameplay (walking around, **not** a menu).
3. Press each of these 3 times, slowly: **F8, A, Space, Enter, left Shift, left Ctrl**.
4. Click the **left** and **right** mouse buttons 3 times each.
5. Press **Cross** on the controller 3 times (this is the built-in check).
6. Unplug the keyboard and the mouse. Press **Cross** 3 times again, and open the mod
   menu once with **L3+R3** as usual.
7. Optional: plug the keyboard back in and press **F8** 3 times. This tests hot-plug;
   whatever happens is a finding.
8. Optional: try **Esc**, the **Windows** key, **Print Screen** and **volume keys**
   once each, and note what the PS5 does.
9. Close the game, then confirm "game closed" before the log is read.

## What to look for (decoded log, grep `[kb]`)

| Result | Meaning |
|---|---|
| **K-PASS** | `[kb] Z0 v1 keys=10`, `[kb] PC found`, `[kb] CAP fn=1 rt=boolean`, then `[kb] DN F8 ...` / `[kb] UP F8 ...` (and the same for A, SpaceBar, Enter, LeftShift, LeftControl, LeftMouseButton, RightMouseButton, Gamepad_FaceButton_Bottom). After the unplug, the `SUM` lines keep `fail=0` and only Cross edges grow. Option C works on Townfall: go to K2. |
| **K-PARTIAL** | `Gamepad_FaceButton_Bottom` edges appear, but there are no keyboard or mouse edges. The probe works, but the engine does not get the keyboard here. Next is the option-A diagnostic (DESIGN.md section 9, step 3). |
| **K-NOPROBE** | `CAP fn=0 ...`, `STOP fail=30`, or never `PC found`. This is a probe problem, not a keyboard verdict; fix it on the Mac. |
| **Must hold in every case** | L3+R3 still opens the mod menu as in the last clean launch, with no new error lines and no stutter. `SUM ... polls=` grows by about 300 per 10 s during gameplay. |

The full marker list is in `probe/EXPECTED-MARKERS-zzkbprobe.txt`.

## If something looks wrong
- No `[kb] Z0` line: the mod was not admitted. Check both loadlist sizes (2193 / 0).
- Stutter, or any crash the last clean launch did not have: remove the
  `ue4ss/Mods/zzkbprobe/` folder and its 2 loadlist rows (user OK). The runtime is
  unchanged, so there is nothing else to roll back.

## K2: the capability launch (only after K1 PASS)
1. **Mac:** write the staged runtime patch (DESIGN.md section 5: the real
   KeyStateReader, the Merged source, and the sampler on the Tick pump). Then host
   tests (`prototype/run.sh`), ONE independent reviewer, and the gates. The import
   count must stay **207**. One build.
2. **Deploy:**
   - the new PRX, through the normal verify/manifest/readback rules;
   - `probe/zzkbbind/`, with rows `ue4ss/Mods/zzkbbind/Scripts/main.lua 570` and
     `ue4ss/Mods/zzkbbind/enabled.txt 0`.
   Keep `zzkbprobe` in place, as it is useful for comparison.
3. **User:** keyboard and mouse plugged in. In gameplay:
   - press F8 3 times, Shift+F8 3 times, numpad 5 3 times, right click 3 times;
   - press keyboard **F7** once (it should open the same mod menu as L3+R3);
   - then use the controller combinations as in the last clean launch;
   - unplug the keyboard and press L3+R3 once.
4. **PASS:**
   - exactly one `[kbb] FIRE F8` per F8 press and one `FIRE SHIFT+F8` per Shift+F8;
   - `FIRE NUM_FIVE` and `FIRE RIGHT_MOUSE_BUTTON` appear;
   - keyboard F7 opens the menu;
   - every pad combination behaves exactly as in the last clean launch, and the
     self-test result is not lower;
   - `[kbm] engine-keys supported=1` appears once, and `[kbm] SUM ... drops=0`.
5. **Rollback:** the last clean build.
