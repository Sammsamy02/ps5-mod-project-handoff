# Common Win32 identity and UTC time imports: Mac continuation

## Result

An isolated stage now offers three more reviewed KERNEL32 rows through the existing context-bound, sealed Win64 import bridge:

| Import | Guest signature | Host behavior checked |
| --- | --- | --- |
| `GetCurrentProcessId` | `DWORD ()` | Current host process ID |
| `GetCurrentThreadId` | `DWORD ()` | Stable process-wide ID for each active host thread, including across shim instances |
| `GetSystemTimeAsFileTime` | `VOID (LPFILETIME)` | UTC FILETIME from the host real-time clock, with null output stopped through the mod hook |

All five unchanged DLL samples statically import these three names, as recorded in [the existing import census](DLL-IMPORT-SURFACE-20261013.json). The signatures and identity contracts follow Microsoft's [process ID](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocessid) and [thread ID](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid) documentation. The output uses the documented [UTC file-time API](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemtimeasfiletime) and [FILETIME epoch/layout](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime).

The source is at `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-common-win32-identity-time-20261013-16`. The stage was copied from `cg-typed-import-install-20261013-15`; the shared candidate, Claude's work, and the original DLL binaries were untouched.

## Verification

The new host test first failed on the three unsupported catalog rows, then passed after implementation. The packaged run recorded **18/18** typed import checks, **13/13** bridge checks, **5/5** registry checks, **12/12** TEB lifecycle checks, **19/19** static TLS checks, and **27/27** loader preflight cases. Read-only TLS preparation passed on the five unchanged DLLs, with their bytes unchanged. The synthetic fixture passed **11/11** checks and its pinned hash was unchanged. The two-file incremental patch passed `git apply --check`, scratch apply, and byte comparison against the staged files. The evidence ZIP passed `unzip -t`.

The host test verifies that each new import remains unresolved before sealing, a Mac RW page becomes RX before publication, calls cross from Win64 to SysV with the correct arity, process/thread identity is stable, FILETIME has the correct UTC epoch and DWORD layout, and null FILETIME output invokes the stop hook.

## Handoff

- [Incremental patch](common-win32-identity-time-increment-20261013.patch)
- [Exact-base manifest](common-win32-identity-time-manifest-20261013.json)
- [Evidence ZIP](common-win32-identity-time-evidence-20261013.zip)

This adds three callable import implementations to the isolated shim. The PE loader still does not install these wrappers automatically, and the unchanged DLLs still refuse before execution because required Windows/CRT/UE4SS imports, static TLS/GS entry, and target correspondence remain unresolved. The Mac host cannot prove PS5 executable-memory sealing or guest segment behavior. It is **not** a console-only wait.
