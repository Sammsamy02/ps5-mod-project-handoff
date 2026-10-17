# Official MSVC x64 runtime intake and import blockers

**Result:** stage 35 can admit a supplied PE under the `VCRUNTIME140.dll` name, but three unchanged, genuine Microsoft runtime DLLs still refuse during import preparation. None executed.

I downloaded Microsoft's [current x64 Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/) through its documented `https://aka.ms/vc14/vc_redist.x64.exe` link. The resolved package came from `download.visualstudio.microsoft.com` and has SHA-256 `843068991daaa1f73ad9f6239bce4d0f6a07a51f18c37ea2a867e9beca71295c`. I extracted the x64 CAB payload for local inspection; no installer ran and no Microsoft DLL is included in the deliverables.

| Unchanged x64 DLL | SHA-256 | Static dependency DLLs | Missing reviewed imports on stage 35 |
|---|---|---:|---:|
| `vcruntime140.dll` | `d1f4225df2cd877dbf130d5668a021dce3f94118455ff5ec952061c30afc9ce7` | 6 | 30 |
| `vcruntime140_1.dll` | `a7146c08f89fe5b04541ab507cdb59ff7b44534d4ba3c668a426c6450a03434e` | 5 | 22 |
| `msvcp140.dll` | `7c26614e1d733892c2deac7e245ce115504b1d80592dd0a01b08e3e5a55f89ca` | 14 | 175 |

The stage 35 read-only PE scanner parsed all three. `vcruntime140.dll` imports five `api-ms-win-crt-*` modules plus `KERNEL32.dll`; `msvcp140.dll` imports `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`, eleven `api-ms-win-crt-*` modules, and `KERNEL32.dll`. Calling the reviewed shim preparation route produced named fail-closed import refusals: **227 missing rows** across the three independently prepared files. Examples include UCRT heap/stdio/runtime functions and `KERNEL32` critical sections, FLS, unwind, and loader APIs. The files' hashes were unchanged after inspection.

The full [machine-readable evidence](/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/MSVC-CRT-IMPORT-GAP-20261013.json) records every static dependency and missing `DLL!symbol` row. These are direct import rows for this redistributable version; they do not prove that supplying every symbol would make the runtime safe or compatible. SEH, thread behavior, UCRT state, guest pointer validity, and target behavior still need separate proof. The package and extracted DLLs remain only under `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/msvc-redist-intake` for local analysis.
