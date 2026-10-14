# P1 (fault-ledger desaturation) — EXACT APPLY PROCEDURE for the AC window

Owner: agent IG (ZCode), 2026-10-10/11. Staged from FX's parked patch set
`outputs/2026-10-08-l25-prep/parked/P1-fault-ledger/{01-fault-ledger-desaturation.patch,NOTES.md}`.
Everything below was executed and verified against a `/tmp` staging copy
(`/tmp/p1-stage-IG`) — see `agent-IG-p1staging.md` for the evidence. The live
tree was NOT touched by staging.

## 0. Preconditions (verify BEFORE applying — hard abort on any mismatch)

```bash
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
cd "$WHAT"

# P0a. the four base files must still hash to the states FX's patch + this
#      staging were verified against (2026-10-11 ~03:00 UTC tree):
shasum -a 256 \
  outputs/2026-10-08-l26-runtime/copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp \
  outputs/2026-10-08-l26-runtime/copyin-probe/MANIFEST.json \
  outputs/2026-10-08-l26-runtime/l26_packaging_driver.py \
  outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2/CopyinSink.hpp
# EXPECT (bridge + frozen sink base are HARD pins; the other two are recorded states):
#   94f81a17d6904b9edfe8b06e8e4284268fbd4c738f96ac4475931225e719274c  .../runtime_start_bridge.cpp
#   f310a7a37952e50af13cfee48f59c64f9f09db80726e345a546db81cd0222882  .../copyin-probe/MANIFEST.json
#   810efaae1fa3067c78ed6c3ad8351a9520dc0065d153f57c5cead0bafdab22d4  .../l26_packaging_driver.py
#   2018c9bdc63b8f2001aeadcc44ab698d0d5c54c08652e5fc0c78f45c0fa65d98  .../lineage-overlay-2/CopyinSink.hpp
```

Drift handling:
- bridge != `94f81a17…` → STOP. Someone re-landed the bridge candidate; FX's
  hunk 2 must be rebased by the new owner. Do not hand-merge.
- `CopyinSink.hpp` != `2018c9bd…` → STOP (frozen lineage moved; hunk-4's base
  pin check would also refuse — that is the designed behavior).
- `l26_packaging_driver.py` != `810efaae…` → allowed ONLY if the hunk-4 context
  still applies clean (step 2 is the test; it applied at +14 lines offset in
  staging). Re-run the dry-run; on any reject, rebase hunk 4 the EQ way.
- `MANIFEST.json` != `f310a7a3…` → usually a harmless added note key; the
  dry-run decides (hunk 3 context is the `runtime_start_bridge` row + the
  `eq_reanchor_note` tail).

## 1. Read the NEVER-APPLY-WITHOUT flag (FX NOTES, top)

P1 changes admission-ADJACENT behavior (ledger input to the
`bridge_copyin_range_faulted` admission gate). Apply ONLY inside the planned
AC console session carrying the ToString-bind context ([Seeds][L27] Phase 4c
label, image 0x63FA00/32B) so the `[ST]` validation signature is observable.
Never apply "cold" into a launch plan that cannot check it.

## 2. Apply (one command, from the worktree root)

```bash
cd "$WHAT"
/usr/bin/patch -p1 --fuzz=0 --dry-run \
  < outputs/2026-10-08-l25-prep/parked/P1-fault-ledger/01-fault-ledger-desaturation.patch
# expect: 4 x "patching file ..." and rc=0  — then apply for real:
/usr/bin/patch -p1 --fuzz=0 \
  < outputs/2026-10-08-l25-prep/parked/P1-fault-ledger/01-fault-ledger-desaturation.patch
```

`--fuzz=0` makes any fuzz fatal (staging: clean, zero rejects, zero fuzz; the
driver hunk lands at line 170 with a +14-line offset vs the header — patch
keeps a `.orig` backup; harmless, delete it or leave it, never upload it).

## 3. Post-apply verification (all four must match FX's construction EXACTLY)

```bash
shasum -a 256 \
  outputs/2026-10-08-l26-runtime/copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp \
  outputs/2026-10-08-l26-runtime/copyin-probe/candidate/CopyinSink.hpp
# EXPECT (byte-match to FX's authoring-time verified construction):
#   a35c7f9111b1f91cead80a05d489db04f6d4a25bb32b6bf0bee09879785baca1  runtime_start_bridge.cpp
#   0b113b1c19b4eac7f9959567487845c8639a28937f94a4881103dcbb163e69cd  CopyinSink.hpp (NEW file)

python3 -m py_compile \
  outputs/2026-10-08-l26-runtime/l26_packaging_driver.py \
  outputs/2026-10-08-l26-runtime/build_variants.py        # both must compile

# host-side self-test of the new header (battery-safe: single TU, no project build):
printf '#include "CopyinSink.hpp"\nint main(){return l13_sink::FaultLedger::self_test_mask()==0xFu?0:1;}\n' \
  > /tmp/p1_sink_selftest.cpp
clang++ -std=c++20 -I outputs/2026-10-08-l26-runtime/copyin-probe/candidate \
  /tmp/p1_sink_selftest.cpp -o /tmp/p1_sink_selftest && /tmp/p1_sink_selftest
echo "rc=$?"   # MUST be 0 (mask == 0xF); a nonzero rc or a static_assert build
               # error = the desaturated semantics regressed — STOP.
```

## 4. JSON row updates the patch does NOT carry (run the staging script)

The patch updates the copyin-probe MANIFEST pins (hunk 3). Two OTHER pin
registries reference the patched files and MUST be re-baselined in the same AC
step or the launch leg (`harvest/bind_launch.sh --all` preflight gate 4) fails:

- `outputs/2026-10-08-l25-prep/harvest/pipeline-pins.json`
  - `pins.bridge_gated.sha256`: `94f81a17…` → `a35c7f91…` (checks BOTH the
    MANIFEST row and the on-disk bridge candidate)
  - `pins.pkgdrv_all.sha256`: → the post-P1 `l26_packaging_driver.py` sha
    (staged value `6d05f322…` if the live driver is still `810efaae…`).
    NOTE: this pin was ALREADY stale pre-P1 (pinned `ee3aa230…` on
    2026-10-10 03:16Z; the live driver moved to `810efaae…` at 2026-10-11
    02:28Z by the P8 238→239 rebaseline without a pins refresh) — P1's edit is
    the natural moment to re-baseline it honestly.

Run (idempotent; verifies construction shas first, refuses on drift):

```bash
python3 outputs/2026-10-08-l25-prep/round5-staging/P1/p1_ac_row_updates.py
```

Documentation-only follow-ups (NOT gate-relevant, do at wrap-up):
`gating-proposal/CHANGES.json` composition chain still ends at "94f81a17
CURRENT candidate" — append the a35c7f91 step; `NEXT-BUILD.md` §"94f81a17
flows through the existing outer_bridge pin path" — refresh the sha mention.
Historical receipts referencing 94f81a17 are immutable provenance: never edit.

## 5. Battery-safe pre-build sanity (plan mode ONLY — never --execute here)

```bash
cd "$WHAT"
python3 outputs/2026-10-08-l26-runtime/build_variants.py --plan      # RC 0 expected
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --plan # RC 0 expected
```

CAUTION: `build_variants.py --plan` DELETES a stale
`typescan-r4/candidate/include/GeneratedBuildStamp.hpp` (gate
G-stamp-stale-cleanup) and overwrites `2026-10-08-l26-build-variants/plans/`.
That is a live-tree write — fine at AC (root-owned), but it is the reason FX
never ran --plan on battery. The staged evidence for what these plans show on
the patched state is in `/tmp/p1-stage-IG` (see report §3); at AC confirm:
- `G2q-copyin-probe-pin-runtime_start_bridge.candidate` = true with sha `a35c7f91…`
- packaging PLAN.json / derived manifest carries `fault_ledger_overlay` and a
  6th `lineage_overlays` destination `CopyinSink.hpp` (`0b113b1c…`, 5054 B).

## 6. Rollback (single patch → single revert)

```bash
cd "$WHAT"
/usr/bin/patch -R -p1 < outputs/2026-10-08-l25-prep/parked/P1-fault-ledger/01-fault-ledger-desaturation.patch
# delete the re-created frozen CopyinSink.hpp candidate copy if reverting by hand:
#   outputs/2026-10-08-l26-runtime/copyin-probe/candidate/CopyinSink.hpp (the NEW file)
```
Then revert the two pipeline-pins.json values from step 4 (script prints the
old values in its receipt). No other state changes.
