# x64 S-list initializer: Mac continuation

## Result

An isolated shim stage now publishes `KERNEL32!InitializeSListHead` through a typed, sealed Win64 wrapper. It requires a non-null 16-byte-aligned pointer and zeroes exactly 16 bytes. All five unchanged DLL samples statically import this name. Microsoft's [InitializeSListHead reference](https://learn.microsoft.com/en-us/windows/win32/api/interlockedapi/nf-interlockedapi-initializeslisthead) defines the call, and its [x64 SDK header](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/winnt.h) declares a 16-byte, 16-byte-aligned `SLIST_HEADER`.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-slist-init-20261013-21`, copied from isolated stage 20. Shared candidate, Claude's work, and unchanged DLL binaries were untouched.

## Verification

The focused test first failed because the import was unresolved, then passed **12/12**. It verifies one aligned header is cleared without touching adjacent canaries, reinitialization works, and null/misaligned calls invoke the stop hook without writing. The packaged regressions passed: lazy imports **18/18**, typed imports **18/18**, context bridge **13/13**, registry **5/5**, TEB lifecycle **15/15**, static TLS **19/19**, loader preflight **27/27**, five unchanged DLL read-only TLS preparations, and synthetic fixture **11/11**. The two-file patch passed exact-base scratch replay and the evidence ZIP passed `unzip -t`.

## Handoff and limits

- [Incremental patch](slist-init-increment-20261013.patch)
- [Exact-base manifest](slist-init-manifest-20261013.json)
- [Evidence ZIP](slist-init-evidence-20261013.zip)

This covers initialization only. Push, pop, flush, and direct guest manipulation of the opaque header have not been validated; an aligned but invalid pointer is not safely probed by this shim. Real DLL execution, target correspondence, and PS5 segment/executable-memory behavior remain unresolved. This is **not** a console-only wait.
