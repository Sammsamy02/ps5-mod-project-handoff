# OWNING-LINEAGE.md — worker-seh64-trampoline-fix (2026-10-05)

Owning lineage and discharge path for the SEH64 trampoline findings. This
worker is part of the SEH64 family lineage; it wrote only its own directory
`outputs/2026-10-04-zcode-team/worker-seh64-trampoline-fix/`. The source
trees were read-only inputs; all fixes live in this worker's own copies.

## Lineage

```
worker-seh64-design (DESIGN.md, design-only)
  -> worker-seh64-family          (SEH64 implemented; vendored tree; receipts
                                   133de5d3... host, 5f7e67b3... SDK ELF)
  -> worker-dll-8fam-combined     (SEH64 grafted into the 8-family combined
                                   tree; receipts 256e9b3a/1fb208c5 host,
                                   c3aba79f SDK ELF)
  -> worker-idle-work-survey      (TRAMPOLINE-REVIEW.md: read-only pre-execution
                                   review; findings F-1, F-2)
  -> worker-seh64-trampoline-fix  (THIS worker: F-1/F-2 discharged in owned
                                   copies of both trees; F-3/F-4 discovered
                                   and fixed; new receipts below)
```

## What this worker owes the lineage (and paid)

1. **F-1 (review): guard-frame funclet entry alignment** — fixed in
   `vendor/windows-services/mb_win_seh64_abi.c` `call_at_frame_win64`:
   guard zone 0x28 -> 0x30 (`subq $0x30, %rsi`), recovery pre-store moved to
   `0x28(%rsi)` so the emulated call-site return slot stays at
   `frame_rsp - 0x8`; pre-call rsp = `frame_rsp - 0x30` = 0 (mod 16) and
   callee entry = `frame_rsp - 0x38` = 8 (mod 16). Discharged by the new
   fixture guards -138/-139 (real non-constant filter funclet + `__finally`
   funclet, each forcing a 16-byte-aligned `__m128`-class spill through a
   local plus a naked rsp probe asserting Win64 funclet entry parity).
2. **F-2 (review): `Context.Rax` comment** — relabeled: the captured Rax is
   the `pushfq` EFlags image (also stored at 0x44), NOT the caller's rax;
   rdx/r8-r11 remain "caller-dead as found".
3. **F-3 (new, this worker): raise-path search context** — the shipped
   `svc_raise_exception` captured its own frame, so the search pass always
   terminated at a driver-side frame with no unwind info. Never exercised
   before because the shipped fixture carried an unconditional
   `BISECT-1: kit only` early return that skipped every raise. Fixed with a
   naked entry shim (OS-faithful: capture the raise caller's context before
   any compiler frame; rip/rsp rewritten to the caller's call site).
4. **F-4 (new, this worker): scope-table encoding** — clang x64 emits
   `__finally` scopes as `{Handler=funclet, JumpTarget=0}` and passes the
   PSP to finally funclets in **rdx** (frame register = PSP + 0x20, PSP =
   the frame's call-site rsp). The shipped code called finally funclets as
   filters on the search pass (with a wrong PSP, corrupting live frames) and
   never on the unwind pass. Fixed: skip finally scopes on search, invoke on
   unwind via the new `call_at_frame2` control hook (PSP in rcx and rdx).
5. **Discharging evidence** (all in this worker, no console contact, no
   target execution):
   - `out-family-baseline/` — shipped-bytes reproduction: PASSED; evidence
     byte-equal to the family's own receipt modulo build_id (proves the
     harness reproduces the lineage evidence and pins the old behavior).
   - `out-family-fixed/` + `out-family-fixed2/` — fixed tree,
     `--host-only --entry --workers --seh64`: OWNED_CANDIDATE_HOST_
     CONTROLS_PASSED twice; 22 commands argv/exit identical; JSONL streams
     and fixture hashes byte-identical modulo build_id. Base results [86,93],
     entry [7,7]/[0,0], worker 258/259 + 0x81234567 all preserved; the full
     -130..-137 census PLUS the new -138/-139 guard-funclet alignment
     discharge complete inside the passing run (the bisect is removed).
   - `out-negative-control*/` — control trees identical to the fixed tree
     EXCEPT the two trampoline immediates reverted to the pre-fix 0x28 form:
     every variant wedges the host driver deterministically at the -138
     filter-funclet invocation (first real filter through the guard window);
     with the aligned-spill fixture the funclet's rsp-relative aligned
     `movaps` is present (disassembly verified) — the review's #GP class.
     FAILED is the expected, receipt-recorded outcome: the test bites.
   - `out-host-8fam/` + `out-host-8fam2/` — 8-family combined tree with the
     fixed SEH64 grafts: PASSED twice; 30 commands; evidence byte-identical
     modulo build_id across runs AND byte-identical to the original
     worker-dll-8fam-combined receipt modulo build_id (all seven other
     families' evidence unshifted).
   - `out-sdk-family/`, `out-sdk-8fam/` — compile-only SDK ELF candidates
     (OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED; never executed
     anywhere): +120 bytes each vs their baselines; undefined-symbol sets
     (36 / 55) and needed libraries identical — `out-sdk-elf-delta.json`.

## What the lineage still owes (unchanged ceilings)

- Console/target evidence: the PS5-route part of F-1 provenance (and the
  F-3/F-4 fixes) remains console-bound; dll8's bounded fixture census on
  console is still the only on-target SEH64 proof path. This worker creates
  NO target evidence.
- Signal->exception delivery, vectored handlers, C++ EH, FP-state restore,
  faulting-instruction retry: documented ceilings, untouched.
- New boundary from F-3: exactly one software raise in flight (the raise
  context is a static); a raise inside a filter/finally funclet is
  unsupported and unexercised.
- Known Rosetta host-route phenomenon (pre-existing, also present in the
  family's own historical outputs): x86_64 drivers can wedge at process
  exit in an unkillable UE state. This worker additionally observed the
  same wedge class mid-run on the PRE-FIX trampoline only (the control
  runs); leftover UE driver processes from both eras exist on this Mac and
  were left untouched per the no-unrelated-kill rule.

## Honest limits

- The family's and 8fam's published receipts claimed "the full -130..-137
  exercise completed"; the shipped fixture bytes contradict this (bisect).
  The claims are corrected by THIS worker's runs, which actually exercise
  the raise/dispatch/unwind/transfer path and extend it with -138/-139.
- Host-route evidence only (Mach-O x86_64 under Rosetta). No Windows OS
  parity, no quality scores anywhere.
