# REPORT — worker-fresh-r11-gate-receipt

Date: 2026-10-04 · Scope: read-only + receipts in own dir · No console contact, no nested subagents.

## Task

Close the worker-gate-evolution side-finding (stale archived gate-receipt-composition5-r5.json vs the rebuilt prx-candidate dir) by producing a fresh, current gate verdict pair for the LIVE r11 staged build and reconciling it against the cycle receipt.

## What was done

1. Confirmed `prx-candidate-composition5-2` still exists → ran BOTH gates two-dir (real D1 compare; no `--baseline-bytes` fallback needed).
2. Ran live gate `worker-composition5-gate/gate_check_composition5.py` → `live-r11-receipt.json`, exit 0.
3. Ran v2 gate `worker-gate-evolution/gate_check_composition5_v2.py` with NO `--manifest` (parity mode) → `v2-r11-receipt.json`, exit 0.
4. Independently sha256-hashed the fself and bare artifacts in both candidate dirs; reconciled against `worker-zcode-controller/console/cycle-receipt.json` (staged `0911af0c…`) and characterized the stale r5 drift.

## Result

- **Both verdicts GO (offline gate)**, `failed_checks: []`, identical 8/8 PASS (W1 W2 W3 I1 D1 F1 S1 P1). Receipts agree on every check; v2 adds only `gate_variant:"v2"`, `manifest:null`, `baseline:null`.
- **Staged identity MATCH**: cycle receipt `staged_sha256 0911af0cd5b4a1f22eac49a79f8b3c9a3d9cac7358b246e16ae2a969b537cab9` == top-level fself in BOTH candidate dirs (30,792,151 B, byte-identical to each other). Gate bare (bare-original) = `7774b663…dcc7`, 30,774,360 B, identical across candidates; cycle receipt's `bare_sha256 a0df5d9b…70aa` == the bare-sdk variant, also present and matching. Every recorded sha resolves to a current file.
- **LOUD NOTE**: cycle receipt `readback_len 30,772,344` matches no current file size; it is a partial console readback window (below its own `e_shoff 30,772,632`), not a full-file length. No sha disagreement anywhere.
- **Stale r5 confirmed stale**: records bare `3695d100…82d` @ 30,772,008 B — matches nothing on disk; describes pre-rebuild state. Superseded by this pair.

## Deliverables (this dir)

- `CLAIM.json` (written first)
- `live-r11-receipt.json`, `v2-r11-receipt.json`
- `RECONCILIATION.md`
- `REPORT.md` (last)

## Recommendation

Archive `worker-composition5-gate/gate-receipt-composition5-r5.json` with a pointer note to this fresh pair (see RECONCILIATION.md); keep the raw file unedited. Owner of worker-composition5-gate should similarly verify r2/r3/r4 receipts' shas against disk before deciding their fate.

## Honest limits

Offline gate only: no new on-console runtime execution, no new visual/audio acceptance. The console-side readback_len question is reported factually, not explained beyond what the bytes show.
