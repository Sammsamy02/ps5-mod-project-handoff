# Unchanged-DLL route: guarded process heap, stage 58

## Scope

The isolated stage `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-process-heap-20261013-58` adds `GetProcessHeap`, `HeapAlloc`, and `HeapFree` as sealed Win64-callable imports when both guarded guest memory maps are configured. It is an exact-stage-56 increment. The process heap has one opaque handle for DLLs sharing a loader shim. Every allocation is registered in the existing bounded guest read and write maps before the pointer is returned; a successful free removes both registrations before releasing memory. Wrong handles, unsupported flags, out-of-range accesses, and double frees refuse. `HEAP_ZERO_MEMORY` produces zeroed allocations. The host test observes 16-byte alignment from `malloc` on x86-64; the pinned PS5 SDK compiles and links the same source.

The supported flag subset is `0` and `HEAP_ZERO_MEMORY` (`0x8`) for allocation and `0` for free. Other options, private heaps, `HeapReAlloc`, and `HeapSize` remain unadmitted. This follows the [Microsoft HeapAlloc](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc), [HeapFree](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree), and [GetProcessHeap](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap) signatures and the documented zero-memory flag. HeapAlloc failure leaves last error untouched as documented; HeapFree failure reports invalid handle or parameter through the guest last-error slot.

## Evidence

| Gate | Result |
| --- | --- |
| Focused real Win64 thunk calls, normal and AddressSanitizer | 13/13 each |
| Prior guest-thread focused checks, normal and AddressSanitizer | 85/85 each |
| Earlier loader regression runners | 15/15 exited 0 |
| Original synthetic PE fixture | 11/11, hash unchanged |
| Pinned PS5 SDK loader compile and offline link | 17 source units, pass |
| Exact-stage-56 scratch replay and evidence ZIP CRC | Pass |

The [four-file source increment](guest-process-heap-increment-20261013.patch) has SHA-256 `9e0ee37ac2f8d454973a956c3b8d7b120dc1d113e7557e6850409e31b69593af`. See its [manifest](guest-process-heap-manifest-20261013.json), [host evidence](guest-process-heap-host-evidence-20261013.zip), [target manifest](guest-process-heap-target-build-manifest-20261013.json), and [target evidence](guest-process-heap-target-build-evidence-20261013.zip).

This closes three measured TrueFOV static import names only at the guarded shim level. It does not make that unchanged DLL load: many Windows, CRT, USER32, and game-binding imports are still unavailable. The test calls the real Win64 wrappers both directly and through the lazy PE import route, but no genuine gameplay DLL ran. Guest code must not use a block concurrently with HeapFree; bounded map callbacks serialize only operations performed through those callbacks. The default production guest-segment backend still refuses entry until its console round trip is proved. No console, shared candidate, original DLL, kernel code, or Claude #2/#4–12 work was changed.
