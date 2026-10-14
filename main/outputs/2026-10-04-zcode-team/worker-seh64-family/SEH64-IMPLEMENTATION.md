# SEH64-IMPLEMENTATION.md — worker-seh64-family

Date: 2026-10-04 · First implementation of the recovered design
(`outputs/2026-10-04-zcode-team/worker-seh64-design/DESIGN.md`, design-only
until this packet). Bounds honored: no console contact, no target execution,
no nested subagents, writes only under `worker-seh64-family/`. This family is
NOT integrated into the 7-family tree (static TLS remains
worker-static-tls-7fam's; its directory was never read or written).

## 1. What was built

An OWN vendored copy of the console-proven combined tree
(`worker-combined-dll-allfam/vendor/{windows-candidate,windows-services,
windows-execution,windows-image,native-host/archive/link_layout.py}`) plus the
SEH64 family as an eighth, standalone opt-in (`--seh64`, requires `--entry`).

### Family units (new, `vendor/windows-services/`)

- `mb_win_seh64.h/.c` — pure-logic owner (freestanding, native CC, no
  allocation): exact Win64 data contracts with `_Static_assert`s (CONTEXT
  0x4D0 with Rax=0x78/Rsp=0x98/Rip=0xF8/FltSave=0x100; EXCEPTION_RECORD
  0x98; RUNTIME_FUNCTION 3xu32; self-consistent DISPATCHER_CONTEXT whose
  personality-consumed fields keep the Win64 position contract); function-
  table registry (fixed 8 slots: image tables + dynamic tables, sorted-range
  validation, handle = slot+1, double-remove refused); binary-search
  `RtlLookupFunctionEntry`; `RtlVirtualUnwind` core (UNWIND_CODE wire format
  pinned: low byte = prologue offset, bits 8-11 opcode, bits 12-15 operand;
  ALLOC_LARGE/SMALL, PUSH_NONVOL, SAVE_NONVOL(+FAR), SET_FPREG establisher
  semantics — EstablisherFrame = frame-register value when SET_FPREG is
  present, else the frame's rsp (Win64 contract, machine-verified against
  clang funclets); XMM homes skipped; PUSH_MACHFRAME and >8-deep chains
  refused as UNSUPPORTED; bounded epilogue scan); `__C_specific_handler`
  decision core (scope tables, filter RVA 0 = `__finally`, 1 = constant
  EXECUTE_HANDLER, else a real filter invoked in the establishing frame;
  noncontinuable refusal; TARGET_UNWIND + target selection); two-pass
  dispatch driver (search → unwind → transfer) with all ms_abi transitions
  injected through `mb_win_seh64_control`; `SetUnhandledExceptionFilter`
  previous-pointer state owner; unhandled path runs the last-chance filter
  and reports — never swallows (anti-fake anchor).
- `mb_win_seh64_abi.h/.c` — session lifecycle (vm/crt-startup pattern),
  binder and Win64 entries. Bound names (ASCII-folded, ordinals unresolved):
  kernel32.dll `RtlCaptureContext`, `RtlLookupFunctionEntry`,
  `RtlVirtualUnwind`, `RtlAddFunctionTable`, `RtlRemoveFunctionTable`,
  `SetUnhandledExceptionFilter` (census stack-walker kit + state owner) plus
  `RaiseException` and vcruntime140.dll `__C_specific_handler` (census
  personality) — the latter two are task-directed substrate entries beyond
  the census rows, flagged in the binder comment and receipt. The exception
  directory of the bound module is pinned at first SEH64 bind and resolved/
  registered lazily at first thunk use (the loader fills `mb_pe_info` while
  binding, so bind-time info is not trustworthy), then removed at session
  end (lease discipline: no dangling tables). Control plane: native-CC
  wrappers (`call_personality`, `call_at_frame`, `resume`) around naked
  asm trampolines — guard-frame invocation (RSP = frame_rsp − 0x28, Win64
  shadow space, return into a recovery stub), NtContinue-equivalent integer
  context transfer, and RtlCaptureContext-semantics capture of the caller.

### Loader amendment (design §7; `vendor/windows-execution/`)

`pe_module.c` admission admits the exception directory (index 3) only when
compiled with `MB_PE_SEH64`: the requirement bit `MB_PE_REQUIRES_EXCEPTION`
is accepted, the directory must lie inside the image, and the historical
refusal stands unchanged without the define. `pe_module.h` documents the
amendment and that the absolute "no exceptions across this boundary" clauses
remain verbatim when SEH64 is not compiled in.

### Fixture + integration (`vendor/windows-candidate/`)

- `tests/worker-seh64.h` — hook riding the consumer `LoaderExercise` ACTIVE
  window, unique codes **-130..-137**: walker kit over the image's real
  MSVC-emitted .pdata/.xdata (-130 capture sanity, -131 in-image lookup,
  -132 unwind caller-context containment + dispatcher fields), same-frame
  guarded raise/dispatch/recover with a `__except` body funclet (-133
  handled, -134 frame state after recovery), cross-frame raise through an
  unwound callee with a `__finally` running during unwind (-135), dynamic
  registration over a non-PE bridge address with identity-verified lookup
  (-136), removal/double-remove/previous-filter semantics (-137). Layout
  mirrors are offset-contract with the owner header. `noinline,
  disable_tail_calls` keeps real frames (a tail call turned the first probe
  into a jmp thunk — caught and fixed during host bring-up).
- `tests/worker-seh64-kernel.def` (7 kernel32 names) +
  `worker-seh64-vcruntime.def` (`__C_specific_handler`).
- `fixtures/entry_consumer.c` hook call; `owned_runtime.{c,h}` session/
  owner/report/stages 55-56/capacity tiers/binder placement (after the
  loader bridge, with the combined-families comment); `result_report.c`
  `seh64` key; `test_entry_candidate.c`/`test_worker_candidate_failures.c`
  count formulas; `build.py --seh64` (profile define, source lists, def
  libs, fixture unwind tables forced ON — the msvc target emits no .pdata by
  default; directory-3 admission in the fixture check; receipt profile;
  IR win64cc expectation 7; result-key asserts).

## 2. Machine-verified behavior (host x86_64 route, Rosetta)

`--host-only --entry --workers --seh64` → `OWNED_CANDIDATE_HOST_CONTROLS_
PASSED` (`out-host-validation/133de5d3886448689e5c75281aa79dab/receipt.json`):

- The raise path is real: `RaiseException` (IAT) → capture → two-pass
  dispatch over the registered image table → `__C_specific_handler` reached
  through the PE's own import thunk (the .xdata handler RVA resolves into the
  mapped image) → scope selection → unwind pass running the `__finally`
  funclet (rcx = establisher frame per the clang PSP convention) → target
  transfer (rip = body, rsp = frame rsp, nonvolatiles as live) → `__except`
  bodies execute → execution continues normally → base results [86,93]
  unchanged, entry attach/detach [7,7]/[0,0], worker begin [258,259] and
  result 0x81234567 all preserved.
- Both driver modes (normal/release) + worker-cleanup-failure controls pass;
  consumer imports 29 = 16+5+8 (exact formula), helper 8; `seh64 {"enabled":1}`;
  cleanup retired {pending:0, retired:1}; 22 recorded commands; 73 hashed
  sources re-verified at the end.
- A standalone data-plane probe (`host-probe.c`, diagnostic aid kept in the
  worker dir) additionally verified lookup + virtual-unwind directly over
  the produced DLL's bytes for a leaf (ALLOC_SMALL) frame and a guard frame.

Bugs found and fixed during bring-up (all receipted in the run history):
UNWIND_CODE bit layout reversed; eager `reg_home` misread ALLOC_SMALL's size
operand as a register; `mb_pe_info` not yet populated at bind time (lazy
directory resolution); tail-call-optimized probe frame; missing service_end
hook (owner close stayed BUSY through cleanup); capacity/binder gating for
--entry without --workers.

## 3. SDK composition (compile-only)

`--entry --workers --seh64 --sdk … --report-path /data/modbridge-owned-dll-
candidate.jsonl` under a 600s guard → `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_
EXECUTED` (`out-sdk-composition/5f7e67b36ce04bb1b0fffc0a20266240/receipt.json`):
SDK AMD64 ET_DYN ELF, 222408 bytes, sha256 9d74b27bd2882b30…, genuine crt1
`_start`; 63 commands; 73 sources digested; dynamic layout validated; the
target-side report path is compiled into the profile only (never opened by
the build); only warnings are the two pre-existing rust-lld `/align` notices.

## 4. Honest boundaries and divergences (all documented in-code)

1. **Guard-rsp contract (design H2)**: filters/bodies invoked at the frame
   run with RSP = frame_rsp − 0x28; bodies that address parent locals through
   the guard RSP are unsupported. Fixture bodies/filters touch globals and
   frame memory only; the transfer path restores the exact frame RSP/RBP, and
   clang funclets (rbp-relative, PSP-in-rcx) were machine-verified.
2. **Naked trampolines on the PS5 route**: the freebsd9/PS5 backend drops
   `ms_abi` on naked definitions (IR-verified: exactly 7 win64cc defines —
   the C-level svc functions; the naked trio takes the default SysV-style
   entry and shuffles registers in-body; the PE-bound capture entry keeps
   the rcx contract and its one internal caller sets rcx explicitly). These
   register contracts are host-verified only; the ELF is a compiled
   candidate and was never executed anywhere.
3. **Integer-state restore only** (XMM/MxCsr/FltSave carried opaque);
   continue-execution resume and the signal→exception shim (design M2/H3)
   are out of scope — `RaiseException` returns after reporting when nothing
   handles; no vectored handlers, callback/growable tables, stack-overflow
   dispatch, or full COLLIDED_UNWIND (design phase 4).
4. **Pass-1 disposition**: the driver embeds the decision core directly for
   the search pass; the personality path through the PE thunk is genuinely
   exercised in the unwind pass and is the bound census surface.
5. `RaiseException`/`RtlAddFunctionTable`/`RtlRemoveFunctionTable` are
   census-not-observed; bound per the worker packet's directed minimum
   (raise/dispatch/recover + dynamic registration), flagged in-code.
6. No Windows ntdll/VCRUNTIME parity claim; no console, PRX, or PS5-device
   evidence of any kind; no >8 quality score claimed anywhere.
