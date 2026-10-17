# Offline DLL admission matrix — 13 October 2026

**Scope:** Read-only Mac analysis against the isolated `cg-console-wait-20261013-04` loader snapshot. The registry checks SHA-256 pins for the loader, PE structures and WinAPI shim sources. The original DLL bytes were not changed or executed. A result with no listed blocker still requires the loader preflight; no result here proves gameplay behavior or PS5 compatibility.

| Unchanged sample | Static TLS | Forwarded exports | Current loader gate |
|---|---:|---:|---|
| TrueFOV `winmm.dll` (gameplay proxy) | 8 initialized bytes; 0 callbacks; callback-table pointer | 181, including one ordinal-only | **Blocked:** real static TLS and callback-table pointer |
| ZeroCompanyMandoWardrobe (gameplay) | 8 initialized bytes; 0 callbacks; callback-table pointer | 0 | **Blocked:** same two features |
| TFWWorkbench (gameplay) | 8 initialized bytes; 0 callbacks; callback-table pointer | 0 | **Blocked:** same two features |
| EventViewerMod (tool) | 384 initialized bytes; 2 callbacks | 0 | **Blocked:** same two features |
| KismetDebuggerMod (tool) | 337 initialized bytes; 2 callbacks | 0 | **Blocked:** same two features |

The loader source explicitly refuses static TLS data and any callback-table pointer because the Windows TEB/GS path is unavailable. Its default 16 KiB permission rule also refuses pages whose section permissions combine write and execute. The dossier catches this for the no-TLS x64 UE4SS fixture; a separate-page, no-TLS worker sample is labeled **requires loader preflight**, never ready.

**Measured limits.** The dossier checks known deterministic blockers, including architecture, CLR descriptor presence, malformed TLS, delay imports, 16 KiB write/execute partition, image-size cap and entry-point bounds. It does not invoke the loader. Relocation/slide, actual import resolution and signatures, dynamic `LoadLibrary`, cross-build target binding, intended gameplay behavior, and console restoration remain unverified. This is a current-loader blocker map, not a claim that every DLL in every game can run.

See the [machine-readable dossiers](DLL-ADMISSION-DOSSIERS-20261013.json), [guarded source patch](pe-analyzer-universal-admission-20261013.patch), [manifest](pe-analyzer-universal-admission-manifest-20261013.json), and [evidence bundle](pe-analyzer-universal-admission-evidence-20261013.zip). The patch is isolated and has not been integrated into the shared candidate.
