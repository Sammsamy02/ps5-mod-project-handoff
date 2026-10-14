# MATRIX NOTE — §4-d DIFF-2 base skew is CLOSED (DIFF 2 rev B)

From: worker-r3-diff2-rebase, 2026-10-04. For adoption by the LAUNCH11-MATRIX owner;
this note does not edit `LAUNCH11-MATRIX.md` in place.

## What is closed

LAUNCH11-MATRIX §4-d measured a BASE SKEW: DIFF 2 (`r3-initializer-r3-marshal-mode.diff`,
worker-r3-diffs-authoring, sha256 `d556c4e7…`) is authored on the query-s2-arm staging
initializer `933d08a2…` while the live merge lineage is `c85c42f7…`
(worker-initializer-integration/staging), making the rebase MANDATORY before any L11
R3-ARMED build (L11-5/L11-8, §3.5 step 2).

**The skew is closed for the c85c42f7 lineage.** Replacement deliverable:

- `outputs/2026-10-04-zcode-team/worker-r3-diff2-rebase/diffs/r3-initializer-r3-marshal-mode.revB.diff`
  sha256 `68d0c0064de69284…` (full hash in `receipts/diff-hashes.json`) — DIFF 2 **rev B**,
  6 hunks, all ported via an anchored replayer (5 anchors, each verified
  occurrence-count == 1 on the pinned `c85c42f7…` bytes; drift aborts, never hand-patch).
- Base pin: `c85c42f7aaab0630…` (attested by
  worker-initializer-integration/receipts/replace-receipt.json). rev A is superseded,
  preserved unchanged, never re-applied.

## Gate evidence (exact composition initializer argv)

Under the notify-cancel build-1 receipt argv (186 args) + the 4 prepended `-I` args the
composition script inserts (`build_runtime_composition4_fixed.py:94`), prospero-clang++
-std=c++23 -O0 (`receipts/compile-gate.json`, `fold0-object-parity.json`):

| variant | result |
|---|---|
| control (unpatched `c85c42f7…`) | exit 0, object reproduces the attested `fe639a85…` BYTE-EXACTLY |
| **fold 0** (rev B, no define) | exit 0, object **sha256-identical to the unpatched `c85c42f7…` object** (byte-faithful, provable — stronger than rev A's kind-parity) |
| **fold 1** (`-DUE4SS_R3_MARSHAL_MODE=1`) | exit 0, distinct object `7f058ed8…`; carries the deferral/refusal wide strings and the three `R3Marshal*` bridge symbols |
| `-Werror` parity | fold 0 and fold 1 add ZERO new diagnostic kinds and lose none vs the pinned baseline (the baseline itself does not pass `-Werror`; original worker's honest kind-parity standard) |

`patch -p1 --dry-run` clean; real `patch -p1` output sha256-identical to the anchored
replayer output (`receipts/patch-dryrun.txt`).

## Rev-B design delta the rows must know (two contract changes)

1. **Preprocessor fold guard**: every rev-B addition is guarded on
   `UE4SS_R3_MARSHAL_MODE` (same in-file fallback 0). Consequence (recorded, intended):
   a DISARMED initializer no longer defines the `R3Marshal*` bridge symbols, so a LuaMod
   TU carrying DIFF 1's `__PROSPERO__` references must be built in the MATCHING fold —
   mismatched folds now fail the LINK loudly. This is ARM-DISARM §2.3's pairing law,
   now linker-enforced; the L11 rows already arm both defines together (§3.5 step 3).
   Armed-fold semantics are rev A's exactly (same constexpr gate, compile-outs, literal
   deferral line, `pump_pending` seam, bridges).
2. **Merge-lineage coexistence** (the §4-d drift reconciliation): the seed consumption +
   Shape-A fold compose with the compile-outs — the seed path skips the detour verifier
   when `SeedArming::bSeedBoundFNameCtor` is set (and settles the same
   `FNameVerificationStatus` latch rev B settles); the EngineTick registration block is
   byte-identical across both lineages (seed work changed only the resolve branch above
   it). Verified on the pinned staging text; armed-seed, Shape-A-refused, and R3-mode
   launches all compose.

## Unchanged obligations (not closed here, restated so they are not lost)

- **§4-e site ownership at merge time**: rev B is anchored on the RAW `c85c42f7…`
  staging bytes. In L11-5's gate-first flow (`apply_gate.py`, 28 anchors, THEN DIFF 2,
  §3.5 step 2), the gate rewrites the two shared sites (FName verifier, EngineTick
  registrations), so rev B's two site anchors WILL drift — that is the expected detector,
  and the gated-bytes variant is a composition-time re-anchor (new ids), never a
  hand-patch. Ownership stays as §4-e decided: in the rebased unit DIFF 2 owns the two
  sites (deferral + bridges); the gate owns every other E-site.
- Fold-0 object parity is per-TU evidence only: no link, no composition run, no boot.
  F-1 remains open; nothing here claims any in-game marshal behavior.
