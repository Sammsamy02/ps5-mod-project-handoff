# REPORT — worker-r3-diffs-authoring (2026-10-04)

Mode: DIFF AUTHORING + HOST VERIFICATION ONLY. No console contact, no nested subagents,
nothing staged into any composition, nothing executed in-game; the spike tree and the
staged initializer were modified NOWHERE — all work ran on hash-verified copies under
this worker's directory. Deliverable order honored: CLAIM.json first (amended once,
mid-session, per the carrier-prep correction discipline), diffs + receipts, this REPORT
last. Tool calls: ~48 (within the 40–80 budget).

## 1. What was asked and what was delivered

Task: author the BOTH_FAIL-branch diff pair of `TEAM:worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md`
(R3D) to build-ready, host-verify against the composition unit argvs, and deliver the
R3-mode arm/disarm procedure + the R8/R9 matrix-row note. Read-order contract honored
(R3D in full first; then carrier-prep, no-exec-gate, the staged initializer, the LuaMod
unit, UFunction.cpp/UFunctionStructs.hpp, NativeGameThreadIdentity.hpp, LAUNCH10-MATRIX).

| deliverable | content |
|---|---|
| `CLAIM.json` | 5 claims (RDA-1..5) + non-claims + verified base pins |
| `diffs/r3-luamod-funcptr-marshal.diff` | DIFF 1, 13 hunks (see §2) |
| `diffs/r3-initializer-r3-marshal-mode.diff` | DIFF 2, 6 hunks (see §2) |
| `ARM-DISARM.md` | arm/disarm procedure (constexpr R3-mode; marshal arming is the calibration's separate act) |
| `MATRIX-ROW-NOTE.md` | R9 consumes the armed build (4 concrete consequences incl. box-2 strings + instrument relabel); R8 unaffected |
| `RECEIPTS.md` + `receipts/*` + `host-tests/*` | full evidence chain (hashes, argvs, compiler output, mock run) |

## 2. Diff contents (as authored, all anchors verified unique)

**DIFF 1 — SettingsManager.hpp (1 hunk) + LuaMod.cpp (12 hunks):**
`GameThreadExecutionMethod::FuncPtrMarshal` appended last (existing integer values
unchanged); `EGameThreadMethod.FuncPtrMarshal` Lua table entry; file-scope marshal state
(no LuaMod.hpp change, so no extra unit recompiles from LuaMod's side) +
`is_funcptr_marshal_armed()`; `r3_marshal_carrier_precheck` implementing R3D §4.3
verbatim (non-null GetFuncPtr, != `UObject::ProcessInternalInternal.get_function_address()`,
FUNC_Native — loud refusals, no store); `arm_funcptr_marshal_drain(carrier, carrier FName id)`
performing the exact UFunction.cpp:78-89 registration via `RegisterPostHook`
(GetFunctionHookData saves the original then `SetFuncPtr(&Internal::UnrealScriptFunctionHook)`
— one aligned heap pointer store, zero syscalls), idempotent per carrier, refusing a
second carrier; `funcptr_marshal_hook` drain body = process_event_hook's
NativeLuaActionDispatchScope prologue + its identical m_game_thread_actions drain block
with `process_delayed_actions<FuncPtrMarshal>` (plus the new executor filter hunk so the
marshal drains only marshal-tagged delayed actions); three-state ladder rungs
(EngineTick → ProcessEvent → FuncPtrMarshal → throw) inserted in ExecuteInGameThread and
all three WithDelay-family ladders, every new fallthrough under
`#if defined(__PROSPERO__)` so non-PROSPERO behavior is unchanged.

**DIFF 2 — staged initializer (SINI base `933d08a2…`):**
`UE4SS_R3_MARSHAL_MODE` define with in-file fallback 0 + constexpr `R3MarshalGate`
(no-exec-gate pattern incl. refusal counting); under kR3MarshalMode only, inside
`__PROSPERO__`: the FName-ctor PLH detour registration compiled out (data-side address
override binding kept; the blocking verification wait settled by storing
FNameVerificationStatus so no deadlock; loud `[R3] refused:` line; honestly labeled
NOT-verified, delegated to the FC-1 validator per VSD §5 C1) and the two
RegisterEngineTickPreCallback registrations compiled out with the literal line
`[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` +
`pump_pending` set (the state DIFF 1 consumes); exported bridges
`R3MarshalPublishGameThreadIdentity / R3MarshalQueryHarnessDispatch / R3MarshalPumpPending`
defined at the RC::Unreal tail, reusing HookedEngineTick's publication logic
(StartupWait cancellation, idempotent publish_current, first-publication immutability)
and QueryHarnessEngineTickDispatch's body unchanged; `[[maybe_unused]]` on the two
statics per the documented no-exec-gate precedent. Mode 0 = behavior-identical to the
staged unit.

## 3. Verification performed here (all green)

- **Base pins:** LuaMod.cpp == RECIPE luamod `source_sha256` (`a2d07f7c…`); SINI ==
  composition-4's attested staged bytes (`933d08a2…`); verified before any edit.
- **Anchored authoring:** 21 anchors, each occurrence-count == 1, none aborted;
  `diff -u` from the anchored output IS the deliverable; `patch -p1 --dry-run` clean for
  both diffs and the applied output hash-identical to the anchored patched copies.
- **Real toolchain, real argvs:** prospero-clang++ (sdk-v0.43 wrapper over
  LLVM-21.1.8-macOS-ARM64, Target: x86_64-sie-ps5) compiling the patched units under the
  RECIPE argvs (initializer + composition-5 W_INCLUDE staging roots). Ten variants: all
  patched variants exit 0 with objects (luamod `69ef0cab…`; initializer disarmed
  `f27a8f25…`, ARMED `-DUE4SS_R3_MARSHAL_MODE=1` `686a573a…` — the define demonstrably
  changes codegen).
- **-Werror, honestly:** the PINNED BASELINES themselves do not pass -Werror (22 / 5
  pre-existing promoted diagnostics), so a literal patched-only -Werror pass is
  impossible without touching pinned code. The delivered gate is exact kind-parity under
  -Werror: patched and patched+armed add ZERO new diagnostic kinds and lose NONE
  (`receipts/compile-gate.json` werror_parity all-empty), plus clean exit-0 compiles.
- **Mock host test (proven pattern):** clang++ -std=c++20 -Wall -Wextra -Werror, 24
  programmatic checks PASS — mode fold, bridge publication mapping + cancellation +
  disarm refusal, drain gating, pending-swap, pre-check refusals, arm idempotence,
  ladder order, and the race semantics (below).

**The session's key catch:** the first drain draft gated on `IsInGameThreadRaw()` BEFORE
publication — under BOTH_FAIL that deadlocks the pump forever (the predicate is false
for every thread until something publishes, and under the marshal route nothing else
publishes). The host mock caught it; DIFF 1 now publishes FIRST (publish_current is
mutex-arbitrated; the winning thread BECOMES the game thread — R3D §4.3's own semantics,
same trust anchor as HookedEngineTick's genuine-callback publication), then applies the
raw gate. Residual hazard pinned and owned: before first publication, whichever thread
invokes the carrier wins and is published — calibration criterion C-4
(game-thread-stable carrier) is what makes arming safe; ARM-DISARM.md §3 records this as
a calibration-protocol obligation.

## 4. Honest limits / handoff

- **Link + composition unverified:** this session compiles (-c) only. Cross-TU link
  (bridge symbol resolution LuaMod↔initializer, same-ABI signature match) and a composed
  boot are the next composition-order task; the signatures are the documented
  DIFF 1/DIFF 2 contract.
- **Recompile-set delta vs R3D §5.4:** the enum lives in SettingsManager.hpp, so the
  honest recompile set is luamod + settingsmanager + ue4ssprogram + luamodref (+ any
  UE4SSProgram.hpp includers), NOT just "luamod + the replaced initializer" — R3D §5.2
  item 1 requires exactly this header edit; the delta is recorded, not hidden.
- **Calibration service NOT implemented** (R3D §5.2 item 4): DIFF 1 delivers
  drain + registration + readiness; the enumeration/trial/selection protocol's driver
  remains future work and is the only intended caller of `arm_funcptr_marshal_drain`
  (file-static by design — the service lives in LuaMod.cpp per R3D).
- **FName verification is NOT performed under R3 mode** (latch settled, loud line);
  the FC-1 read-only validator is the designated replacement and does not exist yet.
- **SettingsManager ini parse** for `"FuncPtrMarshal"` deferred (R3D §5.1 units table
  lists no settingsmanager unit; the R3 world selects the method explicitly via Lua).
- F-1 remains OPEN; nothing here claims any in-game marshal behavior. Nothing staged,
  nothing executed on any console.
