# Capability #1: pinned-source carry contract verdict

## Evidence examined

The pinned `kstuff-lite-v1.11-source.zip` at `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip` has SHA-256 `b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6`. The paths below are inside that archive.

| Path and lines | Observation |
| --- | --- |
| `ps5-kstuff/uelf/main.c:65–84` | The PPR `getppid` interception calls `handle_kekcall`. For a handled sub-op, it writes `err` to `regs[RAX]`, advances the stack, and resumes at external `syscall_after`. Successful result data is separately copied to `td_retval`. This branch does not set or clear `regs[EFLAGS]`. |
| `ps5-kstuff/uelf/kekcall.c:57–62,101–106` | RDMSR returns the MSR value through `args[RAX]` on success; CHECK sets it to zero. |
| `ps5-kstuff/uelf/main.c:429–452` | The trap handler intercepts `syscall_before`, pushes `syscall_after` as a return address, then calls `handle_syscall`. |
| `ps5-kstuff/debug-reader.c:21–46` | The supplied client captures carry after `syscall` and negates the RAX value when carry is set. This is evidence of a client assumption, not a proof of the producer's return flags. |

The archive declares `syscall_before` and `syscall_after` as external firmware addresses; it does not contain the implementation of the resumed kernel syscall-exit path. The only `EFLAGS` writes in this part of `main.c` are unrelated trap handling at lines 344 and 424. Consequently, the pinned source alone cannot establish whether the user-visible carry flag is **always**, **never**, or **conditionally** set for a kekcall error. The verdict remains **unknown pending console raw RAX/CF evidence or the matching firmware exit-path implementation**.

The unchanged v2 probe captures raw RAX and CF separately and correctly avoids classifying a kernel-half LSTAR address as an error merely because bit 63 is set. Its small-positive-RAX fallback detects an errno returned directly in RAX when CF is clear. It is a probe-specific heuristic, not a complete return-contract proof: a different transformation such as a high-bit error encoding would require fresh classification. The new probe has not run on a console. This source verdict does not establish arbitrary kernel-memory reads; the current sub-ops expose registers only.

## Next acceptance gate

Run the already target-built v2 probe once under the existing console authorization gate and retain raw RAX/CF for CHECK, valid RDMSR values, and a controlled failing operation. Reconcile those observations with the exact firmware build's syscall-exit path before any broad carry-based classifier is admitted. Do not infer console success from the Mac control-flow tests or the PS5 compile/link result.
