# Guest TEB last-error coherence: Mac continuation

## Result

An isolated stage shares the per-host-thread Win32 last-error value between the imported `GetLastError`/`SetLastError` handlers and the guest x64 TEB prefix. A TEB refresh publishes the current DWORD at offset `0x68`. While a guest lifecycle callback or DLL entry runs, imported reads observe a direct write to that field and imported writes update the field. On return, the value is copied back to the host-thread slot, and a nested binding restores the prior TEB view.

The offset is part of the **specific x64 TEB profile used by this prototype**, not a guarantee for arbitrary Windows layouts. Microsoft's public [TEB documentation](https://learn.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-teb) warns that its definition can change and discourages direct access. The Windows API contract for [GetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror) remains per-thread.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-teb-last-error-coherence-20261013-19`, copied from isolated stage 18. Shared candidate, Claude's work, and unchanged DLL binaries were untouched.

## Verification

The lifecycle test first failed because the shared state header did not exist. The new host run passed **15/15**, including direct guest TEB modification in a Win64 callback, subsequent imported-state read, imported-state write, and agreement after the fake segment switch leaves. Existing lazy import **18/18**, typed import **18/18**, context bridge **13/13**, import registry **5/5**, static TLS **19/19**, loader preflight **27/27**, and synthetic fixture **11/11** checks passed. Read-only TLS preparation again passed on five unchanged DLLs with source bytes unchanged. The four-file patch passed exact-base scratch replay and the evidence ZIP passed `unzip -t`.

## Handoff and limits

- [Incremental patch](teb-last-error-coherence-increment-20261013.patch)
- [Exact-base manifest](teb-last-error-coherence-manifest-20261013.json)
- [Evidence ZIP](teb-last-error-coherence-evidence-20261013.zip)

The Mac test uses a recording segment switch, not real GS-base transitions. The production placeholder still refuses guest entry, the PE loader still refuses real TLS/GS images, and unchanged DLLs do not execute. This is **not** a console-only wait.
