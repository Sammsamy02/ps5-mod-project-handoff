# Static Windows import surface — 13 October 2026

This read-only census matches direct import names in five unchanged upstream DLLs against the **16 built-in rows** in the source-pinned Mac loader shim. It distinguishes an emulated row, a row that deliberately stops the mod when called, and no built-in row. Matching a name does not prove a correct Win64 signature or behavior. Registered modules such as UE4SS and calls reached through `LoadLibrary` are outside this count.

| Unchanged DLL | Direct import rows | Emulated built-in | Fail-closed built-in | No built-in row |
|---|---:|---:|---:|---:|
| TrueFOV `winmm.dll` | 110 | 4 | 2 | 104 |
| ZeroCompanyMandoWardrobe | 200 | 2 | 1 | 197 |
| TFWWorkbench | 176 | 1 | 1 | 174 |
| EventViewerMod | 283 | 1 | 1 | 281 |
| KismetDebuggerMod | 237 | 2 | 1 | 234 |
| **Total** | **1,006** | **10** | **6** | **990** |

The largest groups without a built-in row are `MSVCP140.dll` (336 rows), `UE4SS.dll` (218), `KERNEL32.dll` (188), `VCRUNTIME140.dll` (49), and CRT API-set DLLs. Several synchronization/time/thread symbols are imported by all five: `AcquireSRWLockExclusive`, `ReleaseSRWLockExclusive`, `SleepConditionVariableSRW`, `WakeAllConditionVariable`, `GetCurrentThreadId`, `GetCurrentProcessId`, and `GetSystemTimeAsFileTime`. This gives a concrete order for generic ABI work, but many C++ exports need compatible data layout and exception behavior in addition to a matching symbol name.

All five are already refused before entry by real static TLS and callback-table pointers. Import implementation alone would not make them runnable. See the [full per-symbol JSON census](DLL-IMPORT-SURFACE-20261013.json), [offline blocker matrix](DLL-ADMISSION-OFFLINE-MATRIX-20261013.md), and [reviewer handoff](CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md).
