# worker-static-tls-7fam — REPORT.md (restart; 2026-10-04)

Worker: worker-static-tls-7fam (restart — the prior attempt completed only
CLAIM.json, which was preserved and reused). Budget ~65 tool calls of 100.
No network, no console contact, no target (PS5) execution, no nested
subagents; writes only under this worker directory.

Status: **STATIC_TLS_7TH_FAMILY_HOST_AND_SDK_ELF_CANDIDATE_PASSED** —
`OWNED_CANDIDATE_HOST_CONTROLS_PASSED` (full 7-family `--host-only`) and
`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; the ELF artifact was never
executed anywhere.

## What was delivered

1. `TLS-ADMISSION-DESIGN.md` — the pe_module TLS-admission decision with
   citations into the vendored loader/PE sources: opt-in admission ticket
   (directory 9 + `MB_PE_REQUIRES_TLS`), load-vs-first-access staging,
   lifecycle/trampoline wiring, x64 index model, and the verified linker
   facts.
2. `CONSOLE-DELTA.md` — expected console-results delta: `static_tls` key
   enabled, consumer_imports 78→82, ELF import delta ZERO, win64cc IR +1.
3. Vendored implementation (all under `vendor/`, a copy of
   worker-combined-dll-allfam/vendor plus the worker-static-tls family
   sources):
   - `windows-execution/pe_module.{h,c}`: opaque `tls_admission` ticket;
     `admission()` accepts dir 9 + the TLS requirement bit only when set;
     consumed before open's memset; default refusal byte-identical.
   - `windows-services/mb_win_static_tls.{h,c}` (vendored family owner; one
     documented adaptation): settable dispatch hook
     (`mb_win_static_tls_set_dispatch`) so the composition routes callbacks
     through an ms_abi pointer type (mb_win_entry pattern) — exactly one
     win64cc *call* in owned_runtime's bridge IR, no extra win64cc
     *definition*.
   - `windows-services/mb_win_static_tls_abi.{h,c}` (vendored, include-path
     adaptation only): binder for the eight adapter names;
     `@tls_import_thunk` is the ELF's single new win64cc definition.
   - `windows-candidate/build.py`: `--static-tls` following the exact `--vm`
     precedent (constraint, profile define, native/abi source lists,
     `statictls-lib` def link, fixture compile define, consumer extra lib,
     receipt `static_tls_profile`, IR expectation
     `mb_win_static_tls_abi: 1`, result-key assertion) plus the verified
     TLS-directory patch: lld-link synthesizes no TLS directory (verified
     against the rust lld-link and LLVM 21.1.8 `lld -flavor link`), so the
     consumer link is post-patched to point data directory 9 at the image's
     own `.rdataT`/`__tls_used` struct (40 bytes, relocation-filled VAs);
     the unsupported-directory guard now allows+requires 9 for the consumer
     only under the flag.
   - `tests/worker-statictls.h` + `worker-statictls-kernel.def`: fixture
     hook riding LoaderExercise with codes -120..-127 (index assigned and
     stable, block=template copy, idempotence, scratch write, ordered
     callback prefix, template bounds, begin-window persistence,
     collect-window isolation/persistence/log) and a real TLS directory
     (one `.tls` region object: 16-byte template + index cell + end marker;
     two callbacks in `.rdataC`; the directory in `.rdataT`).
   - `fixtures/entry_consumer.c` + `tests/worker-fixture.c` hooks; the
     internal worker thread publishes its observations (same index, NULL
     block, readable template — the honest per-thread-isolation evidence
     for a never-attached thread) before its `SetLastError`, because
     `tls_get` resets the thread's last-error slot.
   - `owned_runtime.{h,c}` + `result_report.c`: `static_tls_enabled` report
     key; owner open + directory prepare at stage 55, owner close at 56
     (unique stages); begin_session + admission ticket in service_begin
     (module EMPTY); registration at the first consumer bind (LOADING;
     sections already copied, relocations applied — the load-time index
     write lands on final image bytes); process_attach before
     `mb_win_call_entry`, process_detach + index-cell audit + unregister
     inside `entry_invoke(0)` while the image is still mapped; end_session
     newest-first in service_end; thread_attach in every
     `worker_call_begin` window; import formula +4 (`__tls_index`,
     `__tls_start`, `__tls_end`, `__tls_import`).
   - Driver import formulas extended (`test_entry_candidate.c`,
     `test_worker_candidate_failures.c`).

## Evidence (receipts in this worker dir)

1. Smoke (entry+workers+static-tls): `out-smoke/3e9716d…/receipt.json` —
   `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`, consumer_imports 25, results
   [86,93], static_tls enabled, cleanup retired, contract_errors 0.
2. Full 7-family host validation (`--host-only` + all seven family flags):
   `out-host-validation/4aaec2e…/receipt.json` —
   `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`; both driver modes: consumer
   imports **82**, helper 10, results **[86,93]**, worker 4/4, all nine
   keys `{"enabled":1}` incl. `static_tls`, contract_errors 0, cleanup
   retired; both worker-cleanup-failure modes keep the exact first failure
   (stage 25 / MB_WIN_WORKERS_NATIVE_ERROR, worker 2/2 retained-admission
   proof). Log: `out-host-validation.log`.
3. SDK composition (same flags minus --host-only, `--fileio-root
   /data/modbridge --report-path /data/modbridge-owned-dll-candidate.jsonl`):
   `out-sdk-composition/e72f531…/receipt.json` —
   `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; artifact
   `owned-dll-candidate.elf` 334656 bytes sha256 83ee2ae95268a9b7…;
   **ELF undefined imports 55 — identical set to the allfam baseline
   (delta ZERO, none added/removed)**; win64cc definitions exactly +1
   (`@tls_import_thunk`); the target report path is compiled into the
   profile only (`target_path_opened_by_build: false`); 600 s wall guard
   respected (run completed well under it). Log: `out-sdk-composition.log`.

## Debugging found and fixed during this task (kept honestly)

1. **lld-link cannot emit TLS directories** (experiment, `scratch/`): both
   linkers ignore `__tls_used`; `$`-subsections are regrouped and
   unreferenced template bytes dropped by `/opt:ref`. Fixed by the build
   post-link directory patch + single-object `.tls` region referenced from
   code.
2. **Last-error clobbering**: `mb_win_threads_tls_get/set` reset the
   thread's Windows last-error slot, so publishing TLS observations or
   auditing after a TLS read breaks the fixture's `GetLastError`
   assertions (and the first audit design was destroyed by the owner's own
   detach path). Fixed by ordering (publish before `SetLastError`; collect
   check after the -18 assertion) and by moving the final audit off the
   last-error channel entirely.
3. **Audit off-by-one**: the final-log validation ran in the SECOND
   callback (cb2), which dispatches before cb1 appends the last `(0,1)`
   entry — always saw 7 of 8. Moved to cb1 (true last dispatch).
4. **Callback-log expectation vs admission windows**: WorkerCollect reuses
   WorkerBegin's worker-call admission (by design), so there are four
   attach windows, not five; and cleanup-fault runs detach early. The
   final audit is now structural (attach pair, k ascending attach pairs,
   descending detach pair) with exact prefixes still asserted where
   deterministic (-124, -127).
5. One intermediate run wedged a host driver in Rosetta's abort path
   (bounded-retry watchdog) — the same UE signature the original
   worker-static-tls REPORT documented. Two of this worker's
   `test-sdk-driver` processes remain in uninterruptible (UE) state; no
   global escape and no process termination was used (other workers'
   pre-existing UE processes were also present and left untouched).

## Honest limits and deviations

- No target/console execution, no PRX; the ELF is a compiled candidate.
  `target_executed=false`, `console_contact=false` in every receipt.
- `__tls_array` stays UNRESOLVED; no TEB emulation; lazy (no) allocation
  for never-notified threads — divergences inherited verbatim from
  worker-static-tls REPORT; the fixture asserts the honest observation
  (internal thread sees NULL) instead of fabricating eager blocks.
- One adaptation to the vendored family owner (settable dispatch hook,
  default = original direct call) and two include-path/binder-order
  composition points; everything else is wiring per INTEGRATION.md.
- The exact full-flow callback table (12 entries with file I/O) is
  validated structurally, not positionally, so cleanup-fault runs detach
  with shorter logs by design; the passing full run's count is implied by
  the driver's worker 4/4 and file I/O out-contract assertions.
- `scratch/` retains the linker-experiment sources; `out-smoke/` retains
  the failed intermediate receipts as provenance (raw, unchanged).

## Non-claims

No Windows-loader parity, no console-results claim beyond the delta doc's
expectations, no quality score implied; gameplay remains a prototype and
this is component evidence only.
