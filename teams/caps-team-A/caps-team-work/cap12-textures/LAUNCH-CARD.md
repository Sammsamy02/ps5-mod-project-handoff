# Launch card: #12 Leon full look, derived textures in the PS5 layout (one launch)

**For:** the integrator (console steps) and the user (what to look at).
**Status: Mac-only.** Nothing has been tested on the console yet.

## In one line

Leon's 10 outfit parts use the game's own cloth shader, and every Leon texture they use ships as a corrected
copy: re-ordered into the PS5's own memory layout (so it is not scrambled), and for the shine/metal texture
also with two colour channels swapped. No quality loss; Leon's own files stay untouched; everything can be undone.

## One decision only you can make (needed BEFORE anything is prepared)

**"Treat Leon's body and skin as solid"** (`accept-unproven-opaque`): the files cannot prove whether these
materials have see-through cut-outs. If they do, the cut-outs get filled in. It cannot make Leon invisible.

## Before the launch (the integrator does these; you OK each console step)

1. Mac gate passes (`RUN PASS mf_cap12=51 mf_full=24 cap07=45 cap12=50 textures=47`, `tex_real: PASS`,
   `cap07_replay: PASS`).
2. Plan and manifest made with your decision; the dry run shows 10 "parent ..." lines and 30 "DERIVED TEXTURE"
   lines, and `substituted_chunks_verified == 70`.
3. If an older Leon install is on the console: undo it first (`paktx restore`, your OK).
4. The paks folder checks `PRISTINE` (read-only). Install (your OK) -> `"verdict": "INSTALLED"`.
5. Runtime stays R11 (`bfd95db6`) with all Lua mods. Loadlist row-check green. Game offline.
No console fetch is needed.

## What you look at (one session; game closed before any log read)

Answer yes / no; photos help.
1. Jacket and trousers: Leon's colours and patterns, **sharp**, no checker, no noise or "TV static" squares? (yes / no)
2. Face up close AND at a distance: Leon's face, no scrambled squares or streaks when far away? (yes / no)
3. Folds and creases look darker/deeper (shadows in the cloth)? (yes / no)
4. Pistol, knife, watch look like **metal** (shiny), not plastic? (yes / no)
5. Shine varies across the outfit (leather/buckles shinier than cloth)? (yes / no)
6. Eyes look normal (not white, checkered or noisy)? (yes / no)
7. **Eyelashes (lower-confidence group, check closely):** thin dark strips, no noise or odd colours? (yes / no)
8. Hair: still a checker? (expected yes)
9. Anything strange: too dark, too shiny, oddly coloured spots, metal where there should be none? Say where.
10. Any crash, freeze or new stutter? (yes / no)

## PASS / FAIL

| Check | PASS | FAIL |
|---|---|---|
| Tool | `PRISTINE`, then `INSTALLED` | any `refused` (nothing changed) or `stopped` (undo first) |
| Look | items 1, 2, 6 yes; at least one of 3-5 yes | checker, plain white/black, or noise squares on the parts |
| Game | item 10 no | crash or freeze |
| Not a fail | hair checker (8); a slightly "dry" face | |

How to read a failure:
- **Fine "TV static" in square patches, or garbage only at a distance:** the PS5 layout is wrong for that
  texture group. Note which parts. Eyelashes (128x128, 4 KB mode) are the least certain group.
- **Wrong shine/metal** (cloth like chrome, metal matte, creases bright): the channel swap did not land as
  expected. Note which parts.
- In both cases: undo, report, and the round-4 look (no derived textures) stays available.

## Undo (rollback)

- `paktx restore` puts the 2 original index files back, byte for byte (tested on the Mac, also after an
  interruption at every step). Removing the 2 extra data files needs its own OK.
- If the game crashes: undo, then (separate launch, own OK) the round-4 install without derived textures.
- Runtime: nothing changes. Rollback build stays R11 `bfd95db6`.
