# TRAMPOLINE-REVIEW — SEH64 PS5-trampoline pre-execution review (2026-10-05)

Worker: `worker-idle-work-survey` — executing the DLL track's one named open
successor item (`worker-dll-track-closure/DLL-TRACK-STATE.md` §6: "SEH64
PS5-trampoline pre-execution review … required before any *reliance* on SEH64
semantics beyond the exercised fixture census"). Read-only review: sources
read this pass, nothing executed, no console contact, no files outside this
worker directory written. **This review discharges the "target-route review
before any execution" duty at the evidence level available host-side; it does
NOT create target evidence and does not upgrade any host receipt.**

Reviewed on disk this pass:
- `W/worker-seh64-family/vendor/windows-services/mb_win_seh64_abi.c` (full read;
  the naked trio `call_at_frame_win64` / `resume_win64` / `svc_capture_context`,
  the win64cc svc entries, the binder allowlist, session glue)
- `W/worker-seh64-family/vendor/windows-services/mb_win_seh64.c` (owner core:
  `dctx->frame_rsp = context->rsp` at :389, both `call_at_frame` sites :434/:461,
  `unwind_to` transfer construction :556-565)
- `W/worker-seh64-family/SEH64-IMPLEMENTATION.md` §4 (divergence provenance),
  REPORT.md, `out-sdk-composition/5f7e67b3…/receipt.json` (win64cc counts:
  `win64cc_defs: 0` on the naked unit, `win64cc_entries: 7` overall — the
  naked trio indeed takes the default SysV-style entry on the PS5 route)
- `W/worker-dll-track-closure/DLL-TRACK-STATE.md` §6 (the six divergences the
  review must cover), `TRACK-COMPLETE.md` criterion 4/7
- `W/worker-seh64-family/vendor/windows-candidate/tests/worker-seh64.h`
  (fixture shape: `__except (1)` constant filters → in-function JumpTarget bodies)

`W` = `outputs/2026-10-04-zcode-team`.

---

## 0. The route fact being reviewed

On the PS5 ELF route the freebsd9 frontend drops `ms_abi` on `naked`
definitions, so the trio enters with the **SysV AMD64** argument contract
(rdi/rsi/rdx/rcx/r8/r9) and shuffles into Win64 registers in-body
(SEH64-IMPLEMENTATION §4.2; IR receipt `win64cc_defs: 0` / 7 win64cc entries).
Every C-level caller of the trio is native-CC. One body (`svc_capture_context`)
is itself invoked BY Win64 (ms_abi) code through the PE IAT, so it alone keeps
the Win64 `rcx` contract; its one internal caller (`svc_raise_exception`)
invokes it through an explicit rcx-pinned asm call. Host validation
(`OWNED_CANDIDATE_HOST_CONTROLS_PASSED`, receipt `133de5d3886448689e5c75281aa79dab`)
exercised the full raise→dispatch→unwind→recover path on the Mach-O x86_64
route — which uses the SAME SysV entry the PS5 route takes, so the trio's
entry-shuffle logic is host-exercised at the instruction level; what host
validation can NOT prove is (a) target-route kernel/loader behavior and
(b) parity corners no fixture touches. (a) stays console-bound; this review
covers (b).

## 1. The six required divergences — verdicts

### 1.1 H2 guard-frame RSP parity — **FINDING F-1 (open parity risk, off-by-8 alignment)**

Mechanics as shipped (`call_at_frame_win64`): `rsi = frame_rsp − 0x28`;
recovery address pre-stored at `0x20(%rsi)` = `frame_rsp − 0x8` (emulates the
establishing frame's call-site return-address slot); `rsp = rsi`; `call *%r10`
pushes the return address at `frame_rsp − 0x30`; fn enters with
`rsp = frame_rsp − 0x30`. On return, `1:` restores saved `rbx`/`rsi`/`rsp` and
rets — balanced (rbx callee-saved in BOTH conventions; correctly pushed/popped).

`frame_rsp` provenance: `dctx->frame_rsp = context->rsp` (owner core :389) —
the frame's rsp at its control point. Both providers give `frame_rsp ≡ 0
(mod 16)` for standard frames: the search-pass context is the captured
raise-caller context (capture stores caller rsp = entry rsp + 8, and
`svc_raise_exception` calls it from a 16-aligned win64cc call site → ≡ 0),
and the unwind-pass context is `mb_win_seh64_virtual_unwind`'s exit rsp = the
frame's call-site rsp ≡ 0 (call-site invariant; SET_FPREG adjusters are ×16
multiples, epilogue frames derive from a popped return address). The
`__finally` invocation (:461) passes the same `dctx->frame_rsp`.

**The gap:** the Win64 ABI requires rsp ≡ 0 (mod 16) immediately BEFORE a
`call` and rsp ≡ 8 (mod 16) at the callee's entry. The trampoline calls with
`rsp = frame_rsp − 0x28 ≡ 8 (mod 16)` and enters the callee at
`frame_rsp − 0x30 ≡ 0 (mod 16)` — **inverted by 8** whenever `frame_rsp ≡ 0`,
which is the general case above. A Win64-conformant filter or `__finally`
funclet that uses a 16-byte-aligned stack operand (`movaps`/`movdqa` with
rsp- or rbp-derived addressing; rbp = entry_rsp − 8 is likewise off by 8)
would fault with an alignment `#GP` on its first aligned access. The host
fixtures passed because their tiny C filters/bodies (`worker-seh64.h`,
constant `__except (1)` filters, globals-only bodies) compiled without
aligned SSE stack spills — the host receipts never exercised that class.
Rosetta host validation does not discharge it (the fault is a property of the
emitted callee code, not the route).

Consequence bound: **filters/`__finally` funclets containing aligned SSE
stack spills are not known-safe on any route until fixed or proven absent.**
Discharging evidence (owning lineage, new ids, never a replay): add a host
probe whose filter/`__finally` funclet forces an aligned spill (e.g. a
4-wide float copy through a local array) — it should fault today on the
guard path; then the fix (guard zone 0x28 → 0x30, i.e. `rsi = frame_rsp −
0x30`, recovery pre-store at `0x28(%rsi)` = `frame_rsp − 8`, preserving the
call-site-slot emulation while restoring pre-call ≡ 0 / entry ≡ 8) plus the
same probe green on both the Mach-O host route and the freebsd9/PS5 ELF
compile. No code was changed by this review.

Parity items that are CORRECT as shipped (recorded so the fix does not regress
them): the `frame_rsp − 0x8` return-address-slot emulation; shadow space
entirely below the establisher rsp (writes below a live ms_abi frame's rsp
are red-zone-safe — ms_abi has no red zone, and PE consumer code is ms_abi);
rbx/r10 register discipline; `PSP-in-rcx` delivery for `__finally` funclets
(`arg = dctx->establisher_frame`).

### 1.2 Signal→exception delivery — **BY-DESIGN ABSENT (bound restated)**

The M2/H3 shim was never in scope (SEH64-IMPLEMENTATION §4.3). The family
dispatches software `RaiseException` only; a hardware fault in real mod code
(access violation, div-by-zero) never enters this dispatcher — it follows the
native process fate (PS5 kernel signal/crash), with the unhandled filter NOT
consulted. **Reliance ceiling: mods that signal exclusively through
`RaiseException`.** No "real DLL mods raising exceptions" claim may extend to
hardware faults without the unbuilt shim. Fail mode is loud (process crash),
never a silent misdispatch — acceptable as a documented ceiling, and a reason
criterion 7's "before any *reliance*" wording is load-bearing.

### 1.3 Resume-execution — **PARITY SOUND for the shipped semantics (F-2 noted)**

`ExceptionContinueExecution` after a software raise is implemented as a
normal return through the two-pass driver into `svc_raise_exception`'s
caller — which matches real Win64 software-raise semantics, because the
captured `Rip` is the post-raise return address (capture at the caller's call
site), so "resume at captured rip" and "return from RaiseException" land on
the same instruction. Two exact differences, both ABI-harmless here and named
so they are never silently widened:
- **F-2:** captured `Context.Rax` holds the pre-capture EFlags image (the
  body does `pushfq/popq %rax` before storing 0x78), not "the caller's rax as
  found" as the in-code comment says. Both are volatile garbage by contract;
  the comment should not be quoted as evidence of caller-rax fidelity.
- The real OS resume path (`NtContinue`) restores EFlags/SegCs; the family
  captures both (`0x44`/`0x38`) but `resume_win64` applies neither. Sound
  because the only resume is an unwind-target transfer into integer C code
  (DF = 0 is a call-boundary invariant in both ABIs and nothing executes
  `std`; arithmetic flags are dead across C calls). **This forbids ever using
  `resume_win64` for faulting-instruction retry** (continuable-hardware-fault
  semantics) without adding flag/segment restore — consistent with 1.2.
- `Context.Rcx` = the context pointer, not the caller's rcx (documented
  in-code). The unwind transfer skips 0x80, so the target's rcx is the
  context pointer — harmless because the `__except` body at JumpTarget is an
  in-function continuation of the MSVC x64 SEH model (scope-table JumpTarget
  inside the parent function; mirrored by clang), which re-establishes state
  from its own frame, not from rcx; only the `__finally` funclets consume a
  PSP, and they receive it correctly via `call_at_frame`'s arg
  (`dctx->establisher_frame`). Host receipts corroborate (bodies executed,
  results preserved).

### 1.4 Vectored handlers — **FAIL-CLOSED (bound confirmed)**

The binder is an 8-name allowlist (7 kernel32 + `__C_specific_handler`).
`AddVectoredExceptionHandler`/`AddVectoredContinueHandler` resolve to
`MB_PE_BIND_UNRESOLVED` → loader refusal. No partial emulation, no silent
swallow. A mod needing vectored handlers cannot load rather than misbehaving.

### 1.5 C++ EH — **FAIL-CLOSED (bound confirmed)**

`__CxxFrameHandler3/4`, `__intrinsic_setjmp`, etc. are not in the allowlist →
unresolved-import load refusal. A C++-exception mod cannot reach the
dispatcher at all; mixed modules that use C++ EH anywhere fail at load. This
is the correct fail-closed shape: the census personality handles Win64
scope-table SEH only. (Corollary for consumers: "SEH64 works" claims cover
C-level `__try/__except/__finally` modules only.)

### 1.6 Integer-state-only restore — **ABI-SOUND for this route (reasoning recorded)**

`resume_win64` restores rax/rdx/rbx/rbp/rsi/rdi/r8–r15 and enters the target
at `rsp = frame_rsp` exactly like the OS unwind transfer (the pushed
return-address slot is popped by the final `ret`; entry rsp ≡ 0 matches the
NtContinue-to-JumpTarget model — this is NOT the 1.1 callee-entry case; no
violation). Win64 nonvolatile XMM6–15 are not restored, but every frame
between capture and target is win64cc code, which must home nonvolatile XMM
before use per the Win64 ABI, so the target frame's live nonvolatile XMM
survive the dispatch path untouched; `resume_win64` itself touches none.
MxCsr/FltSave carried opaque is therefore sound for integer continuation and
would be a real gap only for a future FP-continuation feature (none exists;
see 1.2).

## 2. PS5-route trampoline contract — explicit checks

| Check | Verdict |
|---|---|
| SysV entry assumption (rdi/rsi/rdx reads) | Valid for the freebsd9 frontend (IR receipt `win64cc_defs: 0` on the naked unit); identical on the host route, so the shuffle is host-exercised |
| Privileged/ syscall instructions in the trio | None (`mov/lea/push/pop/call/ret/pushfq` only) — no syscall-text risk of the CC1 fold0 class |
| Callee-saved discipline | rbx pushed/popped balanced; rbp/r12–r15 untouched by the trio; `resume_win64` (noreturn) restores the full integer callee-saved set from the context |
| Win64 rcx contract for the IAT entry | Kept (`svc_capture_context` body reads rcx); internal caller pins rcx via register-asm — frontend-independent |
| Red zone | None relied on; below-establisher-rsp writes land in ms_abi territory (no red zone). Residual: a POSIX signal delivered to the thread while rsp is inside the guard window could collide with the guard zone — single-threaded software-raise windows are microseconds; named, not actionable |
| Stack switch correctness | Guard zone fully below the establisher rsp (−0x30..−0x1 plus pre-store at −0x8); recovery restores exact pre-call rsp |

## 3. Review verdict

1. The naked-trio register contracts are **route-correct as designed** (SysV
   entry, in-body shuffle, one deliberate Win64-rcx body) — confirmed against
   the IR receipts, not just the prose.
2. **F-1 (guard-call 16-byte alignment inversion) is the one finding that
   can turn into a target crash** for real-world filters/`__finally`
   funclets with aligned SSE spills; it is invisible to the shipped fixture
   census and must be discharged (probe + 0x28→0x30 guard-zone fix + both-route
   compile) by the SEH64 lineage under new ids BEFORE any reliance beyond the
   exercised census — exactly the "target-route review" trigger this review
   was scoped to evaluate.
3. F-2 is a one-line comment-semantics correction for the owning packet.
4. The other four divergences (signal shim, flag/segment restore,
   vectored handlers, C++ EH, FP restore) are fail-closed or ABI-sound as
   shipped and remain documented ceilings — none silently widened.
5. Nothing here creates target evidence: dll8's bounded fixture census on
   console remains the only on-target SEH64 proof path, and TRACK-COMPLETE
   criterion 4's verdict rules are unchanged. This review gives the
   post-dll8 reliance decision (criterion 7) its named evidence base.

Honesty: read-only review; every code claim cites lines read this pass; no
execution, no console contact, no score, no parity claim beyond what is
argued above.
