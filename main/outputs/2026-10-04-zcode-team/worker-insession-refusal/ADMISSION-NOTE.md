# ADMISSION NOTE — worker-insession-refusal (2026-10-05/06)

For the integrating worker. This unit is ON THE CRITICAL PATH for the next
launch: it makes the bounded diagnostic service callable after a RECOVERABLE
initialization failure (pool/seeds refusal), independently of Lua, the
game-thread pump, controller input and the 120-second seed budget — so
prepared IO probes can be re-run in-session while Townfall stays open, with
no relaunch.

Everything here is STAGED, PROPOSAL/COMPOSITION-LATER. This worker never
dispatched, built for console, or ran anything on console. Host logic proof
only (fixtures/receipts/host-test-pass.txt, PASS 2026-10-06T04:27:30Z,
re-run 3/3 stable).

## 1. What composition gets

Four NEW units (this worker's staging/, no shared-lineage edits):
```
insession_cmd2_core.h/.c      grammar v2 strict scanner (pure C, no I/O)
insession_cmd2_service.{h,.cpp} the refusal-surviving service (thread, poll,
                              dispatch, direct UE4SS.log writer)
```
Two STAGED DIFFS, NOT APPLIED (staging/diffs/):
```
runtime_module_entry-insession2-hook.diff   service_start / settle /
                                            note_refused x3 / service_stop
runtime_owner-insession2-refused.diff       note_refused on the async
                                            init-thread failure catch
```
Plus ONE composition registration hook (documented inside the first diff;
its placement TU is the composer's reconciliation point):
```c
insession_cmd2_register_pool_probe_target(
    SeedTable::kBaseObserved + SeedTable::kFNamePoolLabel,   // 0x400000+0x9365ec0
    (uint32_t)rc_seed::kFNameBlockCapacityBytes);            // 0x20000
insession_cmd2_register_io_control(0..3, bridge_io_control_a..d);
```

## 2. Grammar v2 (frozen)

Intake `/data/modbridge/intake/in-session-cmd.json` -> rename to `.consumed`
(the proven pattern, unchanged from v1). STRICT superset of grammar v1 —
every v1 input keeps its v1 verdict:
```
{"cmd":"noop"}
{"cmd":"diag","tag":"<1..48 [A-Za-z0-9_.-]>","repeat":<1..1024>}
{"cmd":"io_probe","op":"a"|"b"|"c"|"d"}
{"cmd":"pool_probe"}
{"cmd":"log","text":"<1..96 printable ASCII>"}
```
Unknown cmd/key/duplicate/escape/trailing/oversize(>=512 B) -> loud REJECT,
consumed, never re-read, nothing executed. Key order flexible; per-command
key exclusivity strict.

## 3. Thread choice — evidence chain (the decision the task asked for)

Option 1 in the task ("the module_start worker thread continues") is REFUTED
by the sources; option 3 (dedicated thread) is implemented:

- The wiring chain runs SYNCHRONOUSLY on the LOADER CONTROL THREAD inside
  module_start: `start_composed_runtime()` (runtime_module_entry.c:260-330)
  -> `modbridge_runtime_start_with_activation` (runtime_start_bridge.cpp:547)
  -> `StartActivatedRuntimeWithAck` -> `PrepareActivatedUE4SSRuntime` ->
  `UE4SSBackend` ctor (ue4ss_backend.cpp:20-58). That thread MUST return
  ("wiring failures return 0 — the game runs UNMODDED",
  runtime_module_entry.c:38-40); looping on it would wedge the game's load
  path.
- The seeds refusal throws from `consume_required_slot_seeds` — "Runs in
  ScanGame()" (UnrealInitializer.cpp:854-856; `seed_refused` :756-762).
  ScanGame is called from `setup_unreal()` (wpd1 UE4SSProgram.cpp:1319;
  fn starts :1002); the early-scan site swallows runtime_error (:1330-1332)
  and `Initialize` surfaces it (:1814/:1826). `setup_unreal` is called by
  `init()` at :655 — BEFORE the event-loop jthread is created at :709. So
  the refusal ALWAYS happens before the loop exists: v1's poll site (inside
  `update`) NEVER RUNS on a refused boot.
- The RuntimeOwner init thread does NOT survive either: the refusal is
  caught by `UE4SSProgram::init` (:727-743) which runs the uninstall path
  `shutdown_native_program()` (:520-577; `uninstall_mods()` :546), init
  returns, `UE4SSBackend::initialize` throws (ue4ss_backend.cpp:67-71), and
  `RuntimeOwner::initialize` — THE THREAD PROC — returns from its catch
  (runtime_owner.cpp:111-127). The thread terminates (joinable, joined at
  module_stop). And when the refusal happens on the ctor path, the init
  thread never even starts.
- THEREFORE: the service owns ONE dedicated lightweight thread
  (std::thread — the same primitive RuntimeOwner and the UE4SS event loop
  already use in this PRX), spawned by `insession_cmd2_service_start()`
  from module_start AFTER constructor success and BEFORE
  `start_composed_runtime()` (i.e. before ANY seeds work), joined by
  `insession_cmd2_service_stop()` at the top of module_stop (before the
  runtime stop and long before the diagnostic channel closes).

Exact staging copies the line numbers come from (sha256):
- wpd1 UE4SSProgram.cpp: cb6cb14aa044124f17cf8d2115b4962a7d91daa686f6e80a0631061b51b8c8af (identical to v1's pin)
- runtime_module_entry.c (d1-c4 overlay): c97055f40f5a6dd94dd6360c4459538b8130e8b63a806a84d975d7c7604db470
- runtime_owner.cpp (vendored): 54c56c49d0ad89fd6456f58d99f765027f6e16e0bfdbaf8b8e1e044f268aa629
- runtime_start_bridge.cpp (comp6 aside-1791257478, contains the copyin probe): cadeb4ea675a7d5000efdb7bb27324e47c4531f94de827112e3fb61bb7bf0c95
- UnrealInitializer.cpp (copyin-unit staging): 62422c21881b379e3ccfb731e73e4b9e8aaeafcf41ff9a31b5a0ea07f72473f6

## 4. Safety case (as staged)

- Lightweight: 100 ms cadence (10 x 10 ms slices — stop joins in <= ~10 ms
  + one bounded effect); idle poll = one failed open, nothing else; intake
  read fixed 512 B with an 8-attempt bound; no allocation anywhere.
- Dispatch HELD until `insession_cmd2_settle_startup()` (fires after the
  wiring outcome is known, before module_start returns 0): a command can
  never race the wiring thread's startup diagnostic_event burst — the
  lock-free sink's single-writer startup discipline (diagnostic.c:269-271)
  is preserved by construction. After settle on a refused boot, this thread
  is the ONLY in-session diagnostic_event caller.
- Effects use ONLY in-contract symbols. Verified present with provider
  identity in ALL of l10-1/l10-2/l11-2 supplier-contract.json
  (required_strong=196, missing_strong=0):
  sceKernelOpen/sceKernelClose/sceKernelWrite (libkernel/libkernel);
  read/rename/pthread_create/pthread_join/nanosleep/getpid/clock_gettime
  (libkernel/libScePosix); strcmp/strlen (libSceLibcInternal).
  diagnostic_event is the in-link C symbol (diagnostic.c, same PRX);
  bridge_kernel_write_copyin_probe is an in-link bridge definition (same
  PRX, present from composition6-4 onward) — a missing definition is a
  LOUD LINK STOP, never a fallback. Per-build NID-aware import audit stays
  mandatory: the derived contract must come out at exactly 196 strong.
- Never touches engine state: no UE4SS includes at all (no Output.hpp, no
  UE4SSProgram.hpp, no StringType), no queue_*, no hooks, no engine objects.
  All logging is a DIRECT bounded append to /data/plugins/UE4SS.log via the
  proven kernel trio with the runtime_module_entry freshness prefix — the
  UE4SS output devices may be torn down exactly when this service must
  work (shutdown_native_program closes them); multiple appenders to that
  file are the established pattern (entry failure lines + runtime lines).
- pool_probe is a FRESH attempt on every command: the once-per-boot
  gap_probed_this_boot gate lives in the INITIALIZER TU only
  (UnrealInitializer.cpp); the bridge probe is stateless per call (its own
  O_TRUNC sink, /data/plugins/modbridge-copyin-sink.bin) and emits its own
  pool_copyin_probe_fresh/rc/errno/sink diagnostics. Class codes:
  1 readable / 0 refused (EFAULT class, never "unmapped") / -1
  unavailable-or-invalid. Requires a registered target; never probes a
  guessed address.
- io_probe calls ONLY the registered function pointers (worker-io-controls
  batched controls); unwired slots reply "controls not wired" — the unit is
  complete and linkable BEFORE that worker lands. Required ABI:
  `int fn(char *tags_out, unsigned cap)` (bounded printable-ASCII result
  tags, integer rc reported verbatim).

## 5. Integration order

1. Add the four staging units.
2. Apply runtime_module_entry-insession2-hook.diff (5 one-line calls).
3. Apply runtime_owner-insession2-refused.diff (2 one-line calls).
4. Place the registration hook where SeedTable.h + the io-controls symbols
   are visible (reconcile with worker-io-controls' final names).
5. Do NOT apply worker-insession-diag's event-loop hook: v2 supersedes it
   (at most ONE in-session diagnostic_event consumer; the intake rename is
   atomic so two pollers could never double-execute, but the sink
   single-writer law is kept by construction this way). Grammar v2 accepts
   every grammar-v1 input, so no host-visible command changes.
6. Per-build import audit (196 strong) — drift is a stop.
7. First composition compile is the console-branch proof (the host build
   exercises the same code paths through POSIX shims; there is no separate
   console-only branch in this TU — deliberately, unlike v1).

## 6. Host staging discipline (when this ships)

Unchanged from v1: never overwrite a stale intake; STOR
`in-session-cmd.json.next` + FTP RNTO onto the intake name (atomic), or
stage pre-launch (the service tolerates a pre-existing file once settled);
per-command evidence triad = intake GONE + `.consumed` readback + the
UE4SS.log / sdiag lines.

## 7. Honest bounds

- Host logic proof only: parser, dispatch decisions, thread lifecycle,
  settle/refused gating, fresh-probe behavior, file-op sequences, stop
  bound are proven (94 checks + strict python validation, 3/3 stable).
  CONSOLE behavior (FTTP STOR reachability mid-session, libkernel
  read/rename/pthread binding under the game sandbox, real copyin verdicts
  in-game, UE4SS.log interleaving under real frame load, behavior when the
  game quits) is NOT measured and NOT claimed anywhere.
- The async-refusal flag (RuntimeOwner catch) has a documented cosmetic
  edge: if a composition ever routes a cooperative cancellation through
  that catch, the one-line refused triage label would be a mislabel during
  teardown (bounded, harmless, no effect) — reconcile at composition review.
- Line numbers drift if the cited staging copies change (shas pinned in §3).
