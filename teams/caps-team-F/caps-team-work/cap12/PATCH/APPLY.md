# Capability #12 — staged files and how to integrate them

All of this is **new files only**. Nothing in the candidate runtime tree, the
frozen builds, l25-prep, or any mod file is changed. No PRX build is needed:
the tools run on the Mac (or later inside the profile generator), and the probe
is a Lua-only diagnostic folder.

Checksums of every staged file: `SHA256SUMS` (this folder).

## What is staged

| Path | What it is |
|---|---|
| `material-fallback/shader_inputs.py` | Finds the game's own PS5 shader libraries from its own file names + TOCs, and writes the exact byte ranges to fetch: the 4 manifests (`fetch_plan`) and every TOC's container header (`container_header_plan`). |
| `material-fallback/verify_fetch.py` | Checks the fetched library bytes (length + BLAKE3 IoHash from the game's TOC), decodes them, recomputes every shader-group id, matches each given container header to its plan row by IoHash, and joins ALL store shader maps (PASS needs 0 missing and every planned container header). |
| `material-fallback/mod_material_audit.py` | For any mod: lists every material that has no PS5 shader code (these draw as the engine's grey/white checker). |
| `material-fallback/package_fetch_plan.py` | Plans/checks reads of base-game package headers (to learn base materials' parameter names). |
| `material-fallback/reparent_plan.py` | Picks, for each broken mod material instance, a base-game parent: must have PS5 shaders in the game's store, a Material/MaterialInstanceConstant export class, evidence that a base skeletal mesh uses it, must not be overridden by the mod, and (round 2) its class must read the same from two sources (surface, lit, no special shading model, opaque or masked) and match the mod master's blend group. Parameters map by exact name or by a two-source role rename; packed masks are never remapped. Everything it cannot fix is listed as `known-checker` with the reason. |
| `material-fallback/asset_registry.py` | Round 2. Read-only reader for the game's own cooked `AssetRegistry.bin` (class + tags of every asset; fails closed on any count mismatch). |
| `material-fallback/material_class.py` | Round 2. Material domain / blend / lit from two sources: registry tags + the candidate's own compiled PS5 shader types (library manifest + the game's `ShaderTypeInfo-*.stinfo`) compared with leave-one-out peers. Mod side: two readers (shader type hashes in the mod's PC shader map + its base-pass buffer name). |
| `material-fallback/param_roles.py` | Round 2. Parameter roles, texture-binding decode from instance bytes, cooked pixel formats, and the two-source role mapping. |
| `material-fallback/master_fetch_plan.py` | Round 2. Writes a read-only fetch plan for more game materials that pass the two-source gate (plus skeletal-mesh headers and small instances for evidence). Also writes `.p1.json` / `.p2.json`. |
| `material-fallback/mic_reparent.py` | Writes a DERIVED copy of a mod package whose parent import points at that base material (mod file untouched). `--base-header` is mandatory (second source); exit 1 without it. |
| `material-fallback/harvest_matdiag.py` | Joins the probe's console log lines with the audit (and the plan): PASS = at least one mod slot fixed, no unplanned checker slot; base-only logs are INCONCLUSIVE, not PASS. |
| `material-fallback/{utoc,zen,container_header,shader_library,cityhash,blake3}.py` | Small read-only format readers used by the above. |
| `material-fallback/test_cap12.py` | 51 host tests (same file as `../tests/test_cap12.py`). |
| `zzmatdiag/Scripts/main.lua` | Read-only Lua probe v2 (2,944 bytes, under the 4,096 cap): prints every material slot of the player's meshes (and of actors attached to the pawn) and its base material, as `[mat12] [S] ...` lines; samples for 60 min, prints a set only when it changes. |

## Step 1 — Mac-side, now (no console)

1. Copy `material-fallback/` to `outputs/2026-10-08-l25-prep/mod-manager/material-fallback/`.
2. Copy `../evidence/*.json` to `outputs/2026-10-08-l25-prep/evidence/cap12-20261012/`
   (they are Mac-only results; durable tree rule).
3. Run the gate: `bash ../tests/run_tests.sh` (or `python3 -I test_cap12.py` in the
   copied folder). It must print `Ran 51 tests ... OK` and `ALL PASS`. It FAILS if
   zero tests ran, any test failed or was skipped, the staged copy differs from `tools/`,
   or any real input file is missing.

## Step 2 — read-only fetches (integrator; game closed; FTP RETR only, no writes)

Listed in `needs_from_integrator` and in `../evidence/townfall-shader-inputs.json`
(`fetch_plan` = 4 manifests, 1.80 MB; `container_header_plan` = the container
headers of pakchunk1/10/11/12, 12.6 KB in all; pakchunk0's is already on the Mac
and matches its row's IoHash) and `../evidence/fetch-round1-overridden-base-packages.json`.
Save each row under its `local_name`; when a row has several `pieces`, the file is
the pieces concatenated in order. Then (pass EVERY container header, all 5):

```sh
CH="<.superpowers>/native-observations/townfall-original-container-header-1/container-header.bin \
    <fetched>/pakchunk1-ps5.container-header.bin <fetched>/pakchunk10-ps5.container-header.bin \
    <fetched>/pakchunk11-ps5.container-header.bin <fetched>/pakchunk12-ps5.container-header.bin"
python3 -I verify_fetch.py --plan <evidence>/townfall-shader-inputs.json --dir <fetched> \
    --container-header $CH --out <evidence>/cap12-manifest-verify.json          # must print PASS
python3 -I package_fetch_plan.py check --plan <evidence>/fetch-round1-overridden-base-packages.json \
    --dir <fetched-headers> --base-header $CH \
    --script-utoc <global.utoc> --script-ucas <global.ucas> --out <evidence>/round1-check.json
```
`round1-check.json` lists, per base package, its top-level export classes and its
imports that have PS5 shader maps (and any import no loaded container header
lists). Plan round 2 from those (`package_fetch_plan.py plan --packages ...`), check
it the same way, then:

```sh
X=<.superpowers>/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall   # the game's own pak files
CLS="--asset-registry $X/AssetRegistry.bin \
     --library <fetched>/Townfall_Chunk0.type8.bin <fetched>/Global.type8.bin \
     --stinfo $X/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo $X/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo"
python3 -I reparent_plan.py --audit <evidence>/leon-material-audit-5hdr.json --mod-utoc <mod.utoc> --mod-ucas <mod.ucas> \
    --checked-headers <evidence>/round1-check.json <evidence>/round2-check.json [<evidence>/round3-check.json] \
    --fetched-dir <fetched>/headers-r1 <fetched>/headers-r2 [<fetched>/headers-r3] \
    --base-header $CH $CLS [--allow-partial] [--accept-unproven-opaque] --out <evidence>/reparent-plan-r2[-...].json
# --allow-partial and --accept-unproven-opaque are USER decisions (LAUNCH-CARD.md); each affected row says so.
```
Round 3 (optional, widens the pool; plan already written, Mac-only):

```sh
python3 -I master_fetch_plan.py $CLS --base-header $CH --toc <the 5 base .utoc> \
    --checked-headers <evidence>/round1-check.json <evidence>/round2-check.json \
    --mod-utoc <mod.utoc> --mod-ucas <mod.ucas> --out <evidence>/fetch-round3-masters.json
# integrator, game closed, read-only:  ftp_range_fetch.py --plan fetch-round3-masters.json (or .p1.json) --out <fetched>/headers-r3
python3 -I package_fetch_plan.py check --plan <evidence>/fetch-round3-masters.json --dir <fetched>/headers-r3 \
    --base-header $CH --script-utoc <global.utoc> --script-ucas <global.ucas> --out <evidence>/round3-check.json
```
`ftp_range_fetch.py --max-priority` does not see `priority` on #12-shaped rows; use the `.p1.json` file to fetch
only priority 1.
Re-run `mod_material_audit.py` with all 5 container headers first, so the one
pakchunk10 override is no longer "unknown". Every `reparent` / `reparent-partial` row then needs
`mic_reparent.py ... --base-header <that candidate's fetched header> --rename OLD=NEW` (from the row's
`param_renames`).

## Step 3 — what is NOT staged and needs the #7/#8 overlay owner

The existing overlay action `closest-match` (overlay_builder.py:1955-1981) only
rewrites the derived **store entry** import. That is not enough on its own:
the mod package's own header still names the old parent export by its public
export hash (`m_leon_body`), and the base material exports under a different
name/hash (`m_mro_basic`), so the import would not resolve (Mac-only reasoning;
the hash law itself is verified on 127/127 Leon exports). The overlay needs a
new action that ships the **derived package bytes** from `mic_reparent.py` as
the chunk for that package id (new chunk row, IoHash recomputed with the
overlay's existing `blake3_20`), together with the store-entry repoint
(`store_entry_import_change` in the receipt; the slot order matches: store
imports == header import names for 127/127 Leon packages). Original mod files
stay untouched. This is a #7/#8 code change and gets that team's review.

## Step 4 — the probe (Lua-only fast deploy, needs the user's OK)

Deploy `zzmatdiag/` as an exact mirror (it is a diagnostic folder we own), run
the loadlist row-check with its actual size (2,944 bytes). It can ride the
next Leon launch; it changes nothing in the game. After the game is closed,
decode the log with `harvest/decode_session_log.py`, then:

```sh
python3 -I harvest_matdiag.py --log <decoded.txt> --audit <evidence>/leon-material-audit.json --expect-checker   # before the fix
python3 -I harvest_matdiag.py --log <decoded.txt> --audit <evidence>/leon-material-audit.json \
    --plan <evidence>/reparent-plan.json                                                                       # after the fix: must PASS
```
The probe is a NEW owned diagnostic folder (`zzmatdiag`); CLAUDE.md names only
`ueselftest`/`zzdiag`. It needs the user's OK as a new owned folder (or the
integrator folds it into `zzdiag`).

## Gates

- Before use: `run_tests.sh` ALL PASS (51 python + Lua probe test, non-zero counts).
- Every CLI prints how many items it checked and exits 1 on zero.
- No PRX change, so fs_throw_gate / no_detour_gate / verify_build are not
  affected by this folder. If any of it is ever moved into C++, those gates apply.
