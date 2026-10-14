# Stage 62 unchanged-DLL import name match

This recalculates direct-import **name matches** from the unchanged five-DLL census against the stage 62 reviewed shim catalog. A name match is not ABI or behavior proof, does not provide a missing DLL dependency, and does not admit a mod for execution. The default GS backend still refuses real guest entry.

| Unchanged DLL | Direct imports | Reviewed-name matches at stage 61 | Stage 62 | Still unmatched/unpublished |
|---|---:|---:|---:|---:|
| TrueFOV-upstream-winmm.dll | 110 | 21 | 25 | 85 |
| ZeroCompanyMandoWardrobe-upstream-main.dll | 200 | 16 | 20 | 180 |
| TFWWorkbench-upstream-main.dll | 176 | 9 | 12 | 164 |
| EventViewerMod-upstream-main.dll | 283 | 13 | 13 | 270 |
| KismetDebuggerMod-upstream-main.dll | 237 | 12 | 12 | 225 |

The new critical-section names account for **4** additional matches each in TrueFOV and ZeroCompanyMandoWardrobe, and **3** in TFWWorkbench. EventViewerMod and KismetDebuggerMod do not directly import those names.

The largest remaining name gaps are:
- **TrueFOV-upstream-winmm.dll:** kernel32.dll 63, user32.dll 22.
- **ZeroCompanyMandoWardrobe-upstream-main.dll:** msvcp140.dll 82, ue4ss.dll 33, kernel32.dll 20.
- **TFWWorkbench-upstream-main.dll:** ue4ss.dll 81, msvcp140.dll 27, vcruntime140.dll 13.
- **EventViewerMod-upstream-main.dll:** msvcp140.dll 138, ue4ss.dll 45, api-ms-win-crt-stdio-l1-1-0.dll 20.
- **KismetDebuggerMod-upstream-main.dll:** msvcp140.dll 89, ue4ss.dll 59, api-ms-win-crt-stdio-l1-1-0.dll 16.

Next engineering should prioritize dependency/runtime surfaces and real-DLL admission, including CRT, UE4SS, and Win32 behavior, rather than inferring readiness from added catalog rows. The [machine-readable census](DLL-STAGE62-IMPORT-MATCH-20261013.json) pins the source and original census hashes.
