# worker-crt-integrate report — CRT startup family integrated into the production builder

## Result

The CRT startup family is integrated into the vendored production builder and
validated through both gates, on the exact shipped tree (no instrumentation):

- Host validation: `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`
  (`receipts/host-validation-clean/receipt.json`, build `e20ca21fe522416e88b6908a766cef38`),
  flags `--host-only --entry --clock --workers --sync --init-once --mutex --fileio --crt-startup`.
  Both driver modes (`normal` 4 records, `release` 5 records) show
  `"crt_startup":{"enabled":1}`, `consumer_imports: 68` (= 18 base+clock +5 workers
  +21 sync +2 init-once +5 mutex +10 fileio +7 crt-startup), cleanup fully retired.
- Full SDK composition: `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`
  (`receipts/sdk-composition/receipt.json`, build `538085976d31464b9e2ed61cd84f3150`),
  same flags plus `--report-path /data/modbridge-owned-dll-candidate.jsonl`
  (recorded in `result_profile`; `target_path_opened_by_build: false`).
  Artifact for console staging: `sdk-out/538085976d31464b9e2ed61cd84f3150/owned-dll-candidate.elf`,
  281040 bytes, sha256 `a6208aeaf4efd948870bb66ca969118f38e064b9107f764019306661ab60379f`,
  recorded with both receipt hashes in `staging-manifest.json`.
  All build-internal assertions passed, including the win64cc census
  (`'mb_win_crt_startup_abi': 8`), the ELF import-provider check (no undeclared
  dynamic imports; the freestanding owner places zero SDK demands), and the final
  receipt source re-hash. No target execution was attempted or claimed.

## Workspace discipline

Everything lives under `outputs/2026-10-04-zcode-team/worker-crt-integrate/`.
`CLAIM.json` first, this report last. Inputs were used read-only; the worker-fileio-apply
vendored tree and the implement worker's sources were copied into `vendor/` and
`vendor/windows-services/` (copies verified byte-identical before editing; the
one adapted line is documented below). Pinned tools were used via absolute paths
(SDK v0.43 prospero-clang, LLVM 21.1.8, pinned lld-link).

## 10-point build.py wiring (all applied to `vendor/windows-candidate/build.py`)

1. Family promoted into `vendor/windows-services/` (`mb_win_crt_startup.{h,c}`,
   `mb_win_crt_startup_abi.{h,c}`, `tests/test_win_crt_startup.c`).
2. `--crt-startup` flag + guard `--crt-startup requires --sync` (as specified).
3. `owned_profile.h` gains `#define MB_OWNED_CRT_STARTUP`.
4. `native_c += mb_win_crt_startup.c`; `abi_c += mb_win_crt_startup_abi.c` under the flag.
5. Receipt sources: both service headers + the new fixture pair.
6. `tests/worker-crtstartup-runtime.def` — `LIBRARY api-ms-win-crt-runtime-l1-1-0.dll`
   (the census-observed import-DLL name) with the 7 exports; `lld-link /lib` →
   `crtstartup.lib`, linked into the consumer DLL only.
7. Consumer compile gains `-DMB_OWNED_CRT_STARTUP=`; fixture exercise in
   `tests/worker-crtstartup.h` (below); `mb_win_entry` untouched.
8. `crt_startup_profile` receipt: `onexit_table_bytes: 24`,
   `actual_pe_imports: 7`, `target_execution: false`.
9. Census `expected['mb_win_crt_startup_abi'] = 8`.
10. `crt_startup` result key required iff `--crt-startup`, forbidden otherwise.

## Supporting integration (the InitOnce-pattern plumbing the wiring implies)

- `owned_runtime.c/h`: session member, tail position in the shared-name bind
  chain (underscore-prefixed UCRT names intersect no KERNEL32 family name),
  begin/end session, `crt_startup_enabled` report field, consumer-import count
  `+7`; IMPORT_CAPACITY unchanged (58≤64 without fileio, 68≤96 with —
  `--crt-startup` requires `--sync`).
- `result_report.c`: emits `,"crt_startup":{"enabled":1}`.
- `test_entry_candidate.c` / `test_worker_candidate_failures.c`: import-count
  `+7` and `crt_startup_enabled` checks.
- `tests/worker-fixture.c`: default-arg guard, header include, worker-thread
  `mb_worker_crtstartup_thread_begin()` hook, creator-side
  `mb_worker_crtstartup_finish()` check (WorkerCollect `-55`; error codes
  -55..-59 reserved, verified unique in the tree).
- `tests/worker-crtstartup.h`: dllimport declarations of the 7 documented
  signatures; static caller-owned `__onexit_table_t`-shaped {first,last,end}
  triple + 4 slots (the family never allocates); worker-thread exercise:
  `_initialize_onexit_table` EINVAL-on-NULL + zeroing → attach (fixture owns
  the fields, mirroring the documented entry-adapter service) → register→
  execute LIFO (slots cleared, table emptied = last back on first,
  still-backed reuse) → second execute no-op → register again → `_initterm`
  over a sentinel table (interior NULLs skipped, exclusive end) →
  `_initterm_e` error-stop (later entry never runs) → `_crt_atexit` ×2 →
  `_cexit` LIFO → double `_cexit` no-op. Execution-order record: '7','6','8',
  '1','2','3','5','4'. Creator-side `finish()` verifies the recorded state and
  an explicit `done` marker; `init_null_rc` is stored separately from the
  successful table initialize.
- One adapted line in the vendored `mb_win_crt_startup_abi.h`:
  `#include "pe_module.h"` → `#include "../windows-execution/pe_module.h"`
  (tree include convention, documented in `mb_win_fileio_abi.h` as the
  promotion adaptation; the implement worker's original compiled under its own
  -I layout).

## Defects found and fixed during validation (fixture-side; family sources unchanged)

1. First failure (`0xf0010002`, worker exit): my emptied-table check required
   all three pointers NULL after `_execute_onexit_table`; the owner keeps
   first/end attached and resets `last` onto `first` (still-backed reuse is the
   documented model). Fixed to check `last==first` with slots cleared.
   Instrumented step pass then identified the second defect precisely.
2. Second failure (step 21): atexit LIFO order indices were off by one —
   `_initterm_e`'s stopping entry records '3' at order[5] (n=6), so the atexit
   callbacks land at order[6]='5', order[7]='4', n=8. Fixed (n==8, indices 6/7)
   plus a matching `finish()` fix and an `init_null_rc` field so the EINVAL
   proof is not overwritten by the successful initialize.

## Honest notes

- One transient, not fully attributed: one intermediate run's `finish()`
  returned -55 (guard mismatch) on the creator thread despite a successful
  worker exercise; after restructuring `finish()` around an explicit
  worker-written `done` marker (kept as legitimate fixture design) and the
  `init_null_rc` split, the clean tree passed deterministically across the
  host run and the SDK composition's re-run of all host controls (fresh
  processes each). The pass evidence above is from the final clean tree.
- One intermediate run hung in driver exit after a forced `-16` failure path
  (beyond its 600 s bound); I stopped my own identified build/driver PIDs only.
  The driver child was kernel-stuck in an uninterruptible exit wait at kill
  time (SIGKILL delivered). Other workers' driver processes visible on the box
  were pre-existing and untouched. The passing clean runs exit cleanly.
- `helper-link`/`consumer-link` warnings are the pre-existing benign
  rust-lld `/align specified without /driver` notes (fixture DLLs are embedded
  loader data, never run as Windows images).
- Component honesty: this is host-control + SDK-compile evidence only.
  `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED` — nothing executed on target,
  no PS5 support claimed; census/PE-import claims are the build's own recorded
  assertions, not target observations.
- Intermediate FAILED receipt trees are preserved raw under `host-out/`
  (f329d785…, 9ac4a436…, 659e3ea5…, 37f3a754…, 9912295f…, 775d686a… pass with
  temporary diagnostics); the two staged receipts are the clean-tree evidence.

## Files

Owned area `outputs/2026-10-04-zcode-team/worker-crt-integrate/`:
`vendor/` (integrated tree), `receipts/host-validation-clean/`,
`receipts/sdk-composition/`, `staging-manifest.json`, `CLAIM.json`, this report.
