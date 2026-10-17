# DESIGN — CRT startup block for the retained-runtime model (backlog rank 4)

Status: DESIGN + HOST PROTOTYPE. Proposal-only. Not wired into build.py (which is
itself absent from this workspace). The exact observed retained-DLL symbol set is
PENDING on `worker-dll-mod-retention/baseline-dll-census.json`, which is not present
in this workspace (see CENSUS-GAP.md); nothing observed is fabricated below.

Evidence boundary up front: sections 1–2 transcribe **documented** UCRT/MSVC CRT
semantics from the published CRT source layout (`dll_dllmain.cpp`, `internal.cpp`,
`seh_filter_dll.cpp`). No vendored CRT sources or network were available to
re-verify line-level details in this workspace, so exact ordering notes are flagged
where confidence is lower. **No claim of MSVC parity beyond these documented
semantics** — see section 7 for the divergence list.

---

## 1. What a real MSVC DLL body expects at startup

A DLL built with MSVC has PE entry point `_DllMainCRTStartup`, not the user
`DllMain`. The CRT wrapper owns the process-attach/process-detach choreography so
that by the time user `DllMain(hinst, reason, lpv)` runs, C and C++ static
initialization has completed.

### 1.1 Initializer tables (linker-emitted, address-ordered, sentinel-terminated)

Each table is a pair of labels `.a` (begin) / `.z` (end) around a linker-sorted
run of function pointers, followed by a NULL terminator slot inside `.z`:

| Table | Element type | Walk | Meaning |
|---|---|---|---|
| `__xe_a..__xe_z` | `int (fn)(void)` | `_initterm_e` | error-capable C initializers (exe common startup; not part of UCRT DLL attach) |
| `__xi_a..__xi_z` | `int (fn)(void)` | `_initterm` | C initializers (return value ignored) |
| `__xc_a..__xc_z` | `int (fn)(void)` | `_initterm` | C++ dynamic initializers (global ctors, guard-registered) |
| `__xp_a..__xp_z` | `int (fn)(void)` | `_initterm` | pre-terminators (detach) |
| `__xt_a..__xt_z` | `int (fn)(void)` | `_initterm` | terminators (detach) |

The `.z` slots being NULL is what lets the walker be a blind sweep: it never needs
a count, it walks `[begin, end)` and skips NULL entries (linker padding can also
produce interior NULLs; they must be skipped, not treated as errors).

### 1.2 `_initterm` semantics (documented)

```
void _initterm(_PIFV *pfnBegin, _PIFV *pfnEnd):
    while pfnBegin < pfnEnd:
        if *pfnBegin != NULL: (**pfnBegin)()
        ++pfnBegin
```

- Strictly ascending address order of table slots; return values discarded.
- NULL slots (sentinels and linker padding) are skipped silently.
- No error channel: a failing initializer cannot stop the walk.

### 1.3 `_initterm_e` semantics (documented)

Same sweep, but the return value is an `errno_t` channel: the walk stops at the
**first non-zero return**, returns it to the caller, and later entries are not
run. All-zero returns yield 0. UCRT uses it in EXE common startup
(`__scrt_common_main_seh`) before the `xi`/`xc` walks; the UCRT DLL attach path
runs only `xi` then `xc`. We prototype both because retained images may reference
either entry point.

### 1.4 DLL_PROCESS_ATTACH sequence (UCRT `dll_dllmain.cpp` shape)

```
_DllMainCRTStartup(hinst, DLL_PROCESS_ATTACH, lpv):
    (on x86 only: _pei386_runtime_relocator — not applicable to our host model)
    __scrt_current_dll_state = INITIALIZING
    __try:
        _initterm(__xi_a, __xi_z)     // C initializers first
        _initterm(__xc_a, __xc_z)     // then C++ dynamic initializers
    __except (_seh_filter_dll(GetExceptionCode(), GetExceptionInformation())):
        return FALSE                  // init failed -> load fails; NO detach cleanup
    __scrt_current_dll_state = INITIALIZED
    return user_DllMain(hinst, DLL_PROCESS_ATTACH, lpv)   // raw entry invoked last
```

Key contract points:

- The SEH filter covers **only the CRT init window** (the two walks), not user
  `DllMain` and not later calls into the DLL.
- If the filter takes the handler, the load fails and process detach must **not**
  run terminators or atexit callbacks — those objects were never fully constructed.
- On success, user `DllMain` attach runs *after* all static init (this is the
  documented reason global-object state is usable inside `DllMain`).

### 1.5 `_seh_filter_dll` contract (documented behavior; host stub below)

```
int _seh_filter_dll(unsigned long exception_code, EXCEPTION_POINTERS *info)
```

- Returns `EXCEPTION_EXECUTE_HANDLER` (1) exactly when the exception occurred
  while CRT initialization of this DLL was in progress; the CRT then records
  init-failure so that a subsequent DLL_PROCESS_DETACH skips the cleanup walks.
- Returns `EXCEPTION_CONTINUE_SEARCH` (0) in every other state — exceptions after
  init completed, or a second exception once init already failed, propagate to
  other handlers/the OS.
- Never returns `EXCEPTION_CONTINUE_EXECUTION` (-1).
- On non-Windows hosts there is no SEH; our stub models the **decision state
  machine** only (state in, allow/deny out). Exception delivery/unwinding is out
  of scope for the host prototype.

### 1.6 DLL_PROCESS_DETACH sequence (UCRT shape)

```
DLL_PROCESS_DETACH:
    _initterm(__xp_a, __xp_z)   // pre-terminators
    _initterm(__xt_a, __xt_z)   // terminators
    _cexit()                    // atexit()/_onexit() table, LIFO, runs LAST
```

(Confidence note: that xp → xt → `_cexit()` ordering follows the UCRT source
layout; re-verify against vendored CRT sources before any parity claim.)

`atexit`/`_onexit` in a DLL push onto a runtime-owned table; the real table grows
dynamically. Registration is legal at any time after attach; execution is LIFO and
happens only in the detach path above (or at process exit via `_exit` for EXEs).

### 1.7 Loader-lock constraints (documented, binds admission below)

Code running at attach holds the loader lock: no `LoadLibrary` of new images, no
joining threads, no CRT file I/O that internally synchronizes on the loader, no
`atexit`-unsafe allocations. This constraint is the whole reason an admission
boundary exists between "must run at attach" and "can be deferred".

## 2. Mapping onto the retained-runtime model

Our model (per the task brief): retained DLL images whose MSVC CRT dependencies
are satisfied by a retained runtime we own, rather than by linking the real CRT.
The census (`baseline-dll-census.json`) is the authority for which CRT startup
symbols the retained images actually reference; it is PENDING (CENSUS-GAP.md).
The design therefore defines **decision rules per symbol role**, and the fill-in
table in 2.2 gets its rows from the census, not from this document.

### 2.1 Module placement

`crt_startup` (this prototype) is a pure, host-portable C11 module — no OS calls,
no allocation — implementing the walkers, the SEH-filter decision state machine,
the atexit table, and composed attach/detach orchestrators. It is the reference
implementation the entry adapter calls; it never touches the engine.

### 2.2 Admission boundaries (attach vs lazy first-call)

Decision rule, applied per census symbol:

- **Attach-time (eager)**: anything in the `xi`/`xc` initializer-table roles, and
  any CRT startup entry point itself (`_initterm`, `_initterm_e`, `_seh_filter_dll`,
  `_DllMainCRTStartup`-shaped adapters). These must run before user `DllMain`
  attach to preserve documented ordering, subject to loader-lock rules.
- **Lazy first-call (deferred)**: any retained image body whose side effects are
  not ordering-visible to other images (e.g. lazy singleton init exported as a
  function). Implement as a once-guard trampoline that, on first call, runs that
  image's deferred walk. Deferred work must not be reachable from any attach-time
  initializer (that would invert the ordering contract).
- **Never (host model)**: real SEH unwinding, `_pei386_runtime_relocator`
  (x86-only; our loader handles relocations its own way), real loader-lock
  arbitration.

PENDING admission table (rows to be generated mechanically from the census —
see CENSUS-GAP.md for the exact fill-in recipe; symbols below are attributed to
the task prompt, NOT to an observed census):

| Census symbol (role) | Source | Admission | Runs via |
|---|---|---|---|
| `_initterm` | task prompt (census PENDING) | attach-time, eager | `crt_dll_process_attach` → `crt_initterm` (xi then xc) |
| `_seh_filter_dll` | task prompt (census PENDING) | attach-time, eager (installed around the walks only) | `crt_seh_filter_dll` stub |
| `<census row n>` | census | apply 2.2 decision rule | map per table role |

### 2.3 Failure and teardown semantics in our model

- Attach failure: `crt_dll_process_attach` returns -1, state is `INIT_FAILED`,
  and `crt_dll_process_detach` deliberately does nothing (no terminators, no
  atexit) — matching 1.4/1.6.
- Detach: terminators first, atexit LIFO last (1.6). Honest caveat for the
  retained runtime: if the host process never unloads retained images, detach may
  never run; registrations are then effectively unreachable. The entry adapter
  must state which case holds; the prototype supports both honestly.
- `_cexit` reuse: real CRT behavior on table reuse after `_cexit` is a
  verify-against-sources item; our model resets the table and allows reuse
  (documented divergence until verified).

## 3. Composition with existing owners

Honest note: neither owner named in the task brief was found in this workspace
(no entry-adapter source, no workers-admission module, no build.py, no census).
The composition below is specified against the interfaces as named in the task
brief and is ready to be bound when those owners' outputs land; nothing is
claimed integrated.

- **Entry adapter** (proposed owner of the attach choreography): calls
  `crt_dll_state_set(INIT_IN_PROGRESS)` implicitly via `crt_dll_process_attach`,
  which runs the xi→xc walks inside the `_seh_filter_dll` window and only then
  invokes the user/retained `DllMain` body. On -1 it fails the load without
  touching user code. The adapter owns exception *delivery* (SEH on Windows, a
  translation shim on the host); `crt_startup` owns only the filter *decision*.
- **Workers admission** (the studio's dispatch/admission contract): this
  deliverable is proposal-only host code under my directory; it composes by
  staying out of the engine lane. If the census lands under the workers-admission
  packet, fill 2.2 and hand the wiring list (section 5) to the entry-adapter
  owner. Per the retained verifier exception, any engine-side execution of these
  tests later would need the scoped verifier; this prototype needs no such
  exception (pure host C, run once locally for this report).

## 4. Host prototype deliverables (this directory)

- `crt_startup.h` / `crt_startup.c`: `crt_initterm`, `crt_initterm_e`,
  `crt_seh_filter_dll` (stub state machine), `crt_atexit`/`crt_cexit`
  (fixed-capacity LIFO table, cap divergence documented),
  `crt_dll_process_attach`/`crt_dll_process_detach` orchestrators.
- `test_crt_startup.c`: synthetic function-pointer tables with interior NULLs and
  terminator sentinel slots; asserts ordering, NULL-skipping, sentinel safety,
  error-stop for `_initterm_e`, filter decision matrix, detach-cleanup policy,
  atexit LIFO + cap + reuse, and two full attach/detach scenarios (success and
  mid-init failure).
- `build_and_test.sh`: reproducible `cc -std=c11 -Wall -Wextra -Werror -pedantic`
  build + run.

## 5. Wiring list for later (NOT done here; build.py untouched)

1. Land `baseline-dll-census.json`; mechanically emit the 2.2 admission table
   from the observed symbol set (script + director review of the mapping).
2. Bind `crt_startup` into the entry adapter: replace its attach hook body with
   `crt_dll_process_attach(xi, xc)` + post-success user-DllMain call.
3. Emit real initializer tables from the retained images (address-ordered arrays
   with `.a`/`.z` sentinel slots) at build time; reject any table without its
   terminator slot.
4. Add the host exception-translation shim so non-Windows faults enter
   `crt_seh_filter_dll` with a mapped code; document unmapped codes as
   CONTINUE_SEARCH.
5. Route the adapter's load-failure path through `INIT_FAILED` so detach cleanup
   is provably skipped (regression-test both polarities).
6. Bridge the DLLs' `atexit`/`_onexit` imports to `crt_atexit`; decide and
   document the never-detach case explicitly in the runtime docs.
7. Implement lazy-first-call trampolines for census rows admitted as deferred
   (once-guard + deferred walk); assert no deferred row is reachable from any
   eager initializer.
8. Port `test_crt_startup.c` into the host test lane as standing regression
   tests; keep engine build untouched.
9. Re-verify 1.6 ordering and `_cexit` reuse against vendored UCRT sources; fold
   corrections into this design before any parity claim.
10. Only after 1–9: touch build.py — add `crt_startup.c` compilation, table
    emission, and an export check that every census-observed startup symbol
    resolves; gate any engine-side execution on the existing scoped verifier.

## 6. Verified in this pass

Host prototype semantics only, on this Mac with clang via `cc` (see REPORT.md
transcript): walker order/NULL-skip/sentinel behavior, `_initterm_e` error-stop,
filter decision matrix, detach policy, atexit LIFO/cap/reuse, both scenarios.
Nothing engine-side was executed.

## 7. Honest divergence list (no MSVC parity claimed beyond documented semantics)

1. No real SEH: the stub is a decision function; no unwinding, so a modeled
   mid-walk failure does not stop the current table's remaining initializers
   (real SEH would unwind past them). Wiring item 4/5 must add the per-call guard.
2. atexit table is fixed-capacity (32) and host-only; real CRT grows dynamically.
3. `_cexit` reuse semantics are a model choice pending source verification.
4. No `_pei386_runtime_relocator`, no TLS callbacks, no `DllMain` detach-reason
   matrix (thread attach/detach) modeled.
5. Ordering details transcribed from documented CRT layout, not re-verified
   against vendored sources in this workspace (no network).
