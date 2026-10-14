# REPORT — worker-launch10-matrix-prep (2026-10-04)

Mission: pre-build the launch-10 decision matrix as the cross product of launch-9 outcome
dimensions, so the post-launch-9 integration cycle is one lookup. Read-only synthesis.
Deliverable order honored: CLAIM.json first, LAUNCH10-MATRIX.md, this REPORT last.
~18 tool calls (inside the 25–45 bound). No console contact, no nested subagents, nothing
executed, no design or frozen file modified.

## 1. What was done

1. Read the six named inputs end to end: `worker-launch9-prediction`
   (LAUNCH9-PREDICTION.md + REPORT.md + CLAIM.json — terminal shapes T-A/T-REF/T-RD/T-WR/
   T-GM and hop order I6 < I7 < I11), `worker-exec-writability` (PROBE-DESIGN §5 record
   contract + §7 decision tree, EVIDENCE §6, REPORT, CLAIM EW-1..8 — verdict semantics),
   `worker-vtable-swap-design` (DESIGN §1.2 D-1 premise correction, §5 R1–R4 integration,
   §5.1 branch mapping, §6 limits), `worker-fc1-validator-probe` (STAGE-PLAN §6 chain +
   §7 exact record shapes for PASS/refused/refused_after_grace), the initializer's
   `SEED-TABLE-FORMAT.md` (arming semantics §4, compile-time folding §7, re-seed mechanics
   §8), plus both in-flight workers' CLAIM.json (r3-zerosyscall-tier R3-1..R3-5;
   gmalloc-derivation scope + the validator-before-seedable constraint) and
   `worker-comp6-driver/ONE-PASS-CHECKLIST.md` (the command skeleton; generator argv
   verified directly in `gen_seed_table.py`: `gen_seed_table.py <SEED-TABLE.json> <SeedTable.h>`).
2. Built `LAUNCH10-MATRIX.md`: four decision dimensions (D1 terminal × D2 IGW verdict ×
   D3 fc1 verdict × D4 GMalloc hunt), four consistency rules (including the two non-obvious
   ones: T-GM pairs only with W-works-class sessions; W_WORKS does NOT make the
   initializer's own un-transitioned walks safe — the per-page-transition integration delta
   is required in every walk-running row), a six-entry unit-swap catalog
   (initializer re-seed / vtable backend R1+R2 / R3 tier / PLH Prospero layer /
   no-exec-mode gate / slot-3 validator ridealong) with honest build status, a 9-row matrix
   (R1–R9) with per-row unit swaps, settings delta, ini delta, worker diffs, expected new
   terminal, and one-pass command delta, and the 9-box pre-conditions checklist.

## 2. Key findings

- **The matrix compresses to 9 realistic rows, and only 5 recompose.** R1–R5 are the
  W-decided arming launches; R6 (GMalloc hunt fails), R7 (seed REFUTED), R8 (fc1 pool
  refused) are hold/revise rows with NO composition pass — the comp6 driver's
  edited-landed gates refuse a no-op rebuild by design; R9 (IGW FATAL) overrides
  everything to the data-side configuration.
- **Two hidden deltas the naive plan misses:** (a) W_WORKS removes only the kernel
  question — the PolyHook Prospero plumbing (MemProtector/MemAccessor/Misc) is missing
  regardless (EW-5), and the walks need a per-page mprotect transition before the first
  vtable read or they die igf6-class even under W_WORKS; (b) an armed 4-slot launch
  re-opens FLAG-2/FLAG-1 as ITS verdicts (SEED-TABLE-FORMAT §7 says exactly this), which
  is why R1/R2 carry the transition delta and R3/R4 carry D-1.
- **One composition gap has no owner:** under BOTH_FAIL/FATAL the EngineTick registration
  and StaticConstructObject callback arm unconditionally today (L9 I7/I12); a "no-exec
  mode" initializer gate (U-SWAP-5) must be commissioned before any BOTH_FAIL row can run.
  Flagged in the matrix, not silently assumed.
- **Re-seed is genuinely cheap:** slot-3 landing is a pure table update — one
  regeneration + one recomposition, no merged-unit change (STF §8) — so rows R1–R5 share
  one command skeleton whose only row-specific variance is which NEW units (if any) enter
  the receipt/ODR walk.

## 3. The pre-conditions checklist (summary; full version = LAUNCH10-MATRIX §4)

The post-9 controller reads, in order: (1) which SeedTable.h/init actually shipped
(`CON:comp6-integration/change-manifest.json` fold_ins + merge-owner staging) — decides
Shape A vs B interpretation; (2) UE4SS.log tail against the five exact terminal strings of
L9 §4; (3) `runtime-status.txt` entry-r4 freshness-prefixed lines + failed ack; (4)
`/data/modbridge-exec-writability.jsonl` — contiguous seq 1..12 law, seq5/seq10 rc and
seq12 `decision_input`; a missing seq = FATAL = row R9, stop and preserve; (5)
`/data/modbridge-fc1pool-facts.jsonl` + the single `FC1POOL:` log line vs the FC §7
shapes; (6) observer/qdrv1 records for crash localization; then the in-flight
reconciliations: (7) `worker-gmalloc-derivation/GMALLOC-CANDIDATES.json` + REPORT
(candidate + mandated data-side validator), (8) `worker-r3-zerosyscall-tier/
R3-TIER-DESIGN.md` (exact diffs + acceptance verdict), (9) the already-read design inputs
(VT/EW §7/STF/FC STAGE-PLAN/OPC). Only then: consistency rules → row lookup → §3 command.

## 4. Honest limits

- All rows are prediction routers over launch-9's OWN prediction document; nothing here is
  console-backed and no launch-10 outcome is promised or implied.
- worker-r3-zerosyscall-tier and worker-gmalloc-derivation were CLAIM-only at read time;
  every row citing them is marked pending reconciliation (checklist boxes 7–8). The matrix
  must be re-checked against their delivered artifacts before launch-10 admission.
- U-SWAP-2/4/5/6 have no implemented code; "worker diffs apply" means a build worker must
  author and receipt-cover them post-9 — nothing is drop-in.
- Exact host-side console-copy filenames for launch-9 records are capture-run-dependent
  (precedent: `CON:console/igf6-results.jsonl`); the checklist names the on-console paths,
  which are stable.
- No quality, completion, or acceptance claim of any kind; critic-free; the matrix changes
  no gate semantics — it only routes which pre-staged designs apply per outcome.

## 5. Deliverables

| File | Content |
|---|---|
| `CLAIM.json` | scope, inputs read, honesty notes (written first) |
| `LAUNCH10-MATRIX.md` | dimensions + consistency rules, unit-swap catalog, 9-row matrix, one-pass command skeleton + row deltas, 9-box pre-conditions checklist, honesty bounds |
| `REPORT.md` | this file (written last) |
