# agent-HH packer report — 2026-10-10

Both packer-side items done, Mac-only (python edits + local tests; no
--execute, no builds, no console). Prior attempt (interrupted by a
provider-side API error) had only edited the driver docstring; the emit
code, gates, tool, tests, real run and runbook steps are all new this
session.

## A. Symbols sidecar emission — l26_packaging_driver.py (additive)

`outputs/2026-10-08-l26-runtime/l26_packaging_driver.py` (+133 lines, 388→521;
importable, never executed in --execute mode here):

- `emit_symbols_sidecar(variant,record,steps)` runs inside `finalize_variant`
  at final-receipt time, per variant, and writes
  `<pkg>/<prx-name>.symbols.json` beside the packaged PRX.
- Consumer contract learned from the frozen loader
  `phasea-tools/symbolize_crash.py::load_prx_symbols_sidecar`:
  `prx_sha256` (64-hex, the pairing key, hard-refuse on mismatch/absence) +
  `symbols=[{name,addr,size}]` (hex-str or int; extra keys tolerated).
- Map-reconstruction logic is REUSED from that tool via import
  (`parse_map`/`reconstruct_from_map`/`elf_sections`/`elf_dynsym`) — not
  rewritten. Tier honesty: `symbols[]` carries only exact rows (`.dynsym`
  anchors, DYNSYM-EXACT); the `-r` runtime.map reconstruction (VMA=0,
  order-assumed) is embedded under a separate `map_reconstructed[]` block
  (MAP-RECONSTRUCTED) so the consumer never labels approximate rows
  SIDECAR-EXACT; a future final-link map (MAP-EXACT) auto-promotes.
- Sidecar also carries `build_id` (bare PRX `.note.gnu.build-id`),
  `prx_bytes`, bare-PRX pin, `sources` incl. the game-side anchors pointer
  (`game-symbols-townfall.json`, pairs with the GAME image, not the PRX).
- New gates, fail-the-build like all others: **P12** sidecar present + valid
  JSON + required keys + `prx_sha256` == packaged pass-1 PRX; **P13**
  `prx_sha256` == BOTH packaging twins. `--plan` gate list updated;
  FINAL-RECEIPT gains `symbols_sidecar` pin. Docstring item 6 (from the
  interrupted attempt) now matches the implementation.
- Real-artifact probe (read-only, existing package-A): bare ELF 31,387,880 B,
  build_id `885267a3…`, 2 dynsym anchors (DT_INIT 0x15a0 / DT_FINI 0x1e00),
  map → 207,852 sections → 87,264 MAP-RECONSTRUCTED rows (~1 s parse);
  packaged PRX sha `56a9b1a0…` (= the round-3 fallback PRX in runbook §12 —
  consistent). No sidecar was written into archived package dirs.

## B. Provenance whitelist — harvest/pack_layout_tables.py (new)

`outputs/2026-10-08-l25-prep/harvest/pack_layout_tables.py`: reads the
authoring table read-only, neutralizes every known game-name occurrence
(values AND keys, any depth; longest-first so `Townfall-PS5-Shipping.self`
goes whole) into role tokens (MEASURED-TITLE / BLIND-TEST-TITLE /
NEGATIVE-CONTROL-TITLE), writes `blade-prep/shippable/`:
full shippable table + `.shippable.cells.json` (per-version cells) +
`provenance-diff.txt` + `PACK-LAYOUT-TABLES-RECEIPT.json`. Fail-closed:
outputs re-scanned case-insensitively; any survivor => FAILED + exit 1.
Receipt records every replacement (JSON path + counts), pins, retained-item
review notes (measurement sha256s kept — provenance, not names; unknown-name
limitation documented).

Real-table run: **PASS**, exit 0. 9 occurrences in 7 fields neutralized
(Stellar Blade 1 — the GI finding, in `generated_by`; Townfall 6;
`Townfall-PS5-Shipping.self` 1; Astro Bot 1). Before/after diff written to
`blade-prep/shippable/per-version-table.provenance-diff.txt`; the key hunk:

```diff
-  "generated_by": "agent-GJ 2026-10-08 (Claude round-5 addendum A3a: per-version layout tables UE4-5.6 + generator, Stellar Blade blind-test prep)",
+  "generated_by": "agent-GJ 2026-10-08 (Claude round-5 addendum A3a: per-version layout tables UE4-5.6 + generator, BLIND-TEST-TITLE blind-test prep)",
```

Independent grep (outside the tool) of both shippable outputs:
`townfall|astro bot|stellar blade|silent hill` = **zero hits**; all outputs
valid JSON; authoring original byte-identical.

## Tests (fabricated shapes; direct calls; never --execute)

`outputs/2026-10-08-l25-prep/agent-HH-packer-tests/`:

- `test_sidecar_emit.py` — **20/20 PASS**
  (`SIDECAR-EMIT-TEST-RECEIPT.json`). Fabricates a package-receipt shape
  (synthetic ET_EXEC ELF with .dynsym/DT_INIT anchors/build-id note,
  fabricated `-r` map in the real column format, fake faked-signed PRX
  twins), imports the driver, calls `emit_symbols_sidecar` directly, then
  round-trips through symbolize_crash's loader: accept (pairing=verified,
  2 anchors), refuse on wrong sidecar sha, refuse on tampered PRX, refuse
  on missing pairing key; driver `validate_symbols_sidecar` rejects invalid
  JSON / mismatch / absent file.
- `test_pack_layout_tables.py` — **17/17 PASS**
  (`PACK-LAYOUT-WHITELIST-TEST-RECEIPT.json`): neutralization exactness,
  keys+values, non-name data preserved, original untouched, cells file,
  diff carries before/after, counts, gate, unknown-name limitation.

## Runbook

`WAKE-RUNBOOK-20261010.md` §12: appended **Step 6a** (sidecar in every
package pass, P12/P13, post-package Mac-only check) and **Step 6b**
(provenance whitelist before any ship of the layout table; ship only
`shippable/`), placed after step 6 / before the standing laws.

## Syntax

`py_compile` clean on all 4 touched python files. Cross-check note: driver
`--plan`/`--execute` were NOT run (no writes into build-variants receipts);
first real emission happens at the next build window's `--execute`.
