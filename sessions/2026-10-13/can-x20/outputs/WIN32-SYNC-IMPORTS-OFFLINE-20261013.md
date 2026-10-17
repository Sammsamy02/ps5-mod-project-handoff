# Win32 synchronization imports: Mac continuation

## Result

An isolated shim stage now publishes six typed KERNEL32 imports: `AcquireSRWLockExclusive`, `ReleaseSRWLockExclusive`, `AcquireSRWLockShared`, `ReleaseSRWLockShared`, `SleepConditionVariableSRW`, and `WakeAllConditionVariable`. It creates process-wide host lock/condition state on first use of a guest object address, supports exclusive and shared ownership, releases the SRW lock during a condition wait, and reacquires it before return. A timed-out wait returns `FALSE` and sets `ERROR_TIMEOUT` (1460) in the per-thread last-error state. Invalid ownership or arguments stop the mod through its hook.

The five unchanged DLL samples all import exclusive acquire/release, condition wait, and wake-all; one also imports shared acquire/release. The intended behavior follows Microsoft's [SRW lock](https://learn.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks), [condition wait](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablesrw), and [wake-all](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakeallconditionvariable) contracts.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-win32-sync-20261013-20`, copied from isolated stage 19. Shared candidate, Claude's work, and original DLL binaries were untouched.

## Verification

The focused test initially failed because the six imports were unresolved. After implementation, a new reentrancy test caught a deadlock: the stop hook was called while the condition mutex was held. That test timed out before the fix and now passes. The final Win64 synchronization checks passed **9/9**, including exclusive blocking, two-waiter wake-all, timeout and lock reacquisition, shared readers, shared timeout, and invalid-call refusal. Ten repeated contention runs also passed.

The packaged regressions passed: lazy imports **18/18**, typed imports **18/18**, context bridge **13/13**, registry **5/5**, TEB lifecycle **15/15**, static TLS **19/19**, loader preflight **27/27**, five unchanged DLL read-only TLS preparations, and the synthetic fixture **11/11**. The pinned fixture hash stayed unchanged. The three-file patch passed exact-base scratch replay; the evidence ZIP passed `unzip -t`.

## Handoff and limits

- [Incremental patch](win32-sync-increment-20261013.patch)
- [Exact-base manifest](win32-sync-manifest-20261013.json)
- [Evidence ZIP](win32-sync-evidence-20261013.zip)

The host state is keyed by guest object address and retained for process lifetime. This has not been checked against DLL code that directly manipulates the internal SRWLOCK/condition-variable bits or reuses object addresses after unload. Mac thread and fake-segment checks do not validate PS5 scheduling or GS behavior. Other required Windows/CRT/UE4SS imports, target correspondence, and real TLS/GS loader entry remain unresolved. This is **not** a console-only wait or unchanged DLL execution proof.
