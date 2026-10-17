# DESIGN-2: route key presses to the game thread, split the locks, add a no-lock fast path

Angle: ROUTE-TO-GAME-THREAD. Status: design plus a small Mac host prototype. No console contact, no PRX build, no edits to the candidate or original trees. Every claim is **Mac-only** unless a line is marked `[console B4]`, which means "from this morning's B4 launch log" (evidence tree `evidence/launch-l26-B4-launch1-20261012/decoded/ue4ss.decoded.log`, read-only, taken from the driver text and the two mapper reports, not re-decoded here).

Base: candidate `LuaMod.cpp` sha256 prefix `4d6bc63b7a90a802` (build B4). Line numbers below are for that file before any edit.

## 0. In plain words (for the user)

- This morning the F7 key press was noticed once and then nothing ran. Everything that needs one shared lock got stuck behind it: later key presses, the self-test, and the spawner's background thread. The game itself and the minimap kept running.
- This design makes the key press path **unable to get stuck on that lock**: the input thread only drops a note ("mod X, callback Y, key F7") into a tiny queue and goes back to reading the pad. The game thread picks the note up a few milliseconds later and runs the Lua callback there, the same place hooks and `ExecuteInGameThread` closures already run.
- It also stops the game thread from locking and unlocking the shared lock thousands of times per frame. It first looks at a single "is there work?" flag and only takes a lock when there is work.
- Each mod gets its own Lua lock so one mod's code never runs on two threads at once, with a rule that makes two mods unable to deadlock each other.
- The game prints a `[LOCK]` line every 10 seconds so the next launch report can show real numbers.
- What this does **not** do: it does not give PS5 a keyboard (F7/F12 on a keyboard cannot work on this build; only the pad chords do), and it does not by itself explain the early 2-minute lag (section 6).

## 1. The design in one page

Three locks with a fixed order, one queue for key presses, one fast-path flag, one per-mod lock, one statistics block.

```
M (per mod, Lua exclusion, held while a mod's Lua runs)  ->  H (old m_thread_actions_mutex, registry work only)  ->  Q (new leaf queue lock)
```

| Lock | What it protects | Held while Lua runs? | Can a Lua error be raised while it is held? |
|---|---|---|---|
| **Q** `lockfix::g_q` (new, `std::mutex`) | All pending-work queues (simple, engine-tick, delayed, pending copies, key queue) and the work flags | Never | Never (code inside is a vector push or swap, nothing else) |
| **H** `LuaMod::m_thread_actions_mutex` (kept) | Hook and registry containers: `RegisterHook`, `UnregisterHook`, custom events, NotifyOnNewObject lists, `uninstall` | Only the NotifyOnNewObject drain (upstream shape, inactive in B4's world) | Never after this change: every `throw_error` moves before the guard |
| **M** per mod `lockfix::ModEntry::mu` (new, `std::recursive_mutex`) | Exclusive right to run Lua of one mod (its main, hook and async states) | Yes, for the length of one callback | The guard is only created in C++ frames that sit **above** `lua_pcall` (drains, async loop, key runner), so a Lua error cannot jump over it |

**Order rule (written into the source as a comment):**
1. Never acquire M while holding H or Q. Never acquire H while holding Q.
2. A thread that holds one M acquires a second mod's M only with a **bounded wait** (10 ms). If the wait expires it does not block: queue-run entries defer the item, hook entries run anyway and count a `forced=`. A queue-run item that has been deferred continuously for 2 s is also forced, so no mod lock can hold a mod's work back for longer than about 2 s (this also bounds the damage of a leaked or spin-held mod lock).
3. A thread that holds no M may wait longer (Block entries: up to 5 s, then run anyway, counted).
4. Therefore every wait that a lock holder makes is bounded, and unbounded waits are only made by threads that hold nothing, so a wait cycle cannot exist. Mod A on the game thread triggering mod B's hook while B's async code triggers A's hook ends after at most 10 ms with both callbacks completed (host test T3, section 10).

**Key press path (new):**
```
pad poll (event-loop thread, every ~5 ms)
   -> keybind lambda -> lockfix::key_enqueue({mod, lua*, registry ref, key})   [Q for one vector push; no Lua API; no H; no M]
game thread, next carrier/tick event
   -> fast path sees the KEYS flag -> drain_keys: swap out under Q, release,
      for each key: ModScope(mod, Try) -> prints "[l22] keybind callback entry" -> get_function_ref + call_function (same body as today)
      busy mod -> key stays queued (order kept), retried after 2 ms
no pump for 400 ms (loading screen, pump dead)
   -> the owning mod's async thread runs the key (Bounded 50 ms) = upstream behaviour (non-game thread)
```
The input thread now makes **zero Lua API calls** and waits on nothing except a leaf lock held for a vector push. It cannot be blocked by a leaked H, a long closure, or a starving lock.

**Fast path (reviewer addition 1):** at the top of every pump event (`process_event_hook`, `funcptr_marshal_hook`, `VmtEngineTickDrain`, `engine_tick_hook`) the code does, in this order, with no lock: dispatch re-entrancy check (thread-local), `note_pump` (one store), then `pump_should_drain(mask, executor, now_ms)` = three loads: back-off time, work bits, "next due" time of this executor. If false, return. Only a true result takes Q. In the host test: 15,333,392 idle events produced 400 queue-lock takes for 200 enqueued items (T2).

## 2. What I checked and what I assume

Checked in source this session (file:line in the candidate):
- The keybind callback logs its entry, then takes H, then calls Lua (`LuaMod.cpp:2227-2241`). The event-loop thread calls it from `m_input_handler.process_event()` (`UE4SSProgram.cpp:1865`). `[console B4]` line 2410 is that entry line; nothing follows.
- `NativeLuaActionDispatchScope` has a `std::unique_lock` on H as a member (`4590`) and is built on every game-thread pump event in `funcptr_marshal_hook` (`5470`), the only pump active in B4 `[console B4]` (log 918-919). It runs all game-thread Lua under that lock.
- `ExecuteInGameThread` holds H across `make_hook_state`, `lua_xmove`, `luaL_ref`, the push and the diagnostics (`6216-6310`). So does every delayed-action enqueue (`6493, 6547, 6699, 6800, 6952, 7027`) and every timer-control binding (`7080-7736`).
- `RegisterHook` takes H first (`5947`) and raises at `5956, 5966, 5994, 6043`. `UnregisterHook` (`2346`), `RegisterCustomEvent` (`3063`) and `UnregisterCustomEvent` (`3106`) do the same. `throw_error` is a `longjmp` on this port when a Lua error handler is active (`LuaMadeSimple.cpp:1564-1596`).
- `LuaMod.hpp` is not in the candidate tree. It is pinned at `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Mod/LuaMod.hpp`. The queues and `m_is_processing_actions` are `static inline` there. **This design changes no header**: all new state is file-scope in `LuaMod.cpp`, so no wider recompile set.
- `LuaMod.cpp` already includes `<atomic>` (line 9) and already uses `std::atomic`. The new code still uses only `__atomic_*` builtins as the reviewer asked. **No new `#include` line is added.** The new block needs `<mutex> <vector> <chrono> <thread> <cstdio> <cstdint>`, all already reached.
- `fire_update` and the input handler live on the event-loop thread; `start_mod` also runs there (`8297-8345`); the async thread of a mod is started before `main.lua` runs (`8307`).
- Each mod has its own `lua_State` family (`m_lua`, plus coroutine threads `m_main_lua`, `m_async_lua`, `m_hook_lua`); `m_hook_lua` is shared by all game-thread closures of that mod (`1212-1230`). The Lua core lock is global and is released while a C function runs (MAP-2). So it keeps single API calls atomic but does **not** make a callback exclusive. That is the gap M closes.

Diagnosis stance. The two mappers conclude the lock is *leaked* by a Lua error raised inside a `lock_guard` (RegisterHook at the minimap, 06:59:34) and rank starvation as an amplifier. The reviewer accepted starvation (b) as the diagnosis. I did not re-derive either. This design is built so that **it works under both**: (i) the key path and `ExecuteInGameThread` no longer touch H at all, so a leaked H cannot stop them; (ii) nothing holds a shared lock across Lua or re-takes it per event, so starvation cannot occur; (iii) the four raise-under-guard sites are fixed anyway (section 5, item L0); (iv) `HGuard` plus `leak_suspect=` in the `[LOCK]` line lets the B5 report say which one it was. Routing alone would **not** have been enough: if the key enqueue shared H with the leak, the input thread would still hang. That is why Q is a separate lock.

## 3. How Lua stays single-threaded per mod

A mod's Lua may be entered from these places. Each one now holds M(mod) while the Lua runs, except where marked.

| Entry | Thread | M policy | If M is busy |
|---|---|---|---|
| Game-thread closures (`ExecuteInGameThread`) | game | Try | item stays queued, retried after 2 ms; forced after 2 s of continuous deferral |
| Game-thread delayed/loop actions (minimap 50 ms tick, 1 ms frame loop) | game | Try | item goes back to Active, due time unchanged |
| Routed key callbacks | game (or mod async thread after 400 ms) | Try (game) / Bounded 50 ms (fallback) | key stays queued, order kept |
| `LoopAsync` / `ExecuteAsync` / `ExecuteWithDelay` | the mod's async thread | Try | action stays in the list, retried on the next 5 ms loop |
| `RegisterKeyBindAsync` callback | event-loop thread | Bounded 250 ms | runs anyway, `forced=` |
| UFunction hook pre/post callbacks (`RegisterHook`) | caller's thread (game) | Bounded 10 ms | runs anyway, `forced=` |
| LoadMap pre/post (after the deferred arm) | game | Bounded 10 ms | runs anyway, `forced=` |
| `main.lua` in `start_mod`, L23 probe queue | event-loop thread | Block (5 s) | runs anyway, `forced=` |
| `uninstall` (`fire_on_lua_stop`, `lua_close`) | event-loop thread | Block (5 s) | runs anyway, `forced=` |

- **Liveness cap.** `Try` defers, but a mod whose lock has refused its work continuously for 2 s is forced (`forced=`), so a leaked mod lock or a mod that spin-waits on its own closure delays that mod by at most about 2 s instead of for ever (host test T7).
- **Same-thread re-entry** (a hook inside a closure inside a hook, all the same mod) works: M is a `std::recursive_mutex`, so the same thread re-enters. Tested to depth 6 plus A -> B -> A (T3).
- **Where exclusivity is not guaranteed**: only when a Bounded or Block wait expires. That is counted (`forced=`) and shown per mod. Expected value in a healthy run is 0 or very small. If B5 shows many, the first suspect is a long async callback.
- **Everything else in a mod's state** (registry refs, `m_hook_lua` stack, `luaL_ref`) is touched only by a thread that holds that mod's M, because the bindings that touch them (`ExecuteInGameThread`, delayed enqueue, `RegisterHook`) run inside a callback that already holds M. A counter `unwrapped=` increments if one of these bindings is entered without M held, so any entry point I missed shows up in the first launch instead of staying silent.
- **No Lua error may pass a guard in this block.** The guards (`QGuard`, `ModScope`, `HGuard`) are never alive inside a binding frame that can raise. The new gate `lock_raise_gate.py` (section 9) checks this mechanically.

## 4. The other queues: swap out, release, then run

The drains already follow this shape (`process_simple_actions` `4696-4743`, templated `process_delayed_actions` `~4850-5000`). D2 keeps them and changes only: lock identity H -> Q; add the per-item `ModScope(Try)` with a defer path; and remove the lock that wrapped the whole drain. Details:

- `process_simple_actions`: `to_run = std::move(actions)` under Q; Q released; for each action `ModScope(mod, Try)`; busy mod -> `deferred`; deferred items re-inserted under Q (this code exists already for `m_is_currently_executing_game_action`); `eigt_run` counted.
- `process_delayed_actions`: ready list built under Q (marks `Executing`); each ready item runs under `ModScope(Try)`; a busy mod flips the item back to `Active`; the final reschedule/erase block runs under Q and ends with `republish`.
- End-of-drain block (identical in the four pumps): under Q, clear `m_is_processing_actions`, merge the pending vectors into the active ones, then **republish** the work bits and next-due times from the vectors' real contents.
- **Republish is the no-lost-wakeup rule.** The bits are only a cache of "this queue is non-empty". They are recomputed from the vectors under Q on every mutation (enqueue, drain, defer, timer change). Invariant: at every moment Q is released, bits equal the truth. A consumer that reads 0 therefore saw a moment when the queues were empty, and any later push publishes under Q after that moment. A producer sets the bit only after its push, inside Q. Host test T2 ran 80,000 items from 4 producers: no duplicate, no miss, max enqueue-to-run 519 us.
- **Timed work:** one "next due" time per executor (ProcessEvent, EngineTick, Marshal), min over active delayed items of `execute_at` in ms (floored, so never late). Pause/unpause/reset/set-timer bindings set it to 0 ("unknown"), so the next event of that pump takes Q once and re-derives it. Frame-based delays (EngineTick only) keep it at 0, so the engine-tick pump takes Q once per tick; the marshal pump never counts frames.
- **Deferral back-off:** after a deferral `g_retry_until_ms = now + 2 ms`, so a busy mod costs at most one Q take per 2 ms, not one per event.
- **NotifyOnNewObject drain** (`4749-4860`): keeps its own H block (Lua under H, upstream shape), now behind a lock-free pre-check (a plain atomic hint set by the producer under H). Inactive in B4's world; follow-up listed in section 5.

## 5. Site-by-site changes (all in `candidate/src/LuaMod.cpp`; staged as a copy, nothing under the candidate tree is edited)

Everything new is inside `#if defined(__PROSPERO__)` so a non-PS5 build keeps the upstream path.

**L-core.** Insert the block `LOCKFIX-CORE v1` (prototype: `design/hosttest-2/lockfix_core.inc`, 419 lines including comments) inside `namespace RC`, before the `L23GameThreadProbe` block (~line 96). Add ~110 lines of glue after `get_mod_ref` is declared: `mod_entry_of(LuaMod*)` (registers the mod name on first use), `run_key(ev)` (the old callback body), `recompute_bits_locked()` (reads the real vectors), `log_sink(line)` (`Output::send(STR("[LOCK] {}\n"), ensure_str(line))`).

| # | Site (candidate line) | Change |
|---|---|---|
| L0a | `UnregisterHook` 2346-2435 | Move the string/integer validation, the UFunction lookup and the "unable to unregister" message build **before** the guard. Guard (`HGuard`) covers only the `scheduled_for_removal` flag and `registry_index.lua_index = -1`. |
| L0b | `RegisterCustomEvent` 3063-3095, `UnregisterCustomEvent` 3106-3125 | Same: the two `throw_error` calls (3071, 3081 / 3114) move before the guard. |
| L0c | `RegisterHook` 5947-6050 | Phase 1 (no lock): check args (5956, 5966), `StaticFindObject` -> raise 5994 if null, classify native vs script vs neither from `GetFunc()` and flags -> build the 6043 message in a fixed `char[]`. Phase 2: `make_hook_state`, `lua_xmove`, `luaL_ref` (caller holds M). Phase 3 (`HGuard` H): `emplace_back` into `g_hooked_script_function_data` / `m_script_hook_callbacks`, `RegisterPreHook/PostHook`. Verbose log after the guard. Rule for all four: error text in a fixed buffer, raised through `LuaMadeSimple::throw_error_from_buffer` with no destructor-bearing guard alive. |
| 1 | Keybind lambda 2227-2241 and the four `register_keydown_event` calls 2243-2330 | Lambda captures `mod`, `&lua`, `ref`, `key` and calls `lockfix::key_enqueue`. Old body moves to `run_key`. `[l22] keybind queued key=.. ref=..` printed at enqueue; `[l22] keybind callback entry` printed at run (text unchanged, so harvest tools keep working). Callback is invoked without H. |
| 2 | `RegisterKeyBindAsync` 2085-2201 | Wrap the call in `ModScope(mod, Bounded, 250 ms)`. Nothing else. |
| 3 | `L23GameThreadProbe::Queue` 265 | `lock_guard` on H -> `ModScope(owner mod, Block)`. |
| 4 | `NativeLuaActionDispatchScope` 4588-4612 | Remove the `unique_lock` member. Keep `thread_local NativeLuaActionDispatching` and the ownership test; read `m_is_processing_actions` with `__atomic_load_n`. All writes to `m_is_processing_actions` use `__atomic_store_n` and happen under Q. |
| 5 | `process_simple_actions` 4696-4743, templated `process_delayed_actions` ~4850-5000 | As in section 4. Q instead of H; `ModScope(Try)`; defer; counters (`eigt_run`, `dly_run`); republish. |
| 6 | `process_pending_notify...` 4749-4860 | Lock-free pre-check only. |
| 7 | `process_event_hook` 5017-5055; `funcptr_marshal_hook` 5403-5519; `VmtEngineTickDrain` 5540-5620; `engine_tick_hook` 5625-5665 | After the existing `IsInGameThreadRaw` gate and (marshal) after `try_deferred_loadmap_arm()`: `note_pump`; `h_leak_suspect_at_pump`; `pump_should_drain(mask, executor, now)` -> else count `fp_skip` and return; count `fp_take`; `DrainTimer`; `drain_keys(run_key)` first; then the existing three drains; end block under Q + republish. The VMT per-tick cap (128) and 4 ms time box stay; the cap trim moves under Q. |
| 8 | `ExecuteInGameThread` 6090-6350 | Method selection unchanged. Replace the H guard at 6216: do `make_hook_state`/`lua_pushvalue`/`lua_xmove`/`luaL_ref` first (caller's M), then `QGuard`: build the action, choose active vs pending by `m_is_processing_actions`, push, `republish`. Read queue sizes for the depth log under Q, log after release. `eigt_q++`. `unwrapped++` if M is not held. `ensure_*_hooked` calls (non-marshal worlds) keep their own lock (5901, 5920) and run after Q is released. |
| 9 | Delayed enqueues 6493, 6547, 6699, 6800, 6952, 7027 | Same split as 8. `due_min_locked(method, execute_at_ms)`. `dly_q++`. |
| 10 | Timer controls 7080-7736 (12 bindings) | `lock_guard` on H -> `QGuard`; after a mutation `due_unknown_locked()`. Bodies unchanged. |
| 11 | `LuaMod::start_mod` 8297-8345 | Register the mod entry (name) first; `ModScope(Block)` around `load_and_execute_script` and the probe `Queue`. |
| 12 | `ensure_hook_thread_exists` 1210-1221 | After `m_hook_lua` is created: `mod_alias(m_hook_lua, entry)` so a closure's `lua*` maps to its mod without dereferencing it. |
| 13 | `LuaMod::uninstall` 8403-8570 | Keep the same-thread refusal. After the async join: `ModScope(this, Block)`, `mod_tombstone`, purge this mod's keys (Q), keep the existing action purge but under Q, then `lua_close`. H is taken only for `erase_from_container` and hook unregistering, in short blocks, never across `lua_close`. |
| 14 | `update_async` 9704-9725 and `LuaMod::process_delayed_actions()` 9727-9786 | Each due action runs under `ModScope(this, Try)`; busy -> keep in the list. After `process_delayed_actions()` call `drain_keys(fallback runner, 400 ms, this)` and `maybe_log`. The async thread is also the statistics heartbeat, so a `[LOCK]` line appears even if the game pump is dead. |
| 15 | `lua_unreal_script_function_hook_pre/post` 364-700 | `ModScope(entry from lua_data.mod, Bounded 10 ms)` inside the existing `TRY` lambda, around preparation and `call_function`; `forced` is counted by the scope. The single explicit `throw_error` inside that body (unsupported property type, ~443) becomes a flag and is raised after the scope closes. |
| 16 | `lua_load_map_pre/post_callback` 5258-5340 | Same as 15. |
| 17 | `LuaMod::global_uninstall` 1165 (currently an empty function, called from `UE4SSProgram.cpp:2443`) | Final `maybe_log(final=true)`. |
| later | `script_hook` 8612, StaticConstructObject post-callback 9007, engine-event lambdas 8896-9674 | Not active in B4's world (R3 gate C:8871). **Not changed in B5**; listed so the next round wraps them with the same helper. |

Net: 17 edited sites plus the three raise-before-lock restructures; no header, no new file, no new `#include`.

## 6. Early lag (~2 minutes) and the self-test

**Not explained, not claimed.** I can say what this design changes and what it will measure.
- B4 per-event cost: a `unique_lock` on H per carrier event, held while the drain's Lua ran. With nobody else contending (all other threads were blocked on H, `[console B4]` section of MAP-2) that is cheap, so I do not think the lock alone explains 2 minutes of lag.
- The mappers' better candidates (unproven): minimap UI-build bursts logged synchronously on the game thread (1115 widget lines in one second at 06:59:34; 700-866 lines/s at 07:01:50-53; pump gaps up to 1431 ms `[console B4]`), 1355 construct-log lines in 06:59:33-55, `[UF]` error lines, and the 66-carrier rescan (that ran on zzdiag's async thread, not the game thread).
- What B5 will print so the next report can separate the causes: per 10 s window `drain_win_ms` (time the game thread spent inside drains; if this is small relative to 10,000 ms, the pump and Lua are not the lag), `pump gap_max_ms`, `fast skip/take`, `qlock hold_max_ms`, `cb_max_ms` (longest routed key callback), and per-mod `hold_max_ms`/`defer`. The log writer's cost is not measured by this design; if drains are small and lag persists, the next suspect is `Output::send` volume on the game thread, which is outside this fix.

**Self-test.** The 3-minute AUTO trigger fires from the `ueselftest` async thread; its `pcall(ExecuteInGameThread, RUN)` now needs only Q (no H), and RUN executes on the game thread under M(ueselftest). While RUN runs, ueselftest's own LoopAsync ticks defer (they do not block). Timings against the 10 ms lookup target come from the self-test's own lines plus `eigt q/run` and `cb_max_ms`; `[MEM]` lines print inside `ExecuteInGameThread` after Q is released, so they cannot be lost to a stuck lock.

## 7. Reviewer additions, one by one

1. **Fast path.** Section 1 and 4. Uses `__atomic_load_n/__atomic_store_n/__atomic_fetch_add/__atomic_compare_exchange_n` only (pattern `LmsRelaxedCounter`, `LuaMadeSimple.cpp:22-28`). Flags set only under Q by republish, cleared only under Q when the queue is empty. Counters `fast skip=` and `take=` print in the `[LOCK]` line; single-writer counters on the game thread use a plain relaxed store (no atomic read-modify-write on the hot path).
2. **Per-mod locks.** Section 1 (order rule) and 3. Re-entrant (`std::recursive_mutex`). ABBA handled by the bounded cross-mod wait. Mac test T3 runs two mods firing at each other from two threads, 400 iterations each. The same host test file also contains the stuck-key repro (T1) as asked.
3. **Parity.** `design/PARITY-2.md` (full table, also in section 11).
4. **Statistics.** Section 8.

## 8. The `[LOCK]` line (reviewer addition 4)

Rate limit: at most one block every 10 s, guarded by a compare-and-swap on the next-print time; one extra block with `final=1` from `LuaMod::global_uninstall`. Callers: any pump event (one compare), and every mod's async loop (5 ms), so it prints even if the game pump is dead.

```
[LOCK] keys q=<queued> d=<dispatched> drop=<n> fb=<fallback runs> slow=<callbacks >50 ms> wait_ms max=<ms.mmm> mean=<ms.mmm> cb_max_ms=<ms.mmm> | qlock takes=<n> contended=<n> hold_max_ms=<ms.mmm> wait_max_ms=<ms.mmm> | fast skip=<n> take=<n> | eigt q=<n> run=<n> dly q=<n> run=<n> | defer=<n> forced=<n> unwrapped=<n> stale=<n> leak_suspect=<n> | pump gap_max_ms=<n> drain_win_ms=<n> drain_max_ms=<n>[ final=1]
[LOCK] mod=<name> lua_lock wait_max_ms=<ms.mmm> hold_max_ms=<ms.mmm> acq=<n> contended=<n> defer=<n> forced=<n>      (one line per mod)
```
Field meaning in one line each: `keys q/d` keys queued and dispatched; `wait_ms` key press seen by the input thread to callback start (press time is the moment the input thread saw the edge, 5 ms poll resolution); `qlock hold_max_ms` longest time anyone held Q; `mod=` per-mod Lua-lock max wait; `fast skip/take` pump events that took no lock versus took Q; `eigt q/run` `ExecuteInGameThread` closures queued versus run (`dly` is the same for timed actions); `defer` items postponed because a mod's lock was busy; `forced` Bounded/Block waits that expired; `unwrapped` binding entered without the mod lock (should be 0); `leak_suspect` game thread entered the pump holding H (should be 0); `drain_win_ms` game-thread time inside drains in the last window.

Proposed B5 acceptance read-outs (to be confirmed by the reviewer): key `wait_ms max` < 100 ms and `mean` < 20 ms in normal play; `qlock hold_max_ms` < 2; `fast skip` > 99 % of events when idle; `forced`, `unwrapped`, `leak_suspect` = 0; `slow` = 0 for the spawner callback.

## 9. Gates, markers, `CHANGES-ROWS.md` declarations

**Existing gates, unchanged:** `fs_throw_gate.py`, `no_detour_gate.py` before compile; `verify_build.sh --manifest` for the exact PRX after packaging. D2 adds no detour, no hook, no vtable write, no executable edit.

**New gate (must print counts and fail on zero):** `harvest/lock_raise_gate.py`. For `LuaMod.cpp` it finds every scope that declares `lock_guard`, `unique_lock`, `HGuard`, `QGuard` or `ModScope`, and checks that no `throw_error`, `throw_lua_error` or `luaL_error` call appears inside the same braces while the guard is alive. It prints `scopes checked: N, violations: V` and exits non-zero if N is 0 or V is not 0. On the B4 candidate it must report violations at 2346, 3063, 3106, 5947 (a useful self-check: run it on the unmodified file first and expect those four).

**Include gate:** D2 adds zero `#include` lines. Expected report: `new includes: 0`.

**Markers to declare in `CHANGES-ROWS.md` / `add_build_markers.py` set `B5` (verify_build proves each is in the PRX):**

| id | text | encoding | source |
|---|---|---|---|
| `b5-lock-line` | `[LOCK] {}` | utf-16le | `LuaMod.cpp` log sink `Output::send(STR("[LOCK] {}\n"), ...)` |
| `b5-lock-keys` | `keys q=%llu d=%llu` | utf-8 | `lockfix::maybe_log` snprintf format |
| `b5-lock-mod` | `lua_lock wait_max_ms=` | utf-8 | per-mod line format |
| `b5-key-queued` | `[l22] keybind queued key=` | utf-16le | keybind lambda |
| `b5-key-fallback` | `[LOCK] key-fallback` | utf-16le | fallback runner log |

(`[l22] keybind callback entry` is an existing marker; its text is unchanged.)

**`CHANGES-ROWS.md` row for the build:** file `candidate/src/LuaMod.cpp`, new sha256 filled in at staging time, label `B5-lock-route`, description "route key presses to the game thread through a short-lock queue; split Q/H/M locks; lock-free fast path; per-mod Lua locks; `[LOCK]` statistics; raise-before-lock in RegisterHook/UnregisterHook/RegisterCustomEvent/UnregisterCustomEvent", markers_added = the five above, `new_includes: []`, `headers_changed: []`.

## 10. Tests

**Run now (Mac-only host prototype, `design/hosttest-2/`):** Apple clang 17.0.0 arm64, `-std=c++20`, `-O2` and `-O0`, built one at a time with `vm_stat` free above 2 GB (4.0 GB at the last build). The test compiles `lockfix_core.inc` (the proposed production text) unchanged. Result: **checked=25 failed=0** at both optimisation levels, three repeat runs at `-O2` all green. Files: `run-O2.txt`, `run-O0.txt`, `SHA256SUMS.txt`, `toolchain.txt`.

| Test | What it proves |
|---|---|
| T1 stuck-key repro | OLD shape (global recursive mutex, dispatch scope per event, a binding that `longjmp`s out of a `lock_guard`): the keybind thread does not run in 1.2 s while the game thread keeps pumping (about 4,850 events). NEW shape with the **same leak** applied: key dispatched once, press-to-callback about 0.25 ms (242-247 us over four runs; includes the 200 us test pump interval). |
| T2 fast path and wakeups | 4 producers x 20,000 items: each item ran exactly once, max enqueue-to-run 63-519 us over runs. Idle ratio: 7.8 to 15.4 million events, 400 queue-lock takes for 200 items. |
| T3 two-mod ABBA and re-entrancy | Game thread holds A and fires B's hook; B's async thread holds B and fires A's hook; 400 iterations each; both finish, no single mod-lock wait over 2.2 ms (cross-mod bound lowered to 2 ms for the test). In this adversarial loop (both threads grab their own lock, then the other's, at the same instant) every cross-mod attempt expired (`forced` 798-800 of 800), which is the designed fallback, not a hang. Real runs should see far fewer. Nested same-mod depth 6 and A -> B -> A re-enter without deferral. |
| T4 deferral, fallback, tombstone | Busy mod A: its 3 keys deferred, mod B's key ran; after release A1, A2, A3 in order. Fallback ran once, 400 ms after the press. A key taken by the game pump is not run again by the fallback. A tombstoned mod refuses; re-registering revives it. |
| T5 `[LOCK]` line | Rate limit holds (second call within 10 s prints nothing), prints again after 10 s, `final=1` bypasses the limit, all required fields present. |
| T7 deferral cap | A mod lock held for good (stand-in for a leak): `Try` entries defer 28 times, then are forced after the cap (152-155 ms with the cap lowered to 150 ms for the test; 2000 ms in production); after release the streak resets. |
| T6 H ledger | Balanced `HGuard` use is not flagged; a `longjmp` out of an `HGuard` is flagged (`leak_suspect=1`). |

**Limits of this prototype (Mac-only):** it tests the lock primitives and protocols, not the real `LuaMod.cpp`, not the Prospero libc++/scheduler, and not real Lua. It also uses `std::this_thread::sleep_for` polling for bounded waits (granularity on Prospero is unknown, so real waits may overshoot the budgets slightly). The old-shape starvation case is not asserted because the Mac scheduler may hide it; only the leak variant is asserted.

**To run when the real edit is staged (Mac-only):** syntax compile of the staged `LuaMod.cpp` (existing `syntax-receipt` method); `lock_raise_gate.py` on candidate (expect 4 violations) and on staged (expect 0); extraction check that the `LOCKFIX-CORE` text in the staged file is byte-identical to `lockfix_core.inc` (sha recorded); include-graph gate (expect 0 new); then fs_throw, no_detour, build, package, `verify_build.sh --manifest` for the exact PRX. Any console step (upload, readback, row-check, launch) needs its own explicit OK from the user.

**B5 launch checklist (for the report, console-proven only after the launch):** (1) F7 / L3+R3 spawner: `[l22] keybind queued` then `[l22] keybind callback entry` then the spawner's Lua output, repeated for every press; L1+R1 self-test chord likewise. (2) Minimap and stutter: `drain_win_ms`, `pump gap_max_ms`, `fast skip/take`, `slow=`. (3) Self-test timings against the 10 ms target, with `eigt q/run`. (4) The `[LOCK]` lines, including `forced`, `unwrapped`, `leak_suspect`. Use the pad chords, not a keyboard.

## 11. Parity summary (full table: `PARITY-2.md`)

| Entry point | PC upstream | D2 |
|---|---|---|
| Keybind callback | event-loop thread, under H | game thread via a short-lock queue; non-game-thread fallback after 400 ms (deliberate) |
| `RegisterKeyBindAsync` | event-loop thread, no lock | same thread, mod lock Bounded 250 ms |
| `LoopAsync` and friends | per-mod async thread, no global lock | same thread, mod lock Try (deliberate exclusion) |
| `ExecuteInGameThread` enqueue | any thread, H | any thread, Q only |
| Game-thread drain | pump thread, short H blocks, Lua unlocked | game thread only (platform), short Q blocks, Lua under mod lock Try |
| Hook callbacks | calling thread, no lock | calling thread, mod lock Bounded 10 ms |
| `RegisterHook` family | H then raise (safe on Windows) | validate and raise first, then H |
| NotifyOnNewObject | Lua under H | unchanged (inactive in B4 world) |

## 12. Risks (honest list)

1. **Key callbacks now run on the game thread.** A mod that does heavy work directly in a keybind callback will hitch the game while it runs. On PC that work was on the input thread. `slow=` and `cb_max_ms` will show it. The spawner mod already defers its work through `ExecuteInGameThread`, which is the common pattern.
2. **Keys wait for a pump event.** Latency is one pump interval, normally milliseconds; in menus with few carrier calls up to 400 ms, then the fallback runs it on a non-game thread.
3. **Mod-level mutual exclusion is stronger than PC.** A mod that spin-waits in `LoopAsync` for a flag set by its own game-thread closure will see that closure delayed (up to about 2 s, then forced) instead of run at once. The same mod's `main.lua` also holds M, so that mod's closures start only after `main.lua` returns (or after 2 s). Both are visible as `defer=` and per-mod `hold_max_ms`.
4. **Exclusivity is best effort under contention.** Bounded and Block entries run anyway when the wait expires. Counted as `forced`; the audit value is 0 in a healthy run. If it is not 0 the first thing to check is a long async callback.
5. **A mod lock can still leak if a Lua error is raised through a hook callback.** `RegisterHook` callbacks hold M inside the engine's hook frame; if such a hook is triggered from inside another Lua `pcall` and a property pusher raises (longjmp), the jump skips the guard. The effect is bounded: that mod's queue-run work is forced after 2 s and Bounded entries run after their wait, and the game thread itself re-enters freely. `defer=` rising with `acq=` flat for one mod is the signature. The raise-before-lock gate cannot see raises inside callees.
6. **Unwrapped entry points.** If some binding is reached without M held, state protection for that path is the same as B4 minus H. `unwrapped=` exposes it. Sites in the "later" row of section 5 are not wrapped.
7. **Diagnosis not settled.** Leak versus starvation cannot be separated by a launch of this build alone: the fix covers both. `leak_suspect=` tells them apart afterwards.
8. **Hot reload address reuse.** An in-flight popped item for a mod that is uninstalled and re-created at the same address could be matched to the new mod. Same window exists upstream; hot reload is not used in normal PS5 runs.
9. **Platform timing.** Bounded waits poll with 100 us sleeps. If Prospero's timer granularity is coarser, waits overshoot by up to one tick. No new libc++ feature is used (no `timed_mutex`, no condition variable).
10. **No keyboard source on PS5.** Keyboard F7/F12 never reach `RegisterKeyBind` on this build, with or without this fix. Out of scope; reported as a capability gap.
11. **The `LuaMod.cpp.rej` in the candidate `src/` folder** is a leftover patch reject; I did not look inside it. Check that it is not relevant before staging.

## 13. Size and files

- Edited: `candidate/src/LuaMod.cpp` only (staged as a copy under `round7-staging/lock-fix/`).
- Estimated diff: about +650 / -150 lines (core block ~420 lines of which ~100 are comments, glue ~110, site edits ~140 net).
- New includes: none. Headers changed: none. New tool: `harvest/lock_raise_gate.py`. New markers: five.

## 14. Files in `design/`

- `DESIGN-2.md` (this file), `PARITY-2.md`.
- `hosttest-2/lockfix_core.inc` (proposed production block), `test_lockfix.cpp`, `build.sh` (refuses under 2 GB free), `run-O2.txt`, `run-O0.txt`, `SHA256SUMS.txt`, `toolchain.txt`.
