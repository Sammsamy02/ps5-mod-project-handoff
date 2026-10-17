# Console expected-results delta — SEH64 as the 8th family (worker-dll-8fam-combined)

Baseline: the host-validated 7-family plan (worker-static-tls-7fam,
candidate e72f531…: consumer_imports 82, results [86,93], worker 4/4,
entry 7/7 / [0,0] detach, contract_errors 0, cleanup retired, ten keys
`{"enabled":1}` including `static_tls`). This document states exactly what a
console run of THIS worker's 8-family build (`--entry --clock --workers
--sync --init-once --mutex --fileio --crt-startup --module-enum --vm
--static-tls --seh64`) is expected to add or change relative to that
7-family plan. Evidence class: two deterministic host-validated runs +
SDK-ELF compile-only candidate (sha256 79281191…, 336552 bytes); no console
contact, no target execution.

## New / changed keys

| Field | 7-family plan | Expected with --seh64 | Why |
|---|---|---|---|
| `seh64` | absent | `{"enabled":1}`, present from sequence 1 onward (every phase record, like the other family keys) | result_report.c emits it exactly when the flag is on; build.py asserts presence when on and absence when off (host runs measured it on seq 1, 2, 3(+4)) |
| `counts.consumer_imports` | 82 | **90** (+8) | the consumer binds 8 more PE imports: 7 × KERNEL32 (`RtlCaptureContext`, `RtlLookupFunctionEntry`, `RtlVirtualUnwind`, `RtlAddFunctionTable`, `RtlRemoveFunctionTable`, `SetUnhandledExceptionFilter`, `RaiseException`) + 1 × VCRUNTIME140 (`__C_specific_handler`) — enumerated in `tests/worker-seh64-kernel.def` / `worker-seh64-vcruntime.def`; run_body formula term `+(MB_OWNED_SEH64?8u:0u)`; host run measured exactly 90 from the consumer PE metadata (KERNEL32 66→73, VCRUNTIME140 0→1, `mbstatictls.dll` 4 unchanged) |
| `counts.helper_imports` | 10 | 10 (unchanged) | the helper session imports none of the SEH64 names; the binder comment in owned_runtime.c documents the disjoint census |
| SDK ELF undefined imports | 55 | **55 — delta ZERO, identical symbol set and providers** | the family owner/ABI units call only internal `mb_win_*` surfaces on the SDK side; the 8 KERNEL32/VCRUNTIME names exist only in the PE fixture import surface, bound by the loader through the two def libs (verified: `import_providers` key sets of the 7-family ELF e72f531 receipt and this 8-family ELF are equal; the standalone seh64 ELF's 36-symbol set is a subset) |
| win64cc IR definitions | 7-family expected dict incl. `mb_win_static_tls_abi: 1` | **+7 exactly**: `mb_win_seh64_abi: 7` added to the build.py expected dict (session/binder/trampoline definitions); the merged dict asserts both | build.py IR step passed with the union dict; seh64_profile records `win64cc_entries: 7`, `exercised_imports: 8` |

## Census formula changes

- `owned_runtime.c` run_body consumer-imports census and both host drivers'
  formulas gain exactly one additive term `+(MB_OWNED_SEH64?8u:0u)`
  (`test_entry_candidate.c`) / `+(MB_OWNED_SEH64?8u:0u)`
  (`test_worker_candidate_failures.c`, where static TLS keeps its authored
  `MB_OWNED_ENTRY&&` conjunct and SEH64 keeps the unconditioned form from
  its own validated tree). All prior terms and gates are unchanged.
- No census *name* changes: file I/O stays 10 census names, module
  enumeration stays 14 (5 loader-unique), CRT startup stays 7 exercised
  imports, VM stays 5, sync 21, init-once 2, mutex 5, static TLS stays 4.
  The merge resolved the bind order to static TLS (7th) then SEH64 (8th,
  newest, binds last); retire order is SEH64 first ("newest retires first"),
  then static TLS, then families 6..1 — the session Census of the compose is
  order-only, no counts move.

## Unchanged (must not shift)

- `results` stays `[86,93]`; worker `calls_started/returned` 4/4 (begin/
  collect + 2 file I/O), begin `[...,258,259]`, result[0] `0x81234567`;
  file I/O out-contract (`0x46494f31`) untouched.
- `entry`: attach/detach calls [1,1], attach_result [7,7], detach_result
  [0,0], `contract_errors` 0; `clock` sessions [1,1]/[1,1].
- The nine 7-family keys (six allfam + `clock` + `static_tls` + sync/entry/
  worker enablement fields) stay as the 7-family plan states; `static_tls`
  stays `{"enabled":1}` with its -120..-127 fixture evidence intact.
- `cleanup` retires `{pending:0, retired:1}` — the SEH64 owner closes only
  after the consumer session removed its unwind registrations.
- Worker-cleanup-failure mode keeps the first-failure contract (worker 2/2
  before the retained admission; both real worker exports; owners retire
  without repeating PE calls). Host runs measured worker 2/2 and cleanup
  retired in the failure JSONL.

## New in-fixture SEH64 evidence (codes -130..-137, surfaced as failure stages if violated)

-130 context probe: RIP/RSP sanity over a real frame; -131 in-image
lookup (`RtlLookupFunctionEntry` returns a function entry inside the image);
-132 virtual unwind of the caller (`RtlVirtualUnwind` with sane
control PC/establisher frame/frame RSP inside the entry range); -133
same-frame raise must transfer to the handler, never return, and handle
exactly once; -134 guarded-state restore (integer guard words intact); -135
cross-frame raise through an unwound callee: `__finally` ran, handler
reached, unwound frames never resumed (`mb_seh_raiser()` cannot return
`0x5E65`); -136 dynamic registration over a bridge (non-PE) address via
`RtlAddFunctionTable` (lookup hits after register, base exact); -137
removal cleanup, double-removal refused, and unhandled-filter state
round-trip.

## What a console run would still NOT establish

No Windows-loader parity and no target-route proof: the PS5-backend
trampoline route is compile-verified only (7 win64cc IR definitions); design
H2 guard-frame RSP parity, signal→exception delivery, resume-execution,
vectored handlers and C++ EH remain out of scope (SEH64-IMPLEMENTATION §4
divergences). Semantic conformance is evidenced only for the exercised
MSVC-emitted fixtures. Target/console execution remains the console
checkpoint's to prove; this worker's receipts carry `target_executed=false`,
`console_contact=false`, `prx_created=false`.
