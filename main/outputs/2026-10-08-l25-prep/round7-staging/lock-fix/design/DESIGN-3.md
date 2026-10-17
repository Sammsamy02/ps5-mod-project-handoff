# DESIGN-3: FAIRNESS-FIRST lock fix (runtime only, LuaMod.cpp only)

Author angle: keep the current structure (one shared lock for queues, Lua serialized by locks), but make every lock FIFO-fair, stop
re-taking the shared lock on every engine event (lock-free "work queued" check first), and never run Lua or raise a Lua error while the shared lock is held.
Console contact: none. No builds, no PRX, no edits to candidate/original. All claims below are **Mac-only** unless marked otherwise.
Citations: `C:` = candidate `src/LuaMod.cpp` (sha 4d6bc63b7a90 per MAP-1), `U:` = PC upstream RE-UE4SS `LuaMod.cpp`
(`.superpowers/native-dependency-spike/ue4ss-binding-research-1/upstream/RE-UE4SS/UE4SS/src/Mod/LuaMod.cpp`), `MAP-1/2` = `../map/MAP-1.md`, `../map/MAP-2.md`.

---

## 0. Plain summary (for the reviewer)

1. **Fairness alone is not enough, and this design says so up front.** Both mappers found that the best fit for B4 is a *leaked lock level*:
   four Lua bindings raise a Lua error (a `longjmp` on this port) while a `lock_guard` is alive, so the guard never unlocks
   (C:2346, 3063, 3106, 5947; MAP-1 "Verdict", MAP-2 section 4). A fair lock with a leaked level is just as dead as the old one.
   So the design has three layers: (L1) stop raising under a lock, (L2) a runtime net that reclaims a leaked level at the next Lua-call boundary,
   (L3) fairness plus a lock-free fast path plus "no Lua under the shared lock", which removes the starvation amplifier the reviewer named.
2. **Starvation (b) is real on the Mac, measured.** Host model (`hosttest-3/`, T2): a thread that releases and instantly re-takes a lock,
   against a waiter, gave the waiter **1 to 27 acquisitions in 2 s, worst wait 0.7 to 3.5 s with `std::recursive_mutex`**, versus
   1,475 to 1,671 acquisitions and worst wait 31 us to 1.8 ms with the ticket lock (10 runs, all passed; one earlier run saw 4.8 ms). Mac-only: the PS5 pthread
   scheduler was not measured. This is the pattern `NativeLuaActionDispatchScope` creates (C:4588-4612, used at C:5032/5470/5554/5637).
3. **Three lock classes, one order** (section 2): per-mod Lua lock `M_i` (long, fair, re-entrant) -> shared queue lock `Q` (leaf, short, fair, re-entrant) -> leaves.
   Lua for mod *i* only ever runs while the running thread holds `M_i`. That is what keeps each mod's `lua_State` family single-threaded (section 5).
4. **Keys can never freeze input polling again.** Key events go into a short-lock queue and are run by the same UpdateThread, using a
   bounded try-lock on `M_i`; if the mod is busy the event stays queued and is retried every 5 ms (section 3.6).
5. **Fast path:** every engine-event pump first does one lock-free atomic read; the queue lock is taken only when work was queued or a timed action is due
   (section 3.5). Host: 0.6-1.4 ns per event instead of 57-82 ns for the six recursive-mutex takes of one old drain (T5, Mac, not PS5).
6. **Scope:** one file (`LuaMod.cpp`), **zero new `#include`**, **zero header changes** (`LuaMod.hpp` is not touched; the old `m_thread_actions_mutex` stays declared and unused),
   no detours, mods untouched. Diff estimate about +900 / -250 lines in one file.
7. **B5 report data** comes from the `[LOCK] ` line (section 8). It also discriminates the two theories on the next launch: `leak_reclaimed > 0` proves the leak;
   a large key wait with `leak_reclaimed = 0` points to starvation or something else.

What the prototype proves and does not prove: it proves the primitive (FIFO order, recursion, try/timed, leak reclaim after longjmp, ABBA rule, flag protocol, fast-path cost).
It does **not** prove anything about PS5 scheduling, the real minimap trigger, or the early-lag cause.

---

## 1. Facts this design relies on (all Mac-only, cited)

| # | Fact | Where |
|---|---|---|
| F1 | `m_thread_actions_mutex` is ONE process-wide `std::recursive_mutex` (not per mod). 47 `lock_guard`/`unique_lock` sites in `LuaMod.cpp` and no use in any other candidate file | LuaMod.hpp:183 (pinned header, sha e52d29a0b165); `grep` of candidate tree |
| F2 | The dispatch scope takes it before the ownership check and holds it for the whole drain, including all game-thread Lua | C:4588-4612; used C:5032, 5470, 5554, 5637 |
| F3 | Upstream's pumps hold only two short guards and run Lua unlocked; upstream's keybind callback and NotifyOnNewObject (game-thread branch) DO hold the global mutex across Lua | U:4090-4160; U:1653; U:6631 |
| F4 | All of one mod's Lua states are `lua_newthread`s of one `m_lua` (= one `global_State`): main, async, hook states | C:1126 (`m_lua(new_state())`), C:1222-1262 (`make_hook_state`, `make_async_state`, `make_main_state`) |
| F5 | `ExecuteInGameThread` and the other bindings do `lua_xmove` + `luaL_ref` on the shared `hook_lua` stack while holding only the shared mutex | C:6216-6228 |
| F6 | The Lua core lock exists in the composed build: `lua_lock` -> `LuaLock`, one process-wide recursive pthread mutex, released by Lua while any C function runs | `LuaRaw/include/luaconf.h:796-800`; `ue4ss-native-lua-lock-final-1/src/platform_locks.c` |
| F7 | Lua errors are `longjmp` inside any active `pcall` (Lua built as C); destructors in skipped frames do not run | LuaMadeSimple.cpp:1557-1596 (MAP-2 section 4) |
| F8 | `L23GameThreadProbe::Poll()` is defined in `LuaMod.cpp` and called by the UpdateThread every loop (about 5 ms), before input polling | C:284 (`Poll`), UE4SSProgram.cpp:1796-1798, 1865, 1887 |
| F9 | The accepted `<atomic>` problem was in unit `lms-luamadesimple` (B2). `LuaMod.cpp` already includes `<atomic>` (C:9) and uses `std::atomic` (e.g. C:4640). I still use `__atomic_*` builtins in all new code, as the reviewer asked | `evidence/builds-20261012/B2-error-wrapper-memlog/build_variants.log` line 90-91 |
| F10 | Every Lua entry reaches Lua by `call_function`/`lua_pcall` at about 38 sites (table in section 5) | `grep` of C: |
| F11 | `get_mod_ref(lua)` reads a Lua global and can raise a Lua error, so it must not be used inside lock scopes | C:308-318 |

---

## 2. Lock model

```
Level 1  M_i   per-mod Lua lock (one per LuaMod; FairLock, recursive)       LONG: held while that mod's Lua runs, may span engine calls
Level 2  Q     shared queue lock (one; FairLock, recursive)                 LEAF-SHORT: queues, flags, hook containers. Nothing else inside.
Level 3  leaves: LuaLock (Lua core, released in C functions), per-mod m_actions_lock,
         m_funcptr_marshal_carrier_mutex, ScriptHookThunks registry mutex, Output write mutex, ThunkDetail
Order    M_i  ->  Q  ->  leaves.     Never the reverse. Never M_j while holding Q.
```

Rules (R1-R6). Each rule says how it is enforced.

* **R1 Q is a leaf.** Inside a `QueueGuard`: no Lua call, no Lua API that can raise, no Lua error raised, no `Output::send`, no I/O, no `join`/wait on another thread,
  no blocking acquire of any `M_j`. Enforced by (a) `lock_raise_gate.py` (static scan, section 9), (b) runtime: a `ModLuaEntry` created while the thread holds `Q`
  uses try-lock only and bumps `order_violations`.
* **R2 Lua entry holds `M_i`.** Every C++ -> Lua call for mod *i*, on every thread, is inside a `ModLuaEntry` for mod *i*. Re-entrant for the same thread (nested hooks
  inside one mod do not self-deadlock). Enforced by `lua_entry_gate.py`.
* **R3 Cross-mod nesting (ABBA rule).** A thread that already holds `M_x` and needs `M_y` (y != x), for example a hook of mod B firing while mod A's Lua runs on the same thread:
  * the **game thread** may wait for `M_y`, but only a bounded 20 ms, then the callback is skipped, counted (`nested_fail`/`hook_skip`) and logged once per 10 s;
  * **every other thread** may only `try_lock` (zero wait), and skips on failure.
  Why this is deadlock-free: a thread that blocks with a timeout never waits unboundedly; non-game threads never wait while holding a mod lock, so the only waiter is the
  game thread, and its holder (a non-game thread inside `M_y`) cannot be waiting for anything the game thread holds (it only try-locks). Host test T4 proves no deadlock and that the
  same pattern with plain blocking locks does deadlock (control).
* **R4 No waiting for another thread under a lock.** `uninstall()` joins the async thread before taking `M_i` (C:8416-8422 order kept). `ExecuteInGameThread` never waits for the game thread.
* **R5 Never raise a Lua error with a lock object alive in the frame.** Bindings validate and build error text *before* the guard, or end the guard scope and raise afterwards from a frame that
  holds only trivially destructible locals (fixed `char[256]` error buffer). This also removes the std::string/Bridge leaks the longjmp causes (C:5947-6043).
* **R6 The game thread never blocks unboundedly on a mod lock.** Drains use try-lock and defer the action; after 8 consecutive deferrals for one mod they escalate to a single bounded 5 ms wait.

Invariants proved or enforced:
* **I1** at any instant at most one thread is inside mod *i*'s `global_State` (R2).
* **I2** `Q` holds are short and contain no Lua (R1); max hold is reported (`qhold_ms`).
* **I3** a leaked lock level cannot outlive the Lua call that leaked it (layer L2).
* **I4** a pending queue item is never lost to the fast path (section 3.5).

---

## 3. Components (all new code is file-scope in `LuaMod.cpp`, inside `namespace RC`, before the `L23GameThreadProbe` block at C:172)

The block is delimited `// LOCKCORE-BEGIN` / `// LOCKCORE-END`. The same 229 lines are compiled by the host prototype (`hosttest-3/lockcore_proto.cpp`);
`lockcore_extract_check.sh` (section 9) compares the sha of the two blocks so "tested == shipped".
Includes needed by the block: `<chrono>`, `<thread>`, `<cstdint>`, `<cstddef>` (all already reachable in this TU). No new `#include`.

### 3.1 `FairLock` (FIFO ticket lock, recursive by owner token)

* Fields (all zero-initialised, usable from static storage): `next_ticket`, `now_serving`, `owner` (thread token = address of a `static thread_local char`), `depth`, `acquired_at_us`.
  Only `__atomic_*` builtins on the shared fields, as `LmsRelaxedCounter` does (LuaMadeSimple.cpp:22).
* `lock()`: if owner is me, `depth++`. Else `t = fetch_add(next_ticket)`, wait until `now_serving == t` (acquire). Waiting backoff: 64 `pause`/`yield`-hint spins, then `std::this_thread::yield()`,
  then after about 2000 spins `sleep_for(50 us)`. Waiters are served in ticket order. (This is the fairness fix.)
* `try_lock()`: CAS `next_ticket` from `s` to `s+1` only if `now_serving == s` (lock free and nobody queued). So a timed or try caller can never jump ahead of a queued waiter.
* `lock_for(us)`: `try_lock` with backoff until the deadline (a ticket cannot be abandoned, so timed callers do not queue; the escalation rule in R6 gives them a way to get a real ticket).
* `unlock()`: `--depth`; at zero clear owner, `now_serving = now_serving + 1` (release).
* `reclaim_to(d)`: owner only; unlock levels until `depth == d` (layer L2).

Open point (cannot be settled on the Mac): the 50 us sleep step depends on the PS5 `nanosleep` granularity; FreeBSD-derived kernels can round to a scheduler tick. Waiters only reach the sleep step after about 2 ms
of waiting, so the worst case is a hand-off latency, not a hang. The `[LOCK]` per-mod max wait will show it.

### 3.2 `QueueGuard` (replaces `std::lock_guard<std::recursive_mutex> guard{...m_thread_actions_mutex}` at all 47 sites)

* RAII over the global `FairLock g_queue_lock`. Records hold time at the outermost release (`q_hold_max`, `q_takes`).
* Two flavours: default = **mutator** (on release it sets the dirty mask, section 3.5); `QueueGuard::Quiet` = drain-internal / read-only (does not set it).
* Recursive, so the existing nested patterns (for example `ExecuteInGameThread` -> `ensure_*_hooked`, C:5901/5920) keep working.

### 3.3 `ModCell` (the per-mod lock) and how code finds it without touching Lua

* `static ModCell g_cells[32]` (static storage, never freed) plus `g_orphan_cell` for states that belong to no `LuaMod`. `ModCell` = `FairLock`, `char name[24]`, `closed`, `consecutive_defers`, wait statistics.
* The LuaMod constructor (C:1121-1126) stores `cell_index + 1` in the main state's **Lua extra space**: `*static_cast<uintptr_t*>(lua_getextraspace(m_lua.get_lua_state())) = idx+1`.
  `lua_getextraspace` is a macro in the vendored `lua.h:371`; `lua_newthread` copies the main thread's extra space into every new thread (`lstate.c:304-305`, same LuaRaw tree), so main, async and hook states all inherit it.
  Lookup `cell_of(lua_State*)` is two loads and a range check (`1..32`, otherwise `g_orphan_cell`), never touches the Lua stack, never raises (unlike `get_mod_ref`, F11).
  Garbage protection: `lua_newstate` does not zero extra space; the constructor writes it first, and the range check rejects any other value.
* No header change: the cell is not a `LuaMod` member. `uninstall()` sets `closed` under `M_i`; an entry that finds `closed` returns "not held" and the caller drops the action.

### 3.4 `ModLuaEntry` (RAII "I am about to run Lua for mod i") and its policies

| Policy | Used by | Behaviour when `M_i` is busy |
|---|---|---|
| `Block` | async threads (LoopAsync/ExecuteAsync/ExecuteWithDelay), `start_mod` main.lua, `uninstall`, `L23 Queue` | take a FIFO ticket, wait (fair). These threads hold no other lock |
| `GameDrain` | game-thread drains (simple actions, delayed actions, notify) | try-lock; on failure **defer** the action to the front of its queue and set the dirty bit; after 8 consecutive deferrals for this mod, one bounded 5 ms wait (counted as `esc`) |
| `KeyEvent` | keybind queue pump on the UpdateThread | wait up to 1 ms; on failure leave the event queued (retried every 5 ms); after 8 misses one bounded 50 ms wait |
| `Hook` | RegisterHook callbacks, script_hook, load-map / init-game-state / begin-play / end-play / console-exec callbacks, StaticConstructObject / NotifyOnNewObject | game thread: wait up to 20 ms; other threads: up to 5 ms; then **skip the callback**, count `hook_skip`, log `[LOCK] hook-skipped ...` once per 10 s. Expected count in B5: 0 |

Common behaviour:
* Recursion: if this thread already owns `M_i`, the entry just bumps the depth.
* Nesting rule R3 applies before the policy when the thread holds another mod's lock or `Q`.
* Fast path: try-lock first; timestamps (`now_us`) are read only on the contended path.
* Destructor: `reclaim_to(q_depth_at_entry)` on `Q` (layer L2), then unlock `M_i`.
* Statistics: per-mod max wait, `game_block_max`, `defers`, `escalations`, `hook_skips`, `nested_fail`, `order_violations`, `leak_reclaimed`.

### 3.5 Fast path: "work queued" mask, no lost wake-up

Shared state (file scope, `__atomic_*` only):

```
u32 g_q_dirty;            // bit0 simple_gt (game-thread/marshal queue + pending)   bit1 simple_et (engine-tick queue)
                          // bit2 notify    bits3..5 delayed (one bit per executor: ProcessEvent=0, EngineTick=1, Marshal=2)
i64 g_next_due_ms[3];     // earliest due time of a time-based delayed action for that executor; INT64_MAX if none
u32 g_frame_delays;       // number of frame-based delayed actions (EngineTick executor only)
```

Protocol:
* **Producers (every default `QueueGuard`)** set `g_q_dirty = all bits` with a release store *inside* the lock, just before the guard releases. This is conservative by construction: no per-site knowledge needed,
  and a forgotten enqueue site cannot lose work. (Hot sites may pass a narrower mask.)
* **Consumers (the pumps)**, first statement of `process_event_hook`, `funcptr_marshal_hook`, `VmtEngineTickDrain`, `engine_tick_hook`, after the existing thread gate:
  ```
  if (NativeLuaActionDispatching) return;                         // nested pump
  if (!pump_has_work(executor)) { stats.fast_skips++; return; }   // acquire load of g_q_dirty; now >= g_next_due_ms[e] only if a due time exists
  ```
  The existing identity publication, `m_funcptr_marshal_last_fire_ms` store, the 1 Hz `r3_marshal_pumped` event and `try_deferred_loadmap_arm()` stay before the fast-path return (the latter keeps its own atomic pending flag).
* **Drain, under `Q` (Quiet)**, swaps the queues out and **recomputes the bits and the due times exactly** from what is left (queue non-empty -> bit set; earliest `execute_at` of Active time-based actions; frame-based count).
  The dirty bit is cleared only here, under `Q`.
* **Why no lost wake-up:** `flag == 0` implies "queue was empty at the last exact recompute and no producer has released `Q` since". A producer sets the flag before it releases `Q`; a reader that sees the old 0
  simply treats the push as happening after its check and sees it on the next pump (one carrier call later, microseconds). A drain can clear a bit only while holding `Q`, where it sees every completed push.
  Host test T6: 3 producers x 100,000 items, every item consumed, 0.97-2.95 million lock-free skips vs about 90,000 lock takes, in 3 runs at `-O2` and 1 at `-O0`.
* A deferred action (mod lock busy) re-sets the bit, so it is retried on the next pump. Timed actions use `g_next_due_ms`, so the minimap's 1 ms frame loop costs about 1,000 `Q` takes per second instead of one dispatch-scope plus five takes per carrier call.

### 3.6 Key events: short-lock queue, same thread, bounded wait

* The `RegisterKeyBind` lambda (C:2227-2241) and the `RegisterKeyBindAsync` lambda (C:2085-2201): keep the `[l22] keybind callback entry` log line, then
  `QueueGuard { g_key_queue.push_back({cell, &lua, ref, key, press_us}); }` and `key_pump()` on the same thread.
* `key_pump()` (also called from `L23GameThreadProbe::Poll()`, every 5 ms, F8): swap the queue out under `Q`, release, then for each event in order: `ModLuaEntry{cell, KeyEvent}`;
  if not acquired, put the event and every later event of the same mod back at the front of the queue (per-mod order preserved) and continue with other mods; if acquired, record
  `wait = now - press_us`, run the Lua exactly as today (same `try/catch`, same error print), update `key_dispatched`.
* Parity: still runs on the UpdateThread (upstream UE4SSProgram.cpp:1229 -> Handler -> lambda). Difference: it no longer holds the shared lock (the upstream lock at U:1653), and when the mod is busy for more than 1 ms the callback is
  delayed (not lost) until the next 5 ms poll. The UpdateThread can never be stuck behind a mod lock for more than 1 ms (50 ms after 8 consecutive misses), so pad polling continues. This is the direct cure for the B4
  symptom "one `keybind callback entry`, then no more pad edges" (MAP-1 fact 2).
* Queue bound 256; overflow drops the newest and counts `key_drop`.
* Reported: `key_queued`, `key_dispatched`, mean/max press-to-start wait (press time = lambda entry; pad poll to lambda is at most 5 ms and not included, stated in the report).

### 3.7 The leak: three layers

* **L1 (cause).** Restructure the four bindings so no guard is alive when `throw_error` runs (R5):

  | Binding | C: lines | Change |
  |---|---|---|
  | `RegisterHook` | 5947-6050 | drop the function-wide guard (it was protecting Lua-state ops that `M_i` now protects, F5). `QueueGuard` only around `g_hooked_script_function_data.emplace_back` + hook-id assignment and around `m_script_hook_callbacks` updates. `StaticFindObject`, `RegisterPreHook/PostHook` and all error building stay outside. Errors go through `raise_buf(lua, ErrBuf)` after all C++ objects with destructors are gone |
  | `UnregisterHook` | 2346-2440 | same; guard only around the `scheduled_for_removal` mark and the `registry_indexes` edit |
  | `RegisterCustomEvent` | 3063-3099 | guard only around `find_function_hook_data`/`emplace_back` |
  | `UnregisterCustomEvent` | 3106-3118 | guard only around `remove_function_hook_data` |

  `ErrBuf` = `struct { char text[256]; }` filled with `snprintf` (`<cstdio>` already included, C:11). Message text stays identical to today's wording (mods and tests may match it); long names are truncated with an ellipsis.
* **L2 (net).** `ModLuaEntry` destructor calls `g_queue_lock.reclaim_to(depth_at_entry)`. Any leak from a missed site, an out-of-memory `luaD_throw` under a guard, or a future binding is repaired when that Lua call returns,
  logged as `[LOCK] leak-reclaimed levels=N mod=NAME` (rate-limited) and counted. Host test T3: leaked level visible inside the callback, repaired after it, other thread proceeds; the old model blocks the other thread for the whole 300 ms test.
  Limit: between the raise and the end of that callback the thread still holds `Q`, so other threads' enqueues wait; L1 makes this a never-case.
* **L3 (no Lua under Q).** The dispatch scope no longer holds `Q` (section 4, items 1-6), so even an unrepaired leak could only be one thread's recursion count, never "the game thread owns the shared lock during all Lua".

### 3.8 Statistics and the `[LOCK]` line

Section 8.

---

## 4. Site-by-site change list (candidate `LuaMod.cpp` line numbers)

| # | Site (C:) | Today | Change |
|---|---|---|---|
| 1 | 4588-4612 `NativeLuaActionDispatchScope` | unique_lock on the shared mutex for the whole drain | **No lock.** Keeps only the ownership logic: thread-local `NativeLuaActionDispatching` and `m_is_processing_actions` set/restored under a Quiet `QueueGuard` for the few instructions of the flag change. Class name and `owns_dispatch()` kept, so the four call sites do not change |
| 2 | 5017-5066 `process_event_hook` | scope on every ProcessEvent, then 2 more takes + 3 drains | thread gate, then `pump_has_work(ProcessEvent)` fast path (section 3.5), then scope, then drains. Merge block at the end becomes a Quiet guard + exact recompute |
| 3 | 5403-5519 `funcptr_marshal_hook` | scope taken before `owns_dispatch()` is checked, held across `try_deferred_loadmap_arm()` and all drains | keep identity/ticket/`last_fire`/1 Hz event/`try_deferred_loadmap_arm()` first, then fast path, then scope + drains. `try_deferred_loadmap_arm()` now runs with no queue lock held |
| 4 | 5540-5610 `VmtEngineTickDrain`, 5620-5665 `engine_tick_hook` | same as 3 | same as 2/3. The 128-action cap and 4 ms time box stay; unrun work keeps its dirty bit |
| 5 | 4696-4745 `process_simple_actions` | Lua runs after the swap-out but inside the scope, `luaL_unref` outside any lock | per action: `ModLuaEntry{cell_of(lua), GameDrain}`; on failure push to `deferred` (and defer every later action of the same mod in this batch); `luaL_unref` inside the entry; deferred actions go back to the **front** of the queue (keeps per-mod FIFO; today they are appended). Count `game_lua_us`, `eigt_run` |
| 6 | 4747-4860 `process_pending_notify_on_new_object_callbacks` | Lua (C:4826) inside the guard block 4760-4860 | under Quiet `Q`: build `to_execute` as plain data and release; per callback `ModLuaEntry{..., Hook}`; run Lua; collect `callbacks_to_remove` and `thread_refs_to_unref`; apply the `erase_if` under `Q` afterwards and `luaL_unref` inside the entry. Parity difference documented in section 7 |
| 7 | 4864-5013 `process_delayed_actions<Executor>` | readiness scan under the guard, Lua after (inside scope), final block does `luaL_unref` under the guard | scan under Quiet `Q` also recomputes `g_next_due_ms[e]`; each ready action runs under `ModLuaEntry{..., GameDrain}`; if not acquired the action goes back from `Executing` to `Active` (no loss, retried next pump); final block moves the `(state, ref)` pairs of erased actions to a local list, releases `Q`, then unrefs each inside an entry for its mod (if an entry cannot be taken, append to a small `g_deferred_unref` list drained at the next pump) |
| 8 | 5901, 5920 `ensure_engine_tick_hooked` / `ensure_process_event_hooked` | own guard; also called under the binding's guard | unchanged guards (default mutator guard); `ExecuteInGameThread` calls them after releasing its own guard |
| 9 | 6216-6350 `ExecuteInGameThread` | one guard from `make_hook_state` through the `[MEM]` diagnostics (`lua_gc`, `Output::send`) and `ensure_*_hooked` | `make_hook_state`, `lua_xmove`, `luaL_ref` now run under the caller's `M_i` (already held, F5); `QueueGuard` only around the two queue pushes and the depth read; `[l22]`/`[MEM]` prints and `lua_gc` happen after release, still 1 Hz; `eigt_queued++` |
| 10 | 6493, 6547, 6699, 6800, 6952, 7027 (delayed-enqueue bindings), 7080-7736 (handle utilities) | guard around the whole body | guard only around the vector edit; mutator mark. `eigt_queued++` on the enqueue bindings |
| 11 | 2085-2201, 2227-2241 keybind lambdas | async variant: no lock; sync variant: shared lock held across Lua | section 3.6 |
| 12 | 2346, 3063, 3106, 5947 | raise under guard | section 3.7 L1 |
| 13 | 265 `L23GameThreadProbe::Queue` | shared lock across `lua_pcall` | `ModLuaEntry{cell_of(L), Block}` around the pcall; `Q` not held |
| 14 | `L23GameThreadProbe::Poll` (C:284) | probe timeout check | additionally `key_pump()` and `lock_stats_maybe_emit()` |
| 15 | 364-700 `lua_unreal_script_function_hook_pre/post` | no lock (as upstream) | `ModLuaEntry{cell_of(lua_data.lua state), Hook}` around the body; skip + count on failure |
| 16 | 5280-5360 load-map, 8907-8996 init-game-state/begin-play/end-play, 9128-9674 console/exec/command callbacks | no lock (as upstream) | `ModLuaEntry{..., Hook}` around each `call_function`. The unlocked iteration over the callback vectors stays as upstream (a pre-existing race with registration, not part of this fix) |
| 17 | 8612 `script_hook`, 9007 StaticConstructObject post-callback | guard held across Lua | `Q` only for matching/copying the callback list; Lua under `ModLuaEntry{..., Hook}`. Fast path for 9007: relaxed read of a callback count, skip entirely when 0 (this callback runs for every constructed object) |
| 18 | 8228-8296 `load_and_execute_script`, `start_mod` 8301-8345 | no lock | `ModLuaEntry{this cell, Block}` around the whole main script; async threads of that mod wait their turn |
| 19 | 8396-8560 `uninstall` | guard held across teardown; waits for in-flight dispatch via the scope's lock | async join first (unchanged), then `ModLuaEntry{..., Block}` around `fire_on_lua_stop_for_self` and `lua_close`; set `cell->closed` before `lua_close`; `Q` only for the `erase_from_container` calls; the same-thread-dispatch refusal check (thread-local) is kept; also emits `[LOCK] final` |
| 20 | 9704-9790 `update_async` / `process_delayed_actions()` | LoopAsync Lua unlocked | one `ModLuaEntry{this cell, Block}` per due action (not per pass) around `async_lua()->call_function`; also calls `lock_stats_maybe_emit()` |
| 21 | 1121-1126 constructor | - | install cell index into extra space |
| 22 | 1165 `global_uninstall` | - | emit `[LOCK] final` |
| 23 | all other `lock_guard<recursive_mutex>` (about 25 sites: the 47 minus the rows above) | mutex | mechanical rename to `QueueGuard` |

Not changed: `LuaMod.hpp`, `UE4SSProgram.cpp`, `Handler.cpp`, `ScriptHookThunks.cpp`, `LuaMadeSimple.*`, `platform_locks.c`, every mod. Integration note: the staged copies of `LuaMod.cpp` in
`round5-staging/{B2-error-wrapper,B3-iterators,lua-alloc}/` and the world-fix patches `03-map-logger.patch` / `04-event-map-logger.patch` quote the old `lock_guard` line as context (MAP search: 47 sites each).
This change must be generated as a patch **on top of the final merged `LuaMod.cpp`** (last in the order), not merged into those branches.

---

## 5. How Lua stays single-threaded per mod

Claim I1: for each mod *i*, at most one thread executes anything that touches its `global_State` at a time.

* All of mod *i*'s states (main, async, hook, coroutines) share one `global_State` (F4) and one `ModCell` (extra-space inheritance, section 3.3).
* Every site where C++ starts Lua (F10) is wrapped in a `ModLuaEntry` for that mod (table below). Bindings called *from* Lua run on a thread that is already inside an entry,
  so their stack and registry work (`lua_xmove`, `luaL_ref`, `make_hook_state`) is covered by `M_i`. This is the property the old global mutex tried to provide around some bindings only (F5) and never provided for
  LoopAsync, hook callbacks, load-map/console callbacks, or main.lua.
* The Lua core lock (`LuaLock`, F6) remains underneath as the innermost leaf. Order: `M_i` -> `Q` -> `LuaLock` is acyclic; no code path takes `M_i` while inside a Lua core operation, because Lua
  releases `LuaLock` around every C function (the only way into our code from Lua).
* Residual risk (not caused by this change): `LuaLock` is ONE process-wide, **unfair** pthread mutex, so a long pure-Lua stretch on one thread can still delay another mod's thread at its first Lua API call.
  If B5 shows large game-thread Lua time with all `[LOCK]` mod-lock waits near zero, the next suspect is `platform_locks.c`, which could be given the same ticket lock.

| Entry point (C: call site) | Thread | Policy |
|---|---|---|
| Simple game-thread actions (4725), delayed actions (4952), pending notify (4826) | game thread (published identity) | GameDrain / Hook |
| Hook pre/post callbacks (456, 654), script_hook (8702), StaticConstructObject (9042) | thread executing the UFunction / constructing the object (can be a worker) | Hook |
| Load-map (5292, 5347), init-game-state/begin-play/end-play (8907-8996), console exec / command callbacks (9128-9674) | engine hook thread (game thread in practice) | Hook |
| RegisterKeyBind / Async (2235, 2111) | UpdateThread | KeyEvent |
| LoopAsync / ExecuteAsync / ExecuteWithDelay (9757, 9763) | per-mod async thread | Block |
| main.lua (8277), on-unload (8397), L23 queue pcall (269) | UpdateThread | Block |
| `lua_pcall`/`lua_call` at 215, 258, 946, 1555 | already inside a Lua call (C function run by Lua) | covered by the outer entry |

---

## 6. ABBA and re-entrancy (reviewer addition 2)

* **Same mod, nested** (a hook of mod A fires while A's Lua runs on the same thread): `ModLuaEntry` finds `owned_by_me()` and bumps the depth. No wait.
* **Two mods, opposite order** (mod A on the game thread triggers B's hook while B's async code triggers A's hook): rule R3. Test T4 reproduces exactly this with 300 iterations on two threads:
  both finish in 150-190 ms, no deadlock, the async side's nested attempts are skipped (`nested_fail = 300..301`, it only try-locks); the control with plain blocking locks deadlocks (stuck threads 1 or 2), so the test is sensitive.
  Note for the reviewer: in that adversarial setup the async side never gets A because the game thread holds A almost continuously; in real mods nested cross-mod hooks from a non-game thread are rare
  (hook callbacks run where the UFunction runs; workers rarely call hooked Blueprint functions). Every such skip is counted and logged, so B5 will show it.
* **Safe fallback if the rule proves too strict:** replace "skip" by "release the outer `M_x`, requeue the callback to the game queue". Not chosen now because pre-hooks must run synchronously.
* **Shared hook state:** all of a mod's hook callbacks and game-thread closures run on the mod's single `hook_lua` stack. Today two threads can push onto it at once (async `ExecuteInGameThread` vs a running closure).
  After this change they cannot (I1).

---

## 7. PARITY (reviewer addition 3): upstream thread and locking vs this design

Sources: `U:` PC upstream file (above). The `original` tree (`typescan-r4/original`) is the earlier port (l23), not upstream; it already contains the dispatch scope (O:4229, per MAP-1).
"Differs" rows are the **deliberate differences**; everything else is identical to upstream.

| Entry point | Upstream thread / lock (U:) | Candidate B4 | This design | Deliberate difference and reason |
|---|---|---|---|---|
| `RegisterKeyBind` callback | UpdateThread (event loop, U UE4SSProgram.cpp:1229); shared mutex held across the Lua call (U:1651-1665) | same, plus entry log (C:2227-2241) | UpdateThread; no shared lock; per-mod `M_i` with 1 ms bounded wait, else queued and retried at 5 ms | **Differs.** A busy mod delays the key (<= 5 ms normally) instead of freezing all input polling. Reason: B4 symptom. Order per mod preserved |
| `RegisterKeyBindAsync` callback | UpdateThread, no lock (U:1512-1620) | same (C:2085-2201) | UpdateThread, `M_i` like sync | **Differs** only by gaining `M_i` (I1). Name "Async" in upstream does not mean another thread (Mac reading of C:2085-2201: the lambda is called by the Handler) |
| `LoopAsync`, `ExecuteAsync`, `ExecuteWithDelay` Lua | per-mod async thread; no shared lock (U:7230-7313) | same (C:9704-9786) | same thread, plus `M_i` per due action | **Differs** by gaining `M_i` (I1); the 5 ms loop cadence, registration locks (per-mod `m_actions_lock`) and callback order unchanged |
| `ExecuteInGameThread` enqueue | caller thread; shared mutex around make_hook_state + enqueue (U:4366-4368) | same, plus diagnostics under the lock (C:6216-6310) | caller thread; `M_i` (already held) for Lua-state ops; `Q` only for the push; diagnostics outside | **Differs** only in lock placement. Reason: `Q` is a leaf |
| Game-thread closure run | game thread via ProcessEvent / EngineTick hook; swap-out under short guard, Lua unlocked (U:3796, 4090-4160) | scope holds shared mutex across everything (C:4588-4612) | game thread via the same pumps; swap-out under `Q`, Lua under `M_i` only | **Matches upstream** on the shared lock; **adds** `M_i`. Defer-on-busy is new (upstream never meets contention because it has no mod lock) |
| `ExecuteInGameThreadWithDelay` and all `LoopInGameThread*`, handle utilities | caller thread; shared mutex around the vector edit (U:4465-5524) | same | same, guard shrunk to the edit | none observable |
| Hook callbacks (`RegisterHook` pre/post) | thread executing the UFunction; **no** lock (U:132-.., no `m_thread_actions_mutex` in range) | same | same thread; `M_i` (Hook policy, bounded wait, skip) | **Differs** by gaining `M_i`. Skip after 20 ms (game) / 5 ms (worker) is new; expected never. Reason: I1 and deadlock safety |
| `RegisterHook` registration | caller; shared mutex across the whole binding, raises under it (U:4199-4244) | same shape; leaks on PS5 (F7) | guard narrowed, raise outside | **Differs** (fixes the leak). Error text unchanged |
| `UnregisterHook`, `RegisterCustomEvent`, `UnregisterCustomEvent` | same shape as above (U:1748, 2369, 2409) | same | same fix | as above |
| `NotifyOnNewObject` delivery | game-thread branch runs Lua **under** the shared mutex; other threads queue; the pending queue is drained under the mutex too (U:3849, 6631) | same (C:4761, 9007); not registered in the R3 world (MAP-2) | collect under `Q`, Lua under `M_i` outside `Q` | **Differs.** Lua no longer runs under the shared lock. Callback ordering, cancel-by-return-true, object re-validation unchanged |
| `script_hook` (non-native UFunction hooks) | game thread; mutex held for the whole callback (U:6295) | same (C:8612) | `Q` for list copy, Lua under `M_i` | **Differs** as above |
| Load-map / init-game-state / begin-play / end-play / console callbacks | engine hook thread; no lock | same | `M_i` Hook policy | **Differs** by gaining `M_i` only |
| `start_mod` / main.lua | UpdateThread; no lock | same | UpdateThread; `M_i` Block for the whole script | **Differs.** The mod's own async loop and game-thread closures wait until main.lua finishes (they used to interleave with it) |
| `uninstall` | UpdateThread; join async first, then shared mutex across teardown (U:6098-6199) | same + same-thread-dispatch refusal | join first, `M_i` Block for stop callback and `lua_close`, `Q` for container erase | **Differs** in lock set; waits for the mod's own in-flight Lua exactly (the old scope made it wait for *any* mod's drain) |
| Fast path on engine events | none; two short takes per ProcessEvent / EngineTick (U:4098, 4108, 4135, 4145) | scope + 5 takes per carrier call | one atomic load; takes only when work or due | **Differs** (performance only); delivery order and timing semantics unchanged. Timed actions keep their `execute_at` rules |
| Lock fairness | `std::recursive_mutex` (unfair) | same | FIFO ticket for `M_i` and `Q` | **Differs** by design (this angle) |
| Which thread runs what | as above | as above | **unchanged** for every entry point | none |

Invariants that must stay true (parity checks for the host tests and the B5 report): game-thread closures run on the published game thread only; LoopAsync runs on the mod's async thread;
keybinds run on the UpdateThread; hook callbacks run where the UFunction runs; `NotifyOnNewObject` Lua runs on the game thread.

---

## 8. Lock statistics: the `[LOCK] ` line (reviewer addition 4)

* Emitter `lock_stats_maybe_emit(force)`: `compare_exchange` on `g_last_emit_ms`, at most one line per 10 s. Called from three places so a stuck thread cannot silence it:
  `funcptr_marshal_hook` (reuses the `pump_now_ms` it already computes, C:5461-5470), `L23GameThreadProbe::Poll()` (UpdateThread), and each mod's `update_async` loop. `LuaMod::global_uninstall()` (C:1165)
  and each mod's `uninstall()` call it with `force` and prefix `[LOCK] final`. Output goes through `Output::send(STR("[LOCK] {}\n"), ensure_str(buffer))` outside every lock.
* Counters are cumulative; the `max` values are for the 10 s window (the `final` line carries whole-run maxima). All values live in a file-scope `Stats` of `__atomic` `u64`s.
* A startup line `[LOCK] init cells=<n> fair=1 fastpath=1` is printed once (marker for verify_build).

Format (one line):
```
[LOCK] t=+612s keys q=3 d=3 pend=0 drop=0 wait_ms max=1.4 mean=0.3 | q takes=4210 skips=1853022 hold_ms max=0.21 | modwait_ms max minimap=2.1 zzdiag=0.4 ueselftest=0.0 |
       eigt q=87 run=87 | game lua_ms=412 act_max_ms=8.4(minimap) block_ms max=0.9 | defer=12 esc=0 hook_skip=0 nest_fail=0 leak_reclaimed=0 order_viol=0 | rearm n=2 max_ms=3.1
```
Field list requested by the reviewer, mapped:
* key events queued / dispatched -> `keys q= d=` (plus `pend`, `drop`)
* max and mean wait key press -> callback start -> `wait_ms max= mean=`
* max queue-lock hold -> `q hold_ms max=`
* per-mod Lua-lock max wait -> `modwait_ms max name=...`
* fast-path skips vs lock takes -> `q takes= skips=`
* ExecuteInGameThread queued vs run -> `eigt q= run=`

Extras (cheap, and they answer B5 questions): `game lua_ms` (game-thread time inside Lua per window), `act_max_ms` (longest single Lua action and its mod), `block_ms max` (longest time the game thread waited for any of our locks),
`defer/esc/hook_skip/nest_fail`, `leak_reclaimed` (the leak discriminator), `order_viol`, and `rearm n/max_ms` (time spent in `r3_marshal_arm_event_carriers`, C:5760-5800, which walks the whole `GUObjectArray` on the *caller's* thread and can run again whenever armed
carriers have been silent for 1 s, C:5873-5893).

### CHANGES-ROWS.md declaration (for verify_build; same shape as `round5-staging/ROUND5/additions/markers-add-round5.json`)

```
id: lock-stats            canonical_marker: "[LOCK]"
required:
  - text "[LOCK] "                      encodings [utf-16le]   source typescan-r4/candidate/src/LuaMod.cpp (emit function)
  - text "[LOCK] init "                 encodings [utf-16le]
  - text "[LOCK] final "                encodings [utf-16le]
  - text "[LOCK] leak-reclaimed "       encodings [utf-16le]
  - text "[LOCK] hook-skipped "         encodings [utf-16le]
change row: LuaMod.cpp candidate_sha256 refreshed; no added headers; no added files; includes: none added
gate expectations: marker count > 0 for each entry (a gate that finds 0 fails)
```
The shipped PRX must contain all five UTF-16LE literals; `verify_build.sh --manifest` fails otherwise.

B5 report fields this feeds: F7 / L3+R3 spawner (`keys q/d`, `wait_ms`, plus the existing `[l22] keybind callback entry` lines now followed by the spawner's `ExecuteInGameThread queued`), minimap and stutter (`game lua_ms`, `act_max_ms`, `block_ms`,
`q hold_ms`, `modwait_ms`), self-test timings versus the 10 ms target (existing `[ST2]` lines; they can now run because the AUTO `ExecuteInGameThread` is no longer blocked), and the lock statistics themselves.

---

## 9. Tests and gates

### Done (Mac, Apple clang 17.0.0, arm64; `hosttest-3/`; one compile at a time; vm_stat showed 3.3 to 4.8 GB free before each)

`lockcore_proto.cpp`, 14 checks, `checked=14 failed=0` at `-O2` and `-O0` (`run-O2.txt`, `run-O0.txt`, `SHA256SUMS.txt`, `toolchain.txt`). Honest record of the loop runs: 3 of the first 14 runs hung, all inside the
control part of T4, which used a `std::timed_mutex` timed wait in my own *test control* (the lock core printed its T4 result line before the hang). I rewrote the control with polling `try_lock`; after that 13 of 13 runs
(10 + 3 with T6 added) and the final `-O0` run passed. I did not root-cause the `timed_mutex` hang; it is in `libc++` test scaffolding, not in the block proposed for `LuaMod.cpp`.

| Test | What it shows (Mac-only) |
|---|---|
| T1 | recursion depth 2, other thread cannot `try_lock`, 4 queued threads are served in ticket order |
| T2 | starvation model: `std::recursive_mutex` waiter worst wait 0.7-3.5 s (1-27 acquisitions in 2 s); ticket lock worst wait 31 us-1.8 ms (about 1,470-1,670 acquisitions); 10 loop runs plus the saved runs; the 5 ms check threshold was exceeded once (4.8 ms) in an earlier run |
| T3 | **stuck-key repro**: old model (longjmp past a `lock_guard` on the "game" thread, then the game thread keeps re-entering) keeps the "keybind" thread blocked for the whole 300 ms; new model: leak visible inside the callback (`depth=1`), reclaimed when the entry scope ends (`leak_reclaimed=1`), keybind thread then gets both locks |
| T4 | **ABBA** between two mods from two threads, plus a control that deadlocks |
| T5 | fast path cost: 0.57-1.35 ns/event vs 57-82 ns for six `recursive_mutex` takes (single thread, not PS5; the old per-carrier cost was never the main lag source, see section 11) |
| T6 | flag protocol: 3 producers x 100,000 items, all consumed, no item lost, lock-free skips 0.97-2.95 million vs about 90,000 queue-lock takes |

### To do when this design is picked (each is a new Mac test or gate; none needs the console)

* **T7 real-Lua stuck-key repro:** reuse `map/hosttest-longjmp/` (Lua C sources + `platform_locks.c`, `build.sh` already fixed and guarded by the 2 GB check) with a C function that takes a `QueueGuard` and calls `luaL_error`
  under a Lua `pcall`; assert reclaim at the end of the entry and that a second thread then runs its keybind. One compile, after `vm_stat` shows more than 2 GB free.
* **T8 two mods firing hooks at each other** on top of the real LOCKCORE block with a fake `call_function` (the T4 pattern, plus key queue and 2 async threads, 10 s soak).
* **T9 key queue:** order per mod, deferral and retry on 5 ms poll, overflow drop, no event lost when `M_i` is held 100 ms.
* **T10 uninstall race:** `closed` entry returns not-held while another thread waits; no use after `lua_close`.
* **TSAN run** of T1-T9 if the toolchain's sanitizer runtime links.
* **Gates (each prints the number of items checked and exits non-zero when that is 0, per CLAUDE.md section 4):**
  * `lock_raise_gate.py`: scans `LuaMod.cpp` for `throw_error` / `throw_lua_error` / `luaL_error` / `lua_error` lexically inside a `QueueGuard` or `ModLuaEntry` scope; also flags `Output::send`, `call_function`, `lua_pcall`, `.join(` inside `QueueGuard` scopes. Prints scopes scanned.
  * `lua_entry_gate.py`: every `call_function(` / `lua_pcall(` / `lua_call(` site must sit in a function that creates a `ModLuaEntry` or is on the allowlist of "already inside a Lua call" sites in section 5. Prints sites checked (expected about 38).
  * `lockcore_extract_check.sh`: extracts the `LOCKCORE-BEGIN..END` block from `LuaMod.cpp` and from `lockcore_proto.cpp`, compares sha256, prints line count, fails if either block is empty.
  * Existing: `fs_throw_gate.py`, `no_detour_gate.py` before compile; include-graph gate (no new include -> no additions; the new code uses `__atomic_*` only); `verify_build.sh --manifest` for the exact PRX; the new `[LOCK]` markers above.
* **Console proof (B5, after approval, one launch):** the expected sequence is `[LOCK] init`, then ten-second lines; F7 / L3+R3 produce `[l22] keybind callback entry` followed by `[l22] ExecuteInGameThread queued`; `leak_reclaimed=0` if L1 is complete; a non-zero value
  names a site we missed. No claim about the console is made in this document.

---

## 10. Risks, unknowns, costs

1. **New lock primitive in production code.** Mitigated by the host tests and by keeping it in one marked block; a fallback is to keep `std::recursive_mutex` for `Q` (the fast path removes the re-taking pattern T2 models) and use the ticket lock only for `M_i`.
2. **Extra-space inheritance** (section 3.3) is verified in the vendored C source (`lstate.c:304-305`, `lua.h:371`) but not run against the PS5 Lua build. T7 should assert `cell_of(hook_lua) == cell_of(m_lua)` on the real Lua objects.
3. **`Hook` skip fallback** changes behaviour only after a 20 ms stall; it is counted. If B5 shows `hook_skip > 0` the cause is a long Lua action on another thread holding the same mod.
4. **`start_mod` now serializes main.lua with the mod's own async loop and closures.** A main.lua that runs for seconds delays them. PC mods do not rely on interleaving with their own startup (the interleaving was an accident of upstream; Mac reading, not proven for every mod).
5. **Per-mod FIFO on defer** relies on inserting deferred actions at the front of the queue; today they are appended (C:4737). Behaviour is identical when only one mod is busy.
6. **`m_is_currently_executing_game_action` is still a plain static bool** (game-thread only). Unchanged.
7. **Residuals not fixed here:** unlocked iteration of the load-map/begin-play/console callback vectors (upstream race); `LuaLock` unfairness (section 5); `ExecuteInGameThread -> r3_marshal_lazy_arm` can do a full object walk on the caller thread (`rearm` counter added to see it).
8. **After the leak is fixed the minimap's three `pcall(RegisterHook, ...)` calls (main.lua:512-519) will probably still fail** if `StaticFindObject` cannot find those Blueprint events; that is the correct, PC-identical error, but the minimap's door / interact / hide events may stay dead. Separate capability issue, outside lock work (Mac inference from missing "Registered" lines; MAP-1 fact 7).
9. **Keyboard F7 / F12** cannot reach `RegisterKeyBind` on PS5 at all (no keyboard source, MAP-1); only pad chords work. Unchanged by this design.
10. **Merge order:** apply last (section 4 note). Diff about +900 / -250 in `LuaMod.cpp`; new includes: none.
11. **Rollback:** the old behaviour is recoverable by archived B4 (sha in the build ledger); nothing here changes other units.

---

## 11. The early lag (about 2 minutes) - what this design does and does not claim

* Not explained by the lock map (MAP-2 section 6). Candidates there: synchronous log lines from the minimap UI build (1,115 in one second at 06:59:34; 6,698 total), the 66-carrier re-arm, and 18 s with no pump at all (06:59:15-06:59:33, log 933).
* The old per-carrier lock cost is **not** a plausible main cause: six uncontended takes are about 60-80 ns on the Mac model (T5), so even tens of thousands of carrier calls per second would be around 1 percent of one core. (Mac figure; PS5 not measured.)
  What the old scope did add is *latency for other threads* (keys, async enqueues), which shows up as late reactions, not frame time.
* This design therefore does not promise to cure the lag. It adds the measurement needed to explain it on the next launch: `game lua_ms` and `act_max_ms` (is the game thread busy in Lua?), `block_ms max` (does it wait on our locks?), `q hold_ms max`,
  `rearm n/max_ms` (object walks on the caller thread), plus the existing pump gap diagnostics. If `block_ms` and `hold_ms` are small and `game lua_ms` is large, the lag is mod Lua work and log volume, not locking.

---

## 12. Questions for the reviewer

1. Accept the `Hook` skip fallback (skip after 20 ms / 5 ms) or prefer "wait unbounded on the game thread only" (simpler, but reintroduces a deadlock path if a worker holds `M_y` while waiting for the game thread)?
2. Should `LuaLock` (`platform_locks.c`) get the same ticket lock in the same build? It is a separate unit and is not required for the B4 symptom.
3. Is a fast-path flag per executor (as designed) wanted, or is a single mask acceptable (smaller diff, one extra drain per mixed workload)?
4. Is the `start_mod` serialization (risk 4) acceptable for the Townfall mod set?

Files written for this design: this file and `hosttest-3/` (`lockcore_proto.cpp`, `run-O2.txt`, `run-O0.txt`, `SHA256SUMS.txt`, `toolchain.txt`). No other file was created or changed.
