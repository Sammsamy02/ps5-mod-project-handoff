# Unchanged-DLL route: guarded heap resize and size, stage 59

## What changed

The isolated stage `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-heap-resize-20261013-59` adds sealed Win64-callable `HeapReAlloc` and `HeapSize` imports on the exact stage-58 base. Both names occur in the measured unchanged-DLL import surface. `HeapSize` returns the requested size for a live process-heap block and `(SIZE_T)-1` for unsupported or invalid input. `HeapReAlloc` creates and registers a replacement in both guarded guest maps before revoking and freeing the old block; it preserves the existing prefix, zeroes only growth requested with `HEAP_ZERO_MEMORY`, and retains the original block on ordinary allocation or validation failure. Oversized requests above `PTRDIFF_MAX` refuse before calling the allocator.

The supported resize flags are `0` and `HEAP_ZERO_MEMORY` (`0x8`). `HEAP_REALLOC_IN_PLACE_ONLY`, exception generation, private heaps, and `HEAP_NO_SERIALIZE` remain explicit refusals. The shim leaves last error untouched on HeapReAlloc and HeapSize failure. These choices follow the [Microsoft HeapReAlloc contract](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heaprealloc) and [HeapSize contract](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapsize) for the admitted subset. Guest code must not directly access a block concurrently with resize or free; the guarded-map callbacks synchronize only their own operations.

## Evidence

| Gate | Result |
| --- | --- |
| Focused Win64 wrapper and lazy PE import checks, normal and AddressSanitizer | 18/18 each |
| Prior guest-thread checks, normal and AddressSanitizer | 85/85 each |
| Earlier loader regression runners | 15/15 exited 0 |
| Original synthetic PE fixture | 11/11, unchanged hash |
| Pinned PS5 SDK compile and offline ELF link | 17 source units, pass |
| Exact-stage-58 scratch replay and both ZIP CRC checks | Pass |

The [three-file patch](guest-heap-resize-increment-20261013.patch) has SHA-256 `7b8c380bd62e8ed6a40f6330559a0910019a05cde5cdf0d79bc9fe8e48464cde`. Its [manifest](guest-heap-resize-manifest-20261013.json), [host evidence](guest-heap-resize-host-evidence-20261013.zip), [target manifest](guest-heap-resize-target-build-manifest-20261013.json), and [target evidence](guest-heap-resize-target-build-evidence-20261013.zip) pin the source, build, tests, and replay.

This admits five process-heap import names in the isolated shim. It does not make an unchanged gameplay DLL load or execute. Broader Windows/MSVC CRT and UE4SS APIs, Win64 mixed or floating-point signatures, real guest GS behavior, target matching, and console patch/restoration evidence remain. The default production guest-segment backend still refuses entry. No console, original DLL, shared candidate, kernel code, or Claude #2/#4–12 work was changed.
