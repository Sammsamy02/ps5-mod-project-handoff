# REPORT — worker-initonce-review (2026-10-04)

Independent, evidence-required review of the controller's InitOnce integration.
All workspace paths below are under
`/WORKSPACE/.codex/worktrees/translation-core/what/`.
Every statement cites file:line from direct reads. Tool budget: 11 calls used
of 35. Read-only outside this output directory.

## 1. Opt-in isolation (MB_OWNED_INIT_ONCE=0) — PASS

Guard pairs, all balanced `#if`/`#endif`, all removing exactly the new code:

- `outputs/ps5-mod-bridge/windows-candidate/owned_runtime.c`
  - 19-21 include of `mb_win_init_once_abi.h`
  - 39-41 `init_once` session field in `struct service`
  - 52-54 owner + `init_once_live` in `struct owned`
  - 138-140 `mb_win_init_once_bind` in `service_bind`
  - 153-155 `mb_win_init_once_end_session` in `service_end`
  - 196-198 `mb_win_init_once_begin_session` in `service_begin`
  - 232-235 owner open with fail stage 37
  - 353-356 owner close with fail stage 38
- `outputs/ps5-mod-bridge/windows-candidate/tests/worker-fixture.c`
  - 11-13 default 0; 30-32 include; 48-50 thread_begin; 66-68
    thread_complete; 97-100 prepare; 158-160 finish.
  - All pre-existing sync blocks verified unmodified (45-47, 53-65, 77-81,
    93-96, 105-107, 112-114, 133-137, 155-157).

Unguarded edits, verified inert when the flag is 0:

- `owned_runtime.c:212` `owned.report.init_once_enabled=MB_OWNED_INIT_ONCE;`
  writes 0. The field is new (`owned_runtime.h:30`, appended after
  `sync_enabled`), and JSON emission is guarded at
  `result_report.c:51` (`if(r->init_once_enabled)`), so the prior report
  output is bit-identical with the flag off.
- `owned_runtime.c:274`, `test_entry_candidate.c:58`,
  `test_worker_candidate_failures.c:112` add `+(MB_OWNED_INIT_ONCE?2u:0u)`,
  which is +0 when off (value-identical expression).
- `test_entry_candidate.c:60` / `test_worker_candidate_failures.c:114`
  compare the field against the same macro (0==0).
- `owned_runtime.c:24` IMPORT_CAPACITY is unchanged (no init-once term);
  `--init-once` requires `--sync`, so capacity 64 applies and 46 bound
  imports fit.
- `build.py`: every init_once branch is conditional (39, 55-56, 66, 78, 96,
  102, 108, 169-170). `owned_profile.h` always defines the macro (line 44).
  The census entry `'mb_win_init_once_abi':2` (line 209) is consulted only
  for sources actually compiled; `mb_win_init_once_abi.c` is not in `abi_c`
  when the flag is off (line 56 conditional).

## 2. Pattern fidelity vs the sync slice — PASS

- Session begin order (`owned_runtime.c:180-197`): heap, tls, loader, clock,
  worker, critical, address_wait, srw, init_once. `service_end` (151-173) is
  the exact reverse: init_once, srw, address_wait, critical, worker, clock,
  loader, tls, heap. Init-once begins last / ends first — mirrored.
- Owner open (`owned_runtime.c:225-234`): critical(31), address_wait(32),
  srw(33), init_once(37). Config `{&owned.domain,16}` (233) mirrors the srw
  config shape `{domain, capacity}` (`mb_win_init_once.h:16`).
- Owner close (`owned_runtime.c:345-356`): srw(34), address_wait(35),
  critical(36), init_once(38). Init-once closes AFTER the sync owners, per
  the stated design. Dependency-safe: init_once pins only the thread domain
  (`mb_win_init_once.c:84` open, `:103` close) and the domain closes later
  at stage 16 (`owned_runtime.c:363`). Nuance: not strict LIFO across all
  service owners (init_once opened last, closes last among them); the sync
  triple itself remains strict LIFO. No dependency runs between the sibling
  services, so both orders are safe; the implemented order is the specified
  one.
- Bind chain (`owned_runtime.c:134-148`): init_once bind (139) sits after
  all three sync binds and before workers/clock — consistent with the
  sync-slice chain structure.
- Fail-stage uniqueness: `fail(37` occurs only at `owned_runtime.c:234` and
  `fail(38` only at `:355`; a grep across the entire candidate tree finds no
  other reference to stages 37/38. The native-status propagation formula
  `(io==MB_WIN_INIT_ONCE_NATIVE_ERROR&&native?native:(int)io)` mirrors sync
  stages 34-36.
- Begin-failure unwind: `owned_runtime.c:197` mirrors sync (192-194). A
  failed `mb_win_init_once_begin_session` leaves the session inactive
  (`mb_win_init_once_abi.c:12-18` sets `active` only on success), so the
  `s->init_once.active`-guarded end at `owned_runtime.c:154` is skipped —
  matching the sync sessions' `if(s->srw.active && ...)` pattern.

## 3. Fixture semantics — PASS

`tests/worker-initonce.h`:

- Asserts only documented behavior: pending=1 grant on begin (31, 47);
  complete stores context with CHECK_ONLY read-back (32, 34, 54-55, 62);
  CHECK_ONLY on a never-begun object fails with GetLastError 997 (37);
  unknown flag bit 0x8 fails with 87 (39). The header comment (3-10) states
  exactly this contract.
- Duplicate-complete (35) is asserted as failure only
  (`if(InitOnceComplete(...))return -48;`) with no GetLastError pin. The only
  pinned codes are 997 and 87 — both documented by Microsoft and by the
  adapter header (`mb_win_init_once.h:26, 48, 57-58`).
- LastError contract: the adapter saves and restores last_error on every
  successful path (`mb_win_init_once.c:119/127/134/147/153/166/170` in
  begin; saved/restored in complete). Hook placements in
  `tests/worker-fixture.c`:
  - thread_begin (48) runs between `SetLastError` (44) and the worker_error
    read (61), but only issues successful calls — the same convention
    already relied on by the sync calls in that window (EnterCriticalSection
    at 56, etc.).
  - thread_complete (67) runs after the worker_error read (61).
  - finish (159) runs AFTER the `-18` GetLastError check (154) and after
    sync_finish (156) — the exact reverse of prepare order (sync_prepare 94
    then initonce_prepare 98). Prepare's failure unwind (99) releases both
    prepared services in reverse.

Non-defect observation: the CreateThread-failure path
(`worker-fixture.c:104-108`) calls `mb_worker_sync_finish` but not
`mb_worker_initonce_finish`. Harmless: if the thread never started, once_b
was never begun (prepare's CHECK_ONLY passes create=0 —
`mb_win_init_once.c:121`), once_a's sidecar is COMPLETE, and its re-zeroed
public storage (`worker-initonce.h:41`) is never touched again (lookup
aborts on token mismatch — `mb_win_init_once.c:42` — but no further call is
made), so owner close (which requires every sidecar COMPLETE and quiet,
`mb_win_init_once.c:91-92`) still succeeds. This path is unreachable in the
asserted profiles and mirrors the sync slice, which also skips finish in
`mb_worker_fixture_fail` paths.

## 4. Builder invariants — PASS

- Census: `build.py:209` `'mb_win_init_once_abi':2` equals the two ms_abi
  (WIN64) definitions in `mb_win_init_once_abi.c:27-28`
  (`begin_initialize`, `complete_initialize`) and the def file's two exports
  `InitOnceBeginInitialize`/`InitOnceComplete`
  (`tests/worker-initonce-kernel.def:3-4`); bind matches the same two names
  (`mb_win_init_once_abi.c:35-36`).
- Receipt formulas: consumer 46 = 18(clock)+5(workers)+21(sync)+2(init-once),
  helper 10 (`owned_runtime.c:274`, `build.py:78`
  `actual_pe_imports:2` = def count). Sync-only 44/10 is directly evidenced
  by the 6d295f56 group's final record in the console log (below).
- Guard chain mirrors sync: `--init-once requires --sync` (`build.py:39`)
  chains onto `--sync requires --workers` (38) and `--workers requires
  --entry` (37). Fixture compile receives `-DMB_OWNED_INIT_ONCE` (102);
  `initonce.lib` is built from the def (96) and linked into the consumer
  only (108); the new headers and def are in the hashed source list (66).
- Assertions: require `init_once=={'enabled':1}` when on (169); forbid the
  key when off (170) — consistent with `result_report.c:51`.

## 5. Evidence cross-check — PASS

`outputs/2026-10-04-zcode-controller/console/`:

- `1a6b37b7-console-result-2026-10-04.jsonl` sha256
  `4f68182b50c4193ea165db207ec92fa8b1448adb35883b3438a5f397ebbe4287`
  matches receipt line 24 and the file on disk.
- The copy is an accumulated console log: 16 records = 4 builds x 4 records.
  The 1a6b37b7 slice is the final four, phases [1,2,3,5] — matching
  `build.py:146`'s normal-entry expectation. (The briefing's "4 records"
  describes this build's slice; the receipt's history_preserved section
  itself records the prior builds.)
- Group progression in the same log: db1c40e6 10/6 (default), f72faea5
  18/10 (clock), 6d295f56 44/10 + synchronization (sync-only), 1a6b37b7
  46/10 + synchronization + init_once — the +2 delta is exactly the two
  new imports.
- Final 1a6b37b7 record vs fixture assertions and receipt `final_record`
  (all consistent): init_once {enabled:1}; worker 2/2 with begin
  [5,1,258,259] and result [0x81234567, 0x1122334455667788, 0xe17395ab, 0]
  (= EXPECTED_EXIT, PARAMETER_COOKIE, LAST_ERROR, repeat-wait 0);
  dll_calls 3/3; results [86,93]; counts 46/10; cleanup
  {pending:0, retired:1}; first_failure all zeros; delivery file errors 0,
  stdout error 32 (EPIPE — the console has no stdout reader; file delivery
  is the evidence channel and the receipt labels it
  "stdout_error_delivery_only").
- The phase-1 record of every build lacks the feature keys because it
  snapshots the zeroed report before `mb_owned_dll_open` populates the
  enabled flags — identical in the 6d295f56 sync build, i.e. established
  phase-1 semantics, not an init-once regression.
- Scope: receipt line 59 explicitly limits claims to the standalone owned
  SDK ELF diagnostic ("not UE4SS/game integration, not arbitrary DLL
  support"); `game_files_modified` and `game_launched` are false; the
  correction_note documents the superseded c49108fe build as a distinct
  implementation (no unchanged-diagnostic repeat). No target-support
  over-claim found.

## 6. Promotion fidelity — PASS

diff of `outputs/ps5-mod-bridge/windows-services/` against
`outputs/2026-10-04-zcode-team/worker-4-win-services/sources/`:
`mb_win_init_once.h`, `mb_win_init_once.c`, `mb_win_init_once_abi.c`
byte-identical; `mb_win_init_once_abi.h` differs only at line 6
(`#include "pe_module.h"` -> `#include "../windows-execution/pe_module.h"`)
— exactly the single expected change.

## Verdict

PASS — all-clear with citations; no actionable mismatches. The three
recorded observations (result-copy record count characterization,
CreateThread-failure finish asymmetry, owner-level non-LIFO close order) are
documented above as non-defects with supporting evidence.
