OURS-SIDE NOTE — entry harness (modbridge-runtime-aligned-entry, build f6530c79)
worker-injection-latency · 2026-10-05 · proposal-only; owners of the entry build decide.

SOURCE AVAILABILITY HONESTY: the entry harness source that produced
`aligned-entry-log-snapshot.log` (markers worker_sleep_result / worker_sleep_complete,
build f6530c7905934e5a8cc201aa9400ebb0) is NOT pinned anywhere in this workspace
(workspace-wide grep over *.c/*.h/*.cpp/*.py/*.md for those marker strings returns
nothing — CLAIM L11). Only its log output exists in-tree. Therefore these are
behavioral specs with log evidence, not file:line diffs. Any diff must be authored by
whoever holds that source tree.

Context that makes these changes valuable again someday: the launch-10 config
(1216aab6…) disables the entry, but launch-9 measured the harness consuming ~5-6 min
inside ONE module_start on a game thread (LATENCY-ANALYSIS §1 stage 7). If the entry is
ever re-enabled on the normal startup path, R2 below is worth ~5-6 min of game-thread
occupancy per launch.

R2 (highest value): return from module_start immediately.
  Evidence: the launch-9 block phase order is strictly serial inside one
  module_start_entered → ... → worker sleeps → Lua VM phase → next
  module_start_entered, and ploader's shellcode blocks the hijacked game thread until
  sceKernelLoadStartModule returns (CLAIM S11/S12, L14). Required behavior:
  module_start spawns one worker thread, hands it the whole harness
  (import validation → startup input → 232 ctors → fproperty/ffield traces → sleeps →
  Lua VM self-test), and returns 0 within milliseconds. Ordering constraint preserved:
  the supplier (composition) still loads first via ini order; nothing in the harness
  may touch game memory before its own fail-closed checks complete (unchanged).
  Keep marker names and sequence identical so the log stays comparable.

R2a (companion, cheap): EINTR-safe sleep.
  Evidence: every historical block shows worker_sleep_result errno=0x4 (EINTR) twice
  before one success (CLAIM L05) — ptrace attach/detach cycles interrupt the sleep.
  If the current code re-sleeps the FULL duration after EINTR, elapsed time inflates
  up to 3x. Use the remainder-preserving pattern already present in this repo:
  nanosleep(&remaining, &remaining)
  (precedent: outputs/2026-10-04-zcode-team/worker-aligned-entry-observer/source/probe/probe.c:177).

R2b (measurement-enabling, cheap): add CLOCK_MONOTONIC milliseconds to every entry
  diagnostic line. The entry log currently has NO timestamps (CLAIM L honest_limits);
  the ~5-6 min figure for launch-9 is a wall-clock-anchor bound, not a stamped
  measurement. With per-line stamps, the ctor/sleep/Lua split becomes directly
  measurable and the sleep duration can be tuned from evidence instead of bounded.

Bounded scope: markers, sequence, fail-closed refusals, and Lua self-test semantics
unchanged; only threading placement, sleep remainder handling, and timestamping change.
