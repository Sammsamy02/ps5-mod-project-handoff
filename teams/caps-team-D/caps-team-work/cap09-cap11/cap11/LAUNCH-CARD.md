# Launch card — #11 C++ mod windows (one launch)

**What this tests:** the UE4SS-style mod window. A C++ mod adds a tab; the user opens the window with a
button pair, moves with the d-pad, presses a button in the window (the player gets twice as fast), closes
the window and plays on. The window must also work again after going through a door / level change.

**Good to know (plain words):**
- While the window is open, walking and looking are blocked, but **CROSS and the d-pad still do their normal
  game action too** (same as the item spawner menu today).
- **Mod file rebuilt after the ship review fixes (Mac-only, 2026-10-13):** `main.prx` sha256 `ba4675085c601a6ce8dbaafdc4cb62e244344c8484f23d080918c4d72cc0b5ad`, 1774841 bytes, magic
  4f153d1d, 99 imports, all already imported by R11, none new; pass-1 == pass-2; `verify_build --manifest` PASS
  (`cap09/build-out/BUILD-RECEIPT.md`, 14 declared markers incl. the F7 refusal text). Never loaded on a console
  yet. The earlier file `c319ccba…` is superseded: do not upload it (an old host/mod pair would not match: the
  mod now also checks the ImGui context size and config fingerprint, F7).
- **Order:** the project order runs #11 before #9, but #11 uses #9's loader. If #9 has not launched yet, this
  launch is also the first C++ mod load, and the #9 "log stops after `[NMOD] loading C++ mod`" FAIL applies.

**Build:** current runtime + cap09 + cap11 (one build). Rollback = **R11 `bfd95db6`**.

## Before the launch (integrator)

1. `cap09/tests/run.sh` (8 suites) and `cap11/tests/run.sh` (11 suites) PASS, `run_gates.sh` PASS, one build,
   `verify_build --manifest` PASS for the runtime PRX, imports still 207.
2. Mod file: `verify_build` + manifest gate PASS for the **exact sha of `NativeGuiProbe/dlls/main.prx`** =
   `ba4675085c601a6ce8dbaafdc4cb62e244344c8484f23d080918c4d72cc0b5ad` (1774841 bytes; file `cap11/build-out/NativeGuiProbe/final-pass-1/main.prx`; manifest
   `cap11/build-out/verify-manifest-NativeGuiProbe.json`), and of the #9 probe's (`a394b76d17ac08ab…`, `cap09/LAUNCH-CARD.md`) if it stays
   installed. Any other sha = no upload. One independent review of `cap09/build-out/recipe/mbm_mod_module_entry.cpp`
   first (shared by both mod files).
3. Upload (user OK) `main.prx` to `<mods directory>/NativeGuiProbe/dlls/main.prx` plus an empty `enabled.txt`;
   readback with decrypt off (full sha `ba4675085c601a6ce8dbaafdc4cb62e244344c8484f23d080918c4d72cc0b5ad`, magic 4f153d1d). Loadlist: add **only** `file	config	ue4ss/Mods/NativeGuiProbe/enabled.txt	0`
   (never a row for `main.prx`).
4. Pad file: the only free button pair is used for the window: `L1+R2 = F10`
   (`cap11/PATCH/keymap/proposed/PS5Pad.cap11.ini`, 1986 bytes; check it against the console's current pad file
   first). No F9 row in this launch; no tab-switch keys (only one tab). Re-pin the size row, row-check PASS.
5. All 7 Lua mods stay installed. User OK for each console action.

## What the user does

1. Start Townfall, load the save, wait until you can walk.
2. Press **L1 + R2 together**: a dark window titled "UE4SS (PS5 Mod Bridge)" appears on the right.
   Try to walk: the character should **not** walk or turn.
3. Press d-pad **down** once (a highlight appears on "Double player speed"), then **CROSS**. If the highlight
   is on another line, move it with up/down first.
4. Press **L1 + R2** again: the window closes. Walk: the character is now fast.
5. Go through a door or level change. Open the window again (**L1 + R2**): it must show its text again.
   Press the button again (normal speed), close the window.
6. Use the minimap or item spawner once. Quit, close the game.
7. Tell us: was the text readable? Did anything flicker or look wrong? Did walking ever stay blocked?

## PASS lines (decode the log first)

| Line | Means |
|---|---|
| `[GUI] context up imgui=1.92.1 atlas=...` and `[GUI] host ready toggle=F10 on_ui_init_mods=...` | window system started |
| `[NGUI] constructed`, `[NGUI] ui_init imgui=ok` | the mod shares the window system |
| `[GUI] opened`, `[GUI] surfaces built panel=...`, `[GUI] gameplay input blocked (move + look)` | window shown; walking blocked on the first open |
| `[GUI] CanvasUVTri layout element=... pos=double` | triangle layout read from the game |
| `[GUI] atlas route=png-import` (or `route=runs-render-target`) | font uploaded |
| `[NGUI] frame n=1`, `[NGUI] coverage 76/76 distinct ImGui functions exercised` | the tab was drawn |
| `[NGUI] button speed=2.00 write=true`, later `speed=1.00` | the pad reached the button |
| `[GUI] surfaces rebuilt: atlas and input block are redone` (after the door / level change) | window repopulated after the map change |
| `[GUI] closed (gameplay input restored)` (each close) | close works |
| `[PERF]` lines while the window is open: pump calls per second as on R11, no new stutter | the window is drawn at most once per frame (F5) |
| 7 × `[l22] Lua mod started`, self-test as R11 | nothing else broke |

## FAIL (any one = fail, roll back to R11)

- `[GUI] refused:` (names what is missing), `[GUI] imgui assert (host)`, `[SDK] imgui assert (mod side)`,
  `fault in gui tab render`, `UE4SS_ENABLE_IMGUI refused:` (with our own probe this means host and mod files do
  not belong together: wrong upload).
- The window never appears, stays empty after the level change, is unreadable, or walking stays blocked after closing.
- A crash, a freeze, stutter. If #9 has not launched before: the log stops after `[NMOD] loading C++ mod ...`.

`[GUI] recovered ImGui usage error:` alone is not a failure (the host repaired a mod mistake), but report it.
