# REPORT.md — worker-seh64-design

Date: 2026-10-04 · Mode: design-only · Bounds honored: ≤60 tool calls
(15 used), writes only under `outputs/2026-10-04-zcode-team/worker-seh64-design/`,
no nested subagents, no network, no existing file touched.

## Deliverables

- `CLAIM.json` — claim written first.
- `DESIGN.md` — full SEH64 family design (census, architecture, subsystem
  designs, minimal subset, phased plan, hard problems, complexity table,
  non-claims).

## What was read (all read-only)

1. **Census** — `worker-dll-mod-retention/baseline-dll-census.json` and
   `ue4ss-zip-census.json` (both cover `dwmapi.dll` + `UE4SS.dll`; SEH sets
   identical), cross-checked with `worker-modreqs/MOD-CENSUS.md` and
   `SERVICES-BACKLOG.md` §8.
2. **Existing SEH surface** — `worker-crt-integrate/vendor/windows-services/
   mb_win_crt_startup.c/.h` (`mb_win_crt_seh_filter_dll` decision stub) and
   `worker-crt-startup-design/DESIGN.md` (deferrals adopted into this design).
3. **Execution model** — `outputs/ZCODE-HANDOFF-2026-10-04/RUNTIME_MAP.txt`
   (native AMD64 PE execution, win64cc LLVM route, "do not fake" ABI rule).
4. **Integration contracts** — `outputs/ps5-mod-bridge/windows-execution/
   pe_module.h` (admission currently refuses unwind directories; "no
   exceptions" boundary clauses), `windows-services/README.md`,
   `mb_win_entry.h`, `windows-services/mb_win_*.c` owner patterns via
   `worker-4-win-services/REPORT.md`.

## Key findings

1. **Observed census is small and precise.** 10 SEH-relevant imports:
   `__C_specific_handler` + `_CxxThrowException` (VCRUNTIME140, both DLLs),
   `__CxxFrameHandler4` (VCRUNTIME140_1, both), `__CxxFrameHandler3`
   (UE4SS), `_seh_filter_dll` (api-ms-win-crt-runtime, both; already owned
   by the CRT-startup family), and UE4SS-only KERNEL32
   `RtlCaptureContext`, `RtlLookupFunctionEntry`, `RtlVirtualUnwind`,
   `SetUnhandledExceptionFilter` (+ `SetErrorMode`,
   `SetThreadStackGuarantee` as adjacent state owners).
2. **`RtlAddFunctionTable`/`RtlInstallFunctionTableCallback`/`RtlUnwind(Ex)`
   are NOT imported** by either DLL. They are internal substrate. The only
   one promoted to "required internally" is a dynamic function-table
   registry, because win64cc bridge code (ELF, Itanium `.eh_frame`) has no
   `.pdata` and unwinding must cross the DllMain bridge chain.
3. **The UE4SS KERNEL32 imports form a self-consistent stack-walker kit**
   (capture → lookup → virtual-unwind), which is pure data plane and
   host-exhaustively testable; that anchors the minimal-subset ordering.
4. **The primary SEH control-flow scenario is PE-internal**: the MSVC CRT
   init `__try/__except(_seh_filter_dll)` lives in the mapped PE with real
   `.pdata`, so DllMain-time fault swallowing needs no bridge tables — only
   `__C_specific_handler` + `RtlUnwindEx` + the existing filter owner.
5. **Contract break required**: `pe_module.h` currently refuses unwind
   directories and forbids exceptions across entry boundaries twice; the
   design specifies the amendments that must ship with the control plane.
6. **Complexity is asymmetric**: ~70% of the code (layouts, parsers,
   scope-table decisions, dispatch pass logic, unhandled filter) is
   freestanding, host-testable logic; ~30% (personality guard trampolines,
   continue/resume, signal shim) is native-only and carries most of the
   risk. Highest single risk: the filter-invocation-at-establishing-frame
   trampoline (H2).

## Honest limits

- Design only; nothing implemented, compiled, executed or verified.
- Personality/unwind behavioral details come from documented behavior and
  public prior-art knowledge (ReactOS/Wine-style contracts); no vendored
  Microsoft sources were consulted (none retained); byte-exact ntdll parity
  is explicitly not claimed.
- No engine-side, game-run, or PS5-device claims of any kind.
- One noted imperfection: DESIGN.md §5b contains a hedged phrasing of the
  `UWOP_ALLOC_LARGE` two-slot threshold while deferring the exact constant
  to the implementation phase's fixture tests — the rule (1 slot vs 2 slots
  by comparison bound) is stated, the literal 0x817F-vs-0x8000 constant is
  flagged as implementation-time pinned against fixture bytes.
