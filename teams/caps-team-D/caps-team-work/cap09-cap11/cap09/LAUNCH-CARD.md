# Launch card — #9 C++ mods (one launch)

**What this tests:** a small C++ mod made for the PS5 loads next to the Lua mods, gets its start-up
signals, hooks one game function, runs every frame, and makes the player twice as fast on a button press.

**Good to know (plain words):**
- PC C++ mods (Windows DLLs) never run on the PS5. A C++ mod must be rebuilt from its source with our PS5 kit.
- If a C++ mod reports an error, only that mod is switched off. If it really crashes (bad memory), the game crashes.
- **Mod file rebuilt after the ship review fixes (Mac-only, 2026-10-13):** `main.prx` sha256 `a394b76d17ac08abf47b3dbb29435a0959452c2d320e63eda088793379e90f95`, 961193 bytes, magic 4f153d1d,
  92 imports, all already imported by R11, none new; pass-1 == pass-2; `verify_build --manifest` PASS
  (`cap09/build-out/BUILD-RECEIPT.md`). Never loaded on a console yet. The earlier file `40e8b85a…` is superseded:
  do not upload it (it lacks the N2 fix and the F7 ABI field).

**Build:** current runtime + cap09 (one build). Rollback = **R11 `bfd95db6`**.

## Before the launch (integrator)

1. `cap09/tests/run.sh` PASS (8 suites), `run_gates.sh` PASS, one build, `verify_build --manifest` PASS for the
   runtime PRX, imports still 207.
2. Mod file: `verify_build` + manifest gate PASS for the **exact sha of `NativeLifecycleProbe/dlls/main.prx`** =
   `a394b76d17ac08abf47b3dbb29435a0959452c2d320e63eda088793379e90f95` (961193 bytes; file `cap09/build-out/NativeLifecycleProbe/ship-pass-1/main.prx`;
   rerun: `verify_build.sh <that file> --manifest cap09/build-out/recipe/verify-manifest-NativeLifecycleProbe.json
   cap09/build-out/NativeLifecycleProbe/ship-pass-1`). Any other sha = no upload (CLAUDE.md §2). One independent review of
   `cap09/build-out/recipe/mbm_mod_module_entry.cpp` (new code in the mod file) before the first upload.
3. Upload (user OK) `main.prx` to `<mods directory>/NativeLifecycleProbe/dlls/main.prx` (the folder shown at the
   top of the log as `mods directories: [0] ...`) and an empty `enabled.txt` next to `dlls/`. Readback with
   decrypt off: full sha must be `a394b76d17ac08abf47b3dbb29435a0959452c2d320e63eda088793379e90f95`, magic 4f153d1d.
4. Loadlist: add **only** `file	config	ue4ss/Mods/NativeLifecycleProbe/enabled.txt	0`.
   **Never** add a row for `main.prx` (the runtime would refuse to start).
5. Pad file: add the row `L1+R2 = F9` (`cap09/PATCH/keymap/proposed/PS5Pad.cap09.ini`, 1985 bytes; check it
   against the console's current pad file first), re-pin the size row, row-check PASS.
6. All 7 Lua mods stay installed. User OK for each console action.

## What the user does

1. Start Townfall, load the save, walk around 30 seconds.
2. Press **L1 + R2 together** once: the character should run **fast**. (L1 and R2 also do their normal game action.)
3. Press **L1 + R2** again: normal speed.
4. Use the item spawner or the minimap once.
5. Quit to the menu, close the game.

## PASS lines (decode the log first; all must appear)

| Line | Means |
|---|---|
| `[NMOD] C++ mod scan: ... 1 with dlls/, 1 enabled` | the runtime found the C++ mod |
| `[NMOD] loading C++ mod NativeLifecycleProbe from ...` then `[NMOD] load step returned for NativeLifecycleProbe installable=yes` | the PRX loaded |
| `[NMOD] NativeLifecycleProbe: attached abi=1 name='NativeLifecycleProbe'` and `[NMOD] NativeLifecycleProbe: start returned started=yes` | the mod started |
| `[NPROBE] constructed`, `[NPROBE] on_program_start`, `[NPROBE] on_unreal_init object=Class /Script/CoreUObject.Object` | start-up signals + object lookup |
| 17 × `[NPROBE] admission family=...` incl. `process-event route=REFUSED`, `ufunction route=DATA_ONLY_PARTIAL live=1` | hook rules work as designed |
| `[NPROBE] RegisterHook ClientRestart id=` (not `id=-1`), later `[NPROBE] hook ClientRestart post n=1` | function hook fires |
| `[NPROBE] game-thread tick n=1` | runs every frame |
| `[NPROBE] key F9 speed=2.00 write=true readback=2.00`, later `speed=1.00` | button + speed change (and the user saw it) |
| `[NPROBE] on_update n=1`, `[NPROBE] on_cpp_mods_loaded` | remaining signals |
| 7 × `[l22] Lua mod started`, self-test as R11 (161/168) | nothing else broke |

**Not failures:** `object-delete route=REFUSED` and `load-map live=0` (not offered to C++ mods in this version).
The per-frame tick runs once per game-thread pump call, which may be a little more than once per frame.
Hot reload is switched off in every config; if it is ever triggered with a C++ mod loaded, the log shows
`[NMOD] refused: hot reload of all mods while 1 native C++ mod(s) are loaded` and every mod keeps running (by design, F3).
If a C++ mod folder cannot be read, that mod alone is refused with `dlls folder could not be checked (OS error N)` (F1).

## FAIL (any one = fail, roll back to R11)

- **The log stops right after `[NMOD] loading C++ mod ...`** or the game hangs at start: loading a second PRX
  during start-up does not work. Roll back; nothing else to try in this launch.
- `installable=no`, `could not be loaded:`, `start refused:`, `[NMOD] refused:`, `fault in`, or no `[NPROBE]` lines.
- A crash, a freeze, missing Lua mods, new stutter, `drain timeout`.
