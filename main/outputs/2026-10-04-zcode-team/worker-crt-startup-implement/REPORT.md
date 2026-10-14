# worker-crt-startup-implement report — CRT startup family (windows-services resident)

## What was implemented

The MSVC CRT DLL startup block that the retained images' own `_DllMainCRTStartup`
bodies need, as a new windows-services family following the InitOnce/Mutex/FileIO
patterns exactly: `mb_win_` owner unit, `ms_abi` bridge/binder unit, host typed
tests, pinned-toolchain native/bridge object evidence, verifier receipts. All
semantics are the delivered design (`worker-crt-startup-design/DESIGN.md` +
`crt_startup.{h,c}` host prototype, unchanged there), extended with the
census-observed UCRT onexit-table family. The census
(`worker-dll-mod-retention/baseline-dll-census.json` via the controller
addendum) is the observed-symbol authority: **dwmapi.dll (135 imports, 8 startup
imports) and UE4SS.dll (634 imports, 11 startup imports), every startup import
from `api-ms-win-crt-runtime-l1-1-0.dll`**.

Owned files (all under `outputs/2026-10-04-zcode-team/worker-crt-startup-implement/`):

- `sources/mb_win_crt_startup.h` / `.c` — freestanding C11 owner (no OS calls,
  no allocation, no libc): `_initterm` walker (sentinel-terminated tables,
  interior NULLs skipped, returns discarded), `_initterm_e` (errno_t
  error-stop), `_seh_filter_dll` decision state machine (EXECUTE_HANDLER
  exactly once inside the init window → INIT_FAILED, CONTINUE_SEARCH
  otherwise, never CONTINUE_EXECUTION), `_crt_atexit` + fixed-capacity (32)
  global atexit table with `_cexit` LIFO, the documented
  `__onexit_table_t {first,last,end}` trio
  (`_initialize_onexit_table`/`_register_onexit_function`/
  `_execute_onexit_table`), and the composed
  `mb_win_crt_dll_process_attach`/`_detach` orchestrators (xi→xc walks in the
  filter window; detach runs xp→xt→atexit-LIFO only after a completed init).
- `sources/mb_win_crt_startup_abi.h` / `.c` — session lifecycle + the PE
  binder for the observed import names; eight `ms_abi` shims with the exact
  documented Microsoft x64 signatures.
- `sources/tests/test_win_crt_startup.c` — 9 host suites (design's 7 ported
  plus onexit-table and binder/session suites) + the `MB_WIN_CRT_STARTUP_LAYOUT`
  native/bridge layout probe.
- `verify_crt_startup.py` — verifier (host, ASan/UBSan, red-stub negative
  control, native PS5 owner object + unedited-win64cc-IR bridge route).
  Imports the unchanged `windows-services/verify.py` helpers read-only.
- `receipts/` — receipt trees from the runs below (two FAILED intermediates
  preserved as raw provenance; see "Exact commands").
- `CLAIM.json` — written first; this report last.

## Census attribution and ABI decisions (widths are contracts)

- **Bound names/libraries (ASCII-folded, ordinals unresolved)**:
  `api-ms-win-crt-runtime-l1-1-0.dll` (the exact census-observed library for
  every startup import of both DLLs) and the documented canonical home
  `ucrtbase.dll` → `_initterm`, `_initterm_e`, `_crt_atexit`, `_cexit`,
  `_initialize_onexit_table`, `_register_onexit_function`,
  `_execute_onexit_table`. `vcruntime140.dll` → `_initterm`, `_initterm_e`,
  `_seh_filter_dll` (documented vcruntime home; **no census row itemizes
  `_seh_filter_dll`** — both retained images import VCRUNTIME140.dll but the
  census does not itemize its startup names; the filter is bound for family
  completeness with zero observed rows, stated honestly).
- **Census-observed out-of-scope names return UNRESOLVED, never fake success**:
  `_initialize_narrow_environment` (crt_env family), `_errno` (crt_tls_errno
  family), `__stdio_common_vsprintf`/`__stdio_common_vfprintf`
  (api-ms-win-crt-stdio / stdio family). The binder's UNRESOLVED lets the
  import pass carry them to their owning families.
- **Exactly eight Win64 entry points**, verified as unedited win64cc IR from
  the FreeBSD9 frontend: `void @svc_initterm(ptr,ptr)`,
  `i32 @svc_initterm_e(ptr,ptr)`, `i32 @svc_seh_filter_dll(i32,ptr)`,
  `i32 @svc_crt_atexit(ptr)`, `void @svc_cexit()`,
  `i32 @svc_initialize_onexit_table(ptr)`,
  `ptr @svc_register_onexit_function(ptr,ptr)`,
  `i32 @svc_execute_onexit_table(ptr)`. errno_t/int and the Win32
  `unsigned long` exception code are 32-bit (`i32`; the owner takes
  `uint32_t` — a deliberate width fix over the prototype's host-`unsigned
  long`), `_onexit_t` return is pointer-sized. The binder itself stays
  native-C calling convention.
- **Onexit tables are caller-backed, fixed capacity** (documented divergence:
  the real UCRT grows them on the heap; this family never allocates). The
  owner-only `mb_win_crt_onexit_table_attach_storage(table, slots, capacity)`
  helper (not a Windows export) is what the entry adapter calls with the
  retained image's own storage; registration on an unbacked/full table fails
  explicitly (NULL); execution walks LIFO, clears each executed slot, empties
  the table, and a second execution is a no-op; re-attachment is refused while
  registrations are pending. Register-after-execute works (documented reuse
  model choice, same as the prototype's `_cexit` reuse note).
- **Single process-wide attach-window state** (IDLE / INIT_IN_PROGRESS /
  INIT_FAILED / INIT_COMPLETE): correct because the Windows loader
  serializes DLL_PROCESS_ATTACH across the process; matches the design and
  prototype. Failed attach ⇒ detach runs nothing (no terminators, no atexit).

## Exact commands and results

```
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-04-zcode-team/worker-crt-startup-implement/verify_crt_startup.py --host-only
  -> CRT_STARTUP_WIN64_PASSED
     receipts/9b33b20aa8fc453ab1c11fad0ecad8ed/receipt.json
python3 outputs/2026-10-04-zcode-team/worker-crt-startup-implement/verify_crt_startup.py --sanitize
  -> CRT_STARTUP_WIN64_AND_NATIVE_OBJECTS_PASSED
     receipts/df5bd96fc55c4d218beba264981ebb1e/receipt.json
python3 outputs/2026-10-04-zcode-team/worker-crt-startup-implement/verify_crt_startup.py --red-stub --host-only
  -> EXPECTED_ATTACH_STUB_REJECTION
     receipts/c05c5ef2db37470cb21b32b8ab159c9c/receipt.json
```

Intermediates preserved raw: `receipts/6b28d2a1…` FAILED (test harness bound
`_seh_filter_dll` through the runtime library before the vcruntime-home split;
fix is in the delivered test), `receipts/a7f0edf4…` FAILED (verifier Python
bug: `sum()` over `re.search` None; C tests had already passed in that run).

Host evidence — all three non-red runs print all nine PASS lines (host and
ASan/UBSan builds both exit 0):

1. `_initterm` order/null-skip/sentinel (synthetic tables, interior NULLs,
   `.z` terminator slot exclusive and inclusive).
2. `_initterm_e` error channel + stop-on-first-error (later entries not run).
3. `_seh_filter_dll` decision matrix: IDLE/COMPLETE → CONTINUE_SEARCH,
   IN_PROGRESS → EXECUTE_HANDLER exactly once and state leaves the window,
   second fault propagates, state stays INIT_FAILED.
4. atexit/`_cexit` LIFO order, capacity 32 then explicit rejection, NULL
   registration rejected, double-`_cexit` no-op, table reuse.
5. detach cleanup policy polarities (FAILED/IDLE refuse, COMPLETE allows).
6. Full attach/detach success scenario: `132+` (xi walk, xc walk, ctor
   registers atexit) then `42B` (pre-terminator, terminator, atexit LIFO last).
7. Full mid-init failure scenario: atexit registered before the fault never
   runs; attach returns -1; detach provably runs nothing.
8. Onexit-table family through binder-resolved pointers: initialize EINVAL +
   zeroing, unbacked registration fails, LIFO execution, executed slots
   cleared, double execution no-op, reuse after execute, capacity 4 then
   explicit NULL, re-attach refused while pending and allowed when empty.
9. Binder fold/ordinal/polarity + session lifecycle: ASCII-folded library,
   exact-fold name, ordinal UNRESOLVED, wrong library UNRESOLVED, out-of-scope
   observed names UNRESOLVED (env/errno/stdio fall-through), vcruntime/ucrtbase
   homes BOUND, ERROR polarity with output written only on BOUND
   (sentinel preserved), duplicate/taken-session/ACTIVE-end refusals, clean
   end + session reuse.

Negative control: attach-orchestrator stubbed to always return -1 → the typed
success-scenario assert rejects it (`EXPECTED_ATTACH_STUB_REJECTION`).

Native route evidence (unchanged ABI rule; SDK v0.43 `prospero-clang`, pinned
LLVM 21.1.8, no target execution): direct PS5 owner object `mb_win_crt_startup.o`
has **zero undefined imports** (freestanding: it places no demands on any SDK
library); bridge route FreeBSD9-frontend → unedited win64cc IR → PS5 backend
object references only the owner's eight `mb_win_crt_*` symbols, all defined by
the owner object; data layouts equal native vs bridge; layout probe
`[8,8,4,4,24,8,0,8,16,24,0,16]` (ptr, size_t, uint32, int32, onexit_table
size/align, first/last/end offsets, session size, module/active offsets) equal
in both routes; exactly eight win64cc definitions with the exact signatures
above; `target_resolution_proven: false` — **no target(PS5) support claim**.

## build.py wiring points for the controller (NOT edited by this worker)

`outputs/ps5-mod-bridge/windows-candidate/build.py`, mapping the
init_once/mutex integration pattern 1:1:

1. Promote the four C sources + test into `windows-services/` (they are
   pattern-identical residents; my copies exist only so history stays intact).
2. argparse (line ~26–27 pattern): add `--crt-startup`; guard
   `if a.crt_startup and not a.sync: p.error(...)` (line ~40 pattern).
3. Generated `owned_profile.h` (line 46): add
   `#define MB_OWNED_CRT_STARTUP`.
4. Source lists (lines 57–60 pattern):
   `native_c.append(SERV/'mb_win_crt_startup.c')`;
   `abi_c.append(SERV/'mb_win_crt_startup_abi.c')`.
5. Receipt `sources` list (line ~70 pattern): add `mb_win_crt_startup.h`,
   `mb_win_crt_startup_abi.h`, and the new fixture pair below.
6. Import library (lines 97–103 pattern): create
   `tests/worker-crtstartup-runtime.def` with
   `LIBRARY api-ms-win-crt-runtime-l1-1-0.dll` and EXPORTS `_initterm`,
   `_initterm_e`, `_crt_atexit`, `_cexit`, `_initialize_onexit_table`,
   `_register_onexit_function`, `_execute_onexit_table` (the LIBRARY line is
   what records the census-observed import-DLL name in the consumer fixture);
   `lld-link /lib /def:` → `crtstartup.lib`; link it into the consumer DLL
   (line ~115 `extra`). If a fixture later exercises the filter, add
   `tests/worker-crtstartup-vcruntime.def` (`LIBRARY vcruntime140.dll`,
   EXPORTS `_seh_filter_dll`) — zero census rows today, optional.
7. Consumer fixture (line ~109): pass `-DMB_OWNED_CRT_STARTUP=...`; new
   `tests/worker-crtstartup.h` on the `worker-initonce.h` pattern:
   `__declspec(dllimport)` declarations with the exact signatures above,
   static slot storage plus a static `__onexit_table_t`-shaped triple in the
   fixture (the retained image owns table storage — this family never
   allocates), and exercise register→execute LIFO + `_initterm` over a
   sentinel table in the worker fixture so the PE import census actually calls
   the imports. The orchestrators are not Windows exports: the retained
   bodies' own attach code calls `_initterm` directly, so `mb_win_entry`
   (line 229) is unchanged; the entry adapter's ordering contract (user
   DllMain attach only after the walks complete) is enforced by whoever calls
   `mb_win_crt_dll_process_attach`.
8. Receipt profile (line ~83 pattern): `crt_startup_profile` with
   `onexit_table_bytes: 24`, `actual_pe_imports: 7`.
9. win64cc census (line 222): add `'mb_win_crt_startup_abi': 8` to `expected`.
10. Host driver result assertions (lines ~177–178 pattern): require the
    `crt_startup` result key iff `--crt-startup`, forbid it otherwise.

## Open questions / honest divergences (no full MSVC parity claimed)

- Fixed-capacity tables (atexit 32; onexit tables = caller-provided slot
  count) vs the real UCRT's heap growth; no allocation is a family invariant.
- Table reuse after `_cexit`/`_execute_onexit_table` is the design's
  documented model choice, still pending verification against vendored UCRT
  sources (design wiring item 9); re-attachment while pending is refused by
  adapter policy.
- No real SEH delivery: `_seh_filter_dll` is the decision function only
  (delivery/unwinding belongs to the entry adapter; design wiring items
  4–5). A mid-walk fault therefore does not stop the current table's
  remaining initializers in the host model — real SEH would unwind past them.
- `_seh_filter_dll` is bound under `vcruntime140.dll` (documented home) with
  zero census rows; both retained images import VCRUNTIME140.dll but the
  census does not itemize its startup names — a future census addendum should
  confirm before any parity claim.
- `_initialize_narrow_environment`, `_errno`, `__stdio_common_*` are
  census-observed but belong to other families; they resolve UNRESOLVED here
  by design.
- Target (PS5) execution: NOT attempted and NOT claimed (controller-only).
  Host + sanitized evidence, native/bridge objects, and SDK-independent
  freestanding imports are component evidence only.

## Component status

- Host typed Win64 CRT-startup service: implemented and verified (incl.
  ASan/UBSan; red-stub negative control).
- Native PS5 owner + bridge objects: compiled with receipts recorded (SDK
  v0.43, LLVM 21.1.8 pins, unedited IR route, ABI rule intact, zero SDK
  import demands in the owner object).
- Retained-PE-runtime integration (build.py census entry, fixture exercise,
  target dispatch): pending controller action per the wiring points above.
