# ANALYSIS — launch-11 enqueue-ladder blocker (RUNBOOK worker-l11-marshal-integration §5.1 item 1)

Worker: worker-l11-enqueue-ladder (restart, 2026-10-05). LOCAL ONLY. All claims
carry receipts under `receipts/` (compile gate) and `host-test/receipts/`
(host integration test) of this worker directory.

## 1. The blocker, stated precisely

In the W_FAILS_R_WORKS world (initializer's EngineTick registrations deferred;
address IS assigned via vtable fallback; registration refused at the hook
layer), the mod's DEFAULT-method `ExecuteInGameThread` path never reaches any
queue: it attempts the lazy EngineTick detour arm first, the arm throws, and
the closure (the spawner's F7 closure) is lost. The FuncPtr marshal sits armed
and unused, because the DIFF-1 ladder only reaches the marshal when the
ADDRESS is unavailable — and here the address is available. Availability is
not registrability (R3D §4.5 trap).

## 2. Throw-site trace (file:line)

Base = `worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp`
(sha256 `04c9d91664dd82c7…`, the dry-run-GO + marshal-consumer revision).

| step | site | what happens |
|---|---|---|
| 1 | `LuaMod.cpp:4973` | `m_lua.register_function("ExecuteInGameThread", …)` — the binding under test |
| 2 | `LuaMod.cpp:4982` | `method = LuaMod::m_default_game_thread_method` — EngineTick by shipped default (`SettingsManager.hpp:50`) |
| 3 | `LuaMod.cpp:4998-4999` | EngineTick branch; `!is_engine_tick_hook_available()` is **FALSE** in W_FAILS_R_WORKS (vtable fallback assigned the address), so the three-state R3 ladder at 5002–5021 — and with it the `is_funcptr_marshal_armed()` fallback at 5010–5012 — is **skipped entirely** |
| 4 | `LuaMod.cpp:5022-5025` | the `else` branch runs instead: `LuaMod::ensure_engine_tick_hooked();` at **5024** |
| 5 | `LuaMod.cpp:4797-4806` | `ensure_engine_tick_hooked()`: `RegisterEngineTickPreCallback` returns `ERROR_ID` (write-refused world) |
| 6 | **`LuaMod.cpp:4807`** | **`throw std::runtime_error("EngineTick callback registration failed");` — THE THROW SITE** |
| 7 | `LuaMod.cpp:5069` | mutex acquire — **never reached** |
| 8 | `LuaMod.cpp:5083/5087/5095/5099` | the enqueue (`emplace_back`) — **never reached; the closure is lost** |

Runtime surfacing: the throw crosses the registered Lua C closure;
`LuaMadeSimple::process_lua_function`'s two-arg `TRY` (`LuaMadeSimple.hpp:797-810`)
catches it and converts to `throw_error(lua_state, e.what())` — a Lua script
error at the mod's call site. The marshal is bypassed (armed flag still true,
zero closures enqueued anywhere). The host test reproduces exactly this
surface: control world surfaces `"EngineTick callback registration failed"`
through lua_pcall with BOTH queues empty (`host-test/receipts/host-test-control.stdout`).

## 3. The staged fix (enqueue-first; queue survives arm failure)

Revision = `diffs/l11-enqueue-ladder.diff` on the base above; staged =
`staging/luamod-l11-el-ladder/LuaMod.cpp` (`f50976ae1e2626b0…`); patch
round-trip proven (`work/patchcheck` == staging). One replace-unit source
revision, five hunks, one file, all inside the `ExecuteInGameThread` binding,
`__PROSPERO__`-guarded:

1. **Address-available `else` branches (EngineTick + ProcessEvent mirror).**
   Marshal-first when armed (R3D §4.5: the carrier-armed module state outranks
   an address check): `is_funcptr_marshal_armed()` → `method = kFuncPtrMarshalExecutor`
   (the marshal is the game-thread pump in this world; no second EngineTick
   pump may be attempted). Else keep EngineTick/ProcessEvent — and do NOT arm
   here.
2. **Address-unavailable ladders.** The `ensure_process_event_hooked` /
   `ensure_engine_tick_hooked` calls demoted to method selection only; the
   actual arm moved out of the pre-enqueue ladder entirely.
3. **THE ORDER GUARANTEE (new, post-enqueue).** After the closure has been
   enqueued (still under the already-held `recursive_mutex`), the lazy arm runs
   inside the real port `TRY`: a refused registration can no longer discard
   enqueued work. Marshal-selected paths arm nothing here.
4. **Loud stall honesty.** If the post-enqueue arm was refused (pump flag still
   false), an `LogLevel::Error` line is emitted on every such call:
   `[R3] ExecuteInGameThread: closure enqueued but the EngineTick pump is not
   registered (detour registration refused); it stays queued until a pump
   exists` (and the ProcessEvent mirror). The queue survives; the stall is
   never silent.

## 4. Host integration test — evidence shape

`host-test/` drives the mod's ACTUAL default API: a real Lua 5.4 state
(`ExecuteInGameThread(callback)` with ONE argument — the `main.lua:695` shape,
default method EngineTick per `SettingsManager.hpp:50`) calls the REAL staged
binding code, extracted byte-exact per world by `extract_regions.py` (guards:
fix markers only in fixed; `ensure_hooks`/queues/drain/availability/marshal
regions byte-identical across worlds; bindings differ). The REAL
`process_simple_actions` drain, REAL `ensure_engine_tick_hooked`/`ensure_process_event_hooked`
(throw sites untouched by the fix, hash-proven), REAL port `TRY`, REAL
LuaMadeSimple, and REAL queues execute. Only the PS5 platform surface is a
declared shim (registration refusal knob, address-availability knob, Output
capture, FName, single-threaded luauser lock no-ops).

Results (exit 0 both; `PASS-receipt.txt`):
- **control (unpatched, 14/14)**: W_FAILS and armed worlds — the default call
  throws `EngineTick callback registration failed` BEFORE any enqueue; both
  queues empty; with the marshal armed it is still bypassed (the exact
  RUNBOOK §5.1 finding). The test DETECTS the bug it targets.
- **fixed (23/23)**: armed world = enqueued onto the marshal queue + drained by
  the marshal pump (witness executed exactly once; no detour attempted; zero
  error logs). W_FAILS world = enqueued onto the EngineTick queue + the loud
  stall log + the refused-registration log, closure stays queued, never
  silently discarded. Working world = enqueued + armed + drained (no
  regression). Address-unavailable ladder fallback = enqueued + drained
  (pre-existing DIFF-1 fallback intact in both lineages).

## 5. Honest bounds

- This revision patches the DEFAULT `ExecuteInGameThread` ladder only. The
  WithDelay family (`ExecuteInGameThreadWithDelay` base:5108,
  `RetriggerableExecuteInGameThreadWithDelay` base:5302,
  `ExecuteInGameThreadAfterFrames` base:5457, `LoopInGameThreadWithDelay`
  base:5517, `LoopInGameThreadAfterFrames` base:5645) still arms before its
  enqueues and still loses closures in the write-refused world — RUNBOOK §5.1
  item 1's "all three WithDelay-family ladders" remains OPEN and needs its own
  distinct-ID revision.
- Host-only evidence: no PS5/console run, no composition/packaging, F-1
  carrier rate unchanged (OPEN). See REPORT.md.
