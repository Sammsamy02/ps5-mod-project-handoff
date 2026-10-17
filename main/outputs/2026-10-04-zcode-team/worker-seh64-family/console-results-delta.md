# console-results-delta.md — expected console-results delta for SEH64

Worker: worker-seh64-family · 2026-10-04 · Prediction only: nothing in this
packet touched a console, a target, or any PRX. The SEH64 family is NOT
integrated into the 7-family tree; if it is ever composed for a console
run, the expected delta versus the current six/all-family console PASS
(worker-combined-dll-allfam) is:

## Build/composition deltas (no console behavior change without --seh64)

1. With `--seh64` off, every artifact and result key is byte-identical to
   the vendored baseline tree (the only shared-file edits are behind
   `MB_OWNED_SEH64`/`MB_PE_SEH64` compile gates: pe_module admission
   allowlist, owned_runtime capacity/binder/session blocks, result key,
   test count formulas).
2. With `--seh64` on, the payload gains:
   - 8 new PE imports in the consumer fixture (7 kernel32 +
     1 vcruntime140): RtlCaptureContext, RtlLookupFunctionEntry,
     RtlVirtualUnwind, RtlAddFunctionTable, RtlRemoveFunctionTable,
     SetUnhandledExceptionFilter, RaiseException, __C_specific_handler
     (consumer_imports 29 with --entry --workers; IMPORT_CAPACITY tier
     raised to 34, still under the 96 sync cap).
   - The consumer/helper fixture PEs now carry a real exception directory
     (unwind tables on). Expected console loader log delta: the retained
     admission path accepts directory 3 under MB_PE_SEH64 instead of
     refusing it ("Unsupported directory 3" disappears).
   - +2 native/bridge units (mb_win_seh64.c native; mb_win_seh64_abi.c with
     exactly 7 win64cc definitions on the SDK route + 3 naked trampolines).
   - New result key `seh64:{"enabled":1}` in the JSONL result records.
   - Two new native failure stages in the report vocabulary: 55 (owner
     open) and 56 (owner close) — unused in green runs.

## Expected console result-record deltas with --seh64 on (green path)

- `counts.consumer_imports`: 21 → 29 (--entry --workers composition).
- New key: `"seh64":{"enabled":1}`.
- `results` stays [86,93]; entry attach/detach [7,7]/[0,0] with
  attach_result 7; worker begin [_,_,258,259] / result 0x81234567;
  cleanup {pending:0,retired:1} — all unchanged.
- The LoaderExercise window additionally proves, inside the target PE:
  real .pdata registration at bind, walker-kit API calls over the image's
  own unwind data, a same-frame raise→__except recovery, a cross-frame
  raise with __finally during unwind, dynamic function-table add/lookup/
  remove over a bridge address, and last-chance-filter previous-pointer
  round trip. Failure of any of these surfaces as a unique negative
  exercise code -130..-137 returned through stage 8 (first_failure.stage 8
  with native_status = 2^32-130…2^32-137), never as a crash or a hang.

## Risks that console evidence would need to retire (honest)

- Guard-rsp/funclet conventions on the real win64cc route (design H2):
  host-verified only; the SDK ELF was never executed.
- Naked-trampoline register contracts compiled by the PS5 backend
  (attribute-dropped; explicit shuffles) — compile-verified only.
- Rosetta-exit flake note for host validation: during this packet the host
  driver occasionally blocked at process exit after all result records were
  delivered (same signature as stale leftovers in other workers' trees);
  build.py now guards driver runs by evidence completion and records
  `driver_hung_at_exit` in the command log. The final receipt had none.
