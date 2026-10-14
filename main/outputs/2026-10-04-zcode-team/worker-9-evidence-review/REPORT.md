# REPORT — worker-9-evidence-review (slot 8), 2026-10-04

Independent offline evidence review of the promoted 6d295f56 NEXT_DLL_THREADS console
milestone. Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`.
Outputs: `VERDICT.md` (full verdicts + citations), `findings.json` (44 machine-readable
checks), this report. All checks read-only; only this output directory was written.
No network, no console contact, no credentials read.

## Bottom line

The milestone's local evidence is solid and honest. Every documented expectation
re-validates against the raw final record, the receipt and both hash claims verify,
all frozen inputs are unchanged, historical records are byte-identical to their
retained copies, and the controller's address-wait non-claim is correct. The only
actionable items are two wording-level precision amendments. The milestone stands.

## What was verified

- Check 1 — Final record (result log line 12): 3/3 DLL calls, 2/2 worker calls,
  results [86,93], imports 44/10, worker.result = [0x81234567, 0x1122334455667788,
  0xe17395ab, 0] (decimal values verified), acquires==releases==[1,1], cleanup 0/1,
  first_failure all zeros, file delivery errors 0. All PASS.
- Check 2 — Coherent progression: sequence 0-3, phases 1/2/3/5, stages 0/2/9/11,
  marker/schema uniform, delivery.file_records 0->1->2->3, sub-objects phase-gated
  exactly like the f72 precedent. PASS.
- Check 3 — Address-wait adjudication (see below). The record does NOT prove
  blocked-then-woke; the controller's coverage_precision note is correct.
- Check 4 — Receipt sha256 = 93ccaf8d... (2904 B), result copy sha256 = 512b93b2...
  (11624 B, 12 records, 4 for the build), every final_record_validation field matches
  the raw record, artifact hash/bytes match, db1c40e6 lines byte-identical to
  outputs/2026-10-04-owned-dll-console/RESULT.jsonl, f72faea5 lines byte-identical to
  the handoff baseline evidence file. PASS (console-side fetch attestations remain
  controller-attested; not re-performable offline).
- Check 5 — All four frozen inputs re-hash to their expected values (owned-dll-candidate.elf
  0dc08b37.../240960; modbridge_runtime_aligned_entry.prx 3b998f09...;
  TEST_THIS_DLL.elf 588d6e6d...; NEXT_DLL_THREADS.elf 0dc08b37.../240960). PASS.
- Check 6 — No material overstatement in CONTROLLER.md / CONSOLE-LOG.md /
  CURRENT-BUILD.md / checkpoint. Scope boundaries (standalone owned SDK ELF diagnostic;
  not UE4SS, not game integration, not arbitrary DLL support; game unmodified and not
  launched) hold everywhere.

## Address-wait adjudication (the substantive question)

Decoded from the frozen fixture sources
(`outputs/ps5-mod-bridge/windows-candidate/tests/worker-fixture.c`,
`.../worker-sync.h`):

- `worker.begin=[5,1,258,259]` = [thread handle 5, thread id 1,
  WaitForSingleObject(handle,0)=258 WAIT_TIMEOUT, GetExitCodeThread=259 STILL_ACTIVE].
  The review brief's "[5,1,258,258?]" is resolved: 259 is the designed and correct
  value (fixture asserts 259 = STILL_ACTIVE, worker-fixture.c line 102).
- Proven on target: a genuinely pending worker at Begin; the worker's full gated body
  (cookie observed after the gate loop inside the critical section, thread-local
  LastError preserved, exit code through join); creator-side zero-timeout
  WaitOnAddress admission checks (equal-compare FALSE + GetLastError 1460,
  unequal-compare TRUE — worker-sync.h line 35); WakeByAddressSingle/All executed when
  the gate opened (worker-sync.h line 79); a functioning cross-thread SRW/CV
  ready-handshake (bounded SleepConditionVariableSRW loop, worker publishes under
  WakeAllConditionVariable).
- NOT proven: that the worker thread ever parked inside its INFINITE WaitOnAddress
  and was woken. The gate loop is a predicate-recheck loop; if PE scheduling first ran
  the worker after the gate opened, the worker-side WaitOnAddress never executed at
  all — and the telemetry has no blocked/woke counter. This matches the handoff caveat
  (evidence README line 11; RUNTIME_MAP.txt lines 126-129).
- Checkpoint coverage_precision note: quoted and ruled CORRECT (negative sentence
  exactly right). One qualifier recommended for its positive sentence — see below.

## Exact actionable items (mismatches needing correction)

1. outputs/CURRENT-PS5-TEST/CURRENT-BUILD.md, lines 11-13 — sentence:
   "Profile: entry/clock + owned worker threads + critical sections + SRW locks +
   condition variables + address waits."
   Issue: could be read as proving blocking address waits on target; proven executions
   are creator-side zero-timeout admission checks + WakeByAddress wake calls.
   Replace with: "...condition variables + address waits (on-target address-wait
   proof is creator-side zero-timeout admission checks plus WakeByAddress wake calls;
   a blocked worker-side address wait is not claimed — see checkpoint
   coverage_precision)."
2. Checkpoint zcode_takeover_2026_10_04.next_dll_threads_console_run.coverage_precision,
   first sentence — "actual target execution of entry/clock, owned worker threads,
   critical sections, SRW/condition-variable and address-wait APIs with expected
   results and full cleanup."
   Issue: "address-wait APIs" is imprecise about which executions are proven.
   Replace with: "actual target execution of entry/clock, owned worker threads,
   critical sections, SRW/condition-variable APIs, and address-wait APIs in their
   exercised forms (creator-side zero-timeout WaitOnAddress admission checks and
   WakeByAddressSingle/WakeByAddressAll wake calls; the worker-side INFINITE address
   wait's blocking path is not proven invoked), with expected results and full
   cleanup."
   Keep the rest of the note verbatim — it is exactly right.
3. Doc-currency note (no edit required): VERIFIED_VS_MISSING.txt's "NEW PAYLOAD, NOT
   YET TARGET RUN ... no upload was performed as part of this transfer" is superseded
   by this console run; it was accurate at handoff time. Ensure future readers of the
   frozen handoff bundle do not treat it as current (CURRENT-BUILD.md and the
   checkpoint already supersede it).

## Honest boundaries of this review

Offline only: the controller's console-side attestations (upload response OK, FTP
readback hash/bytes, catalog entry, dispatch response OK) are internally consistent
and corroborated by the locally-held result log containing the 4 new 6d295f56 records,
but they were not independently re-fetched and cannot be, under the no-network rule.
All local hashes, records, receipts and doc claims were independently recomputed or
re-derived from raw records.
