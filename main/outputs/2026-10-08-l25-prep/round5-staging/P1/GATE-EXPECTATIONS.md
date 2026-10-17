# P1 — gate expectations at AC (which gates flip, which must not)

Owner: agent IG (ZCode), 2026-10-10/11. Evidence: paired plan runs on
byte-identical /tmp stages — baseline (unpatched) vs patched
(`/tmp/p1-baseline-IG`, `/tmp/p1-stage-IG`) — same layout, same path
normalization, run 2026-10-11.

## 1. Headline: NO gate flips state

`build_variants.py --plan`: 74 recorded gates in BOTH runs; **zero** ok-state
or name diffs. (The patched stage was planned TWICE — once during staging,
once with canonicalized paths — identical results both times.)
`l26_packaging_driver.py --plan`: 37 gates in BOTH runs; zero
diffs of any kind in the gate list. P1 is plan-invisible except for the
intended detail updates below. (The one `ok:false` gate —
`G2q-copyin-probe-pin-unrealinitializer.base` — is BY DESIGN identical in both
runs: base pins are expectations, not invariants; the typescan base
legitimately carries the armored 5f7ae2bc state and the composition takes the
patch-rebased path. It is NOT P1-caused and NOT execute-fatal.)

## 2. What DOES change in plan output (all detail-level, all intended)

| where | baseline | patched | note |
|---|---|---|---|
| G2q-copyin-probe-pin-runtime_start_bridge.candidate (ok both) | pin 94f81a17 == disk | pin a35c7f91 == disk | the gate's `detail` shows only the path; the sha equality is what stays green because hunk 3 updates pin+file together |
| G2q-order-def-before-use detail | monitored statics at def lines 915-918 | same statics at 920-923 (+5) | hunk 2a inserts the two one-shot guard statics above them; definitions still precede uses; ok=true both |
| PLAN.json `copyin_probe.runtime_start_bridge.ready_candidate` | 94f81a17… | a35c7f91… | receipt sha refresh |
| PLAN.json `driver_sha256` (build_variants) | unchanged | unchanged | build_variants.py is NOT touched by P1 |
| PLAN.json per-variant `write-header-overlay.planned_sha256` | 10acfcfb… | 9200b8e5… | STAGING ARTIFACT ONLY, not P1: the vfs overlay dumps alias VALUES (candidate paths) verbatim, and the two /tmp stages rewrite those strings with different prefixes. Semantically identical mappings (header_aliases 51 == 51; P1 adds no alias — neither the bridge .cpp nor anything under copyin-probe feeds `merged_aliases`). At AC (live paths both sides) the overlay hash is EXPECTED UNCHANGED by P1; if it moves at AC, STOP and reconcile |
| Packaging derived manifest `outer_bridge` / `copyin_probe_wiring.wired_outer_bridge` | 94f81a17 | a35c7f91 | proven by derive_manifest instrumentation on the staged tree |
| Packaging derived manifest `lineage_overlays` | 5 destinations | 6 (+`CopyinSink.hpp` 0b113b1c, 5054 B) | the EQ-pattern shadow that makes the patch real |
| Packaging derived manifest `lineage_source_pins` CopyinSink.hpp row | 2018c9bd (frozen) | 0b113b1c (candidate) | repointed by hunk 4 |
| Packaging derived manifest `fault_ledger_overlay` section | absent | present (candidate 0b113b1c replaces frozen 2018c9bd) | FX NOTES §2 wiring — the AC plan-confirm item |

## 3. Execute-time expectations (the P1 build at AC)

- `G2q-copyin-probe-pin-*` for the bridge patch + candidate: stay green
  (patch file untouched, pin+file updated together).
- `G-stamp-present`: green as usual once `BRIDGE_SNAPSHOT_ID` is exported for
  `--execute` (snapshot_tree.sh → build_variants --execute per its header).
- Packaging P0–P13: UNCHANGED expectations — 20/20 static checks, 207 exact
  import identities (P1 has import_delta none), twins identical (P9), A/B
  PRX differ (P10), symbols sidecar pairing (P12/P13).
  NOTE pre-existing quirk (NOT P1): the live driver's P8 gate checks
  `.init_array == 239` (Claude Q4d rebaseline; the frozen efbfc656 build's
  STATIC-CHECKS show init_slots 239) while its PLAN text and two FINAL-RECEIPT
  literals still say 238 — cosmetic doc drift inside the driver, out of P1
  scope, flagging so nobody attributes it to this patch.
- The bridge TU compiles the desaturated header: if the `copyinsink` MANIFEST
  row were missing, hunk 4's wiring raises
  `RuntimeError('copyin-probe MANIFEST missing the copyinsink base_files entry …')`
  — a HARD STOP by design (never silently stage the frozen saturating sink).
  Same for frozen-base pin drift (2018c9bd) or candidate pin drift (0b113b1c).

## 4. Launch-preflight gates (bind_launch.sh --all, step 4) — REQUIRE the row updates

These are the gates that DO flip red at AC if root skips
`p1_ac_row_updates.py` (they check disk shas against pipeline-pins.json):
- `outer_bridge pin is … / disk …` — expects bridge_gated; red unless pin →
  a35c7f91 (checks MANIFEST row AND disk file).
- `l26_packaging_driver.py is …, pipeline-pins.json pins …` — red unless
  pkgdrv_all → post-P1 sha. (ALREADY red today pre-P1: pin ee3aa230 vs live
  810efaae — stale since the 2026-10-11 P8 rebaseline; P1's AC is the moment
  to fix it honestly.)
- `driver_all` (build_variants.py): unaffected — stays in
  recompute-at-preflight mode; P1 does not touch that file.

## 5. Console-side gate (the reason P1 is AC-gated at all)

The planned session must observe the one-shot early pair
`copyin_fault_ledger_st=15` / `copyin_fault_ledger_st_pass=1` in
supplier-diag.jsonl (see EXPECTED-MARKERS.md §2). Anything else = the
desaturated header did not reach the PRX = stop (FX NOTES §3.1).
