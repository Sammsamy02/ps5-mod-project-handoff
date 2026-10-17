# Per-thread Win32 last-error imports: Mac continuation

## Result

The isolated shim now publishes reviewed `GetLastError` and `SetLastError` signatures through the on-demand sealed Win64 bridge. The value is a process-wide, per-host-thread DWORD, so multiple shim instances on one thread see the same error and another thread has its own value. All five unchanged DLL samples statically import `GetLastError`; one also imports `SetLastError`, according to the [existing import census](DLL-IMPORT-SURFACE-20261013.json). This follows Microsoft's [GetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror) and [SetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setlasterror) thread-local contract.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-thread-last-error-20261013-18`, copied from isolated stage 17. The shared candidate, Claude's work, and original DLL binaries were untouched.

## Verification

The new checks failed on unresolved imports before implementation, then passed. The lazy import test passed **18/18**, covering initial zero, `SetLastError`/`GetLastError` round-trip, independent worker value, persistence across `Sleep`, and shared value across two shim instances. Existing typed import **18/18**, context bridge **13/13**, import registry **5/5**, TEB lifecycle **12/12**, static TLS **19/19**, loader preflight **27/27**, and synthetic fixture **11/11** checks passed. Read-only TLS preparation again passed on five unchanged DLLs without modifying their bytes. The two-file patch passed exact-base scratch replay; the evidence ZIP passed `unzip -t`.

## Handoff and limits

- [Incremental patch](thread-last-error-increment-20261013.patch)
- [Exact-base manifest](thread-last-error-manifest-20261013.json)
- [Evidence ZIP](thread-last-error-evidence-20261013.zip)

This supplies the two import-call semantics. It does not yet synchronize the value with direct guest TEB `LastErrorValue` access, and other APIs such as condition-variable waits still require their own error-setting behavior. It is **not** a console-only wait or unchanged DLL execution proof.
