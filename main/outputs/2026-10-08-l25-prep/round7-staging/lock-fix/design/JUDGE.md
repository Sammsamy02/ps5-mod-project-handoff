# JUDGE: lock-fix designs 2 and 3 (round 7)

All claims below are **Mac-only** (source reading plus host tests). Nothing is console-proven by this document. Console evidence cited is this morning's B4 log (`evidence/launch-l26-B4-launch1-20261012/decoded/ue4ss.decoded.log`), read-only. No console contact, no build, no edit to candidate or original trees.

## Verdict

**Winner: DESIGN-2 (route keys to the game thread, Q/H/M lock split, lock-free fast path).**
Graft four or five ideas from DESIGN-3 (list at the end). Do not ship DESIGN-3's fairness lock or its "skip the hook callback" rule.

In plain words: DESIGN-2 makes the key press and `ExecuteInGameThread` paths unable to touch the lock that got stuck. DESIGN-3 keeps them on a shared lock and tries to repair or fairly queue it. If the diagnosis is a leaked lock (what the log fits best), the first approach is safe by construction and the second is safe only if its repair net catches everything.

## What I checked myself (Mac-only)

1. **Host tests reproduce.** I compiled each prototype once into the scratchpad (not into the design folders), with `vm_stat` free memory at 4-5 GB, and ran them from a shell loop with a 90 s kill.
   - DESIGN-2 `hosttest-2/test_lockfix.cpp` at -O2: `checked=25 failed=0`, 3 of 3 runs.
   - DESIGN-3 `hosttest-3/lockcore_proto.cpp` at -O2: `checked=14 failed=0`, 6 of 6 runs, no hang.
   - DESIGN-3's own report says 3 of its first 14 runs hung and the cause was not found. I did not see a hang in 6 runs, so I cannot confirm or refute it; it stays an open risk.
2. **The log fits a leaked lock held by the game thread.** The key callback entry line (`[l22] keybind callback entry key=118`, log line 2410) is printed before the lock is taken. After it, the game keeps running (a recursive lock lets the owner back in) while every other thread waits forever. `AUTO start` at line 7549 is printed through the output writer and needs no lock. This is consistent with "game thread permanently owns the shared lock" and also with severe starvation. I cannot separate them from the log alone. Both designs say they cover both; they differ in how safely.
3. **The "widget/object constructed" lines are not the NotifyOnNewObject path.** They are printed by the Lua binding for `StaticConstructObject` itself (`LuaMod.cpp:2625`). The thousands of them are a log burst on the thread that builds the minimap UI. Neither design touches that. This remains the better candidate for the early lag (still unproven).
4. **DESIGN-3's per-mod lock lookup is sound in the vendored Lua.** `lstate.c:304-305` copies the main thread's extra space into each new thread and `lua.h:371` defines `lua_getextraspace`. Nothing else in the candidate uses it. So the lookup idea is valid for Lua 5.4 as vendored; still untested against the PS5 build.
5. **DESIGN-3's timed and try paths cannot beat queued waiters.** In `lockcore_proto.cpp`, `try_lock` succeeds only when nobody is queued and `lock_for` never takes a ticket. The key path and the game-thread drain use only those (1 ms, 50 ms, 5 ms waits). A mod whose lock has Block waiters queued behind a running holder can therefore keep refusing keys and drains. Fairness helps the Block threads (async loops) and hurts the two callers that matter most. The design text half-acknowledges this (section 3.1 versus R6) but does not test it.
6. **Includes.** Neither design adds an `#include`. `LuaMod.cpp` already uses `std::recursive_mutex`, `std::chrono` and `std::this_thread`; both new blocks use `__atomic_*` builtins as the reviewer asked (pattern `LmsRelaxedCounter`).
7. **`LuaMod.cpp.rej`** in the candidate `src/` holds four rejected hunks of the FL-MAP patch around lines 4955-5200, right at the pump functions both designs rewrite. Both designs will conflict with that patch if it is re-applied. DESIGN-3 says "apply last"; DESIGN-2 does not say it but needs the same.

## Scores (1 = poor, 5 = strong)

| Criterion | DESIGN-2 | DESIGN-3 | Notes |
|---|---|---|---|
| Reviewer 1: fast path, `__atomic`, no lost wakeup | 5 | 4 | D2 recomputes the work bits from the real queues under Q on every change, and its test compiles the shipped block unchanged (80,000 items, none lost). D3 sets "all bits dirty" on every guard release, which is safe but coarse; its test covers the flag protocol only, not the pumps. |
| Reviewer 2: per-mod locks, ABBA | 4 | 3 | D2: recursive M, bounded cross-mod wait, then defer or forced entry, 2 s cap; tested including A to B to A. D3: ticket lock plus "skip the hook after 20 ms"; deadlock-free but drops callbacks and has the try-versus-queue problem above. |
| Reviewer 3: parity table | 4 | 4 | Both give a full table. D3 keeps every entry point on its upstream thread. D2 moves keybind callbacks to the game thread (one deliberate difference, with a 400 ms non-game-thread fallback). |
| Reviewer 4: `[LOCK]` line and markers | 5 | 4 | D2 prints every requested field plus per-mod lines, tested for rate limit, final line and field presence. D3 specifies the fields and five markers but has no test for the line. |
| Fixes the console symptoms with high confidence | 5 | 3 | D2: the key path uses only the leaf lock Q and no Lua on the input thread; `ExecuteInGameThread` uses only Q; the game-thread drain no longer holds H. A leaked H cannot stop any of them. D3: the key path and `ExecuteInGameThread` still use the shared lock (now called Q) and rely on L1 (four sites fixed) plus L2 (repair when the leaking Lua call returns). Any leak outside those four sites, or inside a long call, still blocks the input thread. |
| Lua-state safety | 4 | 4 | Both give one-thread-per-mod exclusion with a documented order. D2 is best effort under contention (counted `forced=`); D3 is exact for Block entries but skips hooks after a stall. D2 adds `unwrapped=` as a runtime check. |
| PC semantics kept | 3 | 4 | D3 keeps threads unchanged but adds skipped hook callbacks and start_mod serialisation. D2 changes the keybind thread, adds exclusion, forces rather than skips. Running callbacks late (D2) is closer to upstream than never running them (D3). |
| Generic (any UE4SS mod) | 5 | 5 | No mod or game names in either. |
| Diff size and review risk | 4 | 2 | D2 about +650/-150, 17 sites plus 3 raise restructures. D3 about +900/-250, 47 mechanical renames, about 38 Lua entry wraps, a new ticket-lock primitive in production. |
| No new includes | 5 | 5 | Both zero. |
| Testability on the Mac | 5 | 3 | D2: 25 checks on the shipped block, covering key routing, fallback, tombstone, deferral cap, leak detection, statistics. D3: 14 checks on the primitive; key queue, uninstall race and real-Lua longjmp are all "to do"; plus 3 unexplained early hangs. |
| **Total (of 55)** | **49** | **40** | |

## Why D2 wins, in four points

1. **Structural isolation beats repair.** Under the leak diagnosis the only reliable cure is that the stuck paths stop using the leaked lock. D2 does that; D3 keeps them on a lock it hopes not to leak and repairs afterwards.
2. **Smaller and more testable.** The production block is what the test compiles; the diff is about 30 percent smaller; no new lock algorithm.
3. **Never drops a callback.** D2 forces after a bounded wait and counts it; D3 skips hook callbacks and counts them. A PC mod would not expect a hook to silently not run.
4. **Honest measurement of the lag.** Both admit the early lag is not explained. D2 adds drain window, pump gap, fast skip/take and slow-callback counters.

## What D2 still has to answer (flag for the reviewer)

- **Keys on the game thread** is the one real parity break. Cost: a long key callback hitches the game. Benefit: Lua never runs on the input thread. The 400 ms fallback keeps loading screens working. I accept it, as the reviewer should confirm.
- **A long async action holds the mod lock.** If a mod runs a multi-second `LoopAsync` body, each of that mod's hook callbacks on the game thread waits the full 10 ms bound before being forced. That could look like stutter. Watch `forced=` and per-mod `wait_max_ms` in B5.
- **H leaks outside the four sites are detected, not repaired** (`leak_suspect=`). Graft 1 below closes this.
- **The lock-raise gate is lexical** and cannot see a raise inside a callee. Say so in the gate's output.

## Ideas to graft from DESIGN-3

1. **Repair net (its L2), adapted to D2.** At `ModScope` exit, if the thread's `HGuard` ledger shows more H levels than at entry, unlock the difference on the owner thread. D2 already counts depth; this turns `leak_suspect` from a report into a repair. Needs every H site converted to `HGuard` (mechanical).
2. **`lua_entry_gate.py`.** Every `call_function` / `lua_pcall` / `lua_call` site must sit in a function that creates a `ModScope` or is on a short allowlist; print sites checked (about 38), fail on zero. D2 only has the runtime `unwrapped=` counter.
3. **Extra-space cell lookup** (`lua_getextraspace` set in the LuaMod constructor, inherited by `lua_newthread`). It replaces D2's `mod_entry_of` / `mod_alias` bookkeeping with two loads that cannot raise. Optional: only if a real-Lua host test asserts `cell_of(hook_lua) == cell_of(m_lua)`.
4. **Real-Lua stuck-key repro** (D3's T7 plan reusing `map/hosttest-longjmp/`): replace D2's simulated longjmp in T1 with a real `luaL_error` under a `pcall`. Also keep D3's T2 starvation measurement as evidence, not as shipped code.
5. **Extra counters and heartbeat.** `game lua_ms`, `act_max_ms` with the mod name, game-thread block time, and `rearm n/max_ms` (the object walk in `r3_marshal_arm_event_carriers`); a startup `[LOCK] init` line (an early marker proof); a third heartbeat from `L23GameThreadProbe::Poll` on the UpdateThread.
6. **Release-note items from D3.**
   - Apply the change last, as a patch on the final merged `LuaMod.cpp` (the FL-MAP `.rej` and the staged B2/B3 copies quote the old lock lines).
   - After the leak fix the minimap's three `pcall(RegisterHook)` calls may still fail if the Blueprint events are not found. That is PC-identical and separate from locking; do not read it as a lock failure in B5.
   - Keyboard F7/F12 cannot reach `RegisterKeyBind` on PS5; B5 must use the pad chords (both designs say this).

## Not to graft

- The ticket lock (`FairLock`). Its try and timed paths lose to queued waiters, it adds a new primitive in production, and its test showed unexplained hangs.
- Skipping hook callbacks after 20 ms or 5 ms.
- Renaming all 47 sites to a new guard (bigger diff, more merge conflict, less structural isolation).

## Before staging D2 (Mac-only to-do, no console)

Resolve the `.rej` and merge order; byte-identity check of the staged block against `lockfix_core.inc` with sha recorded; `lock_raise_gate.py` (expect 4 violations on the unmodified candidate, 0 on the staged copy, non-zero scope count both times); include-graph gate (expect 0 new); then fs_throw_gate, no_detour_gate, build, package, `verify_build.sh --manifest` for the exact PRX with the five B5 markers. Every console step needs its own explicit OK, and the game must be confirmed closed before any log read.
