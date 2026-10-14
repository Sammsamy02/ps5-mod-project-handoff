/* insession_cmd2_service.h — the refusal-surviving in-session command service.
   worker-insession-refusal, 2026-10-05.

   WHY A DEDICATED THREAD (the audit directive): the existing in-session
   diagnostic poll (worker-insession-diag v1) runs inside UE4SSProgram::update.
   That event loop jthread is spawned at UE4SSProgram.cpp:709 — AFTER
   setup_unreal() (:655, which contains the ScanGame call at :1319 that runs
   the seed/pool validation chain). A seeds refusal therefore always happens
   BEFORE the event loop exists, and the loop NEVER STARTS. The threads that
   DO exist around the refusal do not survive it as hosts:
     - the loader control thread that runs module_start -> constructors ->
       runtime wiring MUST RETURN from module_start ("wiring failures return
       0 — the game runs UNMODDED", runtime_module_entry.c:38-40);
     - the RuntimeOwner init thread's proc (runtime_owner.cpp:84-128) RETURNS
       right after the failure catch — the thread terminates (it is the thread
       that ran the uninstall path shutdown_native_program, wpd1:520-577).
   So this unit owns ONE lightweight dedicated thread: spawned by
   insession_cmd2_service_start() from module_start BEFORE any seeds work
   (before start_composed_runtime), joined by insession_cmd2_service_stop()
   from module_stop. It is independent of Lua, the game-thread pump,
   controller input and the 120-second seed budget by construction: it sleeps
   on its own clock and touches NO engine state.

   CALL-ORDER LAW (single-threaded startup discipline, entry-guard
   serialized): register_* calls happen from module_start context (before or
   after service_start — registration is atomic and legal anytime; before
   start is recommended so the first settled poll sees them);
   service_start() once; service_stop() from module_stop ONLY (never from the
   poll thread itself); settle_startup() once, after the wiring outcome is
   known (right before module_start returns). Before settle the poll thread
   only sleeps — no opens, no consumes — so no command can dispatch
   while the wiring thread is still emitting diagnostics.

   SAFETY CASE (per command, the complete effect list):
     - fixed 512 B bounded intake read (O_RDONLY|O_NOFOLLOW, 8-attempt bound)
     - 100 ms poll cadence (10 x 10 ms slices so stop() joins in <= ~10 ms)
     - all logging is a DIRECT bounded append to the UE4SS.log path through
       the proven kernel trio (sceKernelOpen O_APPEND / sceKernelWrite /
       sceKernelClose) with the runtime_module_entry freshness prefix — NO
       Output::send dependency (the UE4SS output devices may be torn down
       exactly when this service must work: after a refusal)
     - diag -> the same-PRX diagnostic_event (sink already open, append-only)
     - pool_probe -> the same-PRX bridge_kernel_write_copyin_probe (fresh
       attempt every time; the once-per-boot cache lives in the INITIALIZER's
       gap_probed_this_boot static, UnrealInitializer.cpp, never in the
       bridge); never touched unless a pool target was registered
     - io_probe -> ONLY the function pointers registered through
       insession_cmd2_register_io_control (worker-io-controls batched
       controls A-D); no slots wired means a loud "not wired" reply
     - never throws (every API is noexcept or swallows), never allocates,
       never reads or writes any engine/UE4SS object
   IMPORT LAW: every kernel touch below (sceKernelOpen/sceKernelClose/
   sceKernelWrite/read/rename) plus pthread_create/pthread_join (via
   std::thread, the same primitive RuntimeOwner and the UE4SS event loop
   already use) and nanosleep (via std::this_thread::sleep_for) is a strong
   member of the 196-import contract — verified present with provider
   identity in l10-1/l10-2/l11-2 supplier-contract.json. strcmp/strlen
   (scanner core) likewise. Zero new imports. */

#ifndef INSESSION_CMD2_SERVICE_H
#define INSESSION_CMD2_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* noexcept on the C++ declarations and definitions alike; C consumers (host
   fixtures include this header as C) get the plain declaration. */
#ifdef __cplusplus
#define INSESSION2_NOEXCEPT noexcept
#else
#define INSESSION2_NOEXCEPT
#endif

/* Intake contract (the proven e2e pattern: STOR-once -> consumed-rename ->
   evidence). One command slot; the host stages the next only after the
   .consumed file appears. */
#ifndef INSESSION2_INTAKE_PATH
#define INSESSION2_INTAKE_PATH "/data/modbridge/intake/in-session-cmd.json"
#endif
#ifndef INSESSION2_UE4SS_LOG_PATH
#define INSESSION2_UE4SS_LOG_PATH "/data/plugins/UE4SS.log"
#endif

/* Signature of a batched bridge IO control (worker-io-controls A/B/C/D).
   Implementation writes a bounded printable-ASCII result-tag string (<= cap
   bytes, NUL-terminated) and returns its own integer result code (reported
   verbatim in the reply line; semantics owned by the io-controls unit). */
typedef int (*insession2_io_control_fn)(char *tags_out, unsigned cap);

/* Spawn the poll thread. Idempotent; tolerates spawn failure silently (the
   service is then simply absent this boot — never fatal). Call from
   module_start AFTER the constructors succeeded and BEFORE
   start_composed_runtime(), so the thread exists before any seeds work. */
void insession_cmd2_service_start(void) INSESSION2_NOEXCEPT;

/* Kill + join the poll thread. Max join latency is one 10 ms slice plus the
   bounded effect of the command in flight. Call from module_stop ONLY. */
void insession_cmd2_service_stop(void) INSESSION2_NOEXCEPT;

/* Mark the startup wiring finished (success OR any recoverable refusal):
   dispatch is held until this fires. Call once, right before module_start
   returns 0. */
void insession_cmd2_settle_startup(void) INSESSION2_NOEXCEPT;

/* Record that the initializer refused (recoverable path). Sets the mode
   flag and writes ONE UE4SS.log triage line the first time. Call from the
   acknowledged failure branches (see staged diffs). */
void insession_cmd2_note_initializer_refused(void) INSESSION2_NOEXCEPT;

/* Register the fresh pool-probe target: the ABSOLUTE runtime pool address
   the composed boot derives from SeedTable.h (kBaseObserved + kFNamePoolLabel)
   and the probe length (rc_seed::kFNameBlockCapacityBytes = 0x20000).
   Unregistered: pool_probe replies "target not registered" and never calls
   the bridge. */
void insession_cmd2_register_pool_probe_target(uint64_t pool_addr,
                                               uint32_t probe_len) INSESSION2_NOEXCEPT;

/* Register a batched IO control slot: 0..3 for ops "a".."d". Passing NULL
   unwires the slot. Registered any time (atomically); the recommended point
   is the composition wiring, before service_start(). */
void insession_cmd2_register_io_control(unsigned slot,
                                        insession2_io_control_fn fn) INSESSION2_NOEXCEPT;

/* ONE bounded poll: read the intake (512 B cap), parse grammar v2, dispatch,
   consume (execute THEN rename — at-least-once, loudly). Exposed so host
   fixtures (and only they) can drive the dispatch deterministically without
   the thread. Never throws. */
void insession_cmd2_poll_once(void) INSESSION2_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* INSESSION_CMD2_SERVICE_H */
