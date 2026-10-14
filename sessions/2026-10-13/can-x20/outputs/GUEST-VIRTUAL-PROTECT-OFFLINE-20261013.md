# Stage 61: guarded VirtualProtect

## Scope

This isolated increment starts from exact stage 60 at `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-virtual-memory-20261013-60`. The new source is `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-virtual-protect-20261013-61`. It does not edit the shared candidate, Claude's #2/#4–12 work, kernel code, or original DLLs.

A sealed four-argument Win64 `VirtualProtect` import now publishes when both guest memory maps bind. It admits an exact owned, committed, whole-region change between `PAGE_READONLY` and `PAGE_READWRITE`. The old-protection output is written through the guarded guest-write route before changing OS and map permissions. Read/write to read-only removes guarded write access; the reverse transition restores it. Invalid output pointers, unowned or partial ranges, and executable protection refuse without changing access. Direct guest users must be quiescent during protection changes.

This is a bounded subset of [Microsoft's VirtualProtect contract](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect). Interior and partial ranges, executable permissions and instruction-cache handling, page modifiers, `VirtualQuery`, and non-owned mappings remain unsupported. They are not silently mapped to this subset.

## Evidence

- Test-first red result: import publication failed before implementation. After implementation, focused real Win64 thunk checks passed **21/21** normally and **21/21** under AddressSanitizer.
- Prior process-heap checks passed **18/18** normal and AddressSanitizer; prior thread checks passed **85/85** normal and AddressSanitizer.
- All **15/15** earlier host runners exited zero. The original synthetic end-to-end fixture passed **11/11**, and its SHA-256 remained `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce`.
- The pinned PS5 SDK compiled all **17** loader units and linked an offline ELF. No console runtime was exercised.
- The [three-file patch](guest-virtual-protect-increment-20261013.patch) passed exact-stage-60 `git apply --check`, application, and byte comparison. Patch SHA-256: `6d43b010bc61ba40ea9da8cce257210d86a6accc34b231f70ad921f79831ab58`. The [manifest](guest-virtual-protect-manifest-20261013.json), [host evidence ZIP](guest-virtual-protect-host-evidence-20261013.zip), [target manifest](guest-virtual-protect-target-build-manifest-20261013.json), and [target evidence ZIP](guest-virtual-protect-target-build-evidence-20261013.zip) passed ZIP CRC checks.

## Status

This is **not a console-only wait**. The default GS backend still refuses guest entry pending its console round trip. No unchanged gameplay DLL has executed its intended effect on PS5. More generic engineering remains in virtual memory, Windows/CRT/UE4SS imports, mixed Win64 ABI, dependency and handle lifecycle, and automatic authenticated PC-to-PS5 target matching. #1 register-read and #3 patch/detour behavior also await console evidence. Unsupported DLL behavior remains a precise refusal.
