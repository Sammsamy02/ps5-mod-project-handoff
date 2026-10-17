# minimap-keys (round-6 prep, 2026-10-12) — Mac-only, read-only

**Answer.** The minimap needs **no button press** to appear. Its 8 keys
(F5, F6, Insert, Delete, PageUp, PageDown, Home, End) **cannot be reached by
any PS5Pad.ini change** with the current runtime. **Recommended change: none.**
Keep PS5Pad.ini at 1736 B / `957d3e25…ed453`. The loadlist row stays
`file<TAB>config<TAB>ue4ss/PS5Pad.ini<TAB>1736` (no size change).

Why (all Mac-only, citing file:line):
- The minimap registers **no UE4SS key binds**: 0 RegisterKeyBind /
  RegisterKeyBindAsync / IsKeyBindRegistered / Key.* / ModifierKey.* in the
  pinned main.lua. It reads the **game engine's** keyboard state instead:
  `pc:IsInputKeyDown` (main.lua:4043, 3271, 3972), `WasInputKeyJustPressed`
  (3272, 3973) and `GetInputAnalogKeyState` (3994).
- The pad bridge creates a key event only when some mod has subscribed that
  key through RegisterKeyBind (PS5PadSource.cpp:164 `allow && subscribed[b.key]`;
  subscriptions are set only at Handler.cpp:191/208). The event goes only to
  RegisterKeyBind callbacks (Handler.cpp:147-173). Nothing feeds the engine's
  input: 0 injection calls across the 427 composed + l26 candidate source files.
- The generator's own checker (keymap-gen.py --check) flags every minimap row
  in the candidate as "orphan row (no enabled mod wants this key)".

Does it show without a key press? By design, yes. The defaults are: enabled
(main.lua:78), 560 px, top-right, split view, "keep minimap without a map" on
(149), and a trusted default layout (1996, 4181). So a blank "paper" minimap
with markers should appear in play (4182-4188). It switches to the real map
picture after the player opens the game's own map once (1525, 4174). The first
time it shows, a help box listing keyboard keys appears for about 7 s (4213);
it can be ignored. **Risk (not console-proven):** each 50 ms tick calls
IsInputKeyDown with a key struct. If that call fails on PS5, the minimap hides
itself (4044-4047, 4084). The self-test does not cover that call.

## Minimap keys (pinned main.lua, 196,743 B, sha256 d8f8d64f…c920f)

| Key (engine name) | UE4SS name | What it does (main.lua) |
|---|---|---|
| F5 | F5 | settings menu open/close; pauses the game (3412-3414) |
| F6 | F6 | Expanded Dual map open/close (3417-3418) |
| Insert | INS | show/hide the minimap, saved to settings.txt (3425-3427) |
| Delete | DEL | map side front/back/split/auto (3436-3443) |
| PageUp / PageDown | PAGE_UP / PAGE_DOWN | zoom in / out (3428-3435) |
| Home / End | HOME / END | map size bigger / smaller (3421-3424) |
| Up Down Left Right Enter Tab, mouse click/wheel | — | used only inside the F5 menu (3958-3959) |
| Tab | — | closes the Expanded map (3263, 3279) |

Config it reads: only `<mod folder>\settings.txt` (241-288). It holds display
settings (enabled, zoom, side, mapPx, mapPos, opacity, show.*). It holds no key
bindings: the keys are fixed in the code (66-67). The path shim sends writes to
the per-mod writable area (LuaPathShim.cpp:211, 832-868). It is unknown
whether a pre-placed settings.txt would be readable at runtime, and none is
needed.

## Pad file facts (pinned PS5Pad.ini, 1736 B)

Rows (confirmed against the launch card): L3+R3=F7 (line 22), UP/DOWN/LEFT/RIGHT
= arrows (23, 24, 26, 27), CROSS=RETURN (25), L1+R1=F12 (31).

Syntax (PS5PadConfig.cpp): `CHORD = [SHIFT|CONTROL|ALT+]KEY` under `[Bindings]`.
16 button names (41-46). Comments only on their own lines (69). Exactly one
'=' per row (77). At most 128 rows (91). No duplicate chord, and no chord that
contains or sits inside another chord (115-120). Free under the project policy
(TOUCH_PAD L1 R1 L2 R2 only): 9 pairs: TOUCH_PAD+L1, TOUCH_PAD+R1, TOUCH_PAD+L2,
TOUCH_PAD+R2, L1+R2, L1+L2, R1+L2, R1+R2, L2+R2.

## Files

| File | What it is |
|---|---|
| `minimap_keys_check.py` | Checker. `python3 -I minimap_keys_check.py` re-verifies every claim above: 8 checks, 66,096 items, exit 0 = PASS. A zero count or any failure exits 1. `--write-candidate` rebuilds the candidate. |
| `check-result.json` | Output of the last checker run: counts, cited lines, key tables, candidate numbers. |
| `PS5Pad.FUTURE-minimap-DO-NOT-UPLOAD.ini` | **Future-only, inert today. Do not upload.** The 7 pinned rows plus 8 minimap rows, placed by the generator's FV1 rule. **1669 B, sha256 `6f81b8e29511c3fded9e18c52b7d65b56794b116849351ce624c4f7b4cd39c6b`.** If it were ever used, the loadlist row would go 1736 -> 1669 (-67 B: the shorter header outweighs the 8 new rows). The loadlist would stay 1772 B, with sha256 e1dbfe43… -> 5483769c…, and rowcheck-pins.json would need both new pins. |
| `generator-run/PS5Pad.regenerated-7mods.ini` | keymap-gen.py output for all 7 Lua mods of this launch, including the minimap. It is identical to the pinned file apart from its inputs-sha256 comment line. |
| `generator-run/keymap-gen-runs.txt` | Output of the generator runs: pinned file 0 violations / 0 warnings; candidate 0 violations / 8 orphan-row warnings. |

Making the keys work later needs a **generic runtime feature**, not a config
change. That feature would press and release the mapped key inside the engine
for as long as the chord is held. The minimap polls every 50 ms (4043, 4268),
so a one-shot event would be missed. Values that ship would need two sources,
and the user must decide whether to do this.
