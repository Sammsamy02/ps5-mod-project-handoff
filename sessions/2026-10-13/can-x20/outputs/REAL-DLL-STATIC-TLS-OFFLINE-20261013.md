# Unchanged DLL static TLS preparation: Mac evidence

**Result:** Five unchanged Windows x64 DLLs passed a read-only host check of PE section copying, base relocation, mapped-image TLS registration, per-thread template copies, and callback-address location. No DLL entry point, callback, import, or gameplay code was executed. All five source SHA-256 values matched their pinned inputs before and after the check.

| Unchanged DLL | Initialized TLS bytes | Zero fill | Callbacks in executable sections | Relocations applied |
|---|---:|---:|---:|---:|
| TrueFOV `winmm.dll` | 8 | 0 | 0 | 918 |
| ZeroCompanyMandoWardrobe | 8 | 0 | 0 | 2,630 |
| TFWWorkbench | 8 | 0 | 0 | 246 |
| EventViewerMod | 384 | 0 | 2 | 545 |
| KismetDebuggerMod | 337 | 0 | 2 | 411 |

The C++ harness uses the same PE section and DIR64/HIGHLOW relocation formats as the staged loader, then calls the [static TLS core](STATIC-TLS-CORE-OFFLINE-20261013.md) on its private mapped copy. It checks that the assigned index was written into the copy, the initial template matches on two separate threads, and every returned callback address falls in file-backed executable code. The [machine-readable evidence](REAL-DLL-STATIC-TLS-PREPARE-20261013.json) contains source paths, pinned hashes, exit codes, and full observations. The core's **19/19** focused checks also passed in this stage.

This establishes that the staged TLS preparation logic handles these five real PE images. It does **not** prove that their Windows `GS` reads can access the prepared array, that callbacks or CRT/UE4SS code run correctly, or that any mod affects its intended game. The loader continues to refuse real TLS before mapping. The next offline integration step is a guest TEB/TLS-array publication route with thread attach/detach and callback ordering, followed by import/ABI and target-binding work.

The isolated test harness is delivered as an [incremental patch](real-dll-static-tls-evidence-increment-20261013.patch) against the static TLS core stage. Its [manifest](real-dll-static-tls-evidence-manifest-20261013.json) gives exact base and staged hashes. A scratch application produced byte-identical files; [logs and patch are bundled](real-dll-static-tls-evidence-20261013.zip). No console or shared candidate was touched.
