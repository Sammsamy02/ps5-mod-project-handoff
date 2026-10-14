# MATRIX-ROW NOTE — LAUNCH10-MATRIX R8/R9 consumption of the R3 diff pair
(worker-r3-diffs-authoring, 2026-10-04. Cite-base: `TEAM:worker-launch10-matrix-prep/LAUNCH10-MATRIX.md` rows R8/R9, §3.1/§4; `TEAM:worker-no-exec-mode-gate/REPORT.md` §3 box-2 precedent.)

## Row R9 (IGW FATAL — syscall-74 route permanently closed; composition falls back to R5's data-side configuration; U-SWAP-3 subject to the R3 worker's verdict)

R9 is the row DIFF 1 + DIFF 2 are built for. The matrix already notes "heap tier needs
NO syscalls, so it survives FATAL" (row R9, U-SWAP-3 cell). These diffs are the concrete
consumable for that cell:

- DISARMED (default 0, the shipped fallback): both diffs are byte-inert vs the staged
  units (initializer constexpr-folded to identity; verified `patched` == `base` diagnostic
  parity, `receipts/compile-gate.json`); R9's data-side fallback composition proceeds
  exactly as the matrix skeleton §3.1 says, with the R5 expected terminal.
- ARMED (`UE4SS_R3_MARSHAL_MODE=1`): the composition gains the R3 pump route — init no
  longer throws at the FName-ctor detour (SINI:954-958) nor the EngineTick registrations
  (SINI:1383-1397); the game-thread pump + query-harness dispatch ride the marshal
  (R3D §4.4a/b). Row-relevant consequences the next revision must carry:
  1. **New evidence strings for the box-2 parser list** (no-exec-gate precedent):
     `[R3] refused: ...` (two refusal sites, counted in `R3MarshalGate::refused_site_count`),
     `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal`,
     `[R3] FuncPtr-marshal armed on carrier <full name> (callback id <n>)`,
     `[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)`.
     A missing deferral line in an ARMED run is itself a box-2 anomaly (the pump route
     silently absent).
  2. **Query-harness instrument relabel (R3D §4.4b):** under the marshal, the harness's
     "two consecutive accepted reads" count CARRIER INVOCATIONS, not engine ticks. Prior
     rung evidence phrased as "ticks" is NOT comparable with marshal observations; R9
     acceptance notes must say `marshal-observation`, not `tick`.
  3. **Runtime-status honesty:** the armed initializer never throws at the two gated
     sites, but it also never silently pretends the pump is armed — the deferral line +
     `pump_pending` state is the row's loud status. Lua can load before the marshal arms
     (R3D §4.4a: mods-started does NOT gate on marshal arming; the first
     ExecuteInGameThread closure awaits the carrier).
  4. **F-1 remains the row's falsifier:** if the calibration records `f1:"FALSE"`, the
     R3-only spawner verdict is NO (R3D §4.2 step 6) — the diffs still stand (init no
     longer throws) but the spawner rungs are unreachable; the trial table is the evidence.

## Row R8 (fc1 pool refusal — HOLD U-SWAP-1, pre-composition)

R8 refuses BEFORE the initializer is ever consumed; both diffs are unreachable in an R8
window and neither changes R8's pre-composition gates, the `[Seeds]` partial-table throw
signature, or the T-A byte reads. No action required for R8 beyond this note. The
disarmed diffs' in-file default (0) guarantees the next R9/R5 recomposition cannot
accidentally inherit armed behavior from these files alone — arming is an explicit,
manifest-recorded act (ARM-DISARM.md §1).

## Cross-row caution

Do NOT pair an ARMED initializer with a Lua bundle whose default
`DefaultExecuteInGameThreadMethod` stays EngineTick and expects ticks (the staged ini
default). The WithDelay-family ladders degrade EngineTick→ProcessEvent→FuncPtrMarshal
per call, but plain `ExecuteInGameThread` overload #1 with an unavailable EngineTick now
follows the same three-state ladder (DIFF 1) — the spawner's closures survive, while any
mod asserting tick-cadence timing needs the relabeled instrument (point 2).
