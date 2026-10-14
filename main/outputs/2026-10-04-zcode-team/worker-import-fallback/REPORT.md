# REPORT.md — worker-import-fallback (2026-10-04)

Task: design and prototype an import-resolution fallback letting the 7
additional imports of the composed runtime resolve lazily or optionally.

## Deliverables (all in this directory)

* `CLAIM.json` — registered scope first.
* `FALLBACK-DESIGN.md` — full design: problem, both resolver paths read,
  four approaches (weak / lazy / runtime-dlsym / conditional) with trade-offs,
  recommended hybrid, risks F1–F4, integration recipe.
* `import_fallback.py` — prototype packaging-side tool: byte-precise ELF64
  `.dynsym` GLOBAL→WEAK demotion of selected imports, mirrored `sce_nid()`
  identity decoding, fail-closed + idempotent, JSON report.
* `test_import_fallback.py` — 18 self-contained assertions on a synthetic
  ELF64 (exactness, controls, idempotency, fail-closed missing/DEFINED,
  CLI + audit). **All pass** (run recorded in section Evidence).
* `optional_imports.h` — reference runtime discipline: module_start weak
  snapshot, capability flags + reason codes, in-module heapsort qsort
  fallback. Proposal-only; composed source untouched.
* `use_optional_imports.c`, `test_optional_imports.c` — compile probe
  (frozen-build flags) and host functional tests. **All pass.**

## Key findings (measured)

1. **The bridge PE loader is strong-unresolved-fatal by design**
   (`windows-image/pe_image.h`: any `MB_PE_BIND_UNRESOLVED` fails prepare;
   `windows-execution/pe_module.c` `open_impl` → `MB_PE_MODULE_UNRESOLVED_IMPORTS`
   + `cleanup()`). No lazy/tolerant hook exists on that path; the fallback
   must therefore live in the PRX side and yield NULL-checked slots.
2. **The frozen 191-import PRX that loads in Townfall already contains six
   WEAK undefined imports** (`frozen_allowed_weak` in
   `worker-prx-packaging/package_prx.py`: `__dladdr __dlclose __dlerror
   __dlopen __dlsym kernel_mprotect`). This is the strongest available
   evidence that weak imports are an accepted, load-tolerated pattern on the
   target loader.
3. **Packager mechanics**: imports = `.dynsym` UNDEF entries with bind
   GLOBAL(1) or WEAK(2), NID long-form identity `<nid>#<lib>#<mod>`
   (mirrored `sce_nid` verified against `malloc→gQX+4GDQjpM`). Weak demotion
   post-link is expressible in exactly one byte per import (`st_info` nibble).
4. **Lazy binding rejected** on evidence: PS5 PRX imports are kernel-bound at
   load; frozen lineage explicitly forbids loader-owned dynamic arrays.
   **Runtime dlsym alone rejected** (bootstrap circularity: `sceKernelDlsym`
   is itself one of the 7); kept as consumer of the weak approach.

## Recommendation

**WEAK demotion (packaging, post-link) + guarded snapshot (runtime) + a
191-strong "compat" flavor kept as the promote default** until in-game
verification F1 passes. `clock_gettime`/`getpid` enter the weakify target
list only after `import_fallback.py audit` on the actual 198-import candidate
confirms them (context flags them "possibly present"; the measured delta is
the 5-symbol list).

## Evidence (executed locally this session; no console/network)

* `python3 test_import_fallback.py` → `ALL TESTS PASSED` (18 assertions,
  incl. exactly-2-bytes-changed, control `sceKernelOpen` stays GLOBAL,
  already-weak `__dlsym` untouched, DEFINED export refused).
* SDK-pinned clang 21.1.8 with the frozen flags
  (`-Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector
  -fvisibility=hidden -ffreestanding -fno-builtin`, `-isysroot` for host
  resource headers only) → freestanding use-TU compiles clean; `nm -u`
  shows **exactly the 5 optional imports** as UND, nothing else.
* `test_optional_imports.c` → `ALL OPTIONAL-IMPORTS TESTS PASSED` (caps
  probe, delegated qsort, fallback heapsort n=0..256 with duplicates,
  degenerate-arg no-ops).

## Honest gaps / not done

* No in-game or loader-level execution: "weak-undef binds to 0 and the load
  continues" is loader-family expectation consistent with finding 2, but F1
  (probe PRX with weakified imports) is still required before any feature
  enables. Weak-ness here changes `.dynsym` only; F3 must confirm the
  SceModuleInfo import-fragment attribute encoding matches the frozen PRX's
  six weak rows.
* `package_prx.py` is read-only for this worker: the integration diff
  (weakify step + weak-aware contract check) is specified in
  FALLBACK-DESIGN.md §7 but not applied by me.
* The Windows PE candidate path still requires real binder resolution for
  these imports (strong-unresolved is fatal there by design); noted for
  parity, out of scope.
* No promotion, no ">8 quality", no completion claim. Budget: 41 tool calls
  of ≤50; no bounds crossed.
