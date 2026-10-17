# DIFF — worker-l11-enqueue-ladder (launch-11 §5.1 item 1: the enqueue-first ladder fix)

One replace-unit source revision, one file (`LuaMod.cpp`), five hunks, all
inside the `ExecuteInGameThread` registration, all `__PROSPERO__`-guarded (the
shipped PS5 branches; other platforms byte-identical to base). Diff and
round-trip receipts in `diffs/`, `work/patchcheck/`.

## Provenance chain (all sha256; verify with `shasum -a 256`)

| link | artifact | sha256 (first 16) |
|---|---|---|
| base | `worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp` | `04c9d91664dd82c7` |
| rev | `diffs/l11-enqueue-ladder.diff` (this worker) | `3b19068a194ec82d` |
| staged | `staging/luamod-l11-el-ladder/LuaMod.cpp` (base + rev; round-trip proven: `work/patchcheck/LuaMod.cpp` == staging) | `f50976ae1e2626b0` |
| control obj | `work/control-el-luamod.o` (base under the frozen argv) — byte-identical to the marshal-integration session's consumer object `854a476bdd4f9985…`: the frozen-argv route is path-independent | `854a476bdd4f9985` |
| fixed obj | `work/el-ladder-luamod.o` (staged under the frozen argv) | `8593884ea825d523` |

## Hunk 1 — EngineTick ladder, address-UNAVAILABLE branch (base ~5005)

`ensure_process_event_hooked(mod)` demoted to method selection only:

```diff
                     if (is_process_event_hook_available())
                     {
-                        LuaMod::ensure_process_event_hooked(mod);
+                        // [R3-L11EL] method selection only; the lazy detour arm
+                        // moved AFTER the enqueue (below), so a refused
+                        // registration can never discard the closure.
                         method = GameThreadExecutionMethod::ProcessEvent;
                     }
```

## Hunk 2 — EngineTick ladder, address-AVAILABLE `else` branch (base ~5022) — THE TRAP

Marshal-first when armed; no arm here:

```diff
                 else
                 {
+#if defined(__PROSPERO__)
+                    // [R3-L11EL] (worker-l11-enqueue-ladder, RUNBOOK 5.1 item 1):
+                    // address-availability is NOT registrability (R3D 4.5 trap).
+                    // In W_FAILS_R_WORKS the EngineTick address IS assigned
+                    // (vtable fallback) and the lazy arm below used to run here,
+                    // THROW before anything was enqueued, and bypass the armed
+                    // FuncPtr marshal entirely (main.lua:695 default method).
+                    // Marshal-first when armed: the marshal is the game-thread
+                    // pump in this world; no second EngineTick pump may be
+                    // attempted (the initializer deferred those registrations).
+                    if (is_funcptr_marshal_armed())
+                    {
+                        method = kFuncPtrMarshalExecutor;
+                    }
+                    // else: keep EngineTick; the lazy arm is attempted only
+                    // AFTER the closure is enqueued (order guarantee below).
+#else
                     LuaMod::ensure_engine_tick_hooked();
+#endif
                 }
```

## Hunk 3 — ProcessEvent ladder, address-UNAVAILABLE branch (base ~5034)

Mirror of hunk 1 for `ensure_engine_tick_hooked`.

## Hunk 4 — ProcessEvent ladder, address-AVAILABLE `else` branch

Mirror of hunk 2 (marshal outranks the address check; arm deferred below).

## Hunk 5 — THE ORDER GUARANTEE + loud stall log (new, post-enqueue, base ~5100)

Inserted after the `emplace_back` enqueue, still inside the binding (the
`recursive_mutex` is already held by this thread — recursive re-entry, same as
the RegisterHook path; the pump queues are guarded by it):

```diff
+#if defined(__PROSPERO__)
+            // [R3-L11EL] THE ORDER GUARANTEE (worker-l11-enqueue-ladder; RUNBOOK
+            // 5.1 item 1): the lazy detour arm runs only AFTER the closure has
+            // been enqueued, so a refused registration can never discard enqueued
+            // work. The queue survives; drain happens via whatever pump exists.
+            // Marshal-selected paths arm nothing here: the marshal is armed by the
+            // calibration consumer and its pump drains the game-thread queue; a
+            // second EngineTick/ProcessEvent pump is exactly what DIFF-2's
+            // deferred initializer registrations must not become. The
+            // recursive_mutex is already held by this thread (recursive re-entry,
+            // same as the RegisterHook path); the pump queues are guarded by it.
+            if (method == GameThreadExecutionMethod::EngineTick || method == GameThreadExecutionMethod::ProcessEvent)
+            {
+                TRY([&]() {
+                    if (method == GameThreadExecutionMethod::EngineTick)
+                    {
+                        LuaMod::ensure_engine_tick_hooked();
+                    }
+                    else
+                    {
+                        LuaMod::ensure_process_event_hooked(mod);
+                    }
+                });
+                // Loud stall honesty: a refused arm leaves the closure queued with
+                // no drainer. The closure is NOT discarded (enqueue-first); say so
+                // every time so "queued forever" is never silent.
+                if (method == GameThreadExecutionMethod::EngineTick && !LuaMod::m_is_engine_tick_hooked)
+                {
+                    Output::send<LogLevel::Error>(STR("[R3] ExecuteInGameThread: closure enqueued but the EngineTick pump is not registered (detour registration refused); it stays queued until a pump exists\n"));
+                }
+                else if (method == GameThreadExecutionMethod::ProcessEvent && !mod->m_is_process_event_hooked)
+                {
+                    Output::send<LogLevel::Error>(STR("[R3] ExecuteInGameThread: closure enqueued but the ProcessEvent pump is not registered (detour registration refused); it stays queued until a pump exists\n"));
+                }
+            }
+#endif
+
             return 0;
```

## What the fix does NOT touch

- `ensure_engine_tick_hooked` / `ensure_process_event_hooked` bodies (the throw
  sites) — hash-proven byte-identical across worlds
  (`host-test/regions/regions.json`: `ensure_hooks` `4abe43c787173260…` both).
- `marshal_state`, `process_simple_actions`, `availability`, `hook_state`
  regions — byte-identical across worlds (same file).
- Everything outside the `ExecuteInGameThread` binding, all other files, the
  initializer, and the driver machinery.
- The WithDelay family ladders (`WithDelay`/`Retriggerable…`/`AfterFrames`/
  `Loop…`, base 5108–5705) — still arm-before-enqueue; explicitly OUT OF SCOPE
  here (see ANALYSIS.md §5 / REPORT.md).

## Full diff text

`diffs/l11-enqueue-ladder.diff` (5 hunks, ~113 lines).
