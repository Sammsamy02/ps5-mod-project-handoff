# Independent l23 game-thread self-test review

**Result: no actionable correctness findings in the reviewed changes.** Review was read-only for the implementation. No console contact, package build, or mod edit was performed.

## Scope and reasoning

Reviewed the two copied units, state header, overlay generator, host queue/state tests, and target compile receipt. The diff from the retained inputs is 68 additions to LuaMod.cpp and eight additions to UE4SSProgram.cpp. The generator targets those owned copies; it does not modify a mod script or add pusher registrations.

- `LuaMod.cpp:121-146` looks up the actual registered `ExecuteInGameThread` global, passes a Lua C callback, and invokes it through `lua_pcall`. It does not insert a synthetic queue entry or drain from startup. `start_mod` requests the probe only after a successful main-script return. `State::begin` admits a single job for the runtime lifetime; the callback captures no LuaMod owner or Lua state.
- The queue call holds the same recursive action mutex used by production dispatcher scopes. The registered API creates the normal hook-state registry reference and inserts the standard action. Production ProcessEvent, EngineTick, and R3 drain paths acquire `NativeLuaActionDispatchScope` before consuming actions; its lock spans the callback invocation, so the smaller unlocked region inside `process_simple_actions` does not permit concurrent uninstall. The outer queue lock also orders the Queued transition before another thread can drain.
- `LuaMod.cpp:7655-7660` cancels the identity token under that action mutex before Lua stop callbacks and state destruction. Existing uninstall removes matching actions from all simple/pending queues before `lua_close`, with the same mutex held. The token is compared but never dereferenced. Cancellation and callback state transitions are serialized by the state mutex; there is no reverse action-mutex acquisition while the state mutex is held.
- Callback success requires the existing `IsInGameThreadRaw()` admission and a positive native thread id. It records the actual callback thread and elapsed monotonic milliseconds; it does not publish identity from the initializer. That verifies consistency with the existing thread-identity mechanism, not an independent proof of how the R3 carrier established that identity.
- Deadline arithmetic uses `steady_clock`, rejects a backward sample, and trips once at 30000 ms. `TimedOut` retains normal queue ownership and allows an explicitly late delivery. Polling touches only the process-lifetime state and diagnostic output; it neither drains Lua nor dereferences an owner. Poll runs in the existing update loop and is therefore observed at its next scheduling opportunity, not by a separate guaranteed-real-time timer.
- `UE4SSProgram.cpp:572` revokes the property reader guard after the update thread joins, mods uninstall and input shutdown complete. This preserves guard availability during mod cleanup and removes it before the remaining program owner is released. The patch adds no earlier revoke.

## Verification performed

Re-ran existing `state-asan` and `state-tsan` binaries: both exited 0. Re-ran `queue-normal` and `queue-asan` for success, wrong-thread, cancellation, Lua API error and missing API: all ten runs exited 0 (7/8/8/4/4 assertions per configuration). The target compile receipt records return code 0 for both copied units, and both receipt source hashes match the files reviewed. This correct receipt supersedes the earlier `main_lua` pointer-call typo; the reviewed source uses `main_lua()->get_lua_state()`.

The queue harness compiles the real probe wrapper/callback and the retained production insertion/drain portions against host Lua. Its `queue_api` is a test registration, so it does not execute the entire production API-selection/hook-state setup path; those parts were source-reviewed and target-compiled. Host tests supply the thread-identity result and native id. Existing Lua C objects are not sanitizer-instrumented. No native callback delivery, 30-second console observation, original menu/spawn result, or full package readiness is inferred from these checks.

## Reviewed file identities

Paths are relative to `outputs/2026-10-08-l23-runtime/thread/`.

| File | SHA256 |
|---|---|
| `build_overlay.py` | `daae58c27c2dcb94d91097b02ee7566e794f67b3503a9d16f3d8330005de41b4` |
| `include/L23GameThreadProbe.hpp` | `e0fdbe8bc711fc57f1e0c4e63c2fe07b69a01c32227b9ef0ab83b71ef18d336e` |
| `src/LuaMod.cpp` | `fc9589dbb81ee7128d5f54b46061a01f3faf54b4b038e6bdfc5ac2626348119f` |
| `src/UE4SSProgram.cpp` | `030407d7351e2d7b78ac817ca572a0574efc985d49983fd24143061d224295b0` |
| `test_queue.py` | `6e25e3589539ae8c326843740c4abcc8111be0c72a1586e8be4599c179c8e9c7` |
| `tests/state.cpp` | `c056c1138dd047c46e3030b51dcda098b65746d1006f103e747bc6150d1df097` |
| `tests/target/RESULTS.json` | `27e75d1b655baf7d957e3bf1bbad6ca0ebdd9264fb51990da98061704995f8b7` |
