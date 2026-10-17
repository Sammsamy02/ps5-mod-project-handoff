# MAP-2: lock map for LuaMod.cpp lines 5000 to end, and the key-press verdict

Build: B4 (log line 23, `id=l26-20261012T040323Z`, snapshot b63ba460). Evidence: this morning's launch only
(`outputs/2026-10-08-l25-prep/evidence/launch-l26-B4-launch1-20261012/`). No console contact, no builds, no edits
to the candidate or original trees.

Labels: **[console]** = this session's console data (decoded ue4ss log line numbers, or this session's rows of
`harvest-l26-B4-launch1-20261012-supplier-diag.jsonl`, i.e. the rows after the last `module_start_entered`, pid 99).
**[Mac]** = my reading of the code on the Mac. `C:` = candidate `src/LuaMod.cpp`. `U:` = PC upstream RE-UE4SS
`.superpowers/native-dependency-spike/ue4ss-binding-research-1/upstream/RE-UE4SS/UE4SS/src/Mod/LuaMod.cpp`.
`O:` = the "original" tree (`typescan-r4/original/src/LuaMod.cpp`). That tree is the earlier **port** (l23),
not PC upstream. It already has the PS5-only `NativeLuaActionDispatchScope`, so I compare against both.

Wall-clock mapping for the diag rows: the diag event `eigt_queue_depth` at t_ms 1418034 is written in the same
code block as log line 916 (06:59:33). All diag times below use that anchor (accuracy about 1 s).

---

## 1. Short answer (plain words)

- **Not a two-thread deadlock (a), and very probably not plain starvation (b) either.** The most likely cause
  is a **lock that is never given back**. On PS5, a Lua error raised inside a runtime function that is holding
  the shared actions lock skips the code that would unlock it. The game thread then owns the lock forever.
  The game thread keeps running, because this lock type lets the thread that already holds it lock it again.
  So the game, the minimap and its F5 key keep working. Every other thread that needs the lock waits forever:
  the key-press thread (F7, F12, pad L3+R3, L1+R1), and the background ("async") threads of the self-test and
  zzdiag.
- **The trigger is in the minimap.** At 06:59:34 the minimap asks to hook three Blueprint events on the
  player's class, using `pcall(RegisterHook, ...)`. None of the three hooks registers. On PS5 each failed
  `RegisterHook` gives up the Lua error while still holding the lock. The minimap is a normal PC mod. On PC
  the same failure releases the lock.
- **There is also a second, smaller problem, the one the reviewer described.** The PS5 port holds the same
  lock for the whole time the game thread runs mod code (all game-thread Lua). That makes other threads wait
  longer even when nothing has leaked. PC upstream does not do this.
- **The self-test:** the 3-minute AUTO trigger fired on time (07:02:15) but the run never started. It got
  stuck on the same lock. So there are **no lookup timings and no self-test memory numbers** from this launch.

---

## 2. Runtime threads involved

| Thread | What it runs (relevant to this lock) | Source |
|---|---|---|
| **Game thread** (engine main thread; identity published by the first carrier fire, self-test thread id 101860 at log 933) | The only active game-thread pump in B4: `funcptr_marshal_hook`, the post-hook on 66 Blueprint-override carrier functions (log 914). It runs ExecuteInGameThread closures, LoopInGameThreadWithDelay / ExecuteInGameThreadWithDelay closures (the minimap's 50 ms `safeTick` and 1 ms `M.frame`), and pending NotifyOnNewObject items. RegisterHook Lua callbacks on hooked UFunctions also run here (without the lock, as upstream does). | C:5403-5519; minimap main.lua:4268, 4034 |
| **UE4SS event-loop thread** (`UE4SSProgram::update`, `m_event_loop` jthread) | Mod start, L23 probe queue/poll, the queued-event batch, `m_input_handler.process_event()`: **keyboard and PS5 pad polling, and every RegisterKeyBind callback** (C:2227-2241, takes the lock, then runs Lua). Also `fire_update`. The pad is polled only here ("sole processing thread"). | UE4SSProgram.cpp:732, 1779-1880 (1865 = input); l22 PS5PadSource.cpp:35, 95-98 |
| **Per-mod async threads** (one `std::jthread` per Lua mod, `LuaMod::update_async`) | LoopAsync / ExecuteAsync / ExecuteWithDelay callbacks on `async_lua()`. **They run Lua without the shared lock**; only the per-mod `m_actions_lock` is taken, briefly. When their Lua calls ExecuteInGameThread & co., that binding takes the shared lock (C:6216 etc.). | C:9704-9786 |
| Engine worker / loading threads | ProcessEvent / carrier fires from workers are refused before any lock (`IsInGameThreadRaw`, C:5026, 5450, 5550, 5633). The StaticConstructObject post-callback (C:9007), which would lock on any thread, is **not registered in B4**: the R3 gate (C:8871) skips the whole eager hook block, and log 780 shows that gate's warning. | |

Other locks on the same paths:

- `m_actions_lock`: a per-mod `std::mutex`. Taken for the async queue merge (C:9731-9736, 9790-9793) and the
  async registrations (part 1).
- `m_funcptr_marshal_carrier_mutex`: a file-static `std::mutex` (C:5768).
- The global **Lua core lock**. `luaconf.h:796-800` in LuaRaw maps `lua_lock` / `lua_unlock` to `LuaLock` /
  `LuaUnlock`. On PS5 those are a process-wide recursive pthread mutex in the composed
  `.superpowers/native-dependency-spike/ue4ss-native-lua-lock-final-1/src/platform_locks.c` (listed in
  `outputs/2026-10-08-l26-build-variants/l26-A/pass1/COMPOSITION-RECEIPT.json`). This is the PS5 copy of
  upstream's Windows CRITICAL_SECTION in `luauser.c`. Lua releases it while any C function runs. **[Mac]**
  So no thread holds it while waiting for the actions lock. The only lock order is actions lock, then Lua
  core lock. There is no reverse edge, so the two locks cannot deadlock each other.
- The ScriptHookThunks registry mutex. It is short: callbacks are fired outside it (ScriptHookThunks.cpp:53-66
  compared with 75-79). Its order is actions lock, then registry. There is no reverse edge.
- The Output `FileDevice::m_write_mutex`, taken on every log line, and the L23 probe state mutex. Both are
  short and have no reverse edge.
- The driver's note that there is no `lua_lock` / `LuaLock` is true only for the candidate folder. **The
  composed build has one** (platform_locks.c). In this codebase it is the "per-state Lua lock" the reviewer
  wants kept separate. It is global, like upstream's.

---

## 3. Every acquisition of `LuaMod::m_thread_actions_mutex` (and the other mutexes) from C:5000 to the end

Notes: "Lua under lock" means mod Lua callbacks run while the actions lock is held. "Waits" means the holder
blocks on another thread while holding the lock. **No site in this range waits on another thread while
holding the lock.** `grep` finds no condition variable, future or sleep in this file under the lock. The only
`join` (C:8421) is deliberately done with the lock released.

| C: line | Function | Thread | Protects | Lua under lock | Waits | Active in B4? | U: (PC upstream) / O: (old port) |
|---|---|---|---|---|---|---|---|
| **5032** (scope class C:4588-4612) | `process_event_hook`, `NativeLuaActionDispatchScope` (`unique_lock`, held to function end) | game | dispatch-ownership flags (`m_is_processing_actions`, thread-local `NativeLuaActionDispatching`); in practice all queues **and** all Lua run in the drain | **YES**: simple actions (call at C:4725), delayed actions (C:4952), pending notify (C:4826) | no | No (ProcessEvent pump not registered in the marshal world) | U:4090-4121 has **no outer lock**, only the two short locks below. O:4672 same as candidate. |
| 5037 | `process_event_hook` | game | sets `m_is_processing_actions` | no | no | No | U:4098, O:4677 |
| 5047 | `process_event_hook` | game | clears flag, merges pending to active queues | no | no | No | U:4108, O:4687 |
| **5470** | `funcptr_marshal_hook`, `NativeLuaActionDispatchScope` | **game** (gated C:5450) | as 5032. Held across `try_deferred_loadmap_arm` (C:5477, one-shot detour registration) and all three drains (C:5494-5497). **Taken again on every carrier fire** (66 carriers, log 914). The lock is taken **before** `owns_dispatch()` is checked, so nested or empty fires also take it. | **YES**: all game-thread mod Lua, including the minimap's 50 ms tick and 1 ms frame loop | no | **YES, the only pump** ([console] log 918; `r3_marshal_pumped` once a second until about 07:03:21) | Port-only (no marshal upstream). Upstream's equivalent pump (process_event_hook U:4090) holds no lock across Lua. O:4890 same as candidate. |
| 5490 | `funcptr_marshal_hook` | game | sets flag | no | no | Yes | O:4900 |
| 5500 | `funcptr_marshal_hook` | game | merges pending queues | no | no | Yes | O:4910 |
| **5554** | `Unreal::VmtEngineTickDrain`, dispatch scope | game | as 5470. The 128-action cap and 4 ms time box (C:5568-5594) **do not shorten the lock**: simple actions run before the time check, and the whole drain is under the scope. | **YES** | no | No ([console] log 623: EU-VMT arming refused) | Candidate-only (EU FIX4); not in O: or U: |
| 5564 / 5596 | `VmtEngineTickDrain` | game | flag + cap trim / merge | no | no | No | none |
| **5637** | `engine_tick_hook`, dispatch scope | game | as 5032 (engine-tick queues) | **YES** | no | No ([console] log 621-622: EngineTick refused) | U:4123-4157 has **no outer lock**; O:4943 same as candidate |
| 5642 / 5652 | `engine_tick_hook` | game | flag / merge | no | no | No | U:4135 / 4145; O:4948 / 4958 |
| 5768 (**other mutex**: `m_funcptr_marshal_carrier_mutex`) | `r3_marshal_arm_event_carriers` (via `r3_marshal_lazy_arm` from `funcptr_marshal_usable`, C:4669-4682, 5873-5893; and at program start) | **whoever calls ExecuteInGameThread & co.** At 06:59:33 that was zzdiag's async thread (log 908-915) | carrier list. Held across 3 StaticFindObject calls, a **full GUObjectArray walk** (~155k objects) and RegisterPostHook per override | no | no | Yes (log 776-779, 908-914) | Port-only. Taken **before** the actions lock in the bindings (C:6107 before C:6216). If called from game-thread Lua, it runs while the actions lock is already held (rate-limited to once a second, and only when carriers are stale). |
| 5901 | `ensure_engine_tick_hooked` (PS5-only lock) | caller | `m_is_engine_tick_hooked` + hook registration (may throw a C++ exception, which unwinds cleanly) | no | no | Not reached in B4 | U:4178: **no lock**. O:5270 |
| 5920 | `ensure_process_event_hooked` (PS5-only lock) | caller | `NativeProcessEventHookId` | no | no | Not reached | U:~4187: no lock. O:5289 |
| **5947** | **`RegisterHook` binding**: lock for the **whole** binding (to C:6050) | caller's thread. **Here: the game thread** (minimap tick) | hook containers (`g_hooked_script_function_data`, `m_script_hook_callbacks`), `make_hook_state`, registry refs | Lua API only (`lua_xmove`, `luaL_ref`); no callbacks | no | **YES** | U:4199-4200 has the same lock placement. **Raises a Lua error with the lock held** at C:5956, 5966, 5994 (no UFunction), 6043 (cannot register). See section 4. |
| 6216 | `ExecuteInGameThread` | caller (async LoopAsync threads, keybind thread, game thread) | queues + `make_hook_state` + queue-depth / [MEM] diagnostics (C:6250-6310); runs `ensure_*_hooked` after the enqueue (C:6321-6346) | no (Lua API only, plus `lua_gc COUNT`) | no (enqueue only) | YES. **This is where the ST2 AUTO run and zzdiag block** | U:4366-4368 same. No raise under the lock except an out-of-memory error from the Lua API. |
| 6493 / 6547 | `ExecuteInGameThreadWithDelay` (handle / no-handle) | caller | delayed queue enqueue | no | no | Yes | U:4465 / 4519 |
| 6699 | `RetriggerableExecuteInGameThreadWithDelay` | caller | delayed queue | no | no | Yes | U:4600 |
| 6800 | `ExecuteInGameThreadAfterFrames` | caller | delayed queue | no | no | Yes | U:4682 |
| 6952 | `LoopInGameThreadWithDelay` | caller (the minimap registers from start-up and from game-thread Lua, C:4034 path) | delayed queue | no | no | Yes | U:4761 |
| 7027 | `LoopInGameThreadAfterFrames` | caller | delayed queue | no | no | Yes | U:4817 |
| 7080, 7146, 7214, 7293, 7343, 7388, 7430, 7472, 7514, 7597, 7682, 7736 | `ResetDelayedActionTimer`, `SetDelayedActionTimer`, `PauseDelayedAction`, `UnpauseDelayedAction`, `CancelDelayedAction`, `IsValidDelayedActionHandle`, `IsDelayedActionActive`, `IsDelayedActionPaused`, `GetDelayedActionTimeRemaining`, `GetDelayedActionTimeElapsed`, `GetDelayedActionRate`, `ClearAllDelayedActions` | caller | find by handle, flip status (no erase; erase happens only in the drain's final locked block, C:5007) | no | no | if called | U:4868, 4934, 5002, 5081, 5131, 5176, 5218, 5260, 5302, 5385, 5470, 5524 |
| 8409 | `uninstall` (PS5-only) | event loop | refuses a same-thread uninstall during dispatch | no | no (released before the `join` at C:8421) | No | none in U:. O:7639 |
| 8425 | `uninstall` | event loop | teardown; `fire_on_lua_stop_for_self` (C:8431) and `lua_close` (C:8556) run Lua finalizers and the unload callback | yes (teardown) | no | No | U:6111 |
| 8516 | `uninstall` | event loop | erases this mod's queued actions | no | no | No | U:6199 |
| 8612 | `script_hook`, lock for the whole callback | game | script-hook containers | **YES** (C:8702); also raises a Lua error under the lock (C:8695) | no | **No** (log 764: ProcessLocalScriptFunction not available; and the R3 gate) | U:6295 same |
| 9007 | StaticConstructObject post-callback (NotifyOnNewObject) | any thread; runs Lua on the game thread | static-construct callback list + pending notify queue | **YES** (C:9042) | no | **No**: not registered in the R3 world (C:8871, [console] log 780). The searcher-pool hook on the same function also failed (log 668). | U:6631 same. **[Mac] So NotifyOnNewObject callbacks never fire on PS5 in this world** (the minimap's Interactable callback, log 839). This is a separate capability gap. |
| 9731-9736 (**other mutex**: per-mod `m_actions_lock`) | `LuaMod::process_delayed_actions` (async loop) | per-mod async | pending-to-delayed merge only. **The LoopAsync Lua runs after unlock, with no actions lock** (C:9738-9786) | **no** | no | Yes | U:7254-7313 identical |
| 9790-9793 (`m_actions_lock`) | `clear_delayed_actions` | caller | async queues | no | no | if called | U:7317 |

Also on these paths, outside the table: `funcptr_marshal_hook` takes a runtime-residence ticket at C:5421
(`NativeNameThread::acquire_runtime`: a short owner mutex, then a ticket; not a lock held across the drain).

---

## 4. Why the lock is never given back (the leak), with file:line

1. **[Mac]** On PS5, Lua is built as C. The composition compiles `ldo.c` with `prospero-clang -std=c11`, so
   Lua raises errors with `longjmp`. LuaRaw `ldo.c:53-74` makes that choice. The port says so itself at
   `LuaMadeSimple.cpp:1557` ("Lua errors are a longjmp on this port ... nothing with a destructor may be alive
   in this frame when luaL_error runs").
2. **[Mac]** `lua.throw_error` goes through `throw_error_impl` to `luaL_error` (`LuaMadeSimple.cpp:1564-1596`).
   It throws a C++ exception only when no Lua error handler is active (LuaMadeSimple.cpp:1569). Inside any `pcall`, a
   handler **is** active, so it uses `longjmp`.
3. **[Mac]** `RegisterHook` takes `std::lock_guard<std::recursive_mutex>` on the actions lock as its very first
   statement (C:5947). It then raises with `lua.throw_error` on every failure: C:5956, 5966, 5994, 6043. The
   `longjmp` jumps over the `lock_guard` destructor, so the calling thread keeps one ownership level per
   failure. Because the mutex is recursive, that thread can still lock it again, while every other thread
   blocks forever.
4. **[Mac, background knowledge, not verified this session]** On PC upstream the same source line is safe. On
   Windows x64, MSVC `longjmp` unwinds C++ frames and runs destructors. So this is a platform difference
   inside one shared source line, not a mod bug.
5. **[Mac]** MAP-1's stand-in host test (`map/hosttest-1/`, run-O0.txt / run-O2.txt) already reproduced it on
   Mac clang: "A longjmp-out-of-lock_guard: other thread acquired within 200ms = false", while a C++ throw
   releases the lock. My second test uses the real Lua C sources and a Lua-level `pcall`
   (`map/hosttest-longjmp/`). It compiled the 32 Lua objects but **failed to link**: `LuaLock` / `LuaUnlock`
   are needed, and `platform_locks.c` was missing. I fixed `build.sh`. I did **not** re-run it, because
   vm_stat "Pages free" was 0.12 GB at the moment of that first build (the check and the build were in the
   same command, so the check did not stop it; that was my mistake). No further compiles were started.
   `build.sh` now refuses to run under 2 GB free.

### Console timeline that fits the leak (and only the leak fits all of it)

| Time | Event | Label |
|---|---|---|
| 06:59:15 | Mods start; the only `[RegisterHook] Registered ...` lines of the whole session are 807 and 818 (CheatManagerEnabler / ConsoleEnabler, on the event-loop thread, both succeeded) | [console] |
| 06:59:30 | pad available (`pad_available`, diag t=1415354) | [console] |
| 06:59:33 | carriers re-armed (66, log 908-914), zzdiag's 18 s probe queued (log 915). This is the **last successful ExecuteInGameThread enqueue of the session**: diag `eigt_queue_depth` has only 2 rows (06:59:15, 06:59:33) to the end of the stream (about 07:03:21) | [console] |
| 06:59:34 | log 994: `GetFullName` of `BlueprintGeneratedClass /Game/Townfall/Character/...`. Among the deployed mods, only the minimap's `registerInteractionHooks` does this (main.lua:507). It then calls `pcall(RegisterHook, classPath..":"..event, fn)` for 3 events (main.lua:512-519). **No "Registered" line follows.** | [console] for the lines; [Mac] for which code produced them |
| from 06:59:34 | the minimap keeps running game-thread Lua (1115 widgets logged in that second; F5 settings I/O at 07:01:25-34, log 3106-3113; widgets to 07:02:27). So the **game thread keeps taking the actions lock** (that Lua only runs inside the dispatch scope, C:5470) | [console] + [Mac] |
| ~06:59:51 | zzdiag's `LoopAsync(18000)` should call ExecuteInGameThread again (zzdiag main.lua:82). There is no queue line and no diag row, then or ever after, so zzdiag's async thread is stuck at C:6216 | [console] absence, [Mac] location |
| 06:59:56.4 | one pad L3+R3 edge, F7 dispatched (`pad_l3r3_f7_dispatched 118`, diag t=1441457). Log 2410 (06:59:57): `[l22] keybind callback entry key=118`. That line is printed **before** the lock (C:2228-2231). No Lua output follows. The second F7 callback (zzdiag registers F7 too, log 852) never logs its entry | [console] |
| after 06:59:56 | **no pad events at all** until the end of the stream (about 07:03:21), although the user pressed L3+R3 and L1+R1 several times. The pad is polled only by the event-loop thread, so that thread is stuck inside the first F7 callback, at the lock | [console] absence, [Mac] location |
| 07:02:15 | log 7549 `[ST2] AUTO start` (180 s after ST2 boot, log 848). Then `pcall(ExecuteInGameThread, RUN)`: no queue line, no `MEM before`, no `RUN start`, no diag row, so the ST2 async thread is stuck at C:6216 | [console] |
| to ~07:03:21 | `r3_marshal_pumped` once a second, 227 rows, largest gap 1431 ms: the game thread never froze | [console] |

Note: the decoded ue4ss log ends at 07:02:27 but the diag stream runs to about 07:03:21. The log tail may be
missing (the FTP read was taken while the game was still running?). The "no enqueue / no pad" conclusions use
the diag stream, which covers the whole window.

### Verdict on the mechanisms

- **(a) Deadlock (the keybind path waits on the game thread while holding the lock): refuted.**
  [Mac] No code path in this range (or in the keybind lambda) waits on another thread while holding the lock.
  [console] If the key-press thread had held the lock, the game thread would have blocked at its next carrier
  fire (C:5470). Pump rows and minimap game-thread work continue for more than 3 minutes. The key-press
  thread never got past the lock: there is no Lua output after the entry line.
- **(b) Starvation (an unfair mutex re-taken over and over by a busy holder): possible in principle, but not
  the best fit.** Three different threads (zzdiag async, event loop, ST2 async) each blocked on their **first**
  try after 06:59:34 and never got the lock in 2.5 to 3.5 minutes, while one burst of failed RegisterHook
  calls sits right at the start of that window. Also, the reviewer's version, "LoopAsync holds the lock for
  its whole run", does not match this code. LoopAsync Lua runs **without** the actions lock (C:9727-9786,
  the same as U:7254-7313). The thing that holds the lock across Lua is the **game-thread** dispatch scope
  (C:5470), which runs the minimap's game-thread timed loops.
- **Other: leaked ownership on the game thread (Lua `longjmp` past a `lock_guard`): most likely.** It is
  deterministic and matches every fact above, including "the game did not freeze", "minimap F5 works",
  "exactly one keybind entry" and the earlier bisect (the minimap is the mod that makes these
  `pcall(RegisterHook)` calls). The one weak link **[Mac]**: I infer that the three RegisterHook calls failed
  from the missing "Registered" lines. A failure by C++ exception (for example inside StaticFindObject) would
  unwind cleanly and not leak. The `lua.throw_error` failures at C:5994 and C:6043 do leak.
- **Secondary, real but not the blocker today:** the port's dispatch scope holds the actions lock across all
  game-thread Lua (C:5032 / 5470 / 5554 / 5637). Upstream does not (U:4090-4157). Even after the leak is
  fixed, a keybind or async enqueue would wait for a whole minimap drain each time, and could starve while
  the map's 1 ms frame loop is running.

A further risk from the same leak **[Mac]**: if any mod's failing `RegisterHook` (or UnregisterHook /
RegisterCustomEvent / UnregisterCustomEvent: C:2346, 3063, 3106, all in part 1) runs on an async or key-press
thread, that thread keeps the lock. Then the game thread blocks at its next carrier fire and **the game
freezes**.

---

## 5. Self-test (reviewer item 3)

- **AUTO did trigger at 3 minutes**: log 7549, 07:02:15 (boot log 848 at 06:59:15). **[console]**
- **The run never started**: there is no `MEM before`, `RUN start`, any `n/168` line or `SUMMARY`. The
  self-test's `pcall(ExecuteInGameThread, RUN)` is stuck at the lock (C:6216). **[console]** for the absence,
  **[Mac]** for the location.
- So there are **no lookup timings to compare with the 10 ms target**, and no self-test Lua memory numbers.
  This launch cannot tell whether the minimap is still slow.
- The only Lua memory data in this session is two `[MEM]` lines. **[console]**
  - Log 811 (06:59:15): `gc_kb=72 live_kb=101 blocks=1210 allocs=1676 fails=0 errs=0 cxx=0`.
  - Log 917 (06:59:33): `gc_kb=105 live_kb=1182 blocks=14208 allocs=27166 fails=0 errs=0 cxx=0`.
  - No later lines exist, because no later enqueue succeeded.
  - `errs` counts raised Lua errors, so the next build's `[MEM]` lines would show the RegisterHook raises
    directly.
- The boot seam self-test (log 926-932) passed 7/7 with no timings. Its `drain=18228ms` (log 933) is time spent
  waiting for a pump, not lookup time.

## 6. Early lag (~first 2 minutes)

Not explained by the lock map alone. What the data shows:

- 06:59:15 to 06:59:33: **no game-thread pump at all.** **[console]** The 2 carriers armed at start never
  fired, and closures waited 18.2 s (log 933). **[Mac]** The re-scan that armed 66 carriers runs only when a
  Lua binding calls `funcptr_marshal_usable` (C:4669-4682, 5873-5893). Here zzdiag's 18 s probe happened to
  do that (log 908-915). Without such a call, mods could stay inactive longer. This is "mods not working
  yet", not frame lag.
- Measured hitches line up with the **minimap building its UI**, each widget logged on the game thread.
  **[console]**
  - 1115 `[l22] widget/object constructed` lines in the second 06:59:34, during the largest pump gap
    (1431 ms, about 06:59:33).
  - 691 to 866 lines per second at 07:01:50-53, during four pump gaps over 1040 ms between 07:01:33 and
    07:02:03.
  - 6698 such lines in total.
  - **[Mac]** Each of them is a synchronous `Output::send` from the game thread (C:2623-2626).
- 07:00 to 07:01 is nearly silent: 14 log lines, normal pump gaps. **[console]** A steady 2-minute lag would
  need frame-time data, which this build does not log.

---

## 7. What this means for the fix (generic, runtime only, mods untouched, no new includes)

1. **Fix the leak first.** It is small and deterministic. Rule: **never raise a Lua error while a C++ lock
   (or any object with a destructor) is alive in a binding frame.** There are two generic ways to apply it:
   - **Per binding.** In `RegisterHook` (C:5947-6050), check and look things up first, take the lock only
     around the container changes, and raise only after the lock's scope has closed. Do the same in the
     part-1 sites C:2346, 3063, 3106, and C:8612 / 8695 if that path is ever enabled.
   - **One change in the library.** When `throw_error_impl` (LuaMadeSimple.cpp:1564) runs inside a
     registered binding, make it throw a C++ exception. The binding frame then unwinds normally, releasing
     its locks, and the existing `TRY` raises the Lua error after its handler exits
     (LuaMadeSimple.hpp:843-860 already works this way). This restores the PC behaviour, where `longjmp` runs
     destructors. `std::runtime_error` is already used in that file (LuaMadeSimple.cpp:1573), so no new include is needed.
     Raw Lua API out-of-memory errors under the lock would still `longjmp`, so keep the lock scopes short as
     well.
2. **Then shorten the dispatch scope** (the reviewer's pattern, which is upstream's shape):
   - `NativeLuaActionDispatchScope` should claim dispatch ownership under a short lock and then release it.
   - The drains already swap out or mark work under their own short locks: `process_simple_actions` at
     C:4699-4707 and the delayed-actions pass at C:4874-5013. They already merge under a short lock at
     C:5500-5517.
   - All pumps are game-thread-only (C:5026 / 5450 / 5550 / 5633), so the only possible double dispatch is
     nesting on the same thread. The thread-local flag (C:4585) already covers that.
   - Needs a replacement: `uninstall`'s "wait for in-flight dispatch" (C:8405-8411) currently relies on the
     scope holding the lock.
   - The keybind lambda holding the lock while its callback runs (C:2231) is the same as upstream (U:1652-1653).
     Once the dispatch scope is short, that is fine, and PC semantics are kept.
3. **Separate the locks.** The "Lua-state lock" the reviewer wants kept apart already exists: the global Lua
   core lock in platform_locks.c, which matches upstream. Fix 2 makes the actions lock a pure queue lock
   again.
4. **The next launch can tell the causes apart.** Use one labelled build per fix.
   - **Fix 1 alone, with all mods including the minimap.** If F7, F12 and AUTO now work, the leak was the
     blocker. If they still stall, the dispatch-scope starvation remains, so apply fix 2.
   - **The reviewer's split test** (B4 with all mods except the minimap) cannot tell the two causes apart: both
     predict that F7 works without the minimap.
   - **Cheap generic diagnostic.** Add a one-line `[RH] raise under actions lock` before each raise in the
     lock-holding bindings. The `errs=` field of `[MEM]` will also show it.
5. Note: `LuaMod.cpp` already includes `<atomic>` (C:9; O:9). Atomics inside this file need no new include.
   Check against the include-graph gate.

## 8. Files

- This map: `outputs/2026-10-08-l25-prep/round7-staging/lock-fix/map/MAP-2.md`
- Host test (not completed, see section 4.5):
  - `outputs/2026-10-08-l25-prep/round7-staging/lock-fix/map/hosttest-longjmp/lua_pcall_lock_leak.cpp`
  - `outputs/2026-10-08-l25-prep/round7-staging/lock-fix/map/hosttest-longjmp/build.sh`
  - partial objects are in `outputs/2026-10-08-l25-prep/round7-staging/lock-fix/map/hosttest-longjmp/build/`
- MAP-1's stand-in test (another agent): `outputs/2026-10-08-l25-prep/round7-staging/lock-fix/map/hosttest-1/`
