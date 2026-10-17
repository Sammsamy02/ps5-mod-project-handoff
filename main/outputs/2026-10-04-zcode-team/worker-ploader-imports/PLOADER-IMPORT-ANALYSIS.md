# PLOADER-IMPORT-ANALYSIS.md

Worker: worker-ploader-imports, 2026-10-04. Scope: source/evidence analysis only — no
console, no build, no runtime claim. Question: how does the PS5 ploader resolve module
imports, and why might 198 imports fail when 191 succeed?

## 1. What the ploader actually does with imports: nothing

Source of truth: `.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`
(pinned upstream Ploader v1.1.0, rev `1409a94aa6a5851acde72f85d3a1f6ae64a1b0a6`, per
`ploader-return-message-1/REPORT.md`).

The ploader is an injector, not a dynamic linker. It has **no code path that reads,
validates, or resolves a plugin's dynamic imports**:

- It resolves exactly **two NIDs for its own plumbing**, by name, in the target process
  (`pt_resolve`, main.c:209-212):
  - `NID_LOADSTARTMODULE = "wzvqT4UqKX8"` (sceKernelLoadStartModule, main.c:33)
  - `NID_GETPID = "HoLVWNanBBc"` (getpid, main.c:34)
  Resolution tries dynlib handle `0x1` first, then `0x2001` (kernel/system module space,
  then application module space — the same two spaces `count_modules` scans at
  main.c:492-499: `0x0..0x3F` and `0x2000..0x203F`).
- Injection (`inject_prx`, main.c:423-454) copies the PRX path plus a small shellcode
  stub into the target, mprotects it executable, and calls the resolved
  **sceKernelLoadStartModule(path, 0, 0, 0, 0, 0) inside the game process** via ptrace
  register hijack + INT3 (`pt_call`, main.c:260-290).
- Therefore: **the kernel's module manager (sceKernelLoadStartModule) resolves all of the
  injected module's imports**, against the modules already loaded in that process.
  The ploader delegates 100% of dynamic symbol resolution to the kernel.

**Limits in the ploader itself: none on imports.** There is no import count, symbol
name, or NID limit anywhere in main.c. Its only numeric caps are unrelated to imports:
`MAX_PLUGINS=10` per title, `MAX_TITLES=10`, `MAX_PATH_LEN=255` (main.c:26-28), and the
timing readiness scan (`deps_ready`/`wait_stable`, main.c:509-559) which requires
libkernel.sprx + libc.prx + libSceFios2.prx present, LoadStartModule resolvable, and a
minimum loaded-module count (6 early mode / 16 late mode) before it will inject.

Failure surfacing: `inject_prx` returns `-1` for its own early failures (access(),
resolve, mmap, copyin); otherwise it returns the low 32 bits of sceKernelLoadStartModule's
return collected at the INT3 trap. The FAIL notification shows that raw value (diagnostic
build adds `rc=0x%08x`; `ploader-return-message-1/REPORT.md`). The API's sixth `int* pRes`
argument stays NULL, so a module initializer result is never collected, and the
notification cannot distinguish which load substage refused.

## 2. How the kernel-side resolver binds the 198 imports

What the injected PRX must satisfy (all kernel-side, none ploader-side):

- **Shape gates run before any symbol binding.** The packager's own documentation states
  the loader "turns away" a module while scanning program headers if it names a dynamic
  table without carrying the fourth (SCE dynamic) segment; PT_SCE_MODULE, module note,
  SDK stamp are similarly structural (SharpProspero `DynamicWriter.cs` header comment;
  `package_prx.py` static checks).
- **SDK version gate.** Observed refusal: `0x80020063` = `ORBIS_KERNEL_ERROR_ESDKVERSION`
  (`ploader-rc-80020063-1/public-orbis-error.h:109`), measured on `modbridge_log_probe.prx`,
  75 ms, user-reported (`ploader-rc-80020063-1/return-path-review.json`). Fixed by stamping
  SDK version 0x11000000 (`finalize_sdk`; `worker-prx-packaging/STAGING-CHECKLIST.md:30`).
  This proves the kernel refuses loads on module metadata before/at binding — a refusal is
  a gate working, not a ploader bug.
- **Exact NID identity binding.** Import names in `.dynstr` are the Packager's long form
  `<nid>#<library>#<module>`; "a run-time resolver lookup succeeds only when the payload
  asks for exactly the identifier the module publishes there"
  (`SharpProspero.Link/NidEncoder.cs` remarks, ground-truthed against the SDK's
  `libc.prx` exports, 28 pairs; worker-prx-link-gaps REPORT §2).
- **Strong vs weak semantics.** "On the PS5 loader (FreeBSD rtld lineage, same as the
  frozen PRX's six existing weak imports) an unresolved weak undefined symbol binds to 0
  instead of aborting the load" (`worker-import-fallback/optional_imports.h`). Strong
  (STB_GLOBAL) undefined imports that cannot be bound abort the load; weak (STB_WEAK)
  bind to 0 and execution continues. All Packager-emitted import records are GLOBAL
  (`DynamicWriter.cs:631`, per `ue4ss-prx-import-contract-1/REPORT.md` interface table).

**No import-count limit is documented in any retained source.** Nothing in the ploader,
the packager, or the retained contract tooling records a per-module import ceiling; 198
strong imports is not near any documented bound. A "too many imports" explanation has no
retained-source support.

### PRX vs standalone payloads

- **Injected PRX** (this pipeline): kernel module manager resolves every import inside the
  game process, with exact `(module, library, NID)` identity, SDK-version and shape gates,
  strong/weak binding semantics above.
- **Standalone payloads** (ploader itself; the spike's `curl_link_probe.elf`,
  `libarchive_probe.elf`): ordinary payload ELFs consumed by the payload loader
  (ps5-payload-dev lineage, retained pins in `ue4ss-public-import-declarations-1/`).
  That environment links against **plain ELF stub libraries with a SONAME** built by
  `sce_stubs/genstub.py`, which "does not preserve the independent library/module
  identities or versions" (retained REPORT, line 11). Resolution is the payload runtime's
  own flat name/SONAME binding — no PT_SCE_MODULE gate, no sce module-manager import
  approval. The ploader's own imports (`sceKernelSendNotificationRequest`,
  `sceKernelGetAppInfo`, main.c:118-119) are resolved this way before any injection
  happens; they never touch sceKernelLoadStartModule.
- Consequence: a symbol that links fine in a standalone payload can still fail as a PRX
  import (different resolver, identity gate), and vice versa. Import-count or bind
  behavior measured on payloads does not transfer to the kernel PRX path.

## 3. Prior import-related load-failure evidence (the checkpoint trail)

Retained, in order of relevance:

1. **ESDKVERSION 0x80020063 load refusal** — the one measured kernel refusal so far.
   `ploader-rc-80020063-1/return-path-review.json` (rc 0x80020063, 75 ms,
   `modbridge_log_probe.prx`, user report) + decode against public
   `orbis_error.h:109`; remediated by the SDK stamp (0x11000000), now a static check
   (`worker-prx-packaging/STAGING-CHECKLIST.md:30`, `FAILURE-TRIAGE.md` §T1.4). It was a
   metadata/stamp refusal, **not** an import-binding failure.
2. **"The 198-import candidate loads but produces no output"** — recorded verbatim in
   `worker-import-fallback/import_fallback.py` docstring. This is the only retained
   observation of the current composed candidate class: the kernel accepted the load (no
   FAIL/abort evidence retained), and the silent failure is downstream of load. This is
   the exact 198-vs-191 case this analysis targets.
3. **Contract refusals on the host side** (never reached the console): the original
   contract compiler refused 82-of-208 strong names with no identity
   (`ue4ss-prx-import-contract-1/REPORT.md`); the frozen console-proven lineage resolved
   this later — the current composed candidate passes with `required_strong=196`,
   `missing_strong=0` (`prx-candidate-composition2-1/supplier-contract.json`).

No retained evidence shows a kernel refusal caused by an unresolvable strong import for
the composed candidate — the ESDKVERSION case is the only measured on-device refusal.

## 4. The SharpProspero NID encoding: are all imports properly encoded?

Yes — for the retained candidates, this is verified, not assumed:

- Encoder: `SceNid.Compute` / `NidEncoder.Encode` — `SHA1(ASCII(name) + salt)` where
  salt = `51 8D 64 A6 35 DE D8 C1 E6 B0 39 B1 C3 E5 52 30`; take first 8 digest bytes,
  reverse (little→big-endian), base64, 11 chars, `/`→`-` (`SharpProspero.Prx/SceNid.cs`,
  `SharpProspero.Link/NidEncoder.cs`). The Link variant's remarks document that dropping
  the byte-swap produces NIDs matching stub `.scenid` bytes but **no on-device module** —
  i.e. a wrong-NID import is silently unbindable, which is exactly the failure class
  the checks below rule out.
- Ground truth: 28 SDK `libc.prx` export pairs (`malloc`→`gQX+4GDQjpM`,
  `memset`→`8zTFvBIAIN8`, ...) plus the frozen console-proven PRX
  (`module_start`→`BaOKcng8g88#F#A`) (`NidEncoder.cs` remarks; worker-prx-link-gaps §2).
- Candidate verification: the composition-2 candidate's UND dynsym was decoded and
  compared as NID identities — "NID-decoded import set equal to the derived 196-import
  contract", with "malformed/unknown-nid accounting" and `unaccounted=[]`
  (`worker-prx-link-gaps/REPORT.md` §2, §5). A raw-text compare had earlier produced
  180-vs-180 false mismatches — a check-representation bug, since fixed.

So invalid-NID rejection by the kernel is **ruled out by evidence** for the retained
composition-2 candidate. Any new candidate must re-run the same NID-aware audit before
relying on this conclusion (the check is per-build, not global).

## 5. Could the 7 additional symbols specifically fail? — Yes; this is the risk set

The accounting, reconciled across retained artifacts:

- 191 = frozen, console-proven strong-import contract — "loads in Townfall every time"
  (`import_fallback.py` docstring).
- +6 weak undefined symbols already in the frozen PRX: `__dladdr __dlclose __dlerror
  __dlopen __dlsym kernel_mprotect` (bind-0 tolerated) (`ue4ss-prx-import-contract-1`).
- +5 measured strong extensions of the composed supplier runtime:
  `sceKernelDlsym, sceKernelGetModuleInfo, sceKernelLoadStartModule,
  sceKernelStopUnloadModule, qsort` (`package_prx.py` lines 23-24, 144-156;
  `import_fallback.py DEFAULT_TARGETS`) → 196-strong contract,
  verified 196/196 covered (`supplier-contract.json` counts).
- +2 context-flagged "possibly present" in composition-4: `clock_gettime`, `getpid`
  → the **198-import candidate** (`import_fallback.py` docstring and DEFAULT_TARGETS
  comment). 191 + 7 = 198.

The 7 additional symbols and their specific failure modes:

| Symbol | Provider evidence | Why it can fail while the 191 succeed |
|---|---|---|
| sceKernelDlsym | libkernel (`package_prx.py` EXTENDED_IMPORTS provider map) | NID is published by libkernel, but import **binding** of dynamic-module-management APIs to a game-context PRX is subject to kernel import approval; an approved-export-yes/bind-no split has no retained measurement either way |
| sceKernelGetModuleInfo | libkernel (composition-2 delta `added=[sceKernelGetModuleInfo]`) | Same approval question; newest addition (not in composition-1), so least soak |
| sceKernelLoadStartModule | libkernel — ploader itself resolves `wzvqT4UqKX8` at handle 0x1 in the game process, proving the **export** exists there (main.c:427) | Export existence is proven, but a PRX *importing* it strongly has never been proven to bind in this process; approval could still refuse the bind — the export/bind distinction is exactly what ploader's own resolution cannot test |
| sceKernelStopUnloadModule | libkernel (provider map) | Same as above |
| qsort | libSceLibcInternal (`package_prx.py:920` provider map) | NID must be published by the **game's loaded libc build**; the other libc imports come from the frozen proven set, qsort is new — a NID/version mismatch leaves it unbound while the older libc NIDs bind |
| clock_gettime | libkernel syscall wrapper (context-flagged, composition-4 only) | Presence itself is only "possibly" in the candidate; must be audited per build before any load claim |
| getpid | libkernel — NID `HoLVWNanBBc` known-good (ploader main.c:34 resolves it in every target) | Same bind-vs-export caveat; lowest actual risk given ploader proves the export |

Key structural points:

- **All 7 are strong (GLOBAL)** in the Packager output. One unbindable strong import
  aborts the entire load with a kernel error — 191 successes cannot offset it. This is
  the concrete mechanism by which "198 imports fail when 191 succeed" can happen: not a
  count limit, but any one of the 7 new strong bindings failing.
- **Self-referential trap:** the composed runtime imports sceKernelLoadStartModule
  strongly while the ploader only ever *resolves* it. If the kernel refuses that bind for
  a game-context PRX, the composed PRX can never load — and ploader would print FAIL with
  the kernel rc.
- The observed variant is milder: the 198-import candidate **loaded** but produced no
  output (`import_fallback.py`). That observation is only consistent with either (a) all
  7 bound successfully and the silent failure is downstream (triage T2/T5 territory, see
  `worker-post-promotion-runtime/FAILURE-TRIAGE.md`), or (b) the audited candidate's
  dynsym did not actually carry them as strong UND entries — which is why
  `import_fallback.py audit` on the actual candidate is step 1 of any load attempt.
- **Mitigation already prototyped:** `worker-import-fallback/import_fallback.py weakify`
  rewrites exactly the 7 extension symbols' bind byte GLOBAL→WEAK (fail-closed: aborts on
  defined/missing entries, verifies every other byte), turning "load abort" into
  "bind 0 + guarded call site" (`optional_imports.h` rules R1-R4: snapshot at
  module_start, capability flags, in-module qsort fallback, no optional references from
  fini paths). This is proposal-only; it has not been applied to a candidate or measured
  on device.

## 6. Direct answers to the task questions

1. **sceKernelLoadStartModule?** Yes — the ploader resolves its NID in the target process
   and executes it there via injected shellcode; that is its entire load mechanism.
2. **All dynamic imports handled by ploader itself?** No. It delegates every import of the
   injected module to the kernel module manager. Limits on import count or symbol names in
   ploader: none exist.
3. **PRX vs standalone payloads?** Different resolvers: kernel rtld with exact
   `(module, library, NID)` identity + shape/SDK gates + strong/weak semantics for PRX;
   payload-runtime flat SONAME stub linking for standalone payloads (whose stub generator
   drops identity tuples). Results do not transfer between the two.
4. **Prior import-related load failures?** ESDKVERSION 0x80020063 (measured, metadata
   stamp, fixed); host-side contract refusals (82 missing identities, later resolved);
   and the "198-import candidate loads but produces no output" observation. No measured
   strong-import bind refusal yet.
5. **NID encoding correct?** Verified for the retained composition-2 candidate
   (NID-decoded set equality, `unaccounted=[]`); per-build check — rerun for any new
   candidate.
6. **Could the 7 specifically fail?** Yes — they are the only strong imports outside the
   console-proven 191; one failed bind aborts the load regardless of the other 197.
   Libc-version NID drift (qsort) and game-context approval of dynamic-module APIs are
   the two live hypotheses; both are testable host-side first (audit dynsym bind types of
   the actual candidate) and only on-device by one staged load with the diagnostic rc
   build.

## Provenance

| Fact | Source |
|---|---|
| ploader resolves 2 NIDs, delegates all imports to kernel | `ploader-cleanup-candidate-1/original/main.c:33-34, 118-119, 209-212, 423-454` |
| rc surfacing / pRes NULL / -1 paths | `ploader-return-message-1/REPORT.md`, `ploader-rc-80020063-1/return-path-review.json` |
| ESDKVERSION 0x80020063 = ORBIS_KERNEL_ERROR_ESDKVERSION | `ploader-rc-80020063-1/public-orbis-error.h:109`; SDK stamp fix in `worker-prx-packaging/STAGING-CHECKLIST.md:30` |
| NID algorithm + ground truths | `sharpprospero-host-tool-1/source/tools/SharpProspero.Prx/SceNid.cs`, `tools/SharpProspero.Link/NidEncoder.cs` |
| Strong-all imports, identity tuple interface | `ue4ss-prx-import-contract-1/REPORT.md` (DynamicWriter.cs:631, Linker.cs:52) |
| 191 + 5 = 196 contract, NID-decoded equality, unaccounted=[] | `worker-prx-link-gaps/REPORT.md`, `prx-candidate-composition2-1/supplier-contract.json` |
| 7 extension names, 198 = 191+7, weakify mitigation, weak-bind-0 semantics | `worker-import-fallback/import_fallback.py`, `optional_imports.h` |
| Failure triage order | `worker-post-promotion-runtime/FAILURE-TRIAGE.md` |

Honest limits: static analysis of retained sources/evidence only. No candidate dynsym was
re-audited here (no ELF bytes opened); the per-candidate audit step remains mandatory. No
on-device behavior was measured by this worker; every load claim above is quoted from
retained evidence, and the kernel's exact refusal code for an unresolvable strong import
is not established in any retained source.
