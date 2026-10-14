# Stage 62: guarded Win32 critical sections

## Scope

This isolated DLL-loader increment starts from exact stage 61 at `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-virtual-protect-20261013-61`. The new source is `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62`. It leaves the shared candidate, Claude's #2/#4–12 work, kernel code, and original DLL bytes untouched.

The reviewed shim now publishes five sealed Win64 names after both guest memory maps bind: `InitializeCriticalSection`, `InitializeCriticalSectionEx`, `EnterCriticalSection`, `LeaveCriticalSection`, and `DeleteCriticalSection`. A per-shim registry owns host synchronization state for each separately registered, aligned 40-byte x64 guest object. It supports recursive entry by the owner, blocks contending threads, requires balanced leave, and refuses deletion while owned or waited on. Invalid flags, unregistered objects, double initialization, wrong-owner leave, and use after deletion fail closed. The optional spin count is accepted as a guest setting but this host implementation uses a condition variable for contention; it does not claim Windows spin timing or scheduler behavior.

The lifecycle follows the documented [initialization](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializecriticalsectionex), [recursive entry](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection), and [deletion](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-deletecriticalsection) contracts at the admitted boundary. The guest object remains logically opaque. Callers must keep its storage alive and unmoved through deletion, and quiesce guest calls before destroying the shim. `TryEnterCriticalSection`, direct inspection of Windows internal fields, full scheduler fairness, and console behavior remain unverified or unsupported.

## Evidence

- Test-first red result: all five imports could not publish before the implementation. Focused direct and lazy Win64 import checks passed **18/18** normally and **18/18** under AddressSanitizer. They exercise recursive ownership, contention, balanced release, invalid lifecycle, no-debug initialization, and lazy PE import installation.
- Prior virtual-memory checks passed **21/21** normal and AddressSanitizer, prior process-heap checks **18/18** in both modes, and prior thread checks **85/85** in both modes.
- All **15/15** earlier host runners exited zero. The original synthetic end-to-end fixture passed **11/11**, with SHA-256 still `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce`.
- The pinned PS5 SDK compiled all **17** loader units and linked an offline ELF. No console execution is claimed.
- The [four-file patch](guest-critical-sections-increment-20261013.patch) passed exact-stage-61 `git apply --check`, application, and byte comparison. Patch SHA-256: `c14007207c396a64d531ea4c1213f98d69b8ba6b436a9c2f16c016131680554c`. The [manifest](guest-critical-sections-manifest-20261013.json), [host evidence ZIP](guest-critical-sections-host-evidence-20261013.zip), [target manifest](guest-critical-sections-target-build-manifest-20261013.json), and [target evidence ZIP](guest-critical-sections-target-build-evidence-20261013.zip) passed ZIP CRC checks.

## Measured import gap

The [stage 62 unchanged-DLL name-match report](DLL-STAGE62-IMPORT-MATCH-20261013.md) compares this source to the five unchanged sample import tables. Reviewed catalog names now match 25/110 TrueFOV, 20/200 ZeroCompanyMandoWardrobe, and 12/176 TFWWorkbench direct imports; the new critical-section rows add 4, 4, and 3 names respectively. The other two sample counts are unchanged. A name match is only a prioritization signal: it does not prove call semantics, provide a dependency DLL, admit a PE, or demonstrate gameplay.

## Status and next work

This is **not a console-only wait**. The dominant remaining direct-import gaps in these samples are CRT/UCRT, UE4SS, and additional Win32 behavior. The default GS backend still refuses real guest entry until its console round trip. Automatic authenticated PC-to-PS5 target matching, #1 register-read behavior, #3 patch/detour behavior, and actual gameplay remain unverified. Unsupported DLL behavior must continue to refuse precisely.
