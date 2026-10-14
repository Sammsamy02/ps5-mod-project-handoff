# MAP-1 — lock map, part 1 of 2 (B4 key-press stall)

Scope: candidate `LuaMod.cpp` lines 1-5000, `LuaMod.hpp`, `UE4SSProgram.cpp`
(register_keydown_event + the input loop), `Input/Handler.cpp`. A few sites past
line 5000 are cited only where the verdict needs them (part 2 owns them).
No console contact, no PRX build, no source edits. All code claims are **Mac-only**.
Console facts are **console-proven** from this session's B4 launch-1 evidence only.

## Short version (plain words)

- The keys did not stop working because two threads were waiting on each other
  (a). They also did not stop only because the game thread was "too busy" (b).
- Best fit: **one lock got stuck "taken" by the game thread for good.** On PS5, Lua
  reports an error by jumping straight back to the caller's `pcall` (the Lua library is
  compiled as C). That jump skips the code that releases a lock. Four runtime
  functions take the shared lock and can then raise a Lua error. The likeliest one
  here is `RegisterHook` when the hooked function does not exist. The minimap calls
  it inside `pcall` for three event names when it first sees the player.
- From then on the game thread still "owns" the lock (it may re-enter it, so the
  game, the minimap and the minimap's F5 keep working). Every other thread waits
  forever: the key-press thread (F7/F12) and the self-test's 3-minute timer thread.
- A tiny Mac test shows this exact behaviour with clang. Upstream UE4SS has the same
  code. On Windows the jump releases the lock, which is why the minimap works on PC.
- Separate finding: on PS5 the runtime only reads the **gamepad**. A USB keyboard's
  F7/F12 can never reach `RegisterKeyBind`, even after the lock is fixed. The
  minimap's F5 works because the minimap reads the game's own keyboard input.

## Files read (sha256, first 12)

| file | sha |
|---|---|
| candidate/src/LuaMod.cpp (pinned in `2026-10-08-l26-build-variants/l26-A/SOURCE-SELECTION.json`, build id l26-20261012T040323Z = log line 23) | 4d6bc63b7a90 |
| candidate/src/UE4SSProgram.cpp | 4be1a757fa2d |
| original/src/LuaMod.cpp | fc9589dbb81e |
| `.superpowers/.../native-coherent-runtime-1/source/port/ue4ss/include/Mod/LuaMod.hpp` (the pinned header; not in candidate tree) | e52d29a0b165 |
| `outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/input-handler-coexist/Handler.cpp` (pinned) | 16fcfe494362 |
| upstream: `.superpowers/native-dependency-spike/ue4ss-binding-research-1/upstream/RE-UE4SS/UE4SS/src/Mod/LuaMod.cpp` | (reference) |
| evidence: `launch-l26-B4-launch1-20261012/decoded/ue4ss.decoded.log` | df84e46d023b |
| evidence: `.../harvest-l26-B4-launch1-20261012-supplier-diag.jsonl` (session pid 99 = lines 84450-85816) | 45fd0a5ac056 |
| mod as deployed (read-only): `deploy-20261012/step5-rowcheck-20261012T115518Z/readback/ue4ss__Mods__TownfallMiniMap__Scripts__main.lua` | d8f8d64f84d7 |

## Threads involved (Mac-only)

| thread | created at | what it runs that touches the lock |
|---|---|---|
| **Game thread** (identity published by the R3 marshal) | engine | `funcptr_marshal_hook` (LuaMod.cpp:5403) on every invocation of the 66 armed Blueprint carriers (ReceiveTick / BlueprintUpdateAnimation / UserWidget:Tick overrides, log line 914). It builds `NativeLuaActionDispatchScope` (5470) and runs every game-thread Lua callback under it: ExecuteInGameThread closures, LoopInGameThread*/WithDelay timers (minimap tick 50 ms, minimap frame 1 ms), NotifyOnNewObject callbacks. This session it was the ONLY pump: EngineTick refused (log 621), VMT pump refused (623, 658), marshal pump (918-919). `process_event_hook` (5018) and `engine_tick_hook` (5625) were not registered. |
| **UE4SS-UpdateThread** (`m_event_loop` jthread) | UE4SSProgram.cpp:732, body `update()` 1686 | Mod start (each main.lua, plus `L23GameThreadProbe::Queue` at LuaMod.cpp:8330). Then every ~5 ms (1887): `L23GameThreadProbe::Poll` (1798), queued events (1806-1823), **`m_input_handler.process_event()` (1865)**, `fire_update` (1870-1876). Keybind callbacks run **synchronously on this thread**. If one blocks, input polling stops for everything. |
| **Per-mod async threads** (7, one per started mod, log 860) | LuaMod.hpp:186, 200-203; body `update_async` LuaMod.cpp:9704 | `process_delayed_actions` (9727): LoopAsync / ExecuteWithDelay / ExecuteAsync. These run Lua with **no** global lock (only the per-mod `m_actions_lock` during the splice at 9731-9736). The Lua they call, for example `ExecuteInGameThread` (6216), takes the global lock. |
| Engine worker / loading threads | engine | StaticConstructObject post-callback takes the global lock for every constructed object (9007, part 2). ProcessEvent and carrier calls from workers are rejected before the scope (5026, 5450). |

How key events arrive (Mac-only):
- **Pad:** `PS5PadSource::poll` (pad-coexist `PS5PadSource.cpp:119-164`) reads the pad state, called from
  `Handler::process_event` (Handler.cpp:96-174) on the UpdateThread. Chords come from
  `ue4ss/PS5Pad.ini`: `L3+R3 = F7`, `L1+R1 = F12`. The handler copies the matching callbacks under
  `m_event_mutex` (151-166) and calls them after releasing it (170-173).
  `UE4SSProgram::register_keydown_event` (3115-3135) only forwards to the Handler and takes no other lock.
- **Keyboard:** there is no keyboard input source in the PS5 composition. The only UE4SS input
  source is the composition-supplied pad source (UE4SSProgram.cpp:794-797), and
  `Input/PlatformInit.cpp` registers only Win32/GLFW sources. A scan of all 4141 pinned source
  files found 0 keyboard sources. So keyboard F7/F12 never reach the Handler.
- **Minimap F5:** the minimap reads keys through the game's own input:
  `pc:IsInputKeyDown` / `WasInputKeyJustPressed` (minimap main.lua:3271-3272, 3972-3973, 4043),
  run in its game-thread tick, not through RegisterKeyBind.

## Lock sites

`m_thread_actions_mutex` = **one** `static inline std::recursive_mutex` shared by ALL mods
(LuaMod.hpp:183). It is not a per-mod lock. It guards the queues and flags at LuaMod.hpp:158-168, 182.
`m_is_currently_executing_game_action` is also written without it (4718, 4732, 4949, 4954).
There is no `lua_lock`/per-state Lua lock anywhere, so this mutex is the only thing that
serializes Lua across threads.

Column "raise under lock": the binding can call `lua.throw_error`/`luaL_error` while its
`lock_guard` is alive. On this port that is a longjmp, which skips the guard's destructor.
See the verdict.

| # | candidate line | function | thread | lock | protects | Lua runs under it | waits on another thread under it | raise under lock | upstream RE-UE4SS / original |
|---|---|---|---|---|---|---|---|---|---|
| H1 | LuaMod.hpp:183 | declaration | — | global recursive | everything below | — | — | — | RE-UE4SS LuaMod.hpp same |
| H2 | LuaMod.hpp:192, 233-240 | `m_actions_lock` + manual `actions_lock()/unlock()` | — | per-mod `std::mutex` | `m_pending_actions` (100) | — | — | — | same |
| 1 | 265 | `L23GameThreadProbe::Queue` | UpdateThread (start_mod 8330) | global | probe state + main-state call | yes (`lua_pcall` -> ExecuteInGameThread -> recursive re-lock 6216) | no | no (protected by its own pcall) | none (PS5 diagnostic); original:134 |
| 2 | **2231** | RegisterKeyBind callback lambda (2227-2241) | **UpdateThread** (Handler.cpp:172) | global | Lua-state serialization only | yes (`call_function` 2235) | no | no (C++ throw path, caught 2237) | RE-UE4SS:1653 identical; original:2037 identical |
| 3 | 2085-2201 | RegisterKeyBindAsync callback | UpdateThread | **none** | — | yes, unlocked | no | — | RE-UE4SS:1534 identical |
| 4 | **2346** | `UnregisterHook` | caller's thread (any) | global | hook containers | no callbacks | no | **YES** 2355, 2366, 2373, 2379, 2385, 2392, 2435 | RE-UE4SS:1748 identical; original:2152 |
| 5 | **3063** | `RegisterCustomEvent` | caller's thread | global | `m_custom_event_callbacks`, hook_lua registry | no | no | **YES** 3071, 3081 | RE-UE4SS:2369; original:2798 |
| 6 | **3106** | `UnregisterCustomEvent` | caller's thread | global | `m_custom_event_callbacks` | no | no | **YES** 3114 | RE-UE4SS:2409; original:2838 |
| 7 | 3827-3829 | `ExecuteAsync` | caller's thread | per-mod (manual) | `m_pending_actions` | no | no | no (raises happen before 3827; a `bad_alloc` in emplace_back would skip the manual unlock) | RE-UE4SS:3067; original:3496 |
| 8 | 3854-3861 | `ExecuteWithDelay` | caller's thread | per-mod (manual) | same | no | no | no | RE-UE4SS:3094; original:3523 |
| 9 | 3885-3892 | `LoopAsync` (registration only) | caller's thread | per-mod (manual) | same | no | no | no | RE-UE4SS:3125; original:3554 |
| 10 | **4588-4612** (lock = member at 4590, taken BEFORE the `OwnsDispatch` check) | `NativeLuaActionDispatchScope` | **game thread**; built at 5032 (ProcessEvent, inactive here), **5470 (marshal, active: every carrier call)**, 5554 (VMT, refused), 5637 (EngineTick, refused) | global, held for the whole drain | all queues + flags + **all game-thread Lua** | **YES**: every ExecuteInGameThread closure, every game-thread timer, every pending NotifyOnNewObject | no | no (pcall/TRY inside) | **RE-UE4SS: no such scope.** `process_event_hook`/`engine_tick_hook` take only short guards and run Lua unlocked (RE-UE4SS LuaMod.cpp:4092-4160). original:4229 (already present before B4) |
| 11 | 4700 | `process_simple_actions` swap-out | game | global | action vector | not under this guard (4710-4733), but on PS5 still under #10 | no | no | RE-UE4SS:3796 identical |
| 12 | 4737 | `process_simple_actions` re-queue deferred | game | global | action vector | no | no | no | RE-UE4SS:3825 |
| 13 | 4761 | `process_pending_notify_on_new_object_callbacks` | game | global | pending-notify queue + `m_static_construct_object_lua_callbacks` | **yes** (4823-4843 inside 4760-4860) | no | no | RE-UE4SS:3849 same (Lua under lock upstream too); original:4401 |
| 14 | 4875 | `process_delayed_actions<Executor>` readiness scan | game | global | delayed-action status | not under this guard (4942-4955), but under #10 | no | no | RE-UE4SS:3959; original:4515 |
| 15 | 4958 | `process_delayed_actions` reschedule / erase (`luaL_unref` under lock) | game | global | same | no | no | no | RE-UE4SS:4032; original:4598 |
| 16 | 4686 (decl), 5768 | `m_funcptr_marshal_carrier_mutex` in `r3_marshal_arm_event_carriers` | any (ExecuteInGameThread -> `funcptr_marshal_usable` 4669 -> `r3_marshal_lazy_arm` 5873, at most 1/s and only while carriers are stale) | `std::mutex` | carrier list; full ForEachUObject scan under it | no | no | no | PS5-only |
| P1 | UE4SSProgram.cpp:1814 | `update()` batch move | UpdateThread | `m_event_queue_mutex` | `m_queued_events` | no; events run after release (1820-1823) | no | — | RE-UE4SS UE4SSProgram.cpp:1192 runs events UNDER the lock (same as the non-PROSPERO branch 1827-1839) |
| P2 | UE4SSProgram.cpp:3092, 3108 | `queue_event`, `is_queue_empty` | any / UpdateThread | `m_event_queue_mutex` | same | no | no | — | upstream does not lock `is_queue_empty` |
| P3 | Handler.cpp:151; 178/197; 255 | `process_event`; `register_keydown_event`; `get_events_safe` | UpdateThread | `m_event_mutex` | key set | callbacks run after release (170-173); `get_events_safe` runs its callback under it | no | — | same pattern upstream |

Sites past line 5000 that the verdict uses (part 2 maps them fully):
- **5947 `RegisterHook`**: guard, then raises at 5956, 5966, **5994 ("no UFunction with the specified name")**, 6043.
  Upstream RE-UE4SS has the same shape (guard 4200, raise 4244).
- 6216 `ExecuteInGameThread`: takes the guard before enqueueing. The `[l22] ... queued` line (6250)
  and the `[MEM]` line (6278-6306) both come after the lock.
- 9727-9786: the async LoopAsync drain holds only `m_actions_lock` (9731-9736). LoopAsync Lua runs with no
  lock at all on `async_lua()`, which is a `lua_newthread` of the mod's state (1258-1282). Upstream is identical (RE-UE4SS:7258-7263).
- 8402-8425 `uninstall`: joins the async thread OUTSIDE the lock (8416-8422), so there is no deadlock there.
- No code in LuaMod.cpp, UE4SSProgram.cpp or Handler.cpp waits for another thread while holding a lock.
  The only waits are `sleep_for` (9716, 9722, UE4SSProgram 1802/1887, all unlocked) and `join` (8421, unlocked).

## Console facts used (console-proven, this session only)

1. `ue4ss.decoded.log` line 2410, 06:59:57: `[l22] keybind callback entry key=118 ref=4`. After that there
   are no keybind lines at all. The second F7 callback (zzdiag also binds 118, line 852) never logged an
   entry, so the UpdateThread never returned from the first callback.
2. supplier-diag lines 85604-85609 (pid 99): `pad_l3r3_edge` ... `pad_l3r3_f7_dispatched` (t_ms 1441457) and
   `pad_key_edge 118` (t_ms 1441458). This is the **only** `pad_key_edge` in session 99 (the tag cap is 60,
   Handler.cpp:129-145). After it the pad was never polled again, which matches a stuck UpdateThread.
   The later L3+R3 and L1+R1 presses never arrived.
3. The last `[l22] ExecuteInGameThread queued` line is line 915 (06:59:33). None appear later, up to the end of
   the log (line 7770, 07:02:27). The spawner's F7 handler is `ExecuteInGameThread(toggleMenu)`
   (spawner main.lua:694-695). It never reached 6250, so the UpdateThread was blocked at
   **LuaMod.cpp:2231**, before the Lua ran.
4. Line 7549, 07:02:15: `[ST2] AUTO start`. That is 180 s after the mod started (line 849), from
   `LoopAsync(180000)` in ueselftest main.lua:118. It is followed by 0 `queued` lines, so that async thread
   was blocked at **6216**.
5. The game thread kept running Lua after 06:59:57:
   - 5341 `widget/object constructed via StaticConstructObject` lines. These come from the Lua binding at
     2625; the minimap is the Lua caller (21 uses in its main.lua).
   - Minimap `settings.txt` io-open lines at 07:01:25-07:01:34, which is the F5 menu working.
6. `r3_marshal_pumped`: 227 events from 06:59:33 to the end, largest gap 1.43 s. The game thread reached
   `funcptr_marshal_hook` every second, and right after that point (5470) it takes the lock. It never blocked.
7. `[RegisterHook] Registered ...` appears only at lines 807 and 818 (06:59:15). Verbose success lines do print in
   this build. The minimap's first tick with a pawn calls `registerInteractionHooks(pawn)` (minimap
   main.lua:4095-4106). That function calls `pcall(RegisterHook, classPath..":"..event)` for
   "On Interact Event", "OnDoorEvent" and "OnHideEvent" (main.lua:512-519). No success line followed.
   *Inference (Mac-only):* all three calls ended in a raise inside RegisterHook, around 06:59:34-35. The
   minimap's `[UF]` lookup lines start at 06:59:35 (line 2213).

## Verdict: (a) deadlock vs (b) starvation vs other

**(a) Deadlock (keybind waits on the game thread while holding the lock): ruled out.**
- The keybind path never got the lock (fact 3).
- No code waits on another thread while holding the lock (see the list above).
- The game thread never blocked (facts 5 and 6).

**(b) Starvation (timed callbacks hold the lock for their whole run and re-take it): a real amplifier, not the cause.**
- True on PS5:
  - Site #10 holds the global lock across all game-thread Lua.
  - It re-takes the lock on every carrier invocation (5470). Upstream does not do this.
  - The timed tasks that hold it are game-thread timers: the minimap `LoopInGameThreadWithDelay(50, safeTick)`
    (main.lua:4268) and, after F5/F6, a 1 ms frame loop (main.lua:4031-4035).
- Not true for LoopAsync: it holds no global lock (9727-9786).
- Not enough to explain the facts: two different threads got **zero** acquisitions for 2.5 minutes, including
  the quiet 07:00:04-07:01:09 window. The lock is free between drains there, so a waiter would eventually get in.

**Other: permanent lock-ownership leak on the game thread (best fit).**
- On this port Lua is compiled as C, so a Lua error is a longjmp:
  - LuaMadeSimple.cpp:1427 and 1557-1558 say so.
  - `throw_error_impl` 1564-1597 ends in `luaL_error`.
- Sites 4, 5, 6 and 5947 take `std::lock_guard<recursive_mutex>` and then raise. The longjmp back to the
  script's `pcall` jumps over the guard's destructor. The recursion count never returns to zero, so the
  thread that made the call owns the mutex for the rest of the process.
- In this session the owner must be the game thread:
  - Any other owner would have frozen the game thread at 5470, and it did not freeze (fact 6).
  - The game thread can re-enter, so the game, the minimap and the minimap's F5 keep working (fact 5).
  - Every other thread blocks forever: the keybind at 2231 (facts 1-3) and the self-test at 6216 (fact 4).
- Most likely trigger: the minimap's `pcall(RegisterHook, ...)` on event names its pawn class does not have
  (RegisterHook 5947 -> 5994). This matches the earlier bisect pointing at the minimap.
- Upstream has identical code. Microsoft documents that on Windows x64 a longjmp unwinds C++ destructors
  (not re-checked this session), so there the guard is released. That is why the same unmodified mod works on PC.
- Host test (Mac-only, Apple clang 17.0.0, arm64): `map/hosttest-1/longjmp_lock_leak.cpp`, built at -O2 and -O0,
  results in `run-O2.txt` / `run-O0.txt`, checked=3 failed=0 both times:
  - A: after a longjmp out of a `lock_guard`, another thread cannot lock the recursive mutex.
  - A': it can once the owner drops one level.
  - C: with a C++ exception the guard is released.
- Not yet console-proven. Proof would need a marker at the raise-under-guard sites, or the fix itself.

Confidence: high that (a) is wrong. High that a leaked count on the game thread explains every fact.
Medium that RegisterHook-from-minimap is the specific trigger (fact 7 is evidence by absence).

## Self-test (reviewer point 3)

- The auto trigger fired on time: line 7549 at 3:00 after start.
- Its `RUN` never ran: it was blocked at 6216 (fact 4). There are **no lookup timings** and no PASS/FAIL/SLOW
  table this session, so nothing can be compared against the 10 ms target.
- The Lua memory log has only two lines, both at startup, because `[MEM]` is printed inside
  ExecuteInGameThread after the lock (6278-6306):
  - line 811: `gc_kb=72 live_kb=101 blocks=1210 fails=0`
  - line 917: `gc_kb=105 live_kb=1182 blocks=14208 allocs=27166 fails=0`
- So we cannot yet tell whether the minimap is still slow. The lock is the confirmed blocker.

## What this means for the fix (for the fix agent; Mac-only)

1. **First: no lock may be alive in a frame that can raise a Lua error.**
   - In sites 4, 5, 6 and RegisterHook 5947: validate and raise before locking. Then lock only a tight block that
     changes the container and contains no Lua call that can raise. Or record the error text, leave the block, then raise.
   - This is generic and runtime-only. Mods stay untouched, PC semantics stay the same, and it adds no detour.
2. Then the reviewer's queue pattern for #10: the dispatch scope should not hold the shared queue lock while Lua
   runs (upstream holds only the short swap locks). If game-thread Lua still needs serializing, use a separate
   lock that is never held while waiting on another thread.
3. Keybinds: have the UpdateThread only enqueue (short lock). That way a Lua problem can never freeze input polling.
4. Includes: LuaMod.cpp already includes `<atomic>` (line 9), `<vector>` and `<memory>`. LuaMod.hpp already
   includes `<mutex>` and `<thread>`. Nothing new is needed in LuaMod.cpp; B2's `<atomic>` refusal must have
   been a different file.
5. Optional, cheap leak detector for the next launch: a thread_local depth counter around the guard sites.
   Log once when a game-thread dispatch scope ends with depth != 0.

## Open items

- Keyboard F7/F12 on PS5 needs a keyboard input source (capability gap; separate from the lock).
- Early lag (~2 min) is not settled by part 1. Candidates, all Mac-only and unproven:
  - game-thread minimap Lua runs on the first carrier drain after each delay expires;
  - 1355 synchronous `StaticConstructObject` log lines in 06:59:33-06:59:55;
  - `[UF]` error lines;
  - the carrier rescan in `r3_marshal_lazy_arm` (5873), if carriers go stale.
  Part 2 / timing work should check these.
- Pre-existing upstream hazard (not this bug): LoopAsync Lua on `async_lua()` runs with no lock, on the same
  global state as the game-thread Lua of that mod (1258-1282 vs 9754-9763).
