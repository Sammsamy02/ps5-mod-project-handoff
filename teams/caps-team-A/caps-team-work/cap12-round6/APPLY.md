# APPLY: #12 round 6, the hidden accessories (Team A, cap12-round6)

Mac-side tooling only. **No runtime source change, no PRX build** (R11 `bfd95db6` stays). The C++ gates
(`fs_throw_gate.py`, `no_detour_gate.py`, `verify_build.sh`) do not apply. `derive_substitutes.py`, `paktx.py`
and every round-5 file are **unchanged**.

`<TEAM>` = `/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6`,
`L25` = `outputs/2026-10-08-l25-prep` (in the `what` worktree), `MF` = `L25/mod-manager/material-fallback`,
`P5` = `L25/evidence/cap12-launch-prep-20261013/reparent-plan-r5-textures.json` (the exact console plan of
launch H). Checksums of every staged file: `PATCH/SHA256SUMS`.

## 0. Bases

- Round 5 as integrated (`MF`, `L25/pak-overlay/paktx.py` + `overlay_builder.py`, `L25/pak-intake/`).
- Team B's `invisible_route.py` (sha `72176825...`, `caps-team-B/.../cap12-layout-invisible/PATCH/tools/`) was
  **never integrated**; this round ships a modified copy of it instead (pre-image: `PATCH/base/invisible_route.py`,
  diff: `PATCH/invisible_route.py.diff`). Do not integrate Team B's copy separately.

## 1. Gate (Mac-only, about 30 minutes)

```sh
sh <TEAM>/tests/run_tests.sh
```
Must end `RUN PASS round6=43 invisible_r6=59 mf_cap12=51 mf_full=24 real_checks=27` (team run: see
`tests/output/run.log`). It fails on zero tests, any failure or skip, stale pins, a diff that no longer
reproduces `PATCH/tools/invisible_route.py`, a changed read-only input, or a `work/tools` copy that is not
byte-equal to the integrated file. Needs `swiftc` + an Apple-silicon GPU (round-5 texture proof inside the
derive step).

## 2. Integrate (Mac)

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
T=<TEAM>/PATCH
(cd $T && shasum -a 256 -c SHA256SUMS)
ls $MF/invisible_route.py 2>/dev/null && echo "STOP: an invisible_route.py is already integrated"
cp $T/tools/invisible_route.py $T/tools/mesh_sections.py $T/tools/base_material_route.py \
   $T/tools/mesh_slot_repoint.py $T/tools/attribution.py $MF/
# every copied file must equal PATCH/SHA256SUMS
R6_TOOLS=$MF PYTHONDONTWRITEBYTECODE=1 python3 -I <TEAM>/tests/test_round6.py                    # 43 tests, 0 failed
CAP12B_TOOLS=$MF CAP12_TOOLS=$MF PYTHONDONTWRITEBYTECODE=1 python3 -I <TEAM>/tests/test_invisible_route_r6.py   # 59, ~7 min
(cd $MF && python3 -I test_cap12.py && python3 -I test_fulllook.py)                                # 51 + 24 OK
```
All five files are NEW in `MF`; nothing is overwritten. `mesh_slot_repoint.py` is a builder for inspection
only (paktx never calls it). Copy `<TEAM>/evidence/` to `L25/evidence/cap12-round6-20261013/` (~75 MB:
it holds the derived chunks of the manifest).

## 3. Plan (Mac, ~30 s)

```sh
TOOLS_DIR=$MF ROUTE=$MF/invisible_route.py sh <TEAM>/run_invisible.sh $P5 <OUT>/reparent-plan-r6.json --apply
```
Expect (also in `evidence/plan-r6/invisible-route.stdout.txt`):
- `377 whole fetched game packages, 12 refused (shared-reader export bytes differ from CookedSerialOffset)`
- `vertex-factory training rows: 135 whole game Materials (82 drawn by StaticMeshes, 20 by SkeletalMeshes);
  static-mesh token FLocalVertexFactoryShaderParameters; skeletal token FGPUSkinVertexFactoryShaderParameters`
- `scanned 418 game Materials; 3 named invisible/hide; 1 proven (blend groups ['masked']); M_Hide proven for SkeletalMesh+StaticMesh`
- `considered 57 known-checker rows of 67; 10 reparent, 4 reparent-partial, 43 refused (21 of them have no run-time user)`
- sha256 `889c986a5a281b487f972dbcd6e51841215e486958c745432443f34d9e60a093` (deterministic; checked with two
  different tool folders, and identical with or without `--base-material-report FILE`, which only writes the
  report-only base-material decisions to FILE). **Keep this file name**: the manifest records it.

## 4. Manifest (Mac, ~5 min; needs both user decisions first)

```sh
cd <OUT-parent> && TOOLS=$MF TILEREF=$L25/pak-intake sh <TEAM>/run_derive.sh <OUT>/reparent-plan-r6.json \
  <OUT>/manifest-r6 --accept-unproven-opaque --allow-partial
```
Expect the 30 texture lines of round 5 unchanged, then `24 derived, 0 not derived, 30 derived textures,
manifest ... sha256 a29bbcaf9984ddde2350105f634c0160fbf11e87226f1dbd9e06146e34529ce0`
(must equal `manifest_sha256` in `evidence/manifest-r6/manifest.json`).

## 5. Dry run (Mac, ~12 min)

Restore any live #12 Leon install first (its own OK). Then:
```sh
python3 -I $L25/pak-overlay/paktx.py install --record $L25/pak-overlay/records/townfall.record.json \
  --mirror $L25/pak-mirror-townfall-20261013 \
  --mod <folder with the unchanged Leon 2K trio> --work W/leon12r6 \
  --substitute <OUT>/manifest-r6/manifest.json --reparent-plan <OUT>/reparent-plan-r6.json \
  --accept-decision accept-unproven-opaque --accept-decision allow-partial --dry-run
```
Expect exit 0, 2 targets, a `SUBSTITUTE (capability #12)` block with **24** `parent ... ->` lines (10 to
`.../M_MRO_Cloth_NonVT` exactly as round 5, 14 to `/Game/MetaHumans/Common/Materials/M_Hide`) and 30
`DERIVED TEXTURE ...` lines; in `W/leon12r6/plan.json`: `substitution.post_build[0].substituted_chunks_verified
== 84`, `len(substitution.textures) == 30`, `user_decisions == ["accept-unproven-opaque", "allow-partial"]`.
Team run: `evidence/plan-r6/paktx-dry-run.txt`, `evidence/plan-r6/paktx-plan.json`.

## 6. Install, launch, restore (console; each step its own user OK)

As round 5 (`../cap12-textures/APPLY.md` step 6) with the flags of step 5 plus `--ftp $FTP --confirm <sha12>
--game-closed`. `verify` must say `INSTALLED`. Loadlist row-check, then launch. Rollback: `paktx restore --plan
W/leon12r6/plan.json --ftp $FTP --confirm <sha12> --game-closed` (byte-exact), plus `--cleanup --allow-delete`
(own OK) for the two `_s1.ucas` files. The round-5 manifest (`d7bf0d45...`) stays available as the fallback.

## 7. Console fetch

**None needed.** Optional, read-only, information only: `evidence/fetch-optional/fetch-plan-optional.json`
(7 whole chunks of `pakchunk0-ps5.ucas`, 1,299,111 B: 2 hair masters, 3 TheWall eye masters, the IV-bag liquid
effect, `BP_Bill`; made by the integrated `package_fetch_plan.py` from the pristine TOC; `ftp_range_fetch.py
--plan ...`, then `package_fetch_plan.py check`). If fetched, re-running step 3 may turn the 3 TheWall eye
rows partial (stricter, same bytes shipped).

## 8. Rollback of the tooling

Delete the five new files from `MF`. Nothing else changed.

## 9. Follow-up for the integrator (not part of this launch)

`zen.ZenPackage.export_bytes` assigns export data in bundle order; for packages whose first serialized
export is not first in the data region (all 14 multi-export mod meshes) that is wrong. Round-5 shipping reads
are single-export only, so unaffected; the round-6 route refuses any package where it differs. Making it
CookedSerialOffset-based needs a re-pin of `zen.py` in `derive_substitutes.py` / `paktx.py` (own review).
