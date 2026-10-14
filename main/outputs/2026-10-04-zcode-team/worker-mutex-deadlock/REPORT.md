# worker-mutex-deadlock: mutex-integration host hang

Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what` (read-only inputs; all builds/artifacts under this worker dir)
Failure under investigation: `build.py --host-only --entry --clock --workers --sync --init-once --mutex` -> `test-sdk-driver normal` blocks forever; recorded as "process in uninterruptible wait during exit; killed". Suspect was the mutex owner's pthread-key abandonment destructor vs `mb_win_workers` exit/join.

## 1. Reproduction (alarm-guarded, vendored harness)

`repro.c` compiles the UNMODIFIED windows-services owners (`mb_win_threads.c`, `mb_win_workers.c`, `mb_win_critical.c`, `mb_win_address_wait.c`, `mb_win_srw.c`, `mb_win_init_once.c`, `mb_win_mutex.c`) with a native harness that replicates the driver call sequence exactly: owners open -> main admitted -> sync/initonce/mutex prepare (line-for-line translation of `worker-sync.h`/`worker-initonce.h`/`worker-mutex.h` creator sides) -> `enter_call` -> CreateThread -> bounded SRW ready handshake -> WorkerCollect equivalent (gate unblock, WaitForSingleObject(thread, INFINITE), exit code, CloseHandle, sync/initonce/mutex finish) -> `leave_call` -> workers collect (pthread_join) -> driver cleanup order (`workers/srw/address_wait/critical/initonce/mutex/threads` close). `alarm(40)` guard on every run; all subprocesses externally killed after a bound.

Result (3/3 runs, `orig_1..3.log`): the composed flow runs CLEAN through the entire suspected deadlock zone and then fails exactly one step:

```
[worker] returning 81234567 (thread exit + TSD destructors follow)
[main] thread signaled; exit code + CloseHandle
[main] abandon wait -> 00000080 (expect 80)          <- WAIT_ABANDONED delivered
[main] join complete; driver cleanup order            <- pthread_join after TSD destructor: fine
[main] mutex close -> 3 (BUSY): leaked fixture handle tokens make it unretirable
[main] FAILED: threads close                          <- cascade: mutex owner keeps its domain pin
```

## 2. Root cause

`tests/worker-mutex.h` deliberately never retires a single mutex handle token ("No CloseHandle on mutex tokens in this fixture (workers family owns that import today)"): `unnamed`, `named`, `opened`, the second folded open, `exists`, `abandon_open`, plus the worker's abandon handle. `mb_win_mutex_close` is documented and correct to refuse service while handles remain, so it returns `MB_WIN_MUTEX_BUSY` forever. Consequences up the stack:

1. `service_end`/`mb_win_mutex_end_session` succeeds (pin released), but `mb_owned_dll_cleanup` fails at stage 40 (`mb_win_mutex_close` BUSY) every iteration.
2. `sdk_driver.c` `mb_owned_ps5_execute`: `while(mb_owned_ps5_cleanup()){ ...; sleep(1); }` retries forever -> the driver never retires -> "blocks forever during exit". This is the hang.
3. In the host test harnesses (`-Dsleep=mb_test_sleep`), the bounded wrapper aborts after its sleep budget; that abort under Rosetta wedges as an unkillable uninterruptible wait (below), which is why the operator saw a U-state process that had to be killed.

Amplifier (why "uninterruptible, unkillable"): during early instrumented runs I repeatedly triggered aborts in this Rosetta (x86_64) process; `abort -> pthread_kill` never completed - the process sat in state `UE` (uninterruptible, exiting) for many minutes, survived `kill -9`, froze the concurrently-running worker thread mid-malloc/fprintf, blocked SIGALRM delivery (the alarm guard never fired), and `lldb` attach hung. Any fail-stop abort in this composed Rosetta flow turns into exactly the recorded symptom.

Exoneration of the four hypotheses (evidence):
- (1) destructor vs join lock cycle: none. `mb_win_workers_collect` unlocks before `pthread_join`; the worker's mutex-key destructor only takes the mutex owner's state lock, which nobody holds indefinitely (all waits are cond-based). Join completed in 3/3 corrected runs.
- (2) destructor firing after the workers domain retires the thread: harmless by design (`thread_exited` uses no thread-domain state; `mb_win_threads_leave` NULLs the domain key before the worker returns, so only the mutex key destructor runs at pthread exit). Abandonment + transfer + reacquire verified 0x80 in composition.
- (3) last-error domain pin: not implicated; the destructor never reads last-error, and worker last-error isolation (`e17395ab`) survived the whole flow.
- (4) fixture wait-semantics regression: the SRW/intonce/address-wait sequence passes 3/3. (Methodology note: my first harness generation accidentally acquired the SRW shared but slept exclusive, which trips `mb_win_srw_sleep`'s documented fail-stop ownership abort and produced dramatic wedges; that was a harness bug, not the shipped fixture's shape. `mb_win_srw_sleep`'s fail-stop on re-entry after a peer's full acquire/release cycle retires the sidecar is a genuine sharp edge worth a future ticket, but it is not triggered by this composition.)

`verify_mutex.py` standalone tests passed because they never compose the fixture handle leak with `mb_win_mutex_close` + the driver's infinite cleanup retry.

## 3. Fix (proposed; originals untouched)

No behavioral change to `mb_win_mutex.c` is required or wanted (its BUSY refusal and abandonment semantics are correct and are preserved). The fix is the integration composition that `mb_win_mutex.h` itself anticipates ("CloseHandle participation is NOT bound by this family... a combined is_handle dispatch remains a documented future composition"):

- `worker-mutex.h.diff` (fixture): record every created/opened token (`mb_worker_mutex_keep`) and CloseHandle all of them at the end of `mb_worker_mutex_finish` (new failure code -78 if a retirement fails).
- `owned_runtime.c.diff` (runtime): combined `CloseHandle` dispatch bound ahead of the workers bind for that name: workers `close_handle` first (thread handles), mutex `close_handle` as fallback (mutex tokens).

Fix-mechanism verification: `repro_fixedflow.c` simulates the fixed fixture by retiring all seven tokens via `mb_win_mutex_close_handle` immediately before `mb_win_mutex_close`: 3/3 runs exit 0 with the full teardown green (`mutex close OK`, `threads close OK`, `REPRO COMPLETED clean`). Abandonment semantics (WAIT_ABANDONED + transfer + reacquire, ERROR_ALREADY_EXISTS, NOT_OWNER 288, alertable 50, backslash 87) re-verified against the unmodified owner in the same runs.

## 4. Remaining risk

- The PE-side diffs are proposals; they still need the controller's real `build.py` pass (import-count assertions change: the consumer gains no new import names; only binding order changes for `CloseHandle`).
- `sdk_driver.c`'s unbounded `while(cleanup) sleep(1)` remains a liveness hazard for any permanently-failing cleanup stage; the host harnesses bound it, the real target-side driver does not.
- The Rosetta abort-wedge (unkillable UE state) affects any future fail-stop abort in this flow; consider fail-code returns instead of abort() on host-only paths.
- Artifacts: `repro*.c`, `mb_win_srw_trace*.c` (debug copies), `repro.asm`, `sample*.txt`, `run*.log`, `fixed_*.log`, `ff_*.log`, `orig_*.log`, `worker-mutex.h.diff`, `owned_runtime.c.diff`, `proposed_worker-mutex.h`, `proposed_owned_runtime.c`.
