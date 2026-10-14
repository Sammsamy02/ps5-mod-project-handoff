# APPLY: texture-diagnosis staged changes (Mac-side tools only; no PS5 code, no PRX)

Nothing here ships to the PS5 or changes the runtime. The C++ gates
(`fs_throw_gate.py`, `no_detour_gate.py`, `verify_build.sh`) do not apply:
no runtime source changes and no build. This patch needs **no build and no launch**.
No step below writes outputs into `outputs/2026-10-08-l25-prep/`; test and tool outputs go to the team folder.

`<TEAM>` = `/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/texture-diagnosis`

## 0. Gate first (Mac-only, writes only under `<TEAM>/tests/`)
```sh
bash <TEAM>/tests/run_tests.sh
```
It must end with `RESULT: PASS`. It runs every suite, prints how many tests each ran, and fails on any failure, any
skip, any suite with zero tests, any zero item count, or any change to a read-only input (11 files, including the live
`pak-overlay/overlay_builder.py`, `test_overlay_builder.py` and both SUSPECT json files).
The patched overlay suite runs from a relocated copy under `<TEAM>/tests/tmp/` and writes its drift receipt to
`<TEAM>/tests/out/overlay/`. **This relocated run is the gate.** Do not run the suite inside `pak-overlay/`.
The lines starting with `(expected, negative test)` are the zero-count gate firing on purpose. They are not failures.

## 1. Closest-match guard for overlay_builder (fail closed)
File: `PATCH/overlay_builder-closest-match-guard.diff`, a unified diff against
`outputs/2026-10-08-l25-prep/pak-overlay/{overlay_builder.py,test_overlay_builder.py}`. It also **creates**
`pak-overlay/cityhash64.py` (new-file hunk), so no separate copy step is needed.
Pre-image sha256: `a72636fbdec542bb6ce9521d6ca5709b61b09739938544671212ec93b0c958e6` and
`965e49b85f42011b2d60afe564d01d7f39b678aaa9b47a89a67b1292dbf51098`. Apply to exactly those versions.
Post-image sha256 (= `PATCH/new/`): `overlay_builder.py` `8b10602a…a24`, `test_overlay_builder.py` `1287f1ef…c9a`,
`cityhash64.py` `5797c5ab…923`.

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
shasum -a 256 outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py outputs/2026-10-08-l25-prep/pak-overlay/test_overlay_builder.py
patch -p1 --dry-run < <TEAM>/PATCH/overlay_builder-closest-match-guard.diff && patch -p1 < <TEAM>/PATCH/overlay_builder-closest-match-guard.diff
shasum -a 256 outputs/2026-10-08-l25-prep/pak-overlay/{overlay_builder,test_overlay_builder,cityhash64}.py   # must equal PATCH/new/
```
What it does:
- `closest-match` refuses with `closest_match_package_import_not_rewritten` when the package's own header still
  imports the old parent, and with `closest_match_package_unparsable` when the payload is not a Zen package, a name is
  not valid UTF-16, or the name bytes do not add up to the batch's own NumStringBytes. `hide` is unchanged.
- Two former success tests now expect the refusal. The Leon test no longer writes `leon-substitutions-fixture.json`.
- The Leon replay test no longer writes `leon-replay-drift.json` next to itself: it writes to `$OVERLAY_TEST_OUT`
  (or a fresh temp folder if that is unset). The SUSPECT files in `pak-overlay/` are never rewritten.

If you ever must run the suite in place after applying, use
`OVERLAY_TEST_OUT=<TEAM>/tests/out/overlay-inplace PYTHONDONTWRITEBYTECODE=1 python3 test_overlay_builder.py`
and hash the folder before and after.

## 2. Material/texture gate and #8 tools (new files)
Copy `tools/*.py` into a new folder `outputs/2026-10-08-l25-prep/pak-overlay/material-gate/`:
`iostore_lib.py`, `cityhash64.py`, `blake3_min.py`, `texture_diag.py`, `material_plan.py`, `zen_import_rewrite.py`,
`base_fetch_plan.py`, `base_texture_layout_stats.py`. All are stdlib-only Python 3, read-only on their inputs, and
fail closed outside UE5 v8 TOC / v5 container header / unvers. Zen. When you run them, give `--out` a path in the team
folder or the evidence tree, never inside `pak-overlay/`.

## 3. Wiring later (not done here)
- Mod-manager install: call `texture_diag.py` and show each material's verdict and `will_draw_checker` before mounting.
  The gate now says OK when the mod's own store entry has shader-map hashes or the export carries the game's own
  shader-platform tokens (positive-control tests prove both).
- #8 reparent, only after the base fetch (DIAGNOSIS.md rows 4-6) confirms parameter names, and only for rows whose
  `static_permutation_check.reparent_can_fix_shader_map` is true (`B_BLOCKED_OWN_STATIC_PERMUTATION` rows never):
  1. `zen_import_rewrite.py` makes the derived MI copies (header names the new parent); `retoc pack-raw` packs them.
  2. **Ordering:** `retoc pack-raw` keeps the **old** parent in the derived container's own store entry. `build_overlay`
     first requires `from_package_id` in that store entry (`substitution_import_not_found` otherwise), then the guard
     reads the package header and admits it only if it names `to_package_id` and not the old parent, then the repoint
     rewrites the merged store entry. A future container writer that rebuilds store entries from headers would make
     step one refuse (fail closed); the row would then need a plain mod entry, not closest-match.
  3. Proven end to end on the real container: `tests/test_zen_import_rewrite.py`
     `test_build_overlay_admits_rewritten_container_end_to_end` (11 rows admitted, store entries repointed).
  Needs its own review and its own launch.
