# APPLY: #12 round 5, derived textures in the PS5 layout (Team A, cap12-textures)

Mac-side tooling only. **No runtime source change, no PRX build** (R11 `bfd95db6` stays). The C++ gates
(`fs_throw_gate.py`, `no_detour_gate.py`, `verify_build.sh`) do not apply.

`<TEAM>` = `/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures`,
`<TB>` = Team B's `cap12-layout-invisible/PATCH/tools`, `L25` = `outputs/2026-10-08-l25-prep` (in the `what`
worktree), `MF` = `L25/mod-manager/material-fallback`. Checksums of every staged file: `PATCH/SHA256SUMS`;
final hashes of the evidence: `evidence/README.md`.

## 0. Order and bases

Merge first (their own APPLY files): `../cap12-fulllook` (material-fallback round 4), `../cap12-overlay`
(paktx `--substitute`, `derive_substitutes.py`), and Team B's `texture_tiling_probe.py` + `tile_reference.py`
into `L25/pak-intake/` (pinned here: `ec0d8db0…` and `3aa85621…`; if Team B's files change, this round must
be re-pinned and re-run). `PATCH/base/` holds byte copies of the two Team A pre-images; the gate checks them.

## 1. Gate (Mac-only, about 70 minutes)

```sh
sh <TEAM>/tests/run_tests.sh
```
Must end `RUN PASS mf_cap12=51 mf_full=24 cap07=45 cap12=50 textures=47` and show `cap07_replay: PASS 155061`
and `tex_real: PASS <n>` (team run: 60121). The round-3 MIC-only real dry run is retired: a shipped instance
must now point at derived PS5 textures (see FIXES review 2 #2). Fails on zero tests, any failure or skip, a zero-count gate, a changed
read-only input (incl. Team B's two files), stale sha pins, or a diff that no longer reproduces `PATCH/tools`.
Needs `swiftc` (Xcode) and an Apple-silicon GPU (the second decoder of the swap proof).

## 2. Integrate (Mac)

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
T=<TEAM>/PATCH
for f in $T/base/material-fallback/*.py; do cmp "$f" "$L25/mod-manager/material-fallback/$(basename $f)" || echo "BASE DIFFERS: $f"; done
cmp $T/base/pak-overlay/paktx.py $L25/pak-overlay/paktx.py || echo "BASE DIFFERS: paktx.py"
cp $T/tools/{param_roles,reparent_plan,derive_substitutes,bcn_swap,bcn_decode,texture_package,material_defaults,hair_mask_check,ps5_tiling,game_layout_census}.py \
   $T/tools/metal_bcn_check.swift $T/tools/ps5_layout_census.json $L25/mod-manager/material-fallback/
cp $T/tools/paktx.py $L25/pak-overlay/paktx.py
# every copied file must equal PATCH/SHA256SUMS
cd $L25/mod-manager/material-fallback && python3 -I test_cap12.py && python3 -I test_fulllook.py   # 51 + 24 OK
```
(Equivalent: `patch -p1` with `PATCH/material-fallback.diff` inside `MF`, `patch` with `PATCH/paktx.py.diff`.)
`mic_reparent.py`, `zen.py`, `utoc.py`, `cityhash.py`, `container_header.py`, `blake3.py` do not change.
Copy `<TEAM>/evidence/` to `L25/evidence/cap12-textures-20261013/` (74 MB: it holds the 60 derived texture chunks).

Optional re-check of the census from the game's own chunks (must reproduce the staged file byte for byte):
```sh
python3 -I $MF/game_layout_census.py --toc $L25/pak-mirror-townfall-20261013/pakchunk0-ps5.utoc \
  --chunks $L25/evidence/cap12-layout-fetch-20261013/chunks/pakchunk0-ps5 \
  --probe $L25/evidence/cap12-layout-fetch-20261013/probe.json <Team B evidence>/q1-probe-local.json \
  --extra-probe-rows <Team B evidence>/q1-probe-local.json --out /tmp-free-folder/census.json
```
Expect `101 game texture rows, 9 with pixel agreement, 0 bad chunks`.

## 3. Plan (Mac)

```sh
TOOLS=$MF sh <TEAM>/run_plan.sh <OUT>/reparent-plan-r5-textures.json --accept-unproven-opaque --derive-textures
```
Expect `30 derived PS5-layout texture(s), 10 of them with an exact R<->B swap; 0 instance(s) refused`,
`actions {'reparent': 10, 'known-checker': 57}`, and the sha256 in `evidence/README.md`. Keep this file name
(the manifest records it).

## 4. Manifest (Mac; needs the user's decision first)

```sh
TOOLS=$MF TILEREF=$L25/pak-intake sh <TEAM>/run_derive.sh <OUT>/reparent-plan-r5-textures.json \
  <OUT>/manifest-r5 --accept-unproven-opaque
```
Expect 30 lines `texture ... -> PS5 SW_64KB_S|SW_4KB_S (tail n mips): ... 4 tiling gates pass` (the 10 ORM
lines also `<n> texels exact on both decoders`) and `10 derived, 0 not derived, 30 derived textures, manifest
... sha256 <S>`; `<S>` must equal `manifest_sha256` in `evidence/manifest/manifest.json`. About 6 minutes.

## 5. Dry run (Mac)

Restore any live #7/#12 Leon install first (its own OK). Then:
```sh
python3 -I $L25/pak-overlay/paktx.py install --record $L25/pak-overlay/records/townfall.record.json \
  --mirror $L25/pak-mirror-townfall-20261013 --mod <folder with the unchanged Leon 2K trio> --work W/leon12r5 \
  --substitute <OUT>/manifest-r5/manifest.json --reparent-plan <OUT>/reparent-plan-r5-textures.json \
  --accept-decision accept-unproven-opaque --dry-run          # --tiling-reference defaults to ../pak-intake
```
Expect exit 0, 2 targets, a `SUBSTITUTE (capability #12)` block with 10 `parent ... -> .../M_MRO_Cloth_NonVT`
lines and 30 `DERIVED TEXTURE ... PS5 SW_64KB_S|SW_4KB_S layout ... round trip byte-exact` lines; in
`W/leon12r5/plan.json`: `substitution.post_build[0].substituted_chunks_verified == 70`, `len(substitution.textures)
== 30`; pakchunk0-ps5 sidecar about 188 MB. About 12 minutes (paktx re-derives and re-checks every texture).
Refusals exit 2 with `"console_changed": false`, e.g. `substitute_texture_bytes_not_reproduced`,
`substitute_texture_round_trip_failed`, `substitute_texture_importer_can_draw`,
`substitute_instance_imports_underived_texture`, `substitute_tiling_reference_sha_mismatch`.

## 6. Install, launch, restore (console; each step its own user OK)

As `../cap12-overlay/PATCH/APPLY.md` step 4, with the flags of step 5 plus `--ftp $FTP --confirm <sha12>
--game-closed`. `verify` must say `INSTALLED`. Loadlist row-check, then launch. Rollback: `paktx restore --plan
W/leon12r5/plan.json --ftp $FTP --confirm <sha12> --game-closed` (byte-exact by rename), plus `--cleanup
--allow-delete` (own OK) for the two `_s1.ucas` files.

## 7. Console fetch

**None needed for this launch.** Optional, read-only, information only: `evidence/fetch-hair-mask-textures.json`
(8 whole chunks, 89,657,752 B) — it cannot unlock the hair under the two-source rule.

## 8. Rollback of the tooling

Copy `PATCH/base/material-fallback/*.py` back over `MF`, delete the new files (`bcn_swap.py bcn_decode.py
texture_package.py material_defaults.py hair_mask_check.py ps5_tiling.py game_layout_census.py
metal_bcn_check.swift ps5_layout_census.json`), and copy `PATCH/base/pak-overlay/paktx.py` back.
