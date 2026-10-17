# REPORT — worker-mutex-fix-review (2026-10-04)

Independent static review of the controller-applied mutex fixes. No build, no run, no
console contact; 14 tool calls (bound 30). Deliverables: CLAIM.json, findings.json,
VERDICT.md, this report. Verdict: **NOT ALL CLEAR** (one high gap).

## 1. Fixture: applied vs proposal (worker-mutex.h, worker-fixture.c)

The applied `outputs/ps5-mod-bridge/windows-candidate/tests/worker-mutex.h` is the proposal
(`proposed_worker-mutex.h` / `worker-mutex.h.diff`) applied cleanly — token table
(`:36-37`), keep() with cap 8 (`:47-48`), CloseHandle dllimport (`:46`), second-open keep
(`:64-65`), worker-side token (`:74-78`), retirement loop + `-78` (`:91-93`) are all present
and match the diff line-for-line.

**Gap (MFR-1, HIGH):** `abandon_open` is opened at `:82` without `mb_worker_mutex_keep`,
so only **6 of the 7** tokens are retired. The deadlock worker's validated fix retired
**7/7** (`repro_fixedflow.c:181` keeps it; `ff_1.log`: `retired 7/7` → `mutex close -> 0 (OK)`;
REPORT.md:23,43,46 enumerate seven). Since `mb_win_mutex_close` BUSYs while any handle entry
remains (`mb_win_mutex.c:229-231`) and cleanup stage 40 (`owned_runtime.c:401-402`) feeds the
driver's unbounded retry (worker-mutex-deadlock REPORT.md:26), the applied fixture reproduces
the original hang for the `--mutex` profile. **Prediction: the running rebuild wedges in
teardown and dies at the 15-minute watchdog; not a green receipt.** Minimal fix: one keep()
line after `:82`. Note the gap is proposal-original — the proposal contradicts its own
REPORT and harness; the controller applied it faithfully.

**MFR-2 (medium):** retirement only executes on the fully-successful collect path.
Prepare failures route to `finish()` whose first step (`:82-83`) returns `-72` before the
loop (`worker-fixture.c:110-113`); collect failures `-12..-16` return before the finish at
`:174-175`. Kept tokens leak on failure runs → same stage-40 BUSY hang, and `-78` never fires.
The `:40-45` comment ("every public handle token … must be retired") overpromises.

## 2. Runtime: combined_close_handle + interception (owned_runtime.c)

**MFR-3 (verified correct):** `:72-81` — definition and use share the
`MB_OWNED_WORKERS && MB_OWNED_MUTEX` guard; `ms_abi int32_t(void*)` matches the fixture's
`mb_worker_i32 CloseHandle(void*)` (duplicate-but-compatible declarations, no new import
name → `IMPORT_CAPACITY :27` unchanged); workers-first (`:78`) returns 1 only for worker
tokens (`mb_win_workers.c:168-172` returns 0 otherwise → thread-handle behavior identical to
pre-fix); mutex fallback casts the pointer token losslessly (`:79`); close_handle returns
BOOL-clean 0/1 and restores last error on success (`mb_win_mutex.c:435-450`), so the workers
probe's transient error-6 cannot leak.

**MFR-4 (verified correct):** `:160-166` — requires `name.size==11 && !by_ordinal &&
name.data`, full byte-exact "CloseHandle" compare; block placed BEFORE `mb_win_workers_bind`
(`:168`) and `mb_win_mutex_bind` (`:175`). Workers-only / no-mutex profiles compile the
interception and combined function out entirely — provably unbroken. Mutex-without-workers
is not a supported composition (`service_bind` needs CLOCK||WORKERS, `:149`).

**MFR-5 (controller deviations = corrections):** proposal bound `*out`, but the callback
signature is `service_bind(..., uint64_t *address)` (`:150`) — the proposal would not have
compiled; applied `*address` (`:164`) is right. Dropped module-state guard + added
`name.data` null-check: minimal residual risk (combined is a behavioral superset of the
workers dispatch for thread handles).

## 3. Comments / invariants (MFR-6, low)

`worker-mutex.h:14-15` still claims "No CloseHandle on mutex tokens in this fixture" — now
false (`:46`, `:91-92`); inherited from the proposal. `owned_runtime.c:172-174` "combined
is_handle … future composition" remains literally true for is_handle but misleads next to
the new combined close. `token_count` reset without zeroing `tokens[]` (`:93`) is safe under
the cap-8 keep() invariant (≤7/run).

## 4. Remaining hang risks for the running rebuild

1. **Mutex profile (high confidence):** MFR-1 → stage-40 BUSY → infinite driver retry →
   watchdog kill at 15 min.
2. Rosetta abort-wedge (unkillable UE state) if anything fail-stops mid-teardown
   (worker-mutex-deadlock REPORT.md:27,29).
3. Workers-only / no-mutex profiles: no new risk.
4. Even with MFR-1 fixed, MFR-2 failure-path leaks and the driver's unbounded retry remain
   liveness hazards for any permanently failing stage (worker-mutex-deadlock REPORT.md:51).

## Recommended controller actions

1. Apply the one-line MFR-1 keep for `abandon_open` (worker-mutex.h after `:82`); give the
   revision a distinct ID and re-run host validation — do not re-rate unchanged evidence.
2. Optionally backport retirement coverage to failure paths (MFR-2) and refresh the two
   stale comments (MFR-6).
3. Mutex host receipt stays blocked until a validated pass; mutex compose/target run
   (handoff queue item 3) stays non-blocking for the game path.
