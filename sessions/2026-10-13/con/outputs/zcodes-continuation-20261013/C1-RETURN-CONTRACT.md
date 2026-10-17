# Probe return contract and remaining evidence gap

Mac-only analysis, 2026-10-13. No console contact or new console evidence. Source IDs resolve to absolute paths and complete SHA256s in SOURCE-PINS.json. Original artifacts are unchanged. The handoff supplied context; it did not authorize console actions, kernel changes, external dispatch, or publication.

## Verdict

**CF is not established as always set on error by the pinned source. Its user-visible value depends on the kernel syscall-exit implementation outside this archive.** This is an unresolved proof dependency, not evidence that CF is never set. Do not replace the missing proof with the client’s assumption.

Trace in S1 (archive prefix `kstuff-lite-1.11/`):

1. `ps5-kstuff/uelf/main.c:429-451` intercepts syscall dispatch, pushes the `syscall_after` continuation and calls `handle_syscall(regs,1)`. The alternative int3 dispatch at 356-364 reaches the same handler.
2. At 65-74, eligible PPR getppid reads the caller frame and dispatches the high RAX sub-op. `kekcall.c:59-62` returns 0 on successful RDMSR and EFAULT on failure; CHECK returns zero at 102-106.
3. `main.c:75-81` copies a successful payload into `td_retval`, writes the service errno into the intercepted **kernel** RAX, pops the synthetic return and resumes at `syscall_after`. It does not directly set user RAX to that errno, nor set the caller’s CF. Thus “errno in regs[RAX]” alone is not a user-return contract.
4. `syscall_after` is an external kernel address (firmware offset declarations in `prosper0gdb/offsets/*.h`), not a function implemented in this source archive. No archived body establishes its error translation, user trap-frame CF update or final preservation through resume. `doreti_iret` handling in main.c:453-466 restores a frame; it does not prove the omitted syscall-exit processing.
5. `debug-reader.c:23-46` consumes CF as the expected ABI. That is corroborating intent, not a producer-side proof. ENOSYS follows the original getppid route; failed initial frame reads also fall through. Neither is arbitrary memory-read support.

## Classifier consequence

Retain the high-bit fix: a canonical LSTAR address must never be classified by signedness. S2’s small-positive fallback rejects EFAULT=14 if it reaches user RAX even with CF clear. It is **not sufficient as a general RDMSR classifier**: successful values 1..4095 are falsely rejected, and stale CF can falsely reject any success. CHECK additionally requires raw zero and CF clear. Current EFER/LSTAR/nontrivial TSC expectations are narrower than arbitrary MSR support.

Fresh S3 host run: **13 assertions PASS**, exit 0. The source prints `cases=14` as a literal; count the assertion lines, not that summary. This checks classification policy only and cannot establish how the target produces CF. The test also passes classification and `v` as separate arguments to `expect`; sequence the call and value check in a future revision for compiler-independent evidence.

Closing C1 requires already-authorized offline evidence for the exact firmware syscall-exit path, or a future separately authorized diagnostic recording raw RAX+CF for success and error. No such console run was made here; C1 stays **partially resolved**. Do not rerun v0 or relabel old logs as v1.

## Loading

No payload was loaded. Existing v1 build status is historical.

## Target resolution

The intercept and sub-op dispatch are source-located; the target kernel syscall-exit body is missing from this proof.

## Working patch behavior

Not exercised; arbitrary kernel-VA reads remain unestablished.
