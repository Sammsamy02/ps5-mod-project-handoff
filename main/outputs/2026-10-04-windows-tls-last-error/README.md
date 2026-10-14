# Windows dynamic TLS and last-error — 2026-10-04

This slice implements six exact `KERNEL32.dll` imports: `TlsAlloc`, `TlsFree`, `TlsGetValue`, `TlsSetValue`, `GetLastError`, and `SetLastError`. A self-built no-entry DLL called them successfully on the **local x86_64 host**. PS5 evidence is **object compilation only**: no game or console success, downloaded-DLL execution, or new playable UE4SS build is established.

| Evidence | Result |
|---|---|
| [Native TLS receipt](NATIVE-TLS-RECEIPT.json) | Three actual pthreads verify isolated values, cross-thread reset on index reuse, all 1,088 slots and exhaustion, last-error/errno separation, and owner lifetime. Normal and ASan/UBSan runs pass. Direct PS5 C object: 10,624 bytes. |
| [Errno regression](ERRNO-REGRESSION-RECEIPT.json) | Existing three-thread errno, explicit detach, exit destructor, and BUSY-close checks still pass normally and under sanitizers. |
| [Heap regression](HEAP-REGRESSION-RECEIPT.json) | Prior owned DLL still executes five heap/errno imports plus composed `strlen`, preserves allocations on realloc failure, isolates errno, and unloads correctly. |
| [TLS execution](EXECUTION-RECEIPT.json) | Owned DLL executes all six new imports plus composed `_errno` and `strlen`; live-worker reuse resets to NULL, last-error effects and cleanup pins pass, and the module unloads. Six Microsoft ABI thunks compile to a 7,368-byte PS5 bridge object. All 18 recorded commands pass. |

The native domain has a bounded 1,088-index pool and per-thread value arrays. Allocation selects the lowest free index and clears it on every registered thread; new registrations start with NULL values. Freeing an index never frees its caller-owned pointee. Service pins keep the domain alive independently of admitted threads. Domain close refuses while pins, registrations, or allocated indices remain, and bridge sessions retain pins through consumer cleanup failure.

`TlsGetValue` clears last-error on every successful access, including NULL results. Successful Alloc/Free/Set preserve it. Get/Set accept indices 0–63 even before allocation, reflecting Microsoft's documented minimum-index behavior. Last-error is separate unsigned 32-bit thread state, independent of CRT errno. See [TlsGetValue](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlsgetvalue), [TlsSetValue](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlssetvalue), [TlsAlloc](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlsalloc), and [TlsFree](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlsfree).

Exhaustion error **8** and invalid-index error **87** are explicit adapter policy, not universal Windows error guarantees. Their meanings are listed in [system error codes](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-); Microsoft notes that per-function error codes can vary in [GetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror). [SetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setlasterror) documents the per-thread setter. The [TLS overview](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-local-storage) describes the minimum and current maximum slot counts.

The native owner compiles directly as PS5 C. The bridge uses the established FreeBSD9 frontend → unedited `win64cc` IR → PS5 backend route. Its six Windows-callable signatures are explicit; native control APIs remain native C.

The owner's twelve native imports are unchanged. [Prior SDK declaration evidence](../2026-10-04-windows-heap-process/THREAD-IMPORT-EVIDENCE.json) records their exact stub names and SONAMEs; it does **not** establish in-game module/library-version resolution. Concrete target resolution and native executable-memory integration remain necessary.

[Retained import metadata](RETAINED-IMPORTS.json) supports these service names and still shows an original image requiring **PE static TLS**, which this slice does not implement. FLS callbacks, Windows thread creation, loader thread notifications, SEH, and real dynamic-module services also remain outside this slice. Unreal integration still lacks the actual game provider, verified layouts, and actual Tick integration.

The current thread-domain implementation supersedes the thread-source hashes recorded in the prior `2026-10-04-windows-heap-process` report. That report remains a historical snapshot. The fresh errno and heap regression receipts here bind the existing behavior to the current thread sources.

[CHECKPOINT.json](CHECKPOINT.json) pins all receipt origins and hashes. Receipt copies are byte-identical and current source hashes match them. The retained import census is preserved. No console contact or game/system writes occurred.
