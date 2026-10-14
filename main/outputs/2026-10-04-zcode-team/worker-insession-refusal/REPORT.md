# REPORT — worker-insession-refusal (2026-10-05/06)

Bounded unit: the in-session command service that SURVIVES a recoverable
initialization failure (the pool/seeds refusal), extending
worker-insession-diag's grammar v1 to v2 and moving the poll off the
never-started event loop onto a dedicated thread that exists from
module_start until module_stop. No composition, no console, no build, no
game claims. No shared-lineage edits (worker-insession-diag's frozen files
are untouched).

## 0. Staged file list

```
outputs/2026-10-04-zcode-team/worker-insession-refusal/
├── REPORT.md                                   this note
├── ADMISSION-NOTE.md                           integration note (compose-time)
├── staging/
│   ├── insession_cmd2_core.h        grammar-v2 API (C/C++ dual, NOEXCEPT macro)
│   ├── insession_cmd2_core.c        strict scanner: pure, fixed buffers, no I/O
│   ├── insession_cmd2_service.h     service API + thread/safety/import laws
│   ├── insession_cmd2_service.cpp   dedicated poll thread + dispatch + direct
│   │                                UE4SS.log writer (NO UE4SS includes at all)
│   └── diffs/
│       ├── runtime_module_entry-insession2-hook.diff   staged, NOT applied
│       └── runtime_owner-insession2-refused.diff       staged, NOT applied
└── fixtures/
    ├── host_shims2.c                POSIX shims + fake copyin bridge + fake
    │                                IO controls a..d (host only)
    ├── test_insession2_host.c       s0-s10 scenario driver (host only)
    ├── run_host_tests2.sh           harness + receipt writer (v1 pattern)
    └── receipts/host-test-pass.txt  PASS receipt (sha-pinned sources)
```

Read-only references (never modified): worker-insession-diag/staging+REPORT
(grammar v1 lineage), worker-pad-defer/staging/program-wpd1/
UE4SSProgram.cpp, worker-d1-determinism-diagnosis/.../runtime_module_entry.c,
worker-ack-integration-deep/vendored/bootstrap/runtime_owner.cpp +
ue4ss_backend.cpp, worker-copyin-unit/staging/UnrealInitializer.cpp,
worker-comp6-driver/.../runtime_start_bridge.cpp (copyin probe),
worker-diagnostic-sink/staging/diagnostic.c, l10-1/l10-2/l11-2
supplier-contract.json.

## 1. The task's three answers

**(1) Thread choice — dedicated thread (option 3).** The task's preferred
option is refuted by the sources: the module_start chain runs SYNCHRONOUSLY
on the loader control thread (which must return from module_start,
runtime_module_entry.c:38-40), the RuntimeOwner init thread's proc RETURNS
after the refusal catch (runtime_owner.cpp:111-127 — it is the thread that
ran the uninstall path, wpd1 UE4SSProgram.cpp:520-577/:546, and then
terminates), and the event loop jthread (wpd1:709) is created only AFTER
setup_unreal (:655) which contains the ScanGame seeds chain (:1319 ->
UnrealInitializer.cpp:854-856, seed_refused :756-762; early-scan swallow
:1330-1332, Initialize surfaces :1814/:1826). On EVERY refusal path no
existing thread remains as a persistent host, and v1's poll site (inside
`update`) never runs. The service therefore spawns its own lightweight
std::thread in module_start (before start_composed_runtime, i.e. before any
seeds work) and joins it at the top of module_stop. Full evidence chain with
pinned shas: ADMISSION-NOTE §3.

**(2) Command surface v2** (strict superset of v1; one intake slot
`/data/modbridge/intake/in-session-cmd.json`, rename-consumed to
`.consumed`):
- `{"cmd":"noop"}` -> ack line
- `{"cmd":"diag","tag":...,"repeat":N}` -> diagnostic_event("insession_"+tag, N)
- `{"cmd":"io_probe","op":"a"|"b"|"c"|"d"}` -> registered batched control
  slot (worker-io-controls); registration API
  `insession_cmd2_register_io_control(slot, fn)` with required ABI
  `int fn(char *tags_out, unsigned cap)`; unwired -> "controls not wired"
  (unit links and works before that worker lands)
- `{"cmd":"pool_probe"}` -> FRESH `bridge_kernel_write_copyin_probe(
  registered_addr, 0x20000)` per command (the once-per-boot gate is the
  INITIALIZER's static, never the bridge's); registered via
  `insession_cmd2_register_pool_probe_target(SeedTable::kBaseObserved +
  SeedTable::kFNamePoolLabel, kFNameBlockCapacityBytes)`; unregistered ->
  "target not registered"; reply line carries addr/len/class
- `{"cmd":"log","text":"<1..96 printable ASCII>"}` -> sanitized append to
  /data/plugins/UE4SS.log with the entry freshness prefix
- all logging is a DIRECT bounded file append through the proven kernel
  trio — deliberately NOT Output::send, because after a refusal the UE4SS
  output devices may already be torn down

**(3) Safety case** — ADMISSION-NOTE §4 in full: 100 ms cadence / 10 ms
stop latency; idle = one failed open; 512 B bounded read; dispatch held
until settle_startup (no command can race the wiring thread's diagnostics);
post-settle single-writer law; ONLY in-contract symbols (sceKernelOpen/
Close/Write, read, rename, pthread_create/pthread_join via std::thread,
nanosleep via sleep_for, getpid, clock_gettime, strcmp/strlen — all
verified strong in l10-1/l10-2/l11-2, 196/0); zero UE4SS includes; killable
at module_stop; never throws.

## 2. Host-test results (host logic proof ONLY — no console claim)

`fixtures/run_host_tests2.sh` compiles the staged units with the REAL
precedent-pinned diagnostic.c (sha f5e96c41..., r-s1 copy) against POSIX
shims; receipt at fixtures/receipts/host-test-pass.txt
(PASS 2026-10-06T04:27:30Z; re-run 3/3 stable; Apple clang 17):

- **s0 grammar v2 table** — v1 carriers keep v1 verdicts (order-free diag,
  repeat 1/1024 pass and 0/1025/dup/leading-zero reject, tag 48/49 boundary,
  escapes, unknown cmd incl. "reload"/case-sensitivity, trailing bytes);
  v2 rows: io_probe a-d pass; op e/A/ab/missing/dup reject; pool_probe pass
  and extra-keys reject; log 96 pass / 97 / empty / tab-escape / extra-key
  reject; strict key exclusivity per command.
- **s6 idle** — exactly one open-try, zero effects.
- **s2 unwired** — io_probe with no controls and pool_probe with no target:
  bounded loud replies, bridge and controls never touched, consumed.
- **s1 wired dispatch** — control b called once (rc=42 + tags line),
  pool_probe hits the fake bridge with EXACTLY the registered addr/len,
  log text line, noop ack.
- **s7 freshness** — a second pool_probe command calls the bridge AGAIN
  (fresh, not once-per-boot).
- **s4 rejects** — garbage / unknown cmd / trailing / unknown key / bad op /
  escape / oversize(599 B): every one loud, consumed, exactly one rename,
  never re-read on the next poll.
- **s5 at-least-once** — injected rename failure: executes, loud
  CONSUME-RENAME FAILED, file kept, re-executes on the next poll, consumes.
- **s3 THE DIRECTIVE PROOF (real service thread)** — with NO event loop, NO
  pump, NO engine, and `insession_cmd2_note_initializer_refused()` set:
  refused triage line exactly once; a staged diag command is HELD (file
  untouched, nothing executed, no bridge calls) across multiple poll cycles
  until `settle_startup()`, then executes + consumes ("diag tag=probe1
  repeat=7 emitted"). This is the audit directive demonstrated end-to-end:
  the service is callable after the initializer refused.
- **s8 real-thread io probes** — io_probe d and a dispatched through the
  intake by the poll thread itself.
- **s10 stop bound** — service_stop joined in 1-10 ms (< 500 bound); a
  command staged after stop is left untouched.
- **python strict validation** — 3/3 supplier rows strict-parse (seq 1..3,
  tags {insession_probe1, insession_flaky x2}, values {7,3,3}, t_ms
  nondecreasing, no "reload" ever); entry channel 3 lines; UE4SS.log
  exactly 21 lines, freshness prefix on every line, full inventory matched.
- **Totals: 94 C checks, 0 fails; python PASS; 3/3 repeat runs PASS.**

## 3. Bugs the host test caught (and one courtesy flag)

- `insession2_append_u32` originally passed a non-NUL-terminated decimal
  scratch buffer to the string-append helper: stack bytes leaked into the
  log line (observed as garbage mid-line in the fixture's hex dump). Fixed
  to direct char copies in this unit. COURTESY FLAG: worker-insession-diag's
  frozen `insession_cmd_poll.cpp` carries the same helper shape
  (insession_append_u32 -> insession_append(&digits[--n])); its v1 tests
  passed (small repeat values), but the latent shape is identical — its
  owner should confirm or fix at composition time. This worker did NOT edit
  that file.
- A 9-byte scratch buffer truncated the pool_probe len hex ("len=0x000000");
  replaced with a bounded direct 8-digit hex append.
- `noexcept` in a header included as C breaks C consumers; fixed with the
  v1-style INSESSION2_NOEXCEPT macro.

## 4. Honest bounds

- Host logic proof only. Parser, dispatch decisions, thread lifecycle,
  settle/refused gating, fresh-probe behavior, consumption semantics, stop
  bound and file-op sequences are proven. Console behavior (FTTP STOR
  reachability mid-session, libkernel read/rename/pthread binding in the
  game process, real copyin verdicts in-game, UE4SS.log interleaving under
  real frame load, behavior when the game quits while the service runs) is
  NOT measured and is not claimed anywhere.
- The service TU has NO console-only branch (unlike v1): host and console
  compile the same code against different symbol providers; the first
  composition compile plus the mandatory per-build 196-import audit is the
  remaining console-side proof.
- Line numbers cited are from the exact sha-pinned staging copies listed in
  ADMISSION-NOTE §3 and drift if those files change.
