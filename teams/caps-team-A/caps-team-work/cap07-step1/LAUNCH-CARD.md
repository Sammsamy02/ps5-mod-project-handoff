# LAUNCH CARD: capability #7 (pak mods), one launch, Leon outfit

## In one line

A new tool puts the unchanged Leon mod into the game the way that already worked once,
now automatically. It checks everything first and can undo everything afterwards.

## About what you saw last time

- **Bright outside, monster easy to see:** an old test from 2026-10-02 is still on the
  console. It swapped one game file for a copy with PC "performance" settings: no fog, no
  eye adaptation, higher gamma, fewer shadows. That copy is still in place. The original
  is saved next to it. Before this launch we put the original back, so the darkness
  should return.
- **Gray/white checkerboard on Leon:** Leon's materials only have PC shader code, so the
  PS5 draws its "no shader" placeholder. Loading the mod (this test) cannot fix that.
  It is capability #8/#12. **Expect the checkerboard again. It is not a FAIL here.**

## Before the launch (the integrator does these; you OK each console step)

1. The Mac tests pass: `RUN PASS tests_run=45 replay_items_checked=155059`.
2. **Clean the paks folder:** follow `../PRISTINE-PAKS.md`. You get 5 separate requests
   for an OK: look; move the old Leon files out; put the 2 original game files back;
   move the old leftover out; check. It ends with `"verdict": "PRISTINE"`.
3. Install Leon: you see the printed plan first, then you OK it. The tool then prints
   `"verdict": "INSTALLED"`.
4. Runtime stays R11 (`bfd95db6`) with all Lua mods. The loadlist row-check is green. The
   optional probe folder is deployed.

## What you do in the game

1. Start Townfall and load your save.
2. Is your character Leon (body and outfit, even if checkered)?
3. Go to the same outside door. Is it dark and foggy again, like the normal game?
4. Play for about 3 minutes, open the menu once, then quit to the home screen. Tell us
   what you saw.

## PASS / FAIL

| Check | PASS | FAIL |
|---|---|---|
| Tool before launch | `"verdict": "PRISTINE"`, then `"verdict": "INSTALLED"` | any `refused` (nothing was changed) or any `stopped` (something may have changed: undo first) |
| You | Leon's model and outfit | the old character |
| Game | dark and foggy outside, no crash, menus work | crash, or bright again |
| Log (with the probe) | `[pk] probe start …`, then at least one `[pk] FOUND /Game/…` | no `[pk] FOUND` line by the last `[pk] Tn` printed while Leon was on screen, or `[pk] ERR` |
| Not a FAIL | checkered Leon | |

The game prints no log line about loading or shaders, so the probe and your eyes are the
evidence. Two of the 8 probe objects (knife, pistol) may only load when a weapon is out.
One FOUND is enough.

## Undo (rollback)

- **Undo Leon:** `paktx restore` puts the 2 original index files back, byte for byte. Our
  leftover files (the 2 extra `_s1.ucas` data files and 2 retired indexes) stay until
  you OK their removal, and the next install waits for that. The cleaned paks folder from step 2 stays cleaned (darkness stays back).
- **If the game crashes with Leon:** undo, then one more launch later with only the main
  part of Leon (`--only-into pakchunk0-ps5`, the shape that worked before). That launch
  needs its own OK.
- **Runtime:** nothing changes. The rollback build stays R11 `bfd95db6`.
