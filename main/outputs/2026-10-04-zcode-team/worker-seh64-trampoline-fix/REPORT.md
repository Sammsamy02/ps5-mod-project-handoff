# REPORT.md — worker-seh64-trampoline-fix

Worker: worker-seh64-trampoline-fix (ZCode team, 2026-10-05). No console
contact, no target/PS5 execution, no nested subagents, no network; writes
only under `outputs/2026-10-04-zcode-team/worker-seh64-trampoline-fix/`.
worker-seh64-family and worker-dll-8fam-combined were read-only inputs.

Status: **SEH64_TRAMPOLINE_FIX_DISCHARGED_HOST_SIDE** — F-1 and F-2 from
`worker-idle-work-survey/TRAMPOLINE-REVIEW.md` are fixed in owned copies of
the SEH64 family tree and the 8-family combined tree; the discharging host
probe passes twice deterministically and provably bites on the pre-fix
bytes; two additional latent defects (F-3, F-4) were discovered and fixed
because the probe could not run without them — all four provably never
exercised before this worker (the shipped fixture carried an unconditional
`BISECT-1: kit only` early return; every raise/dispatch/unwind/transfer
path was dead in all prior validated receipts). Host-route evidence only;
nothing console-proven; no target evidence is created.

## Deliverables (in write order)

1. `CLAIM.json` — bounds/claims first, plus a dated addendum recording the
   F-3/F-4 discoveries and control outcomes.
2. `vendor/` — fixed copy of the SEH64 family vendored tree. Changed files
   (diffs in `diffs/`):
   - `windows-services/mb_win_seh64_abi.c`
     - **F-1**: `call_at_frame_win64` guard zone 0x28 -> 0x30
       (`subq $0x30, %rsi`; recovery pre-store `0x20(%rsi)` -> `0x28(%rsi)`,
       keeping the emulated return-address slot at `frame_rsp-0x8`);
       pre-call rsp = `frame_rsp-0x30` = 0 (mod 16), callee entry =
       `frame_rsp-0x38` = 8 (mod 16). Extended to a 4-arg form (arg2 in
       SysV rcx -> Win64 rdx).
     - **F-2**: `svc_capture_context` comment corrected — stored Rax is the
       captured EFlags image (pushfq), not the caller's rax; header block
       comment updated to the same effect.
     - **F-3**: `svc_raise_exception` is now a naked entry shim (Win64 arg
       registers — the only callers are PE IAT calls): captures the raise
       CALLER's context before any compiler frame exists (nonvolatiles
       live), rewrites `ctx.rip = [entry_rsp]` and `ctx.rsp = entry_rsp+8`,
       then calls the C dispatch body `svc_raise_body` and rets to the
       caller. Route-aware asm symbol macro (`_` prefix on Mach-O only).
   - `windows-services/mb_win_seh64.c` — **F-4**: scope-table semantics for
     the real clang x64 encoding: finally scopes are `{Handler=funclet,
     JumpTarget=0}`; skipped on the search pass, invoked on the unwind pass
     via the new `call_at_frame2` control hook with the PSP (the frame's
     call-site rsp) in rcx and rdx — clang funclets read rdx and form their
     frame register as PSP+0x20 (verified against emitted code). Control
     validation requires the new hook.
   - `windows-services/mb_win_seh64.h` — `call_at_frame2` added to
     `mb_win_seh64_control` (documented).
   - `windows-candidate/tests/worker-seh64.h` — the discharging probe:
     the `BISECT-1: kit only` early return REMOVED (all -130..-137 checks
     now actually run), plus two new guards:
     - **-138** `mb_seh_guard_sse_filter`: raises through a REAL
       non-constant filter funclet (`__except (mb_seh_filter_sse())`).
       The funclet forces a 16-byte-aligned `__m128`-class stack spill
       (bare aligned vector type; no intrinsic header needed — this SDK's
       fixture compiles are -ffreestanding with no compiler header dir;
       movaps verified in the disassembly), an aligned global store
       round-trip, and a naked rsp probe (`mb_seh_sp_probe`) asserting
       Win64 call-site alignment (callee entry rsp = 8 mod 16,
       prologue-shape independent).
     - **-139** `mb_seh_guard_sse_finally`: the same aligned spill + probe
       inside a `__finally` funclet exercised through the unwind pass.
     Codes -138/-139 are unique (census used -130..-137; vm -110..-117,
     moduleenum -100..-104, crtstartup -90..-97, fileio -80..-89).
3. `diffs/` — unified diffs of all four changed files against the
   worker-seh64-family lineage originals (`.orig` copies included; the
   diffs contain no diagnostics).
4. `baseline-tree/` + `out-family-baseline/8866fee6…/` — shipped-bytes
   reproduction run: `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`, 22 commands,
   evidence byte-equal to the family's own receipt
   (133de5d3…) modulo build_id — proves this harness reproduces the
   lineage evidence and pins the pre-fix behavior.
5. `out-family-fixed/b62837c7…/` and `out-family-fixed2/197d8dc5…/` — the
   fixed tree, `--host-only --entry --workers --seh64`:
   `OWNED_CANDIDATE_HOST_CONTROLS_PASSED` twice. Determinism: 22 commands
   with identical argv/exit sequences, fixture hashes identical
   (consumer f2548057…, helper c2daff94…), JSONL evidence streams
   byte-identical modulo build_id, no hung drivers. Prior evidence
   unshifted: results [86,93], entry [7,7]/[0,0], worker begin [258,259]
   result 0x81234567, imports 29/8, cleanup {pending:0, retired:1},
   `seh64 {"enabled":1}`. win64cc_entries 7, exercised_imports 8 unchanged
   (the naked shim does not change the win64cc count; svc_raise_body
   replaces svc_raise_exception's win64cc definition 1:1).
6. `negative-control/` + `out-negative-control*/` — the bite proof:
   control trees byte-identical to the fixed tree EXCEPT the two trampoline
   immediates reverted to the pre-fix 0x28 form (verified byte-identical to
   `diffs/mb_win_seh64_abi.c.orig` inside the trampoline):
   - Full fixture (`out-negative-control/add92799…`): FAILED — driver
     wedges deterministically at the -138 filter-funclet invocation
     (core-side trace shows `filter invoke funclet=4fd0` as the last event;
     no phase-3 records; exit -1, hung). Disassembly confirms the funclet's
     rsp-relative aligned `movaps 0x30(%rsp)` — the review's #GP class.
   - Two control variants (`out-negative-control-probeonly*`): the wedge
     reproduces even after removing the SSE spill (probe-only2's filter is
     SSE-free by disassembly) — the misaligned funclet entry alone breaks
     funclet execution on this host route. The fixed tree runs the same
     funclet green, twice, deterministically. Single-variable causality:
     only the two immediates differ.
   - Note: the control trees carry CONTROL-ONLY stderr diagnostics in
     their `mb_win_seh64.c` (marked CONTROL-ONLY DIAGNOSTIC); the fixed
     `vendor/` has none.
7. `vendor-8fam/` + `out-host-8fam/8fd6c382…` + `out-host-8fam2/…` — the
   8-family combined tree with the four fixed SEH64 files grafted (all
   other bytes from worker-dll-8fam-combined): `OWNED_CANDIDATE_HOST_
   CONTROLS_PASSED` twice for the full 12-profile host validation; 30
   commands, identical argv/exit; JSONL byte-identical modulo build_id
   across runs; and byte-identical to the ORIGINAL worker-dll-8fam-combined
   receipt (256e9b3a…) modulo build_id — all seven other families' evidence
   is unshifted; seh64 profile stays win64cc_entries 7 / imports 8.
8. `out-sdk-family/07872399…/` and `out-sdk-8fam/c22279d9…/` — compile-only
   SDK ELF candidates (`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`;
   never executed anywhere). **ELF delta**: family 222408 -> 222528 bytes
   (+120, +0.05%); 8-family 336552 -> 336672 (+120). Undefined-symbol sets
   identical (36 / 55) and needed libraries identical to their baselines —
   imports unchanged, as predicted. `out-sdk-elf-delta.json` holds sizes,
   SHA-256s and set comparisons.
9. `OWNING-LINEAGE.md` — lineage note (design -> family -> 8fam -> survey
   review -> this worker), what was discharged, the new F-3 boundary
   (exactly one software raise in flight), and the unchanged ceilings.
10. This `REPORT.md` (last).

## Findings discovered and fixed beyond the review (F-3, F-4)

Both are latent defects in the inherited tree that NO prior receipt ever
exercised, because `worker-seh64.h` carried an unconditional
`if (mb_seh_probe_ctx.rip) return 1;` right after the walker kit (the
"BISECT-1: kit only" line) — every raise, filter, finally, unwind and
transfer was dead code in the shipped/validated configuration. The family
and 8fam reports' claim that "the full -130..-137 exercise completed inside
the passing run" is contradicted by the shipped bytes and is corrected by
this worker's runs.

- **F-3 (raise search context)**: the shipped `svc_raise_exception` ran
  `svc_capture_context` inside its own C frame, so the search pass started
  at a driver-side rip with no unwind info and every raise went unhandled
  (first diagnostic run: clean `-133`; instrumented run: first search
  lookup `NO_UNWIND_INFO` on the family's own frame). Fixed OS-faithfully
  (the OS captures the raiser's call site, never RaiseException's own
  frame).
- **F-4 (scope encoding + PSP)**: with the raise path alive, the cross-frame
  guard corrupted frames: clang emits `__finally` scopes as
  `{Handler=funclet, JumpTarget=0}` and the shipped code (a) invoked the
  finally funclet as a FILTER on the search pass and (b) never invoked it on
  the unwind pass; additionally the funclet PSP must be the frame's
  call-site rsp delivered in **rdx** (frame register = PSP+0x20), not the
  SET_FPREG establisher frame in rcx. Fixed as described above.

## Debugging provenance (kept for review)

`out-e1/`, `out-e1b/`, `out-e2/`, `out-e3/` — instrumented intermediate
runs (distinct failure codes -140/-150/-151, core stderr traces) that
localized each failure: -37 garbage (first run, corrupted transfer),
-140 filter-never-invoked (F-3), -133 constant-filter unhandled (F-3),
-135/-150 finally-not-run (F-4), then green. The final tree contains no
diagnostics (diff-verified).

## Honest limits

- Host route only (Mach-O x86_64 under Rosetta). Nothing PS5/target-executed;
  the PS5-route half of F-1 provenance (and the F-3/F-4 fixes) remains
  console-bound — dll8's bounded census is still the only on-target SEH64
  proof path. No target evidence is created here.
- No Windows OS parity; no quality score anywhere; the SDK ELFs are
  compiled candidates, never executed.
- New documented boundary (F-3): exactly one software raise in flight; a
  raise inside a filter/finally funclet would reuse the static context and
  is unsupported/unexercised.
- The review's and family docs' "PSP in rcx" funclet claim is corrected:
  clang finally funclets read the PSP from rdx (and the PSP is the frame's
  call-site rsp).
- Known Rosetta phenomenon (pre-existing, also visible as ~32 UE-state
  `test-sdk-driver` leftovers from the family's own historical runs): host
  drivers can wedge unkillably. This worker's control runs additionally
  wedged mid-run on the pre-fix trampoline (that wedge IS the negative
  result); leftover processes were left untouched per the
  no-unrelated-kill rule.
- The `negative-control/probeonly` run intended as a "spill removed"
  variant actually retained an rsp-relative aligned `movaps` (volatile
  read) — documented as-is; the truly SSE-free variant is
  `out-negative-control-probeonly2`.

## Verdict

The survey review's trigger is discharged at the host-evidence level it
asked for: the alignment fix is in, the census blind spot (aligned-SSE
funclets) is now a passing test that fails loudly on the pre-fix bytes, the
whole raise/dispatch/unwind/transfer path is genuinely exercised for the
first time and green, the combined 8-family validation is green and
byte-stable, and the SDK candidates differ from their baselines by exactly
+120 bytes with identical import surfaces. What remains console-bound is
stated in OWNING-LINEAGE.md and must not be widened without new evidence.
