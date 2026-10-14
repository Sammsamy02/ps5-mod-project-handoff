# PARITY-1 — upstream UE4SS threading vs B4 vs the DESIGN-1 fix

All rows Mac-only (source reading). `U:` = true PC upstream RE-UE4SS
`.superpowers/native-dependency-spike/ue4ss-binding-research-1/upstream/RE-UE4SS/UE4SS/src/Mod/LuaMod.cpp` (7331 lines) and its `UE4SSProgram.cpp`.
`O:` = `outputs/2026-10-08-l26-runtime/typescan-r4/original/src/LuaMod.cpp`. NOTE: `original/` is the older l23 PORT, not upstream (it already has
`NativeLuaActionDispatchScope`, O:4229), so upstream facts below are read from the `.superpowers` upstream tree and cross-checked against `original/`
where noted. `C:` = candidate (B4). "Global lock" = `LuaMod::m_thread_actions_mutex`. Fix terms: Q = queue lock, M = per-mod Lua lock, see DESIGN-1 section 3.

Threads (upstream): **UpdateThread** = `UE4SSProgram::update` on the `m_event_loop` jthread (UE4SSProgram.cpp:416, 1104, 1229 `process_event`); **game thread** =
whichever thread runs the ProcessEvent/EngineTick hook or the hooked UFunction (normally the engine game thread; upstream does no thread gating);
**async thread** = one `std::jthread` per Lua mod, `update_async` every 5 ms (U:7231).

## Entry points

| entry point | upstream thread and locking | B4 candidate | DESIGN-1 fix | difference |
|---|---|---|---|---|
| mod start, `main.lua` | UpdateThread; no lock | same; `start_async_thread` runs BEFORE main.lua (C:8307) | same; UpdateThread loop-head leak sweep | none |
| `RegisterKeyBind` callback | UpdateThread (inside `process_event`); global lock held for the whole Lua call (U:1653) | same plus entry log BEFORE the lock (C:2228-2231); pad source only on PS5 | UpdateThread; press is queued (short Q), run at once if the mod lock M is free within 3 ms, else retried every 5 ms poll; M held, Q not held | **D1, D2** |
| `RegisterKeyBindAsync` callback | UpdateThread; no lock (U:1534) | same (C:2085) | same + loop-head leak sweep | none |
| `ExecuteInGameThread` enqueue | caller's thread (any); global lock across `make_hook_state`, `xmove`, `luaL_ref`, enqueue (U:4366-4368); queue chosen by `m_is_processing_actions` | same plus `[l22]`/`[MEM]` logging and `lua_gc` under the lock (C:6250-6310) | caller's thread; Q only for ref + push; ref taken on the caller's own stack; logging after Q | **D3** |
| `ExecuteInGameThread` closure run | game thread via EngineTick/ProcessEvent hook (U:4090-4157); swap-out under lock, **Lua runs with no lock** (U:3796-3830) | game thread via `funcptr_marshal_hook` (C:5403, carrier post-hook, 66 carriers) inside `NativeLuaActionDispatchScope`: **Lua under the global lock, lock re-taken on every carrier call** (C:5470) | game thread, same pump; swap-out under Q; each item under its mod's M by `try_lock` (busy -> deferred to the next pump, same-mod order kept); fast path skips idle calls | **D1, D5, D6** |
| `ExecuteInGameThreadWithDelay`, `LoopInGameThread*`, `...AfterFrames`, `Retriggerable...` | enqueue under global lock (U:4465-5002); run in the same hook as above, readiness scan under lock, Lua unlocked (U:3959-4032) | same as upstream shape but inside the dispatch scope (Lua under lock) | enqueue under Q (ref on own stack); run as the row above; a timer wake (`g_wake_us`) replaces "look at every call"; timer set/reset/unpause/clear are Producer sections | **D1, D3, D6** (timer precision: same as B4, a due timer is seen on the next carrier call) |
| `Pause/Unpause/Cancel/Reset/Set DelayedAction`, `Is*`, `Get*`, `ClearAll` | caller's thread; global lock for the lookup | same | same, queries do not wake pumps | none |
| `LoopAsync`, `ExecuteAsync`, `ExecuteWithDelay` | per-mod async thread; Lua on `async_lua`, **no global lock**, per-mod `m_actions_lock` only for the splice (U:7254-7313) | same (C:9727-9786) | same; no M; loop-head leak sweep | **D4** (kept equal to upstream) |
| `RegisterHook` registration | caller's thread; global lock for the whole binding; raises a Lua error with the lock held (U:4199-4244) | same shape (C:5947-6043); on PS5 the raise is a `longjmp` and **skips the guard destructor** (leak, B4 suspect) | Q only; `unlock_early()` before each of 4 raises; refs taken on own stack | **D3, D7** |
| `RegisterHook` callbacks (native hooks, `lua_unreal_script_function_hook_pre/post`) | thread that calls the UFunction; **no lock** (U:132-236) | same (C:456, 654); PS5 raw-thread gate refuses worker threads in the marshal world | same + `LeakProbe` | none (hooks stay unlocked, D5) |
| `UnregisterHook`, `RegisterCustomEvent`, `UnregisterCustomEvent` | global lock; raise under lock (U:1748, 2369, 2409) | same, same PS5 leak | Q; `unlock_early()` before every raise (7 + 2 + 1 sites) | **D7** |
| script hook for non-native BP functions (`script_hook`, ProcessInternal post-callback) | caller's thread; **global lock for the whole callback**, Lua under it (U:6295) | same (C:8612); inactive in the marshal world (log 764) | Tier 2: snapshot under Q, run each under M (`Nested`: wait <= 2 ms, then skip and count) | **D8** |
| `NotifyOnNewObject` delivery, game thread | Lua runs inside the StaticConstructObject post-callback, global lock held (U:6631-6680) | same (C:9007); not registered in the marshal world (R3 gate, log 780) | Tier 2: collect ids under Q, run under M; busy -> queued for the drain (no loss) | **D8** |
| `NotifyOnNewObject` delivery, queued (non-game thread) | queued under global lock; drained in the pump with **Lua under the global lock** (U:3849) | same (C:4761) | Q builds the list; Lua runs outside Q, under M by `try_lock`; busy -> put back with its serial | **D1, D6** |
| load-map / init-game-state / begin-play / end-play / console-exec / `CallFunctionByName` callbacks | thread that triggers the engine event (game thread); no lock | same (C:8907-9674) | same (Tier 2: add `LeakProbe`) | none |
| mod uninstall / hot reload | UpdateThread; global lock for the whole teardown including `fire_on_lua_stop` and `lua_close` (U:6111) | same + refuses a same-thread uninstall during dispatch (C:8402-8411) | M (blocking) for teardown; Q only around container erases; key queue purged; refuses same-thread-in-dispatch as before | **D1** |
| `L23GameThreadProbe::Queue` (PS5 diagnostic) | none upstream | global lock across `lua_pcall` (C:265) | M (blocking), no Q across Lua | PS5-only |
| `UnrealInitializer` pumps (EngineTick, ProcessEvent, VMT, marshal) | upstream registers ProcessEvent/EngineTick hooks only | PS5 adds marshal (C:5403) and VMT (C:5542) drains with the same scope | all four pumps use the same scope/fast path | PS5-only, unchanged shape |

## Deliberate differences, each with its reason

- **D1 per-mod lock M instead of one global lock.** Upstream and B4 serialize keybinds, notify and script hooks (and, in B4, whole drains) on one lock. Each mod has its
  own `lua_State` (own `new_state()` per `LuaMod`), so cross-mod exclusion protected nothing; per-mod M keeps exactly the within-mod exclusion B4 mods were verified against
  and removes cross-mod waiting. Reason: no thread may wait for an unrelated mod's Lua.
- **D2 a key may start a few ms after the press when its mod is busy.** Upstream blocks the UpdateThread (and with it all input polling) on the lock; here the press is queued and
  retried every 5 ms, with the game thread yielding to it. Reason: B4 shows the blocking form can freeze input polling for the whole session. Order per mod is FIFO; at most 64 queued
  events (oldest dropped, counted `ovf`).
- **D3 function references are taken on the caller's own Lua stack** (`luaL_ref(L, ...)`) instead of `xmove` to the shared `hook_lua` first. Same registry entry, same later
  `get_function_ref`. Reason: `hook_lua` is one coroutine shared by every closure of the mod; the old global lock was the only thing keeping other threads off its stack, and the
  long hold is being removed. Upstream has the same latent race (its own comment, U:4366).
- **D4 async callbacks run outside M**, as upstream. Reason: a long `LoopAsync` body must not stall that mod's game-thread closures or key callbacks; the Lua core lock covers kernel
  integrity. Cost of the opposite choice is in DESIGN-1 section 6 (R1).
- **D5 hook callbacks run outside M**, as upstream. Reason: the game thread must never wait for a mod. Within one mod they cannot overlap each other (same thread).
- **D6 a drained item whose mod is busy is deferred (<= 1 ms retry), not waited for.** Upstream never waits either (its drains hold no lock); B4 waited on the global lock. Same-mod order is kept.
- **D7 four bindings release Q before raising a Lua error.** On MSVC x64 a `longjmp` runs destructors so upstream's guard is released; on the PS5 C build it is not. The error text,
  type and timing are unchanged; the only difference is that Q is already free when the error propagates.
- **D8 (Tier 2) skip/requeue instead of unbounded wait.** A script hook that cannot get M within 2 ms is skipped and counted (`hook_skips`); a notify is requeued. Only reachable when
  two mods' Lua nest through hooks, which no Tier-1 path does.
- **D9 pump fast path.** Upstream runs the drain on every hook call. Here an idle call is two atomic loads. The observable Lua behavior (order, thread, timing granularity = next carrier call
  after the due time) is the same; `try_deferred_loadmap_arm` is forced through the fast path so its one-shot still runs.
- **D10 `[LOCK]` statistics line** and `[LOCKLEAK]` line are new log output; no Lua-visible change.

## Things that stay different from upstream and are NOT touched here

- PS5 has no keyboard input source (MAP-1): `RegisterKeyBind` only fires for pad chords from `PS5Pad.ini`.
- PS5 refuses ProcessEvent/EngineTick registration in the marshal world, so the carrier post-hook is the pump; `NotifyOnNewObject` and `script_hook` are not delivered there (log 764, 780).
