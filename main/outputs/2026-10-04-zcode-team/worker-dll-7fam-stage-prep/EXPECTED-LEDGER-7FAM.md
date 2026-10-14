# EXPECTED-LEDGER-7FAM — all-families + static-TLS candidate on console

Target file: `/data/modbridge-owned-dll-candidate.jsonl` (append-only; the
four new records below must be the NEXT appended records, after the **36
already collected** for db1c40e6 ×4, f72faea5 ×4, 6d295f56 ×4, 1a6b37b7 ×4,
6f5bf85a ×4, 664f83a5 ×8, 53808597 ×4, 133c5e9e ×4 — count re-verified from
the controller result copy 2026-10-04).

Derivation, cited:
- The candidate's own SDK host validation, normal mode, final record —
  `worker-static-tls-7fam/out-sdk-composition/e72f5318233845369a68556cd61e59ca/`
  `receipt.json` -> `host_result_records.normal.final` (cross-checked against
  `host-normal.jsonl` lines, all 4 records read) — the exact oracle for every
  value below. Host values are the oracle because the 6-family run proved
  console records match host records field-for-field (except the two
  console-only delivery deltas, below).
- Baseline being differenced against: the 6-family console pass — build
  `133c5e9e59f24c2fb57e7e3422c665a1`, `ALLFAM_DLL_TEST`, 4 records on target,
  per `worker-dll-console-stage-prep/EXPECTED-RESULTS-LEDGER.md`.
- Fixture codes read directly from
  `worker-static-tls-7fam/vendor/windows-candidate/tests/worker-statictls.h`
  (codes -120..-127 verified in source, unique across the fixture tree).

## Build-frame assertions (apply to all 4 records)

- `marker` = `"modbridge.owned-dll"`, `schema` = 1
- `build_id` = `"e72f5318233845369a68556cd61e59ca"` (NOT 133c5e9e — that was
  the 6-family build already on the file)
- `sequence` = 0,1,2,3 contiguous; `phase` = 1,2,3,5
- Appended AFTER the existing 36 records; no existing byte modified.

## Phase-shape change vs the 6-family run: NONE

Phases [1,2,3,5], `execution.stage` 0/2/9/11, `dll_calls` 3/3,
`worker.calls_started/returned` 4/4, `entry` attach slots [7,7] — the record
skeleton is identical to the 6-family run. Exactly three intra-record values
change, and one key appears:

| field | 6-family console (133c5e9e) | 7-family expected (e72f5318) | why |
|---|---|---|---|
| `static_tls` | absent | `{"enabled":1}` from sequence 1 onward; **absent at sequence 0 is CORRECT** (census-zero record carries no family keys, host seq0 confirmed) | report key asserted exactly when `--static-tls` is on (build.py result-key assertion, result_report.c) |
| `counts.consumer_imports` | 78 | **82** | consumer binds 4 more PE imports: `__tls_index`, `__tls_start`, `__tls_end`, `__tls_import` (host measured 82 in both driver modes) |
| `memory.last.length` | 98304 | **147456** | last memory operation's length scales with the mapped image; the ELF grew 300456 -> 334656 bytes (larger image, +1 family). Host recorded 147456; 6-family proved this field matches host on target |

`counts.helper_imports` stays **10** (helper carries no TLS directory and
imports none of the adapter names). The ELF's undefined-import SET is
byte-identical to the console-passed 6-family ELF (55 == 55, `nm -u` diff
clean this session) — the 4 TLS symbols are bound as PE imports inside the
consumer, not new ELF dynamic imports; win64cc definitions are +1 exactly
(`@tls_import_thunk` in `mb_win_static_tls_abi`).

## Record 0 (sequence 0, phase 1) — census-zero bootstrap

Identical shape to every prior build's sequence-0 record: all counters zero
(`execution` all 0, `counts` all 0, `results` [0,0], `cleanup` {pending:0,
retired:0}, `delivery` all 0), `entry.enabled` 0, and NO `static_tls` key
(and none of the other family keys). This record existing at all proves crt1
startup + report-file open succeeded.

## Record 1 (sequence 1, phase 2) — attach stage, family census visible

- `execution`: `{stage:2, started:1, run_attempted:0, completed:0,
  dll_calls_started:0, dll_calls_returned:0}`
- `entry.enabled` 1, attach/detach calls all [0,0]
- `clock` `{enabled:1, session_begins:[0,0], session_ends:[0,0]}`
- `worker` `{enabled:1, calls_started:0, calls_returned:0, begin:[0,0,0,0],
  result:[0,0,0,0]}`
- **Family keys — the load-bearing assertion**: all SEVEN of
  `synchronization`, `init_once`, `mutex`, `fileio`, `crt_startup`,
  `module_enum`, `vm`, **`static_tls`** present, each exactly
  `{"enabled":1}`. `static_tls` appears on target for the FIRST time in
  this record (host seq1 confirmed present here, absent at seq0).
- `cleanup` {pending:1, retired:0}; `delivery` file_records 1.

## Record 2 (sequence 2, phase 3) — run complete

- `execution`: `{stage:9, started:1, run_attempted:1, completed:1,
  dll_calls_started:3, dll_calls_returned:3}`
- `entry`: attach_calls [1,1], attach_result **[7,7]**, detach_calls [0,1],
  detach_result [0,0], contract_errors 0
- `clock`: begins [1,1], ends [0,1]
- `worker`: calls_started **4**, calls_returned **4** (UNCHANGED from
  6-family — the TLS family adds no worker calls), begin **[5,1,258,259]**
  (the 258/259 timeout proof), result
  **[2166572391, 1234605616436508552, 3782448555, 0]**
  (= 0x81234567 / cookie 0x1122334455667788 / 0xe17395ab / 0)
- `counts`: consumer_imports **82** (delta vs 6-family +4), helper_imports
  **10**, source_acquires [1,1], source_releases [0,1]
- `results` = **[86,93]** (UNCHANGED)
- `static_tls` = `{"enabled":1}`
- `first_failure`: all zeros (stage 0, native_status 0, windows_error 0,
  module_cause 0)
- `memory.owner_status` 0; `memory.last`: `{operation:3, native_result:0,
  native_errno:0, offset:0, length:147456, rights:0}`;
  `memory.first_failure` all zeros
- `delivery`: first/last_file_error 0; first/last_stdout_error **32**
  (console-only; see below), file_records 2, stdout_records 1

## Record 3 (sequence 3, phase 5) — FINAL record, pass/fail oracle

Everything in record 2, plus:
- `execution.stage` = **11**
- `entry`: detach_calls [1,1], detach_result [0,0], attach_result still
  [7,7], contract_errors 0
- `clock`: session_ends [1,1]
- `counts.source_releases` = [1,1]
- `cleanup` = **{pending:0, retired:1}** — the static TLS owner closes only
  after the consumer unregistered with zero live blocks (unload cleanup
  proof; owner close is the newest-first stage 56)
- `first_failure` all zeros; `delivery.first_file_error` 0; file_records 3
- `results` still [86,93]; consumer_imports still 82; helper_imports 10;
  `memory.last.length` still 147456

PASS = final record matches every bolded value above and first_failure is
all zeros.

Console-only deltas to NOT misread (identical to every prior run):
- `stdout_error 32` with `stdout_records 1` — present in every console run;
  host shows 0/0. Delivery-only (target has no stdout; error 32 is the
  delivery channel's status), not a failure.
- `worker.calls_started 4/4` — correct HERE and in 6-family (2 begin/collect
  + 2 file I/O pairs). The TLS family adds no worker pair; 4/4 unchanged
  from the 6-family console pass.

## Failure triage — TLS fixture codes (first_failure.stage in phase 3/5)

The 7-family fixture rides LoaderExercise with codes -120..-127
(`worker-statictls.h`, verified in source). Range ownership:
sync/initonce −1..−53, mutex −60..−78, fileio −80..−89, crt_startup
−90..−97, module_enum −100..−104, vm −110..−117 (all inherited, see the
6-family ledger), **static_tls −120..−127 (new)**.

| code | assertion that fired (source-verified) | meaning / first suspect |
|---|---|---|
| -120 | TLS index is 0xffffffff or != `__tls_index` | index never assigned/stable — the target loader did not consume data directory 9, or the pe_module admission ticket refused (dir-9 + `MB_PE_REQUIRES_TLS` opt-in); `module_cause` names the PE module |
| -121 | block NULL or not the template copy | callback fired but block assembly wrong — `.tls` template copy path (16-byte template) |
| -122 | `__tls_import(idx,0)` != block | same-thread idempotence broken — the `@tls_import_thunk` win64cc definition / index routing |
| -123 | scratch u32 at block+16 != 0x5a17beef | block memory not writable/readable — memory-backend rights on the allocated TLS block |
| -124 | callback log prefix wrong (expect [(1,1),(1,2)] then this window's [(2,1),(2,2)]) | `.rdataC` callback table dispatch order — two-callback ordering |
| -125 | `__tls_start` template unreadable or `__tls_end != __tls_start+24` | directory-9 VA patch bounds wrong (`.rdataT` struct / relocation-filled VAs) |
| -126 | begin-window block != the entered thread's block | per-thread persistence — index cell high-bit / cell handling on the entered thread |
| -127 | collect-window checks: internal thread must see SAME index, NULL slot, readable template; main block persists; template bytes 0..16 intact; 6-entry prefix log | internal-thread isolation evidence diverged — a fabricated block for a never-attached thread, or the published observations (written before `SetLastError`) disagree with the main thread's view |

Note: -127's "internal thread sees a NULL block" is the HONEST expected
observation (lazy allocation, no TEB emulation). A run where the internal
thread sees a NON-NULL block should be treated as suspicious-of-fabrication,
not as a pass.

## Failure signatures (generic record shapes)

| observation | meaning | first suspect |
|---|---|---|
| 0 new records for build_id e72f5318… | payload never ran: dispatch not actually executed, wrong catalog path, or crash before phase-1 logging | `/list_payloads` + manager responsiveness; verify dispatch URL matched `ALLFAM7_DLL_TEST` alias; do NOT blind re-dispatch |
| exactly 1 record (phase 1 only) | crash between phase-1 log and stage 2 — early load problem. NOT the import set: the undefined-import set is byte-identical to the console-passed 133c5e9e ELF, so import resolution is pre-proven | the NEW surface: the image's TLS directory (post-link-patched data directory 9 -> `.rdataT`), `.tls` region, `.rdataC` callbacks — i.e. the target loader's handling of a directory-9 image |
| 2 records, stops at phase 2 (stage 2, run_attempted 0, cleanup pending 1) | hang/crash during family owner setup or worker spawn before the consumer run | the static TLS owner open + directory prepare (stages 55/56) or the admission path in service_begin; then the pre-existing suspects (VM owner init, CRT census walk, 4-worker spawn) |
| phase 3/5 present but `results` != [86,93] | consumer export gate broken (attach results not 7/7 or an export refused) | entry adapter / bind chain; compare attach_result |
| `consumer_imports` != 82 | bind-chain regression | 82 = 78 (console-passed allfam) + 4 TLS symbols; if 78, the TLS binds never happened; if 83+, something double-bound. `helper_imports` must stay 10 |
| `static_tls` key missing or `{enabled:0}` from seq1 onward | build ran without the static-TLS profile or its service init failed even though the build claims it | fixture/service begin path for static_tls; verify the artifact hash — a stale ALLFAM (6-family) payload under the new alias would show 78/no key |
| `first_failure.stage` in −1..−117 | pre-existing family assertion fired | 6-family ledger ranges (sync/initonce, mutex, fileio, crt_startup, module_enum, vm); module_cause names the PE module |
| `first_failure.stage` in −120..−127 | TLS fixture assertion fired | per-code table above |
| `worker.begin` not [5,1,258,259] or `worker.result` not the oracle quadruple | owned-worker contract broken (timeout proof or cookie lost) | mb_win_threads/worker ABI |
| `cleanup` {pending:1, retired:0} in phase 5 | session teardown incomplete | service_end ordering (owners close newest-first; static TLS owner is stage 55/56 and closes only after consumer unregister with zero live blocks) |
| `delivery.first_file_error` != 0 | report-file write failed on target | path is compiled `/data` — treat as filesystem/service error (the /tmp-trap class) |
| `memory.first_failure` nonzero or `memory.owner_status` != 0 | PS5 memory backend error | pe_ps5_memory rights/offset fields in that record |
| manager `OK` but console unresponsive afterwards | payload wedged the process | stop, record, no re-dispatch; recovery is a user-level console action |

## What this console run would still NOT establish

No Windows-loader parity (inherited verbatim from CONSOLE-DELTA.md): no
TEB/`__tls_array` emulation (the thunk and `__tls_index` data are the
adapter surface), lazy (not eager) blocks for never-notified threads, no
loader lock; callbacks are component evidence routed through the
composition's win64cc adapter. A PASS is the target-run proof of the
candidate as built — component evidence, not full parity, and not a
gameplay claim.

## Disposition rules

- All four records match the ledger: stage PASSED; write the console
  receipt (status pattern `ALLFAM7_DLL_TEST_TARGET_RUN_PASSED`), preserve
  the result copy, update CURRENT-BUILD notes, leave the JSONL intact.
- Any mismatch: freeze the record copy + receipt with
  `expected_values_matched: false` and the exact divergent fields; no
  re-dispatch, no rebuild decision in the console session — reconciliation
  happens host-side against the vendor tree first.
