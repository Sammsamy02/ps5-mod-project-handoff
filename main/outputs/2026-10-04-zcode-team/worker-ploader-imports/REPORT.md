# REPORT.md — worker-ploader-imports

Date: 2026-10-04 · Scope: read-only source/evidence analysis; no console, no build, no
runtime claim. Deliverable: `PLOADER-IMPORT-ANALYSIS.md` (same directory).

## Result

**The ploader never resolves a plugin's imports — the kernel does, inside the game
process.** The ploader (`ploader-cleanup-candidate-1/original/main.c`, pinned Ploader
v1.1.0) resolves exactly two NIDs for its own plumbing (sceKernelLoadStartModule
`wzvqT4UqKX8`, getpid `HoLVWNanBBc`) via `kernel_dynlib_resolve` at dynlib handles 0x1 /
0x2001, injects a shellcode stub through ptrace, and calls
`sceKernelLoadStartModule(path, 0, 0, 0, 0, NULL)` in the target. Every import of the
injected module — all 198 — is bound by the kernel module manager against the process's
loaded modules. The ploader has **no import-count limit and no symbol-name handling at
all**; its only caps are 10 plugins/title, 10 titles, 255-char paths, plus timing gates
(libkernel+libc+Fios2 present, module count >= 6/16) before it injects.

## Why 198 can fail when 191 succeed

Reconciled accounting from retained evidence: 191 = frozen console-proven strong contract
("loads in Townfall every time"); +6 already-weak symbols (`__dladdr __dlclose __dlerror
__dlopen __dlsym kernel_mprotect` — weak binds to 0, load continues, FreeBSD rtld
lineage); +5 measured strong extensions (`sceKernelDlsym`, `sceKernelGetModuleInfo`,
`sceKernelLoadStartModule`, `sceKernelStopUnloadModule`, `qsort`) = verified 196-strong
contract; +2 context-flagged (`clock_gettime`, `getpid`) in the composition-4 candidate =
**198 imports, of which the 7 beyond the frozen 191 are the only unproven strong
bindings**. All Packager imports are emitted GLOBAL (`DynamicWriter.cs:631`), so one
unbindable strong import aborts the whole load — 191 successes cannot offset it.

Live failure hypotheses for the 7, in order of retained support:

1. qsort NID drift vs the game's loaded libc build (the other libc imports are proven;
   qsort is new — provider map `libSceLibcInternal`, `package_prx.py:920`).
2. Game-context import **approval** of the four dynamic-module APIs: ploader proves their
   *exports* exist (it resolves LoadStartModule at handle 0x1 in every target), but a PRX
   *importing* them strongly has never been measured to bind.
3. Per-build NID encoding error — ruled out for the retained composition-2 candidate
   (NID-decoded set equality, `unaccounted=[]`, ground-truthed encoder), but the check is
   per-build and must be rerun on any new candidate.
4. Not a count limit: no retained source documents any per-module import ceiling; a
   metadata refusal like the one measured case (ESDKVERSION `0x80020063`, 75 ms,
   `modbridge_log_probe.prx`, fixed by SDK stamp 0x11000000) fires before binding anyway.

Observed on the 198-import candidate: "loads but produces no output"
(`worker-import-fallback/import_fallback.py` docstring) — i.e. no strong-import abort was
recorded, so either all 7 bound and the silence is downstream (see
`worker-post-promotion-runtime/FAILURE-TRIAGE.md` T2/T5), or the candidate's dynsym bind
types differ from assumption. Mandatory next step before any load claim: run
`import_fallback.py audit` on the actual candidate ELF, then at most one staged load with
the diagnostic `rc=0x%08x` ploader build (`ploader-return-message-1`).

Mitigation already prototyped (proposal-only, unapplied): `import_fallback.py weakify`
demotes exactly those 7 symbols GLOBAL→WEAK (fail-closed byte-exact rewrite) so the
failure mode becomes bind-0 + guarded call sites (`optional_imports.h` R1-R4, in-module
heapsort fallback) instead of a load abort.

## PRX vs standalone payload

Different resolvers, non-transferable results: kernel rtld with exact
`(module, library, NID)` long-form identity plus shape/SDK gates for PRX; flat SONAME
stub linking by the payload runtime for standalone ELFs (the ps5-payload-sdk stub
generator drops identity tuples — retained `ue4ss-public-import-declarations-1` finding).

## Limits

Static analysis only; no ELF bytes re-audited, no device contact. The kernel's exact
refusal code for an unresolvable strong import is not established in any retained source.
All on-device claims above are quoted from retained evidence with paths in the analysis
file's provenance table. Tool calls used: 28 of 40.
