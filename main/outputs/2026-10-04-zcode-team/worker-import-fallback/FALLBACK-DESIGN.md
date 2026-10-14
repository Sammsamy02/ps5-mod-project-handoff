# FALLBACK-DESIGN.md — non-fatal resolution for the composed runtime's additional imports

worker-import-fallback, 2026-10-04. Proposal only: no console contact, no
in-game execution, no promotion claim. All file paths relative to the
translation-core workspace root unless absolute.

## 1. Problem

* The frozen, console-proven **aligned-entry PRX** carries a **191-strong-import**
  contract and **loads in Townfall every time**.
* The composed runtime extends the contract by strong undefined symbols that
  the older stage27 branch satisfied internally. Measured by the combined-object
  pre-check (`worker-prx-packaging/REPORT.md`): `sceKernelDlsym`,
  `sceKernelGetModuleInfo`, `sceKernelLoadStartModule`, `sceKernelStopUnloadModule`
  (libkernel) and `qsort` (libSceLibcInternal) — contract grows 191 → 196; the
  composition-4 candidate totals **198 imports** (the delta also includes the
  typed bootstrap-shim references and context-flagged `clock_gettime`/`getpid`).
* Symptom: the 198-import PRX **loads but produces no output**. The 7 additional
  symbols exist in the on-disk SDK libraries but evidently do not resolve the
  same way at runtime module load; the extra strong imports are the only
  structural difference against the loading 191-import PRX.

## 2. What the two resolvers actually do (read, not assumed)

### 2.1 Bridge PE loader (windows-execution — the desktop execution mirror)

* `windows-image/pe_image.h`: `mb_pe_prepare(..., mb_pe_binder, ...)` — the
  binder callback fills each IAT slot. **Any `MB_PE_BIND_UNRESOLVED` makes the
  whole prepare fail with `MB_PE_UNRESOLVED_IMPORTS`; "each unresolved IAT slot
  keeps its original value"** — there is no per-import tolerance concept.
* `windows-execution/pe_module.c`: `open_impl` propagates that to
  `MB_PE_MODULE_UNRESOLVED_IMPORTS` and runs `cleanup()` — the module never
  activates. `bind_nonzero` additionally rejects a `BOUND` result with a zero
  value. So in the bridge's own loader, **strong-unresolved = fatal by design**;
  the only tolerated pattern is "binder returns BOUND with a usable address".
  Lesson for the PS5 side: whatever fallback we choose must still yield a
  *callable or NULL-checked* slot, never a half-state.

### 2.2 SharpProspero packager (import binding)

* `worker-prx-packaging/package_prx.py` (frozen) / `worker-prx-link-gaps/package_prx.py`
  (vendored, NID-aware): imports are derived from the linked ELF's `.dynsym`
  `UNDEF` entries (`bind in (GLOBAL=1, WEAK=2)`), identity-encoded by
  SharpProspero's `SceNid.Compute` — `SHA1(name + 16-byte suffix)`, first 8
  bytes reversed, base64 truncated to 11 chars, `/`→`-`, published as the long
  form `<nid>#<library>#<module>` (verified against ground truth
  `malloc→gQX+4GDQjpM`, `module_start→BaOKcng8g88`).
* Crucially, the **frozen contract already contains six WEAK undefined symbols**
  (`frozen_allowed_weak`: `__dladdr`, `__dlclose`, `__dlerror`, `__dlopen`,
  `__dlsym`, `kernel_mprotect`) and that exact PRX **is the one that loads in
  Townfall every time**. The loader demonstrably accepts a PRX whose `.dynsym`
  mixes strong and weak undefined imports.

## 3. Approaches considered

### (a) WEAK imports — demote the 7 extension symbols to `STB_WEAK`  ✅ recommended

* Mechanism: after link, before packaging, flip the `st_info` bind nibble of
  exactly the 7 extension dynsym entries GLOBAL→WEAK (type nibble preserved).
  Loader semantics (FreeBSD rtld lineage, which PS4/PS5 dynload follows):
  an unresolved **weak** undefined symbol binds to **0** and the load continues,
  while a strong unresolved import aborts the module.
* Evidence for: the frozen loading PRX already ships 6 weak imports (§2.2);
  zero source changes; the 191 strong contract stays bit-identical; byte-scoped,
  auditable, reversible.
* Requirement: every call site must NULL-check (weak-zero is callable-looking
  but fatal). Handled by the runtime discipline in §4.
* Risk (honest): "weak-undef binds to 0 on Townfall's loader" is expected from
  the loader family and consistent with the frozen PRX, but the *zero-bind*
  behavior itself is not yet proven in-game — it is measurable with the existing
  probe pattern (§6 F1) before any feature relies on it.

### (b) LAZY binding — resolve at first call  ❌ rejected

* ELF lazy binding needs `DT_JMPREL`/`DT_PLTGOT` processing by the runtime
  loader plus a writable GOT after load. PS5 PRX imports are bound by the
  kernel module loader during `sceKernelLoadStartModule`; the frozen lineage
  pins `DT_INIT`/`DT_FINI` and forbids loader-owned arrays (package_prx checks
  `explicit_dt_init_dt_fini`, `loader_owns_no_arrays`). There is no exposed
  lazy-binding hook, and the bridge PE mirror (§2.1) has the same load-time
  fatal semantic. Also NID-resolved imports are not name-lookup based, so
  there is nothing to defer into.

### (c) RUNTIME lookup — `sceKernelDlsym` after load  ⚠️ secondary, not standalone

* Keep the shipped contract at 191 strong and resolve the extra functions at
  `module_start` through a dynamic-lookup entry point. Catch-22: the natural
  bootstrap (`sceKernelDlsym`) is itself one of the 7. The frozen contract's
  existing weak `__dlsym` could serve as bootstrap, but its presence/semantics
  in the game process are unproven, and NID→address lookup via dlsym needs the
  friendly-name/NID mapping at runtime.
* Verdict: viable **only as the consumer** of (a): once the 4 `sce*` symbols
  are weakly bound, the composed runtime can additionally *verify* them at
  runtime (probe + reason codes) rather than trust the load. Not a replacement.

### (d) CONDITIONAL inclusion — compile/link the extension only when active  ✅ companion

* Keep a **compat flavor**: exclude the dynamic-module-adapter objects so the
  PRX stays exactly at the frozen 191-import contract (guaranteed-load path),
  and an **extended flavor** using (a)+§4 for the feature-enabled path.
* Costs a second build flavor; zero new loader risk. Recommended as the ship
  ladder: compat first, extended only after F1 evidence (§6).

## 4. Recommended design: WEAK + guarded snapshot (a) with (d) as fallback flavor

Two halves, both prototyped in this worker directory:

### 4.1 Packaging side — `import_fallback.py` (post-link, byte-precise)

* Parses the linked ELF64 (little-endian), finds the single `SHT_DYNSYM`,
  resolves identities via mirrored `sce_nid()` (or `--raw`/`--map`), and
  rewrites **only** the `st_info` bind nibble GLOBAL→WEAK for the requested
  imports (`--names`, default = the 5 measured; add `clock_gettime`/`getpid`
  only after an `audit` of the actual candidate proves them).
* Fail-closed: aborts on a requested name that is missing from `.dynsym`,
  absent, or DEFINED; verifies every other output byte is identical to the
  input; idempotent (already-weak matches are reported, not re-patched);
  emits a JSON report with per-entry offsets, before/after `st_info`, and
  input/output SHA-256.
* Pipeline position: **after `ld.lld` partial link, before the SharpProspero
  harness invocation** — the packager already treats `bind==2` UNDEF entries
  as imports, and the static checks count weak imports, so the extended
  contract check (`imports_equal_extended_contract`) must mark these rows
  `weak: true` (contract 191 strong + 6 weak **+ 7 weak-extended**).

### 4.2 Runtime side — `optional_imports.h` (reference pattern, proposal)

* R1: probe once at `module_start` into an `optional_import_caps` snapshot
  (address-of weak symbol is 0 when unresolved).
* R2: all-or-nothing capability flags (`dynamic_module_tools`) + reason codes;
  features degrade explicitly, never half-bound.
* R3: `qsort` gets a complete in-module heapsort fallback, so libc absence is
  transparent; the `sce*` group degrades to "feature off".
* R4: no optional import referenced from fini paths.

## 5. Prototype evidence (executed here, all local)

| Artifact | Check | Result |
|---|---|---|
| `import_fallback.py` + `test_import_fallback.py` | synthetic ELF64: patch exactness (2 bytes), control/weak/DEFINED symbols untouched, idempotency, fail-closed missing/defined, CLI + JSON report, NID long-form decode + post-patch audit | **18/18 assertions pass** |
| `optional_imports.h` via `use_optional_imports.c` | strict compile with the frozen build's flags (`-Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector -fvisibility=hidden -ffreestanding -fno-builtin`, SDK LLVM 21.1.8 clang) | compiles clean; `nm -u` of the object = **exactly the 5 optional imports**, no hidden dependencies |
| `test_optional_imports.c` | host functional: caps probe, delegated qsort path, fallback heapsort over n=0..256 with duplicates, degenerate args | **ALL PASS** |

## 6. Risks and required verifications (none performed here)

1. **F1 (loader tolerance)**: build the extended flavor with weakified imports,
   run the existing no-output probe PRX pattern in-game; expect load + probe
   report showing `dlsym==NULL` (weak-zero) or real addresses. No feature may
   enable before this evidence exists.
2. **F2 (packager acceptance)**: run `package_prx.py --link` with the weakify
   step inserted; SharpProspero must accept WEAK UNDEF entries (expected —
   frozen PRX proves the format, not this exact packager path).
3. **F3 (SceModuleInfo import attributes)**: confirm the PRX import-table
   fragment marks the extended entries with the weak attribute byte the kernel
   expects (static inspection of the candidate's module info vs the frozen
   PRX's six weak rows — same encoding to clone).
4. **F4 (`clock_gettime`/`getpid`)**: present in the 198-import candidate per
   context but not in the measured 5; `import_fallback.py audit` on the actual
   candidate settles the final target list before weakify runs.
5. The bridge's PE mirror keeps strong-unresolved fatal by design (§2.1); if
   the Windows candidate path also needs these imports, it requires a binder
   that resolves them (or a stub) — out of scope here, noted for parity.

## 7. Integration recipe (minimal diff to today's flow)

1. `package_prx.py`: after partial link, before the harness step:
   `python3 import_fallback.py weakify --elf combined.o --out combined.weak.o
   --names <audit-confirmed list> --report fallback-report.json`; fail the run
   if the report shows `missing` non-empty.
2. Extend `imports_equal_extended_contract` to expect `bind == WEAK` for the
   extension rows (static check only; contract JSON gains `weak: true`).
3. Composed runtime: adopt `optional_imports.h` discipline; wire
   `dynamic_module_tools==0` to the existing "adapter absent" log path so the
   no-output symptom can never silently return.
4. Ship ladder: compat-191 flavor stays the promote default until F1 passes;
   then extended-198-weak becomes the feature build.
