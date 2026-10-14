# agent-GY-carrierdrive.md — carrier-drive analysis + zzdiag cb= line (2026-10-10)

Tasking: answers (a)/(b) on the sustained-driver chain, (c) the cb= rate
line in zzdiag-v11/main.lua, (d) record the VMT arming refusal. Analysis
only + one Lua edit; no builds, no console. Full detail: CARRIER-DRIVE.md.

## (a) Drive chain — compact answer

Sustained periodic work (LoopAsync/ExecuteWithDelay, zzdiag probes,
keybinds) does NOT depend on the carriers at all:

- LoopAsync/ExecuteWithDelay/ExecuteAsync run on the per-mod ASYNC
  EVENT THREAD (start_async_thread; process_delayed_actions + 5ms sleep
  loop, LuaMod.cpp:9417-9497). This thread is the actual sustained
  driver; observed firing a 18s loop at exactly 18s across all states
  (00:53:26→00:55:32, 8 firings).
- Keybinds fire from input dispatch, independent (00:53:59, before any
  game-thread drain).
- The R3 FuncPtr-marshal carriers ARE genuine per-frame UFunction hooks
  (RegisterPostHook Func-swap on Blueprint overrides of UserWidget:Tick
  [FName 0xF7BD], Actor:ReceiveTick [0x28D0],
  AnimInstance:BlueprintUpdateAnimation [0x16AFB, by elimination]) — but
  they only drain the ExecuteInGameThread(-WithDelay) game-thread queue
  (funcptr_marshal_hook, LuaMod.cpp:5084). EngineTick detour dead
  (compiled out under R3 marshal); VMT Tick pump refused (KI-1).

Load-bearing find: this build's LoopAsync ERASES a loop whose callback
returns true (remove_if result law); return false sustains. Log proof:
v10 6s T-cycle (return true) fired once (T1 00:53:14 only); 18s loop
(return false) fired every 18s all session.

## (b) States (log claude2-menu-session-20261010)

- Main menu 00:53:08-00:54:02: ZERO game-thread dispatches (4 closures
  queued undrained, lines 816-851; [l23] 30s no-drain line 836; first
  carrier fire only line 852).
- World load 00:54:02: mass arm (74 carriers, line 849) + first fire +
  all 4 drained same second (857-904).
- In-world + mod-menu-open 00:54:02-00:55:47: every queue drains
  same-second (921-1109); cadence unchanged with menu open.
- Paused: untested (unknown).

## (c) cb= line — LANDED

outputs/2026-10-08-l25-prep/zzdiag-v11/main.lua now 4094/4096 bytes,
luac -p clean: `local n, ran, n250`; `LoopAsync(250, function()
n250=n250+1 return false end)`; T-line ends `cb=<n250>` + reset.
Deviation (documented in EXPECTED-MARKERS GY addendum + CARRIER-DRIVE):
closures return FALSE, not the tasked true — true erases after one fire
on this build (proven above); the pre-existing 6s T-cycle was flipped
true→false for the same reason (v11 as shipped would print T1 only).
Host simulation of the C++ loop law against the edited file: T1@6s
cb=23, T2@12s cb=24. cb measures the async-thread driver rate (what
actually sustains mods), NOT carrier fire rate — scope stated in all
three docs.

## (d) Refusal recorded

parked/NOTES-REGISTER.md KI-1 (new file): `[EU-VMT] arming refused:
non-text-ent...` (log line 625, truncated wide render) =
VmtTickPump FailReason::NonTextEntryInTable — "ent" is "entry", NOT an
entitlement; the GameEngine vtable copy found a non-null word inside
its extent that fails the pinned seed CodeCallOnly text class
(UnrealInitializer.cpp:2179-2191, 2311-2315). Echoed by `[ST] pump vmt
REFUSED` (line 660). Suspected causes + rework sketch included; fix
DEFERRED until after the spawn launch per Claude.

## Files touched (only authorized ones)

- outputs/2026-10-08-l25-prep/zzdiag-v11/main.lua (4094 bytes, syntax OK)
- outputs/2026-10-08-l25-prep/zzdiag-v11/EXPECTED-MARKERS.txt (appended)
- outputs/2026-10-08-l25-prep/parked/NOTES-REGISTER.md (new)
- outputs/2026-10-08-l25-prep/CARRIER-DRIVE.md (new)
- this report
