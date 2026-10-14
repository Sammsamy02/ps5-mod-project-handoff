# Stage 34: validated dynamic lookup of attached modules

**Status:** isolated Mac host work; not a console-only wait or universal DLL compatibility.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dynamic-module-lookup-20261013-34`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-directory-dependency-20261013-33`

The reviewed shim now publishes Win64-callable `GetModuleHandleA` and `GetProcAddress` only after its caller installs a byte reader that validates guest addresses. Strings are copied through that reader, one byte at a time, with a 4096-byte limit. Named module lookup uses the existing case-insensitive registry, recognizes the usual extensionless `.dll` form, and can return a separately configured main executable handle for `GetModuleHandleA(NULL)`. Procedure lookup requires a known module handle and resolves exact-case names or low-word PE ordinals through the existing registry and bounded forwarder logic. Missing modules, procedures, invalid handles, and malformed pointers return null with named Win32 last-error codes. No raw guest string pointer is passed to `strlen` or `strcmp`.

Focused x64 Mac host checks passed **19/19**, including actual calls through sealed Win64 context trampolines, reviewed built-in export calls, module and ordinal lookup, error codes, invalid guest pointers, and a missing-reader refusal. All **15** offline regression runners exited 0, and the original synthetic end-to-end fixture passed **11/11**. The four-file patch passed `git apply --check`, exact-stage-33 scratch application, byte-for-byte replay, and ZIP CRC verification.

- [Incremental patch](dynamic-module-lookup-increment-20261013.patch)
- [Base/stage hash manifest](dynamic-module-lookup-manifest-20261013.json)
- [Regression and replay evidence](dynamic-module-lookup-evidence-20261013.zip)

The new focused test uses registered module callbacks, not an attached unchanged gameplay DLL. The production adapter has not supplied a guest-memory reader or main executable handle. Built-in HMODULE values are stable opaque tokens for these APIs, not PE image bases for direct inspection. `LoadLibrary`, `GetModuleHandleW`, new-thread behavior, CRT/UE4SS compatibility, target matching, and PS5 guest execution remain unresolved. No console was contacted and no original DLL or shared candidate was edited. The API contract follows Microsoft's [GetProcAddress](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress), [GetModuleHandleW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandlew) (the Unicode version's module-name rules), and [system error codes](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-) documentation.
