# SERVICES-BACKLOG.md — ranked next Windows-service families from ACTUAL retained import evidence

worker-modreqs, 2026-10-04.

## Evidence base

Retained original PE images (the ONLY real-mod DLL imports that exist in this workspace; both are members of
`nexus-real-mods-2026-10-01/ue4ss-v1.zip`, provenance `windows-image-original-inputs-1/inputs.json`):
- `dwmapi.dll` (UE4SS's proxy loader) — 135 imports / 13 libraries; frozen readobj: `.superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/dwmapi.dll.llvm.txt`
- `ue4ss/UE4SS.dll` — 634 imports / 30 libraries, **static TLS directory present** (tls_rva 17832576); frozen readobj: `final-1/UE4SS.dll.llvm.txt`
- Coverage census: `outputs/2026-10-04-pe-dependency-runtime/RETAINED-IMPORT-COVERAGE.json` — today exactly **5/135 dwmapi** and **14/634 UE4SS** import slots name-match implemented services (memset/memmove/memcpy/wcslen/strlen + memchr/memcmp/strstr/strchr/strrchr/wcsncmp/strcmp/wcscmp/wcscpy).
- Implemented-service inventory: `outputs/ps5-mod-bridge/windows-services/` (CRT/heap/process bounded, TLS+LastError, module load/lookup, QPC/QPF, owned worker threads) + `outputs/2026-10-04-dll-synchronization/` (7 critical, 11 SRW/CV, 3 address-wait APIs; consumer binds 44 imports).

Census-vs-fixture separation rule (per `outputs/2026-10-04-dll-synchronization/CRITICAL.md`): a family is
"**observed in retained mod imports**" only if the exact name appears in the two readobj files above.
Fixture-only = implemented/verified purely against our source-built PE diagnostics.

## Ranked families

### 1. One-time initialization (InitOnce) — P1
- Observed in retained imports: `InitOnceBeginInitialize`, `InitOnceComplete` (UE4SS.dll, KERNEL32; readobj Import blocks).
- Not implemented today (absent from the 7+11+3 sync slice; TEAM_PLAN worker-4 names it as the start: "starting with one-time initialization if still missing").
- Semantics to own: begin/complete pairing, `fBeginThreadInitialization` out flag, one-time block execution, last-error on contention.

### 2. Mutex + waitable-handle waits — P1
- Observed: `CreateMutexA`, `ReleaseMutex`, `WaitForSingleObject`, `WaitForSingleObjectEx`, `OpenThread`, `Sleep`, `SwitchToThread`, `CloseHandle` (UE4SS.dll). dwmapi.dll: `CloseHandle` only.
- Status: Sleep/SwitchToThread behavior exists inside service tests but the named KERNEL32 imports and mutex objects are not bound; current waits are SRW/CV/address-wait only.
- Note: **event objects are NOT observed in either retained DLL** (no CreateEvent/SetEvent/ResetEvent import anywhere). Do not spend the next slice on events on the strength of this census.

### 3. File I/O and metadata family — P1 (breadth: both DLLs)
- Observed (UE4SS.dll): `CreateFileW`, `CreateFile2`, `ReadFile`, `WriteFile`, `FindFirstFileW`, `FindFirstFileExW`, `FindNextFileW`, `FindClose`, `GetFileAttributesExW`, `GetFileInformationByHandle`, `GetFileInformationByHandleEx`, `SetFileInformationByHandle`, `DeleteFileW`, `CreateDirectoryW`, `GetCurrentDirectoryW`, `GetFinalPathNameByHandleW`.
- Observed (dwmapi.dll too): `CreateFile2`, `FindFirstFileW`, `FindClose`, `GetFileAttributesExW`, `GetFileInformationByHandleEx`, `AreFileApisANSI`.
- Also both: change notifications (`FindFirstChangeNotificationW`, `FindNextChangeNotification`, `FindCloseChangeNotification` — UE4SS).
- Version APIs: **not observed** (no VERSION.dll / GetFileVersionInfo / VerQueryValue in either image).

### 4. CRT startup/runtime block (api-ms-win-crt-runtime) — P1 (gate for any real MSVC DLL body)
- Observed (dwmapi.dll, 11): `_initterm`, `_initterm_e`, `_crt_atexit`, `_register_onexit_function`, `_execute_onexit_table`, `_initialize_onexit_table`, `_configure_narrow_argv`, `_initialize_narrow_environment`, `_seh_filter_dll`, `_cexit`, `abort`.
- Observed (UE4SS.dll, 17 more incl. the above families plus `_initialize_locks`-class dependents; readobj).
- Without these, a real CRT-initialized DLL cannot pass DllMain→CRT init even if mapped and relocated (parser work in `outputs/ps5-mod-bridge/windows-image/` already maps/relocates/binds; it refuses unknown requirements — correct behavior).

### 5. Static TLS + FLS — P2
- Observed: `TlsAlloc/TlsFree/TlsGetValue/TlsSetValue` AND `FlsAlloc/FlsFree/FlsSetValue` (UE4SS.dll; note FlsGetValue not imported). UE4SS.dll carries a **non-zero static TLS directory** (original-input-comparison.json `tls_rva`), so the loader must honor IMAGE_TLS_DIRECTORY (template copy + callbacks), which today's per-thread TLS service does not cover (no evidence of static-TLS section processing in windows-services).
- dwmapi.dll has tls_rva 0 — the proxy alone would not need it; UE4SS.dll does.

### 6. Module handle/enumeration extension — P2
- Observed: `LoadLibraryW` (dwmapi), `LoadLibraryA/LoadLibraryExA/LoadLibraryExW`, `GetProcAddress`, `FreeLibrary`, `GetModuleHandleA/W/ExW`, `GetModuleFileNameA/W`, `AddDllDirectory`, `RemoveDllDirectory`, `GetSystemDirectoryW` (UE4SS/dwmapi mix); PSAPI `K32EnumProcessModules`, `K32GetModuleBaseNameA`, `K32GetModuleInformation`, `K32GetProcessMemoryInfo`; toolhelp `CreateToolhelp32Snapshot`, `Thread32First/Next` (UE4SS).
- Current loader binds exact imports/dependencies but the census names above are not all bound; enumeration/PSAPI is census-observed and unimplemented.

### 7. Virtual memory & file mappings — P2
- Observed (UE4SS.dll): `VirtualAlloc`, `VirtualFree`, `VirtualProtect`, `VirtualQuery`, `VirtualQueryEx`, `CreateFileMappingW`, `MapViewOfFile`, `UnmapViewOfFile`, `GetLargePageMinimum`, `GlobalMemoryStatusEx`.

### 8. Exception/unwind (SEH64 + C++ EH) — P3 (large; blocks only crash paths initially)
- Observed (UE4SS.dll): `RtlCaptureContext`, `RtlLookupFunctionEntry`, `RtlVirtualUnwind`, `SetUnhandledExceptionFilter`, `SetErrorMode`, `SetThreadStackGuarantee`; (both DLLs) VCRUNTIME `__CxxFrameHandler4`, `_CxxThrowException`, `__C_specific_handler`, `__std_terminate` family.
- RUNTIME_MAP ABI rule: unwind/personality must follow the pinned SDK/ms_abi route; do not fake.

### 9. Console/environment — P3
- Observed (UE4SS.dll only): `AllocConsole`, `GetConsoleMode`, `SetConsoleMode`, `GetConsoleOutputCP`, `GetStdHandle`, `WriteConsoleW`, `GetEnvironmentVariableW`; USER32 `MessageBoxA/W` (dwmapi), SHELL32 `CommandLineToArgvW` (dwmapi).

### 10. MSVCP140 C++ stdlib surface — P4 (biggest count, latest)
- Observed: 61 symbols dwmapi.dll (iostream/locale/codecvt facets, `_Lockit`, `_Locinfo`, exception helpers `_Xbad_alloc/_Xlength_error/_Xout_of_range`); 207 + MSVCP140_ATOMIC_WAIT 13 + CODECVT_IDS 1 on UE4SS.dll (incl. `Concurrency::details::_TaskEventLogger` family and `std::_Execute_once`-class interops).
- Real dependence: any downloaded MSVC-built DLL mod will import slices of this; ATOMIC_WAIT pairs with api-ms-win-core-synch `WaitOnAddress` family (already implemented — the only census-observed family with a completed slice).

## Explicitly NOT observed in retained mod imports (do not prioritize on this census)
- Event objects (`CreateEventW/SetEvent/ResetEvent/OpenEvent`), semaphores (`CreateSemaphore/ReleaseSemaphore`).
- Version-information APIs (`GetFileVersionInfo*`, `VerQueryValue`) and VERSION.dll.
- `WaitForMultipleObjects`, `SignalObjectAndWait`, waitable timers, `SuspendThread/ResumeThread`, fibers (`IsThreadAFiber` IS observed — read only).
- Anything from GDI32/IMM32/D3D (UE4SS.dll imports them for its GUI overlay; they are runtime-self needs, not generic mod service priorities — same P3 caveat as §9).

## Fixture-only vs census-observed audit of the CURRENT implemented sync slice
- Census-observed and implemented: `InitializeCriticalSection`, `EnterCriticalSection`, `LeaveCriticalSection`, `DeleteCriticalSection` (dwmapi+UE4SS), `Acquire/ReleaseSRWLock{Shared,Exclusive}`, `SleepConditionVariableSRW`, `WakeAllConditionVariable` (UE4SS), `WaitOnAddress/WakeByAddressAll/WakeByAddressSingle` (UE4SS via api-ms-win-core-synch-l1-2-0).
- Fixture-only (implemented as supporting services, no retained mod imports them — stated in CRITICAL.md): `TryEnterCriticalSection`, `InitializeCriticalSectionAndSpinCount`, `SetCriticalSectionSpinCount`, and the TryAcquireSRW pair / `WakeConditionVariable` / explicit `InitializeSRWLock`/`InitializeConditionVariable` initializers (implicit-init contracts instead).
