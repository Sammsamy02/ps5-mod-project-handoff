# STARTUP-TRACE — composition-4 PRX: module_start to first logging

Worker: worker-bootstrap-trace, 2026-10-04. Every claim below is sourced from the
actual composed sources and receipts on disk (paths cited inline). No console
contact, no execution.

## 0. What actually runs (architecture facts, receipt-proven)

- The composition-4 PRX's `module_start` is the **frozen probe bootstrap**
  `lifecycle/module_entry.c`, NOT the UE4SS bootstrap. Proof:
  `prx-candidate-composition4-1/verification-evidence/static-checks.json`
  `entry_alignment_evidence.module_start` — first bytes
  `554889e5...b101 860d0586cd01 b8ecffffff` = `force_align_arg_pointer` prologue +
  entry_guard `xchg` + `mov eax,-20`; codegen of the frozen file, size 351.
- The probe bootstrap sources were compiled fresh into the candidate from
  `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/`
  (build-receipt.json commands): `module_entry.c`, `lifecycle.c`,
  `constructor_boundary.cpp`, `diagnostic.c`, `import_check.c`, `probe.c`,
  `startup_input.c`, `memory_lua.c`, `sentinels.c`, `import_references.S`,
  plus `supplier_import_extension.S` (compiled in the candidate's own build dir).
- The composed UE4SS supplier (368 units, `combined-supplier.o`, 117,779,552
  bytes relocatable, run-3 receipt) is linked behind the same bootstrap. The
  coherent-runtime lineage README states its composed object "has no
  module_start entry" — the probe bootstrap is the only entry.
- `generated/probe_config.h` (shared, same `-I` in every bootstrap compile):
  `PROBE_LOG_PATH "/data/townfall_runtime_aligned_entry.log"`,
  `PROBE_TITLE "PPSA33286"`, `PROBE_INPUT_PATH "/data/townfall_worker_input.lua"`
  (88 bytes), `PROBE_DELAY_SECONDS 5`.

## 1. Exact numbered sequence of module_start

Source: `native-runtime-startup/lifecycle/module_entry.c` (identical file
compiled into the candidate), `lifecycle/lifecycle.c`, `probe/probe.c`,
`diagnostic.c`.

| # | Step | What it does | Observable? |
|---|------|--------------|-------------|
| 1 | entry_guard test-and-set (`__atomic_test_and_set`, ACQUIRE) | re-entry lock | no — `-20`, nothing written |
| 2 | `lifecycle.state` check | `PROBE_ACTIVE`→0; `!= PROBE_NEW`→`-21` | no |
| 3 | `diagnostic_ready()` | volatile-read the three import bindings `sceKernelOpen/Write/Close`; any NULL → `-22` | no |
| 4 | `diagnostic_open_channel()` | `sceKernelOpen("/data/townfall_runtime_aligned_entry.log", O_WRONLY\|O_CREAT\|O_APPEND\|O_NOFOLLOW, 0600)`; fd<0 → `-22` | no |
| 5 | `diagnostic_event("module_start_entered",0)` | **FIRST OBSERVABLE WRITE** — first line of the probe log | yes (probe log) |
| 6 | SP alignment events | `entry_incoming_sp_mod16`, `entry_call_sp_mod16` | probe log |
| 7 | `constructor_count` event | `( __init_array_end - __init_array_start ) / 8` — 235 slots in composition-4 (static-checks `init_slots: 235`) | probe log |
| 8 | `probe_lifecycle_start(...)` prologue | ops/`span()`/eh-frame-bounds validation; any failure → state=FINISHED, `-1` | only the trailing `module_start_return` event |
| 9 | `o->validate()` = `probe_runtime_validate_and_preload` | (a) `probe_imports_validate` walks `modbridge_import_table[0..191)` (the frozen aligned-entry contract rows): `import_validation_begin`, `missing_import slot=…` (first only), `import_validation_missing_count`; missing→`-1`. (b) `__cxa_finalize` binding non-null else `dso_finalizer_binding_missing`,`-10`. (c) `startup_input_load("/data/townfall_worker_input.lua", 88)`: `startup_input_open_result/read_error/truncated/changed/loaded` events via the channel fd; failure → rc | probe log only |
| 10 | constructor loop ×235 | per slot: `constructor_begin(i)` → `probe_call_constructor_catching` (C++ try/catch, `constructor_boundary.cpp`) → `constructor_return(rc)`; first throw → `finish_partial_constructor` (`__cxa_finalize` only) → `PROBE_CTOR_EXCEPTION` | probe log only — **this is where all 368 composed units' C++ static initializers run** (incl. `NativeQueryHarness::Global()` init, UnrealInitializer statics, UE4SS/port singletons) |
| 11 | `o->log_status()` | `diagnostic_event("full_runtime_constructors_complete",0)` | probe log |
| 12 | `o->worker_start()` | `pthread_create(worker_main)`; `worker_create_result` event; nonzero → full `finish()` (fini-array reverse walk + `__cxa_finalize`) → rc | probe log |
| 13 | state=`PROBE_ACTIVE`, return 0 | worker_main: 5 s sleep (`worker_sleep_result/complete`), then buffered Lua check of the 88-byte input (`buffered_lua_begin/result`) — **the probe worker never starts UE4SS** | probe log |
| 14 | `diagnostic_event("module_start_return", result)`; on failure+FINISHED: release input, close channel | last probe-log line of the session | probe log |

## 2. What "first logging" means here — and what was actually checked

- The FIRST observable write of the whole runtime is step 5, to
  **`/data/townfall_runtime_aligned_entry.log`** — the probe diagnostic log —
  written with the three frozen bootstrap imports (`sceKernelOpen/Write/Close`)
  and nothing else. It happens **before** any composed-runtime code, before any
  constructor, and before any of the 7 additional imports could execute.
- `UE4SS.log` is written only by `RC::UE4SSProgram`, reachable only via
  `UE4SSBackend::initialize → program_->init()` (`bootstrap/ue4ss_backend.cpp`),
  reachable only via `PrepareActivatedUE4SSRuntime` / `PrepareDeferredUE4SSRuntime`.
- `runtime-status.txt` is opened only by `RuntimeAckSink::Open` inside
  `PrepareActivatedUE4SSRuntime` (`bootstrap/ue4ss_backend.cpp` lines 100-102;
  `bootstrap/runtime_owner.cpp` lines 130-153).
- **Structural fact (WIRING.md §4, quoted):** "The game-bound entry (future
  unit) keeps exactly two obligations: call `mfa_activate` … and hand this
  supplier the activation…". The only caller chain
  (`module-runtime-ack-supplier`'s `StartActivatedRuntimeWithAck` →
  `PrepareActivatedUE4SSRuntime` → `RuntimeOwner::start`) has **no call site
  anywhere in the composed image** (verified by tree-wide grep over
  `.superpowers/` and `outputs/` sources; only host-test copies define/call it).
  Therefore, by construction, composition-4 can load perfectly and still
  produce **neither `runtime-status.txt` nor an ack-driven `UE4SS.log`**. Their
  absence alone is not evidence of a startup failure. Any engine start in the
  working builds must originate in the deep pinned retained objects
  (`retained_fixed_dependencies`), whose sources are not all on disk; that
  call path could not be located in the available sources and is flagged as an
  open wiring question, not a finding.

## 3. Silent-failure points (load OK, zero output), ranked

- **S1 — step 4 (`-22`): diagnostic channel open fails.** `/data` not writable
  in the load context (mount/sandbox timing, title context). Returns `-22`
  before ANY write; steps 1-2 likewise silent (impossible on a first load).
  Every subsequent log is impossible. Independent of all imports. Mitigating:
  identical path/binding in the working aligned-entry, so it requires a
  run-context delta. module_start's nonzero return must simply be ignored by
  the loader for this to read as "loaded OK".
- **S2 — pre-module_start load-time work.** The PRX carries `DT_INIT` (0x15a0
  value 5536) run by the loader before `module_start`, plus load-time
  relocations over the enlarged 30.8 MB image (incl. the
  `modbridge_supplier_import_table` `.data.rel.ro` slots and 235 init slots —
  `loader_owns_no_arrays` proves the loader runs none of the array entries; the
  ctor walk is ours). A crash/hang in load-time code leaves the module mapped
  (loader "OK") with zero output. What `DT_INIT` resolves to could not be
  determined from on-disk artifacts — flagged honestly as unverified. The 0.6 MB
  size delta vs the working PRX lives exactly in this class.
- **S3 — step 10: a throwing/hanging static constructor.** Observable ONLY in
  the probe log (`constructor_begin` is the last line before a hang; a throw
  yields `constructor_return(≠0)` then `PROBE_CTOR_EXCEPTION`). This is the
  first execution of composition-4's real new code (query-harness singleton,
  replaced UnrealInitializer statics). If only `UE4SS.log`/`runtime-status.txt`
  were checked, this reads as "zero output".
- **S4 — steps 8-9: import-validation or input-preload failure.** Silent to
  `UE4SS.log`/`runtime-status.txt`, loud in the probe log
  (`import_validation_missing_count`, `startup_input_*`).
- **S5 — structural (§2 above): ack/engine start path never called.** Not a
  failure — an un-wired future unit.

Note on return codes: every pre-ACTIVE failure path returns nonzero
(`-20,-21,-22,-1,-10,PROBE_CTOR_EXCEPTION,worker rc`). Whether ploader/the game
surfaces module_start's return is unknown; all of S1/S3/S4 are consistent with
"ploader says OK, zero files".

## 4. The 7 additional imports (198 vs 191) — step-by-step culpability

Measured facts: aligned-entry contract = 191 required strong imports
(`native-full-runtime-aligned-entry-1/import-contract.json`,
`required_strong: 191`). Composition-4 candidate: composed-baseline undefined
= 198; PRX contract check 196 = 191 + 5 extension (`static-checks.json`);
composition-4 vs composition-3 composed-object import delta = **EMPTY**
(composition-4-fix REPORT §4, receipt `undefined-delta.json`).

The only 7 symbols that can account for 198−191:

| import | introduced | referenced from | first executable moment | can it fail startup silently? |
|---|---|---|---|---|
| `sceKernelDlsym` | supplier extension | `CppModModule::FindExport` (`native-coherent-cpp-loader-1/build-1/source/CppModModule.cpp`) + `modbridge_supplier_import_table` data slot | C++ mod loading, after engine init | no — not on any pre-log path; load-time slot relocation failure would fail the LOAD, contradicting "ploader OK" |
| `sceKernelGetModuleInfo` | composition-3 | module-manager glue + extension table | post-engine module queries | no — same class |
| `sceKernelLoadStartModule` | supplier extension | `CppModModule::Open` + table slot | C++ mod loading | no |
| `sceKernelStopUnloadModule` | supplier extension | `CppModModule::Close` + table slot | teardown only | no |
| `qsort` | supplier extension | composed C++ runtime (libSceLibcInternal provider) | anywhere, incl. step 10 | theoretical only: provider is the same SDK libc family the working build already binds (malloc identity donor); a qsort stub failure would equally be loud elsewhere |
| `clock_gettime` | ≤notify-cancel base (in 197-symbol baseline before composition-4) | `MakeRuntimeAckConfig` (`runtime_ack_supplier.cpp:35`) and libc internals | ack path only | no — the only first-party caller is `StartActivatedRuntimeWithAck`, which has no call site (§2) |
| `getpid` | ≤notify-cancel base | `MakeRuntimeAckConfig` (`runtime_ack_supplier.cpp:51`) | ack path only | no — same |

**Verdict: none of the 7 imports executes before the first log write (step 5);
five of them first execute only after the engine is up, and two only inside a
function that is never called.** The only load-time footprint they add is the
five `.data.rel.ro` address slots in `supplier_import_extension.S`; an
unresolvable slot is a load-time relocation failure — loud, not silent. The
"7 imports" are therefore a **weak** explanation for "loads OK, zero output".

## 5. Most likely silent-failure candidate

1. **Check `/data/townfall_runtime_aligned_entry.log` first.** It is the
   startup trace. Its content decides everything:
   - absent → S1 (step 4, `-22`) or S2 (never reached `module_start`): the
     failure is environment/load-time, not code logic, and not the imports;
   - ends at `module_start_entered`/`constructor_count`/`constructor_begin(i)`
     → S3 (constructor hang/crash in the composed runtime — composition-4's
     actual new code);
   - shows `import_validation_missing_count >0` or `startup_input_*` error →
     step 9; shows `constructor_return(≠0)`/`PROBE_CTOR_EXCEPTION` → which
     constructor threw, by index;
   - shows `module_start_return 0x0` → module_start SUCCEEDED; "zero output"
     then means only that the un-wired ack/engine start path (S5) never ran —
     the expected composition-4 state per WIRING.md §4.
2. **Primary code-level suspect if the probe log is absent:** S2 (load-time),
   because the symptom's precondition (ploader OK + zero execution evidence)
   fits "mapped but init never completed", and the delta vs the working build
   is in image content, not in any pre-log import call.
3. **The 7 imports are exonerated for the pre-logging window** (§4); if a
   constructor-phase crash (S3) is confirmed, the first-place suspects become
   the composition-4 replacements/extras' static init (query harness singleton,
   replaced UnrealInitializer), not the imports.

## 6. Evidence index

- Entry/lifecycle/probe/diagnostic sources:
  `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c`,
  `.../lifecycle/lifecycle.c`, `.../lifecycle/constructor_boundary.cpp`,
  `.../diagnostic.h`; `.superpowers/.../native-full-runtime-aligned-entry-1/`:
  `diagnostic.c`, `probe/probe.c`, `probe/startup_input.c`, `import_check.c`,
  `lifecycle/sentinels.c`, `generated/probe_config.h`, `import-contract.json`.
- Backend/owner/ack: `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/
  ue4ss_backend.cpp`, `.../runtime_owner.cpp`;
  `outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/
  new-units/runtime_ack_supplier.cpp`, `.../WIRING.md`.
- Composition-4 candidate: `outputs/2026-10-04-zcode-team/worker-composition-4-fix/
  REPORT.md`, `prx-candidate-composition4-1/build-receipt.json`,
  `.../verification-evidence/static-checks.json`, `.../build/supplier_import_extension.S`;
  run receipt `.superpowers/.../ue4ss-native-tick-file-owner-1/
  ue4ss-native-supplier-composition-4/run-3/receipt.json`.
- Note: the task brief's `bootstrap/module_entry.c` path does not exist; the
  entry point is `native-runtime-startup/lifecycle/module_entry.c` (the
  `bootstrap/` dir holds only the UE4SS backend/owner/activation units).
