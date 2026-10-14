# Agent F — l24 crash forensics (Townfall PPSA33286, CE-108255-1 at save-load)

Date: 2026-10-08. READ-ONLY analysis. Evidence: `/tmp/l24-diag.jsonl` (28,413 lines,
45 module_start sessions), `/tmp/l24-aligned.log` (47,935 lines, 81 sessions, same
marker `modbridge-runtime-aligned-entry/v1 build=f6530c79…`), `/tmp/l24-ue4ss-full.log`
(608 lines) = UE4SS.log of the crashing launch, `/tmp/l24-ue4ss-4.log` (609 lines) =
same file after one later appended line, `/tmp/l24-census.json` (PRX symbol census).

## Headline finding

**There are zero diagnostic events of any kind between the end of UE4SS init
(13:47:29) and the crash.** The l24 diagnostic vocabulary is init-only: across all
28,413 diag lines and all 179 distinct tags there is no tag for LoadMap,
InitGameState, ProcessEvent, R3 marshal, game-thread pump, ExecuteInGameThread,
menu, or save-load (grep count = 0 in both /tmp/l24-diag.jsonl and
/tmp/l24-aligned.log). Every one of the 45 diag sessions spans ≤5.8 s and ends at
the init self-test `buffered_lua_result`. The crash window is therefore invisible
to l24 instrumentation by construction — silence in that window is absence of
instrumentation, not proof of inactivity.

## Session/pid boundaries

Diag sessions carry boot-relative `t_ms` + `pid`. Pid 222 (mfa failure,
`l24-ue4ss-4.log:609`) shares one boot with the last three diag sessions, which
have monotonically rising pids and t_ms:

| # | pid | t0 (t_ms) | boot-relative | events | outcome |
|---|-----|-----------|---------------|--------|---------|
| 1 | 102 | 130,925   | 2m 11s        | 1083   | runtime_start_ok |
| 2 | 127 | 295,399   | 4m 55s        | 1083   | runtime_start_ok |
| 3 | **144** | **3,743,513** | **62m 16s** | **1083** | **runtime_start_ok → CRASH SESSION** |
| 4 | 222 | 4,657,636 | 77m 38s       | (no diag) | `mfa_activate_failed code=-8` before diag emission |

- **Crash session = pid 144.** Its UE4SS.log is the copy we hold: UE4SS.log is
  truncated per launch, pid 222 never started UE4SS, so the 608-line file is the
  last successful init = pid 144. Corroboration: pid 144's diag tail
  (`copyin_initializer_id_hash_lo 0xd15e21b2` … `module_start_return` …
  `buffered_lua_result`) is byte-identical to the final session of
  /tmp/l24-aligned.log.
- Wall-clock anchoring: pid 144 diag ends t=3,749,279 ms just before UE4SS's first
  line `13:47:23` (`ue4ss-full.log:1`), so boot ≈ 12:44:54 console-clock; pid 222's
  failure lands ≈ 14:02:11 console-clock, i.e. **~15 min after the crash session's
  init**. The user-reported "18:53" for that relaunch implies the console clock
  lags real local time by ≈4 h 51 m; the relative spacing (same boot, 15 min
  later) is the reliable fact. Note the mfa error was *appended* to the crash
  session's UE4SS.log, confirming no UE4SS launch happened in between.
- Sessions #1/#2 (pids 102, 127) are two earlier full inits in the same boot,
  ~2.6 min apart; no logs from them survive. All other 42 diag sessions belong to
  earlier boots/builds (older instrumentation: 495–725 events, or 1083-event pids
  184/196 at 17–19 h uptime in a prior long boot).

## Timeline — final 30 s of diag evidence before the crash (pid 144)

t_ms is boot-relative; console-clock ≈ 12:44:54 + t. The crash itself is minutes
later (menu idle + save-load) and produced no log line anywhere — so "final 30 s
of evidence" is the last 30 s of *anything recorded*, which is still init.

| t_ms | seq | Event | Note |
|-------|-----|-------|------|
| 3,745,276 | 977 | `copyin_unlink_raw=0/errno=0` (last of 64 staging open/unlink cycles) | copyin startup channel precondition setup |
| 3,745,314–3,745,331 | 1000–1012 | anchors OK, image_segment base 0x400000, lmf extent [0x400000,0x0c0602c8), 7 anchors | link-map/anchor wiring |
| 3,747,438–3,747,508 | 1013–1062 | code_image auth: fd 103, read 194,241,424 bytes, `verified_pins=3`, elapsed 2123 ms; path decodes to `/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin`; reason: "whole image authenticated; all function prefixes cached; fd closed" | whole-eboot authentication re-run |
| 3,747,517–3,747,526 | 1063–1067 | `pad_input_deferred=6`, `pad_input_deferred_error=0x80920004`, `settings_mod_source=1`, `pad_input_deferred_input_gate=1` | PS5Pad deferred, input gate ARMED |
| 3,747,547 | 1068 | **`runtime_start_ok`** | modbridge runtime up |
| 3,747,547–3,747,549 | 1069–1071 | `copyin_initializer_id_hash_lo=0xd15e21b2` (before) / `module_start_return=0` / `_hi=0x392d0595` (after) | copyin initializer thread runs **asynchronously around** module_start return — stays resident |
| 3,747,938–3,747,941 | 1072–1074 | `native_thread_import_count=16`, slot ok | worker thread imports |
| 3,749,267 | 1075 | `worker_sleep_complete` | 1.72 s worker hold |
| 3,749,268–3,749,279 | 1076–1083 | buffered Lua self-test, all 0 (OK), ends `buffered_lua_result` | **LAST diag event of the session** |
| ≈3,749,280 → console 13:47:23–13:47:29 | — | UE4SS init (608 lines): type scan, Seeds, addresses; last line `Adding shutdown listener` (`ue4ss-full.log:608`) | **LAST line anyone logged** |
| (minutes: menu idle; user clicks "load save") | — | **NOTHING.** No diag event, no UE4SS line, no copyin attempt, no hook trace | crash window uninstrumented |
| crash | — | CE-108255-1, libkernel.sprx RIP 0x80004920, klog "copyin: has nonsleeping lock" (per task facts) | no accompanying log record |
| 4,657,636 (pid 222) | — | `modbridge module_start failed: mfa_activate_failed code=-8 … UE4SS-settings.ini` (`ue4ss-4.log:609`) | later failed relaunch, same boot |

Key UE4SS init facts (crash session), with line numbers in /tmp/l24-ue4ss-full.log:
- `:584` LoadMap address 0x3c4d460; `:593` InitGameState 0x33db2b0; `:596`
  ProcessEvent 0x859830 — **addresses located, no hook-install confirmation logged**.
- `:589-590` "[R3] refused: EngineTick pre-callback registrations … compiled out
  under R3 marshal mode" / "EngineTick detour unavailable; deferring game-thread
  pump to R3 marshal" — the only R3 lines in the entire log.
- `:528` FName pool 0x9765EC0 admitted via image-segments + VirtualQuery,
  `oracle-probes=0`; `:538` slot 2 FName ctor bound at **0x63d590** and
  behaviorally verified (ByteProperty id 0x3), "detour verification skipped".
- `:603` TypeScan scan-complete core-ready=true (3 missing layout keys).
- `:608` "Adding shutdown listener" — last line. No mods were active
  (`:21` "Setting up mods..." with no mods listed), so hook callbacks, even if
  installed, would have had nothing to log.

## Direct answers

1. **LoadMap / InitGameState hook callbacks before the crash?** No trace of either
   running. The functions were located (log `:584/:593`) but no install line, no
   callback line, no diag event exists. Under R3 marshal mode the per-tick EngineTick
   detour was explicitly refused (`:589`), so the only hook guaranteed to fire early
   and often was compiled out. An unlogged/uninstrumented LoadMap detour firing at
   save-load cannot be excluded — but it also cannot be shown.
2. **Did the R3 marshal pump run?** No evidence it ever started. Its only mention is
   the init-time deferral (`:590`). No marshal activation, tick, or error line in
   UE4SS.log; no marshal tag exists in the diag vocabulary.
3. **Any ExecuteInGameThread item queued/pumped?** No. The string does not occur in
   any evidence file; no queue/pump-tagged event exists anywhere in the 76,348
   combined diag+aligned lines. The 88-byte buffered-Lua test is the worker's init
   self-test, not a game-thread pump.
4. **Copyin attempts / lock errors / FName pool activity at runtime?** None logged.
   All copyin activity in pid 144 is init-phase (cred/jail checks, 64 staging
   open/unlink cycles, initializer id hash). The GATE-1 oracle experiments — io_ctrl
   A–D channel tests and `pool_copyin_probe` results (rc=64 short write, sentinel
   0x7ffdfffe after Control D, rc=4096, rc=0x8002000e, rc=-1) — appear ONLY in
   earlier boots (pids 101/106/108/115/137/138/162/174/188/947/968/1034/1047/1059/
   1072/1084). The final boot's three sessions (102/127/144) ran zero io_ctrl and
   zero probe events; pool admission used the readable-memory path instead.
   The lone `pool_copyin_fault_addr 0x63d590` is pid 106's earlier-boot probe
   experiment, not the crash session. No memory-fault diag exists for pid 144.

## Attribution (best supported)

- What was resident at crash time: modbridge worker thread (16 native imports OK);
  an **asynchronously-running copyin initializer thread** (its id-hash events
  interleave around `module_start_return`), with the census confirming the PRX
  still contains `bridge_kernel_write_copyin_probe_until`,
  `bridge_write_copyin_capture`, `bridge_copyin_range_faulted`,
  `bridge_copyin_initializer_enter/complete/leave`; an armed deferred pad input
  gate; and a fully initialized UE4SS whose only per-tick engine entry point was
  deliberately compiled out.
- The crash signature — kernel-side copyin assertion ("has nonsleeping lock") with
  RIP in libkernel — requires a thread performing a copyin syscall while holding a
  spinlock. Nothing observable from UE4SS ran (no hook, no pump, no queue, no log),
  and a stock game's LoadMap syscalls do not trip kernel lock assertions.
  **Best-supported attribution: the resident modbridge GATE-1/copyin machinery
  (copyin initializer / kernel-write probe path, uninstrumented at runtime in l24)
  interacting with the game thread's save-load (LoadMap) syscall activity.**
  Secondary, non-excludable candidate: an uninstrumented LoadMap detour at
  0x3c4d460 whose callback path left no trace (no mods were installed to log
  anything). The two candidates are distinguishable only with runtime-phase
  instrumentation that l24 lacks.

## l25 recommendations

1. Emit diag events from the runtime phase, not just init: marshal-pump
   start/stop, LoadMap/InitGameState/ProcessEvent detour entry (even a single
   seq-stamped tag each), ExecuteInGameThread queue depth, copyin-probe attempts,
   pad-gate polls. Without these the crash window is a blind spot.
2. Log an explicit "detour installed / not installed" line per hook at init.
3. Consider gating the copyin initializer's kernel-write probe path off during
   LoadMap (or logging it) to test the primary attribution directly.

## Provenance

- Diag parsing: sessions split on `module_start_entered` (45 in diag.jsonl);
  pid/t_ms/seq per line. Aligned log verified same-marker, same final session.
- Strings decoded from word tags: code_image path/reason; pool_copyin path
  `/data/plugins/modbridge-copyin-p000000000063d590-000000221dd1c466-0000006c-00000001`.
- No files other than this report were written.
