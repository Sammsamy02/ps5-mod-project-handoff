# REPORT — worker-v3-startup-diag (2026-10-04)

## Status: DELIVERED — frozen minimal diagnostic variant + corrected oracle
## + ploader return-code analysis + v2↔v3 binary diff. Cause remains
## UNCONFIRMED by design of the evidence; the staged diagnostic decides it.

## Deliverable (frozen, ready to stage)

**MINIMAL STARTUP DIAGNOSTIC — build `igd1-fc26d6c03038`**

- Payload: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-v3-startup-diag/diag/determinism/run1/modbridge_ingame_facts.prx`
- fself 12173 B, sha256 `13eeb7c55e82a83b4aee3b90d36231ca7738c5431d4974ed9785d4555dd28754`
- Inner ELF (bare-sdk) 68880 B, sha256 `78dbf702f486255b6239ff8a9c9e76f4100d29cbf242370981145deaebc39ecd`
- Contract `c5c482a75423fab0ce24f21710f24e1a6f496f53cc93d0c8f02b741b1171e17f` — the SAME 7 verbatim frozen rows.
- Module name / staged path / log path UNCHANGED (replaces the v3 file in
  place, no ini change). Records carry `"build_id":"igd1-fc26d6c03038"`.
- Two-run determinism: run1 == run2 byte-identical (fself, inner ELF
  original+stamped, combined object, contract) — DETERMINISM-PASSED.
- Packaging pipeline = the frozen v2/v3 recipe end to end (import_references-
  first `ld -r`, SharpProspero Packager, frozen finalize_sdk.py SDK
  0x11000000 stamp, pinned make_fself); LOCAL_PACKAGE_CHECKS_PASSED.
- module_start (275 B, disassembly-verified) = guard →
  `sceKernelOpen(/data/modbridge-ingame-facts.jsonl, O_WRONLY|O_CREAT|
  O_APPEND|O_NOFOLLOW, 0600)` → one seq=1 `"entry"` record (v3 writer,
  bounded 64-attempt write) → `sceKernelClose` → **return 0 always** (even
  on open failure — the controller's "return 0" directive; v3 returned -22
  there, and kernel-side propagation of a nonzero entry return is unknown,
  so the diagnostic removes that variable). NO experiments, NO VMMAP, NO
  predicates, NO base scan, NO ctor walk, **NO C++ in the binary at all**.
- Staging runbook + outcome matrix: `STAGE-PLAN-DIAG.md` (same dir).

## 1. ploader semantics (retained source, read in full context)

`.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`:

- `inject_prx()` resolves NID `wzvqT4UqKX8` (sceKernelLoadStartModule) and
  calls it remotely with the PRX path; its return value `mod` is the ONLY
  thing ploader sees.
- Line 810: `int ok = (rc > 0);` → **"OK" means LoadStartModule returned a
  positive module id.** "OK (~1207ms)" on both v3 launches therefore proves
  the kernel loader ACCEPTED the v3 PRX (mapped segments, resolved the 7
  imports) — injection-level, not module_start-level.
- **module_start's own return code never reaches ploader.** It goes to the
  kernel's module-start step. Whether the kernel folds a nonzero entry
  return into LoadStartModule's result is NOT determinable from this source.
  Consequently: v3's `-22` open-failure branch would only have surfaced as
  ploader "FAIL" IF the kernel propagates entry failures — user saw OK, so
  either v3's module_start never ran, ran and returned 0, or ran, returned
  -22, and the kernel ignored it.

## 2. Staging was NOT the failure (oracle forensics)

The controller-reported console readback (108216 B, sha 86def0e7…, `\x7fELF`)
initially looks like a mismatch against STAGE-PLAN-V3's oracle ("exactly
109776 B / c52e582d"). It is not:

- The ftpsrv decryption readback is the **segment-covered prefix** of the
  inner ELF (make_fself embeds per-phdr `filesz` slices only; the SHT tail
  is not embedded). For the frozen v3 inner ELF the last phdr-covered byte
  is 0x1a6b8 = **108216 — exactly the observed size.**
- The same law holds across all three stagings: readback = bare-sdk − 1560
  (v1: 98240→96680; v3: 109776→108216; the new diag build: 68880→67320).
- Full-hash equality through this channel is impossible by construction
  (SHT tail stripped, e_shnum zeroed in the embedded header — verified in
  the pinned make_fself.py and encoded in check_ingame_artifact.py line 44).
  STAGE-PLAN-V2 documented this ("full-hash equality IMPOSSIBLE"; v1
  tolerance "sparse header/SHT-rebuild bytes + trailing-section truncation").
  **STAGE-PLAN-V3's exact-size/exact-hash oracle expectation contradicted
  that documented channel behavior and was wrong.**

Conclusion: the on-console file was structurally exactly the frozen igf3
inner as embedded (size law + ELF magic). Staging/upload is exonerated; the
v3 zero-records result is a module/runtime-level fact.

## 3. v2 ↔ v3 binary diff (inner ELFs, llvm-readelf/objdump)

Loader-visible structure — IDENTICAL in shape:

- dynsym: same 10 entries in both (7 verbatim NID-encoded UND imports +
  module_start/module_stop exports, same NID names).
- DT_INIT = module_start (the guard/-20/-22 pattern; renamed
  `BaOKcng8g88#C#A`) and DT_FINI = module_stop in BOTH.
- **module_start is instruction-for-instruction IDENTICAL between v2 and v3**
  (same 588 bytes; only addresses differ). The pre-marker path (guard →
  open → build record → write) is the same bytes that logged 56 records in
  v2.
- **Task item 4 answered NO: v3 introduces NO static C++ constructors.**
  `.init_array` is exactly 8 bytes (one sentinel slot) in BOTH builds;
  boundaries.S is the sole contributor and C++ globals with dynamic
  initialization would have grown it. There is no code that runs before
  module_start except the loader's own relocation/import processing.
- Program headers: same 13-header layout, including the empty TLS phdr,
  PREINIT/INIT/FINI arrays all zero-size (loader-unowned) in both.
- Relocations: v3 has one extra R_X86_64_RELATIVE (13 vs 12) and identical
  import relocation kinds (7 R_X86_64_64 + 7 GLOB_DAT). Nothing that could
  plausibly fault a loader that accepted the same 25 before.

The ONLY loader-visible deltas v2→v3 are SIZE-shaped: .text 22 KB→40 KB,
.rodata 3 KB→7 KB, **.bss 4.2 MB→5.6 MB** (v3's IG_SPAN_MAX 16384 evidence
arrays), fself 30701→53165 B, +1 relative reloc. Since module_start's code
is byte-identical and no pre-start code exists, the unconfirmed hypotheses
that survive the "OK + untouched log + no game crash" evidence are:

1. module_start never ran (loader accepted the module but the start step did
   not reach the entry — mechanism unknown; size/segment-shape sensitivity
   cannot be excluded from here);
2. module_start ran and `sceKernelOpen` failed (-22), with the kernel
   ignoring the entry return (ploader showed OK);
3. module_start ran, open succeeded, and the first write failed silently
   (append-open does not touch mtime, so an untouched file is consistent
   with this too) — least likely given the sibling wrote fine in the same
   launch, but not excludable.

## 4. How the diagnostic decides (STAGE-PLAN-DIAG.md §4)

- Record APPEARS → loads+starts; write path fine → v3's failure is in the
  post-marker experiment code → incremental bisection ladder (igd2 = +VMMAP,
  igd3 = +base hunt, igd4 = +predicates/C++), one launch each.
- Record ABSENT, game fine → packaging/size-level (the only surviving delta
  class); control-restage v2 to re-prove the slot, then bisect build deltas
  (.bss 24 B vs 5.6 MB; GLOB_DAT 4 vs 7; fself 12 KB vs 53 KB).
- Record ABSENT + crash → PRX-format/import-level fault (a 12 KB 7-import
  module faulting the system implicates the format itself).
- Record malformed → writer bug in the shared record path.

Honest limitation (frozen scope): absent-record cannot separate "never
started" from "open failed" — no second channel exists in the frozen spec.

## 5. Files (all under outputs/2026-10-04-zcode-team/worker-v3-startup-diag/)

- `CLAIM.json` — original claim (task later corrected by the controller:
  cause unconfirmed, minimal variant + matrix required; this report follows
  the correction).
- `STAGE-PLAN-DIAG.md` — staging runbook, CORRECTED oracle expectations
  (67320 B = 68880 − 1560, ELF magic, no full-hash), outcome matrix,
  ploader semantics, rollback.
- `diag/src/` — minimal variant sources (v3-vendored record_sink/libc/abi/
  imports/boundaries/sentinels byte-identical; module_entry.c rewritten to
  the exact minimal contract).
- `diag/build_ingame.py` — vendored build script (no C++, no experiments,
  igd1- prefix); `diag/harness/` + `diag/check_ingame_artifact.py` vendored.
- `diag/determinism/run1/`, `run2/` — the two byte-identical builds
  (payload in run1); `diag/build-fc26d6c03038/` removed by the determinism
  runner (renamed to run1/run2).

## 6. Not done / not claimed

- No console access, no staging, no launch performed by this worker — the
  controller stages per STAGE-PLAN-DIAG.md; the launch stays user-coordinated.
- The v3 failure cause is NOT claimed identified. The incremental igd2–igd4
  bisection variants are NOT built (controller directive: one frozen minimal
  variant now; the ladder runs only on matrix outcome 1).
- The exact ftpsrv reassembly transform (for full-hash oracle prediction) is
  not derivable from the workspace; the size law (−1560, three builds) plus
  magic is the established and sufficient acceptance check.
- Old 018 art-merge integration and all other queues: out of scope.
