# Fixes for REVIEW.md: capability #12 (Team F)

2026-10-12. Fixer pass. **Everything here is Mac-only:** host runs only. There
was no console contact, no PRX build, and no change to mod files, the candidate
tree or other teams' folders.

## Final host run

`bash tests/run_tests.sh` gives:

- `Ran 39 tests ... OK` (it was 31)
- `luac syntax OK`
- `zzmatdiag host test: checked 21 slot lines, 15 sets, 64 scans, size 2944 bytes: PASS`
- `python=0 luac=0 lua=0`
- `ALL PASS (39 python tests, lua probe test 21 slot lines)`

The output is in `tests/test-output.txt`. The staged copies in
`PATCH/material-fallback/` are byte-identical to `tools/*.py` and
`tests/test_cap12.py`. `PATCH/SHA256SUMS` was regenerated: 15/15 OK.

**Runner proof (planted failures).** I ran `run_tests.sh` on throwaway scratch
copies with a planted fault, then deleted the copies. Every case gives exit 1:

| Planted fault | Runner output |
|---|---|
| A failing Python test | `FAIL: a test failed` |
| An empty Python suite | `python tests run: 0 ... FAIL: zero python tests ran` |
| A failing Lua assert | `FAIL: a test failed` |
| A Lua test that checks 0 slots | `FAIL: lua test checked nothing` |

The runner now prints both counts, and its error messages name the real cause.

## Findings

| # | Finding | Result | What changed | Test evidence |
|---|---|---|---|---|
| F1 | Harvest PASS when no mod slot was probed; probe stopped early or too soon | **fixed** | See [F1 details](#f1-details). | See [F1 details](#f1-details). |
| F2 | Parent chooser ignored class and skeletal-mesh usage | **fixed** (the morph/cloth gap is disclosed) | See [F2 details](#f2-details). | `test_candidates_need_class_store_usage` (synthetic headers: Niagara and mesh excluded by class, static-only material excluded by usage, store-unknown and mod-overridden excluded, export name taken from the header); `test_plan_and_check_with_local_mod_as_stand_in` (real header-only prefix: class `SkeletalMesh`; real MIC: `MaterialInstanceConstant`) |
| F3 | Second source optional where bytes are written | **fixed** | See [F3 details](#f3-details). | `test_cli_requires_base_header`: no flag gives rc 1 and no file; a header naming another package gives rc 1 and no file; a matching header gives rc 0. `test_refusals` and `test_base_header_second_source` also pass. |
| F4 | No path for the 10 directly used mod Materials; PASS was impossible | **fixed by narrowing, as the review allowed** | See [F4 details](#f4-details). | `test_plan_rules_on_real_leon_audit`, real Leon audit, all 67 materials: 11 `reparent` and 56 `known-checker`. All 43 M_Invisible children are `known-checker` and parameterless. All 13 Materials are `known-checker`. With the restore-base option there are exactly 24 `restore-base`, all with base shader maps `True`. The harvest cases with `--plan` are in `test_join`. |
| F5 | pakchunk0 restore had no hash or readback | **fixed (card)** | See [F5 details](#f5-details). | Mac-only: `shasum -a 256` of the pulled `.bak` gives `ee145cbc…d867`. That matches `console-pulls-20261013-paks/SHA256SUMS` and `trial-1/build-receipt.json` `source.sha256`, `bytes` 297572462. |
| F6 | Only the chunk0 container header fed the store; the audit said False instead of unknown | **fixed** | See [F6 details](#f6-details). | `test_all_type8_ids_from_game_names`: 5 header rows; the local chunk0 file matches its row's IoHash and length; chunk10 is 416 B. `test_leon_all_materials_lack_ps5_shaders`: exactly 1 unknown, in pakchunk10; still 24 True; 18 False. `test_planned_container_header_not_given_fails`, `test_unplanned_container_header_refused`. |
| N1 | verify_fetch: missing ≠ 0 passed; zero-group library passed; non-contiguous rows | **fixed** | See [N1 details](#n1-details). | `test_store_map_missing_from_libraries_fails`, `test_zero_group_library_fails`, `test_pass` (`group_ids_checked` = 3, missing = 0) |
| N2 | 31 header-only rows are not IoHash-covered | **partly fixed** | See [N2 details](#n2-details). | Covered by the F3 tests (wrong header gives rc 1) |
| N3 | A rename hits every use of a name | **fixed (guard)** | See [N3 details](#n3-details). | `test_refusals` (renaming the export name or a path name is refused); `test_rename_roundtrip_all_packages` (more than 20 real packages with free names, plus 127/127 stand-in relayouts byte-identical) |
| N4 | Probe: new folder name, scan cost, attached actors, pcall | **fixed except the folder name (user decision)** | See [N4 details](#n4-details). | Lua host test (above) |
| N5 | "LOADS-CLEAN, 0 missing globals" claim not shown in any output | **withdrawn** | The claim is in no file in this folder (grep finds it only in REVIEW.md). It is not used as evidence anywhere. The Lua host test above is the only probe evidence. | n/a |
| N6 | Rollback does not name the overlay files | **deferred** | Those files are defined by the #7/#8 overlay action, which this team does not own. The card says the #7/#8 owner names them before the card is final. | n/a |

## Details by finding

### F1 details

**What changed:**

- `harvest_matdiag.py`: a slot counts as a MOD slot only when its material
  package is one of the audit's material packages.
- New verdicts:
  - PASS needs at least 1 fixed MOD slot, zero unplanned CHECKER and zero
    UNREADABLE.
  - Slot lines but no MOD slot gives INCONCLUSIVE (exit 1).
  - Zero lines gives FAIL.
- New `--plan`: slots whose material the plan lists as `known-checker` are
  reported as KNOWN_CHECKER and listed, but are not a FAIL.
- Probe v2 (`main.lua`):
  - samples every 30 s for 60 min
  - scans only when the pawn changes, or every 2 min
  - prints a slot set only when it differs from the last one (cap: 12 sets)
  - prints `[END]` at the end
- Launch card: states the 60-min window.

**Test evidence:**

- `test_join` has 10 cases. They include the review's exact repro, a base-only
  `MI_MenuChar` log, which now gives INCONCLUSIVE with rc 1. The others:
  - base + Leon gives PASS
  - fixed + known-checker with the plan gives PASS; without the plan, FAIL
  - known-checker only gives FAIL
- Lua host scenario A:
  - menu pawn, then player pawn, then one material change: 3 sets, 9 slot lines
  - stops at tick 120 with `[END]`
  - at most 68 scans (64 seen)
- Lua host scenario B: stops at the 12-set cap.

### F2 details

**What changed:**

- `package_fetch_plan.py check` now records each header's top-level export
  names and classes. It needs `--script-utoc/--script-ucas`, and header-only
  prefixes now get the full structural parse (`zen.ZenPackage(header_only=True)`).
- `reparent_plan.candidates()` keeps a package only if all of these hold:
  1. it is verified
  2. it is not overridden by the mod (new: the mod's copy would win at load)
  3. it is listed in a loaded store, with PS5 shader maps
  4. it has exactly one top-level `Material`/`MaterialInstanceConstant` export
  5. a fetched base `SkeletalMesh` header imports it, directly or through
     fetched base MIC parents
- `new_export` is now the export's own name from the header, not the package
  basename.
- Excluded packages are listed with their reason.
- The card states the morph/cloth usage gap.

### F3 details

- `mic_reparent.reparent()` raises an error when there is no base header.
- The CLI prints `FAIL: --base-header is mandatory ...; 0 written` and exits 1
  without writing anything.
- Other refusals (input = output, any format error) also exit 1 cleanly.
- The base header may be a header-only prefix.

### F4 details

**Rules in `reparent_plan.plan()`:**

- **Mod Materials:** these are the 13, including `Simon_Slot0..7` and
  2× `M_Invisible`. They become `known-checker`, because a material graph
  cannot be rebuilt without a shader compiler.
- **Parameterless MICs:** these are the 43 under `M_Invisible`. They become
  `known-checker`. Nothing can be matched, and "invisible" cannot be proven
  from the files, so pointing them at a visible base material is refused.
- **Restore-base:** only with `--allow-restore-base`, and only where the base
  is proven (`True`) to have PS5 shader maps. It is no longer the default.

**PASS rule:** narrowed to the fixed slots. KNOWN_CHECKER slots are listed in
harvest and in the card.

### F5 details

The card's step 4 now has:

- the full sha256 `ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867`
  and the size 297,572,462 B, from two sources
- a full-sha readback after the copy
- a warning that the `9a914e38` in the file name is not the hash
- a separate console action with its own OK
- an instruction to keep the restored pak on rollback

### F6 details

- `shader_inputs.py` now plans every TOC's container header
  (`container_header_plan`: 5 rows; 4 to fetch, 12.6 KB).
- `verify_fetch.py` matches each given header to its row by IoHash and fails
  if a planned header is missing.
- The audit reports `base_had_target_shader_maps: None` when no loaded header
  lists the package, and lists those packages.
- `package_fetch_plan check` lists imports that no loaded store lists.
- `reparent_plan` excludes store-unknown candidates and names them.
- APPLY step 2 passes all 5 headers everywhere.
- The evidence JSONs were regenerated. The only differences are the new keys,
  plus `MI_Simon_BodywarmerBeanieTag_Deluxe` going from `False` to `None`.

### N1 details

- PASS now needs `missing == 0` for every container header.
- A library with zero groups is a failure.
- Fetch rows carry exact `pieces`; the length is the sum of the pieces, and the
  file is their concatenation. All current rows are still contiguous.

### N2 details

- Candidate rows record `candidate_verified` (`iohash` or `name-hash`).
- The export hash written into the derived package must equal the base
  header's export map entry (F3). So a corrupt header-only read makes it fail
  closed instead of writing wrong bytes.
- Parameter-name matching on name-hash-only headers is still not IoHash
  covered. A bad name there can only lower the coverage, and that is disclosed
  in the card.

### N3 details

- `rename_names` refuses a source name that is the package name, an export
  name or a `/` path.
- A name that is shared with property data cannot be ruled out without
  decoding the properties; this is stated in the docstring.

### N4 details

- **Fixed:**
  - Scans are limited to pawn changes plus one every 2 min: at most 30 scans
    in 60 min, up from 16 in 5 min, spread out. The card mentions a possible
    tiny hitch.
  - Components of actors attached to the pawn (`GetAttachParentActor`) are
    now included.
  - All calls stay pcall-wrapped.
- **Not fixed:** the new folder `zzmatdiag` needs the user's OK, or it gets
  folded into `zzdiag` by the integrator. The card's step 5 and APPLY step 4
  say this.

## Files changed (team folder only)

- `tools/`: `zen.py`, `package_fetch_plan.py`, `reparent_plan.py`,
  `mic_reparent.py`, `harvest_matdiag.py`, `mod_material_audit.py`,
  `shader_inputs.py`, `verify_fetch.py`, plus the same files in
  `PATCH/material-fallback/`
- `PATCH/zzmatdiag/Scripts/main.lua` (v2, 2,944 B)
- `tests/test_cap12.py`, `tests/lua/test_zzmatdiag.lua`, `tests/run_tests.sh`
- `evidence/townfall-shader-inputs.json`, `evidence/leon-material-audit.json`
- `PATCH/APPLY.md`, `PATCH/SHA256SUMS`, `LAUNCH-CARD.md`, `NOTES.md` (§6 row
  and §9 count)
