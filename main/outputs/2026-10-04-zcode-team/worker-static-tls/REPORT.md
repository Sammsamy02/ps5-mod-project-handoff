# worker-static-tls — REPORT.md (2026-10-04)

## Task
Implement the static TLS family — the next windows-services family after CRT startup:
PE TLS directory parsing (all six fields), per-thread static TLS allocation, callback
dispatch (DLL_PROCESS/THREAD ATTACH/DETACH), index management, the
`__tls_import`/related-names PE binder, host tests, pinned prospero-clang
cross-compile evidence, integration notes.

## Census findings (authority: worker-dll-mod-retention/baseline-dll-census.json)
- **UE4SS.dll**: TLS directory PRESENT — 728 raw bytes, `size_of_zero_fill=0`,
  characteristics 0x500000, `callbacks_field_kind="va"`, 2 callbacks
  (0x180c956f0, 0x180c95a10). Also imports dynamic TLS/FLS (TlsAlloc/TlsGetValue/
  TlsSetValue/TlsFree, FlsFree/FlsAlloc/FlsSetValue — dynamic surface stays with
  `mb_win_tls_bridge`; FLS is out of scope here).
- **dwmapi.dll**: `tls.directory_present = false` — **no static TLS**. The task
  premise ("both retained DLLs have static TLS with callbacks") is contradicted by
  the census; reported honestly, no fixture fabricated. The family still supports
  zero-raw-byte rejection, empty callback arrays, and multi-callback arrays.

## Delivered (all under outputs/2026-10-04-zcode-team/worker-static-tls/)
- `CLAIM.json` (first), `sources/mb_win_static_tls.{h,c}` (owner: parse, register/
  unregister with load-time index write into the image's AddressOfIndex DWORD,
  per-thread template+zero-fill blocks stored through the mb_win_threads dynamic
  pool, ascending-attach / descending-detach callback dispatch read live from mapped
  memory, pin/session/close ownership), `sources/mb_win_static_tls_abi.{h,c}`
  (session pattern + binder for `__tls_index`/`_tls_index`/`__tls_start`/
  `_tls_start`/`__tls_end`/`_tls_end`/`__tls_import`/`_imp___tls_import`, exact
  ASCII, any library, ordinals unresolved, `__tls_array` deliberately UNRESOLVED),
  `sources/tests/test_win_static_tls.c` (parse, census-shaped fixture, registration,
  index write, template copy + zero fill, order-observable callback log
  [1,1,0] ascending / [0,1,1] descending, two-thread isolation, lazy re-alloc,
  index reuse with fresh template, error paths, binder suite),
  `verify_static_tls.py`, `INTEGRATION.md` (10-point wiring list), `receipts/`.
- Nothing outside this directory was written; no existing file modified.

## Evidence (current-source receipts)
- host-only: receipts/d88000816f2548c08be6669729496501 — STATIC_TLS_HOST_TESTS_PASSED
- ASan+UBSan (with native section): receipts/c2ec618539b64aecb9b03ebf45a76c41 — PASSED
- TSan: receipts/ff1bb0195bcb48b7b41302937df5276c — PASSED
- red-stub negative control (owner open stubbed to BAD_STATE must fail the suite):
  receipts/df394d16b6054cb1872a89331ceb2d00 — EXPECTED_OPEN_STUB_REJECTION
- full: receipts/5a4bada72fcf4cbea046b43d5e8aa57b —
  STATIC_TLS_DIRECTORY_BLOCKS_CALLBACKS_AND_NATIVE_OBJECTS_PASSED:
  layout parity [8,8,40,8,0,8,16,24,32,36,120,40,16] across native/bridge IR,
  prospero-clang direct PS5 owner object (mb_win_static_tls.o, imports all
  mb_win_threads_*), FreeBSD9-frontend bridge IR with exactly one win64cc
  definition `@tls_import_thunk(i32, i64)`, PS5 backend objects, llvm-nm/objdump
  captures, SDK export coverage (owner requires no SDK symbols). All receipts:
  `target_executed=false`, `console_contact=false`. Earlier receipts retained for
  provenance include genuinely FAILED attempts (see debugging notes); raw files
  unchanged.

## Documented divergences (no full Windows-loader parity claimed)
1. Lazy per-thread block allocation (Windows allocates eagerly for pre-existing
   threads without thread-attach callbacks; content identical).
2. `__tls_import` is an adapter-defined thunk (controller-backlog name), not a
   Microsoft declaration; no census import uses it.
3. No TEB emulation (`reserved` passed as NULL), no gs:[0x58]; `__tls_array`
   unresolvable by design.
4. Static TLS indexes share the domain's dynamic TLS pool (Windows TEB shares the
   first 64 slots similarly; expansion differs).
5. No loader lock; the entry adapter owns DllMain ordering; callbacks run outside
   the domain mutex. No exception trapping around callbacks.
6. `characteristics` carried opaque, never validated; image index DWORD left stale
   (unread) after unregister.
7. Host tests run under Rosetta x86_64; win64cc callback dispatch on target needs
   the entry-bridge trampoline wiring (INTEGRATION.md point 6) before any target
   execution claim.

## Bugs found and fixed during this task (kept honestly)
1. **BOOL inversion**: `mb_win_threads_tls_set/tls_free` return Windows BOOL
   (nonzero = success); initial code treated 0 as success, so a successful publish
   freed the block and left a dangling slot pointer (found via a minimal debug
   harness after a host hang: main in pthread_join, garbage block, live=0).
2. Test fixture `build_directory` wrote 8 bytes at offset 36 of a 40-byte buffer —
   4-byte stack overflow caught by ASan (`__asan_report_store1` in build_directory);
   fixed to a 32-bit store.
3. Test never assigned `g_module` before the worker attach (worker suite ran against
   NULL); begin/end session state ordering fixed (begin requires EMPTY).
4. Unregister left `tls_index=0` (looks registered) instead of `NO_INDEX`;
   register now accepts a zeroed-or-NO_INDEX record.
5. Red-stub control initially walked into NULL derefs after the stubbed open
   (wedging under Rosetta's exception server); the suite now fails fast and cleanly.
6. Two Rosetta-hosted stale test processes from interrupted runs remained in
   uninterruptible state (UE) and could not be killed; they hold no shared state
   relevant to this family (isolated receipts subdirectories). No global escape or
   unrelated process termination was used.

## Non-claims
No target/PS5 execution, no runtime behavior claim for retained DLLs, no FLS
implementation, no full MSVC/Windows loader parity, no quality score implied.
Gameplay remains a prototype; this is component evidence only.
