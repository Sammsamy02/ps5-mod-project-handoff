# Agent Z10 — adversarial re-verification of Agent Q's copyin-probe retirement claim

Date: 2026-10-08. READ-ONLY except this report. Target claim (agent-Q-copyin-probe.md):
the copyin sink bank is fully retired before init ends, NO post-admission caller of the
kernel-write copyin path exists across all 374 composed units, therefore the resident
copyin probe could not have issued the kernel copyins implicated in the CE-108255-1.

## VERDICT: NARROWED — Q's structural claim about the probe SURVIVES every attack;
but the crash evidence in the klog harvest identifies the actual crash driver, it is
NOT save-load, and it is a post-admission modbridge-thread → game-code path Q listed
only as "something not yet instrumented".

---

## 1. Re-entry paths (vector 1) — none found

- sigaction: exactly one registration in all 374 composed units:
  `native-coherent-runtime-1/source/port/unreal/src/Core/Misc/Crc.cpp:400-416`
  (CRCRuntimeHWSupportDetector, static-init-time SIGILL probe, handler restored at
  Crc.cpp:416 before the ctor returns; handler body is a siglongjmp back to the same
  thread's sigsetjmp at :410). Init-phase only, restored, no copyin interaction.
- atexit / on_exit / timer_create / setitimer / kqueue / pthread_cleanup /
  pthread_atfork: zero hits across all 374 unit sources (grep of every path pinned in
  `2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json` selected_units, 374/374
  present on disk).
- thread_local: one, `LuaMod.cpp:4226` (`NativeLuaActionDispatching{}` bool, trivial
  dtor, no re-entry).
- Lazy static initializers post-init: none call bridge symbols — see §4.
- Bridge side (`2026-10-08-l24-package/lineage/lifecycle/runtime_start_bridge.cpp`):
  no signal/atexit/timer/thread machinery; its only thread involvement is the
  `std::hash<std::thread::id>` at bridge:1065-1067.
- Probe worker (`lineage/probe/probe.c:57-83`): one-shot (sleep 5 s → 88-byte owned
  Lua self-test → `return NULL` at :73). No re-arm.

## 2. Retained artifacts (vector 2) — cannot re-arm or dispatch

- Fault ledger `g_copyin_faults` (bridge:910): written ONLY at bridge:1296 inside
  `probe_until` (owner-gated); read only via `bridge_copyin_range_faulted`
  (bridge:1131-1135, returns 1 fail-closed for non-owner BEFORE reading the ledger).
  Exclusion-only; no dispatch.
- 3×4 KiB captures (bridge:1126-1130): written only by the `sceKernelPread` at
  bridge:1324 inside probe_until; readers (`bridge_write_copyin_capture` bridge:1212,
  allows()-gated at :1217) copy OUT to callers; no write-back path to game memory.
- Code cache `g_code_cache` (bridge:1139): consulted read-only
  (`bridge_code_cache_context` :1198, `bridge_code_cache_signature` :1205, both
  allows()-gated); cleared only by `bridge_startup_resources_release` (bridge:1141)
  from the module_stop paths (bridge:1612/1632). A fault on a page covered by the
  cache dispatches nothing — the cache has no fault hook.
- Ack sink / diag channel / UE4SS log fds: all write kernel-side from
  modbridge-owned buffers only (`lineage/diagnostic.c:49,98`;
  `lineage/lifecycle/runtime_module_entry.c:220,251`; runtime_owner.cpp:166-204
  transitions). State-transition-driven, not save-load-driven.

## 3. fd-retirement atomicity / TOCTOU (vector 3) — closed in the crash scenario

- `allows()` is THREAD-EXACT (`lineage/CopyinStartup.hpp:26-29`: `initializer_ ==
  std::this_thread::get_id()`), so the parent-fd check at bridge:1256 is reachable
  only by the owner thread. `complete()`/`leave()` are also owner-gated
  (bridge:1074, 1080) and run on that same thread — single-thread interleaving, no
  TOCTOU between release and dispatch.
- Thread identity chain verified end-to-end: RuntimeOwner spawns the init thread
  (`2026-10-07-l19-r3-full-build/units/runtime_owner.cpp:36`), `enter()` at :101 on
  that thread; backend initialize calls `program_->init()` synchronously
  (`native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp:41-53`);
  `setup_unreal()` runs inside init() on the same thread
  (`2026-10-08-l23-runtime/thread/src/UE4SSProgram.cpp:661,1014`); seed consumption
  + `copyin_cleanup.complete()` are inside setup_unreal's chain
  (`typescan-r3/candidate/src/UnrealInitializer.cpp:1358,973,1148`). So complete()
  is NOT a silent non-owner no-op.
- The one ungated release is `bridge_startup_resources_release` from
  `modbridge_runtime_stop` (bridge:1612 no-owner path; :1632 after
  `StopRuntimeWithAck`), which joins the init thread first (runtime_owner.cpp:64
  `thread_.join()`; CopyinStartup.hpp:8-9 "stop joins this owner before its fallback
  release"). `std::exchange` on `handle.fd` (bridge:1048 vs :1262) gives
  exactly-one-winner even in a hypothetical race. module_stop never ran in any
  crash session (process died in crash; no stop events).
- Retirement observability gap (Q's l25 overlay addresses it): successful closes in
  `bridge_copyin_directory_release` are diag-SILENT (only close FAILURES log,
  bridge:1050-1052). Retirement is proven structurally (thread identity + ordering),
  plus the crash-log ordering: `copyin_cleanup.complete()` (r4:1148) precedes the
  "[Seeds] slot 2 FName ctor bound … detour verification skipped" line (r4:1149,
  present at ue4ss-4.log:538 region, 13:47:24, five seconds BEFORE init end).

## 4. Post-admission kernel-write producers (vector 4)

- Grep of all 374 unit sources for `sceKernelWrite|sceKernelPwrite|pwrite|
  process_vm_writev|process_vm_readv|vm_write|copyin(|copyout(|sceKernelCopyIn|
  sceKernelCopyOut`: ZERO hits. The only kernel-write producer in the whole PRX is
  the bridge itself (packaging side). Q's "no post-admission caller of the
  kernel-write copyin path" is corroborated.
- Nuance on Q's wording: "only UnrealInitializer.cpp references the admission
  engine / copyin bridge symbols" — imprecise. `runtime_owner.cpp:6-7,93,101` also
  references `bridge_copyin_initializer_enter/leave` (init/shutdown-time only; no
  kernel-write path; conclusion unchanged).
- Runtime FName path: `native_fname_thunk` (r4:959-967) is installed session-long at
  r4:1142 (`FName::ConstructorInternal.assign_address`). `rc::fnameabi::invoke`
  (lineage/NativeFNameAdapter.hpp:39-49) does a UTF16 copy into a 1025-unit STACK
  buffer and a plain indirect call — no kernel write. Hazard found (not the observed
  crash): on a pathological name (>1024 units / unpaired surrogate, e.g. from save
  data) invoke returns !ok and the thunk calls `seed_refused` — `[[noreturn]]`
  THROW (r4:734-740) on the GAME thread through the ctor hook = terminate shape.
- `bridge_io_controls` (bridge:1349) calls the probe at bridge:1478 but is
  exported-without-caller; reaching it requires a foreign module to NID-resolve the
  bridge export. Zero diag evidence (its distinctive `copyin_jail`/Control tags are
  absent from pid 144). Theoretical only.

## 5. Crash-evidence cross-check (vector 5) — the claim's context is WRONG in a way
   that matters more than the probe question

- Diag stream verified: crash session = pid 144, 1083 events, ZERO
  `pool_copyin_probe*` / `io_ctrl*` events (only `probe_lifecycle_result` seq 488,
  an unrelated lifecycle tag); all copyin tags are init-phase (cred/jail/startup
  open/unlink/parent/id-hash). Stream ends `buffered_lua_result` t_ms=3,749,279
  (seq 1083). Aligned log's final session is byte-identical. No second stream: the
  ab2 files are 14:06-14:18 A/B artifacts; /tmp/l24-trackb is OFFSETS.md only.
- NEW kernel evidence: `/tmp/l24-harvest/klog-20261008-133536.log` (capture window
  13:35:36→14:32) contains EIGHT fatal-signal dumps; six are the SAME crash
  (pids 144, 178, 193, 208, 251, 266): reason "abort is called", rip
  0x80004920 (libkernel copyin), "copyin: has nonsleeping lock", and BYTE-IDENTICAL
  21-frame backtraces:
  `0x40f918 0x5e3ad8 0x575197 0x4b2379 0x4b4b75` (5 GAME eboot frames,
  0x400000-0x787c000) over `0x80b2d36b 0x80b81f80 0x818b6840 0x80ad6392 0x80ad4a14
  0x80ad486f 0x80b5068a 0x80b4bdd3 0x80db89fe 0x80db7ded 0x81c0b4b7 0x81c0e094
  0x81c0fc52 0x81c0fbde 0x81c0f9b0` (16 MODBRIDGE frames, PRX xotext
  0x80a8c000:0x81cb4000) ending at the libkernel thread trampoline 0x8000763d.
  I.e. a MODBRIDGE-SPAWNED THREAD (outermost frames are std::thread/function
  trampolines incl. a vtable+0x10 virtual dispatch) called into GAME code, and the
  syscall inside that game code tripped the copyin-with-nonsleeping-lock abort.
  NO probe/sink/bridge-write frames anywhere in the stack — the probe is
  kernel-exonerated, stronger than Q's structural argument.
- Crash timing: the dump sits at kernel-uptime ≈3750.93 s (OnionHEN reconcile line
  `[3750.930]` immediately preceding `mDBG …`/fatal-signal block), i.e. ~1.65 s
  after pid 144's last diag event (3,749.279 s) and ≈1 s after the last UE4SS.log
  line (13:47:29). F's "minutes later (menu idle + save-load)" is contradicted.
- Determinism and settings-independence: ALL FOUR surviving UE4SS logs
  (/tmp/l24-ue4ss-2.log:608 13:41:48, /tmp/l24-ue4ss-3.log:608 13:45:38,
  /tmp/l24-ue4ss-full.log:608 13:47:29, /tmp/l24-ab2-ue4ss.log:602 14:07:39) end at
  exactly "Adding shutdown listener" — NO session ever emitted the next expected
  line "Starting callback garbage collector!" (r4:542). The crash window is
  therefore UnrealInitializer.cpp:528-541: after
  `UObjectArray::AddUObjectDeleteListener(&FShutdownDeleteListener::…)` (r4:528)
  and before the GC-start print (r4:542). The identical stacks recur across the
  14:0x-14:2x A/B relaunch cycles (settings-arm-both-on / initgs-only /
  loadmap-only variants) — hook arming is irrelevant to this crash.
- Mechanism identification: `UObjectArray::AddUObjectDeleteListener`
  (native-coherent-runtime-1/source/port/unreal/src/UObjectArray.cpp:379-387)
  does `GUObjectArray->GetUObjectDeleteListeners().Append(TArray{Listener})` —
  grows a GAME-memory TArray from the MODBRIDGE INIT THREAD. Disassembly of the
  innermost modbridge frame 0x80b2d36b (va 0xa136b, via the pass-1 PRX twin)
  shows a 4-arg indirect call `call rax` (rdi/rsi/rdx/ecx from stack slots,
  target from a stored function object) into the game — consistent with the
  RC Function<>-wrapped game allocator/TArray-grow path; the 5 game frames are
  that called game function's internals ending in the syscall whose kernel
  copyin hit the held nonsleeping lock (game allocator/listener-array lock
  racing the game thread). Precise symbolization of the 5 game frames was not
  possible (no local eboot matches the authenticated image sha 3b6cfad4…; local
  copies cbb4ee4d/3374b5ab/afe0a2a5 are older).
- Note: "copyin: <thread> has nonsleeping lock" is also emitted as a per-thread
  annotation in OTHER crashes (pid 167's page-fault dump names RTHeartBeat /
  SceShareLibContentEventThread, klog lines 6521-6809); for the six game crashes
  the annotation is on the faulting thread with rip inside copyin — the
  assertion is the abort cause there.

## 6. Provenance discrepancies found (do not change the verdict)

- Q cites the l24 lineage bridge base sha `89e4b643…`; the actual
  `2026-10-08-l24-package/lineage/lifecycle/runtime_start_bridge.cpp` hashes
  `ba095f56e0a8454ff61447b1ed731b2d642181f7`, and NO copy matching 89e4b643 exists
  under outputs/. Q's cited line numbers all match the actual file, so this is a
  stale/incorrect sha citation, not a wrong analysis.
- The diag/aligned marker build id `f6530c7905934e5a8cc201aa9400ebb0` matches all
  sessions; the pass-1/pass-2 PRX twins hash 2e712a8b… per FINAL-RECEIPT.json.

## 7. Residual post-admission producers, ranked by crash/save-load plausibility

1. **UObjectArray listener registration at init end** — UnrealInitializer.cpp:528
   (AddUObjectDeleteListener → UObjectArray.cpp:379-386 game-TArray Append → game
   allocator on the modbridge init thread → syscall → kernel copyin with nonsleeping
   lock). PRIME suspect with direct kernel-backtrace evidence; fires ≈1 s after the
   last log line of every session; NOT save-load-triggered (identical across
   automated A/B relaunches with no user interaction). This is post-admission
   (after r4:1148 complete()) and is exactly the "UE4SS's own thread interacting
   with game memory" class.
2. **Runtime FName thunk** — UnrealInitializer.cpp:959-967/1142: session-long hook;
   every FName (incl. save-load) runs game ctor from whichever thread calls it;
   pathological UTF16 → `seed_refused` [[noreturn]] throw on the game thread
   (r4:734-740, 965). Real save-load hazard, wrong signature for the observed klog.
3. **Searcher-pool population / StaticConstructObject post-callback**
   (UnrealInitializer.cpp:481-524, 546-547) — same game-array-mutation class,
   pre-crash-window (logged region), only if bUseUObjectArrayCache armed.
4. **R3 marshal pump / LoadMap detour** (F's secondary) — NOT supported: no detour
   trampoline frames in the kernel backtrace, and crash is hook-settings-independent.
5. **bridge_io_controls Control D** (bridge:1478) — exported, caller-less, needs
   foreign NID resolution; zero evidence.
6. **Ack/diag/log writers** — own-buffer, transition-only; not the crash shape.

## 8. Bottom line

Q's two structural assertions hold under adversarial review (bank retired
pre-init-end; zero post-admission callers of the kernel-write copyin path in the 374
composed units), and the kernel backtraces independently exonerate the probe. But the
"save-load CE-108255-1" framing that the attribution change rode on is refuted by the
klog: the crash is a deterministic init-completion abort on modbridge's own init
thread inside UnrealInitializer.cpp:528-541, byte-identical across six launches and
all hook A/B settings, ~1.65 s after the last diag event. l25's runtime-phase
instrumentation should target the listener-registration/GMalloc interaction
(guard or log around r4:528 and UObjectArray.cpp:379-386), not just the copyin probe.
