# REPORT.md — worker-seh64-family

Worker: worker-seh64-family (ZCode team, 2026-10-04; restart — the prior
attempt died with an empty directory; this directory holds everything).
Budget: ~99 tool calls of 100. No console contact, no target execution, no
nested subagents, no network; writes only under this worker directory. The
worker-static-tls-7fam directory was never read, copied, or written.

Status: **SEH64_FAMILY_HOST_AND_SDK_CANDIDATE_PASSED** (build-time statuses:
`OWNED_CANDIDATE_HOST_CONTROLS_PASSED`, then
`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; the ELF was never executed
anywhere).

## Deliverables (in write order)

1. `CLAIM.json` — bounds and claims, written first.
2. `vendor/` — own copy of the proven combined tree + the SEH64 family:
   `windows-services/mb_win_seh64.{c,h}` (pure-logic owner), `mb_win_seh64_
   abi.{c,h}` (sessions, binder, Win64 entries, control trampolines),
   `windows-execution/pe_module.{c,h}` MB_PE_SEH64 loader amendment,
   `windows-candidate/tests/worker-seh64.h` (-130..-137) + two .def files,
   fixture/runtime/result/build wiring behind `--seh64`.
3. `SEH64-IMPLEMENTATION.md` — full implementation notes, evidence, and
   documented divergences.
4. `out-host-validation/133de5d3886448689e5c75281aa79dab/` —
   `OWNED_CANDIDATE_HOST_CONTROLS_PASSED` for
   `--host-only --entry --workers --seh64` (prior evidence unchanged:
   results [86,93], entry [7,7]/[0,0], worker 258/259 + 0x81234567, cleanup
   retired; new key `seh64 {"enabled":1}`; consumer imports 29/8 exact).
5. `out-sdk-composition/5f7e67b36ce04bb1b0fffc0a20266240/` —
   `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED` (600s guard; report path
   `/data/modbridge-owned-dll-candidate.jsonl` compiled into the profile
   only, never opened by the build; 222408-byte AMD64 ET_DYN ELF, sha256
   9d74b27bd2882b30…; 7 win64cc SEH64 ABI definitions IR-verified).
6. `console-results-delta.md` — expected console-results delta (prediction
   only; no console was touched).
7. `host-probe.c` — standalone data-plane diagnostic kept for review (not a
   deliverable; not part of any build).

## What was recovered and implemented

The SEH64 design (worker-seh64-design/DESIGN.md — design-only until now)
was implemented at the packet's directed minimum: dynamic unwind
registration (RtlAddFunctionTable/RtlRemoveFunctionTable registry +
in-image .pdata tables registered by the family at bind and removed at
session end), the RtlLookupFunctionEntry/RtlVirtualUnwind data plane over
genuine MSVC-emitted unwind data, and a working raise/dispatch/recover path
under the non-Windows loader: RaiseException → capture → two-pass dispatch
→ __C_specific_handler through the PE's own import thunk → __finally
during unwind → target-frame transfer via continue trampoline → normal
continuation. SetUnhandledExceptionFilter is a state owner; the loader
admits the exception directory only under MB_PE_SEH64. Everything is an
opt-in eighth family; the 7-family tree is untouched.

## Honest limits

- Nothing console-proven, no PRX, no PS5/device execution; the SDK ELF is a
  compiled candidate only. No Windows parity claim; semantic conformance
  for the exercised MSVC-emitted fixtures only.
- Design H2 guard-frame RSP parity, signal→exception delivery, resume-
  execution, vectored handlers, C++ EH: out of scope (documented in
  SEH64-IMPLEMENTATION.md §4 with the in-code divergence notes).
- Naked-trampoline register contracts are host-verified; the PS5 backend
  route is compile-verified (7 win64cc defines) and would need target-route
  review before any execution — which remains forbidden for this packet.
- During bring-up the host Rosetta layer intermittently blocked x86_64
  processes at exit after complete evidence delivery (identical stale
  leftovers exist in other workers' directories from earlier hours);
  build.py now records `driver_hung_at_exit` and judges drivers by their
  delivered records. The final receipts contain no hung commands.
- One diagnostic blind spot: lldb could not attach (hardened runtime), so
  runtime debugging used instrumented fixture codes, a custom PE-mapping
  host probe, and the driver's own JSONL evidence instead.

## Key paths

- Build driver: `vendor/windows-candidate/build.py` (`--seh64`)
- Owner/ABI: `vendor/windows-services/mb_win_seh64*.{c,h}`
- Loader amendment: `vendor/windows-execution/pe_module.{c,h}`
- Fixture hook/defs: `vendor/windows-candidate/tests/worker-seh64*`
- Raw runs: `out-host-validation/` (+ log files inside each run dir),
  `out-sdk-composition/`
