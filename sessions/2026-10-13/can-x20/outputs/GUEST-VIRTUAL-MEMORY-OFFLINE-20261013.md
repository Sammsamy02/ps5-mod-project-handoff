# Stage 60: guarded VirtualAlloc and VirtualFree

## Scope

This isolated DLL-loader increment starts from the exact stage 59 source at `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-heap-resize-20261013-59`. The new source is `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-virtual-memory-20261013-60`. It does not edit the shared candidate, Claude's #2/#4–12 work, kernel code, or original DLLs.

The previous `VirtualAlloc` body allocated untracked `calloc` memory and was not guest-callable; no `VirtualFree` catalog row existed. Stage 60 replaces it with an owned anonymous page-region manager and publishes sealed Win64 `VirtualAlloc` and `VirtualFree` wrappers only after both guest read and write maps bind. New committed pages are zeroed by the anonymous mapping. Read-only pages enter only the read map; read/write pages enter both maps. `MEM_RELEASE` revokes registrations before unmapping, and invalid or double release refuses. Direct and lazy PE-style import paths are tested.

Admitted calls: null-address `MEM_RESERVE`, `MEM_COMMIT`, or `MEM_RESERVE | MEM_COMMIT`; exact whole-region commit of a previously reserved region; `PAGE_READONLY` and `PAGE_READWRITE`; whole-region base-address `MEM_RELEASE` with size zero. This is a bounded subset of [Microsoft's VirtualAlloc contract](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc) and [VirtualFree contract](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree). Fixed-address reservation, partial-region commit, decommit, protection changes, executable pages, advanced allocation flags, and Windows allocation-granularity equivalence remain unsupported. These cases refuse; no broad virtual-memory compatibility is claimed. The caller must quiesce direct guest users before release or shim destruction.

## Evidence

- Focused real Win64 thunk checks: **15/15** normal and **15/15** AddressSanitizer. The test was red before implementation because the imports could not publish.
- Prior process-heap checks: **18/18** normal and **18/18** AddressSanitizer; prior thread path: **85/85** normal and **85/85** AddressSanitizer.
- All **15/15** prior host runners exited zero. The unchanged synthetic fixture passed **11/11**. Original fixture SHA-256 remained `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce`.
- The pinned PS5 SDK compiled all **17** loader source units and linked an offline ELF. This is build evidence, not PS5 execution.
- The [four-file patch](guest-virtual-memory-increment-20261013.patch) replayed with `git apply --check`, `git apply`, and exact byte comparison against stage 59. Patch SHA-256: `e9f538ff18a89ae2b2231ed7d8f4d505172fa86a1040de9cc9545380dc96b0f3`. The [manifest](guest-virtual-memory-manifest-20261013.json), [host evidence ZIP](guest-virtual-memory-host-evidence-20261013.zip), [target manifest](guest-virtual-memory-target-build-manifest-20261013.json), and [target evidence ZIP](guest-virtual-memory-target-build-evidence-20261013.zip) passed ZIP CRC checks.

## Status and next work

This is **not a console-only wait**. The default GS backend still refuses guest entry pending the console round trip. No unchanged DLL has executed its intended behavior on PS5. Next generic memory work includes partial commit/decommit, `VirtualProtect` and `VirtualQuery`, exact Windows allocation granularity where required, and safe executable-memory integration. Other Windows/CRT/UE4SS imports, mixed Win64 ABI signatures, dependency/lifecycle semantics, automatic authenticated PC-to-PS5 target matching, and #1/#3 console behavior remain open. The runtime must refuse any DLL whose required behavior cannot be established safely.
