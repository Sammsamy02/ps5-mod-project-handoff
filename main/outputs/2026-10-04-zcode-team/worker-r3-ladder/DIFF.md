# DIFF — worker-r3-ladder (the §5.1 WithDelay-family ladder re-order)

Base: `worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp`
(`c92c92ff66686bf6…`, the pinned current-tree LuaMod).
Staged: `staging/luamod-l11-r3-ladder/LuaMod.cpp`
(`6f97f8a1727b7e49…`, 8463 lines, +121 vs base).
Diff: `diffs/l11-r3-ladder.diff` (`29535f023c0105f3…`, 6 hunks, ONE file).
Round-trip proven both directions (byte-exact).

## Intent (RUNBOOK l11-marshal-integration §5.1 item 1, the remainder)

In a walk-permitted world the initializer's vtable fallback assigns
`TickInternal` (`e7f349af…` :2051-2054/:2060-2063), so
`is_engine_tick_hook_available()` — an ADDRESS check
(`UE4SSRuntime.cpp:11-16`, `TickInternal.is_ready()`) — reads true while the
detour registration is refused (write-gate). The WithDelay family still ran
`ensure_engine_tick_hooked()` BEFORE the enqueue at its availability-first
rungs, so the first delay-API call threw
`EngineTick callback registration failed` (LuaMod base :4820-4822) with the
closure discarded and the armed marshal bypassed. The EL worker already fixed
exactly this for the default `ExecuteInGameThread` (their declared scope);
their REPORT names this family as the open follow-up. This diff completes it.

## The 6 hunks

1. **Helper (staged :4180-4199)** — `funcptr_marshal_usable()` under
   `#ifdef __PROSPERO__`, next to `is_funcptr_marshal_armed()`:
   `armed && Unreal::R3MarshalPumpPending()`. The rev-B pairing law as a
   predicate: the marshal is the pump only while the initializer deferred its
   registrations. Deliberately NOT a "drain succeeded" test — between arming
   and the first carrier invocation publication is legitimately pending
   (queueing then is the designed lazy semantics, R3D 4.4a); publication
   refusal stays the drain body's own fail-closed gate (`funcptr_marshal_hook`,
   base :4566-4574). `armed && !pump_pending` is treated as the PROVABLY DEAD
   drain.
2. **ExecuteInGameThreadWithDelay (staged :5236-5261)** — top-gate inserted
   directly after `GameThreadExecutionMethod method = …`: usable ⇒
   `method = kFuncPtrMarshalExecutor` (BEFORE any availability check or
   ensure); armed-but-dead ⇒ `lua.throw_error("…: R3 FuncPtr-marshal carrier
   is armed but its drain is dead (pump_pending=false); refusing to queue onto
   a dead pump and refusing to arm a second EngineTick pump")`. The OLD
   availability-first ladder below is untouched and now only reachable when
   the marshal is NOT armed — its third-rung throw (no backend at all) is the
   only remaining unhooked-tick throw.
3. **RetriggerableExecuteInGameThreadWithDelay (staged :5446-5471)** — same.
4. **ExecuteInGameThreadAfterFrames (staged :5621-5636)** — usable ⇒
   `lua.throw_error("…: frame-based delays need an EngineTick pump; only the
   R3 FuncPtr-marshal pump exists in this world and its drain cannot count
   frames")`. The marshal drain deliberately skips frame-based actions
   (`process_delayed_actions`, base :4376-4385 "frame-based delays remain
   EngineTick-only"), so refusing precisely beats both the forbidden detour
   attempt and a silent forever-stall. Non-marshal worlds keep the existing
   behavior byte-for-byte.
5. **LoopInGameThreadWithDelay (staged :5700-5725)** — same as hunk 2.
6. **LoopInGameThreadAfterFrames (staged :5848-5863)** — same as hunk 4.

## What deliberately did NOT change

- `ensure_engine_tick_hooked` / `ensure_process_event_hooked` (the throw
  sites) — hash-proven byte-identical across the lineages
  (`host-test/regions/regions.json`, `ensure_hooks` equal).
- The EL worker's `ExecuteInGameThread` fold (already marshal-first when
  armed) — untouched.
- The enqueue-first order guarantee, drain bodies, queue types, and the
  W-works (no marshal) behavior of every function.
- Residual, OUT OF SCOPE (documented, distinct concern from §5.1): when the
  marshal is NOT armed and the tick address is assigned, the WithDelay family
  still runs `ensure_engine_tick_hooked()` BEFORE the enqueue, so a refused
  registration still discards that closure (the EL enqueue-first discipline
  has not been ported to this family). This revision is the §5.1
  marshal-bypass trap only; the enqueue-first port would be its own distinct-ID
  revision. In the R3 world with a USABLE marshal that path is unreachable
  (the top-gate routes first) — which is the launch-11 candidate's world.
