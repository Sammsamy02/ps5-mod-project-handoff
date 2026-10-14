# REPORT — worker-launch11-integration-matrix, 2026-10-04

Read-only synthesis; no console contact, no nested subagents, nothing executed (one
sha256 readback across four staging copies, itself recorded as ledger evidence L11M-2d).
No design, staging, spike tree, or sibling-worker file was modified; all writes are the
three deliverables in this directory. Deliverable order honored: `CLAIM.json` first,
`LAUNCH11-MATRIX.md`, this `REPORT.md` last.

## 1. What was asked and delivered

| task | delivery |
|---|---|
| 1. Update the launch-10 matrix so launch 11 is ONE LOOKUP | `LAUNCH11-MATRIX.md` §0-§3: the successor-artifact maturity table (what landed vs what was DESIGN-ONLY at L10M time), the unchanged dimension law + two new consistency rules, and a 10-row master matrix keyed on launch-10's observed outcome (D1 × D2 × D3 × D4). Each row names the exact candidate base, which diffs arm, the rebuild/receipt steps (including the mandatory asmhelper prebuilt-object rebuild law §3.2 and the frozen-script arming decision §3.3/§4-g), the expected new terminal, and the one-pass command delta. |
| 2. Integration-decision ledger | `LAUNCH11-MATRIX.md` §4: ten ledger points (a-j) where two successor artifacts touch, each with an honest verdict + evidence — backend×guard compatible (disjoint units/page classes), R3-ARMED×backend incompatible (alternative pump routes), gate×guard-folds complementary, the MEASURED base skew (DIFF 2's `933d08a2…` base vs the `c85c42f7…` lineage) requiring a rebase, gate×DIFF-2 site ownership, the asmhelper stale-link law, the frozen-script arming blocker, PLH-layer composability, cc1's ini-law adoption, and the armed-table revert path. |
| 3. Pre-conditions checklist | `LAUNCH11-MATRIX.md` §5: ten ordered boxes (launch-10 shipped-candidate identity, expanded D1 log match, runtime-status, IGW seq law, fc1 pool facts, observer records, slot-3/D4 evidence, successor-artifact hash verification, design citations, standing ritual) plus §6, the new evidence-vocabulary table for box 2. |

## 2. Key findings of the synthesis

1. **The launch-11 compositions are concrete, not aspirational.** Unlike the launch-10
   matrix (where U-SWAP-2/4/5 had "no implemented code"), every row now starts from a
   staged, gate-passed candidate (L11PB disarmed vehicle, byte-identical to launch-10's
   shipped artifacts) and adds measured diffs (WST, NE, RDA) or compile-gated ones (PLH).
2. **Two measured mechanics facts dominate every row:** the asmhelper unit is never
   recompiled by the pass (PREBUILT `.o` = the link input → explicit rebuild + `--file`
   attestation or the stale object ships silently), and R1 arming is inexpressible
   through the frozen comp6 fixed script (cross-TU ODR on `unreal-hooks`) → a NEW
   fixed-script edition is a named, blocking integration decision for L11-4.
3. **One honest incompatibility found and recorded:** R3-mode ARMED and the armed vtable
   backend are alternative pump routes (DIFF 2 compiles out the registrations the backend
   targets). The matrix pins R3-ARMED to the BOTH_FAIL/FATAL rows only; DIFF 2 DISARMED
   (byte-inert default) rides safely everywhere.
4. **One measured base skew:** the r3 initializer diff is authored against
   `933d08a2…` (query-s2-arm staging) while the gate and seam build on `c85c42f7…`
   (merged staging; the L11PB vehicle is `39fe33f3…` = that + 2 include lines). Verified
   by direct sha256 readback this session. Any L11-5/L11-8 composition therefore includes
   a mandatory DIFF-2 rebase authored under new ids — anchored-replayer drift aborts are
   the detector; hand-patching forbidden.
5. **New rule for the dimension law:** with the armed table shipped (Shape-A fold
   measured gone from the shipped object), a bare `T-A` terminal is a staging anomaly →
   reconcile-first, not a row.

## 3. Honest limits

- No launch-10 outcome is known here; the matrix routes, it does not predict.
- PLH remains IN FLIGHT (CLAIM + compile-gate receipts landed; REPORT pending) — rows
  L11-2/L11-3 re-verify box 8 at integration time.
- D-1 (backend data-page transition), the DIFF-2 rebase, the new fixed-script edition,
  and the cc1 9-entry ini adoption do not exist yet; their rows say so and gate them on
  controller-owned decisions under new ids. Nothing is drop-in; "armed" is always a
  receipt-covered compile-time act.
- Link-stage and composed-boot verification of the RDA diffs remains open (their worker
  compiled `-c` only); the matrix carries that as the L11-5/L11-8 comp-pass obligation.
- Codex usage: zero (local reads only, no web requests, no console contact).

## 4. Deliverables (this dir)

- `CLAIM.json` — written first (L11M-1..4 + inputs + non-claims)
- `LAUNCH11-MATRIX.md` — the matrix (§0-§3), integration-decision ledger (§4),
  pre-conditions checklist (§5), evidence vocabulary (§6), honesty bounds (§7)
- `REPORT.md` — this closing report, written last
