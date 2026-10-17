# REPORT — worker-staging-pkg-c4 (2026-10-04)

Task: update the master staging package to the composition-4 PRX (replacing the
composition-3 placeholder), activate Tier B evidence expectations, update the ploader
entry if needed.

## Deliverable

`outputs/2026-10-04-zcode-team/worker-staging-pkg-c4/MASTER-STAGING-PACKAGE-V2.md`
— full surgical update of V1
(`outputs/2026-10-04-zcode-team/worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md`).
V1 left untouched (read-only per bounds).

## What changed vs V1

1. **File 1 (PRX)** — now
   `worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx`,
   **30778567 B**, sha256
   `de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f`,
   destination unchanged `/data/plugins/modbridge_supplier_composition.prx`.
2. **Tier B evidence expectations ACTIVE** — §E rewritten: with the composition-4
   ack-supplier unit staged, `runtime-status.txt`, `UE4SS.log`, and Lua mod lines are
   EXPECTED; their absence next to a healthy Tier A log is now a failure/wiring state
   to triage, not a placeholder limitation.
3. **ploader entry — UNCHANGED (verified, not guessed):** composition-4 is packaged
   under the SAME module filename `modbridge_supplier_composition.prx`, so the §C line
   `/data/plugins/modbridge_supplier_composition.prx = 2` (mode 2 LATE) needs no edit;
   only pre-image backup + per-launch re-send apply. Both PRX hashes recorded in §C
   (superseded placeholder `b8bfae77…` 30756791 B; active composition-4 `de372302…`
   30778567 B).
4. Consistency sweep: §B Step 2/3 commands and hash comments updated to the
   composition-4 path/sha; Gate A1 expectation unchanged (local FSELF magic
   `4f 15 3d 1d` → readback must be `7f 45 4c 46`); §F rollback notes the pre-image
   hash (not filename) decides restoration; §H documents the V2 on-disk verification.

## Verification performed on disk (this worker)

- sha256 recomputed via `shasum -a 256` → `de372302…09aa0f` (matches checkpoint
  `zcode_supplier_composition_4` prefix `de372302`).
- Size via `ls -l` → 30778567 B (matches checkpoint).
- First 4 bytes via `xxd` → `4f 15 3d 1d` (FSELF).
- `build-receipt.json` → `gaps == []`.
- `verification-evidence/lineage-verification.json` → 33 checks, ALL pass, 0 fail,
  including `ledger_checkpoint_present = zcode_supplier_composition_4`, composed
  object sha `45dd9901…` (11777955 B), frozen-base PRX `3b998f09…` (30151623 B).
- Checkpoint record cited: `zcode_supplier_composition_4`, 17/17 checks (task
  directive); the stronger on-disk 33/33 lineage tally corroborates it.

## Honest limits

- The delivered FSELF is encrypted; a plaintext symbol probe cannot independently
  re-confirm `MakeRuntimeAckConfig`/`StartActivatedRuntimeWithAck`. The ack-supplier
  claim rests on the checkpoint/lineage verification records (read and re-verified
  for hashes/sizes/tallies by this worker), not on a performed launch. This caveat is
  stated in V2 §A and §H.
- Staging has NOT been performed; Gate A0/A1 still gate execution. No runtime success
  is claimed.
- Loadlist (file 2) intentionally unchanged — the PRX is not loadlist-preloaded; it
  loads via the ploader route.

Calls used: well under the 20-call bound (9). No nested subagents, no network, writes
confined to `outputs/2026-10-04-zcode-team/worker-staging-pkg-c4/`.
