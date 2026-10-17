# VERDICT — worker-mutex-fix-review (2026-10-04)

**NOT ALL CLEAR.** One high-severity gap (MFR-1) reintroduces the exact deadlock the fix
was meant to close. Everything else reviewed (combined_close_handle, bind interception,
profile guards, bind order, controller deviations) verifies correct.

## MFR-1 (HIGH, blocks mutex host validation): abandon_open is never retired — 6/7 tokens

The applied `tests/worker-mutex.h` keeps six tokens (`:51,56,58,60,64,76`) but never calls
`mb_worker_mutex_keep` on `abandon_open` (`:82`), so the retirement loop (`:91-92`) closes
only 6 of the 7 tokens. The deadlock worker's own validated composition retired **7/7**:

- `repro_fixedflow.c:181` — `uint64_t ao=mb_win_mutex_open_named(...);keep(ao);`
- `ff_1.log` — `retired 7/7 mutex handle tokens` → `mutex close -> 0 (OK)` → `REPRO COMPLETED clean`
- `REPORT.md:23,43,46` — the seven tokens are enumerated and the green runs retired all of them

With `abandon_open` left open, `mb_win_mutex_close` returns BUSY forever
(`mb_win_mutex.c:229-231` refuses while `s->handles` is non-empty), `mb_owned_dll_cleanup`
fails stage 40 (`owned_runtime.c:401-402`), and the driver's unbounded
`while(mb_owned_ps5_cleanup()) sleep(1)` (`worker-mutex-deadlock REPORT.md:26`) retries
forever — the originally recorded hang.

**Prediction for the running rebuild:** with `--mutex`, the fixture passes but teardown
wedges at stage 40; expect the 15-minute watchdog to kill it. Not a green receipt.

**Minimal fix (one line):** in `finish()`, after `worker-mutex.h:82`, add
`mb_worker_mutex_keep(mb_worker_mutex_state.abandon_open);`
Cap of 8 still holds (7 tokens/run). Then re-run host validation. Note: the gap is
proposal-original (`proposed_worker-mutex.h` and the diff omit it, contradicting the
worker's own REPORT + validated harness) — applied faithfully by the controller.

## Verified correct

- **MFR-3 combined_close_handle** (`owned_runtime.c:72-81`): dual guard, ms_abi, workers-first
  (`:78`, semantics identical to the old workers bind per `mb_win_workers.c:168-172` returning
  0 for foreign tokens), mutex fallback with lossless pointer→uint64 token (`:79`), BOOL-correct
  returns, last-error restored on success (`mb_win_mutex.c:435-450`) — no pollution from the
  workers probe.
- **MFR-4 interception** (`owned_runtime.c:160-166`): exact 11-byte "CloseHandle" compare,
  ordinal/name.data guards, placed BEFORE workers bind (`:168`) and mutex bind (`:175`).
  Workers-only and no-mutex profiles compile it out entirely — unchanged behavior.
- **MFR-5 controller deviations**: proposal's `*out` would not compile (callback signature is
  `uint64_t *address`, `owned_runtime.c:150`); applied `*address` + null-check, dropped
  module-state guard — necessary corrections, minimal residual risk.

## Secondary

- **MFR-2 (medium):** retirement runs only on the fully-successful collect path; prepare
  failures hit `finish()`'s `-72` early-return before the loop (`worker-fixture.c:110-113`),
  collect failures `-12..-16` return before `:174-175` — kept tokens leak, same BUSY hang in
  failure scenarios.
- **MFR-6 (low):** stale comments — `worker-mutex.h:14-15` still says "No CloseHandle on mutex
  tokens" (contradicted by `:46`/`:91-92`); `owned_runtime.c:172-174` "combined is_handle …
  future composition" is still literally true for is_handle but reads misleadingly next to the
  new combined close.

## Does not block

Workers-only profiles, no-mutex profiles, and the non-blocking game path (handoff item 3
scopes mutex work as non-blocking). The v3 probe path is unaffected.
