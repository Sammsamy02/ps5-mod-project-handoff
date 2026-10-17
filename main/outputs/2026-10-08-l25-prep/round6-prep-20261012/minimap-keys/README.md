# minimap-keys (round-6 prep, 2026-10-12): Mac-only, read-only

Round 2. This round fixes the five points in `REVIEW.md`. The verdict is the
same as round 1.

**Answer.** The minimap needs **no button press** to appear. Its 8 keys
(F5, F6, Insert, Delete, PageUp, PageDown, Home, End) **cannot be reached by
any PS5Pad.ini change** with the launched runtime (B4, PRX `d044cf0b`).
**Recommended change: none.** Keep PS5Pad.ini at 1736 B / `957d3e25…ed453`.
The loadlist row stays `file<TAB>config<TAB>ue4ss/PS5Pad.ini<TAB>1736`, so its
size does not change.

All claims below are Mac-only and cite file:line. I made no console contact and
read no console logs.

## Why no pad row can help

- The minimap registers **no UE4SS key binds**. The pinned main.lua has 0 uses
  of RegisterKeyBind, RegisterKeyBindAsync, IsKeyBindRegistered, Key.* or
  ModifierKey.*.
- It reads the **game engine's** keyboard state instead: `pc:IsInputKeyDown`
  (main.lua:4043, 3271, 3972), `WasInputKeyJustPressed` (3272, 3973) and
  `GetInputAnalogKeyState` (3994).
- The pad bridge creates a key event only for a key that some mod subscribed
  through RegisterKeyBind (PS5PadSource.cpp:164 `allow && subscribed[b.key]`).
  Subscriptions are set only at Handler.cpp:191 and :208. The event is passed
  only to RegisterKeyBind callbacks (Handler.cpp:155-162).
- Nothing sends keys into the engine's input. The scan found 0 injection calls
  in 404 distinct files: the 378 sources composed into B4 plus the rest of the
  l26 candidate tree. Limit: headers outside that tree were not scanned.
- Provenance (round-2 fix). The pad and input sources are the exact files that
  B4 composed. The chain is checked by `minimap_keys_check.py` check 5:
  - the B4 PRX sha comes from two sources: BUILD-LEDGER line 9 and the frozen
    `.prx.sha256` file. The checker also hashes the frozen PRX.
  - frozen FINAL-RECEIPT -> runtime object `732aef21`
  - `2026-10-08-l26-package-A/BUILD-INPUTS.json` -> l26-A pass1/pass2
    COMPOSITION-RECEIPT (`0f5e2745` / `e6234ec7`)
  - The receipts pin these unit sources: PS5PadSource 6a0633ca, pad-config
    5303f351, native-pad-input 03c9f6d0, input-handler 16fcfe49, keydef
    5d27a2c7, PS5PadNative 345d3680.
  - The older l25-A receipts pin the same 6 shas. They are used only as a
    cross-check.
  - The B4 symbol table contains `parse_ps5_pad_config`, `PS5PadSource::poll`
    and the shim's `io_open_wrapper`.
- The generator's own checker (keymap-gen.py --check) flags every minimap row
  in the candidate as "orphan row (no enabled mod wants this key)".

## Does it show without a key press? (round-2 fix: every gate listed)

By design, yes. The defaults are enabled (main.lua:78) and "keep minimap
without a map" on (149). It uses a trusted default layout (1996, 4181). The
tick runs every 50 ms (9, 4268) and shows the map only if **all** of these
pass, in this order:

1. A local PlayerController exists (304-308, 4064). If it is missing, the map
   is hidden (4068).
2. All 8 `IsInputKeyDown` polls return without error (4037-4053, 4084). If one
   fails, the map is hidden and `S.pc` is cleared (4045).
3. The pawn has a valid `MapRenderComponent` (312-316). If it does not, the map
   is hidden and the controller is searched for again about once a second
   (4090-4091).
4. The widget is built from the game's MapUserWidget class (1118, 4113-4119).
5. These calls do not throw: `comp:IsMapOpen()` (4136), and the game and HUD
   state reads (4140). A throw is caught at 4242, and the next tick rebuilds the
   widget (4245, 4113-4116).
6. The HUD is in a play state or map state, and is not hidden (72-74, 4173).

The map then shows as `S.enabled and (snapshot or S.blank or W.active) and hudOk`
(4182-4188, applied at 4238). It shows a blank "paper" map with markers until
the player opens the game's own map once (1525, 4174). On the first show
only, a help box listing **keyboard** keys appears for about 7 s (14, 4213). It
can be ignored.

## Risks for "no stutter" (round-2 fix: new)

- If `IsInputKeyDown` fails on PS5, then **every 50 ms** the map hides itself
  and `S.pc` is cleared (4045). The next tick runs `FindAllOf("PlayerController")`
  again (305-307). That is about 20 full object searches a second, which could
  stutter. What one call costs on PS5 is unknown (Mac-only). The self-test
  times one `FindAllOf("PlayerController")` call (ueselftest part5.lua:6,
  sha-verified against its pin), so that timing times 20 gives the per-second
  cost.
- If a tick keeps throwing after the polls (for example at 4136), the widget is
  destroyed and rebuilt on every tick (4245, 4113-4116).
- Gate 3 failing (no MapRenderComponent, for example in menus) causes one
  controller search per second (4090). That is light.

## Reading this launch's log (round-2 fix: a rule for the driver, not a verdict)

The reviewer pointed to a line in the B4 launch-1 harvest report
(`HARVEST-l26-B4-launch1-20261012.md:2752`). I **did not open** it, because my
rules forbid reading console logs. Use this code-based rule on this launch's
decoded log (`harvest/decode_session_log.py`), never on an earlier session's
log:

- Look for `[SHIM] io-open resolved=ue4ss/Mods/TownfallMiniMap\settings.txt-><target>`.
  - If `<target>` is under the **writable root**, it was a **write** open. That
    is the root named by the `[SHIM] write-area TownfallMiniMap -> …` line
    (LuaPathShim.cpp:851-855).
  - If `<target>` is under the **mod root**, it was a **read** open
    (LuaPathShim.cpp:871-875, from loadSettings at main.lua:251). A read only
    proves that the mod loaded.
- The minimap opens exactly one file, and only twice in the code: a read at 251
  and a write at 293. With no keyboard, the only route to that write is the
  first-show block (4211-4214). Every other `saveSettings()` caller needs an
  engine key-down (`onKey`, reached only from `pollKeys` at 4049) or the open
  F5 menu. `M.open = true` is set only at 3916, inside `M.openMenu`, which is
  called only from `onKey` (3414). This call graph is checked by check 9.
- So a **write** marker means that, in that tick, gates 1-6 all passed. In
  particular `IsInputKeyDown` worked, and show became true.
- It does **not** prove the map stayed visible, or that there was no stutter.
- **No** write marker does not prove failure either:
  - once a `settings.txt` with `hintShown=true` exists in the writable area,
    the first-show write never happens again (275, 4211);
  - markers are capped at 10 per Lua state (LuaPathShim.cpp:92).

## Minimap keys (pinned main.lua, 196,743 B, sha256 d8f8d64f…c920f)

| Key (engine name) | UE4SS name | What it does (main.lua) |
|---|---|---|
| F5 | F5 | settings menu open/close; pauses the game (3412-3414) |
| F6 | F6 | Expanded Dual map open/close (3417-3418) |
| Insert | INS | show/hide the minimap, saved to settings.txt (3425-3427) |
| Delete | DEL | map side front/back/split/auto (3436-3443) |
| PageUp / PageDown | PAGE_UP / PAGE_DOWN | zoom in / out (3428-3435) |
| Home / End | HOME / END | map size bigger / smaller (3421-3424) |
| Up Down Left Right Enter Tab, mouse click/wheel | (none) | used only inside the F5 menu (3958-3959) |
| Tab | (none) | closes the Expanded map (3263, 3279) |

The only config it reads is `<mod folder>\settings.txt` (241-288). That file
holds display settings and no key bindings; the keys are fixed in the code
(66-67).

## Pad file facts (pinned PS5Pad.ini, 1736 B)

Rows, confirmed against the launch card: L3+R3=F7 (line 22), UP/DOWN/LEFT/RIGHT
= arrows (23, 24, 26, 27), CROSS=RETURN (25), L1+R1=F12 (31).

Syntax (PS5PadConfig.cpp):
- Rows are `CHORD = [SHIFT|CONTROL|ALT+]KEY` under `[Bindings]`.
- There are 16 button names (41-46).
- Comments go only on their own lines (69).
- Each row has exactly one '=' (77).
- At most 128 rows (91).
- No duplicate chord, and no chord that contains, or sits inside, another
  chord (115-120).

Free under the project policy (TOUCH_PAD L1 R1 L2 R2 only): 9 pairs.
TOUCH_PAD+L1, TOUCH_PAD+R1, TOUCH_PAD+L2, TOUCH_PAD+R2, L1+R2, L1+L2, R1+L2,
R1+R2, L2+R2.

## Files

| File | What it is |
|---|---|
| `minimap_keys_check.py` | Checker (stdlib, `python3 -I`). It re-verifies every claim above in 9 checks. Last run: PASS, exit 0. It checked 66,171 items: **636 read from files**, plus 65,535 button combinations generated in memory (counted separately). A check that reads zero items from files fails, and so does any failure; either exits 1. `--result NAME` writes the result inside this folder only. `--write-candidate` rebuilds the candidate. |
| `check-result.json` | Result of the last real-tree run: counts per check, cited lines, the B4 provenance chain, the call graph and the driver rule. |
| `checker-run.txt` | Console output of that run. |
| `negative-test/` | Negative runs. `--root` was set to an empty folder (`emptyroot/`) and to a missing path. All 9 checks FAIL with "zero items checked from files", 0 items in total, exit 1. Each run's `.txt` output and `.json` result are kept. |
| `PS5Pad.FUTURE-minimap-DO-NOT-UPLOAD.ini` | **Future-only, inert today. Do not upload.** It holds the 7 pinned rows plus 8 minimap rows placed by the generator's FV1 rule. **1669 B, sha256 `6f81b8e29511c3fded9e18c52b7d65b56794b116849351ce624c4f7b4cd39c6b`.** If it were ever used, the loadlist row would go from 1736 to 1669 (-67 B). The loadlist would stay 1772 B, its sha256 would change from e1dbfe43… to 5483769c…, and rowcheck-pins.json would need both new pins. |
| `generator-run/PS5Pad.regenerated-7mods.ini` | keymap-gen.py output for all 7 Lua mods, the minimap included. It is identical to the pinned file apart from its inputs-sha256 comment line. |
| `generator-run/keymap-gen-runs.txt` | Round-1 generator runs. |
| `generator-run/rerun_keymap_checks.py`, `generator-run/keymap-gen-runs.round2.txt` | Round-2 re-run of `keymap-gen.py --check` with the 7 sha-verified mods. Pinned file: 0 violations, 0 warnings. Candidate: 0 violations, 8 orphan warnings. PASS, exit 0. |
| `previous-round/` | Round-1 checker, result and README, kept unchanged. |
| `REVIEW.md` | The adversarial review of round 1 (written by the reviewer). |

## Round-2 changes (from REVIEW.md)

1. Provenance now follows the B4 build, not the l25-A receipt. If BUILD-INPUTS
   has moved to `archive-r6/B4-shim-final-stack/`, the checker finds it there
   by sha.
2. The negative run is now honest. Check 4 checks zero items when the pinned
   rows are missing. Generated combinations are reported apart from file items.
3. The full list of show gates is above, plus the stutter risks.
4. There is a log-reading rule for the driver. I did not apply it to the B4
   launch-1 harvest.
5. The two round-1 temp files in the harness scratchpad were removed:
   `emptyroot/` (empty) and `check-result.good.json` (byte-identical to the
   round-1 result). All round-2 temp output stays in this folder.

Side note for the driver, not changed by me: the runtime file
LuaPathShim.cpp:211 has a **comment** that names "MiniMap". It is a comment
only, with no code effect, but the generic-runtime rule may want it reworded.

Making the keys work later needs a **generic runtime feature**, not a config
change. While a chord is held, the runtime would press the mapped key inside
the engine's own input. The minimap polls key-down every 50 ms (4043, 4268), so
a one-shot event would be missed. Shipped values would need two sources, and
the user must decide whether to do this.
