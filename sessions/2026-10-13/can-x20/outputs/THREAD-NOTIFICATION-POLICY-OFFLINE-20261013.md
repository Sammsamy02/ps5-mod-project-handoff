# Module-aware thread notifications: Mac continuation

## Result

An isolated shim stage now publishes `KERNEL32!DisableThreadLibraryCalls` with a module-handle policy registered by the loader adapter. A null or unregistered handle returns `FALSE`. A module marked with active static TLS also returns `FALSE` and keeps its thread notifications, as required by Microsoft's [function reference](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls). A registered non-TLS module returns `TRUE` only after its lifecycle callback has disabled thread attach/detach notifications. Process attach/detach still run, and the lifecycle rejects wrong-thread/wrong-bank TEBs.

All five unchanged DLL samples statically import this API and have PE TLS directories. The stage sets `ERROR_INVALID_PARAMETER` (87) on failure; the public reference describes the failure conditions but does not specify an exact error code, so that code is an emulation choice pending Windows comparison.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-thread-notification-policy-20261013-22`, copied from isolated stage 21. Shared candidate, Claude's work, and unchanged DLL binaries were untouched.

## Verification

The focused test first failed to compile because policy registration and lifecycle suppression did not exist. With the implementation it passed **15/15**, including invalid handles, adapter refusal, a `DllMain` call for a non-TLS module, static-TLS refusal, preservation of thread notifications for that module, process notifications after suppression, duplicate-handle refusal, and reset after a failed process attach. The packaged regressions passed: S-list/sync **12/12**, lazy imports **18/18**, typed imports **18/18**, context bridge **13/13**, registry **5/5**, TEB lifecycle **15/15**, static TLS **19/19**, loader preflight **27/27**, five unchanged DLL read-only TLS preparations, and synthetic fixture **11/11**. The six-file patch passed exact-base scratch replay; the evidence ZIP passed `unzip -t`.

## Handoff and limits

- [Incremental patch](thread-notification-policy-increment-20261013.patch)
- [Exact-base manifest](thread-notification-policy-manifest-20261013.json)
- [Evidence ZIP](thread-notification-policy-evidence-20261013.zip)

The current PE loader does not yet call the policy-registration API with validated mapped-module metadata, and it still refuses real TLS/GS images. This stage demonstrates module-aware behavior through an isolated host adapter; it does not show unchanged DLL execution or PS5 segment behavior. Other imports and target correspondence remain unresolved. This is **not** a console-only wait.
