# Launch card: #12 Leon full look, round 6 (one launch)

**For:** the integrator (console steps) and the user (what to look at).
**Status: Mac-only.** Nothing in this round has been seen on the console yet.

## In one line

Round 5 fixed Leon's own outfit. The checker you still saw comes from **Simon's things that the mod hides
on PC** (his glasses, his IV bag, a few face parts): on the PS5 the mod's "invisible" material has no
console version, so the game draws its checker instead. This round points those parts at the game's own
"hide" material, which draws nothing, as on PC.

## What changes for you

| You saw | Now |
|---|---|
| Glasses: the lens ("window") and frame checkered | **gone** (hidden, as on PC) on both kinds of glasses the cutscenes use, as long as the game draws them as normal or skinned objects (partial, see below) |
| IV bag on Simon's clothes (front) checkered | **gone** |
| IV bag (back half) checkered | **gone where the game draws it as a normal object** (see the decision below) |
| The liquid inside the IV bag | **may still be checkered** (see "still checker") |
| Hair | still checker (unchanged, refused) |
| Leon's jacket, trousers, face, eyes, pistol, knife, watches, props | unchanged from round 5 |

## The decisions you give (BEFORE anything is prepared)

1. **"Treat Leon's body and skin as solid"** (`accept-unproven-opaque`): same as round 5.
2. **"Partial is OK"** (`allow-partial`): needed again this round, for **4** parts: the glasses **lens**, the
   glasses **frame**, the **back half of the IV bag**, and an enemy's cannula. Plainly: *they are hidden
   wherever the game draws them as normal or skinned objects; if the game draws them another way (instanced,
   Nanite, effects), the checker stays there, exactly as today. Nothing gets worse.*
   (Team B's 4 partial parts were the face part, the glasses frame, the glasses lens and the cannula. The face
   part is now fully proven; the other three stay partial for a stricter reason found in review: the game's
   own original materials for them can also be drawn those other ways. The IV bag back is new.)

## Before the launch (the integrator does these; you OK each console step)

1. Mac gate passes: `RUN PASS round6=43 invisible_r6=59 mf_cap12=51 mf_full=24 real_checks=27` (APPLY.md 1).
2. Plan and manifest made with both decisions; the dry run shows 24 `parent ... ->` lines (10 to
   `M_MRO_Cloth_NonVT`, 14 to `M_Hide`) and 30 `DERIVED TEXTURE` lines, `substituted_chunks_verified == 84`.
3. If the round-5 Leon install is on the console: undo it first (`paktx restore`, your OK).
4. Paks folder checks `PRISTINE` (read-only). Install (your OK) -> `"verdict": "INSTALLED"`.
5. Runtime stays R11 (`bfd95db6`), all Lua mods. Loadlist row-check green. Game offline.
**No console fetch is needed.**

## What you look at (one session; game closed before any log read)

Answer yes / no; photos help.
1. Leon's outfit looks as in round 5 (colours, folds, shine, metal)? (yes / no)
2. **Glasses:** in a cutscene where Simon would wear glasses: no lens, no frame, no checker? (yes / no)
3. **IV bag:** where Simon's IV bag would hang on his clothes: no checker bag? (yes / no)
4. Anything **still** checkered? Say exactly where (bag liquid, a cap tag, hair, something else).
5. Any part that is now **missing** that should be there (Leon's own clothes, face, weapons)? (yes / no)
6. Hair: still a checker? (expected yes)
7. Any crash, freeze or new stutter? (yes / no)

## PASS / FAIL

| Check | PASS | FAIL |
|---|---|---|
| Tool | `PRISTINE`, then `INSTALLED` | any `refused` (nothing changed) or `stopped` (undo first) |
| Look | 1 yes; 2 and 3 yes; 5 no | glasses / IV bag still checkered, or a Leon part missing |
| Game | 7 no | crash or freeze |
| Not a fail | hair checker; checkered liquid inside the IV bag; a checkered cap tag in a cutscene; a checkered tag on a bed | |

## Still checker (expected), and why

- **Hair:** the game's only hair shader needs a packed mask Leon does not have (refused since round 5).
- **IV-bag liquid:** the game draws its original liquid with its **effects system** (its own data says
  so: the original liquid material is built only for normal objects and effect meshes). The game's hide
  material has no effects version, so where the effect draws the liquid, the checker stays.
- **Cap tag (deluxe outfit, cutscenes only):** the game's original is also built for "instanced" and Nanite
  objects, which the hide material is not; and nothing proves where the cutscene puts it. Left as is.
- **A tag on a bed prop:** that bed is a Nanite object; the hide material has no Nanite version. Left as is.
- **Leon's own body:** 8 tiny placeholder triangles (1 mm, inside the hips) still use the mod's PC-only
  material. They cannot be seen, so they are left unchanged.

## Undo (rollback)

- `paktx restore` puts the 2 original index files back, byte for byte. Removing the 2 extra data files
  needs its own OK.
- If anything looks worse than round 5: undo, then (separate launch, own OK) the round-5 install
  (manifest `d7bf0d45...`) is still available unchanged.
- Runtime: nothing changes. Rollback build stays R11 `bfd95db6`.
