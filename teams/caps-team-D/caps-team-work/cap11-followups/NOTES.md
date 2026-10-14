# cap11 follow-ups: sharp C++ mod window + 4 fixes (Team D, 2026-10-13)

All of this is Mac-only. Nothing here has run on the console yet.

## What you will see

- **Sharp text on the 4K TV.** The C++ mod window ("UE4SS (PS5 Mod Bridge)") stays the same size and in the
  same place as in launch G, but the letters are crisp instead of soft (boxes and lines keep a thin soft edge).
  - Before: the window was drawn as a 1100 x 864 picture and the TV stretched it 2x. The letters also came
    from a 13-pixel font blown up to about 40 pixels, so they were 3x blown up in total.
  - Now: the window is drawn as a 2200 x 1728 picture (the TV's real pixels), and the font is made at the
    exact size it is shown. One font pixel = one TV pixel (host test: texel ratio 1.00, was 3.08).
  - On a 1080p TV the text gets sharper too (it was a 13-pixel font stretched to 20).
  - On a 1440p screen (scale 1.33) the font is also made at the right density, but letters land between
    pixels, so text there is a little softer than at 4K or 1080p. Lines and boxes keep slightly soft edges
    (ImGui's own anti-aliasing), only the text is pixel-exact.
- **If the window cannot be shown** (for example during a loading screen, or if building it fails part-way),
  your controls are no longer frozen behind an invisible window. Movement and camera come back until the
  window can be shown again. Closing the window always gives the controls back.
- **If the screen size or UI scale changes** while the game runs, the window is laid out again the next time
  you open it, or within about 2 seconds if it is open.
- **C++ mods run their per-frame code once per frame.** Before, it ran on every runtime "pump" call
  (about 170 to 1,960 times a second). Mods that count frames or move things each frame now run at the right
  speed and use less CPU. Lua mods are not affected.
- If the game cannot report its screen scale, you get **one warning line**, not an error line, and the
  window still works (at scale 1.0).
- **Safety limits (fail closed):** if the window cannot be built 3 times in a row, it stops trying (one warning)
  until you reopen it; retries reuse the picture they already made, so memory cannot pile up. If a mod adds a
  very large font (thousands of letters), the sharp font is skipped (one warning, text looks like launch G),
  and a font picture larger than 4096 x 4096 is never uploaded (the window turns off for that session instead
  of stalling the game).

## Memory (Mac-only estimate)

- The window picture at 4K: 2200 x 1728 = 3.8 million pixels.
  - 15.2 MB if the engine stores it as 8-bit colour, **30.4 MB** if it uses Unreal's default render-target
    format for this class (16-bit float colour, 8 bytes per pixel). The format is not read from the engine,
    so take 30.4 MB as the planning number.
  - Before it was 3.8 MB / 7.6 MB, so the extra cost is **about 11 to 23 MB of GPU memory**.
  - It is created the first time you open the window and kept until the next map change (as before).
- Font picture: 512 x 512 (was 512 x 128): about 1 MB on the GPU, 0.25 MB on the CPU.
- Safety cap: a window picture larger than 4096 pixels on a side is never made. Above that (only on screens
  with UI scale above about 3.7) the window falls back to the old size and logs one warning.

## What to look for in the next launch log (expected lines)

- `[GUI] surfaces built panel=1100x864 rt=2200x1728 viewport=3840x2160 scale=2.000000 units=1920x1080 pos=772,108`
- `[GUI] font baked size=20 density=2.000000 atlas=512x512`
- `[GUI] atlas route=png-import bytes=...` and `[GUI] atlas uploaded runs=...` (once per open after a map change)
- No `[GUI] refused: ... GetViewportScale` line.
- For the #9 C++ mod probe: tick callbacks at about the frame rate (about 60 per second), not hundreds.

## What changed (files)

| File (typescan-r4/...) | Change |
|---|---|
| include/Platform/NativeGuiHost.hpp | renders at the backend's pixel scale; bakes the font for that density; releases the input block on a frame where the window cannot be shown |
| include/Platform/NativeModHostCore.hpp | `Host::game_thread_pump`: one shared once-per-frame gate for the C++ mod frame |
| candidate/src/Mod/NativeGuiEngineBackend.inc | render target in pixels (cap 4096), `m_built` guard, 2 s retry after a failed build, quiet scale lookup (one warning), layout re-check |
| candidate/src/Mod/NativeModEngineServices.inc | the pump calls go through `game_thread_pump` (no per-pump mod tick) |

No new import, no new engine field read, no new filesystem call, no detour, no game- or mod-specific code.

## Tests (Mac-only)

- Team D's cap09 + cap11 host suites against the **live** headers: 13/13 PASS, 271 compiled checks (no test
  needed fixing: the only stale copy was NativeModHostCore.hpp's removed `<exception>`, which no test relied on;
  the include proof shows every project header now comes from the live tree).
- The same suites + the new follow-up test (49 checks) with the patch: 14/14 PASS, 320 compiled checks.
- One independent review: PASS-WITH-FIXES, 0 blockers; 7 of 8 findings fixed, 1 kept on purpose (FIXES.md).
- Target compile of the changed unit with R15e's own command: PASS, **0 new undefined symbols**, 0 duplicates.
- Field gate: the same 49 engine fields as the live tree (48 proven + 1 accepted). No-detour gate: PASS.
  fs-throw gate: 0 sites in the changed files.
