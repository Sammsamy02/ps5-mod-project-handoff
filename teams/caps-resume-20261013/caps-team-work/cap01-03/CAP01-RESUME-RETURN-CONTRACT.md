# Capability #1: pinned return-contract audit (Mac-only)

2026-10-13. Scope: `probe/KernelReadProbe.c` (v0),
`probe/KernelReadProbe-v1.c`, its existing console log, and
`../what/outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip`
(SHA-256 `b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6`).
Archive citations below use member paths relative to `kstuff-lite-1.11/`.
No target, kstuff, or console changes were made.

## Verdict

V1 removes a definite *classifier* mistake: a successful high-bit LSTAR
value is negative as `int64_t`, so v0's sign check necessarily rejects it
(`probe/KernelReadProbe.c:61-75,126-133`). V1 carries raw `uint64_t` RAX
and a separate CF bit (`probe/KernelReadProbe-v1.c:84-103,155-163`).
This is the right local correction **if** the kernel returns a successful
LSTAR read with CF clear. It is not yet an end-to-end proof that the observed
v0 LSTAR failure was only that classifier bug: the v0 log has just
`RDMSR_LSTAR verdict=FAIL` and no raw RAX or CF
(`probe/EVIDENCE-CONSOLE-20261013.log:1-5`). A real RDMSR failure could
produce the same v0 line.

The pinned source proves how the handler supplies a value or error to the
kernel's syscall exit, but the actual `syscall_after` instructions are only
referenced as an external kernel address here. It does not prove the
resulting user-visible CF polarity or how CF is preserved through the
exit. The use of `=@ccc` in pinned `ps5-kstuff/debug-reader.c:21-36`
shows that kstuff's own client assumes a CF-based error convention; it is
not a proof of that convention on this path.

## Exact source path

| Hop | Pinned evidence | Established behavior |
| --- | --- | --- |
| Caller | `probe/KernelReadProbe-v1.c:84-103,105-115` | The inline `syscall` uses `RAX=(sub<<32)|0x27`, sends arguments in the documented registers, and captures raw RAX and `=@ccc` separately. |
| Intercept | `ps5-kstuff/uelf/main.c:429-451,65-74` | The intercepted `syscall_before` path calls `handle_syscall`; only `allow_kekcall && IS_PPR(getppid)` reaches `handle_kekcall`. The syscall number comes from the saved user frame. |
| CHECK | `ps5-kstuff/uelf/kekcall.c:102-107` | Sub-operation `0xffffffff` writes `args[RAX]=0` and returns handler error `0`. |
| RDMSR | `ps5-kstuff/uelf/kekcall.c:59-62`; `ps5-kstuff/uelf/utils.c:799-811` | Sub-operation 3 runs `rdmsr(args[RDI], &args[RAX])`. The helper joins RDX:RAX into the full 64-bit value on success; it returns failure when the gadget fails or remains at its start. The handler converts failure to `EFAULT`. |
| READ_DR | `ps5-kstuff/uelf/kekcall.c:21-47` | Sub-operation 1 stages six debug registers and a later `copyout`; its direct setup errors return `EFAULT`. This operation has an additional resumed kernel copyout path. |
| Return handoff | `ps5-kstuff/uelf/main.c:74-82` | For a handled sub-operation, success copies `args[RAX]` to `td_retval`; a copy failure becomes `EFAULT`. It then sets `regs[RAX]=err`, skips its injected return address, and resumes at `syscall_after`. |
| Trap restoration | `ps5-kstuff/uelf/main.c:493-512`; `ps5-kstuff/uelf/structs.h:22-27` | The user ELF copies the trap register frame in and out, including an EFLAGS slot. The source does not show what `syscall_after` does with `err`, `td_retval`, or EFLAGS before userspace returns. |

The archive has per-firmware address definitions for `syscall_after`
(for example `prosper0gdb/offsets/11_00.h:35-36`), but no implementation of
that kernel routine. `ps5-kstuff/uelf/main.c:77-81` is therefore the last
source-proven step in the result/error handoff. The fact that the return
path sets `regs[RAX]=err` does not mean user RAX must be that same value:
`td_retval` was also written and the subsequent kernel code is outside this
source package.

## V1 ambiguities and staged correction

1. V1 treats CF as authoritative for `classify_msr_result`, yet CHECK
   accepts `rc==0` without testing CF (`probe/KernelReadProbe-v1.c:184-205`),
   and READ_DR accepts `rc==0` plus changed output bytes without testing CF
   (`:294-314`). Thus an observed `(RAX=0, CF=1)` can pass either gate.
   The staged `probe/CAP01-RESUME-KernelReadProbe-v2.c` requires both zero
   RAX and clear CF, uses the same result for CHECK's skip/exit decision,
   and records CF-set failures. It has a distinct probe ID and log path.
   V1 source and its console evidence were preserved.
2. V1's fallback `0 < rc < 0x1000 => error` (`:155-163`) is a heuristic,
   not a general RDMSR return contract. A legitimate low nonzero MSR value
   is indistinguishable from errno in raw RAX if CF is unavailable. Its own
   EFER comment allows a `0xD01`-class value (`:235-239`), which the fallback
   would reject. The recorded EFER value was `0x11d01` and does not hit this
   particular boundary (`probe/EVIDENCE-CONSOLE-20261013.log:4`). V2 leaves
   this heuristic unchanged because removing it before CF is verified
   would also remove its protection against an errno-only exit.
3. V1 describes EFER as evidence-only (`:235-239`) but increments
   `g_failures` if the classifier rejects it (`:241-253`). This does not
   affect the recorded `0x11d01` case. It remains a separate policy choice
   for any later probe revision.

The v2 correction is **source-only**. It was intentionally not compiled,
host-tested, built for the target, or run on a console in this audit. It is
not a console-ready artifact. A later authorized run needs a fresh log with
raw RAX and CF for any failure; the previous v0 outcome must not be counted
as proof of v1/v2 success. None of these probes reads an arbitrary kernel
virtual address, so capability #1's kernel-VA read remains open.
