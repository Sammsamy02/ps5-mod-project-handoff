# DESIGN-1 — MINIMAL lock fix (short queue lock, swap-then-run, per-mod Lua lock only around Lua)

Status: design plus a Mac prototype of the lock layer. No candidate-tree edit, no console contact, no PRX build.
Labels: **Mac-only** = my reading of code or a host test. **console-proven** = B4 launch-1 decoded log
(`evidence/launch-l26-B4-launch1-20261012/decoded/ue4ss.decoded.log`, lines I re-read myself are marked "re-read").
`C:` = candidate `LuaMod.cpp`, `U:` = true PC upstream RE-UE4SS `LuaMod.cpp`, `O:` = the `original/` tree
(the old l23 port, already has `NativeLuaActionDispatchScope`, so it is NOT upstream).
Companion files in this folder: `PARITY-1.md`, `CHANGES-ROWS-1.md`, `proto-1/` (prototype + host fixture + results).

## 1. In plain words

- The F7/F12 keys stopped working because the one shared lock that protects "Lua may run now" never became free
  for the key thread or the self-test thread. Two ways that can happen, and the code has both:
  (a-leak) a Lua error thrown inside four runtime functions that hold the lock skips the release (Mac-only,
  reproduced by MAP-1's test); (b-starve) the game side holds the same lock for whole stretches of mod Lua and takes it
  again on every engine event (Mac-only; my T2 control below starves a waiter for more than 3 s).
- This design does not need to know which one it was. It (1) makes sure no Lua error can skip a release,
  (2) shrinks the shared lock to "touch the queue for microseconds", (3) gives each mod its own Lua lock that the game
  thread only ever *tries* (never waits for) and that the key thread gets first, (4) skips the lock completely when
  nothing is queued, and (5) prints a `[LOCK]` statistics line so the B5 launch tells us which one it was.
- Smallest diff that does all of that: one new self-contained file (`LuaModLockLayer.inc`, ~600 lines, already
  tested on the Mac) plus mostly mechanical edits in `LuaMod.cpp` (about 45 lock swaps, 14 one-line unlocks, 10 ref
  changes, 8 structural edits). No header change, no new `#include`, no detour.

## 2. What I checked in the driver findings, and what is new (all Mac-only unless marked)

Confirmed: no `lua_lock` in the candidate tree; `m_thread_actions_mutex` is one `static inline std::recursive_mutex`
(pinned `LuaMod.hpp:183`, header is NOT in the candidate tree); `NativeLuaActionDispatchScope` (C:4588-4612) holds it
across every game-thread drain (C:5032, 5470, 5554, 5637) and is taken before `OwnsDispatch` is decided; the keybind
lambda logs then locks (C:2227-2241); console-proven (re-read): log line 2410 `keybind callback entry key=118 ref=4`,
line 7549 `[ST2] AUTO start`, only two `[RegisterHook] Registered` lines (807, 818, both 06:59:15), carriers armed 06:59:33 (line 914).

New findings that change the design:

1. **`m_hook_lua` is ONE Lua coroutine per mod, shared by every closure, timer and hook of that mod** (C:1210-1217
   `ensure_hook_thread_exists`; `make_hook_state` returns it every time). Every `ExecuteInGameThread*`/`RegisterHook`
   does `lua_xmove(L, hook_lua, 1)` + `luaL_ref(hook_lua)` (C:6221, 6517, 6552, 6731, 6805, 6957, 7033, 5974, 5987,
   3021...) and the drains run Lua on that same stack. The comment at C:6214 and U:4366 says so ("race ... when async
   thread and game thread both access the same hook_lua state"). That is the real reason the port wrapped whole drains in
   the global lock. **Removing the long hold without changing these `xmove` sites would reopen a stack race.** The
   design changes the ten hot sites to take the reference on the caller's own stack (`luaL_ref(L, ...)`): same registry
   entry, no foreign stack touched (section 6).
2. `try_deferred_loadmap_arm()` (C:5373, called at C:5477 and C:5562) sits inside the dispatch scope. A fast path that
   skips the whole pump when no work is queued would never run that one-shot. The scope takes a `force` argument for it.
3. `ExecuteInGameThread` does `Output::send` (two kinds of line), `lua_gc` and `query_lua_memory` while holding the lock (C:6250-6310). Queue
   sections must hold no I/O; those lines move after the unlock.
4. `funcptr_marshal_hook` comment C:5479-5486 records an EARLIER launch ("launch-2: F7 keybind entries with ZERO Lua
   output and ZERO ExecuteInGameThread queues") that was attributed to the 40K-object scan. Same symptom as B4. Mac-only
   inference: this stuck-lock symptom is older than B4 and was probably mis-attributed.
5. Host test (proto-1 T2 control): a plain `std::recursive_mutex` with a holder that re-takes at once (0.9 ms busy, ~99 %
   duty) starved a waiter for more than 3 s on this Mac (Apple clang 17, arm64, -O2/-O0). The same waiter shape on the new
   layer got its lock in 0.4-0.9 ms. Caveat: B4's game thread was not at 99 % duty in quiet windows (MAP-1 section
   "Verdict"), which is why the leak is still the better fit for B4.
6. Includes: `LuaMod.cpp` already includes `<atomic>` (line 9), `<mutex>`/`<thread>` come with `LuaMod.hpp`. The B2
   refusal was `LuaMadeSimple.cpp`. The new code still uses only `__atomic_*` builtins (reviewer rule, `LmsRelaxedCounter`
   pattern), so it is include-gate neutral either way. One new project file `LuaModLockLayer.inc` follows the
   `VmtTickPumpCore.inc` precedent (declared in `CHANGES.json` `added_headers` with sha; see CHANGES-ROWS-1.md).

## 3. The design in one page

Three lock classes, strict rules:

| class | what | holds | never held across |
|---|---|---|---|
| **Q** queue lock | the existing `LuaMod::m_thread_actions_mutex`, wrapped by `LockLayer::QueueLock` | queues, flags, hook containers, registry `luaL_ref/unref`, key queue | Lua code, a Lua error raise, I/O/logging, any wait, requesting M |
| **M** mod Lua lock | one `ModSlot` per Lua mod (file-scope table, no header change), recursive | non-async Lua of that mod: keybind callbacks, game-thread drains (simple, delayed, notify), Tier-2 script/notify hooks, teardown | waiting for another thread (game thread only *tries*) |
| **C** Lua core lock | existing `platform_locks.c` (process-wide, released while a C function runs) | Lua kernel | untouched |

Rules: R1 Q only for container edits; swap out under Q, release, then run. R2 Lua runs only under M (or on the async
thread, see 6). R3 a game-thread drain never waits for M: `try_lock`, else defer the item. R4 a waiting key gets the lock
first (the game thread yields to a queued key for at most 100 ms). R5 a thread holding mod X may *wait* for mod Y only
if rank(Y) > rank(X); for rank(Y) < rank(X) it makes one `try_lock` and defers/skips. R6 no raise under Q.

Why R5 is deadlock-free: a wait-for cycle must contain an edge from a higher to a lower rank, and those edges never wait.
Proven by the T3 host test (two mods, two threads, 60,000 nested acquisitions, completes, mutual exclusion held; the
naive opposite-order control deadlocks).

## 4. Sites that change (candidate `LuaMod.cpp`, line numbers of the current file)

Tier 1 = on the B4/B5 path or shared by every pump. Tier 2 = inactive on the console in the marshal world
(log 764, 780, 621-623) but shipped code; may slip one build without risk because it keeps today's behavior.

| # | site (C:line) | change | tier |
|---|---|---|---|
| 1 | new block after `namespace RC {` (before the L23 probe, ~C:132) | `#include "LuaModLockLayer.inc"` with `LOCKLAYER_QUEUE_MUTEX = LuaMod::m_thread_actions_mutex`, `LOCKLAYER_EMIT` = `Output::send(STR("{}\n"), ensure_str(line))` | 1 |
| 2 | all 45 `std::lock_guard<std::recursive_mutex> guard{...m_thread_actions_mutex}` (sed) | `LockLayer::QueueLock guard{}` (Producer kind: sets work bits and zeroes wakes on release; a forgotten site is SAFE) | 1 |
| 3 | drain-internal sections C:4700, 4737, 4761, 4875, 4958, 5037, 5047, 5490, 5500, 5564, 5596, 5642, 5652 | `QueueLock guard{QueueLock::Kind::Drain}` (they re-arm from real contents instead). Also the six read-only delayed-action queries (C:7388, 7430, 7472, 7514, 7597, 7682) | 1 |
| 4 | C:4588-4612 `NativeLuaActionDispatchScope` | rewrite (below): no member lock, ctor `(work_mask, exec, force)`: nested check, fast path, short claim | 1 |
| 5 | call sites C:5032, 5470, 5554, 5637 | pass `(mask, exec)`; marshal/VMT pass `force = m_loadmap_deferred_arm_pending`; no other body change except #6 | 1 |
| 6 | the 4 pump final blocks (C:5047+, 5500+, 5596+, 5652+) | after the existing merge: `pump_rearm(guard, game_left, tick_left, notify_left)` and `recompute_wakes_locked()` (scan `m_delayed_game_thread_actions`, min over due times; frame-based EngineTick = now; folded with `take_defer_wake()`) | 1 |
| 7 | C:4693-4745 `process_simple_actions` | per item: `slot = slot_for(lua_data.lua->get_lua_state())`; skip if slot already blocked this pass; `ModGuard mg{slot, Entry::Drain}`; busy -> push to `deferred`, mark slot blocked, `note_defer()`. `luaL_unref` at C:4731 moves into a Drain-kind Q section. `note_work()` + `eigt_run` count | 1 |
| 8 | C:4856-5015 `process_delayed_actions` | same per-item ModGuard; busy -> `status = Active`, `execute_at = max(execute_at, now+1 ms)`; `dly_run` count | 1 |
| 9 | C:4748-4860 `process_pending_notify_on_new_object_callbacks` | Q section only builds `to_execute`; the Lua loop (C:4817-4843) moves out of the guard block, runs under `ModGuard` (busy -> push the pending item back); the `luaL_unref`/erase tail re-takes Q (Drain). Add `object_serial` to the local struct | 1 |
| 10 | C:2227-2241 keybind lambda | log entry (kept) -> `key_enqueue(slot, &lua, ref, key)` -> `key_pump(run_key_event)`; no lock held while waiting | 1 |
| 11 | C:2085-2201 RegisterKeyBindAsync lambda | no lock (as U); covered by the update-loop leak sweep (#16) | 1 |
| 12 | C:2346, 3063, 3106, 5947 (UnregisterHook, RegisterCustomEvent, UnregisterCustomEvent, RegisterHook) | `guard.unlock_early();` immediately before each raise: 7 + 2 + 1 + 4 = 14 one-line inserts (C:2355, 2366, 2373, 2379, 2385, 2392, 2435; 3071, 3081; 3114; 5956, 5966, 5994, 6043). Raise-site text and messages unchanged | 1 |
| 13 | ref creation, 10 sites: C:3087 + `make_ref` (RegisterCustomEvent), 5974-5977 and 5986-5987 (RegisterHook), 6221-6222, 6517-6518, 6552-6553, 6731-6732, 6805-6806, 6957-6958, 7033-7034 | replace `lua_xmove(L, hook_lua->state, 1); luaL_ref(hook_lua->state, ...)` by `luaL_ref(L, ...)` (or `lua.registry().make_ref()` where the value is consumed). Still inside a Q section | 1 |
| 14 | C:6216-6310 `ExecuteInGameThread` | Q section = `ensure_hook_thread_exists`, `luaL_ref`, push, depth snapshot. Throttled `[MEM]`/`queued`/depth logging moves after the unlock (values copied out). `eigt_queued`/`dly_queued` counts in the 7 enqueue sites | 1 |
| 15 | C:481, 487, 491 hook-remove `luaL_unref` | wrap in a Drain-kind Q section (registry discipline, 3 lines) | 1 |
| 16 | `L23GameThreadProbe::Poll()` (C:~288, called every 5 ms from `UE4SSProgram.cpp:1798`, no `UE4SSProgram.cpp` edit) | add: `repair_leak("update-loop", 0)`; `key_pump(run_key_event)`; `maybe_emit(false)`. Async loop `update_async` (C:9704) top: `repair_leak("async-loop", 0)` and `maybe_emit(false)` | 1 |
| 17 | hook callbacks C:456, 654 | `LeakProbe probe{"hook"}` before `call_function` (no M, as U) | 1 |
| 18 | `L23GameThreadProbe::Queue` C:265 | `ModGuard{slot_for(L), Entry::Blocking}` instead of Q across `lua_pcall` | 1 |
| 19 | `LuaMod::start_mod` (C:8297) and `ensure_hook_thread_exists` (C:1210) | `slot_create(narrow mod name)`, `slot_bind(state, slot)` for root, main, async, hook states | 1 |
| 20 | `LuaMod::uninstall` C:8402-8560 | drop the Q read at 8409 (it only reads a thread_local); take `ModGuard{Blocking}` for the whole teardown; wrap each container-erase group (8441-8461, 8516-8541) in a short Q section; `key_purge(slot)`, `slot_unbind_all`; `maybe_emit(true)` | 2 |
| 21 | `script_hook` C:8612-8760 | Q only to snapshot `{lua, ref, identifier}` of the matching hook; run each under `ModGuard{Entry::Nested}`; busy after 2 ms -> skip, `hook_skips++` | 2 |
| 22 | StaticConstructObject post-callback C:9007-9100 | atomic callback counter first (nothing registered -> return before any lock, this callback runs for every object construction); collect ids under Q; run under `Entry::Nested`; busy -> push into `m_pending_notify...` + NOTIFY bit (no loss) | 2 |

Not touched on purpose: `LuaMod.hpp`, `UE4SSProgram.cpp`, `Handler.cpp`, `LuaMadeSimple.cpp`, any mod, the other ~19
register-callback `xmove` sites (C:3021 NotifyOnNewObject, 3138-3332, 3912-4102; they take no lock today, unchanged risk, see section 10).

### Dispatch scope after the change (replaces C:4588-4612)

```cpp
class NativeLuaActionDispatchScope {
    bool OwnsDispatch{false}, PreviousProcessing{false};
    LockLayer::PumpTimer Timer;
public:
    NativeLuaActionDispatchScope(unsigned mask, int exec, bool force = false) {
        if (NativeLuaActionDispatching) return;                       // nested on this thread: no lock, no counter
        if (!force && !LockLayer::pump_should_run(mask, exec)) return; // 2 atomic loads (+1 clock read when a wake is armed)
        LockLayer::QueueLock q{LockLayer::QueueLock::Kind::Drain};    // short claim, released at the closing brace
        PreviousProcessing = LuaMod::m_is_processing_actions;
        if (PreviousProcessing) return;
        OwnsDispatch = true; LuaMod::m_is_processing_actions = true; NativeLuaActionDispatching = true;
        LockLayer::pump_claim(q, mask); Timer.begin();
    }
    ~NativeLuaActionDispatchScope() {
        if (!OwnsDispatch) return;
        LockLayer::QueueLock q{LockLayer::QueueLock::Kind::Drain};
        LuaMod::m_is_processing_actions = PreviousProcessing; NativeLuaActionDispatching = false;
    }
    bool owns_dispatch() const noexcept { return OwnsDispatch; }
};
```

`uninstall`'s old "wait for an in-flight dispatch" (C:8405-8411) is replaced by `ModGuard{Blocking}`: it waits for the
game thread to finish the current closure of that mod, which is the same guarantee, without holding Q.

## 5. Protocols

### 5.1 Key path (fixes the stuck key, and keeps the UpdateThread free)

`RegisterKeyBind` lambda (still on the UpdateThread, as U:1653): log entry; `key_enqueue` (short Q section, 64-slot FIFO,
oldest dropped and counted if full); `key_pump`: for the first queued event whose mod is not busy this pass:
`ModGuard{Entry::Key}` = spin `try_lock` up to 3 ms with `waiters++`. Got it: erase the event, run the Lua callback
(`get_function_ref` + `call_function`, same catch/log as today), count it. Not got it: the event stays queued (FIFO per
mod), the pump moves on to other mods and returns. `Poll()` calls `key_pump` every 5 ms, so a busy mod's key is retried
within one loop. The UpdateThread therefore never blocks and keeps polling the pad (the B4 symptom: no pad events at all
after 06:59:56, supplier-diag 85604-85609, as cited by MAP-1; I did not re-read the diag file).
While an event is queued (or a Key spinner is waiting) the game thread's `Entry::Drain` for that mod returns "busy" and
defers its items; the preference expires after 100 ms so a dead UpdateThread cannot starve the game thread. Mac-only
numbers (T2, 3.5 s, game thread 0.9 ms-busy hot loop): 60/60 keys dispatched, wait mean 0.4-0.7 ms, max 0.93-0.96 ms,
game thread still ran 3,886 items.

### 5.2 Fast path (reviewer addition 1), with the no-lost-wakeup rule

State (file-scope, `__atomic_*` only): `g_work_bits` (GAME, TICK, NOTIFY) and `g_wake_us[3]` (earliest time pump
ProcessEvent/EngineTick/Marshal must look; `kNever` = no timed work).
Every hook starts with `pump_should_run(mask, exec)`: `bits & mask` OR `now >= wake[exec]`. Idle cost = two loads.

Invariant W1: at the end of every Q section, if a queue holds something a pump must handle, then its bit is set or its
wake is at or before the time it should run. Transitions (all under Q):
- Producer section (default for every non-drain `QueueLock`): on release, while still holding Q, set all bits and zero all wakes.
  Order is push, set bit, unlock, so a consumer that takes Q later sees both.
- Claim (drain start): clear this pump's bits. A producer arriving after the claim sets them again; the item went to the
  *pending* queue (processing flag is true) and is merged at the end.
- End of drain: merge pending into active, then `pump_rearm` from the REAL queue contents (game/tick/notify non-empty),
  then `recompute_wakes_locked()` over the delayed list. This covers VMT cap overflow, time-box skips and items put back.
- Deferral (mod busy): the item goes back, `note_defer()` sets wake = now + 1 ms (no spin of lock takes while M is busy).
Clearing never happens outside Q, and the only writer that clears also swaps the queue out in the same pump, so there is no window
where work exists and neither flag is set. Mac-only evidence: T5 pushed 500,000 items from 4 producers (random bursts
and sleeps, 25 rounds) through claim/rearm with zero loss; T7 checks Drain sections never wake and Producer sections always do;
1,000,000 idle calls took the queue lock 0 times (T5). TSAN: 0 warnings.
Wake slots are indexed by the executor's enum value (ProcessEvent 0, EngineTick 1, marshal 2 = `kFuncPtrMarshalExecutor`, C:4624), so `recompute_wakes_locked` is one pass over the delayed list.
Effect on B4's shape: the 66 carriers call the pump thousands of times per frame; with this, only calls that find a due
timer or a queued item take Q (the minimap's 1 ms frame loop still takes it once per millisecond, which is real work).
The stat `pump skips : takes` in `[LOCK]` measures exactly this.

### 5.3 ABBA / nesting (reviewer addition 2)

Per-mod locks are `std::recursive_mutex`, plus a per-thread held-set. Same-thread re-entry (hook inside hook inside one
mod, closure -> CallFunction -> callback) never waits (T4). Rank = registration order; the orphan slot (unknown Lua states)
has the highest rank. Rule R5 above. Honest scope statement: **in the B5 build no Tier-1 path holds one mod lock while
requesting another** (drains release before the next item; keybinds are top-level; hooks and async take no M), so the ABBA
protocol is armed only for Tier-2 script/notify hooks and for turning on hook/async coverage later. It is tested anyway
(T3) so flipping that switch needs no new proof. Fallback when a nested acquire fails: notify -> requeue (no loss),
script hook -> skip and count (`hook_skips`), logged at most once per `[LOCK]` window.

### 5.4 Leak defense in depth (reviewer pattern "no raise under the queue lock")

1. Source: 14 `unlock_early()` inserts (#12). Error text and Lua semantics unchanged.
2. Backstop: `ModGuard` exit, `LeakProbe`, and two loop-head sweeps (UpdateThread `Poll`, async `update_async`) compare the thread's
   queue-lock depth with what the frame entered with and release the surplus, log `[LOCKLEAK]` once per second, count `leaks`.
   Valid because a thread's top-level frames never hold Q legitimately, and any frame *above* a pcall sees a leak only from a longjmp below it.
3. Visibility: `raise_unlocks` counts the explicit unlocks, so a B5 line with `raise_unlocks>0` and no stuck key is console proof that the
   RegisterHook-raise path was exercised and is now safe; `leaks>0` means a site I did not find.
Mac-only evidence: T1 reproduces the B4 shape (longjmp over a live `QueueLock`): control stays stuck, `ModGuard`, `LeakProbe` and the
update-loop sweep free it, the `unlock_early` shape leaks nothing.

## 6. How Lua stays single-threaded per mod

- **I1 (stack ownership)**: each `lua_State` is entered by one OS thread at a time. `main_lua`: UpdateThread (mod start; keybinds under M).
  `async_lua`: that mod's async thread only. `hook_lua` (shared by closures, timers, hooks): game thread only, because after #13 no other
  thread ever pushes onto it (callers anchor the function with `luaL_ref` on their OWN stack; the registry entry is the same).
  The registry free-list (`luaL_ref`/`luaL_unref`, not atomic across API calls) is serialized by Q at every site that matters
  (#13, #15, drain unrefs).
- **I2 (mod level)**: all non-async Lua of one mod (keybind callbacks, drained closures/timers/notify, Tier-2 hooks, teardown) is mutually
  exclusive through M. This is the exclusion the old global lock gave K, D, S and N, now per mod and without ever holding Q.
- **I3 (family level)**: the async thread and one M-holder can be inside the same mod's `lua_State` family together. Same as U (U:7254-7313
  runs LoopAsync unlocked) and what every PC mod is written against. The Lua core lock C serializes the kernel. I did NOT put async under M:
  a long LoopAsync body would then stall that mod's game-thread hooks and key callbacks, and there is no ABBA benefit to buy back.
  A one-constant switch (`Entry::Blocking` around the async `call_function`, same ModGuard) exists if the reviewer insists; cost is stated in PARITY-1 (D4).
- Hook callbacks (`RegisterHook`, C:456/654) stay unlocked, as upstream, so the game thread never waits on a mod; they run on `hook_lua`
  on the game thread, the same thread as every drain. Cross-thread hooks (a worker thread calling a hooked UFunction) are refused by the
  PS5 raw-thread gate in the marshal world (C:5450), as before.

## 7. Reviewer additions, one line each

1. FAST PATH: section 5.2; bits and wakes are `__atomic_*` on file-scope ints (no `<atomic>`); set under Q after the push, cleared only under
   Q by the pump that swaps the queue out; counters `pump calls/skips/takes/empty`, `pump ms_max/ms_win/over4ms`, `qlock takes/contended/hold_max`.
2. PER-MOD LOCKS: sections 3, 5.3, 6; re-entrant; ranked try-lock rule; T3 two-mod/two-thread test beside T2's stuck-key repro.
3. PARITY: `PARITY-1.md` (every entry point, upstream thread and lock vs the fix, 10 deliberate differences with reasons).
4. LOCK STATISTICS: one line starting `[LOCK] ` at most every 10 s from whichever of UpdateThread, async thread or game thread asks first
   (CAS on the last-emit time, so it still prints if one of them is stuck), an `init layer=B5-1` beacon on first use, and a `final` line from
   `uninstall`. Fields in 8. Declared for verify_build in CHANGES-ROWS-1.md.

## 8. The `[LOCK]` line (B5 report input)

```
[LOCK] stats t=<s>s key q=<queued> d=<dispatched> deferred=<n> ovf=<n> wait_ms mean=<x> max=<x>
 | qlock takes=<n> contended=<n> wait_max_ms=<x> hold_max_ms=<x>
 | pump calls=<n> skips=<n> takes=<n> empty=<n> ms_max=<x> ms_win=<x> over4ms=<n>
 | eigt q=<n> run=<n> dly_q=<n> dly_run=<n>
 | modlock game_defers=<n> key_yields=<n> nested_skips=<n> hook_skips=<n> order_viol=<n> leaks=<n> raise_unlocks=<n>
 | mods <name>:wait_max_ms=<x>/takes=<n>/defers=<n>/skips=<n> ...
```
(one physical line; shown wrapped). Sample from the T6 fixture is in `proto-1/run-O2.txt`. Mapping to the B5 report asks:
F7/L3+R3 spawner = `key q/d`, `wait_ms max`, spawner's own Lua lines; minimap and stutter = `pump ms_win`, `over4ms`, `skips:takes`, `qlock hold_max_ms`;
self-test = its own timing table (the 10 ms target) plus `eigt q/run`; the diagnosis = `raise_unlocks`, `leaks`, `game_defers`, `key_yields`.
Acceptance thresholds I propose: `key wait max < 50 ms`, `d == q` within 10 s of each press, `eigt run == q` after 5 s idle, `qlock hold_max < 5 ms`,
`leaks == 0`, `pump skips >= 95 %` of calls.

## 9. Tests

Mac host fixture (`proto-1/test_lock_layer.cpp`, compiles the SAME `.inc`; one compile at a time, vm_stat 3.6-5.0 GB free each time).
-O2, -O0 and ThreadSanitizer (T1 excluded under TSAN, it longjmps over destructors on purpose): **8 tests, 92 checks, 0 failed, 0 TSAN warnings**
(`proto-1/run-*.txt`, `SHA256SUMS.txt`). A test with 0 checks counts as failed (CLAUDE.md gate rule).

| test | what it proves |
|---|---|
| T1 stuck-key repro | B4 shape (longjmp over a live Q guard): control stays stuck; ModGuard / LeakProbe repair it and log `[LOCKLEAK]`; `unlock_early` shape leaks nothing; legit outer level untouched |
| T2 key vs hot game thread | 60/60 keys, max wait 0.93 ms, game thread keeps running; control with the old shape starves a waiter > 3 s |
| T3 two mods, two threads | A->B vs B->A nesting, 60,000 attempts, no deadlock, per-mod mutual exclusion exact; naive control deadlocks |
| T4 re-entrancy | five same-mod nested guards in all modes, A re-entered while B held, all released |
| T5 no lost wakeup + idle | 500,000 items, 4 producers, 25 rounds, zero loss; 1,000,000 idle calls, 0 queue-lock takes |
| T6 stats | 25 simulated seconds -> 3 lines; every line starts `[LOCK] `; fields present; `final` line; sample line |
| T7 wake semantics | Drain sections never wake, Producer sections always do, timed wake and one-shot defer wake |
| T8 key queue | FIFO per mod, busy mod does not block others, preference and its 100 ms expiry, overflow keeps newest 64, purge, no re-entrant pump |

Still to write for the implementation round (not done here, Mac-only): (i) a drain-level fixture that builds the real `process_simple_actions`/
`process_delayed_actions` shapes against stub Lua (deferral order, wake recompute with pending delayed items); (ii) the optional static gate
`lock_raise_gate.py` (every `register_function` lambda that declares a `QueueLock` must call `unlock_early()` before each `throw_error`/`throw_lua_error`;
it must print how many lambdas and raise sites it checked and FAIL on zero). Existing gates `fs_throw_gate.py`, `no_detour_gate.py` and `verify_build.sh --manifest` still apply unchanged;
this design adds no detour and no executable change.
Console evidence cannot come from this round. After B5 the claims "keys work", "no stutter", "self-test timings" are console-proven only from B5's own log.

## 10. Risks, residuals, open questions (please decide)

- **R1 (decision)**: async and hook callbacks are NOT under M (parity, no game-thread waits). If the reviewer wants literal "no two threads in one lua_State family",
  the switch is small but a long LoopAsync body will delay that mod's game-thread closures (they defer) and hooks (they wait up to 2 ms, then skip).
- **R2**: ~19 unlocked register-callback bindings (C:3021, 3138-3332, 3912-4102) still `xmove` onto `hook_lua` from the caller's thread. Unchanged risk (they never took Q);
  only matters if a mod calls them from LoopAsync while the game thread runs that mod. Mechanical follow-up: same `luaL_ref(L)` change.
- **R3**: the one-shot `try_deferred_loadmap_arm` (a detour registration) used to run under the global lock; now under dispatch ownership only. It runs once, on the game
  thread, with `NativeLuaActionDispatching` set. Unproven that the engine-side registry needs the old lock (Mac-only reading: it has its own).
- **R4**: `NativeNameThread::acquire_runtime()` is still taken on every carrier call before the fast path (C:5421). Not changed (unload safety). `[LOCK] pump calls` will show whether it matters.
- **R5**: Q sections also hold `RegisterPostHook`/`StaticFindObject` in `RegisterHook` (as today). `hold_max_ms` will show if that is long.
- **R6**: keyboard F7/F12 still cannot reach `RegisterKeyBind` on PS5 (no keyboard input source, MAP-1). This design makes the pad chords reliable; the keyboard gap is separate.
- **R7**: the early ~2 min lag is NOT explained by locks alone (MAP-2 section 6): no pump for 18 s until a binding re-armed the carriers, plus up to 1,115 synchronous widget log lines per second on
  the game thread. This design only adds evidence (`pump ms_max/over4ms`, `skips:takes`); a rate limit on the widget log line is a separate, one-line change that I recommend but did not include.
- **R8**: `QueueLock` costs two clock reads per take. At the expected take rate (real work only) this is noise; it is the price of `hold_max`.
- **R9**: the prototype's key queue is process-wide with 64 slots; a mod flood could crowd out another mod's keys (oldest dropped, `ovf` counted).
- Diff size: new file ~610 lines (+542 test); `LuaMod.cpp` ~45 mechanical swaps, ~14 inserts, ~10 ref edits, ~8 structural edits, ~250-350 changed lines in total.
  New includes: none. Header edits: none.

## 11. Plain-words answer to "what happens on the next F7 press"

The pad poll finds L3+R3, the callback queues the press (microseconds), tries the spawner's Lua lock for up to 3 ms, and runs the spawner's Lua. If the game
thread happens to be inside that mod's closure, it finishes its item, sees the waiting key, steps aside, and the key runs. If a Lua error happens inside one of the four
registration functions, the lock is released before the error is raised. Nothing the key thread does can wait on the game thread, and nothing the game thread does waits on a mod.
