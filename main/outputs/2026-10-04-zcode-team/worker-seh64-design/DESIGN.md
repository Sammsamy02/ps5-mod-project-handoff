# DESIGN.md — SEH64 structured exception handling family

Worker: `worker-seh64-design` · Date: 2026-10-04 · Mode: **design-only, no implementation**

Status honest labels: this is a design derived from the retained census, the
integrated CRT startup, the windows-execution loader contracts and the
RUNTIME_MAP execution model. Nothing here has been implemented, compiled or
verified. No ">8 quality" and no Windows-parity claim is made.

---

## 0. Executive summary

The observed census needs a **much smaller** SEH64 surface than the full
Windows dispatcher: **10 imported symbols across two DLLs**, of which one
(`_seh_filter_dll`) is already owned by the CRT-startup family and four are
logic-only context/unwind query APIs. The genuinely hard parts — the two-pass
`RtlUnwindEx` engine, the personality-invocation trampolines, and the
signal→exception translation shim — are *internal* substrate that the census
never names directly but that `__C_specific_handler` cannot function without.

Minimal viable subset (Section 6): `RtlCaptureContext`,
`RtlLookupFunctionEntry`, `RtlVirtualUnwind`, `__C_specific_handler`,
`SetUnhandledExceptionFilter`, the `_seh_filter_dll` delivery wiring, plus the
internal `RtlUnwindEx`/dispatch engine and a bridge-frame function-table
slice. Everything else (callback tables, vectored handlers, C++ EH
personalities, stack-overflow support) is optional or belongs to the separate
C++ EH family.

**Contract-breaking fact that drives the architecture:** the current loader
admission (`windows-execution/pe_module.h`) explicitly refuses images with
unwind directories — *"No TLS/unwind/CFG/CLR support"* and *"No
exceptions/longjmp may cross this boundary."* SEH64 is precisely the family
that amends that contract; until it lands, every real mod DLL in the census
(DLLs whose CRT init code contains `__try/__except(_seh_filter_dll)`) is
unadmittable or crash-vulnerable at DllMain time.

---

## 1. Observed census (exact, from both censuses)

Source: `outputs/2026-10-04-zcode-team/worker-dll-mod-retention/
baseline-dll-census.json` and `ue4ss-zip-census.json` (identical SEH sets);
cross-checked against `worker-modreqs/MOD-CENSUS.md` and
`SERVICES-BACKLOG.md` §8.

| DLL | Library | Symbol(s) | Family relevance |
|---|---|---|---|
| both | VCRUNTIME140.dll | `__C_specific_handler` | SEH64 personality (scope tables) |
| both | VCRUNTIME140.dll | `_CxxThrowException` | C++ EH family (raise path only) |
| both | VCRUNTIME140_1.dll | `__CxxFrameHandler4` | C++ EH family (EH5 bytecode) |
| UE4SS only | VCRUNTIME140.dll | `__CxxFrameHandler3` | C++ EH family (EH3/4 FuncInfo) |
| both | api-ms-win-crt-runtime-l1-1-0.dll | `_seh_filter_dll` | **already owned** by CRT-startup family |
| UE4SS only | KERNEL32.dll | `RtlCaptureContext` | SEH64 context capture |
| UE4SS only | KERNEL32.dll | `RtlLookupFunctionEntry` | SEH64 .pdata lookup |
| UE4SS only | KERNEL32.dll | `RtlVirtualUnwind` | SEH64 unwind interpreter |
| UE4SS only | KERNEL32.dll | `SetUnhandledExceptionFilter` | SEH64 last-chance handler |
| UE4SS only | KERNEL32.dll | `SetErrorMode`, `SetThreadStackGuarantee` | adjacent state owners (P3, small) |
| UE4SS only | MSVCP140.dll | `?ReportUnhandledError@_ExceptionHolder@...` | C++ std detail, not SEH64 |

**Not observed** (and therefore not required by the census, despite being part
of a "full" SEH64): `RtlAddFunctionTable`, `RtlDeleteFunctionTable`,
`RtlInstallFunctionTableCallback`, `RtlAddGrowableFunctionTable`, `RtlUnwind`,
`RtlUnwindEx`, `RaiseException`, `RtlRaiseException`,
`AddVectoredExceptionHandler`, `AddVectoredContinueHandler`,
`UnhandledExceptionFilter` (the callable one), `RtlUnwind`-family anything.

This is expected: on real Windows those live in ntdll and are invoked by the
*dispatcher*, not by user images. They are **internal substrate** of our
runtime: `__C_specific_handler` is specified as calling `RtlUnwind`, and our
dispatcher plays the OS role. Only `RtlAddFunctionTable` is promoted from
"not observed" to "needed internally" — see §5a: our own win64cc bridge code
has no `.pdata` and must register synthetic tables so unwinding can cross it.

---

## 2. Existing SEH surface (current state)

`outputs/2026-10-04-zcode-team/worker-crt-integrate/vendor/windows-services/
mb_win_crt_startup.c` — `mb_win_crt_seh_filter_dll(uint32_t code, void *info)`:

- A **decision state machine only**. While `g_dll_state ==
  INIT_IN_PROGRESS`: first fault → state `INIT_FAILED`, return
  `EXCEPTION_EXECUTE_HANDLER` (1). Everything else → `CONTINUE_SEARCH` (0).
  Never returns `CONTINUE_EXECUTION` (-1). `code` and `info` are ignored; no
  `EXCEPTION_POINTERS` is recorded.
- The crt-startup-design DESIGN.md explicitly deferred delivery:
  *"Exception delivery/unwinding is out [of scope]"*, and listed two follow-up
  items this design adopts: (1) a host exception-translation shim that feeds
  mapped codes into the filter; (2) a per-call guard because the current stub
  never unwinds, so a modeled EXECUTE_HANDLER would resume into nonsense if
  wired to real delivery without the `__except` body jump.
- Boundary contracts that SEH64 must change, currently absolute:
  - `windows-execution/pe_module.h`: images with unwind directories not
    admitted; *"No exceptions/longjmp may cross this boundary"* (twice).
  - `windows-services/mb_win_entry.h`: *"No exception/longjmp may cross this
    call boundary; this bridge provides no exception handling."*
  - `windows-services/README.md`: *"There is no exception handling in this
    bridge."*

---

## 3. Runtime model constraints (why this design looks the way it does)

From `outputs/ZCODE-HANDOFF-2026-10-04/RUNTIME_MAP.txt`:

- Real AMD64 PE bytes are mapped, relocated and executed natively (PS5 = x86-64).
  **The PE's own `.pdata`/`.xdata` bytes are present and valid** in the mapped
  image — the unwind data is real, so `RtlLookupFunctionEntry`/
  `RtlVirtualUnwind` operate on genuine MSVC-emitted data, not a re-model.
- Bridges/services are compiled by the pinned route: FreeBSD9 frontend →
  **unedited win64cc LLVM IR → PS5 backend**. `win64cc` fixes the calling
  convention but the object is ELF: **LLVM emits Itanium `.eh_frame`, not
  `.pdata`** for bridge frames. Consequence: bridge frames are invisible to a
  Windows unwinder unless tables are registered for them (§5a).
- Native host/service C++ outside the win64cc route uses SysV/Itanium EH and
  cannot be SEH64-unwound at all (§8, hard problem H4).
- The RUNTIME_MAP ABI rule — *"unwind/personality must follow the pinned
  SDK/ms_abi route; do not fake"* — forbids pseudo-personalities (e.g.
  intercepting scope tables and interpreting them in a host loop instead of
  invoking real personalities under win64cc). Personalities must be real
  ms_abi functions callable from the unwinder with the documented Win64
  register/stack contract.
- Host dev tests run as pure freestanding C on macOS (ARM64 — cannot execute
  AMD64 at all). So: **all data-plane logic (parsing, opcode interpreter,
  scope-table decisions, filter dispatch order) is host-testable as pure
  functions over synthetic `CONTEXT`/byte fixtures; all control-plane
  (trampolines, resume, signal shim) is verifiable only on the native win64cc
  route.** The design is shaped so the testable surface is maximal.

---

## 4. Architecture

### 4.1 Family owner shape (house pattern)

Follows the `worker-4-win-services` / crt-startup owner pattern:

```
windows-services/mb_win_seh64.h / .c        pure logic owner (freestanding:
                                              no OS calls, no allocation, no libc)
windows-services/mb_win_seh64_abi.h / .c    win64cc bridge + ms_abi binder table
windows-services/tests/test_win_seh64.c     host typed-ABI + layout + fixture tests
windows-services/verify_seh64.py            verifier (host ASan/UBSan + win64cc IR
                                              route + red-stub controls), per house
```

Pure-logic owner (host-testable, no assembly):

- `UNWIND_INFO` decoder + `UNWIND_CODE` opcode model
- epilogue recognition / prologue replay = `RtlVirtualUnwind` core
- `RtlLookupFunctionEntry` search over a **function-table registry**:
  1. mapped PE module exception directory (image base + pdata range supplied
     by the pe-module owner at admit time),
  2. dynamic tables (`RtlAddFunctionTable`-shaped registry, §5a),
  3. (optional, phase 4) callback tables.
  Lookup is image-range-checked; a `ControlPc` outside all ranges returns
  `NULL` (real Windows semantic: unwinding stops).
- `__C_specific_handler` scope-table walk and filter/handler selection
  decisions — as a *decision core* the bridge personality calls into.
- dispatch driver (the `KiUserExceptionDispatcher` equivalent): two-pass
  frame walk, exception-record propagation, unhandled-filter invocation —
  parameterized by an injected `resume_context` callback so host tests can
  drive a full dispatch without signals.
- per-thread sidecar state (house sidecar pattern): unhandled filter pointer,
  error-mode bits, stack-guarantee value, dispatch depth for nested-exception
  detection and unwind-collision guards.

win64cc bridge (`_abi`), the only assembly-adjacent part:

- `RtlCaptureContext` stub (ms_abi, captures caller's regs into `CONTEXT`).
- Personality entry points: `__C_specific_handler` (exported, binder target),
  plus `RtlpExecuteHandlerForException`/`...ForUnwind`-equivalent guard
  trampolines that invoke a filter/handler **as if called from the
  establishing frame** (§8, H2).
- `RtlUnwindEx` trampoline: applies a target `CONTEXT` to the machine
  (`NtContinue`-equivalent, minus syscalls) — implemented as IR-level
  register restore + indirect jump, win64cc, no MS-ABI erasure.
- Signal-shim glue (native target only): converts `ucontext_t` → `CONTEXT`
  and re-arms return; FreeBSD and macOS-host variants behind one interface.

### 4.2 Ownership boundaries

- `_seh_filter_dll` stays owned by the **CRT-startup family**. SEH64 does not
  re-implement it; the binder routes the api-ms-win-crt-runtime import to the
  existing owner and SEH64 merely delivers `(code, EXCEPTION_POINTERS*)`
  into it. (Duplicate ownership would violate the one-owner house rule.)
- `__CxxFrameHandler3/4`, `_CxxThrowException`, `RaiseException` internals →
  **C++ EH family** (backlog §8 second half). SEH64 provides only the
  substrate contracts: dispatch, unwind passes, `RaiseException`-equivalent
  record construction, and the documented personality calling convention the
  C++ handlers plug into. This design fixes those interfaces, not the C++
  data formats.
- `SetErrorMode` / `SetThreadStackGuarantee` → tiny adjacent state owners in
  the SEH64 sidecar (phase 4; they are census-observed but crash-path-only).

### 4.3 Data contracts (widths are contracts, house rule)

- `CONTEXT`: exact Win64 layout — 16-byte aligned, 0x4D0 bytes;
  `ContextFlags`/`MxCsr`, `SegCs..SegSs` (6×16-bit), `EFlags`, `Dr0–Dr7`,
  `Rax..R15` (offset 0x78…0xF8), `Rip` at 0xF8, `FltSave` XSAVE area at 0x100,
  `VectorRegister`. `_Static_assert` offsets in owner and ABI TUs, compared in
  the native/bridge layout probe like InitOnce/SRW receipts do.
- `RUNTIME_FUNCTION` {`BeginAddress`, `EndAddress`, `UnwindData`} — 3×u32.
- `UNWIND_INFO` header: Version(3)/Flags(5), SizeOfPrologue, CountOfCodes,
  FrameRegister:4/FrameOffset:4; unwind codes 2 bytes each, array padded to
  even count; then chained `RUNTIME_FUNCTION` RVA (`UNW_FLAG_CHAININFO`) or
  personality RVA + handler data.
- `EXCEPTION_RECORD` {`ExceptionCode` u32, `ExceptionFlags` u32,
  `ExceptionRecord` ptr, `ExceptionAddress` ptr, `NumberParameters` u32,
  `ExceptionInformation[15]` u64} — 0x98 bytes for x64.
- `DISPATCHER_CONTEXT` {`ControlPc`, `FunctionEntry`, `EstablisherFrame`,
  `ContextRecord`, `HandlerData`, `HistoryOracle` (literal 1 in real
  ntdll), `ScopeIndex`, `ControlPcIsUnwinding:1`, `NonvolatileContextPointerThreshold`,
  ...} — versioned cautiously: **we define our own binary layout but the
  fields the personalities consume (`ControlPc`, `FunctionEntry`,
  `EstablisherFrame`, `ContextRecord`, `HandlerData`, `ControlPcIsUnwinding`)
  are position-contract with real VCRUNTIME personalities**. This matters
  because a real MSVC-compiled PE never hands us `DISPATCHER_CONTEXT` memory
  itself (the unwinder builds it), so only self-consistency is required —
  but `__C_specific_handler`'s *parameter semantics* are not negotiable.
- Filter/handler return codes: -1/0/1 (`CONTINUE_EXECUTION`/`CONTINUE_SEARCH`/
  `EXECUTE_HANDLER`); `ExceptionContinueExecution/Search/Handler` = 0/1/2
  personality returns; unwind flags `EXCEPTION_UNWINDING(2)`,
  `EXCEPTION_EXIT_UNWIND(4)`, target-unwind `EXCEPTION_TARGET_UNWIND(8)`,
  `EXCEPTION_NONCONTINUABLE(1)`, `COLLIDED_UNWIND(0x20)`.

---

## 5. Subsystem designs

### 5a. RUNTIME_FUNCTION table registration

**Census status: not imported by either DLL; required internally.**

- Public exports `RtlAddFunctionTable`/`RtlDeleteFunctionTable` (dynamic
  registry: base address, entry list, count; image-relative RVAs; O(1)
  insert, list scan + binary search within a table at lookup). Deleted
  tables: house lease discipline — a table whose module was released must be
  removed at release time or lookup refuses (no dangling reads).
- `RtlInstallFunctionTableCallback` / `RtlAddGrowableFunctionTable`:
  **phase 4+ optional**, not census-required; design reserves lookup slot 3.
- The driving internal need: **bridge frames have no `.pdata`** (ELF win64cc).
  Options assessed:
  1. *Build-time `.pdata` synthesis* — a codegen step emits hand-written
     `RUNTIME_FUNCTION`+`UNWIND_INFO` blobs for the (small, enumerable) set
     of win64cc bridge functions that can sit between a fault and its
     handler: `mb_win_entry` invoke, service binder shims on the DllMain
     path, the personality/unwind trampolines themselves. Pros: real unwind
     data, honest. Cons: must be kept in sync with codegen; only enumerably
     many frames.
  2. *LLVM-driven unwind emission* — make the PS5 backend route emit x64
     Windows unwind info. Rejected: violates "unedited win64cc LLVM IR" and
     means editing the pinned toolchain route.
  3. *Terminal-frame policy* — frames without registered info stop the walk
     (real Windows treats missing unwind info as fatal). Mandatory as the
     **fallback**, optional as the sole mechanism.
  **Decision:** (1) for the enumerated DllMain chain, (3) as global fallback
  with a distinct diagnostic (`MB_SEH_NO_UNWIND_INFO`), never silently
  swallowed. `RtlAddFunctionTable` is therefore implemented in phase 3 even
  though no DLL imports it.

### 5b. Unwind info parsing (`UNWIND_INFO`, opcodes)

Pure data plane, exhaustively host-testable.

- Opcode set to implement: `UWOP_PUSH_NONVOL(0)`, `UWOP_ALLOC_LARGE(1)` (2
  slots if size ≥ 0x8000... precisely: 1 slot if < 0x817F wait — codified as:
  1 slot when next slot < 0x8000 else 2), `UWOP_ALLOC_SMALL(2)`,
  `UWOP_SET_FPREG(3)`, `UWOP_SAVE_NONVOL(4)`, `UWOP_SAVE_NONVOL_FAR(5)`,
  `UWOP_SAVE_XMM128(8)`, `UWOP_SAVE_XMM128_FAR(9)`, `UWOP_SAVE_XMM(6/7)`
  (legacy, 64-bit-only images: reject), `UWOP_PUSH_MACHFRAME(10)` (interrupt
  frames — parse, only needed for kernel trampolines; host tests only).
- Flags: `UNW_FLAG_NHANDLER/EHANDLER/UHANDLER/CHAININFO`; chain resolution
  with a depth limit (real Windows allows chains; we cap and refuse loops).
- `RtlVirtualUnwind(HandlerType, ControlPc, FunctionEntry, ContextRecord,
  DispatcherContext*, ContextPointers*)` three-region rule:
  - `ControlPc` inside a recognizable **epilogue** (patterns:
    `add rsp,imm / pop reg* / ret|jmp`): pop registers in reverse, add RSP;
  - inside **prologue** (per SizeOfPrologue): replay unwind codes whose
    prologue offset is ≥ distance from start;
  - in **body**: restore RSP from FrameRegister/FrameOffset if SET_FPREG,
    else from the context's RSP already advanced by epilogue emulation of
    the *next* frame boundary — precisely: body means registers restored
    from `ContextPointers` snapshot by replaying all codes, RSP unchanged.
- Nonvolatile registers saved where the codes say (ContextPointers records
  homes so personalities can read caller locals — `__C_specific_handler`
  filters need this).
- MSVC-specific epilogue splitting (single `ret` counted as epilogue) and
  `SizeOfPrologue==0` leaf functions (RSP-only: RSP += 8, done).
- **Honest scope cut:** JIT-style `UNWIND_INFO` with dynamic handler data,
  `UWOP_PUSH_MACHFRAME` real dispatch, and OPTIL are out of scope.

### 5c. Exception dispatch (`__C_specific_handler`, scope tables, try/except)

- Scope table (at `DispatcherContext->HandlerData`, an image-relative RVA —
  note: for `__C_specific_handler` MSVC emits RVA-relative handler data
  inside the image; the bridge resolves via image base):
  `{ ULONG Count; SCOPETABLE_ENTRY[] }`, entry =
  `{BeginAddress, EndAddress, HandlerAddress(filter), JumpTargetAddress}`.
  Semantics:
  - `HandlerAddress == 1` → `__finally` style: no filter; during unwind pass
    the body at `JumpTargetAddress`... (precisely: `JumpTargetAddress == 0`
    for `__finally`; the body executes on the unwind pass).
  - filter called as `int filter(EXCEPTION_POINTERS*)` in the frame's
    context; returns -1/0/1.
  - `__except` with constant filter emits filter = the constant (0/1);
    `__except(expression)` emits real filter code.
- Personality contract (`__C_specific_handler(ExceptionRecord,
  EstablisherFrame, ContextRecord, DispatcherContext)` →
  `EXCEPTION_DISPOSITION`):
  - **Search pass** (`ExceptionFlags == 0`): for scopes covering
    `DispatcherContext->ControlPc`, invoke filters innermost-first. 0 → next
    scope; 1 → select this scope: set `ExceptionRecord->
    ExceptionFlags |= EXCEPTION_TARGET_UNWIND`, record `JumpTargetAddress`
    in `DispatcherContext->ScopeIndex`-adjacent slot (our own storage —
    real implementations pass it via the dedicated "target frame PC" in
    `RtlUnwindEx`'s `TargetIp`), then invoke `RtlUnwind(EstablisherFrame,
    target-ip-of-__except-body, ExceptionRecord)`. -1 → return
    `ExceptionContinueExecution` (noncontinuable check first:
    `EXCEPTION_NONCONTINUABLE` → raise `STATUS_NONCONTINUABLE_EXCEPTION`).
  - **Unwind pass** (`EXCEPTION_UNWINDING` set): execute `__finally` bodies
    (and the selected `__except` body at the target frame via TargetIp)
    for scopes covering ControlPc below the target, then return
    `ExceptionContinueSearch`.
- Dispatch driver (OS-role): pass 1 — walk frames fault→root via
  `RtlVirtualUnwind`, calling each personality with `EHANDLER`; if any
  returned `ExceptionContinueHandler`/requested unwind → pass 2 — repeat the
  walk calling personalities with `EXCEPTION_UNWINDING` until the target
  frame, then transfer control to `TargetIp` with the unwound context
  (trampoline restore). No personality found anywhere → unhandled path (§5e).
- `__try/__except` inside the PE's CRT startup (`__scrt` init window): the
  fault is in PE frames only; the walk never needs bridge tables in this
  path. This is the *primary acceptance scenario* (§7 phase 3): init fault →
  our `__C_specific_handler` → PE's `_seh_filter_dll` (crt-startup owner) →
  `EXECUTE_HANDLER` → unwind → `__except` body → DllMain returns FALSE
  cleanly.
- Nested exceptions (fault inside a filter/handler): recursion of the
  dispatcher is legal on x64 (all state is on the machine stack); the
  sidecar depth counter exists for diagnostics and unwind-collision
  (`EXCEPTION_COLLIDED_UNWIND` on re-entering an active target frame —
  real Windows semantic; phase 3 diagnostics, phase 4 full).

### 5d. `RtlUnwind` / `RtlUnwindEx`

- `RtlUnwind(TargetFrame, TargetIp, ExceptionRecord, ReturnValue)` = legacy
  wrapper over `RtlUnwindEx`; implement as such (thin).
- `RtlUnwindEx(TargetFrame, TargetIp, ExceptionRecord, ReturnValue,
  OriginalContext, HistoryOracle, ...)` engine:
  1. Validate noncontinuable collision (`TargetFrame` already in an active
     unwind → `COLLIDED_UNWIND`).
  2. Search pass from `OriginalContext` toward `TargetFrame`: personality
     calls with clean flags (lets C++ EH register catch targets later; for
     pure SEH it is nearly a no-op).
  3. Unwind pass: per frame, `RtlVirtualUnwind(NHANDLER, ...)` then
     personality with `UNWINDING`; personality runs `__finally` bodies by
     **invoking them at the establishing frame** via the guard trampoline
     (§8 H2); frame advance = context produced by `RtlVirtualUnwind`.
  4. At `TargetFrame == EstablisherFrame`: restore `CONTEXT` with
     `Rip = TargetIp`, `Rax = ReturnValue` — via the win64cc
     continue-trampoline. If `TargetFrame` falls off the stack root before
     matching: `STATUS_UNWIND` fault semantics (fatal, by design).
- **Both functions are internal substrate** (no census import). Exposed in
  the bridge only for the C++ EH family and our dispatcher. Do not bind them
  as PE imports.

### 5e. `SetUnhandledExceptionFilter` / `UnhandledExceptionFilter`

- Per-session sidecar: one filter pointer (real Windows: per-process; our
  sessions are the process model — house session pinning).
- `SetUnhandledExceptionFilter(new)` returns the previous pointer; NULL
  clears. SetErrorMode-like concurrency: single init thread per house
  entry rules; document as unsynchronized otherwise.
- `UnhandledExceptionFilter(EXCEPTION_POINTERS*)` (internal; also bindable
  later if a census demands): first-call semantics, `SEM_NOGPFAULTERRORBOX`
  interaction with `SetErrorMode` (suppresses the *imaginary* WER dialog —
  in our runtime there is no dialog; the bit only decides whether we re-raise
  to the host for its native crash reporting), then:
  - filter returns `EXCEPTION_EXECUTE_HANDLER` → terminate the session the
    Windows way: route to exit-owner (`ExitProcess`-equivalent) with mapped
    exit code.
  - returns 0 / no filter → **re-raise to the host** as the original signal
    with default disposition (so the PS5/host crash reporting sees a real
    crash instead of a swallowed one). This is the anti-fake anchor: we never
    pretend a crash was handled.
- Where it is called from: the dispatch driver when pass 1 finds no handler
  and the frame walk reaches the root. UE4SS's crash logger (the reason it
  imports this) then works: it installs a filter and gets real
  `EXCEPTION_POINTERS` with a real Windows `CONTEXT` (captured/converted by
  our shim) and can symbolize via `RtlCaptureContext`-based walking using
  `RtlLookupFunctionEntry`/`RtlVirtualUnwind` — note UE4SS's imports form a
  **self-consistent stack-walker kit**, which is exactly what our design
  must make true, not just bindable.

---

## 6. Minimal viable subset (census coverage)

Tiered by "makes the observed imports *semantically true*" vs "bindable only":

**M0 — make imports bindable (no delivery):** `CONTEXT` layout +
`RtlCaptureContext`; registry + `RtlLookupFunctionEntry`; `UNWIND_INFO`
parser + `RtlVirtualUnwind`; `SetUnhandledExceptionFilter` (store/filter
state owner). All four UE4SS KERNEL32 imports bind and behave for their
*non-crash* uses (stack walking works fully — it is pure data plane).
Effort: M. Verifiable: exhaustively on host.

**M1 — make the observed SEH control flow real (PE-internal):**
`__C_specific_handler` + scope tables; `RtlUnwindEx` engine; dispatch driver
over injected contexts; `_seh_filter_dll` delivery wiring (crt-startup owner
unchanged). Acceptance scenario: fault inside PE init → swallowed →
DllMain FALSE. Effort: L. Host-verifiable with synthetic fixtures; native
route for trampolines.

**M2 — make real faults arrive:** signal→exception shim (FreeBSD primary,
macOS-host test variant), resume path, unhandled-filter invocation +
re-raise policy, `RtlAddFunctionTable`-shaped registry filled with the
enumerated bridge-frame tables, `MB_SEH_NO_UNWIND_INFO` fatal fallback.
Effort: M/L, highest native-route verification burden.

**Deliberately NOT in the minimal subset:** `RtlInstallFunctionTableCallback`,
`RtlAddGrowableFunctionTable`, vectored handlers, `RaiseException` export,
C++ EH personalities (separate family), stack-overflow dispatch,
`EXCEPTION_COLLIDED_UNWIND` full semantics, `SetErrorMode`/
`SetThreadStackGuarantee` real effects (state owners only).

Coverage after M2: **10/10 census SEH symbols bound and semantically real**
(`_seh_filter_dll` via existing owner; `SetErrorMode`/
`SetThreadStackGuarantee` as honest state owners with documented no-op
effects), with the C++ EH census symbols (`__CxxFrameHandler3/4`,
`_CxxThrowException`) explicitly out of SEH64 scope.

---

## 7. Phased implementation plan

Each phase = distinct worker packet IDs, bounded tasks, own verifier receipt;
phases are independent-admissible in order; no phase claims engine-side
acceptance without the native win64cc route.

- **Phase 0 — contracts.** `mb_win_seh64.h`: `CONTEXT`/`RUNTIME_FUNCTION`/
  `UNWIND_INFO`/`EXCEPTION_RECORD`/`DISPATCHER_CONTEXT` layouts with
  `_Static_assert`s; error codes; sidecar struct. Layout probe test like the
  InitOnce receipts. No behavior. Exit: layout probe passes host + bridge.
- **Phase 1 — data plane.** Registry, `RtlLookupFunctionEntry`,
  `UNWIND_INFO` decoder, `RtlVirtualUnwind`, `RtlCaptureContext` (logic
  model on host; asm stub on bridge). Fixtures: extracted `.pdata`/`.xdata`
  byte ranges from the two census PEs (retained originals — read-only
  inputs) + synthetic edge cases (chained info, every opcode, leaf, epilogue
  split). Exit: fixture suite green; UE4SS stack-walker kit demonstrably
  works over a synthetic thread stack.
- **Phase 2 — decision plane.** Scope tables, `__C_specific_handler`
  decision core, dispatch driver over injected resume-callback,
  `RtlUnwindEx` pass structure in pure form. Tests: full dispatch scenarios
  with fake frames (fault→filter 0→filter 1→finally chain→body;
  noncontinuable; nested filter fault). Exit: dispatch scenarios green on
  host; `_seh_filter_dll` delivery unit test with the crt-startup owner.
- **Phase 3 — control plane (native).** Personality guard trampolines,
  continue-trampoline, signal shim (FreeBSD first), bridge-frame synthetic
  tables for the DllMain chain, `RtlAddFunctionTable` registry integration,
  unhandled filter + host re-raise. End-to-end on the win64cc/PS5 route:
  (a) PE init fault swallowed via `_seh_filter_dll`; (b) UE4SS crash-filter
  receives real `EXCEPTION_POINTERS` then host sees the crash on re-raise;
  (c) red-stub controls: dispatch with no tables refuses loudly. Exit:
  verifier receipts per house rules; **no engine/game-run claims**.
- **Phase 4 — optional surface.** Callback/growable tables, vectored
  handlers, stack-overflow guard-stack dispatch, `COLLIDED_UNWIND`,
  `SetErrorMode`/`SetThreadStackGuarantee` effects. Only on demand, each
  with its own bounded packet.
- **Out of family:** C++ EH (EH3/EH4 FuncInfo + EH5 bytecode + raise path)
  — substrate interfaces fixed here (personality contract, `RaiseException`
  record shape, unwind passes), implementation is its own large design.

Loader/entry contract amendments required (tracked, must ship with phase 3):
`pe_module` admits exception directories (in-image, in-range, referenced
sections executable) and replaces the two absolute "no exceptions" clauses
with the SEH64-active clause; `mb_win_entry` documents the exception contract
of the invoke boundary (may fault into SEH64; may not return via longjmp).

---

## 8. Hard problems (honest assessment)

- **H1 — Unwind correctness long tail.** The opcode interpreter is easy to
  get 95% right; the last 5% (MSVC epilogue splitting, SET_FPREG body
  cases, chained-info nesting, far saves at odd prologue offsets) silently
  corrupts the frame walk and produces wrong *everything* downstream.
  Mitigation: exhaustive fixture tests from real census-image bytes, plus a
  cross-check mode that runs `RtlVirtualUnwind` over every `ControlPc` in a
  mapped image comparing against LLVM-unwind-based reference expectations
  where available. Risk: medium; detectability: good on data plane.
- **H2 — Personality invocation contract.** Filters/`__finally` bodies must
  execute **as if called by the frame itself** (they access locals via
  frame-relative addressing). Real ntdll uses guard-page trampolines
  (`RtlpExecuteHandlerForException` pushing a return into a stub). We must
  build the win64cc equivalent: the trampoline constructs a return address
  pointing at a recovery stub, sets RSP to the establishing frame, and jumps
  to the filter; the stub resumes the unwinder. Getting RSP/align wrong =
  stack corruption that only manifests under specific unwind shapes. This is
  the highest-risk piece; it is unverifiable on the macOS host, only on the
  native route. Risk: high.
- **H3 — Resume semantics.** `EXCEPTION_CONTINUE_EXECUTION` requires
  rewriting the OS signal context and returning from the signal handler
  (FreeBSD `ucontext`/sigreturn dance), including correct `%cs/%ss`/flags
  and trap-frame restoration; divergence risk vs real `NtContinue`
  (syscall-restart, pending-signal interactions). Mitigation: restrict
  documented support to faults raised by our shim (we control the origin),
  treat signal-arrival-during-syscall as unsupported-continue. Risk:
  medium-high, narrow surface.
- **H4 — Two EH worlds on one stack.** Windows frames (SEH64) and SysV
  frames (host/game C++ with `.eh_frame`) coexist. If a fault escapes PE +
  bridge frames into SysV land, our walk must terminate (no unwind info) and
  the host must see a plain signal — never a half-unwind. Conversely a SysV
  throw crossing a win64cc frame is undefined and must be refused by
  boundary contract. Mitigation: terminal-frame policy + boundary clauses;
  the enumerated bridge tables exist precisely to make the walk end at a
  *known* frame. Risk: medium; policy, not code.
- **H5 — Personality contract subtleties.** `__C_specific_handler`'s exact
  `RtlUnwind` argument choice, `TARGET_UNWIND` flag timing, scope-index
  carrying, and `__finally` during both passes are specified from documented
  behavior + ReactOS/Wine prior art, **not** from vendored Microsoft
  sources (none retained). Byte-exactness with ntdll is not claimable;
  semantic conformance for MSVC-MSVC round trips is the target and is
  testable end-to-end (phase 3 scenarios). Any code path where our
  implementation differs observably must be written down, not papered over
  (RUNTIME_MAP "do not fake" rule). Risk: medium.
- **H6 — Stack overflow & reentrancy.** Dispatching from a nearly exhausted
  stack (guard-page faults inside the dispatcher) needs alternate-stack
  handling; collisions (exception during unwind of the same frame) need
  `COLLIDED_UNWIND`. Both deferred to phase 4 with the census justification
  that UE4SS only sets `SetThreadStackGuarantee` (query/state) — no observed
  deep-overflow recovery requirement. Risk: high if attempted early; honest
  deferral.
- **H7 — Verification asymmetry.** Data/decision planes are exhaustively
  host-testable; control plane is native-only. The verifier split (host
  fixture suite + win64cc IR checks + native-route scenarios) must be
  designed in from phase 0, or phase 3 becomes unreviewable.

---

## 9. Complexity assessment summary

| Subsystem | Essential for census? | Size | Risk | Verifiable on host |
|---|---|---|---|---|
| CONTEXT + RtlCaptureContext | yes (UE4SS) | S | low | layout yes; capture native-only |
| Registry + RtlLookupFunctionEntry | yes | S/M | low | yes |
| UNWIND_INFO parse + RtlVirtualUnwind | yes | M | medium | yes, exhaustively |
| __C_specific_handler + scope tables | yes | M | medium | decision core yes |
| Dispatch driver (OS role) | internal, required | M | medium | yes (injected contexts) |
| RtlUnwindEx engine | internal, required | L | medium | pass logic yes |
| Guard/continue trampolines | internal, required | M | **high** | native only |
| Signal shim + resume | internal, required for real faults | M | medium-high | native only |
| Unhandled filter family | yes (UE4SS) | S | low | yes |
| RtlAddFunctionTable (+bridge tables) | internal, required for M2 | M | medium | registry yes; tables native |
| Callback/growable tables, vectored, stack-overflow | no | L each | high | partial |
| C++ EH personalities (EH3/4/5) | separate family | XL | high | partial |

Overall: the SEH64 family is the **largest remaining service family**, but
the census-minimal core is bounded: roughly S+M+M+M+M+L+M+M+S+M ≈ one
large multi-phase effort, dominated by H2/H3 native control-plane work, not
by parsing complexity. The honest headline: **~70% of the code is
host-exhaustively testable pure logic; the remaining 30% carries ~80% of the
risk.**

---

## 10. Explicit non-claims

- No Windows ntdll/VCRUNTIME parity claim; semantic conformance target only.
- No engine-side or game-run acceptance; phase 3 receipts are the only
  native evidence this design anticipates, and none exist yet.
- C++ EH behavior is not designed here beyond substrate interfaces.
- `__C_specific_handler`/`RtlUnwindEx` behavioral details derive from
  documented behavior and public prior art (ReactOS/Wine), not vendored
  Microsoft sources; divergences will be receipted, never hidden.
