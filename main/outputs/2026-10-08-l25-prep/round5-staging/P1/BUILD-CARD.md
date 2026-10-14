# P1 BUILD CARD — fault-ledger desaturation, one labelled gated build at AC

Owner: agent IG (ZCode), 2026-10-10/11. Scope of THIS card: the Mac-side P1
build in the AC queue (P1 → P2 → world-fix → round-5; Claude round-8 item 7:
each fix gets its own labelled gated build). Console contact / launch / upload
approval are SEPARATE gates owned by the AC operator, not this card.

Label suggestion: `p1-fault-ledger-desaturation` (build id carries the
timestamp via the FG stamp: `BUILD_ID`/`BRIDGE_SNAPSHOT_ID`).

## Preconditions (all must be green before step 1)

- [ ] FX NEVER-APPLY-WITHOUT honored: the AC console session carries the
      ToString-bind context ([Seeds][L27] Phase 4c label, image
      0x63FA00/32B) so the `[ST]` signature is checkable.
- [ ] APPLY.md §0 base-sha preconditions verified (bridge `94f81a17…`,
      frozen sink `2018c9bd…`; driver `810efaae…` or clean dry-run).
- [ ] No other agent is mid-edit on `outputs/2026-10-08-l26-runtime/` (the
      round-4 frozen state; P2 is NOT applied — P2's patches rebase on P1).

## 1. APPLY (patch + row updates)

```bash
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
cd "$WHAT"
/usr/bin/patch -p1 --fuzz=0 --dry-run < outputs/2026-10-08-l25-prep/parked/P1-fault-ledger/01-fault-ledger-desaturation.patch
/usr/bin/patch -p1 --fuzz=0       < outputs/2026-10-08-l25-prep/parked/P1-fault-ledger/01-fault-ledger-desaturation.patch
python3 outputs/2026-10-08-l25-prep/round5-staging/P1/p1_ac_row_updates.py   # prints receipt; idempotent
```

## 2. VERIFY THE APPLIED STATE (30 s)

```bash
shasum -a 256 \
  outputs/2026-10-08-l26-runtime/copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp \
  outputs/2026-10-08-l26-runtime/copyin-probe/candidate/CopyinSink.hpp
#   a35c7f9111b1f91cead80a05d489db04f6d4a25bb32b6bf0bee09879785baca1  (bridge)
#   0b113b1c19b4eac7f9959567487845c8639a28937f94a4881103dcbb163e69cd  (sink, NEW)
```
Static self-test evidence: the build itself IS the test — the header's
`static_assert(self_test_mask() == 0xFu)` compiles in every TU (host-verified
PASS in staging). Optional 5 s host probe: APPLY.md §3.

## 3. SYNC + SNAPSHOT (id must be recorded in the receipt)

```bash
SNAP=$(outputs/2026-10-08-l25-prep/harvest/snapshot_tree.sh | awk '/^SNAPSHOT-ID /{print $2}')
echo "P1 snapshot id: $SNAP"   # RECORD THIS in the AC receipt (frozen-tree provenance)
```

## 4. BUILD (the one P1 execute; per FY: ~concurrent twins, L26_BUILD_JOBS default)

```bash
BRIDGE_SNAPSHOT_ID="$SNAP" python3 outputs/2026-10-08-l26-runtime/build_variants.py --execute
```
Expected: EXECUTION-RECEIPT gates all green (G1…G10; the
`G2q-…-runtime_start_bridge.candidate` pin now reads a35c7f91 and MUST be
green — pin+file moved together). NOTE: both drivers refuse to overwrite
prior outputs — archive/move ALL FOUR first:
`outputs/2026-10-08-l26-build-variants/l26-A`, `l26-B` (G3-variant-dir-fresh)
and `outputs/2026-10-08-l26-package-A`, `l26-package-B` (P3-pass-dir-fresh +
the P1-package-dir-reuse manifest equality check would fail against the old
bridge pin). The frozen efbfc656 receipts stay immutable under their archived
names — never edit, just move.

## 5. PACKAGE

```bash
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --prepare
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --execute
```
Expected: P0–P13 green, 20/20 static checks, 207 imports, P9 twins identical,
P10 A/B differ, sidecar pairing P12/P13. P1-SPECIFIC receipt checks:
- `<pkg>/BUILD-INPUTS.json` → `lineage_overlays` contains destination
  `CopyinSink.hpp` sha `0b113b1c…` AND `fault_ledger_overlay` section present
  (candidate 0b113b1c replaces frozen 2018c9bd).
- `<pkg>/PREPARATION-RECEIPT.json` → `copyin_probe_wiring.wired_outer_bridge`
  sha `a35c7f91…`.
- `<pkg>/pass-1/STAGED-SOURCE-PINS.json` → `lineage/CopyinSink.hpp` pinned to
  `0b113b1c…` (the controller staged the candidate, proof the shadow worked).

## 6. VERIFY — composition markers (Mac-side, pre-upload)

```bash
python3 outputs/2026-10-08-l25-prep/harvest/grep_markers.py \
  outputs/2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx \
  --json --marker copyin_fault_ledger_st --marker copyin_fault_ledger_st_pass \
  --marker copyin_fault_ledger_reject_invalid --marker copyin_fault_ledger_full
```
EXPECT all four present (UTF-8). `copyin_fault_ledger_st` == 0 both encodings
→ the desaturated header did NOT reach the PRX → STOP (see
EXPECTED-MARKERS.md §1). If root registers the tag in
`harvest/markers.json` as critical (recommended), the registry count goes
54 → 55 — note "markers count changed" in the AC receipt.

## 7. P1 receipt checklist (what the AC record must contain)

- [ ] patch applied clean (`--fuzz=0`, 4 files, zero rejects; note any offset)
- [ ] shas of step 2 + `p1_ac_row_update_receipt.json` (old/new pin values)
- [ ] snapshot id `$SNAP`; build EXECUTION-RECEIPT driver_sha256 == disk
- [ ] packaging BUILD-INPUTS `fault_ledger_overlay` + 6th overlay row;
      STAGED-SOURCE-PINS sink pin `0b113b1c…`
- [ ] grep_markers result (four tags present, counts)
- [ ] PRX sha256 + BUILD_ID (new baseline for the queue; do NOT reuse
      prx_baseline_5ea2 — bind_launch refuses byte-identical relaunch anyway)
- [ ] console-side `[ST]` pair observed in the planned session
      (`copyin_fault_ledger_st=15`, `_st_pass=1`) — REQUIRED before P1 is
      called proven (FX NOTES §3; this is the AC session's job, not the build's)

## Rollback

Single patch → single revert (`patch -R -p1`), revert the two pipeline-pins
values (receipt records old values), delete the NEW CopyinSink.hpp candidate.
No other state changes. Never touch historical receipts.
