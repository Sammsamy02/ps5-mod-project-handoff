# ERROR-PATH-ANALYSIS — composition-4 composed runtime (run-3)

Subject: `…ue4ss-native-supplier-composition-4/run-3/coherent-tick-file-witness-runtime.o`
sha256 `45dd9901…75af5`, 368 units, composed by worker-composition-4-fix. Analysis is
source-level; nothing was executed on a console.

Sources read (exact files):
- `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c`
- `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/lifecycle.c`
- `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/constructor_boundary.cpp`
- `outputs/ps5-mod-bridge/native-runtime-startup/diagnostic.h` (decl)
- `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/diagnostic.c`
  (byte-identical in the four newest trace generations: aligned-entry-1,
  registration-trace-1, worker-channel-1, import-diagnostics-1, startup-owned-input-1 —
  the diagnostic implementation is NOT under outputs/ps5-mod-bridge; it ships prebuilt
  in the composition baseline objects)
- composition-4's ue4ss_backend.cpp replacement =
  `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp`
  (composition-3 replace-receipt pins this byte-copy; composition-4 re-included it
  "byte-identical", replaced again as `bootstrap-ue4ss_backend.o` in run-3)
- `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp`
  (RuntimeAckSink implementation; replaced as `bootstrap-runtime_owner.o` in run-3)
- `outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp`
  (composition-4 extra unit `module-runtime-ack-supplier.o`)
- `…worker-ack-integration-deep/controller-patch/02-composition-wiring/WIRING-SPEC.md`

Evidence boundary: `probe_runtime_validate_and_preload` / `probe_runtime_log_status` /
`probe_worker_start` (probe_api.h) are declared but their implementation objects are
prebuilt baseline artifacts; their sources are not in this workspace. Everything below
about them is inferred from the ops contract in lifecycle.c, not from their bodies.

---

## 1. module_start: order of operations and the FIRST calls

`module_start` (`module_entry.c:48`) does, in order:

1. **FIRST statement of all**: `__atomic_test_and_set(&entry_guard, ACQUIRE)` (line 51).
   The very first *function call of substance* is therefore this atomic, and the first
   *external* call is `diagnostic_ready()` at line 57 — but only after three state gates.
2. `lifecycle.state == PROBE_ACTIVE` → `return 0` (idempotent re-entry, no output).
3. `lifecycle.state != PROBE_NEW` → `return -21`.
4. `!diagnostic_ready() || diagnostic_open_channel() < 0` → `return -22`.
5. **First observable output**: `diagnostic_event("module_start_entered", 0)` (line 59) —
   a `sceKernelWrite` of `"[PPSA33286] <marker> module_start_entered value=0x0\n"` to the
   build-pinned `PROBE_LOG_PATH` (generated probe_config.h, e.g.
   `/data/townfall_runtime_registration_trace.log` in the registration-trace generation),
   opened `O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600` via `sceKernelOpen`.
6. SP-alignment + constructor_count events, then `probe_lifecycle_start(...)` (lifecycle.c):
   ops validation → span/frame-bounds checks → `o->validate()`
   (`probe_runtime_validate_and_preload`) → per-init-array `o->invoke_constructor(cb)`
   → `o->log_status()` → `o->worker_start()` → `PROBE_ACTIVE`.

Constructor exceptions are converted by `probe_call_constructor_catching`
(`constructor_boundary.cpp`): `try { callback(); return 0; } catch (...) { return
PROBE_CTOR_EXCEPTION; }`. **No exception can escape module_start**; the kernel boundary
only ever sees an integer return.

## 2. Early-return paths that skip ALL logging

Yes — four, and they are *structural*: the diagnostic channel is the only logging
mechanism, so every path at or before `diagnostic_open_channel()` succeeding cannot
leave a trace anywhere by construction.

| line | condition | return | output | module stays loaded |
|---|---|---|---|---|
| 51 | `entry_guard` already set (re-entry / concurrent start+stop) | `-20` | ZERO | yes |
| 53 | state `PROBE_ACTIVE` | `0` | ZERO (by design, idempotent) | yes |
| 55 | state neither NEW nor ACTIVE (e.g. after FINISHED) | `-21` | ZERO | yes |
| 57 | `diagnostic_ready()` false — sceKernelOpen/Write/Close bindings NULL (import table unresolved) | `-22` | ZERO | yes |
| 57 | `diagnostic_open_channel() < 0` — `sceKernelOpen(PROBE_LOG_PATH,…)` failed (missing /data mount, EACCES, O_NOFOLLOW trip, fd exhaustion) | `-22` | ZERO | yes |

Condition 57b is the purest zero-output failure: the caller (game kernel loader) sees
only a negative module_start, the log file is never created (or not appended), and no
other subsystem has run. Even the reason (errno) is discarded — `sceKernelOpen`'s
return is compared `< 0` and the value dropped.

Additionally, even after the channel opens, `diagnostic_event` → `write_line`
(diagnostic.c:39) **silently drops** the line if the fd went bad and stops mid-line on
partial `sceKernelWrite` (`break` on `written <= 0`); truncation at 512 bytes is silent.
Evidence loss under disk pressure is unobservable.

## 3. ue4ss_backend.cpp — PrepareActivatedUE4SSRuntime failure behavior

Path map (`bootstrap-stage/…/ue4ss_backend.cpp:84-119`):

1. `context.mod_source` already set → `throw std::invalid_argument` (line 89) — **before
   the ack sink exists**.
2. `MakeActivatedUE4SSModSource(activation, title)` (line 90) validates the activation
   (title match, declared unreal/ue4ss-ps5 files, preload). On failure it throws —
   **also before the sink is opened**.
3. `RuntimeAckSink::Open(runtime_ack)` (lines 96-98) — only when `status_root` non-empty.
4. `try { UE4SSBackend ctor … }` — the **only catch blocks that write anything**:
   `catch (const std::exception&) { if (ack) ack->failed(1, error.what()); throw; }` and
   `catch (...) { if (ack) ack->failed(1, "…unknown exception"); throw; }`
   (lines 106-117). Both rethrow the original error; the ack never masks it.
5. Success: `ack->activated()`, then `RuntimeOwner` wrapping.

Consequences:
- Failures at steps 1-2 hit `if (ack)` with `ack == nullptr` → **no `failed` line, no
  runtime-status.txt, ever**. The exception propagates into the constructor caller;
  `probe_call_constructor_catching` reduces it to the code `PROBE_CTOR_EXCEPTION` —
  **the `e.what()` text is discarded** (the catch boundary returns only a code, and
  `invoke_constructor` logs `constructor_begin`/`constructor_return` with a hex value,
  no text). So an activation-validation failure leaves exactly one numeric probe-log
  event and zero status-file output.
- Failures inside the UE4SSBackend ctor (step 4: `UE4SSProgram` construction rejected,
  native inventory rejected, bad_alloc) DO write a `failed` line — observable, as
  designed (RUNTIME-ACK point 4, preparation-time).
- `UE4SSBackend::initialize` (line 59-71) also throws on
  `native_startup_error_message()` after `program_->init()`; this runs on
  RuntimeOwner's init thread, caught by `RuntimeOwner::initialize`'s catch-all →
  `ack->failed(1, …)` → observable.

## 4. runtime_ack_supplier / RuntimeAckSink — silent-failure surface

`RuntimeAckSink::Open` (`runtime_owner.cpp:130-153`) never fails startup:
- empty `status_root` → returns an **inert sink** (`fd = -1`,
  `last_error_ = "runtime ack root is not configured"`).
- `mfa_status_open` negative → inert sink, `last_error_ = "mfa_status_open failed: N"`.
- any exception (allocation) → `nullptr` → session unacknowledged.
- The inert sink's `last_error_` lives **only in memory**; nothing ever persists it.
- `emit()` (`:154-170`): fd<0 → recorded no-op; `mfa_status_write` failure →
  `last_error_` updated in memory; all exceptions swallowed. A mid-session write
  failure (ENOSPC) leaves the file without a complete final line — the e2e reader
  defines "running" as last complete line == `mods-started`, so the session reads as
  **no output at all** to the installer records.

`runtime_ack_supplier.cpp` adds two degenerate inputs:
- `MakeRuntimeAckConfig(nullptr|"")` → empty `status_root` → inert sink (silent).
- `clock_gettime` failure → `now_ms = 0` (degraded but recorded — only if open succeeded).
- `StopRuntimeWithAck(nullptr)` → returns false silently.

## 5. Caller wiring of the ack path (composition-4 as shipped)

`StartActivatedRuntimeWithAck` is defined in `module-runtime-ack-supplier.o`, exported
in the PRX (run-3 `runtime.map`, `exports.stdout`), and per WIRING-SPEC §S5.5 "is
authored with the next game-bound module's caller unit". No source file in the composed
closure calls it (workspace-wide grep: only definitions/headers/tests); the run-3
undefined-import delta added only `sceKernelGetModuleInfo`. Until that caller unit
lands, **the composed PRX as loaded will never open runtime-status.txt regardless of
engine behavior** — a systemic zero-output condition for the status channel (the probe
log channel still emits normally).

## 6. Paths where the module does NOT stay loaded (crash)

Exactly one found: `RuntimeOwner::~RuntimeOwner` (`runtime_owner.cpp:17-22`)
`if (!stop()) std::terminate();`. `stop()` returns false when called from the
initializer thread itself (`worker_is_current` guard, `:49`). Destroying the owner from
its own init thread therefore **terminates the game process** — deliberate
("deliberately no detached-thread fallback"). Everything else degrades to integer
returns from module_start/module_stop; the game stays up.

## 7. Silent-failure paths ranked by likelihood

1. **Inert/unconfigured ack sink** (`RuntimeAckSink::Open` → fd<0, or empty
   `status_root` from `MakeRuntimeAckConfig`). Condition: `mfa_config::root` empty/wrong,
   or `<mod root>/runtime-status.txt` not openable (permissions, /data timing).
   Mechanism: return code inside `Open`, no exception, no crash. Module loaded, engine
   may run fully unacknowledged; `last_error_` never persisted. **By design silent.**
2. **Status file never created because the ack caller is not wired yet** (section 5).
   Condition: running the composition-4 PRX as-is. Mechanism: absent call site (not a
   failure at runtime). Module loaded; zero status output guaranteed.
3. **Activation rejection before sink open** (`MakeActivatedUE4SSModSource` throws, or
   mod_source pre-set). Condition: title mismatch / missing activation files.
   Mechanism: C++ exception → caught by constructor boundary → numeric
   `constructor_return` probe event only; message lost; **zero status-file output**;
   module loaded, game continues.
4. **Diagnostic channel open failure** (`diagnostic_open_channel() < 0` → module_start
   `-22`). Condition: `PROBE_LOG_PATH` unopenable (missing /data, EACCES, O_NOFOLLOW,
   fd exhaustion) or NULL sceKernel bindings (`diagnostic_ready()` false).
   Mechanism: plain early return; **ZERO output of any kind, structurally**; module
   loaded. Likelihood lower than 1-3 (path is fixed at build time and precedents
   succeeded) but total when it hits.
5. **Mid-session ack write failure** (`mfa_status_write` ENOSPC etc.) — status file
   ends without a complete final line; reader sees "not running". Mechanism: recorded
   no-op. Module loaded.
6. **Silent diagnostic evidence loss** — `write_line` drop on bad fd / partial write;
   512-byte line truncation. Mechanism: silent drop inside the logger itself.
7. **Re-entry gates `-20` / `-21` / idempotent `0`** — zero output by design; benign
   concurrency/state conditions, not faults.
8. **Ack sink allocation failure** → `nullptr` → unacknowledged session (silent,
   startup continues). Rare (single small allocation).
9. **Crash (module/game does NOT stay loaded)** — `RuntimeOwner` destroyed from its own
   init thread → `std::terminate()`. Not silent, but the only non-survivable path found.

## 8. Answers to the task's specific questions (condensed)

- module_start FIRST action: the `entry_guard` atomic test-and-set; first external
  call: `diagnostic_ready()`; first real work: `probe_lifecycle_start`.
- First error path (guard/state/diagnostic gates): **returns silently** — nothing is
  written; the `-22` path is unwritable by construction.
- Early returns skipping all logging: `-20`, `0` (ACTIVE), `-21`, `-22` (two distinct
  conditions) — module_entry.c lines 51-58.
- ue4ss_backend ctor failure: catch blocks write `ack->failed(...)` **only if the sink
  had already been opened**; activation-stage failures produce no status output at all
  and their exception text is discarded at the constructor boundary.
- Ack sink open failure: **yes, effectively silent** — inert sink, in-memory error
  string only, startup proceeds unacknowledged.
- Zero-output paths ranked: see section 7; the top three all concern
  runtime-status.txt, the top structural one is the `-22` diagnostic gate.
