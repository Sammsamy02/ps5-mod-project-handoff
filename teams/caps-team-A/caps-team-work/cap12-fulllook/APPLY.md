# #12 full look: how the integrator merges this (Team A, Mac-only)

Nothing here touches the console. All inputs are read-only game files already on the Mac.

## 1. What changes

Target folder (integrated tools): `outputs/2026-10-08-l25-prep/mod-manager/material-fallback/`

| File | Change |
|---|---|
| `material_props.py` | NEW. Reader C: blend mode and shading model from the material's own property block. Decoder learned from the game's own materials against the asset registry. |
| `texture_layout.py` | NEW. Proves a mod texture is a plain mip chain, not a virtual texture. Also has the alpha cut-out check. |
| `part_roles.py` | NEW. Part role (skin / hair / eye / cloth / hard). Used only to rank parents. |
| `param_roles.py` | CHANGED. Slot gate: a virtual-texture slot is lost, and the sampler class must match. Packed masks: same channel order only (rename OK, never a swizzle). Colour+alpha role. |
| `reparent_plan.py` | CHANGED. Same-model route for special models (hair). Reader C on both sides. Role/model ranking. New row fields. Exit 1 if reader C cannot be learned. |
| `master_fetch_plan.py` | CHANGED. New `--same-model-special` option. The default behaviour is unchanged. |
| `test_cap12.py` | CHANGED. Fixtures adapted to the slot gate. T15 now uses rounds 1-3 and checks that rounds 1-2 alone fail closed. |
| `test_fulllook.py` | NEW. 24 tests for every new rule. |

Unchanged (their sha256 stays as Team A's overlay pins them): `mic_reparent.py`, `zen.py`, `utoc.py`, `cityhash.py`, `container_header.py`, `blake3.py`.

The full diff is in `PATCH/cap12-fulllook.diff` (made against `PATCH/tools-orig/`, a byte copy of the integrated tools from 2026-10-13).

## 2. Merge steps

```sh
T=outputs/2026-10-08-l25-prep/mod-manager/material-fallback
P=/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH
diff -rq "$P/tools-orig" "$T" -x __pycache__      # must print nothing (the base has not moved)
cp "$P/tools/"{material_props,texture_layout,part_roles,param_roles,reparent_plan,master_fetch_plan,test_cap12,test_fulllook}.py "$T/"
cd "$T" && python3 -I test_cap12.py && python3 -I test_fulllook.py     # expect 51 + 24 OK
```

If the first `diff` prints anything, the integrated tools changed after this patch was made. Stop and re-base.

## 3. Produce the plan the overlay uses

The plan must use all three header rounds. Reader C needs at least 20 fetched Materials; rounds 1-2 hold only 14, so the tool refuses (exit 1).

```sh
caps-team-work/cap12-fulllook/run_plan.sh <out>/reparent-plan-r4.json --allow-partial --accept-unproven-opaque
```

(`run_plan.sh` lists every input path. Set `TOOLS=$T` to run the merged copy.)

Expected stdout lines:
- `reader C trained on 135 game materials: learned {BlendMode 12, ShadingModel 18, ShadingModels 20}`
- `27/27 candidates have a two-source shading model`
- `actions {'reparent-partial': 10, 'known-checker': 57}`
- `parameter/slot gate evaluated 201 ... pairs for 11 instances`

Team A's overlay (`derive_substitutes.py`) reads only `action`, `mod_package`, `old_parent`, `new_parent`, `new_export` and `param_renames`. These fields keep their meaning; the new fields are additions. The two user decisions are the same as before: `--allow-partial` and `--accept-unproven-opaque`. Both must be passed to the overlay too.

## 4. Console fetch

**No fetch is needed for Leon.**
- Every game master that has PS5 shaders and is used by a skinned mesh is already fetched.
- All 91 skeletal meshes in the registry are fetched.
- The normal-route fetch plan has 0 priority-1 rows.

An optional read-only plan exists for extra evidence and for other mods. It cannot change Leon's result:

- `fetch-plan-r4/fetch-round4-slot-evidence.json` has 171 rows, 5.76 MB, all whole-chunk and IoHash-checkable. The `.p1.json` file has 1 row and `.p2.json` has 170.
- What it adds:
  - more texture-binding evidence (texture class, virtual-texture flag, alpha) for about 20 masters, including 4 more instances of `M_Hair`;
  - the special-model masters (eye, hair-card materials) for future mods.
- If run (integrator only, read-only FTP):
  - `harvest/ftp_range_fetch.py --plan <p1 or p2 json> --out <dir>`
  - then `package_fetch_plan.py check`, as in rounds 1-3.

## 5. Rollback

Copy `PATCH/tools-orig/*.py` back over the target folder. Then delete the 4 new files.
