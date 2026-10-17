# CARRIER-DRIVE.md — what actually drives sustained work on console (agent-GY, 2026-10-10)

Inputs: evidence/claude2-menu-session-20261010.log (grep -a, NUL-interleaved),
l26-runtime/typescan-r4/candidate/src/{LuaMod.cpp, UnrealInitializer.cpp,
VmtTickPumpCore.inc}, zzdiag-v11/main.lua. Line numbers below are log line
numbers (1-based, as printed by `grep -a -n`).

---

## (a) The exact drive chain today

Three game-thread pumps exist in the code; in this session exactly ONE is
live, and it is NOT the one that drives periodic Lua work.

1. **EngineTick detour pre-callbacks — DEAD this process.**
   00:53:06 `[R3] refused: EngineTick pre-callback registrations
   (GameThreadInitializer + QueryHarnessDispatch) (detour registration
   compiled out under R3 marshal mode)` + `[R3] EngineTick detour
   unavailable; deferring game-thread pump to R3 marshal`.
2. **EU copied-vtable UEngine::Tick pump — DEAD (refused).**
   00:53:06 `[EU-VMT] arming refused: non-text-ent...` (truncated wide
   render of `NonTextEntryInTable`; line 625; echoed by `[ST] pump vmt
   REFUSED` line 660). Recorded as parked/NOTES-REGISTER.md KI-1; fix
   deferred until after the spawn launch per Claude.
3. **R3 FuncPtr-marshal event carriers — LIVE, and IS the per-frame
   driver of the game-thread queue.**
   - Armed on Blueprint OVERRIDE UFunctions (script funcs, no
     FUNC_Native) found by GUObjectArray scan walking SuperStruct to
     three engine per-frame event bases (LuaMod.cpp r3_marshal_arm_event_
     carriers, ~5506-5566; arm_event_carrier_locked ~5471):
       * `/Script/UMG.UserWidget:Tick`      — FName id 0xF7BD ("Tick",
         [ST] decode line ~707)
       * `/Script/Engine.Actor:ReceiveTick` — FName id 0x28D0 (decode
         line ~708)
       * `/Script/Engine.AnimInstance:BlueprintUpdateAnimation` — id
         0x16AFB (by elimination: both other ids decoded, the candidate
         table has exactly these three bases; no direct decode line in
         this log — 3-line caveat)
   - RegisterPostHook = UFunction::Func swap on the override object; the
     engine calls these events per-frame via ProcessEvent → Invoke →
     Func on the game thread. **So yes: the carriers are hooked to
     genuine per-frame engine functions — that engine invocation IS the
     per-frame tick source** (UserWidget::Tick / Actor ReceiveTick /
     AnimInstance BlueprintUpdateAnimation — by engine contract).
   - Hook body `funcptr_marshal_hook` (LuaMod.cpp:5084-5190): first
     invocation publishes the game-thread identity (line 852, 00:54:02),
     then per fire: IsInGameThreadRaw gate → dispatch scope → drain
     `m_game_thread_actions` (ExecuteInGameThread closures) +
     `m_delayed_game_thread_actions` (ExecuteInGameThreadWithDelay) +
     pending NotifyOnNewObject + requeue pending.

**But sustained periodic/looping Lua work does not ride that queue.**
`LoopAsync` / `ExecuteWithDelay` / `ExecuteAsync` push an AsyncAction into
the per-mod `m_pending_actions` → `m_delayed_actions`, executed by
`LuaMod::process_delayed_actions` (LuaMod.cpp:9438-9497) on the **per-mod
async event thread** started at mod start (`start_async_thread`, loop body
= process_delayed_actions + 5ms sleep, lines 9417-9434), calling the
closure on the mod's async Lua state. Keybinds fire from the input
dispatch path (RegisterKeyBind → register_keydown_event), also independent
of the pump. That is why the 6s probes, keybinds and the menu survive a
dead EngineTick detour AND a dead VMT pump: **the sustained driver is the
mod-owned async thread; the carriers only gate game-thread closures**
(widget construction, engine-call marshaling) — see the [l22] pairs.

**LoopAsync return semantics (this build, load-bearing):**
process_delayed_actions uses remove_if with `result = callback-bool`
(LuaMod.cpp:9462-9489): callback returning TRUE → action ERASED (loop
stops); returning FALSE → kept, created_at reset (loop continues). Log
proof: the 18s loop (`return false`) fired exactly every 18s — 00:53:26,
:44, 00:54:02, :20, :38, :56, 00:55:14, :32 (lines 834, 838, 850, 921,
1018, 1034, 1046, 1059); the 6s T-cycle (`return true`) fired ONCE
(T1 line 832, 00:53:14) and never again.

### Observed cadence per driver

- Async thread: 18s-interval actions fire at exactly 18s across every
  state (timestamps above). Effectively "deadline-accurate periodic"
  (5ms poll).
- Carrier queue: 0 dispatches 00:53:08→00:54:02 (54s); after the first
  fire, every `[l22] queued` is followed by its `drained` within the
  SAME log second (≤1s resolution; true latency is one carrier fire).

## (b) States — where dispatches are present

- **Main menu (pre-save-load), 00:53:08–00:54:02:** NO game-thread
  dispatches. Queued lines 816-817, 834-835, 838-839, 850-851 (4
  closures, depth 1→4) all sat undrained; explicit 30s-deadline miss
  line 836 (`[l23] game-thread self-test NOT DRAINED within 30000ms`).
  The 2 armed main-menu carriers (F7BD Tick overrides, line 782 scan)
  never invoked the hook — first invocation only at line 852. Unknown
  why (frontend widgets presumably not ticking through those override
  UFunctions); the log cannot say more.
- **Save-load/world-init, 00:54:02:** mass arming (74 overrides, line
  849) + FIRST carrier fire (852) + all four backlogged closures drained
  in the same second (857, 872, 888, 904). During the loading screen
  itself there was no queue traffic to observe — honest unknown at
  sub-second granularity.
- **In-world, 00:54:02–00:55:47:** dispatches present, queue→drain
  same-second (lines 921-923, 935-941+995, 1006-1019, 1030-1036,
  1046-1048, 1059-1061). Async 18s cadence unchanged throughout.
- **Mod menu open (F7 00:54:35 line 938; interaction burst 00:55:39-47
  lines 1071-1109):** cadence UNCHANGED — every menu-driven queue
  (ref12) drained same-second; keybind entries (keys 37/38/39/40/118/13)
  keep firing.
- **Paused:** NOT TESTED this session — unknown.

## (c) The cb= rate line (zzdiag-v11/main.lua edit, no PRX change)

Implementation (file now 4094/4096 bytes, `luac -p` clean):

- `local n, ran, n250 = 0, false, 0`
- one sustained counter loop: `LoopAsync(250, function() n250=n250+1
  return false end)`
- every 6s T-line now ends `cb=<n250>` and resets: `.. " cb=" .. n250)
  n250=0` — cb = fires of the 250ms loop in the PRECEDING 6s window.

**Deviation from the tasking, deliberate and documented:** the task
specified the closure "returning true"; this build erases on true
(remove_if law + the T1-only/18s-forever log proof above), so both the
new counter AND the pre-existing 6s T-cycle were set to `return false`
— otherwise cb would read 1 once and the T-line (and every per-state
reading) would appear exactly once, v10-session-style. This also makes
the file match its own "@6s ticks"/EXPECTED-MARKERS "every 6s" contract.
Host simulation of the C++ loop law (5ms poll, erase-on-true) against
the edited file: T1@6.0s cb=23, T2@12.0s cb=24 — meets the ~24/6s
contract.

**What cb= measures — precise scope:** the async-action-thread callback
rate (the sustained driver from (a)), at 250ms granularity, per 6s
window, per game state. It is NOT the carrier fire rate; game-thread
queue behavior stays observable from the [l22] queued→drained pairs.
Interpretation: cb≈24 = driver alive in that state; 0 with T present =
degraded between T-lines; T absent = action pump dead; 1 = semantics
flipped back (distrust the reading). EXPECTED-MARKERS.txt carries the
same contract (GY addendum).

## What the round-4 log will prove

1. Recurring T-lines (every 6s) through main menu → load → world →
   menu-open, each with its own cb reading: quantifies the async
   driver per state (expect 24 everywhere — it is our own thread).
   Round-3 could not show this (single T1 = erase-on-true bug, not a
   stall).
2. Cross-reference vs [l22] pairs: main menu expected to show T+cb=24
   with NO drains (carriers silent in frontend), in-world/menu-open
   same-second drains — confirming the (a) split: async thread
   state-independent, carrier queue world-bound.
3. Any state where cb drops (0/low) with T present names the state
   that starves the async thread — the only remaining failure mode
   for tick-style mods short of Lua death.
