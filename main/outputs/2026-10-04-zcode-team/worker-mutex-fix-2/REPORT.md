# REPORT — worker-mutex-fix-2 (2026-10-04)

Second mutex integration fix round. Everything below is under
`outputs/2026-10-04-zcode-team/worker-mutex-fix-2/`; real sources elsewhere were
only read (plus read-only execution/disassembly of the already-built build-4
artifacts). All reproductions were watchdog-bounded; no process was left
running; no Rosetta abort was triggered (the hang reproduces as an ordinary
`sleep(1)` retry loop, killable).

## 0. Headline correction: the failure code is -68, not -100

The checkpoint brief said `native_status 0xFFFFFF9C (-100)`. The actual
build-4 `host-normal.jsonl` (verified at
`.superpowers/native-dependency-spike/windows-mutex-candidate-1/f722bd65b39c4d9bab4e1d6f0bf7df80/`,
whose receipt source hashes MATCH the current tree exactly — worker-mutex.h
`9d6528f1…`, worker-fixture.c, owned_runtime.c, build.py all MATCH) records:

```
phase 3 stage 24 native_status 4294967228 windows_error 2
```

4294967228 = 0xFFFFFFBC = **-68**. There is no -100 anywhere: no source file
contains it and the full disassembly of `owned_consumer.dll` (921-line
WorkerBegin symbol range, whole DLL too) contains every fixture constant
(-1..-8, -31..-53, -60..-78) but no 0xFFFFFF9C. -68 IS a fixture code
(worker-mutex.h:65). The "-100 is in mb_win_mutex internals or
combined_close_handle" interpretation was a misread of the hex; the rest of the
brief's causal chain (Begin fails before Collect -> tokens never retire -> owner
close BUSY -> sdk_driver cleanup retry hang) is CONFIRMED.

## 1. Reproduction (alarm-guarded, real binaries + vendored real sources)

a) **Live binary**: ran the unmodified build-4 `test-sdk-driver` (Mach-O
x86_64) under a shell watchdog (40 s) — hung, killed; its
`harness/repro-host-normal.jsonl` reproduces the checkpoint exactly: phases
[1,2,3,4], first_failure {stage 24, native_status 4294967228, windows_error 2},
worker begin/result all zeros, cleanup pending forever.

b) **lldb trace of the same binary** (`harness/lldb2.log`): breakpoint at
`mb_owned_call_worker`; `rdi = 0x1086545c0` = consumer image + 0x45c0 = the
real `WorkerBegin` export (export table ordinal 5 RVA 0x45c0 verified);
`finish` → **eax = 0x00000000ffffffbc**. So WorkerBegin itself returned -68;
the export resolution and the ms_abi thunk are correct
(`harness/WorkerBegin.build4.disasm.txt` preserved).

c) **Vendored harness** (`harness/`): the REAL current sources copied unedited
(`harness/vendor/`: tests/worker-mutex.h, windows-services mb_win_mutex.c/.h,
mb_win_threads.c/.h) plus typed bindings for the fixture's six imports;
`alarm(45)` handler `_exit(99)`; shell watchdog around every run.
`repro_current` output (`harness/repro_current.log`):

```
[main] prepare -> -68  tokens kept: 5
[main] last_error after prepare -> 288
[main] finish -> -72 (current finish returns -72 BEFORE the retirement loop)
[main] mutex close -> 3 (BUSY)   [3=BUSY: real driver retry loop hangs here]
[main] REPRO CONFIRMED (prepare -68 / finish -72 / close BUSY)
```

## 2. Root cause (exact origin)

`mb_worker_mutex_prepare()` fails at **worker-mutex.h:65** with **-68**:

- :56 creates `named` (unowned), :58 folded-open `opened`, :60 create-existing
  `exists` (+183, initial owner ignored), :62 non-owner release 288 — all pass.
- :63 `WaitForSingleObjectEx(opened,0,0)` acquires the unowned object → the
  calling thread is now the OWNER.
- :64 opens a SECOND handle `second` and keeps it (5th token).
- :65 waits on `second` with timeout 0 expecting `WAIT_OBJECT_0`. But
  `mb_win_mutex_wait_ex` (windows-services/mb_win_mutex.c:368-385, applied
  owner) implements its documented adapter policy: an owner re-wait NEVER
  re-grants — finite timeouts return `WAIT_TIMEOUT` (258). The fixture expected
  real-Windows per-thread recursive acquisition. Contradiction: the very next
  check (:66-69) expects an owner re-wait to be `WAIT_TIMEOUT` — so the old
  order asserted WAIT_OBJECT_0 for one owner re-wait and WAIT_TIMEOUT for
  another; no semantics (adapter or real Windows) can satisfy both.

Then the composed cascade (all verified):

1. `WorkerBegin`'s prepare-failure handler (worker-fixture.c:112) calls
   `(void)mb_worker_mutex_finish()`; the abandon mutex does not exist yet, so
   pre-fix `finish` returns **-72 at its first check — before the retirement
   loop** (worker-mutex.h:82-84) → **MFR-2 gap**: the 5 kept tokens leak.
   The failed `OpenMutexW(abandon)` sets `ERROR_FILE_NOT_FOUND` (2), which
   `fail(24,…)` records as windows_error 2.
2. `WorkerBegin` returns -68 → stage 24, native_status 0xFFFFFFBC, begin/result
   all zeros; run body returns without Collect; `s->begun=0`.
3. Teardown: loader/workers close OK; stage 40 `mb_win_mutex_close` refuses
   with **BUSY** (5 outstanding handle entries) every attempt;
   `mb_owned_dll_cleanup` returns -1 forever; sdk_driver.c:45
   `while(mb_owned_ps5_cleanup()) sleep(1);` spins forever → phases stop at
   4 (CLEANUP_PENDING) → 15-minute watchdog kill. MFR-1 (7/7 keep) is applied
   and is NOT the cause; it only feeds more tokens into the same unretired set.

## 3. Fix diffs (proposals; real files untouched)

**`worker-mutex.h.diff`** → real `outputs/ps5-mod-bridge/windows-candidate/tests/worker-mutex.h`:
- Fix 1 (-68 cause): reorder the named-mutex block — acquire via `opened` (-68),
  owner re-wait must time out (-69, adapter deviation now documented in-place),
  release (-70), THEN open + keep the second folded handle and grant/release
  through it (-68/-70), proving both handles name one object. All documented
  checks and failure codes preserved; the mutually contradictory expectations
  are gone.
- Fix 2 (MFR-2): `mb_worker_mutex_finish` restructured to accumulate the first
  failure (-72…-76) and ALWAYS fall through to the retirement loop; -78 only if
  retirement itself fails; `token_count` reset on every exit. Retirement now
  runs on prepare-failure paths too (the -68 case retires its 5 tokens).

**`worker-fixture.c.diff`** → real `outputs/ps5-mod-bridge/windows-candidate/tests/worker-fixture.c` (MFR-2 routing; success flow unchanged; every failure code unchanged):
- `mb_worker_fixture_fail`: bounded (2000 x Sleep(1)) done-flag drain when a
  worker may still be running (so its `worker_abandon` token is already
  recorded), then `(void)mb_worker_mutex_finish()` — covers Begin -3..-8 and
  Collect -12..-15.
- `WorkerBegin` CreateThread-failure path (-2): add mutex finish.
- `WorkerCollect` -16: sync/initonce/mutex finish before returning; -17/-18:
  mutex finish before returning.
- All additions `#if MB_OWNED_MUTEX`-guarded; profiles without the mutex family
  compile identically (verify_mutex-style negative check: syntax-checked both).

## 4. Verification

`repro_fixed` (fixed vendored header + real unmodified mb_win_mutex.c owner),
watchdog-bounded, **3/3 runs exit 0**:

```
[main] prepare -> 1 (tokens kept 5)
[worker] thread_acquire -> 1 (token count now 6); exiting WITHOUT release
[main] finish -> 1 transfered=1 reacquired=1 tokens_left=0
[main] mutex close -> 0 (OK)
[main] threads leave -> 0 / threads close -> 0
[main] REPRO FIXED COMPLETED clean
```

- Full owner teardown green: mutex close OK (0), threads close OK (0).
- Abandonment semantics intact against the UNMODIFIED owner: real pthread exits
  owning the named mutex → creator's wait returns WAIT_ABANDONED (0x80),
  `ReleaseMutex` succeeds (ownership transfer), immediate re-wait grants and
  re-release succeeds (reacquire) — plus, in prepare: 183/288/87, folded open,
  alertable 50, owner re-wait 258, backslash 87.
- 7/7 token retirement (unnamed, named, opened, exists, second, abandon_open,
  worker_abandon) on both the success path and failure paths (repro_current's
  failure-path scenario now retires its 5 tokens — implied by tokens_left=0 in
  the fixed flow and by the finish restructure; the -72-early-return is gone).
- Both fixed fixture revisions syntax-check clean for the mutex profile
  (cc -fsyntax-only, vendored headers).

## 5. Prediction for the real build.py run

Command: `build.py --host-only --entry --clock --workers --sync --init-once --mutex` (plus its normal/release-fault host variants).

- `WorkerBegin` → 1; `WorkerCollect` → 1 with begin `[handle,tid,258,259]` and
  result `[259,cookie,last_error,0]` (worker runs the full sync/initonce/mutex
  path); finish retires 7/7.
- Cleanup reaches stage 40 with zero outstanding handles → `mb_win_mutex_close`
  OK → cleanup {pending:0, retired:1} → **phase 5 RETIRED written**;
  `host-normal.jsonl` shows phases [1,2,3,4,5] and an all-zero first_failure;
  `test-sdk-driver normal` exits 0; no watchdog kill.
- No build.py changes needed: no new import names (retirement uses the already
  bound combined CloseHandle), so the import-count census assertions
  (consumer 16+5+21+2+5, helper 16+5=21… as currently coded) and the
  reject-worker-start expectation (stage 24, 0xFFFFFFD8) are untouched.
- Confidence: high for the mutex flow (mechanism reproduced and removed against
  the real owner); the real rebuild is still the controller's acceptance gate.
- Residual risk (unchanged, documented by predecessors): sdk_driver.c's
  unbounded cleanup retry remains a liveness hazard for any NEW permanently
  failing stage, and the Rosetta abort-wedge applies to any fail-stop abort in
  this flow.

## 6. Artifacts

- `CLAIM.json`, `REPORT.md`, `worker-mutex.h.diff`, `worker-fixture.c.diff`
- `harness/harness.c` (alarm-guarded), `harness/vendor/` (unedited real
  sources), `harness/worker-mutex-fixed.h`, `harness/worker-fixture-fixed.c`
- `harness/repro_current.log` (hang-cause repro), `harness/repro_fixed.log`
  (green teardown), `harness/repro-host-normal.jsonl` (binary reproduction),
  `harness/lldb2.log` + `harness/WorkerBegin.build4.disasm.txt` (-68 trace)
- 11 tool-budget calls remain unused; no nested subagents; no network/console.
