# REPORT — worker-r3-ladder

Worker: worker-r3-ladder · 2026-10-05 · unit/proposal only. LOCAL ONLY: no
console contact, no composition run, no packaging, no game claims; wrote ONLY
under `outputs/2026-10-04-zcode-team/worker-r3-ladder/` (staged unit in own
dir; shared lineage files untouched). Mission: the RUNBOOK
l11-marshal-integration §5.1 item 1 ladder trap — the DIFF-1 WithDelay-family
ladders keep address-availability as the primary predicate, so in a
walk-permitted world the first default-method delay call throws while the R3
marshal sits armed and unused — traced against the CURRENT tree
(initializer `e7f349af…` + LuaMod `c92c92ff…`), then fixed as a staged unit
with host proof.

## 1. The traced predicate chain (current tree, file:line)

Aliases: `UI` =
`outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp`
(sha256 `e7f349aff7594ba2…`); `LM` =
`outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp`
(sha256 `c92c92ff66686bf6…`); `RT` = the UE4SSRuntime source
(`.superpowers/…/source-dependencies/ue4ss/UE4SS/src/UE4SSRuntime.cpp`).

**Who computes engine-tick availability:**
- LM:4673-4676 `is_engine_tick_hook_available()` → RT:11-16
  `UE4SSRuntime::IsEngineTickAvailable()` = `UEngine::TickInternal.is_ready()`
  — an ADDRESS-assigned check, NOT a hook-armed check (KB anti-pattern 13).
- The address is assigned by UI's vtable fallback in a walk-permitted world:
  UI:1986-1992 (`Finding GameEngine::Tick` + vtable resolve), UI:2051-2054
  (scan-fail branch: `Scan failed, falling back to vtable address for
  GameEngine::Tick` → `UEngine::TickInternal.assign_address(vtable_address)`)
  or UI:2060-2063 (vtable-preferred branch). UI:2029-2045 documents that with
  armed seeds this fallback is the ONLY tick route.
- The R3 armed fold defers the registrations: UI:178-179
  (`UE4SS_R3_MARSHAL_MODE 1` default), UI:2071-2087
  (`R3MarshalGate::refuse_registration_site("EngineTick pre-callback
  registrations …")` + `pump_pending.store(true)`), bridges at UI:2350-2418
  (`R3MarshalPublishGameThreadIdentity`, `R3MarshalQueryHarnessDispatch`,
  `R3MarshalPumpPending`).

**Where the marshal carrier is armed and drained:**
- Armed: LM:4725-4751 `arm_funcptr_marshal_drain` (precheck :4690-4714;
  `RegisterPostHook(&funcptr_marshal_hook)` at :4740 — the SetFuncPtr carrier
  store; armed flag release-store at :4748; oracle line :4749). Driven by the
  calibration consumer LM:4776-4808 (`__PROSPERO__`-gated; requires
  `Unreal::R3MarshalPumpPending()` at :4782; ranked carriers
  Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation :4787-4790).
- Drained: LM:4553-4622 `funcptr_marshal_hook` (publication gate
  :4566-4574 — drains NOTHING until `R3MarshalPublishGameThreadIdentity()`
  succeeds; thread gate :4584-4587; then `process_simple_actions`
  :4597 + `process_delayed_actions<kFuncPtrMarshalExecutor>` :4598). The
  marshal executor drains ONLY marshal-tagged, TIME-based delayed actions:
  LM:4376-4385 ("frame-based delays remain EngineTick-only").

**Where ensure throws:** LM:4812-4828 `ensure_engine_tick_hooked` →
`RegisterEngineTickPreCallback` returns ERROR_ID under the write-refused world
→ LM:4820-4822 `throw std::runtime_error("EngineTick callback registration
failed")`.

**What the FIRST default ExecuteInGameThread actually consults:** the first
one is the first F7/OPTIONS press, not load time — canonical main.lua
(package-v3, `f586043a…`) registers every game-thread call inside keybind
callbacks; :694-696 `RegisterKeyBind(Key.F7, function() ExecuteInGameThread(
toggleMenu) end)`; no WithDelay/AfterFrames/Loop call exists in this mod. The
default method is `EngineTick` (`LuaMod.hpp:167`
`m_default_game_thread_method{GameThreadExecutionMethod::EngineTick}`), so
the call enters LM:5013's EngineTick branch.

**Drift vs the reports (important):** the walkguard-gate REPORT §4 and the
RUNBOOK describe the trap at `:5015/:5058` — the default
`ExecuteInGameThread`. In the CURRENT merged tree that site is ALREADY
REPAIRED by the enqueue-ladder fold (worker-l11-enqueue-ladder, merged in
c92c92ff): marshal-first-when-armed at LM:5041-5056 (and the ProcessEvent
mirror :5089-5097), plus the enqueue-first order guarantee with the loud stall
lines :5150-5184. The EL worker's own REPORT declares the WithDelay family
OUT of its scope ("WithDelay family NOT patched … still arm-before-enqueue
and still lose closures"). The trap therefore REMAINS at five sites, all
availability-first with ensure BEFORE enqueue and NO marshal rung on the
address-available path:
- LM:5213-5223 `ExecuteInGameThreadWithDelay` (`if
  (is_engine_tick_hook_available()) LuaMod::ensure_engine_tick_hooked();`)
- LM:5400-5410 `RetriggerableExecuteInGameThreadWithDelay` (same shape)
- LM:5553-5561 `ExecuteInGameThreadAfterFrames` (throws if address
  unavailable; if available, ensure BEFORE enqueue)
- LM:5615-5624 `LoopInGameThreadWithDelay` (same shape as WithDelay)
- LM:5741-5749 `LoopInGameThreadAfterFrames` (same shape as AfterFrames)
In the walk-permitted world (M-A/GATE-1 lifted) availability reads TRUE at
each of these, `ensure_engine_tick_hooked()` runs, the detour registration is
refused, and the throw fires — with the closure never enqueued and the armed
marshal bypassed. That is the §5.1 ladder trap, current-tree-anchored.

## 2. The staged unit (diff summary)

`diffs/l11-r3-ladder.diff` (`29535f023c0105f3…`): 6 hunks, ONE file. Base
`c92c92ff…` → staged `6f97f8a1727b7e49…` (8463 lines, +121). Round-trip
proven byte-exact both directions. Full hunk table in DIFF.md; semantics:
- NEW `funcptr_marshal_usable()` = `is_funcptr_marshal_armed() &&
  Unreal::R3MarshalPumpPending()` (staged :4194-4197, `__PROSPERO__`-gated) —
  the rev-B pairing law as the usable-pump predicate.
- The three WithDelay-family functions get a TOP-GATE before any availability
  logic (staged :5236-5261, :5446-5471, :5700-5725): usable ⇒
  `method = kFuncPtrMarshalExecutor` (marshal first, deterministically, both
  EngineTick and ProcessEvent defaults); armed-but-dead ⇒ loud
  `lua.throw_error` naming `pump_pending=false` (fail-closed; no silent
  queueing onto a dead drain, no second EngineTick pump). The pre-existing
  ladders below stay byte-identical and remain reachable only when the marshal
  is NOT armed — their third-rung throw is now the ONLY unhooked-tick throw.
- The two AfterFrames functions get the marshal-world gate (staged
  :5621-5636, :5848-5863): a usable marshal cannot count frames (drain skips
  frame-based actions by design), so the request is refused LOUDLY and
  precisely instead of attempting the forbidden detour or silently stalling;
  non-marshal worlds byte-identical.
- NOT changed: the ensure throw sites, the EL fold in `ExecuteInGameThread`,
  drain bodies, W-works behavior. Residual (out of scope, distinct-ID):
  WithDelay-family enqueue-first discipline for the marshal-NOT-armed world
  (unreachable in the R3 candidate's usable-marshal world).

## 3. Host-test results (predicate order proven)

`host-test/PASS-receipt.txt` (`85aaeac9d888673a…`); both worlds exit 0:
- CONTROL (unpatched `c92c92ff…`): **12/12 PASS** — the trap REPRODUCED:
  WithDelay + AfterFrames throw `EngineTick callback registration failed`
  BEFORE any enqueue in the armed+assigned+refused world; queues empty;
  marshal still armed and bypassed; exactly one forbidden detour attempt;
  third-rung throw intact.
- FIXED (staged `6f97f8a1…`): **40/40 PASS** —
  P1 armed+alive with the tick address ALSO assigned ("both alive"): no
  throw, all three closures enqueued tagged `kFuncPtrMarshalExecutor`, ZERO
  detour attempts (marshal wins deterministically);
  P2 armed+dead (`pump_pending=false`): loud dead-drain refusal with and
  without the tick address, nothing enqueued, no detour attempt;
  P3 no backend at all: the pre-existing third-rung throw fires (the only
  unhooked-tick throw, both lineages);
  P4/P5 AfterFrames: precise frame-capability refusal in the marshal world,
  nothing silently stalled; R1/R2 working worlds preserved byte-for-byte in
  behavior (detour armed, actions tagged EngineTick).
- The harness drives the REAL staged binding code (byte-exact regions,
  hashed in `host-test/regions/regions.json` `941e076bf6cd5cfd…`), the REAL
  delayed queues, REAL ensure/throw sites (hash-proven identical across
  lineages), REAL port TRY, REAL Lua 5.4 (LuaRaw as C++) and REAL
  LuaMadeSimple; the PS5 surface is a declared shim (availability knob,
  registration-refusal knob + attempt counters, `R3MarshalPumpPending`
  pairing knob, Output capture, FName, luauser lock no-ops). HOST evidence
  only; no console run was performed or simulated.

## 4. Compile gate + expected object sha (pinned argv)

Frozen 172-token argv (worker-l11-enqueue-ladder pattern, prospero-clang++,
LLVM_CONFIG route; only source/-o/-MF redirected — `work/argv-*.json`):
- control (c92c92ff): exit 0, 0 errors, 24 warnings; object
  `8343578bfa735843189165ff6ed96d5721c53ae586680f5ec331890fb8044de` — a NEW
  baseline for the DRIFTED tree (the staging gained +15 unrelated
  normalize_path lines post-merge; NOT the merge worker's `813b249d`
  comparator).
- staged (6f97f8a1): exit 0, 0 errors, 24 warnings, exact warn-kind parity;
  **expected object `761b610a20e9d6cbac494976f907cbd0bd053e21005a675e2d7339cf31cd8941`**
  (7,028,928 B), reproduced byte-identically on a second compile with a
  different `-o` path (path-independence).
- UND delta EMPTY (540 = 540): the five new `R3MarshalPumpPending` call sites
  bind to the bridge UND already present in this lineage; no
  `--extend-undefined-baseline` needed.

## 5. Composition admission shape (controller steps; NOT run by this worker)

```
  --replace-runtime-unit luamod=outputs/2026-10-04-zcode-team/worker-r3-ladder/staging/luamod-l11-r3-ladder/LuaMod.cpp \
  --replace-receipt outputs/2026-10-04-zcode-team/worker-r3-ladder/receipts/replace-receipt.json \
```

(`luamod` ∈ RETAINED_REPLACEABLE, fixed l11 driver :92; no `--extra-unit`
needed; fresh run dir; serialized. Full comparator table +
pairing-law restatement in `receipts/ADMISSION.md`.) The receipt's
`expected_object_sha256_under_pinned_argv` is the offline-computed
`761b610a…` above; the composed FINAL object sha stays unmeasured/unclaimed
(controller's pass). Provenance note for the controller: base is the MERGED
staging (EL + loadmap-defer already inside); the separate loadmap-guard-r2
lineage (`9a7a37d6…`, built on `04c9d916…` without the EL delta) is a
DIFFERENT branch — do not stack this diff onto it without re-derivation.

## 6. Honest bounds

- Source + host-derived only; zero console contact; no composition; nothing
  here is in-game acceptance evidence. The oracle lines (RUNBOOK §3.1/§3.3)
  remain LIVE-only.
- The host shims declare the platform surface; the PROSPERO compile gate is
  the target-toolchain evidence; neither replaces a console run.
- F-1 (carrier invocation rate) unchanged/open; this unit does not touch the
  drain; GATE-5 (LuaMod eager LoadMap registration) unowned and untouched;
  GATE-1 (walkguard fold) unchanged.
- Residual WithDelay-family enqueue-loss in the marshal-NOT-armed world is
  documented (DIFF.md) and deliberately left to its own distinct-ID revision.
- This worker wrote only under `outputs/2026-10-04-zcode-team/worker-r3-ladder/`.
