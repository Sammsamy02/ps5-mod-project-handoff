# Windows heap and process-service checkpoint — 2026-10-04

This slice implements owned allocation services and per-thread Windows errno, with pinned Microsoft x64 import bridges. A self-built DLL called the services successfully on the **local x86_64 host**. PS5 evidence is **object compilation only**. No game or console success is established, and no new playable UE4SS build was produced.

| Evidence | Verified result |
|---|---|
| [Heap receipt](HEAP-RECEIPT.json) | Six native groups pass normally and under ASan/UBSan: pinned lifetime, 16-byte alignment/accounting, calloc/overflow, realloc semantics, failure preservation, and errno-slot identity. Direct PS5 C object: 4,216 bytes, no undefined imports. |
| [Thread receipt](THREADS-RECEIPT.json) | Three actual native threads verify isolated stable Windows errno, explicit detach, exit destruction, and BUSY close; sanitizer run passes. Direct PS5 C object: 6,368 bytes, twelve native imports. |
| [Execution receipt](EXECUTION-RECEIPT.json) | Owned no-entry DLL executes five heap/errno imports plus the existing composed `strlen`, preserves the old allocation on failed realloc, isolates thread errno, passes session lifetime checks, and unloads. PS5 bridge object: 7,416 bytes. |

The new exact import pairs are `api-ms-win-crt-heap-l1-1-0.dll!malloc/calloc/realloc/free` and `api-ms-win-crt-runtime-l1-1-0.dll!_errno`. DLL matching is ASCII case-insensitive; symbols remain exact. Existing stateless CRT functions remain available through the composed binder.

Allocation uses real caller-supplied backend callbacks. `malloc(0)` and zero-factor calloc return freeable tokens on success; `realloc(nonNULL, 0)` frees and returns NULL. Calloc checks overflow and zeroes requested bytes; failed realloc preserves the prior allocation. Success leaves Windows errno unchanged. Explicit thread admission prepares the signed 32-bit errno slot before DLL calls. Bridge sessions pin the heap owner through consumer lifetime and cleanup retries, and supplied native locking serializes heap operations. Live allocations and pins prevent teardown. This does not implement general Windows TLS, loader thread notifications, or a complete CRT.

The bridge preserves Microsoft calling conventions through the previously verified FreeBSD9 frontend → unedited `win64cc` IR → PS5 backend route. The heap owner and native pthread adapter compile directly as PS5 C. These objects have not been executed on PS5.

[Thread import evidence](THREAD-IMPORT-EVIDENCE.json) records all twelve exact SDK stub declarations and SONAMEs: four libc symbols in `libSceLibcInternal.so` and eight pthread symbols in `libkernel_web.so`. The older provider map covers eleven; `pthread_key_delete` has a declared SDK provider and requires a new map entry. Stub names and SONAMEs do not prove an in-game module/library version is loaded or resolves those symbols.

Next Windows work is real dynamic-module and thread-state services, followed by native executable-memory integration and concrete target-provider resolution. Unreal integration still requires the actual game provider, verified layouts, and actual Tick integration. This checkpoint establishes prerequisites for the broader DLL/runtime goal, which remains incomplete.

[CHECKPOINT.json](CHECKPOINT.json) records exact receipt origins, hashes, object evidence, and limits. The three receipts were copied byte-for-byte; current source hashes match them. No console contact or game/system writes were performed.
