# R3-MODE ARM/DISARM PROCEDURE — worker-r3-diffs-authoring (2026-10-04)

Scope: the `UE4SS_R3_MARSHAL_MODE` constant introduced by
`diffs/r3-initializer-r3-marshal-mode.diff` (DIFF 2) and consumed by
`diffs/r3-luamod-funcptr-marshal.diff` (DIFF 1). Mirrors the
`TEAM:worker-no-exec-mode-gate` arm/disarm discipline (its design §4), adapted to the
R3 marshal. PROPOSALS ONLY: nothing in this procedure has been executed against a
composition; steps reference the proven composition-4/5 receipt flow.

## 0. Invariants (by design, both states)

- Compile-time only. There is NO runtime re-arm path: no ini key, no Lua call, no probe
  file can flip the mode of a compiled unit (SETTINGS-V3 trap 2 rationale).
- Mode 0 (in-file fallback default) keeps the initializer byte-behavior-identical to the
  staged initializer and leaves LuaMod's new code dormant-but-present (the marshal arms
  only when `arm_funcptr_marshal_drain` is called — see §3).
- LuaMod.cpp has NO mode define: DIFF 1 is mode-agnostic by construction. Only the
  initializer carries the R3-mode constant.

## 1. ARM (R3 both-fail world, LAUNCH10-MATRIX R9 fallback composition)

1. Apply DIFF 1 to the luamod unit source and DIFF 2 to the staged initializer
   (`--replace-runtime-unit unreal-unrealinitializer=<staging copy of the patched
   UnrealInitializer.cpp>`), staging the patched SettingsManager.hpp with the edited
   enum so the fresh-dependency pin walk sees it (the composition-4 ordering lesson:
   replacement sources must be visible BEFORE the fresh-unit loop).
2. Compile the initializer with `-DUE4SS_R3_MARSHAL_MODE=1` (preferred: build define
   recorded in the run manifest; fallback: flip the in-file define to 1 — either way the
   chosen source must be receipt-covered). Both forms verified to compile clean this
   session (`receipts/compile-gate.json`, variant `patched-armed`).
3. Regenerate BOTH receipts with `--program-source` at the new staging dir, then
   `--verify-only`, one pass (no-op rebuilds are refused by design for
   edited-landed gates).
4. Record in the run manifest: mode=1, the define source (build vs in-file), the diff
   hashes (`receipts/diff-hashes.json`), and the carrier-calibration outcome record
   (F-1 falsifier, R3D §4.2 step 6) once cc1/calibration lands.
5. Expected armed-startup log lines (add to box-2 parser list, see MATRIX-ROW-NOTE.md):
   `[R3] refused: FName constructor verification detour ...`,
   `[R3] refused: EngineTick pre-callback registrations ...`,
   `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal`,
   and, at first marshal publication,
   `[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)`.

## 2. DISARM

1. Compile with mode 0 (omit the build define or flip the in-file define back to 0).
2. Regenerate both receipts as in ARM step 3 (mandatory re-pinning; do not reuse armed
   receipts for a disarmed build or vice versa).
3. Behavior contract: the initializer is then behavior-identical to the staged unit
   (constexpr folds every R3 site away); the three exported bridges still exist under
   __PROSPERO__ but refuse loudly if ever called
   (`[R3] publish bridge called while R3 marshal mode is disarmed; refusing`).
   A disarmed initializer MUST NOT be paired with a Lua world that assumes the marshal
   is the pump — ExecuteInGameThread closures routed to FuncPtrMarshal would enqueue and
   never drain (the ladder's throw is the louder failure; see §3).

## 3. Marshal arming is a SECOND, separate step (the calibration's)

The R3-mode constant only moves the PUMP DEFERRAL. The drain itself arms when the
calibration owner (R3D §4.2 protocol; per R3D §5.2 item 4 that service lives in
LuaMod.cpp and is OUTSIDE this diff) calls the file-static
`arm_funcptr_marshal_drain(carrier UFunction*, carrier FName id)` with its selected
candidate. Until then:
- `is_funcptr_marshal_armed()` is false; the Lua table exposes
  `EGameThreadMethod.FuncPtrMarshal` but ExecuteInGameThread closures routed there are
  enqueued and await arming (lazy, one warning line);
- the WithDelay-family ladders throw their three-way error instead of selecting the
  marshal rung.
Residual hazard recorded for the calibration owner: before FIRST publication, whichever
thread invokes the carrier wins `publish_current` and BECOMES the published game thread
(drain cannot distinguish threads pre-publication; pinned by host test
`host-tests/run-output.txt` race-semantics check). Calibration criterion C-4
(game-thread-stable carrier) is what makes this safe; arming before C-4 evidence is a
calibration-protocol violation, not a drain defect.
