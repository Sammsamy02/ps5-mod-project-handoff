# LAUNCH CARD K2: USB keyboard and mouse for mod key binds (Team I2)

## In one sentence
After this build, a mod that says "when F8 is pressed, do this" also works with a real USB keyboard (and mouse
buttons) plugged into the PS5, next to the controller button combinations, which keep working exactly as before.

## What it is, plainly
- The game already reads the USB keyboard and mouse itself. The runtime does **not** open them.
- Once per game frame (never more than about 120 times a second), the runtime asks the game "is F8 down?" for the
  keys that mods use, and turns a new key press into the same kind of key event a controller combination makes.
- If anything about the game's answer looks wrong, the keyboard part switches itself off and only the controller
  combinations are left (exactly as today). With no keyboard plugged in, nothing happens.
- It is one build and one launch. Every console step below needs the user's OK for **that** step.
- The K1 probe (`zzkbprobe`) has not run yet. K2 is safe either way; if the game does not see the keyboard,
  this launch shows it (verdict K2-NOKEYS below) and nothing breaks.

## What the integrator does (before the launch)
1. Apply the patch (`APPLY.md`), run the gates (expected numbers are in `APPLY.md`), one build, the normal
   verify_build / manifest / readback rules. The import count must not change.
2. Deploy the test mod `zzkbbind` (prints a line each time one of its key binds fires):
   ```
   file	lua_mod	ue4ss/Mods/zzkbbind/Scripts/main.lua	570
   file	lua_mod	ue4ss/Mods/zzkbbind/enabled.txt	0
   ```
   main.lua sha256 `45604aa20090ba22ad61392b105b9092ff12136491a4c1c17a2eeb71f02d4551`. Exact mirror, then the
   row-check. It binds: **F8**, **Shift+F8**, **numpad 5**, **right mouse button**. None of these is a controller
   combination in `PS5Pad.ini`, so every `FIRE` line comes from the keyboard or mouse.
3. If `zzkbprobe` is installed from K1, leave it; it is a useful second opinion.

## What the user does
1. **Before starting the game**, plug a USB keyboard and a USB mouse into the PS5. Turn **Num Lock on** (its light
   on) so numpad 5 is "5".
2. Start Townfall and get into normal gameplay (walking around, not a menu).
3. Press, slowly, with a short pause between presses:
   - **F8** three times;
   - **Shift+F8** three times (hold Shift, tap F8);
   - **numpad 5** three times;
   - **right mouse button** three times.
4. Press keyboard **F7** once. The mod menu should open, the same as **L3+R3**. Close it the usual way.
5. Use the controller combinations as in the last clean launch (L3+R3 menu, the menu buttons).
6. Unplug the keyboard and the mouse. Press **L3+R3** once more.
7. Close the game, then confirm "game closed" before the log is read.

## What proves PASS (decoded log; grep `[kbm]` and `[kbb]`)

| Check | Line(s) that prove it |
|---|---|
| The keyboard part started | exactly one `[kbm] engine-keys supported=1 reason=ok bound=<n>` (n is at least 3; it counts every bound key of every mod that the engine has a name for) |
| F8 works, once per press | `[kbb] FIRE F8 n=1`, `n=2`, `n=3` ... one line per press, no doubles |
| Shift is respected | each Shift+F8 press gives one `[kbb] FIRE SHIFT+F8` and **no** `FIRE F8` for that press |
| Numpad and mouse | `[kbb] FIRE NUM_FIVE` and `[kbb] FIRE RIGHT_MOUSE_BUTTON` lines |
| Keyboard F7 = L3+R3 | the mod menu opens on keyboard F7 |
| It stays cheap | one `[kbm] SUM frames=... sampled=... reads=... fails=0 events=... drops=0 divisor=1` line per minute; `events` grows only while keys are pressed |
| Controller unchanged | every pad combination works exactly as in the last clean launch; no new error lines; no stutter; the self-test result is not lower |
| Unplugged = quiet | after the unplug, later `SUM` lines show `events` no longer growing and `fails=0` |
| Never | no `[kbm] engine-keys stopped ...` line |

## Other verdicts
| Verdict | What the log shows | Meaning / next step |
|---|---|---|
| **K2-PASS** | everything in the table above | Keyboard and mouse binds work on Townfall. |
| **K2-NOKEYS** | `supported=1`, but no `FIRE` lines while typing | The runtime route works, but the game's engine does not get the keyboard here (same as K1 K-PARTIAL). Pad unaffected. Next: the device-route diagnostic (DESIGN.md section 9, step 3). |
| **K2-REFUSED** | `[kbm] engine-keys supported=0 reason=<...>` | A safety check said no (the reason names which). The keyboard part is off, the pad is unaffected. Fix on the Mac. |
| **K2-STOPPED** | `[kbm] engine-keys stopped reason=<...>` after `supported=1` | It switched itself off during the session. Pad unaffected. Fix on the Mac. |
| **K2-WAITING** | `[kbm] engine-keys waiting reason=no-controller bound=<n>` and no `supported=` line | In the first minute the runtime found no local player controller to ask. If `supported=1` follows later, that is fine (it was a long loading screen). If it never follows, fix on the Mac. (`reason=no-binds` only means no mod bound a key yet.) |
| **K2-NOLINE** | no `[kbm]` line at all | The new code did not run (check verify_build markers `k2-kbm-*`). |
| **SLOW** | `SUM ... divisor=2`, `4` or `8` | The game answered too slowly, so the runtime sampled less often by itself. Not a failure unless there is stutter; report it. |

Note for the report: keyboard presses also create `pad_key_edge` diagnostic tags. In this launch those tags do
not prove controller input; use the controller-only steps (5, 6) for that.

## If something goes wrong
- Stutter, a crash, or any controller combination behaving differently from the last clean launch: stop,
  roll back to the last clean build (user OK for each step). The pad chords do not depend on the keyboard part,
  so this would be a new finding: keep the log.
- No `[kbb] Z0 v1 binds=4` line: the test mod was not admitted. Check both loadlist sizes (570 / 0).
- Known limits (not failures): keys may not arrive while a game menu takes all keyboard input (a text box, a
  "UI only" screen); F13-F24, media and IME keys do not exist in the engine; a bind fires on press only, with no
  auto-repeat (the same as UE4SS on PC); mouse movement and a mouse pointer for mod windows are a later step.

## Rollback
The last clean build.
