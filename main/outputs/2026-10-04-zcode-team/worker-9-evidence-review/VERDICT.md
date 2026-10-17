# worker-9-evidence-review — VERDICT on the promoted 6d295f56 NEXT_DLL_THREADS console milestone

Reviewer: worker-9-evidence-review (slot 8), 2026-10-04. Independent, offline, read-only.
All paths relative to `/WORKSPACE/.codex/worktrees/translation-core/what/`.
Raw-record citations use the result log
`outputs/2026-10-04-zcode-controller/console/6d295f56-console-result-2026-10-04.jsonl`
(12 lines/records, 1-indexed by line). The FINAL 6d295f56 record is line 12
(sequence 3, phase 5). Source-semantic citations are to the frozen fixture sources that
produced the telemetry: `outputs/ps5-mod-bridge/windows-candidate/tests/worker-fixture.c`
and `outputs/ps5-mod-bridge/windows-candidate/tests/worker-sync.h`.

---

## Q1. Do all documented expectations hold in the FINAL 6d295f56 record? — VERDICT: PASS (every expectation, from the raw record, not the receipt)

| expectation | actual (line 12) | citation (field = value) |
|---|---|---|
| 3 DLL calls started/returned | 3 / 3 | `execution.dll_calls_started=3`, `execution.dll_calls_returned=3` |
| 2 worker calls started/returned | 2 / 2 | `worker.calls_started=2`, `worker.calls_returned=2` |
| results [86,93] | [86,93] | `results=[86,93]` |
| consumer/helper imports 44/10 | 44 / 10 | `counts.consumer_imports=44`, `counts.helper_imports=10` |
| worker exit 0x81234567 (2166572391) | 2166572391 = 0x81234567 | `worker.result[0]=2166572391` |
| parameter cookie 0x1122334455667788 (1234605616436508552) | 1234605616436508552 = 0x1122334455667788 | `worker.result[1]` |
| worker LastError 0xe17395ab (3782448555) | 3782448555 = 0xe17395ab | `worker.result[2]` |
| repeat wait 0 | 0 | `worker.result[3]=0` |
| source acquires == releases == [1,1] | [1,1] / [1,1] | `counts.source_acquires=[1,1]`, `counts.source_releases=[1,1]` |
| cleanup pending 0 / retired 1 | 0 / 1 | `cleanup.pending=0`, `cleanup.retired=1` |
| first_failure all zeros | all zeros | `first_failure={stage:0, native_status:0, windows_error:0, module_cause:0}` |
| file delivery errors 0 | 0 | `delivery.first_file_error=0`, `delivery.last_file_error=0` |

Supporting context also present and consistent: `entry.attach_result=[7,7]`,
`entry.detach_result=[0,0]`, `clock.session_begins=[1,1]`, `clock.session_ends=[1,1]`,
`delivery.first_stdout_error=32 / last_stdout_error=32` (stdout EPIPE-32 delivery-only
limitation, matching the db1c40e6/f72faea5 precedent recorded in
`outputs/ZCODE-HANDOFF-2026-10-04/VERIFIED_VS_MISSING.txt` item 2).

Note on the review brief's "[5,1,258,258?]": the actual value is
`worker.begin=[5,1,258,259]` — and 259 is the designed expectation, not an anomaly:
`worker-fixture.c` line 99 requires `WaitForSingleObject(handle,0)==258`
(WAIT_TIMEOUT) and line 101-102 requires `GetExitCodeThread==259` (STILL_ACTIVE).
The receipt's `[5,1,258,259]` matches the raw record.

## Q2. Sequence/phase sanity — VERDICT: PASS

The four 6d295f56 records (lines 9-12) form a coherent progression:
- `sequence` = 0,1,2,3 (monotonic, no gaps/duplicates)
- `phase` = 1,2,3,5; `execution.stage` = 0,2,9,11 (advancing)
- `marker`/`schema` = `modbridge.owned-dll`/1 uniformly across all four
- `delivery.file_records` = 0 -> 1 -> 2 -> 3 (strictly increasing)
- Sub-objects appear phase-gated exactly as in the f72faea5 precedent: `entry`
  present from sequence 0 (enabled 0), `clock`/`worker`/`synchronization` present from
  sequence 1 with enabled flags, first `worker.calls_*` counts and telemetry vectors
  populated from sequence 2, `cleanup.retired` flips to 1 only at sequence 3.
- Schema delta vs the f72 baseline
  (`outputs/ZCODE-HANDOFF-2026-10-04/evidence/CONSOLE-ENTRY-CLOCK-RESULT.jsonl`)
  is exactly the new `worker` and `synchronization` objects — expected for this build's
  worker/sync profile; db1c40e6 (older build) additionally lacks `entry`, `clock`,
  `worker`, `synchronization` and `first_failure.module_cause`.

## Q3. Address-wait claim adjudication — VERDICT: the record does NOT prove a waiter blocked and then woke; the controller's coverage_precision note is CORRECT and needs only a minor sharpening of its positive sentence

What `worker.begin` and `worker.result` actually are (from the frozen fixture source):
- `worker.begin = [handle, thread_id, wait0, exit_probe]`:
  `[5, 1, 258, 259]` = thread handle 5, thread id 1,
  `WaitForSingleObject(handle,0)=258` (WAIT_TIMEOUT — worker not finished at
  WorkerBegin return) and `GetExitCodeThread=259` (STILL_ACTIVE).
  Source: `worker-fixture.c` lines 94, 99, 101-102.
- `worker.result = [exit_code, observed_cookie, worker_error, repeat_wait]`:
  `[0x81234567, 0x1122334455667788, 0xe17395ab, 0]`, written by `WorkerCollect`
  after `WaitForSingleObject(handle,0xffffffff)` must return 0.
  Source: `worker-fixture.c` lines 129-135.

What the record DOES establish (all on target):
1. A genuinely pending gated obligation: at WorkerBegin return the worker thread was
   created but still running (258/259).
2. The worker thread executed its full body — the observed parameter cookie was written
   only after the gate loop and inside the critical section, the thread-local LastError
   0xe17395ab was preserved, exit 0x81234567 arrived through join, repeat wait 0
   (`worker-fixture.c` lines 42-58, 129-137; failure paths would have failed the run).
3. `WaitOnAddress` demonstrably executed on target — but only in its creator-side,
   zero-timeout admission checks inside `mb_worker_sync_prepare`
   (`worker-sync.h` line 35: unequal-compare must return TRUE, equal-compare must
   return FALSE with `GetLastError()==1460`); WorkerBegin returning 1 proves both ran.
4. The wake side demonstrably executed on target: `mb_worker_sync_unblock` calls
   `WakeByAddressSingle(gate); WakeByAddressAll(gate)` (`worker-sync.h` line 79)
   inside WorkerCollect.
5. A real cross-thread SRW/condition-variable handshake functioned: the creator's
   bounded ready-wait (`worker-sync.h` lines 59-64, `SleepConditionVariableSRW` 100 ms,
   <=20 attempts) only completes when the worker publishes `ready` under SRW via
   `WakeAllConditionVariable` (lines 71-76).

What the record does NOT establish:
1. That the worker thread ever parked inside its INFINITE `WaitOnAddress` and was
   woken. The gate loop (`worker-fixture.c` line 46,
   `while(!__atomic_load_n(&s->gate,ACQUIRE)) if(!WaitOnAddress(...))`) is a
   predicate-recheck loop: if actual PE scheduling first ran the worker thread after
   `mb_worker_sync_unblock` stored gate=1, the worker-side `WaitOnAddress` never
   executed at all; and even if it executed, its compare-to-wait semantics can return
   without parking. The telemetry has no blocked/woke counter, and `failure=0` is
   consistent with every path. This is exactly the documented caveat:
   `outputs/ZCODE-HANDOFF-2026-10-04/evidence/README.md` line 11 and
   `outputs/ZCODE-HANDOFF-2026-10-04/RUNTIME_MAP.txt` lines 126-129 ("Do not state
   that a particular payload necessarily blocked/woke merely because the thread's
   overall result passed").
2. That any waiter was parked when `WakeByAddressSingle/All` were called (wakes with
   no waiters are no-ops).
3. Any deterministic lost-wake / FIFO / wake-selection behavior on target — that
   evidence is host-side service tests only.

Checkpoint coverage_precision note (zcode_takeover_2026_10_04 ->
next_dll_threads_console_run.coverage_precision, quoted verbatim):
> "Target pass establishes actual target execution of entry/clock, owned worker
> threads, critical sections, SRW/condition-variable and address-wait APIs with
> expected results and full cleanup. It does NOT establish deterministic blocking
> address-wait coverage: the record does not prove a waiter actually blocked and then
> woke, since actual PE scheduling can bypass the blocking address wait; deterministic
> blocked/woke evidence remains from the separate host service tests only. Do not
> claim deterministic target blocking from this run."

Ruling: the negative half is exactly correct and matches the raw record and the
frozen caveat sources. The positive half is defensible but slightly imprecise:
"actual target execution of ... address-wait APIs" should say WHICH address-wait
executions are proven (creator-side zero-timeout admission checks and the wake calls);
the worker-side blocking wait's invocation itself is unproven. Suggested amendment
(keep everything after "It does NOT establish..." verbatim; replace only the first
sentence's address-wait clause):

"Target pass establishes actual target execution of entry/clock, owned worker
threads, critical sections, SRW/condition-variable APIs, and address-wait APIs in
their exercised forms (creator-side zero-timeout WaitOnAddress admission checks and
WakeByAddressSingle/WakeByAddressAll wake calls; the worker-side INFINITE address
wait's blocking path is not proven invoked), with expected results and full cleanup."

Exact wording the controller should use when describing the run's address-wait
coverage:

"On target, the worker fixture's gated handshake completed with expected results:
at WorkerBegin the worker thread was still active (WaitForSingleObject(handle,0)=258
WAIT_TIMEOUT; GetExitCodeThread=259 STILL_ACTIVE); WorkerCollect opened the gate,
joined the thread (infinite wait returned WAIT_OBJECT_0) and collected exit code
0x81234567, observed parameter cookie 0x1122334455667788, preserved thread-local
LastError 0xe17395ab, repeat wait 0. WaitOnAddress demonstrably executed only in its
creator-side zero-timeout admission checks (expected FALSE/ERROR_TIMEOUT 1460 and
compare-to-wait TRUE) and WakeByAddressSingle/WakeByAddressAll executed when the gate
opened. The record does not prove that the worker thread ever blocked inside the
INFINITE WaitOnAddress and was then woken — actual PE scheduling may first run the
worker after the gate opened, skipping the wait entirely, and the telemetry carries
no blocked/woke counter. Deterministic blocking/wake-selection evidence remains with
the separate host service tests."

## Q4. Receipt integrity — VERDICT: PASS (offline-verifiable parts all match; remote-readback claims are internally consistent but not independently re-verifiable offline)

- Recomputed sha256 of `outputs/2026-10-04-zcode-controller/console/6d295f56-console-receipt.json`
  = `93ccaf8da695d164cdd3b6fb50a0596adfd23d5a7ae200c6d2562bd60ef4a05b` (2904 bytes) —
  matches the claimed receipt hash in CONSOLE-LOG.md item 5 and the checkpoint
  (`next_dll_threads_console_run.receipt_sha256`).
- Recomputed sha256 of the result log copy =
  `512b93b23229e3b328d418d1b2998bcf75cbfe9d15405bc77f888b2acb365eb6`, 11624 bytes —
  matches receipt fields `result_copy_sha256`/`result_copy_bytes`; `records_total_in_log=12`
  and `records_for_build=4` match the actual file (12 lines; build_id counter:
  db1c40e6=4, f72faea5=4, 6d295f56=4).
- Every field of `receipt.final_record_validation` matches the raw final record
  (line 12), including `worker_begin=[5,1,258,259]`, `entry_attach_result=[7,7]`,
  `entry_detach_result=[0,0]`, `stdout_error_delivery_only=32`
  (= delivery.first/last_stdout_error 32 with stdout_records 1), `file_delivery_errors=0`.
- Receipt `artifact` = `outputs/ZCODE-HANDOFF-2026-10-04/NEXT_DLL_THREADS.elf`,
  recomputed `0dc08b37...b724`, 240960 bytes — matches `artifact_sha256`/`artifact_bytes`.
- Historical records unaltered: the 4 db1c40e6 lines (1-4) are byte-identical to the
  prior retained copy `outputs/2026-10-04-owned-dll-console/RESULT.jsonl` (diff clean),
  and its final record still shows results [86,93], imports 10/6, releases [1,1],
  cleanup 0/1, first_failure zeros, file_records 3 — consistent with
  `outputs/ps5-dll-test-data-db1c40e6/README.md` line 9 expectations. The 4 f72faea5
  lines (5-8) are byte-identical to the baseline
  `outputs/ZCODE-HANDOFF-2026-10-04/evidence/CONSOLE-ENTRY-CLOCK-RESULT.jsonl`
  (sha256 `462235361c1a11839bf03db906fcab1075f9c37ad59e3c2877c7592e32d6048c`, 4 lines),
  final record still matches VERIFIED_VS_MISSING.txt item 3
  (attach/detach [7,7]/[0,0], clock begins/ends [1,1]/[1,1], results [86,93], cleanup 0/1).
- Boundary: `upload.response "OK"`, `ftp_readback_sha256`, and catalog details describe
  console-side fetches performed by the controller. This offline review cannot
  re-perform them (no network by rule); they are internally consistent and
  corroborated by the locally-held 11624-byte console result log containing the 4 new
  6d295f56 records, but they remain controller-attested.

## Q5. Frozen inputs — VERDICT: PASS (all four, re-hashed this session)

| file | expected sha256 | recomputed | bytes |
|---|---|---|---|
| `.superpowers/native-dependency-spike/windows-sync-candidate-1/native-integration/6d295f566d084f578836b245a7748391/owned-dll-candidate.elf` | `0dc08b3766c6d8d690745562462ced538030c9be11883f9318b2e3f53473b724` | same | 240960 |
| `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx` | `3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654` | same | 30151623 |
| `outputs/CURRENT-PS5-TEST/TEST_THIS_DLL.elf` | `588d6e6ddf476fe764153df9a3235a3558f0e98e2a3b4b6070a6fb645c47bff4` | same | 202680 |
| `outputs/CURRENT-PS5-TEST/NEXT_DLL_THREADS.elf` | `0dc08b3766c6d8d690745562462ced538030c9be11883f9318b2e3f53473b724` | same | 240960 |

Bonus: the handoff alias `outputs/ZCODE-HANDOFF-2026-10-04/NEXT_DLL_THREADS.elf`
(the receipt's `artifact`) also re-hashes to `0dc08b37...b724`, 240960 bytes — the
owned-dll-candidate, the handoff artifact and the local console alias are one byte-identical triple.

## Q6. Claims audit — VERDICT: NO material overstatement found; two precision amendments recommended; one doc-currency note

Scope boundaries hold everywhere checked: every document (CONTROLLER.md, CONSOLE-LOG.md,
CURRENT-BUILD.md, receipt `scope`, checkpoint `scope`) states this is a standalone owned
SDK ELF diagnostic — not UE4SS, not game integration, not arbitrary downloaded DLL
support — with `game_files_modified=false`, `autoload_modified=false`, `game_launched=false`.
No evidence contradicts these; the result log contains only the diagnostic's own records.

1. RECOMMENDED AMENDMENT — CURRENT-BUILD.md line 11-13:
   "Profile: entry/clock + owned worker threads + critical sections + SRW locks +
   condition variables + address waits."
   Precision gap: as adjudicated in Q3, the proven on-target address-wait executions
   are the creator-side zero-timeout admission checks and the WakeByAddress* calls; a
   blocked worker-side address wait is not proven. Proposed corrected wording:
   "Profile: entry/clock + owned worker threads + critical sections + SRW locks +
   condition variables + address waits (on-target address-wait proof is creator-side
   zero-timeout admission checks plus WakeByAddress wake calls; a blocked worker-side
   address wait is not claimed — see checkpoint coverage_precision)."
2. RECOMMENDED AMENDMENT — checkpoint coverage_precision positive sentence (exact
   replacement text in Q3; negative sentence stays verbatim).
3. DOC-CURRENCY NOTE (not an overstatement) — VERIFIED_VS_MISSING.txt section
   "NEW PAYLOAD, NOT YET TARGET RUN" (lines 26-31, "All new target worker/synchronization
   behavior is pending; no upload was performed as part of this transfer"): accurate at
   handoff time, now superseded by the controller's console run. CURRENT-BUILD.md and the
   checkpoint already reflect the run; future readers of the handoff bundle alone should
   not treat that section as current. No change required inside the frozen handoff.
4. Controller-attested-only claims (CONSOLE-LOG.md items 1-3 upload/readback/catalog,
   receipt `upload.*`, `dispatch.response "OK"`): internally consistent and corroborated
   by the local result-log copy with 4 new records; not independently re-verifiable in
   this offline review. Not overstatements on available evidence, but they should remain
   attributed to the controller's console actions rather than restated as reviewer-verified.
5. Verified-as-accurate headline claims (no change needed): CONSOLE-LOG.md item 5
   "Final record PASSED all expected values" (true per Q1); CONSOLE-LOG.md item 6 /
   CURRENT-BUILD.md local-alias mirroring (hashes match locally); checkpoint
   `final_record_matched_all_expected_values: true` and status `PASSED_ON_TARGET`
   (justified by the appended console records); CONTROLLER.md "Frozen artifacts
   re-verified this session" (confirmed by Q5 hashes).

---

### Overall verdict

The promoted 6d295f56 milestone's local evidence is internally consistent, complete and
accurate: all 12 documented expectations hold in the raw final record; the 4-record phase
progression is coherent; the receipt and both hash claims verify; all frozen inputs are
unchanged; historical db1c40e6/f72faea5 records are byte-identical to their retained
copies; and the controller's address-wait non-claim (coverage_precision) is correct.
Recommended actions are wording-level only (Q3 amendment, Q6 items 1-2). The one hard
limitation is inherent to an offline review: the controller's console-side fetch
attestations (upload OK, FTP readback) cannot be independently re-performed here.
