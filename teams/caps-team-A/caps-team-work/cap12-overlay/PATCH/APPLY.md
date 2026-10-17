# APPLY: capability #12, ship derived package bytes (Team A, cap12-overlay)

Mac-side tooling only. **No runtime source change, no PRX build** (R11 `bfd95db6` stays). The C++
gates (`fs_throw_gate.py`, `no_detour_gate.py`, `verify_build.sh`) do not apply.
`<TEAM>` = `/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-overlay`,
`L25` = `outputs/2026-10-08-l25-prep`. Checksums of every staged file: `SHA256SUMS`.

## What is staged

| File | What | Goes to |
|---|---|---|
| `PATCH/paktx.py.diff` | unified diff against the integrated `L25/pak-overlay/paktx.py` (pre-image sha `a7446c1b750718d1f75f5d92877e8e07724c303d5b3b5025f081b5ed042012dd`) | apply in place (step 2) |
| `PATCH/tools/paktx.py` | the post-image (= integrated + diff) | `L25/pak-overlay/paktx.py` |
| `PATCH/tools/derive_substitutes.py` | new: reparent plan -> `mic_reparent.py` -> derived-packages manifest | `L25/mod-manager/material-fallback/derive_substitutes.py` (it needs the readers + `mic_reparent.py` next to it; it checks their sha pins) |
| `tests/` | cap07 suite copy + `test_substitute.py` (50) + `real_cap12_dryrun.py` + `run_tests.sh` | stay in the team folder (the gate) |
| `evidence/` | Mac-only real dry-run outputs (manifest + 10 derived packages, plans, verifier receipt) | copy to `L25/evidence/cap12-overlay-20261013/` (durable tree) |

**overlay_builder.py is NOT changed: its pin stays `a72636fbdec542bb6ce9521d6ca5709b61b09739938544671212ec93b0c958e6`.**
The new paktx does not use `closest-match` at all. (If Team A's texture-diagnosis closest-match guard
diff is integrated later, the builder sha changes: update `BUILDER_SHA256` in paktx then, and re-run
this gate; nothing in this feature depends on that guard.)

New pins inside paktx (all already integrated files, Team F cap12, `L25/mod-manager/material-fallback/`):
`zen.py b3c421d4…`, `cityhash.py 95955e58…`, `blake3.py 0613633f…`, `container_header.py e703f080…`
(loaded from verified bytes, only when `--substitute` is used; default folder
`../mod-manager/material-fallback` relative to paktx, or `--readers DIR` / `PAKTX_READERS`), and
`mic_reparent.py 520a8055…` (the manifest must name exactly that writer).

## Step 0: gate (Mac-only)

```sh
sh <TEAM>/tests/run_tests.sh
```
Must end `RUN PASS cap07_tests=45 cap12_tests=50` with `cap07_replay: PASS 155061`, `cap12_real: PASS <n>`
(n > 0). It fails on zero tests, any failure or skip, zero real items, a changed read-only input, or a
stale diff. Takes about 12 minutes.

## Step 1: integrate (Mac)

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
shasum -a 256 outputs/2026-10-08-l25-prep/pak-overlay/paktx.py          # must be a7446c1b…
patch -p1 --dry-run < <TEAM>/PATCH/paktx.py.diff && patch -p1 < <TEAM>/PATCH/paktx.py.diff
shasum -a 256 outputs/2026-10-08-l25-prep/pak-overlay/paktx.py          # must equal PATCH/tools/paktx.py in SHA256SUMS
cp <TEAM>/PATCH/tools/derive_substitutes.py outputs/2026-10-08-l25-prep/mod-manager/material-fallback/
mkdir -p outputs/2026-10-08-l25-prep/evidence/cap12-overlay-20261013 && cp -R <TEAM>/evidence/* outputs/2026-10-08-l25-prep/evidence/cap12-overlay-20261013/
```
Then point the cap07 tests at the integrated copy once (or re-run step 0: it runs the staged copy,
which is byte-identical).

## Step 2: make the manifest (Mac, needs the user's two decisions first)

The user must say yes to BOTH (see `../LAUNCH-CARD.md`): `allow-partial` (2 of 3 Leon textures used,
ORM dropped) and `accept-unproven-opaque` (Leon's body/skin treated as opaque).

```sh
MF=outputs/2026-10-08-l25-prep/mod-manager/material-fallback
E=outputs/2026-10-08-l25-prep/evidence; F=$E/cap12-fetched-20261013
LEON=<folder with the unchanged Leon 2K v1.1 files>/zzz_Leon_Townfall_2K_P
python3 -I $MF/derive_substitutes.py --plan $E/cap12-20261012/reparent-plan-r3-partial-opaque.json \
  --mod-utoc $LEON.utoc --mod-ucas $LEON.ucas --mirror outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013 \
  --headers $F/headers-r1 $F/headers-r2 $F/headers-r3 \
  --base-store $F/manifests/pakchunk{0,1,10,11,12}-ps5.container-header.bin \
  --material-fallback $MF --allow-partial --accept-unproven-opaque --out $E/cap12-overlay-20261013/manifest-run
```
Expect `10 derived, 0 not derived` and manifest sha256 **`44b4e00fbdb9741cc230651102cbe03a68701bb7e8fdac0067f2296e663dd057`** (deterministic: same inputs,
same sha, any folder).

## Step 3: dry run (Mac)

If a plain Leon install (#7) is live on the console, it must be restored first (`paktx restore …
--cleanup --allow-delete` with its own OK, or `repair --only`), because the install gate refuses a
changed index. Then:

```sh
T=outputs/2026-10-08-l25-prep/pak-overlay/paktx.py
python3 -I $T install --record outputs/2026-10-08-l25-prep/pak-overlay/records/townfall.record.json \
  --mirror outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013 --mod <Leon folder> --work W/leon12 \
  --substitute $E/cap12-overlay-20261013/manifest-run/manifest.json \
  --reparent-plan $E/cap12-20261012/reparent-plan-r3-partial-opaque.json \
  --accept-decision allow-partial --accept-decision accept-unproven-opaque --dry-run
```
Expect exit 0, 2 targets (pakchunk0-ps5: 66 replaced + 59 new, 10 substituted; pakchunk10-ps5: 2
replaced), a `SUBSTITUTE (capability #12)` block with 10 lines `parent /Game/Leon/M_Leon_… ->
/Game/Townfall/Characters/Materials/M_SkinBasic_Hands`, and in `W/leon12/plan.json`
`substitution.post_build[0].substituted_chunks_verified == 10`. **The plan sha depends on the
`--work` path**, so use the sha THIS run prints (the team's run in `tests/work/real12/plan-sub` was
`4af19c6da2f98cee6cf79877e617552773e8a441b8aa7efadc1821ca2aba3df9`).

Refusals (exit 2, `"console_changed": false`, nothing changed) name the problem, e.g.
`substitute_user_decision_missing`, `substitute_plan_sha_mismatch`, `substitute_base_header_iohash_mismatch`.

## Step 4: install, launch, restore (console; each step its own user OK)

Same as cap07-step1 APPLY steps 5-8, with the four extra flags above on `install`:
`--ftp $FTP --confirm <sha12> --game-closed`. `verify` must say `INSTALLED`. Run the loadlist row-check,
then launch (user OK). Rollback = `paktx restore --plan W/leon12/plan.json --ftp $FTP --confirm <sha12>
--game-closed` (byte-exact by rename), plus `--cleanup --allow-delete` (own OK) to remove the two
`_s1.ucas` files. Optional probe for the look: Team F's `zzmatdiag` (needs its own OK as a new owned
folder) with `harvest_matdiag.py --plan`.

## Fallback

- Game crash with the substituted install: restore, then the plain #7 Leon install (no `--substitute`;
  byte-identical to the integrated tool's) to tell #12 from #7. Separate launch, own OK.
- Wrong look but no crash: restore is enough; the plan rows' `look_notes` say what to expect.
