# Launch card: "Leon off" check

**What it does:** one launch of the same game, the same runtime (R11 `bfd95db6`) and the same 7 Lua mods, with
**Leon fully removed** and the game's pak files back to their original state. It gives a clean "before" picture:
normal character, normal dark outdoors. No new build, no new code.

**When:** this launch is not in the current order. You decide if and when it runs.

## Before the launch (integrator; every console step needs your OK; one agent on the console)
1. Read-only LIST of the game's **whole** `content/paks/` folder, **subfolders included**. Save it to evidence.
   Write down every `zzz_Leon` file and every other file that is not part of the game. If there is any other mod pak, stop and tell you.
2. **Console action (needs OK):** follow `../PRISTINE-PAKS.md` exactly. **If that file does not exist yet, stop here.**
   - The 3 Leon files (`zzz_Leon_Townfall_*_P` `.pak` / `.ucas` / `.utoc`) go to a folder **outside** `content/paks/`,
     for example a `leon-off/` folder next to `content/`.
   - They must not go to a subfolder of `paks/` (such as `paks/~mods-off/`): the game also loads paks from subfolders.
   - This is a move, not a delete.
3. LIST the whole `content/paks/` tree again. It must contain **no `zzz_Leon` file anywhere**, subfolders included.
   The game's own pak files must match the original checksums given in `../PRISTINE-PAKS.md`. If either check fails, do not launch.
4. Usual checks: console offline, PRX decrypt-off readback (full sha256 + magic) passes, loadlist row-check passes.

## What you do in the game
1. Load the same save as last time. Look at the player character in third person for about 30 seconds.
2. **Check:** does the player look like the normal game character, with no Leon and no grey/white checker pattern anywhere?
3. Go to the same door and open it to the outside. Is it dark and foggy (normal), or bright as day?
4. Take one screenshot of the character and one at the door. File them in `evidence/SCREENSHOTS/` as
   `leon-off-<what>-<UTC>.jpg`, with their sha256.
5. Close the game normally. When asked "game closed?", answer before anyone reads the logs.

## Logs (decode with `harvest/decode_session_log.py`)
- Normal runtime: `[l22] Lua mod started` ×7 and `[WORLD-EV] route live`. No crash block in klog.
- No log line measures looks or brightness. The verdict comes from your eyes and the screenshots.

## Reading the result
- **INVALID:** Leon or the checker pattern is still visible. The game still loaded Leon files, so the test tells us nothing.
  Do not draw any conclusion. Roll back and find the leftover file.
- **PASS, dark outdoors:** this is the clean baseline. It confirms the Mac finding: the brightness came from the changed
  `PS5Engine.ini` inside the modified `pakchunk0-ps5.pak` file, not from the runtime.
- **PASS, still bright:** Leon is gone, so Leon is not the cause. Next, check the game's own brightness and HDR settings.
- **FAIL:** a crash, or the mods do not start. That is a runtime problem, not this test. Roll back.

## Rollback (console action, needs OK)
Put the files back as `../PRISTINE-PAKS.md` describes, then LIST again to confirm. The runtime is not touched:
R11 `bfd95db6` stays the last clean build.
