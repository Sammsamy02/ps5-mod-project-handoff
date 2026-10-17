# Stage 41: PS5-target loader build

**Status:** all isolated loader translation units compiled and linked with the pinned PS5 SDK offline. No console execution or unchanged gameplay DLL execution is claimed.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ps5-loader-build-20261013-41`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-reentrant-segment-20261013-40`

The SDK's `sys/mman.h` hides `MAP_ANON` and `MAP_ANONYMOUS` under strict `_XOPEN_SOURCE=700`. The isolated one-file change in `loader/src/pe_loader.cpp` selects the exposed POSIX spelling where available and uses the pinned SDK's `0x1000` anonymous-map value under `__PROSPERO__` when those names are hidden. Other POSIX targets fail at compile time if neither name is available. This changes only the mmap flag selection; it does not enable the absent production guest-segment backend.

With `LLVM_CONFIG` set to the pinned bundled LLVM 21.1.8 path, `prospero-clang++` reported `x86_64-sie-ps5` and compiled all **12** loader C++/assembly units using C++17, PIC, `_XOPEN_SOURCE=700`, and `-Werror`. The `pe_loader.o` references `mmap`, `mprotect`, and `munmap`. An offline link with all 12 objects, the SDK libraries, and a trivial `main` stub exited 0 and produced an x86-64 FreeBSD-style PIE ELF. The link used `--no-gc-sections` to retain all loader objects. `llvm-readelf` reported imports from `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. The stub is a link probe; it is not a console-ready launch path.

All **15** Mac regression runners exited 0, the guest TEB AddressSanitizer check passed, and the original synthetic PE fixture passed **11/11** checks. The one-file patch passed `git apply --check`, scratch application onto exact stage 40, byte-for-byte replay, and ZIP CRC verification.

- [Source patch](ps5-loader-build-increment-20261013.patch)
- [Base and stage hash manifest](ps5-loader-build-manifest-20261013.json)
- [Mac regressions and replay evidence](ps5-loader-build-host-evidence-20261013.zip)
- [Target build manifest](ps5-loader-target-build-manifest-20261013.json)
- [Target objects, link probe ELF, and logs](ps5-loader-target-build-evidence-20261013.zip)

This closes target **compilation and linking** for the isolated loader stage. Runtime memory permissions and cache behavior, PS5 GS/TEB segment transition, Windows CRT/SEH and framework imports, target correspondence, real DLL execution, and gameplay effects remain unverified. The shared candidate, kernel code, Claude's other capabilities, and original DLL binaries were untouched.
