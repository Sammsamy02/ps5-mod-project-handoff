# NOTES: capability #12, "ship the derived package" in the pak tool (Team A)

2026-10-13. **Everything here is Mac-only.** No console contact, no build, no change to the
candidate tree, frozen builds, mod files, other team folders or the integrated tools in
`outputs/2026-10-08-l25-prep/pak-overlay/` (staged copies + diff only).

## The problem in one paragraph

Leon's material instances (MI_Leon_*) point at Leon's own master materials, which have no PS5
shader code, so the PS5 draws them as a grey/white checkerboard. Team F's plan (r3) picks a
game material for 10 of them (`M_SkinBasic_Hands`, opaque, lit) and `mic_reparent.py` writes
a corrected copy of each package. Changing only the container's store entry (the old
`closest-match` action) is not enough, because the package header itself names the parent by
a public export hash. So the pak tool must ship the corrected package bytes in place of the
mod's own. That is what this change adds.

## What changed

1. **`paktx.py install --substitute MANIFEST --reparent-plan PLAN --accept-decision D ...`**
   (staged `PATCH/tools/paktx.py`, diff `PATCH/paktx.py.diff` against the integrated
   `a7446c1b…`). For each listed package it:
   - puts the derived bytes into the intermediate projected trio in place of the mod's chunk:
     new stored blocks, new length, IoHash = BLAKE3-20 of the derived bytes; the trio's store
     entry gets the repointed import (old mod parent -> base parent, same slot). The trio is
     deleted after the build; `overlay_builder.build_overlay` is unchanged (pin `a72636fb…`
     stays) and copies it into the sidecar as usual.
   - **re-checks every row before anything is planned** (refuses otherwise, exit 2, nothing on
     the console changes), with two sources for every hash:

     | Value | Source 1 (recomputed here) | Source 2 (game / mod data) |
     |---|---|---|
     | new parent's export hash | CityHash law of the export name | the parent's own export map in its fetched header, which must be the WHOLE base chunk: length + IoHash equal the base TOC row (two BLAKE3 implementations) |
     | new parent's package id | CityHash law of the name | present in a base store (store tied to its TOC by IoHash, read by two parsers) and as an export chunk id in a base TOC |
     | derived chunk IoHash | overlay_builder BLAKE3 | Team F `blake3.py` (independent code), and the manifest value |
     | store-entry imports | CityHash law of the header's own import names | the mod's own store entry (slot order must match 1:1) |
     | the derived header | `zen.py` (Team F) parse | a struct-only byte comparison: everything except the name map, the one hash slot and the one imported-package slot is byte-identical; offsets move by exactly the name-map delta; every stored FName hash follows the law (proved on the cooker's own original hashes) |
   - also checks: the manifest is bound to the reviewed reparent plan by sha256, and each row
     must equal exactly one plan row (package, action, old/new parent, export, renames, repoint);
     the user decisions come **from the plan** and must be restated **exactly**
     (`--accept-decision allow-partial --accept-decision accept-unproven-opaque` for r3);
     the mod is the exact build the manifest was made from (utoc + ucas sha256) and the
     original chunk sha + IoHash match; export bytes and everything after the header are
     identical; only declared parameter renames; the entry has no own shader maps; the
     package is in exactly one mod; the parent is not overridden by any mod; the parent's own
     imports are base packages; **every package import of every derived package resolves at
     export level** (target = the winning mod package, or the verified base header) and at
     package level; with `--only-into`, every substituted package and every mod package it
     imports must be installed by this plan.
   - **after the build, re-reads what will ship**: the derived index re-parses and its lookup
     validates; each substituted row lies in the sidecar and extracts as the derived bytes with
     the right IoHash; the merged store (two parsers) holds the repointed entry; all imports of
     the substituted entries name known packages.
   - writes it all into the plan (`plan["substitution"]`: rows, base stores, post-build counts,
     plan rows NOT shipped). The plan sha binds the manifest (nonce).
   - **Restore is unchanged**: the original index goes back by rename (byte-exact by
     construction). Proven byte-exact in the tests, also after every interruption point.
   - Without `--substitute` the tool behaves as before: the plain Leon install is
     byte-identical to the integrated paktx (A/B check on real Leon and two synthetic games).
2. **`derive_substitutes.py`** (new, staged `PATCH/tools/`): reparent plan -> runs
   `mic_reparent.py` (unchanged, pinned) per row -> self-contained `manifest.json` with the
   derived packages, the parent header and the 5 base container headers copied next to it
   (relative paths, so its sha does not depend on the folder). A `reparent-partial` row needs
   `--allow-partial`; an `unproven_opaque` row needs `--accept-unproven-opaque`; a row whose
   decision is missing is not derived and is listed. Exit 1 on zero rows or any refusal.

## Real data (Mac-only, read-only inputs)

Inputs: plan r3 `reparent-plan-r3-partial-opaque.json` (sha `58ed3724…`), fetched headers
`cap12-fetched-20261013/headers-r{1,2,3}`, the 5 fetched container headers, record
`townfall.record.json` (`6acb5013…`), mirror `pak-mirror-townfall-20261013`, Leon 2K trio.

- The parent header `09fc94d29f805ce3.hdr.bin` (22,388 B) is the whole base chunk: its IoHash
  `3e5f7d0f…` equals the pakchunk0 TOC row. All 5 fetched container headers match their TOC rows.
- **Manifest sha256 `44b4e00fbdb9741cc230651102cbe03a68701bb7e8fdac0067f2296e663dd057`**: 10 derived packages (815–849 B; originals 792–818 B), all
  to `/Game/Townfall/Characters/Materials/M_SkinBasic_Hands`, rename `BaseColor -> Albedo`,
  store slot repointed `1532e3f9…` (M_Leon_Body, 8 rows) or `98936729…` (M_Leon_Skin, 2 rows) -> `09fc94d2…`. None has own shader maps.
- **Dry-run plan (`tests/work/real12/plan-sub`): sha `4af19c6da2f98cee6cf79877e617552773e8a441b8aa7efadc1821ca2aba3df9`**, 2 targets:
  pakchunk0-ps5 (66 replaced + 59 new packages, 10 of them substituted) and pakchunk10-ps5
  (2 replaced, no substitution; byte-identical to the plain plan). Plain plan for comparison:
  `d0a5eb252ab0ed22e45519ccdede078b83b6f72c29c7ca61789e3ab6613cef37`. **The plan sha includes the `--work` path; the integrator's own dry run will
  have its own sha, which is the one to confirm.**
- Independent re-read (Team F readers, not overlay_builder): 60,055 items PASS: 59,411 base rows identical to the plain plan, 149 mod rows byte-identical, the 10 derived rows equal the derived bytes with matching IoHash, the merged store differs from the plain plan in exactly those 10 entries (one import slot each, shader maps kept), 40/40 package imports of the derived packages resolve at package and export level, pakchunk10 is byte-identical, and the plain install is byte-identical to the integrated paktx's (4 files).
- One finding, not caused by #12: 8 of Leon's own unchanged packages (`Simon_Slot0..7`) import
  `/InterchangeAssets/Functions/MF_PhongToMetalRoughness`, an editor-only package that is in no
  base store. That is the same in the plain Leon install; the plan lists them under
  `unchanged_mod_entries_with_unknown_imports`. The 10 substituted packages import only known
  packages.

## Tests (`sh tests/run_tests.sh`)

Final run 20261013T152506Z: `cap07: 45/45`, `cap07_replay: PASS 155061`, `cap12: 50/50`, `cap12_real: PASS 60055`, `RUN PASS cap07_tests=45 cap12_tests=50` (about 12 min).

- cap07 suite (45 tests) + real Leon replay run against the STAGED paktx: unchanged results.
- cap12 suite, 50 tests: real Leon bytes in a synthetic game (MI_Leon_Eye + its 3 textures +
  M_Leon_Body, the real fetched parent). Positive: plan, install + independent re-read of the
  installed files, `cycle`, byte-exact restore, every interruption point restored byte-exact,
  determinism, manifest sha independent of folder, multi-block derived chunk, A/B vs the
  integrated paktx. Negative (each names its code, console folder unchanged): decisions missing /
  partial / unknown / not needed / stripped from the manifest; no plan / other plan / row not as in
  the plan; manifest tampered; derived bytes changed; wrong public hash; export bytes changed;
  tail after exports changed; texture hash changed; FName hash tampered; unread header region
  tampered; old hash claim wrong; name change not declared; original not this mod; other mod
  build; store claim wrong; duplicate row; zero rows; base header tampered / prefix only /
  missing; base store tampered / disagrees with record / missing (parent imports unresolved);
  reader pin; `--only-into` drops the package; post-build catches unshipped bytes and a store
  entry not repointed; parent overridden by another mod; package in two mods; import target won
  by another mod; import unresolvable at export level; parent not in base store; own shader maps
  (generator and paktx); generator: decisions, non-empty out folder, bad / prefix header,
  parent overridden.
- Planted faults (scratch copies, removed after): turning off the base-header IoHash check, the
  post-build payload check, the decision check, the trio IoHash, the export-level check, the raw
  map comparison or the plan-row match each makes at least one test fail (1–6 tests).
- The runner fails on zero tests, any failure or skip, a count mismatch, zero real items, a
  changed read-only input (integrated paktx, overlay_builder, mic_reparent, plan r3), or a diff
  that no longer reproduces the staged paktx.

## Review

One independent reviewer (12 findings: 0 blocker, 2 major, 7 minor, 3 notes). All fixed except
where noted; see `REVIEW.md`.

## Open risks / not proven

1. **Nothing here is console-proven.** The engine loading a derived MIC whose parent is a base
   material is Mac-only reasoning (the hash law is verified 127/127 on Leon; the store/header slot
   order 10/10 on these rows).
2. **Look**: Leon's ORM (roughness/metal/AO) texture is not used (parent defaults); the parent's
   shading model is SubsurfaceProfile (one source, Team F `look_notes`); opaque is a user
   decision (`--accept-unproven-opaque`), and partial coverage (2 of 3 parameters) is a user
   decision (`--allow-partial`). Leon's hair stays checkered.
3. The substituted mod is always projected (the console-proven Leon run used the unprojected
   whole mod for pakchunk0). The projection itself is byte-checked and the cap07 replay proves
   it equal to the console-proven derived index, but this is the first launch of a projected
   pakchunk0 trio with replaced payloads.
4. Untested paths (no real case): a derived package importing another derived package; a mod
   chunk with non-zero meta flags (ModSpec refuses those anyway); a mod-provided import target
   outside the `--only-into` container (refusal code exists, no fixture).
5. The parent's lit model and morph-target/cloth usage are not checked (Team F open items).
