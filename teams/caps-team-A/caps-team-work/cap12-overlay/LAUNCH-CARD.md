# LAUNCH CARD: capability #12 (fix Leon's checkerboard), one launch

## In one line

Leon's clothes and skin use materials the PS5 cannot draw (grey/white checkerboard). This launch
points 10 of them at one of the game's own skin materials, by shipping a corrected copy of those 10
small files inside the install. The mod files themselves stay untouched, and everything can be undone.

## Two decisions only you can make (needed BEFORE anything is prepared)

1. **"Partial is OK"** (`--allow-partial`): the game material uses Leon's colour and bump textures,
   but not his third texture (shine/metal/shadow). Leon may look flatter or shinier than on PC.
2. **"Treat Leon's body and skin as solid"** (`--accept-unproven-opaque`): the files cannot prove if
   these materials have see-through cut-outs. If they do, those cut-outs get filled in. It cannot make
   Leon invisible.

Without both yes answers the tool refuses and nothing is changed. Leon's hair stays checkered either way.

## Before the launch (the integrator does these; you OK each console step)

1. Mac tests pass: `RUN PASS cap07_tests=45 cap12_tests=50`, real dry run `PASS`.
2. If the plain Leon install from #7 is on the console: undo it first (`paktx restore`, your OK).
3. The paks folder checks `PRISTINE` (read-only check).
4. Install: you see the printed plan first (10 lines "parent … -> M_SkinBasic_Hands"), then you OK it.
   The tool then prints `"verdict": "INSTALLED"`.
5. Runtime stays R11 (`bfd95db6`) with all Lua mods. Loadlist row-check green.

## What you do in the game

1. Start Townfall and load your save.
2. Look at Leon: face, eyes, mouth, upper and lower body, knife, pistol, props, watches.
   Are they drawn with Leon's colours instead of the grey/white checkerboard?
3. Play about 3 minutes, open the menu once, quit to the home screen. Tell us what you saw
   (photos help).

## PASS / FAIL

| Check | PASS | FAIL |
|---|---|---|
| Tool before launch | `PRISTINE`, then `INSTALLED` | any `refused` (nothing changed) or `stopped` (undo first) |
| You | the 10 parts show Leon's colours (may look flatter; hair still checkered) | the same checkerboard on those parts |
| Game | no crash, menus work | crash or freeze |
| Not a FAIL | checkered hair; less shine; filled-in cut-outs | |

## Undo (rollback)

- `paktx restore` puts the 2 original index files back, byte for byte (tested on the Mac, also
  after an interruption at every step). Removing the 2 extra data files needs its own OK.
- If the game crashes: undo, then one launch with the plain #7 Leon install (no fix), to see whether
  the fix or the mod itself causes it. That launch needs its own OK.
- Runtime: nothing changes. Rollback build stays R11 `bfd95db6`.
