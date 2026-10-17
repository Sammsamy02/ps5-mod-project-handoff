# How the integrator merges this (Team B, #12 support, Mac-only)

Nothing here touches the console. All tool inputs are read-only game/mod files on the Mac. The only console step is the optional read-only fetch in section 4, which the integrator runs.

## 1. What is in PATCH

All three files are NEW. No existing file changes.

| File | Goes to | Needs |
|---|---|---|
| `PATCH/tools/invisible_route.py` | `outputs/2026-10-08-l25-prep/mod-manager/material-fallback/` | Team A's cap12-fulllook tools in the same folder (`asset_registry`, `cityhash`, `container_header`, `material_class`, `material_props`, `mod_material_audit`, `reparent_plan`). **Merge Team A's cap12-fulllook patch first**: `material_props.py` (reader C) only exists there. |
| `PATCH/tools/texture_tiling_probe.py` | `outputs/2026-10-08-l25-prep/pak-intake/` | `ue_formats.py` (via `--formats-lib`) |
| `PATCH/tools/layout_fetch_plan.py` | `outputs/2026-10-08-l25-prep/pak-intake/` | `ue_formats.py` (via `--formats-lib`) |
| `PATCH/tools/tile_reference.py` | next to `texture_tiling_probe.py` (it imports it) | nothing else. It is the **oracle** for Team A's converter (spec: NOTES.md Q1 §7) |
| `tests/test_invisible_route.py` | next to `invisible_route.py` | env `CAP12B_TOOLS`, `CAP12_TOOLS` = that folder |
| `tests/test_layout_tools.py` | next to the two pak-intake tools | env `CAP12B_TOOLS` = that folder; `CAP12B_ADDRLIB` = `caps-team-B/.../evidence/public-addrlib` |

Checksums: `PATCH/SHA256SUMS`.

```sh
B=/WORKSPACE/.codex/worktrees/translation-core/caps-team-B/caps-team-work/cap12-layout-invisible
T=outputs/2026-10-08-l25-prep/mod-manager/material-fallback
I=outputs/2026-10-08-l25-prep/pak-intake
(cd $B/PATCH && shasum -a 256 -c SHA256SUMS)
cp $B/PATCH/tools/invisible_route.py $B/tests/test_invisible_route.py "$T/"
cp $B/PATCH/tools/texture_tiling_probe.py $B/PATCH/tools/layout_fetch_plan.py $B/PATCH/tools/tile_reference.py $B/tests/test_layout_tools.py "$I/"
CAP12B_TOOLS=$T CAP12_TOOLS=$T PYTHONDONTWRITEBYTECODE=1 python3 -I "$T/test_invisible_route.py"      # expect 59 tests ran, 0 failed
CAP12B_TOOLS=$I CAP12B_ADDRLIB=$B/evidence/public-addrlib PYTHONDONTWRITEBYTECODE=1 python3 -I "$I/test_layout_tools.py"   # expect 18 tests ran, 0 failed (about 3 min)
```

## 2. Invisible route: where it sits in the pipeline

```
reparent_plan.py  ->  invisible_route.py --apply  ->  derive_substitutes.py  ->  paktx.py
(Team A, unchanged)   (adds rows, this patch)        (Team A, unchanged)       (unchanged)
```

Run it on the plan Team A's `run_plan.sh` makes. Every input path is listed in `run_invisible.sh`, including the game's `global.utoc/.ucas` (ScriptObjects, for the classes of mod-only users). Set `TOOLS_DIR=$T` to run the merged copy:

```sh
TOOLS_DIR=$T $B/run_invisible.sh <reparent-plan-r4.json> <reparent-plan-r4-invisible.json> --apply
```

Expected stdout:
- `usage-flag training rows: 135 whole game Materials (20 skeletal-used, 37 static-only); learned index 53 (absent in 32 static-only)`
- `scanned 418 game Materials; 3 named invisible/hide; 1 proven (blend groups ['masked'])`
- `checked 13 mod Materials; 2 proven to draw nothing`
- `considered 57 known-checker rows of 67; 3 reparent, 4 reparent-partial, 50 refused (21 of them have no run-time user)`
- `DONE`

Without `--apply` the rows stay exactly as Team A's plan has them. Only the `invisible_route` report section is added (dry run).

What the overlay needs (all already true for these rows; checked in `tests/test_invisible_route.py`):
- The rows use the normal `reparent` / `reparent-partial` fields: `mod_package`, `old_parent`, `new_parent`, `new_export`, `param_renames` (empty) and `store_entry_repoint` (hash law).
- `derive_substitutes.py` needs the new parent's whole header in its `--headers` dirs: `M_Hide` is `cap12-fetched-20261013/headers-r2/ad7b3742feeb3d06.hdr.bin` (IoHash-verified).
- The 4 partial rows need the user decision `allow-partial`. The user already gives that decision for Leon's 10 clothing rows. **Tell the user the decision now also covers these 4 rows**: "hidden on skeletal meshes, unproven on the other users (there it stays the checker, as today)". Each row's `look_notes` and `partial_reason` say this. `coverage` stays 1.0; `user_coverage` is the proven share.
- The new plan's sha256 changes. Rebuild the substitute manifest from it, since paktx binds the manifest to the plan sha.

Rollback: skip the `invisible_route.py` step. The plan is then byte-identical to Team A's.

## 3. Q1 tools (read-only analysis)

```sh
# a) fetch plan (already generated: $B/fetch-plan/layout-fetch-plan.json, 127 chunks, 10,274,390 bytes)
python3 -I $I/layout_fetch_plan.py --utoc <pristine pakchunk0-ps5.utoc> --ucas-name pakchunk0-ps5.ucas \
  --formats-lib $I --out <plan.json>
# b) probe local containers (game pakchunk10 + the Leon mod): 39 textures checked
python3 -I $I/texture_tiling_probe.py --formats-lib $I --local <utoc> <ucas> [--local ...] --out <probe.json>
```

The pristine pakchunk0 TOC is `.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc` (29,707,562 bytes, the same size as the console's `.original-*.bak`). Do **not** use the console's current, rewritten `pakchunk0-ps5.utoc`. The `.ucas` on the console is the untouched base file, so the pristine TOC's ranges are right for it.

## 4. Read-only fetch: DONE (2026-10-13, 127/127 chunks, IoHash 127 OK)

Results are in NOTES.md Q1 §5. The commands below are kept for re-runs or other games:

```sh
python3 -I outputs/2026-10-08-l25-prep/harvest/ftp_range_fetch.py --plan $B/fetch-plan/layout-fetch-plan.json \
  --host <LAN ip> --port 2121 --remote-dir "<game>/townfall/content/paks" \
  --out outputs/2026-10-08-l25-prep/evidence/cap12-layout-fetch-<date>
python3 -I $I/texture_tiling_probe.py --formats-lib $I \
  --fetched <pristine pakchunk0-ps5.utoc> outputs/2026-10-08-l25-prep/evidence/cap12-layout-fetch-<date>/pakchunk0-ps5 \
  --out outputs/2026-10-08-l25-prep/evidence/cap12-layout-fetch-<date>/probe.json
```

- The probe uses only chunks whose length and IoHash equal the pristine TOC.
- Read the printed header histogram:
  - **Any `HasOptData=0` game texture** whose verdict is `LINEAR (header and pixels agree…)`: the console renders PC-style linear textures. That points to (a) for Leon's large mips.
  - **Only `ExtData` values whose verdict agrees with the public mode:** proves ExtData = AMD swizzle mode from two sources.
  - **The priority-2 pairs** (if their header says `PF_DXT1`): `SW_64KB_S (header and pixels agree)` proves the 8-byte-block tile order on game pixels. A converter is then fully provable for every Leon format.

## 5. Console test (after Team A's material launch)

See the table in `NOTES.md`, sections Q1 §4 and Q2 "In-game check". One launch with the Leon overlay answers both questions. Ask the user to note:
- Leon's clothes up close and far away;
- the TheWall enemy's eyes;
- Simon's glasses, the canula, and Leon's face at middle distance.
