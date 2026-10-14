# Agent CM — l26 build drivers repointed + row-synced (plan-only, no builds)

Date: 2026-10-08. Mac-only, no console/network contact, no toolchain
execution (`--plan` only), no source edits outside the l26 tree
(`outputs/2026-10-08-l26-runtime/`). The l25 drivers were imported in shape
and repointed, exactly following the l25-round pattern
(l25 was BASE=l24-build-r2 OUT=l25-build-r2; l26 is BASE=l25-A
OUT=l26-build-variants).

## 1. Driver changes (file:line)

### `outputs/2026-10-08-l26-runtime/build_runtime.py`
- :6 `BASE=ROOT/'outputs/2026-10-08-l25-build-variants/l25-A'` — the l26
  BASE is the **l25 variant-A build dir** (see §2 for why this is the right
  "l25-built state").
- :7 `OUT=ROOT/'outputs/2026-10-08-l26-build-r2'` (single-runtime fallback
  path; the twin driver is the real flow).
- :71-76 overlay conflict `layer_rank` now prefers
  `l25-runtime > l24-runtime > l23-runtime` (the BASE argvs reference the
  l25-A merged overlay; profile-header names conflict with the older
  l22-build-r3 overlay, so the l25 layer must win).
- :118 selection label `l26_fresh_...`; :135 `runtime-l26.o`;
  :150 `MAC_ONLY_L26_COMPLETE_LINK` + l26 scope text;
  :160-161 `MAC_ONLY_IDENTICAL_L26_RUNTIME_PAIR` + PASS line.

### `outputs/2026-10-08-l26-runtime/build_variants.py`
- :2-36 docstring rewritten for l26, explicitly documenting the variant-dir
  BASE model and that the token keeps its L25-era spelling
  (`MODBRIDGE_L25_SKIP_DELETE_LISTENER`) because the guards already landed
  in the tree use that spelling (renaming would be a source edit and would
  trip G4).
- :53 `PAIR_ROOT=ROOT/'outputs/2026-10-08-l26-build-variants'` (QCOMPOSED
  staging path derives from it).
- :57-69 VARIANTS → dirs `l26-A`/`l26-B`, artifacts `runtime-l26-A.o` /
  `runtime-l26-B.o`, labels + `MAC_ONLY_L26_*` statuses.
- :137 layer_rank gains the l25 layer (same reason as build_runtime).
- :153-175 G2r wording: rows must key on identities the **l25-A BASE build**
  consumed; G2t note updated (the four listener units are already fresh l25
  objects in BASE; rows must keep them affected).
- :472 `l26_variant_...` selection label; :517 G12 renamed
  `G12-header-routing-resolves-to-l26-candidates`; :526 scope text;
  :554-557 `l26_variant` marker keys; :568 pair receipt
  `MAC_ONLY_L26_VARIANT_PAIR_A_VS_B_RECEIPT`; :343/:378 wiring paths →
  l26 tree / `l26_packaging_driver.py`.

### `outputs/2026-10-08-l26-runtime/l26_packaging_driver.py`
(renamed from `l25_packaging_driver.py`; the frozen derivation source stays
`L24PKG=outputs/2026-10-08-l24-package` — scripts byte-copied from there,
manifest derived from its frozen `BUILD-INPUTS.json`, exactly like l25 did)
- :42-43 `L26RUN=HERE`, `PAIR_ROOT=.../2026-10-08-l26-build-variants`.
- :49-54 variants → `l26-A/l26-B` build dirs, package dirs
  `outputs/2026-10-08-l26-package-A/B`, artifacts `runtime-l26-A/B.o`,
  updated b.note labels (l25-A BASE + carried l25 profile promotion).
- :62/:79/:89-91 profile lineage from the l26 tree candidates;
  `l26_profile_digest`/`l26_profile_promoted_facts` + lineage note.
- :97 `l26_variant`; :104-117 copyin-probe wiring reads the repointed l26
  `copyin-probe/MANIFEST.json` (bridge base 89e4b643 still asserted == the
  frozen l24 manifest outer_bridge).
- :123-146 `l26map` remaps the frozen l24-build-r2 pins to the l26-A/B
  outputs (key set unchanged — they are l24-relative rel paths);
  :200 `MAC_ONLY_L26_PACKAGE_PREPARED`; :248/:274 l26 final/pair statuses;
  :308 `P2-l26-profile-candidates`.

## 2. BASE model verification (variants-dir base)

The pair root itself has no `pass1/pass2`; each variant dir does. **BASE =
`outputs/2026-10-08-l25-build-variants/l25-A`** (variant A = the production
normal-registration line; l25-B is the UNSAFE skip-listener diagnostic and is
never a BASE). Verified against the real receipts: `l25-A/` carries
`SOURCE-SELECTION.json`, `pass{1,2}/COMPOSITION-RECEIPT.json` (374 units,
pinned argvs/objects/deps), `pass{1,2}/undefined.stdout` (G7 baselines),
`LINK-COMMAND.json` (391 ordered inputs + fixed_inputs), `headers.vfs.json`
(29 merged roots covering every historical spelling). G1 passed against all
BASE selection pins. The overlay-rewrite chain
(`rewritten_inherited` + `layer_rank`) maps every historical identity —
port-root, l22/l23/l24 names — to the l26 candidates because the rows are
keyed on the l25-runtime candidates those overlays target.

## 3. Row-sync (the l25-round discipline, applied to l26)

Empirical rule: a row's `original` must be an identity the **l25-A build
actually consumed**. Verified every candidate identity against the l25-A
receipts (unit sources, dep universe from all 374 `.d` files, overlay
targets). Fixes made in
`outputs/2026-10-08-l26-runtime/{profile,typescan-r4,thread}/CHANGES.json`:

- Carried l25 rows (19 typescan + 3 profile headers + 1 profile lineage
  json): `original` re-keyed onto the **l25-runtime candidate paths** (the
  consumed identities; e.g. `FMapProperty.cpp` → unit
  `unreal-src-property-fmapproperty`'s l25 source). A concurrent agent had
  repointed all candidates at the l26 tree first, which would have left
  identity no-op rows surviving only via the fragile (name,digest) alias —
  now keyed correctly.
- `UnrealInitializer.cpp`: original := the **composed staging file**
  `outputs/2026-10-08-l25-build-variants/staging-copyin-probe/UnrealInitializer.cpp`
  (sha `44b05cc6…`) — that is what the l25-A unit compiled; the row
  candidate stays the l26-tree file so the copyin-probe second-hop override
  lookup (`ui_row`) matches and G2q composes Q's patch on the CURRENT tree
  file.
- `include_directories`/`include_dirs`/`added_headers` repointed to the l26
  tree (l23-runtime/thread/include kept — still the consumed location).
- `source_selection` provenance pin → `l25-A/SOURCE-SELECTION.json`.
- `copyin-probe/MANIFEST.json`: base/candidate paths repointed into the l26
  tree; `base_sha256` deliberately KEPT at Q's recorded pre-listener
  `c4364dd7…` so the driver always takes the **patch-rebased** path
  (adopting the ready candidate would drop the listener edits and the token
  guards); `l26_repoint_note` added. Fixed one hash transcription typo I
  introduced in the bridge patch pin (6→8 at pos 57; caught by G2q).

**New rows that landed DURING this prep (mid-flight, verified):**
- CL logwriter (4): `FileDevice.hpp`, `Output.cpp`, `PosixFile.cpp`,
  `LuaLibrary.cpp` — arrived keyed on l26-candidate identities; re-keyed on
  their true port-root consumed originals (unit-source/dep-universe
  verified). CL∩CN file overlap: empty.
- CN hardening (6): `LuaUObject.cpp` (arrived keyed on
  `2026-10-08-menu-complete-build/...` — NOT consumed by l25-A; re-keyed on
  `l22-runtime/diagnostics/src/LuaUObject.cpp`, the lua-luauobject unit
  source), `LuaUObject.hpp` (port-root original is only an overlay NAME in
  the l25-A graph; re-keyed on the consumed
  `l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp` so
  `rewritten_inherited` maps the port-root spelling to the l26 candidate —
  same mapping as CN's `/tmp/l26-harden-smoke/headers-l26.json`), and
  `UObject.cpp` / `NameTypes.cpp` / `NameTypes.hpp` / `UObjectGlobals.cpp`
  (correctly keyed on port-root/l22 identities by the agents themselves;
  pins refreshed from disk — UObject.cpp drifted mid-landing f75799→ee5607
  and is now pinned at CN's final sha).
- Final tree state at certification: **30 typescan rows + 4 profile rows,
  zero pin drift**. Agents were landing concurrently; any further landing
  must refresh its row hashes or the pre-execute `--plan`/`--execute` will
  (correctly) hard-fail.

## 4. Plan status vs current tree (final run, 30+4 rows)

`build_variants.py --plan` — **exit 0, 63 gates, 718 steps**;
176 affected / 198 retained units, 34 change rows, 4 added headers.
Gate table: G1 ✓ (BASE pins); G2 ✓ ×34 (all row pins); G2r ✓ (all rows
classify consumed: 17 unit-source, 12 dep-universe, 1 lineage-data);
G2s ✓; G2t ✓ (4 listener units affected); G2q ✓ ×5 (patch/candidates; the
only `ok:false` in the whole plan is `G2q-...-unrealinitializer.base`, the
by-design informational base expectation — same as l25's final plan; the
rebase path is taken and composes cleanly with the order gate
G2q-order-def-before-use ✓); G2q-second-hop-override ✓ (composed overlay
wired for unreal-unrealinitializer); G4 ✓ token in exactly 2 files
(composed staging + LuaMod.cpp); G5 define_units =
`['luamod','unreal-unrealinitializer']`, conservative off; G12 ✓ ×4
(variant×pass); G12b ✓ (0 gaps); G6/G7/G9/G10 PLANNED (execute-time).
Plan artifact: `outputs/2026-10-08-l26-build-variants/plans/PLAN.json`.

`l26_packaging_driver.py --plan` — **exit 0, 57 gates**; 8 frozen l24
source pins ✓; l26 profile candidates ✓; 48 `P-await-runtime-artifacts`
(correctly PENDING until the l26 builds exist); manifest derivation ran for
both variants incl. lineage overlays, copyin-probe bridge assertion, and
the full l24 pin remap (no unmapped pins).
Artifact: `outputs/2026-10-08-l26-build-variants/plans/PACKAGING-PLAN.json`.

## 5. E3-class routing (G12/G12b) — verified, including firing proof

Positive (from PLAN.json `header_routing_proof` / G12b):
- `LuaUObject.hpp`: spelling `LuaType/LuaUObject.hpp` → l26 candidate
  `cbcb61cc…` via `-I l22-runtime/diagnostics/include + vfs-overlay
  redirect`; bare spelling direct via `-I candidate/include/LuaType`.
  G12b: 33 consumers, 0 retained.
- `NameTypes.hpp`: `Unreal/NameTypes.hpp` → l26 candidate `3723dbe3…` via
  port-root `-I` + overlay redirect; G12b: **168 consumers**, 0 retained.
- `UObjectArray.hpp`: both spellings → l26 candidate `69a2391a…` (port-root
  spelling via redirect); G12b: 162 consumers, 0 retained.
- `FileDevice.hpp`: 16 consumers, 0 gaps.

Negative (in-memory tampering, no writes; proves the gates FIRE):
- removing the l22-diagnostics overlay redirect → G12 produces 2
  LuaUObject.hpp violations with winners = the stale l22 identity;
- removing the port-root NameTypes.hpp alias → 2 violations, winner = the
  stale port-root original;
- dropping `lua-luauobject` from the affected set → G12b gap fires.

## 6. Final build command sequence (same 4-command shape; from `what/`)

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what

# 0. Preconditions: CL/CN agents confirm landing + refresh their CHANGES row
#    hashes; no agent mid-edit (single owner per file at build time).

# 1. Validate wiring + pins on the final tree (no toolchain):
python3 outputs/2026-10-08-l26-runtime/build_variants.py --plan
#    EXPECT exit 0; all gates true except G2q-...-unrealinitializer.base
#    (informational by design). If G2q reports patch-failed-rebase-needed,
#    the copyin-probe patch must be rebased onto the new tree base first.

# 2. Twin runtime builds (A then B; pass1/pass2 twin each):
python3 outputs/2026-10-08-l26-runtime/build_variants.py --execute
#    EXPECT G1..G12b true; artifacts under outputs/2026-10-08-l26-build-variants/
#    (l26-A/, l26-B/, staging-copyin-probe/, VARIANT-PAIR-RECEIPT.json).
#    Never rerun in place — move failed trees aside (dirs refuse overwrite).

# 3. Packaging dry-run on the built variants:
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --plan
#    EXPECT exit 0; no P-await-runtime-artifacts gates remain.

# 4. Packaging passes (2 per variant) + acceptance + final receipts:
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --execute
#    EXPECT P0..P11 true; 20/20 static checks, 238/2 constructor arrays,
#    exactly 207 import identities per pass, identical PRX twins per
#    variant, A/B PRXes differ, equal 207-name import sets.
#    FINAL-RECEIPT-VARIANT-{A,B}.json in outputs/2026-10-08-l26-package-{A,B}/
#    + VARIANT-PAIR-PACKAGING-RECEIPT.json in l26-build-variants/.
```

l26-specific gates beyond l25's set: none added — the same gate inventory
runs (G1–G12b + P0–P11); what changed is the BASE (l25-A variant dir, G7
baselines now llvm-nm equality with the l25-A undefined sets) and the
row-sync law in §3. Variant B remains UNSAFE diagnostic-only (stale object
pointers not cleared); never promote/upload B as the long-lived runtime.

## 7. Honest notes

- `--execute` was NOT run (per instruction); execute-time gates
  (G3/G6/G7/G8/G9/G10/G11, packaging P3–P11) remain needs-full-run, same as
  the l25 pre-clearance posture.
- The token spelling stays `MODBRIDGE_L25_SKIP_DELETE_LISTENER` (guards in
  the tree use it); variant-B receipts label it clearly.
- build_variants `--plan` prints a warning if CHANGES pins drift (parallel
  agents mid-flight) — a green plan is wiring proof, not final pin health;
  the pre-execute plan re-run is the authoritative pin check.
- The l25-B variant and its package are never consumed by l26 (production
  line is A only).
