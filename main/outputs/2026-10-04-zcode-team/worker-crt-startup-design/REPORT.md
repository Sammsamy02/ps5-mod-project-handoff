# REPORT — worker-crt-startup-design (2026-10-04)

Deliverable complete with one honestly-marked missing input. All writes confined
to `outputs/2026-10-04-zcode-team/worker-crt-startup-design/`. Tool calls used:
~20 of 35. No network, no console (game-console) targeting, no nested agents, no
engine or build.py changes.

## What was delivered

1. **DESIGN.md** — documented MSVC DLL startup sequence (PE entry
   `_DllMainCRTStartup` → CRT attach body → user `DllMain`), `_initterm` /
   `_initterm_e` algorithms, the xi/xc/xe/xp/xt sentinel-terminated table roles,
   the `_seh_filter_dll` contract (EXECUTE_HANDLER once during the init window,
   CONTINUE_SEARCH otherwise, never CONTINUE_EXECUTION), the detach ordering
   (xp → xt → atexit LIFO via `_cexit`), loader-lock constraints, mapped onto
   the retained-runtime model with attach-time vs lazy-first-call admission
   rules, composition map to the entry adapter / workers admission owners, and
   the 10-point later-wiring list. Scope: NOT wired into build.py.
2. **crt_startup.h / crt_startup.c** — host-portable C11 reference prototype:
   both walkers (NULL-skip blind sweep; errno-style stop-on-first-error), the
   SEH-filter decision state machine, fixed-capacity LIFO atexit table, and
   composed attach/detach orchestrators implementing the documented ordering and
   the failed-init-no-cleanup contract.
3. **test_crt_startup.c** — 7 suites over synthetic function-pointer tables with
   interior NULLs and terminator sentinel slots: walker order/NULL-skip/sentinel
   safety, `_initterm_e` error-stop, filter decision matrix, atexit
   LIFO/cap/reset/reuse, detach policy polarities, and full success + mid-init-
   failure attach/detach scenarios.
4. **build_and_test.sh** — reproducible build+run (`cc -std=c11 -Wall -Wextra
   -Werror -pedantic`).

## Verification transcript (actual run, this Mac, clang via cc)

```
PASS _initterm order/null-skip/sentinel
PASS _initterm_e error channel + stop
PASS _seh_filter_dll decision matrix
PASS atexit LIFO/cap/reset
PASS detach cleanup policy
PASS attach/detach success scenario
PASS attach failure scenario (cleanup skipped)
ALL TESTS PASSED (7/7 suites)
EXIT=0
```

Two iterations were needed before green: a `-Werror` function-pointer type error
in a test initializer, then a wrong expected log in the success-scenario assert
(the prototype was right — detach logs pre-term, terminator, then atexit LIFO).

## Honest boundaries (no MSVC parity claimed beyond documented semantics)

- **Census missing**: `worker-dll-mod-retention/baseline-dll-census.json` and the
  controller handoff `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`
  do not exist in this workspace (same gap the sibling e2e-console-staging
  worker documented in its BLOCKED.md). The exact observed symbol set was NOT
  fabricated; DESIGN.md 2.2 is a PENDING table with only the prompt-named
  symbols, plus a mechanical fill-in recipe in CENSUS-GAP.md.
- Owners named for composition (entry adapter, workers admission) were not found
  as source in this workspace; section 3 binds to their interfaces as specified
  in the task brief and is marked not-integrated.
- Semantics were transcribed from the documented UCRT source layout without
  network access; line-level ordering details (notably xp→xt→`_cexit` and
  `_cexit` table-reuse) are flagged for re-verification against vendored CRT
  sources before any parity claim. Full divergence list: DESIGN.md section 7
  (no real SEH/unwinding — modeled mid-walk faults do not stop the current
  table's remaining initializers, unlike real SEH; fixed atexit cap; no TLS
  callbacks; no detach-reason matrix).

## Unblocked by

1. Controller delivering `ZCODE-FORK-HANDOFF.md` + the census under
   `worker-dll-mod-retention/` (or wherever the retained-DLL artifacts live).
2. The entry-adapter owner landing its interface; then wiring-list items 1–10
   (DESIGN.md section 5) apply in order, with build.py touched only at step 10.
