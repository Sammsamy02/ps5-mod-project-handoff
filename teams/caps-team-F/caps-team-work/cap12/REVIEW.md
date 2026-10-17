# Review — capability #12 (Team F): shader inputs / reuse the game's own PS5 shaders

Reviewer: independent reviewer agent, 2026-10-12. Everything below is **Mac-only**
(read from local files and host runs). No console contact.

## Verdict: PASS-WITH-FIXES

The staged work is new Mac-side tools plus a read-only Lua probe. There is no C++,
no PRX change, no detour, no network code, and no mod file is written
(`mic_reparent.py` refuses `--out == --in` and opens its output with `'xb'`). The
tools hard-code no game, mod or asset names (grep: only two docstring mentions of
"Leon"). No Epic or Sony source is copied. The research findings I checked hold up.
The launch card already says "not ready to launch".

Before the #12 launch, items F1–F6 must be fixed. F1 and F2 can each produce a
**false PASS**.

## Host tests, re-run by me

`bash tests/run_tests.sh`: **Ran 31 tests ... OK**, `luac syntax OK`,
`zzmatdiag host test: checked 6 slot lines, size 2185 bytes: PASS`,
`python=0 luac=0 lua=0`, `ALL PASS`. `PATCH/SHA256SUMS`: 15/15 OK. The staged
`PATCH/material-fallback/*.py` files are byte-identical to `tools/*.py`, and the
staged test file is identical to `tests/test_cap12.py`.

Side effect, disclosed: `run_tests.sh` rewrites `tests/test-output.txt`. My run
changed only the timestamp line (03:52:42Z → 03:53:36Z) and the timing (4.757s →
4.735s). I put the team's two values back. A copy of my run's output is in the
reviewer scratchpad (`reviewer-run-test-output.txt`).

Most tests use real data:
- CityHash on >30,000 real package ids
- 4/4 type-8 ids from the game's own names
- BLAKE3 against real TOC IoHashes and the 2.27 MB container header
- the Leon census, a byte-identical name-batch writer, and the rename round trip on 127/127 packages
- the audit on the real mod

The **shader-library decoder and the type-9 group-id law are tested only against
the team's own synthetic writer** (`shader_library.build`). That is circular, and
the team says so. The real check comes only when `verify_fetch.py` runs on the
fetched bytes.

## Claims I re-checked (Mac-only)

- **Overlay gap is real.** `l25-prep/pak-overlay/overlay_builder.py:1955-1981`
  rewrites only the `StoreEntry` import list. Nothing touches the package header's
  import hash or name.
- **Bright outdoors.** `evidence/console-pulls-20261013-paks/`: the current pak has
  5 `r.VolumetricFog=0` strings and the `.bak` has 4. The `.bak` sha256 is
  `ee145cbc808f…d867`. That equals the *original* sha256 in
  `.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/build-receipt.json`,
  and the current pak `8bcf5eb1…` equals that trial's `derived.pak`. So the
  config-trial pak is what is on the console. This gives two sources for the
  restore target.
- **Audit counts reproduced from `evidence/leon-material-audit.json`:**
  - 13 Materials, all mod-only paths, all with inline `PCD3D_SM5`/`PCD3D_SM6`
  - 54 MICs
  - 43 MICs override base packages (42 in pakchunk0, 1 in pakchunk10)
- **Fetch plans.** All 4 type-8 ranges are contiguous. Round 1 is 68 rows /
  2,879,815 B, of which 37 are whole chunks.

## Findings

### F1 (FIX-SOON) — harvest gives PASS when no Leon slot was probed

Where: `harvest_matdiag.py` and `zzmatdiag/Scripts/main.lua`.

- **Harvest.** `harvest_matdiag.py` counts a slot as OK whenever its base is not one
  of the 13 audited materials. It never checks that any probed slot belongs to the
  mod. I reproduced this: a one-line log
  `mat=MaterialInstanceConstant /Game/Townfall/UI/MI_MenuChar... base=Material /Game/Townfall/Characters/Materials/M_X...`
  gives `harvest_matdiag: PASS`, rc=0.
- **Probe stopping early.** The probe stops after `good>=2` (two runs with any slot
  on the controller's pawn) or after 10 ticks × 30 s. A main-menu or intro pawn
  with a skeletal mesh would therefore fill the log with base-game slots and stop
  before Leon spawns, and that gives a false PASS.
- **Probe window too short.** If loading the save takes more than about 5 minutes
  after the mods load, the probe ends with zero `[S]` lines, and that gives a false
  FAIL.
- **Fix:**
  - Harvest: require at least 1 slot whose `mat` package is one of the audit's
    material packages; otherwise FAIL or INCONCLUSIVE.
  - Probe: count only runs that saw such a slot, or keep sampling all session and
    log only when the slot set changes.
  - Launch card: state the time window for the probe.

### F2 (FIX-SOON) — the parent chooser ignores material usage and class

Where: `reparent_plan.py` (`choose`, `plan`, `main`).

- **Usage flags.** Candidates are *any* fetched base package that has store shader
  maps, ranked only by name overlap.
  - In a cooked build, a skeletal mesh whose material was not cooked with the
    skeletal-mesh usage flag gets the engine default material at render time.
    Morph-target and cloth usage work the same way.
  - Source: my own UE knowledge (Mac-only). The local Epic tree has no Materials
    sources, so I cannot cite path:line.
  - In that case `GetBaseMaterial()` still returns the base master. The probe logs
    a base-game base, harvest says PASS, and the user still sees the checker.
- **Class.** The chooser also never checks that the candidate's export is a
  Material or MaterialInterface. A Niagara or other shader-map-owning package could
  win.
  - `new_export` is assumed to be the package basename.
  - With the wrong class, the `Parent` reference would be nulled at load: the
    checker again, no crash.
- **Fix:** take candidates only where the base export class is
  `/Script/Engine/Material` (or a MaterialInterface subclass), read from the
  fetched header + ScriptObjects. Prefer candidates that base skeletal meshes
  already use (this is evidence the usage flag is set). State the usage-flag gap
  in the card.

### F3 (FIX-SOON) — second source is optional where the bytes are written

Where: `mic_reparent.py` (`--base-header`, `reparent()`).

- Without `--base-header`, the tool writes the derived package with the new public
  export hash from the CityHash law alone. The receipt says
  `'cityhash law only (single source)'`, but the bytes are still written.
- CLAUDE.md requires two sources for shipped values.
- **Fix:** make `--base-header` required, or exit 1 when only one source is
  present.

### F4 (FIX-SOON) — no staged path fixes the 10 mod Materials used directly

Where: `reparent_plan.py`, LAUNCH-CARD.md PASS rule, NOTES §6.

- `reparent_plan.plan()` only handles MICs with `INHERITS_MISSING`. Nothing plans
  the mesh-package repoints for `Simon_Slot0..7` and the two `M_Invisible`
  materials, or for `M_Leon_*` if a mesh imports them directly.
- The card's PASS needs zero CHECKER, and its FAIL list names exactly these
  materials. As staged, the launch therefore cannot PASS.
- `M_Invisible` cannot be chosen by parameter names: it has to stay invisible.
  Pointing it at a visible base material would show the Simon parts the mod hides,
  overlapping Leon.
- **Fix:** add a rule for directly used Materials (which base material, how its
  invisibility is proven from the files, or "known FAIL slots" listed in the card),
  or narrow the PASS rule to the MIC slots and state that.

### F5 (FIX-SOON) — pakchunk0 restore step lacks its hash and readback

Where: LAUNCH-CARD.md, precondition 5.

- The card says "restore `pakchunk0-ps5.pak` from its `.bak`" but gives no expected
  hash.
- The `.bak`'s name tag `9a914e38` is **not** its sha256 (`ee145cbc…`) or its sha1
  (`fcb59683…`), so checking it by file name proves nothing.
- **Fix:**
  - Expected full sha256 `ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867`.
    Sources: the pulled `.bak` in `console-pulls-20261013-paks/SHA256SUMS`, and
    `trial-1/build-receipt.json`.
  - Full-sha readback after the copy.
  - The copy is a separate console action that needs its own OK.
  - Keep the restored pak as it is on rollback.

### F6 (FIX-SOON) — only the chunk0 container header feeds the store

Where: `reparent_plan.py --base-header`, APPLY.md step 2, `mod_material_audit.py`.

- APPLY step 2 and the tests pass only the chunk0 container header. Shader-map
  candidates whose packages are in pakchunk1/10/11/12 are then dropped without a
  message.
- The audit also reports `base_had_target_shader_maps: False`, instead of unknown,
  for `/Game/Townfall/Characters/Simon/Materials/MI_Simon_BodywarmerBeanieTag_Deluxe`
  (a pakchunk10 override). So "24 of 43" may undercount.
- **Fix:** pass every fetched container header, and report `None`/unknown when the
  package's container header is not loaded.

### Notes (lower risk)

- **N1. `verify_fetch.py`:**
  - PASS does not require `missing == 0` in the container-header join, although
    NOTES expects all 1,500 maps.
  - A library with zero groups counts as "group ids recomputed" without checking
    anything.
  - A non-contiguous fetch row could never pass its IoHash check. All 4 current
    rows are contiguous.
- **N2. Unverified header bytes.** 31 of the 68 round-1 rows are header-only. They
  are verified only by the hash of the package name inside them, so those
  parameter names come from bytes no IoHash covers.
- **N3. Renames.** `rename_names` renames *every* use of that name in the package,
  not only the parameter. It does not check whether the name is used for something
  else in the package.
- **N4. Probe:**
  - It is a new diagnostic folder (`zzmatdiag`), while CLAUDE.md names
    `ueselftest`/`zzdiag`. Either fold it into `zzdiag` or get the user's OK for a
    new owned folder.
  - It runs `FindAllOf` scans on the game thread up to 16 times, which bears on the
    "no stutter" acceptance.
  - The owner filter misses meshes on attached child actors.
  - Its calls are pcall-wrapped. That matters because B7 (Lua errors unwinding as
    C++) is not in R11.
- **N5. A claim not shown in the test output.** The summary's "shared FU UE4SS stub
  harness reports LOADS-CLEAN, 0 missing globals" is not produced by
  `run_tests.sh` and does not appear in `tests/test-output.txt`. I could not
  reproduce it.
- **N6. Rollback.** "Remove the derived overlay files" does not name the files yet.
  The #7/#8 overlay action has to define them before the card is final.

## Hard-rule check

| Rule | Status |
|---|---|
| No console contact | OK. No network code; all console needs are listed for the integrator |
| Mods untouched | OK (input refused as output; `'xb'`) |
| Generic | OK. Tools take all names from the files |
| No detours / data-only | OK. No C++; the derived package is data only |
| Two sources | **F3** (optional second source at the point where bytes are written) |
| Fail closed | Mostly OK; F6 and N1 are gaps |
| Zero-count gates fail | OK for all CLIs and run_tests.sh |
| No SDK / Epic copy | OK. Sources are retoc (MIT) and the game's files |
| Evidence in the durable tree | Pending integrator copy (team folder only), as stated |
