# REPORT — worker-l11-enqueue-ladder (restart)

Worker: worker-l11-enqueue-ladder, 2026-10-05. Mission: the launch-11
enqueue-ladder blocker recorded in
`worker-l11-marshal-integration/RUNBOOK.md` §5.1 item 1 — in W_FAILS_R_WORKS
the default `ExecuteInGameThread` throws before enqueueing, bypassing the
armed FuncPtr marshal; the mod's F7 closure is lost. LOCAL ONLY throughout:
this worker dir + owned staging; marshal-integration/launch10 artifacts
read-only; no console contact, no composition, no packaging.

## What was delivered (all receipts in this directory)

1. **Throw-site trace (file:line)** — ANALYSIS.md §2. Throw site:
   `LuaMod.cpp:4807` (`throw std::runtime_error("EngineTick callback
   registration failed")` inside `ensure_engine_tick_hooked`), reached from
   the address-available `else` at `LuaMod.cpp:5024` of the
   `ExecuteInGameThread` binding (registered at 4973, default method EngineTick
   per `SettingsManager.hpp:50`), because the address IS assigned in
   W_FAILS_R_WORKS and the marshal fallback (5010–5012) is only reachable when
   the address is NOT. The throw precedes the mutex (5069) and every enqueue
   (5083/5087/5095/5099) — closure lost, marshal bypassed.
2. **Staged enqueue-first fix** — `staging/luamod-l11-el-ladder/LuaMod.cpp`
   (`f50976ae…`) via `diffs/l11-enqueue-ladder.diff` (`3b19068a…`), patch
   round-trip proven. Marshal-first when armed (R3D §4.5); lazy detour arm
   moved AFTER the enqueue inside the real port `TRY`; loud `LogLevel::Error`
   stall log on every refused arm; marshal-selected paths arm nothing.
3. **Compile gate (PASS)** — both control and staged units under the frozen
   argv pattern (`work/argv-control-el.json`, `work/argv-el-ladder.json`;
   prospero-clang++ `-c`, `LLVM_CONFIG=outputs/ps5-mod-bridge/native-probe/
   build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`). 0 errors both;
   23 warnings both with exact warn-kind parity. Control object
   `854a476bdd4f9985…` byte-matches the marshal-integration session's consumer
   object (frozen-argv path-independence, as that runbook predicted). Fixed
   object `8593884ea825d523…`.
4. **Host integration test (PASS)** — `host-test/PASS-receipt.txt`. Through the
   mod's ACTUAL default API (a real Lua 5.4 state calls
   `ExecuteInGameThread(callback)` with one argument — the main.lua:695 shape),
   driving the REAL staged binding code, REAL queues, REAL
   `process_simple_actions` drain, REAL ensure-*/throw sites (hash-proven
   untouched), REAL port `TRY`, REAL LuaMadeSimple:
   - control (unpatched) 14/14: enqueue loss reproduced in BOTH the W_FAILS
     world and the armed world (armed marshal bypassed; queues empty; the
     surfaced error is the traced throw site) — proving the test detects the
     bug it targets;
   - fixed 23/23: armed world = enqueued + drained by the marshal pump (no
     detour attempted, zero error logs); W_FAILS world = enqueued + loud stall
     log + refused-registration log, closure stays queued, never silently
     discarded; working world = enqueued + armed + drained (no regression);
     address-unavailable ladder fallback intact in both lineages.

## What this worker fixed from the prior (lost) attempt

The prior attempt's `extract_regions.py` binding end-anchor `"        });"`
substring-matched the INNER 16-space `});` of the fixed lineage's
order-guarantee TRY block — truncating the binding BEFORE the loud-stall log,
i.e. the fixed world's key evidence would never have been tested. Corrected
(anchor on the next binding's comment; the extract's stop index is exclusive),
guards added (`binding_complete`, `has_stall_log`), regions re-extracted. The
prior compile-gate artifacts (diff, staging, objects, argv, gate receipts)
verified intact and were reused unchanged.

## Honest limits (read before any launch brief)

- **HOST evidence only.** The integration test runs on macOS with the PS5
  platform surface shimmed and declared: registration refusal and address
  availability are knobs; Output is captured to a buffer; FName is a stub; the
  Windows-only `luauser.c` lock hooks are single-threaded no-ops (the shipped
  module compiles the real file on the PS5 toolchain). No PS5/console run was
  performed or simulated; nothing here is in-game acceptance evidence.
- **`__PROSPERO__` is a host define here** (the regions' shipped PROSPERO
  branches compile against the shims). The PROSPERO compile gate is the
  target-toolchain evidence; the host test is behavioral evidence. Neither
  replaces a console run.
- **WithDelay family NOT patched** (RUNBOOK §5.1 item 1 requires them):
  `ExecuteInGameThreadWithDelay` (base 5108), `Retriggerable…` (5302),
  `ExecuteInGameThreadAfterFrames` (5457), `LoopInGameThreadWithDelay` (5517),
  `LoopInGameThreadAfterFrames` (5645) still arm-before-enqueue and still lose
  closures in the write-refused world. Any mod calling those APIs from a
  default-method path remains exposed. This revision scopes ONLY the default
  `ExecuteInGameThread` ladder per the mission statement.
- **R3-world boot unchanged:** the LoadMap eager-registration blocker
  (RUNBOOK §5.1 item 2) is unowned and untouched; §3.1/§3.2 oracle-line
  reachability is unchanged by this diff.
- **Stall log volume:** the stall line prints on EVERY refused-arm call by
  design ("never silent"); a chatty mod in a permanently refused world will
  spam it. That is the declared trade for enqueue-first honesty.
- **W_FAILS-without-marshal behavior is a stall by design:** the closure stays
  queued with no drainer and never runs. The fix's promise is "never silently
  lost", not "always runs".
- **F-1 carrier rate, composition acceptance, packaging:** unchanged/open; the
  staged revision is proposal-only until the controller composes it (its
  comparator must switch from `854a476b…` to `8593884e…` for the luamod unit).
- **Restart provenance:** this dir's pre-restart partial artifacts (diff,
  staging, gate receipts, argv, two extract-region shims) were verified by
  hash and reused; everything else was rebuilt this session.

## Deliverable index

| deliverable | path |
|---|---|
| claim | `CLAIM.json` |
| analysis (throw-site trace) | `ANALYSIS.md` |
| staged diff + explainer | `diffs/l11-enqueue-ladder.diff`, `DIFF.md` |
| staged source | `staging/luamod-l11-el-ladder/LuaMod.cpp` (`f50976ae…`) |
| compile gate | `receipts/gate-el-ladder.*`, `receipts/gate-control-el.*`, `work/argv-*.json`, `work/*.o` |
| host test + PASS receipt | `host-test/PASS-receipt.txt`, `host-test/receipts/`, `host-test/host_test.cpp`, `host-test/extract_regions.py`, `host-test/build_host_test.sh`, `host-test/run_host_test.sh`, `host-test/regions/regions.json` |
