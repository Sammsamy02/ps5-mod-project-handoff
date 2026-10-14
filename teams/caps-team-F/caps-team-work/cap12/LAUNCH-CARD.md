# Launch card: capability #12, "Leon without the checker"

**Status: not ready to launch yet.** The fetches and the overlay change listed
below must be done first. This card describes the one launch after that.
Everything here is Mac-only until the launch happens.

## What this launch does

- Leon's materials have no PS5 shader code, so the game draws them as a
  grey/white checker.
- In this launch, Leon's material instances borrow one of the game's own PS5
  materials. Leon's textures are kept.
- The mod's files are not changed. Only new overlay files are added.
- The PRX stays **R11 bfd95db6**. No new build.
- A small read-only Lua probe (`zzmatdiag`) logs which material each body
  part uses.

## What to expect (honest version)

- **Should be fixed (round 2):** 10 of Leon's 11 material instances (body,
  face, mouth, eyes, lashes, gear, knife, pistol, watches, props). They borrow
  the game's own opaque, lit cloth material `M_MRO_Cloth_NonVT`. Both sources
  (the game's asset registry and its own compiled PS5 shaders) say that it is
  opaque, surface and lit.
- **User decision needed first:** the files cannot prove whether Leon's body
  and skin materials are opaque or masked. They are surely not see-through.
  The tool refuses unless the user says "treat them as opaque"
  (`--accept-unproven-opaque`). If they were really masked, small cut-outs
  (for example lash edges) would show as solid.
- **Look loss (needs the user's OK):** Leon's colour and normal textures are
  used. Leon's ORM texture (roughness / metal / shadowing) is **not** used,
  because the game material packs those channels in a different order (MRO).
  Surfaces may look flatter or shinier than in the mod. The lighting style is
  the game material's, which has some cloth sheen.
- **Will stay checkered if they appear ("known-checker"):** 57 of the mod's
  materials:
  - Leon's hair instance: it is masked, and no good masked game parent is
    known yet.
  - 13 mod master materials, including `Simon_Slot0..7` and the two
    `M_Invisible`.
  - The 43 hidden-Simon-part instances, which have no settings of their own.

  Nothing in the files lets us rebuild these safely. The tool lists every one
  of them, with the reason.
- Textures may still look scrambled. That is a separate problem (#8, Team A).

## Before the launch (all needed)

1. #7/#8 done: Leon mounts and is visible on Townfall (a checker counts as
   visible).
2. Integrator fetches done and checked on the Mac (read-only, game closed):
   - the 4 shader manifests and the 4 small container headers: **done**
     2026-10-13, `verify_fetch.py` PASS
   - round 1 and round 2 base headers: **done**, `package_fetch_plan.py check`
     PASS
   - optional round 3 (`evidence/fetch-round3-masters.json`, 356 rows,
     21.0 MB; or only `fetch-round3-masters.p1.json`, 5 rows, 294 KB). It may
     find a closer parent and a hair parent. Afterwards, run `check` and pass
     the result to `reparent_plan.py`.
   - **The user OKs both decisions above**, then run `reparent_plan.py
     --allow-partial --accept-unproven-opaque` (APPLY.md step 2). Leon's 10
     instances must say `reparent-partial` with an opaque, lit parent whose
     two sources agree.
3. The #7/#8 owner has added and reviewed the "ship derived package bytes"
   overlay action. The rebuilt overlay re-parses with zero errors.
4. **Fix the bright-outdoors problem first, as its own console action with its
   own OK.** Copy `pakchunk0-ps5.pak` back from its `.bak`. Afterwards, read
   back the whole file and check:
   - sha256 `ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867`
   - size 297,572,462 bytes
   The hash comes from two sources: the pulled `.bak`, and the original in
   the config trial's build receipt. Do not trust the `9a914e38` in the file
   name; it is not the file's hash. Keep this restored pak even if #12 is
   rolled back.
5. The user OKs `zzmatdiag` as a new diagnostic folder. CLAUDE.md lists only
   `ueselftest`/`zzdiag`; the alternative is to fold it into `zzdiag`.
6. Usual console rules: an OK for each upload and for the launch,
   decrypt-off readback, loadlist row-check (`zzmatdiag` main.lua is
   2,944 bytes), one launch.

## What the user does in game

1. Load the save and play as Leon for at least 3 minutes. The probe checks
   every 30 seconds and keeps going for 60 minutes after the mods load, so a
   slow save load is fine. It may cause a tiny hitch every 2 minutes; it is
   removed after this test.
2. Look at Leon's body, face and hair: in third person, in a mirror or a
   cutscene, and in first person (hands and gear).
3. Answer three questions:
   - Is any part grey/white checkered? Which part? (The hair is expected to be.)
   - Do the textures look like Leon, or scrambled?
   - Do any Simon parts show through?
4. Quit to the home screen. Close the game before any log is read.

## PASS / FAIL

The verdict uses this session's decoded log and what the user sees.

Run `harvest_matdiag.py --log <decoded> --audit leon-material-audit-5hdr.json --plan reparent-plan-r2-partial-accept-opaque.json`
(or the plan re-run after round 3):

- **PASS:**
  - at least one of Leon's own slots is fixed
  - zero unplanned checker slots, zero unreadable slots
  - the user sees no checker on Leon's body or face (the hair is a planned
    known checker in this plan)

  The only checker slots allowed are the listed "known-checker" ones.
- **INCONCLUSIVE:** the log shows only game slots (a menu or intro character)
  and no Leon slot. That is not a pass; repeat with a longer play time.
- **FAIL:**
  - any Leon slot still checkered that the plan said it would fix, or
  - zero slot lines, or
  - the user sees a checker on Leon's body or face.
- Materials fixed but textures scrambled: the materials work, and the textures
  are the next problem (#8). Record that as a separate finding.

## Known gaps (not checked by any tool)

- The chooser checks that a game skeletal mesh already uses the chosen game
  material. It does not check morph-target or cloth use. If Leon needs those,
  some parts may still show the default material.
- The exact lighting model (DefaultLit vs Cloth vs Subsurface) has only one
  source (the registry). It is shown as a note and never used to accept a
  parent.
- "Opaque" for Leon's body and skin rests on the user decision above, not on
  two sources.
- 31 of the 68 round-1 headers are only partial reads (checked by the package
  name inside them). The chosen parent's header is also used as the second
  source for the new link, so a bad read fails safely.

## Rollback

- The R11 PRX bfd95db6 stays as it is.
- Remove the derived overlay files; the #7/#8 owner names the exact files
  before this card is final.
- Remove `zzmatdiag` from the loadlist.
- Keep the restored `pakchunk0-ps5.pak` (step 4).
- No kernel work, so the 3-pass rule does not apply.
