# worker-4-win-services report — KERNEL32 one-time initialization family

## What was implemented

`InitOnceBeginInitialize` and `InitOnceComplete` as a new windows-services-family
owner, following the existing owner patterns exactly (thread/last-error domain
reuse, sidecar public storage, ms_abi bridge binder, verifier receipts). All new
files live only under this output area and are labeled as vendored adapted
copies; no existing file was touched.

Owned files (all under `outputs/2026-10-04-zcode-team/worker-4-win-services/`):

- `sources/mb_win_init_once.h` / `sources/mb_win_init_once.c` — domain owner.
- `sources/mb_win_init_once_abi.h` / `sources/mb_win_init_once_abi.c` —
  ms_abi bridge: session pinning plus the `mb_pe_binder` for the two imports.
- `sources/tests/test_win_init_once.c` — host typed-ABI test plus the
  `MB_WIN_INIT_ONCE_LAYOUT` native/bridge layout probe.
- `verify_init_once.py` — verifier (host, ASan/UBSan, TSan, red-stub control,
  native PS5 object route). Imports the unchanged existing
  `windows-services/verify.py` helpers read-only.
- `receipts/` — receipt trees produced by the runs below.

## Win64 semantics decisions (widths are contracts)

- `INIT_ONCE` is `RTL_RUN_ONCE { PVOID Ptr; }`: **8 bytes, 8-byte aligned**
  public storage, modeled as `mb_win_once { uint64_t opaque; }` with
  `_Static_assert`s in both owner and ABI translation units. Same sidecar
  pattern as SRW/CV: Windows storage holds only an opaque token; all native
  pthread state stays in the owner's private sidecars; nothing is embedded.
- **Both APIs return 32-bit `BOOL`** (verified as `win64cc i32` in the bridge
  IR: `define internal win64cc i32 @begin_initialize(ptr, i32, ptr, ptr)` and
  `@complete_initialize(ptr, i32, ptr)`). This deliberately differs from the
  8-bit `BOOLEAN` of `TryAcquireSRWLock*` recorded by the sync slice — InitOnce
  is documented BOOL-returning, `fPending` is a `PBOOL` (32-bit out), context
  is pointer-sized.
- Microsoft flag values: `INIT_ONCE_CHECK_ONLY=1`, `INIT_ONCE_ASYNC=2`,
  `INIT_ONCE_INIT_FAILED=4`.
- `CHECK_ONLY`: success with `pending=0` plus stored context only when
  initialization completed; otherwise the call fails with last error
  `ERROR_IO_PENDING` (997) as Microsoft documents. CHECK_ONLY takes priority
  over ASYNC (ntdll behavior). Unknown flag bits fail with 87.
- Blocking waiters (there is no timed InitOnce API) are released FIFO by
  completion, or by `INIT_ONCE_INIT_FAILED` handoff: the first queued waiter is
  handed the initialization right; with no waiters the object resets to
  never-begun and public storage returns to zero (failure-then-retry).
- `INIT_ONCE_ASYNC`: every asker is granted pending immediately (no blocking);
  first `InitOnceComplete` wins and stores the context; later right holders
  fail with `ERROR_ALREADY_EXISTS` (183). `INIT_ONCE_INIT_FAILED` never
  combines with ASYNC (87) and its context argument is ignored (documented).
- Adapter-policy failures (documented where Microsoft documents the behavior,
  policy where it does not): complete without the right / duplicate complete /
  never-begun object → 31 `ERROR_GEN_FAILURE`; async race loser → 183;
  capacity/token/allocation exhaustion → 8. Failed calls never write outputs;
  successful calls preserve LastError (verified with a per-thread marker on
  every path, including worker threads).
- Lifecycle: completed objects retain their context until owner close
  (mirroring a real Windows completed INIT_ONCE holding its encoded context),
  so they count against capacity; owner close refuses active rights/waiters,
  frees completed contexts without touching public storage, and the caller
  re-zeroes storage before reuse exactly as a Windows application must.

## Exact commands and results

```
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-04-zcode-team/worker-4-win-services/verify_init_once.py --sanitize --tsan
  -> status INITONCE_PTHREAD_WIN64_AND_NATIVE_OBJECTS_PASSED
     receipt receipts/237c77e8f8ee4478a822e7fdd1f19ffd/receipt.json
python3 outputs/2026-10-04-zcode-team/worker-4-win-services/verify_init_once.py --red-stub --host-only
  -> status EXPECTED_OPEN_STUB_REJECTION
     receipt receipts/86276415296445bca9f3d73e835ac6f1/receipt.json
```

Evidence in the passing receipt (host/sanitized/TSan runs all exit 0 and print
all eight PASS lines):

1. Host typed ms_abi calls through binder-resolved function pointers
   (kernel32 name, ASCII-folded library): pending/completed lifecycle,
   check-only both ways, duplicate completion 183, outputs untouched on
   failure, unknown flags 87, check-only 997, failure-then-retry with storage
   back to zero, INIT_FAILED context ignored, INIT_FAILED+ASYNC rejected.
2. Real pthread concurrency: two blocking waiters receive the completed
   context; INIT_FAILED hands off to the first queued waiter while a later
   waiter sees the retry's result; ASYNC grants two concurrent initializers
   with the second admitted before the first completes (non-blocking proof),
   first completion wins, loser gets 183; a non-async caller waits for an
   async initialization.
3. Session/binder refusals (wrong library, unknown name, copied session,
   end-while-ACTIVE BUSY), capacity exhaustion fails explicitly with 8,
   close BUSY with an active initializer, close OK releasing completed
   contexts, final domain close clean.
4. Native route (unchanged ABI rule): `prospero-clang` direct PS5 owner object
   plus FreeBSD9-frontend → **unedited win64cc LLVM IR** → PS5 backend for the
   ABI unit; data layout equality native vs bridge; layout probe equality
   `[8,8,4,4,8,8,8,16,16,40,16,32]` (pointer, size_t, DWORD, BOOL,
   INIT_ONCE size/align, owner, pin, config, session, offsets); exactly two
   win64cc definitions, both `i32`-returning with the exact documented
   parameter kinds; binder stays native-C calling convention.
5. PS5 SDK declaration check (component evidence only): owner imports are
   pthread_cond_*, pthread_mutex_*, pthread_self/equal, calloc/free/malloc/
   abort, all declared by `libkernel_web.so` / `libSceLibcInternal.so` in SDK
   v0.43. `target_resolution_proven: false` — **no target(PS5) support claim**.
   Host evidence + native objects + SDK declarations are component evidence;
   target support stays a separate explicit status for the controller.

## build.py wiring points for the controller (NOT edited by this worker)

The sync slice integration pattern in
`outputs/ps5-mod-bridge/windows-candidate/build.py` maps 1:1 for this family:

1. Promote the four C sources + test into `windows-services/` (they are
   pattern-identical residents; my copies exist only so history stays intact).
2. argparse: add `--init-once` requiring `--sync` (line ~37 pattern: sync
   requires workers requires clock requires entry).
3. Generated `owned_profile.h` (line ~42): add `#define MB_OWNED_INIT_ONCE`.
4. Source lists (lines ~50-52): `native_c += [SERV/'mb_win_init_once.c']`,
   `abi_c += [SERV/'mb_win_init_once_abi.c']`.
5. Receipt `sources` list (lines ~59-61): add `mb_win_init_once.h`,
   `mb_win_init_once_abi.h` and the new fixture defs/header below.
6. Import libraries (lines ~87-89): create
   `tests/worker-initonce-kernel.def` exporting `InitOnceBeginInitialize`,
   `InitOnceComplete`; `lld-link /lib /def:` like `sync-kernel.lib`; link it
   into the consumer DLL (line ~100 `extra`).
7. Consumer/helper fixture (lines ~93-95): pass `-DMB_OWNED_INIT_ONCE=...`,
   and exercise the imports in the worker fixture (extend
   `tests/worker-sync.h` pattern with an InitOnce begin/complete pair so the
   PE import census actually calls them).
8. Receipt profile (line ~72): `init_once` profile with
   `init_once_bytes: 8`, `pe_imports: 2`.
9. win64cc census (line ~199): add `'mb_win_init_once_abi': 2` to `expected`.
10. Host driver result assertions (lines ~159-160 pattern): require/forbid the
    new `init_once` result key per flag.

## Open questions

- Which waiter Windows picks after INIT_ONCE_INIT_FAILED is not documented; we
  implement FIFO among queued waiters and document it as adapter policy.
- The exact Windows failure code for "complete without owning the right" is not
  documented; 31 is adapter policy. 997 for check-only-not-complete and the
  "first async completion wins, later ones fail" rule are the documented parts.
- CHECK_ONLY|ASYNC combination: we follow the observed ntdll behavior
  (CHECK_ONLY wins, ASYNC ignored); no Microsoft statement found either way.
- Host evidence quality: the two blocking-waiter tests use bounded
  flag+pause choreography (same technique as the SRW tests); the ASYNC
  non-blocking proof is deterministic (admission recorded before release).
- Target (PS5) execution: NOT attempted and NOT claimed; PS5 (ps5-host-not-configured.invalid)
  is controller-only per assignment.

## Component status

- Host typed Win64 ABI service: implemented and verified (incl. ASan/UBSan/TSan).
- Native PS5 owner + bridge objects: compiled and receipts recorded (SDK
  v0.43, LLVM 21.1.8 pins, unedited IR route, ABI rule intact).
- Retained-PE-runtime integration (build.py census, PE fixture exercise,
  target dispatch): pending controller action per the wiring points above.
