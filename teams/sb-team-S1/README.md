# Team S1 — one runtime for every game (plain words)

**All of this is Mac-only. Nothing was built or sent to the console.**

## The problem
Today the runtime is built for one game (Townfall). About a dozen tables of
game-specific numbers (where things live in memory, the game's fingerprint,
etc.) are baked into the program. A different game needs different numbers, so
today every new game would need a whole new build.

## What we did
We made the runtime able to **read those numbers from a small text file** that
ships next to the mods, instead of baking them in. So one build can serve every
Unreal game: you just drop in the right file for the game.

- The file is named after the game's own fingerprint and sits with the mods.
- When the game starts, the runtime reads the file into memory (the one moment
  it can), checks it is the file for THIS exact game, and then uses those
  numbers. If there is no file for this game, the runtime **safely does nothing**
  (the game runs with no mods) and writes down what it saw, so we can make the
  file. Nothing can run on wrong numbers — every number is re-checked against
  the real game before it is used.

## Did we prove it works for Townfall?
Yes, on the Mac. We wrote a small program that reads today's baked-in Townfall
numbers and writes them into one of these new files, then a test that loads that
file back through the new reader and checks **every single value matches** —
all 88 of them do. So Townfall behaves exactly as it does today.

## Three smaller fixes in the same drop
1. **Don't let one optional feature stop everything.** Before, if any one memory
   fingerprint was off, the whole runtime refused to start. Now only the two
   essential ones do that; the optional ones (pak mods, listeners) just switch
   that one feature off, so the rest still works. We also removed one leftover
   fingerprint that nothing used anymore.
2. **Stop assuming the game is Townfall's engine.** Some checks had Townfall's
   exact 5.6 numbers written in and would reject a correct different-engine game.
   Those now compare against the game's own file instead.
3. **The engine version comes from the file**, not hand-typed in a settings file.

## What's left for the integrator
The reader, the file format, the Townfall proof, and the three fixes are done
and checked on the Mac. The last step — swapping each of the dozen baked-in
tables over to read from the file — is written up as an exact recipe
(`APPLY.md`) for whoever does the real build, because only they can compile and
launch the console runtime. The safety checks all still run exactly as before;
only where the numbers come from changes.

## Files
- `PROFILE-FORMAT.md` — exactly what the file contains (for the team that will
  make these files).
- `new/ModProfile.hpp`, `new/ModProfileLoader.hpp` — the reader.
- `gen/`, `host-tests/` — the Townfall proof and the tests (all pass on the Mac).
- `s1.patch`, `base/`, `new/` — the staged changes.
- `APPLY.md` — step-by-step for the integrator. `pipeline-gates/NOTES.md` — how
  the build checks adapt.
