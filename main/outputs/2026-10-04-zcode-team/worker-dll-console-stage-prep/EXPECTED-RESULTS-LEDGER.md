# EXPECTED-RESULTS-LEDGER — all-families candidate on console

Target file: `/data/modbridge-owned-dll-candidate.jsonl` (append-only; the four
new records below must be the NEXT appended records, after the 20 already
collected for db1c40e6, f72faea5, 6d295f56, 1a6b37b7, 6f5bf85a).

Derivation, cited:
- The candidate's own SDK host validation final record (normal mode),
  `worker-combined-dll-allfam/out-sdk-composition/133c5e9e59f24c2fb57e7e3422c665a1/receipt.json`
  -> `host_result_records.normal.final` — the exact oracle for every value below.
- The candidate's result-key assertions: all six family keys `crt_startup`,
  `module_enum`, `vm` + prior `mutex`, `fileio`, `init_once`,
  `synchronization`, `clock`, `worker`, `entry` present with `{"enabled":1}`
  (receipt profiles `crt_startup_profile` etc., and combined REPORT.md item 1).
- Console deltas (stdout_error 32, stdout_records 1) taken from every prior
  console result copy — the target has no stdout, error 32 is delivery-only and
  documented as expected (1a6b37b7 receipt `stdout_error_delivery_only: 32`).

## Build-frame assertions (apply to all 4 records)

- `marker` = `"modbridge.owned-dll"`, `schema` = 1
- `build_id` = `"133c5e9e59f24c2fb57e7e3422c665a1"`
- `sequence` = 0,1,2,3 contiguous; `phase` = 1,2,3,5
- Appended AFTER the existing 20 records; no existing byte modified.

## Record 0 (sequence 0, phase 1) — census-zero bootstrap

Identical shape to every prior build's sequence-0 record: all counters zero
(`execution` all 0, `counts` all 0, `results` [0,0], `cleanup` {pending:0,
retired:0}, `delivery` all 0), `entry.enabled` 0. This record existing at all
proves crt1 startup + report-file open succeeded.

## Record 1 (sequence 1, phase 2) — attach stage, family census visible

- `execution`: `{stage:2, started:1, run_attempted:0, completed:0,
  dll_calls_started:0, dll_calls_returned:0}`
- `entry.enabled` 1, attach/detach calls all [0,0]
- `clock` `{enabled:1, session_begins:[0,0], session_ends:[0,0]}`
- `worker` `{enabled:1, calls_started:0, calls_returned:0, begin:[0,0,0,0],
  result:[0,0,0,0]}`
- **Family keys — the load-bearing assertion**: all of
  `synchronization`, `init_once`, `mutex`, `fileio`, `crt_startup`,
  `module_enum`, `vm` present, each exactly `{"enabled":1}`.
  `fileio`, `crt_startup`, `module_enum`, `vm` appear on target for the
  FIRST time in this record.
- `cleanup` {pending:1, retired:0}; `delivery` file_records 1, stdout_records 1,
  stdout errors 0 at this phase.

## Record 2 (sequence 2, phase 3) — run complete

- `execution`: `{stage:9, started:1, run_attempted:1, completed:1,
  dll_calls_started:3, dll_calls_returned:3}`
- `entry`: attach_calls [1,1], attach_result **[7,7]**, detach_calls [0,1],
  detach_result [0,0], contract_errors 0
- `clock`: begins [1,1], ends [0,1]
- `worker`: calls_started **4**, calls_returned **4**, begin **[5,1,258,259]**
  (the 258/259 timeout proof), result
  **[2166572391, 1234605616436508552, 3782448555, 0]**
  (= 0x81234567 / cookie 0x1122334455667788 / 0xe17395ab / 0)
- `counts`: consumer_imports **78**, helper_imports **10**,
  source_acquires [1,1], source_releases [0,1]
- `results` = **[86,93]**
- `first_failure`: all zeros (stage 0, native_status 0, windows_error 0,
  module_cause 0)
- `memory.last`: `{operation:3, native_result:0, native_errno:0, offset:0,
  length:98304, rights:0}`; memory.first_failure all zeros
- `delivery`: first/last_file_error 0; first/last_stdout_error **32**
  (delivery-only), file_records 2, stdout_records 1

## Record 3 (sequence 3, phase 5) — FINAL record, pass/fail oracle

Everything in record 2, plus:
- `execution.stage` = **11**
- `entry`: detach_calls [1,1], detach_result [0,0], attach_result still [7,7],
  contract_errors 0
- `clock`: session_ends [1,1]
- `counts.source_releases` = [1,1]
- `cleanup` = **{pending:0, retired:1}**
- `first_failure` all zeros; `delivery.first_file_error` 0; file_records 3
- `results` still [86,93]; consumer_imports still 78; helper_imports 10

PASS = final record matches every bolded value above and first_failure is all
zeros. Expected-vs-prior deltas to NOT misread:
- `worker.calls_started 4/4` — every prior console run showed 2/2. The +2 pair
  is the fileio integration's extra worker pair (fileio-apply host record also
  4/4; combined REPORT: hooks added without touching earlier evidence). 4/4 is
  CORRECT here; 2/2 would mean fileio workers did not run.
- `consumer_imports 78` = prior console best 51 (mutex) + 10 fileio + 7
  crt_startup + 5 module_enum + 5 vm (receipt profiles: fileio
  actual_pe_imports 10, crt_startup exercised_imports 7, module_enum 5,
  vm 5).
- `stdout_error 32` with stdout_records 1 — present in every console run;
  host shows 0/0. Delivery-only, not a failure.

## Failure signatures (what each plausible record shape means)

| observation | meaning | first suspect |
|---|---|---|
| 0 new records for the build_id | payload never ran: dispatch not actually executed, wrong catalog path, or crash before phase-1 logging | check `/list_payloads` + manager responsiveness; verify dispatch URL matched the uploaded alias; do NOT blind re-dispatch |
| exactly 1 record (phase 1 only) | crash between phase-1 log and stage 2 — early load problem; with this build that means ELF/import resolution, and the 12 never-dispatched fileio imports (closedir, lstat, mkdir, opendir, pread, pwrite, readdir, strcmp, strdup, strlen, strrchr, unlink) are the prime suspects | import providers: libSceLibcInternal (closedir/opendir/readdir/strcmp/strdup/strlen/strrchr), libkernel_web (lstat/mkdir/pread/pwrite/unlink) |
| 2 records, stops at phase 2 (stage 2, run_attempted 0, cleanup pending 1) | hang/crash during family owner setup or worker spawn before the consumer run | new VM owner init (mb_win_vm_abi glue), CRT startup census walk, or the 4-worker spawn |
| phase 3/5 present but `results` != [86,93] | consumer export gate broken (attach results not 7/7 or an export refused) | entry adapter / bind chain; compare attach_result |
| `consumer_imports` != 78 or `helper_imports` != 10 | bind-chain regression — the exact host-visible failure mode the combined build hit at host stage 8 when the module-enum binder shadowed GetModuleHandleA/GetProcAddress/LoadLibraryA (77/80 bound) before the documented bind-order fix | bind order: loader-bridge fallback BEFORE family binders |
| a family key missing or `{enabled:0}` (esp. fileio/crt_startup/module_enum/vm) | that family's profile define/service init failed even though the build claims it | fixture/service begin path for that family |
| `first_failure` nonzero in phase 3/5 | fixture assertion fired; decode by range: sync/initonce −1..−53, mutex −60..−78, fileio −80..−89 (WriteFile/GetLastError paths incl. documented OPEN_ALWAYS quirk −85), crt_startup −90..−97 (_initterm/_initterm_e/atexit/onexit), module_enum −100..−104 (probe record mbmodenum.dll/MbProbe), vm −110..−117 (alloc/protect/free/query) | the family owning the range; module_cause names the PE module |
| `worker.begin` not [5,1,258,259] or `worker.result` not the oracle quadruple | owned-worker contract broken (timeout proof or cookie lost) | mb_win_threads/worker ABI |
| `cleanup` {pending:1, retired:0} in phase 5 | session teardown incomplete | service_end ordering (owners close newest-first; VM 53/54) |
| `delivery.first_file_error` != 0 | report-file write failed on target | the /tmp-trap class (c49108fe); but path is compiled /data — treat as filesystem/service error |
| `memory.first_failure` nonzero | PS5 memory backend error | pe_ps5_memory rights/offset fields in that record |
| manager `OK` but console unresponsive afterwards | payload wedged the process | stop, record, no re-dispatch; recovery is a user-level console action |

## Disposition rules

- All four records match the ledger: stage PASSED; write the console receipt
  (status `ALLFAM_DLL_TEST_TARGET_RUN_PASSED` pattern), preserve the result
  copy, update CURRENT-BUILD notes, leave the JSONL intact for the next build.
- Any mismatch: freeze the record copy + receipt with `expected_values_matched:
  false` and the exact divergent fields; no re-dispatch, no rebuild decision in
  the console session — reconciliation happens host-side against the vendor
  tree first.
