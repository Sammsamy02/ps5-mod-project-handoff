# worker-mutex report — KERNEL32 mutex + waitable-handle family

## What was implemented

`CreateMutexW`, `CreateMutexExW`, `OpenMutexW`, `ReleaseMutex`,
`WaitForSingleObject`, `WaitForSingleObjectEx` as a new windows-services-family
owner, following the InitOnce/SRW owner patterns exactly (thread/last-error
domain reuse, sidecar public handles, ms_abi bridge binder, verifier
receipts). All new files live only under this output area and are labeled as
vendored adapted copies; no existing file was touched.

Owned files (all under `outputs/2026-10-04-zcode-team/worker-mutex/`):

- `sources/mb_win_mutex.h` / `sources/mb_win_mutex.c` — domain owner
  (objects, handle registry, name table, waiter queues, abandonment key).
- `sources/mb_win_mutex_abi.h` / `sources/mb_win_mutex_abi.c` — ms_abi
  bridge: session pinning plus the `mb_pe_binder` for the six imports.
- `sources/tests/test_win_mutex.c` — host typed-ABI test plus the
  `MB_WIN_MUTEX_LAYOUT` native/bridge layout probe.
- `sources/tests/worker-mutex.h` + `sources/tests/worker-mutex-kernel.def` —
  controller-ready PE consumer fixture (5 import names; see wiring point 6).
- `verify_mutex.py` — verifier (host, ASan/UBSan, TSan, red-stub control,
  native PS5 object route). Imports the unchanged existing
  `windows-services/verify.py` helpers read-only.
- `receipts/` — receipt trees produced by the runs below (including the two
  intermediate failed runs preserved as raw provenance).

## Win64 semantics decisions (widths are contracts)

- `HANDLE` is **pointer-sized**; mutex handles are opaque nonzero 64-bit
  tokens, never reused, with all native pthread state in owner sidecars
  (same sidecar discipline as SRW/CV/InitOnce — nothing is embedded in
  caller storage). `BOOL`/`DWORD` are 32-bit: `ReleaseMutex` returns
  `win64cc i32`, both waits return `win64cc i32` (u32 DWORD codes), the three
  create/open paths return `win64cc ptr` (verified in the bridge IR below).
- Wait codes: `WAIT_OBJECT_0=0`, `WAIT_ABANDONED=0x80`, `WAIT_TIMEOUT=0x102`,
  `WAIT_FAILED=0xffffffff`; `INFINITE=0xffffffff` as `dwMilliseconds` DWORD.
  An unowned mutex is signaled: any wait on it acquires immediately.
- **Windows mutexes are not recursive.** Owner re-wait with a finite timeout
  follows the real timeout path (monotonic deadline, `WAIT_TIMEOUT`); owner
  re-wait with `INFINITE` is a guaranteed deadlock on Windows, so this
  adapter **refuses instead of hanging**: `WAIT_FAILED` + 771
  `ERROR_POSSIBLE_DEADLOCK` (adapter policy, tested, `alarm(30)` guard).
- **Abandonment**: the owner installs its own `pthread_key` destructor; the
  first time a thread takes ownership it registers in a per-thread owned-object
  list, and native thread exit (no release) marks every still-owned mutex
  abandoned and hands the first queued waiter `WAIT_ABANDONED` **plus
  ownership**; `ReleaseMutex` then succeeds for the new owner (tested across
  two real pthreads). This uses the same thread-domain integration style as
  the sync families (every calling thread admitted; last-error via domain).
- `WaitForSingleObjectEx` alertable flag is documented and **fails
  explicitly**: `WAIT_FAILED` + 50 `ERROR_NOT_SUPPORTED`. No APC/user-APC
  surface is implemented and none is faked (brief rule).
- Named objects: per-owner private name table with **ASCII-only case
  folding** (A–Z lowered; other UTF-16 code units compared verbatim — the
  Windows object manager's upcase table is locale-wide, so exact non-ASCII
  folding is marked adapter policy), optional folded `Local\` prefix accepted
  and stripped into the same private namespace, any other backslash refused
  with 87 (Microsoft: the name "can contain any character except the
  backslash character"), names limited to MAX_PATH (260) UTF-16 units
  (over-length → 206 `ERROR_FILENAME_EXCED_RANGE`, adapter policy),
  NULL/empty name = unnamed for creation (empty-name choice is adapter
  policy; NULL is documented). `Global\`/`Session\` are not separate
  namespaces here: they are refused as backslash names — single-private-
  namespace adapter policy.
- `CreateMutex` on an existing name returns a **new handle to the same
  object, succeeds, and sets last error 183 `ERROR_ALREADY_EXISTS`** while
  ignoring the initial-owner request (Microsoft-documented; the only success
  that writes last error — all other successes preserve LastError, verified
  with a per-thread marker on every path including worker threads).
  `OpenMutexW` on a missing name → NULL + 2 `ERROR_FILE_NOT_FOUND`
  (Microsoft-documented); NULL/empty name to open → 87 (adapter policy).
  `ReleaseMutex` by a non-owner → FALSE + 288 `ERROR_NOT_OWNER`
  (Microsoft-documented). Invalid handle → `WAIT_FAILED`/FALSE + 6.
  Unknown `CreateMutexExW` flag bits → NULL + 87. Security-descriptor
  pointers and `dwDesiredAccess` are accepted structurally and not enforced
  (single-process runtime, no ACL model — adapter policy).
- Close policy (documented, tested): owner close refuses service pins,
  blocked waiters, or outstanding handles (BUSY). `mb_win_mutex_close_handle`
  is the CloseHandle composition point (the workers family owns the actual
  KERNEL32 `CloseHandle`/`WaitForSingleObject` thread bindings today); an
  object is destroyed when its last handle retires **and** no blocked waiter
  references it, and destroying an owned object drops that ownership exactly
  like the object vanishing on Windows. FIFO waiter handoff is adapter
  policy (Windows does not document wake order).
- Failure codes follow Microsoft where documented (183, 2, 288, 6, 87) and
  are adapter policy elsewhere (771 refusal, 50 alertable, 206 length, 8
  exhaustion, 31 native failure). Unimplemented paths fail explicitly; no
  guessed aliases; no event/semaphore/timer/wait-multi objects (not
  census-observed and not this family).

## Exact commands and results

```
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-04-zcode-team/worker-mutex/verify_mutex.py --sanitize --tsan
  -> status MUTEX_PTHREAD_WIN64_AND_NATIVE_OBJECTS_PASSED
     receipt receipts/941cafe38fd843efb813a60e7b7ba3f1/receipt.json
python3 outputs/2026-10-04-zcode-team/worker-mutex/verify_mutex.py --red-stub --host-only
  -> status EXPECTED_OPEN_STUB_REJECTION
     receipt receipts/ef28254e02054afbb5fa919142cb215f/receipt.json
```

Evidence in the passing receipt (host/sanitized/TSan runs all exit 0 and
print all eight PASS lines):

1. Host typed ms_abi calls through binder-resolved function pointers
   (kernel32 name, ASCII-folded library): unnamed lifecycle
   (signaled-when-unowned immediate grant, owner finite-timeout 258, owner
   INFINITE refusal 771, release, non-owner release 288), invalid handles 6
   for wait/release/close/is_handle, alertable 50 vs alertable-zero acquire,
   unknown Ex flags 87.
2. Real pthread concurrency: initial-owner contention with two blocked
   native threads granted FIFO (order asserted after controlled queueing),
   each releasing successfully with per-thread LastError preserved.
3. Abandonment across threads: owner thread exits without release (pthread
   key destructor), waiter receives `WAIT_ABANDONED` (0x80), takes
   ownership (`ReleaseMutex` succeeds), abandoned flag consumed afterwards.
4. Named objects: mixed-case create + folded open, `Local\` prefix strip +
   fold, existing-name create returns handle + 183 with initial owner
   ignored (release proves nobody owns), backslash names 87, 261-unit name
   206, NULL/empty open names 87, missing name 2.
5. Close policies: owner close BUSY with a blocked waiter; retiring one of
   two handles keeps the object alive until the waiter's own reference ends;
   last handle retires the object.
6. Session/binder refusals: wrong library, unknown name (`CreateMutexA`
   included — census-observed but not this family), ordinal import,
   zeroed/copied/ended sessions, end-while-ACTIVE BUSY, cross-session copy
   refused.
7. Capacity exhaustion fails explicitly with 8; owner close requires zero
   handles; final domain close clean. Red-stub control: only `open` mutated
   to BAD_STATE; the real typed test rejects it (expected failure asserted),
   sources stable.
8. Native route (unchanged ABI rule): `prospero-clang` direct PS5 owner
   object plus FreeBSD9-frontend → **unedited win64cc LLVM IR** → PS5
   backend for the ABI unit; data layout equality native vs bridge; layout
   probe equality `[8,8,4,4,8,16,16,40,16,32]` (pointer, size_t, DWORD,
   BOOL, owner, pin, config, session, offsets); **exactly six win64cc
   definitions** — three `ptr`-returning (`create_mutex_w(ptr,i32,ptr)`,
   `create_mutex_ex_w(ptr,ptr,i32,i32)`, `open_mutex_w(i32,i32,ptr)`) and
   three `i32`-returning (`release_mutex(ptr)`,
   `wait_for_single_object(ptr,i32)`,
   `wait_for_single_object_ex(ptr,i32,i32)`) — binder stays native-C
   calling convention.
9. PS5 SDK declaration check (component evidence only): every owner import
   (`pthread_key_create/setspecific/getspecific/key_delete`,
   `pthread_mutex_*`, `pthread_cond*` incl. `timedwait` and `condattr`,
   `pthread_self/equal`, `clock_gettime`, `calloc/free/malloc/memmove`,
   `abort`) is declared by `libkernel_web.so` (19) / `libSceLibcInternal.so`
   (5) in SDK v0.43. `target_resolution_proven: false` — **no target(PS5)
   support claim**. Host evidence + native objects + SDK declarations are
   component evidence; target support stays a separate explicit controller
   status.

## Census honesty (retained import evidence)

The retained readobj (`windows-image-original-inputs-1/final-1/UE4SS.dll.llvm.txt`)
names exactly: `CreateMutexA`, `ReleaseMutex`, `WaitForSingleObject`,
`WaitForSingleObjectEx` (+ `OpenThread`, `SwitchToThread`, `Sleep` outside
this family); `dwmapi.dll` names only `CloseHandle`. So of my six bound
names, `ReleaseMutex`, `WaitForSingleObject`, `WaitForSingleObjectEx` are
census-observed; `CreateMutexW`/`CreateMutexExW`/`OpenMutexW` are the
generic W implementation required by the assignment brief. The observed
`CreateMutexA` slot is **not** bound by me (an A→W shim needs a CP_ACP
decision and is beyond my named scope) — controller decision required to
cover that slot. No event/semaphores (correctly absent from this family).

## build.py wiring points for the controller (NOT edited by this worker)

The `--init-once` integration in `outputs/ps5-mod-bridge/windows-candidate/build.py`
maps 1:1 for this family (line numbers from that file):

1. Promote the four C sources + host test into `windows-services/`
   (`mb_win_mutex.c/.h`, `mb_win_mutex_abi.c/.h`) — pattern-identical
   residents; my copies exist only so history stays intact.
2. argparse (~line 26): add `--mutex` requiring `--sync` (line ~39 pattern:
   `--init-once requires --sync`).
3. Generated `owned_profile.h` (~line 44): add `#define MB_OWNED_MUTEX`.
4. Source lists (~lines 55-56): `native_c += [SERV/'mb_win_mutex.c']`,
   `abi_c += [SERV/'mb_win_mutex_abi.c']` under the new flag.
5. Receipt `sources` list (~line 66): add `mb_win_mutex.h`,
   `mb_win_mutex_abi.h`, `tests/worker-mutex.h`, `tests/worker-mutex-kernel.def`.
6. Import library (~line 96): `lld-link /lib /def:tests/worker-mutex-kernel.def`
   → exports `CreateMutexW CreateMutexExW OpenMutexW ReleaseMutex
   WaitForSingleObjectEx`; link into the consumer `extra` (~line 108).
   Deliberately **no `WaitForSingleObject`** in this def: `worker-kernel.def`
   already provides it for thread handles. Composition: route the shared
   KERNEL32 `WaitForSingleObject` through `mb_win_mutex_is_handle()` first
   (mutex token) and the workers `wait_thread` otherwise; likewise compose
   `CloseHandle` to call `mb_win_mutex_close_handle` for mutex tokens. This
   dispatch is the single real integration decision this family adds.
7. Consumer/helper fixture (~lines 93-102): pass `-DMB_OWNED_MUTEX=...`;
   extend `tests/worker-fixture.c` with `#if MB_OWNED_MUTEX #include
   "worker-mutex.h" #endif` and call `mb_worker_mutex_prepare()` /
   `mb_worker_mutex_thread_acquire()` (worker path, gated like
   `mb_worker_initonce_thread_begin`) / `mb_worker_mutex_finish()` after
   join — the fixture header is written against the existing
   `mb_worker_*` types and the worker exit-without-release flow.
8. Receipt profile (~line 78): `mutex_profile` with `handle_bytes: 8`,
   `pe_imports: 5`.
9. win64cc census (~line 209): add `'mb_win_mutex_abi': 6` to `expected`
   (3 ptr + 3 i32 returns per the receipt).
10. Host driver result assertions (~lines 169-170): require/forbid the
    `mutex` result key per flag.

## Microsoft-doc citations vs adapter-policy markers

- Documented (Microsoft Learn, CreateMutexW/CreateMutexExW/OpenMutexW/
  ReleaseMutex/WaitForSingleObject(Ex) and Object Names pages): existing-name
  create returns handle + `ERROR_ALREADY_EXISTS` and ignores initial owner;
  NULL name = unnamed; name may not contain backslash; `ReleaseMutex` fails
  `ERROR_NOT_OWNER` for non-owner; `WAIT_ABANDONED` grants ownership to the
  waiter; wait returns `WAIT_OBJECT_0`/`WAIT_TIMEOUT`/`WAIT_FAILED`;
  abandoned mutex state; object names are case-insensitive in the object
  namespace; `OpenMutex` missing name → `ERROR_FILE_NOT_FOUND`.
- Adapter policy (documented in-code, honest boundaries): 771 for the
  refused owner-INFINITE re-wait; 50 for alertable (APC out of scope);
  206/260-unit name bound; ASCII-only folding extent; empty-name=unnamed;
  single private namespace (Local\ stripped, Global\ refused); 87 for
  unknown Ex flags and NULL open name; 8 exhaustion; FIFO handoff; handle
  tokens never reused; ownership dropped when the last handle retires;
  SECURITY_ATTRIBUTES/dwDesiredAccess ignored.
- Not in scope and failing explicitly: alertable/APC waits, non-mutex
  handle types in this bridge's wait (thread handles belong to the workers
  binding until the composed dispatch lands), `CreateMutexA`, events,
  semaphores, `WaitForMultipleObjects`, waitable timers.

## Open questions

- Wake order after release/abandonment is not documented by Microsoft; FIFO
  is this adapter's policy (asserted only under controlled queueing).
- The exact Windows error for a backslash name and for over-length names is
  not pinned by the docs I could rely on; 87/206 are policy.
- Whether an empty (non-NULL) name creates an unnamed mutex on every
  Windows version is unclear from the docs; unnamed is our policy.
- Target (PS5) execution: NOT attempted and NOT claimed; PS5 (ps5-host-not-configured.invalid)
  is controller-only per assignment.

## Component status

- Host typed Win64 ABI service: implemented and verified (ASan/UBSan/TSan,
  red-stub negative control).
- Native PS5 owner + bridge objects: compiled and receipts recorded (SDK
  v0.43, LLVM 21.1.8 pins, unedited IR route, ABI rule intact, 6 win64cc
  definitions counted for the census).
- Retained-PE-runtime integration (build.py census, PE fixture exercise,
  shared WaitForSingleObject/CloseHandle dispatch, `CreateMutexA` census
  slot): pending controller action per the wiring points above.
