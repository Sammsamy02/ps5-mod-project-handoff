# REPORT.md — worker-import-audit-weakify

Date 2026-10-04 · Scope: read-only import-surface audit of the actual composition-4
packaged artifact, the sdg2 import contract, and comp5; weakify procedure prepared but
NOT applied; SharpProspero Packager weak-emission capability verified from source. No
console contact, no target execution, no frozen artifact modified, no nested subagents.

## Result in one paragraph

**composition-4's actual packaged artifact carries exactly 196 imports, every one
STB_GLOBAL, zero weak — and it equals the frozen console-proven 191 plus exactly the 5
extension rows (sceKernelDlsym, sceKernelGetModuleInfo, sceKernelLoadStartModule,
sceKernelStopUnloadModule, qsort). sdg2's contract and staged probe are
identity+bind-set-identical to that surface (196 GLOBAL / 0 weak,
`required_strong=196 covered=196 missing=0 conflicts=0`), so sdg2 directly tests the
only 5 unproven strong binds. comp5 has already dropped 4 of the 5 structurally
(192 GLOBAL = frozen191 + GetModuleInfo only). The SharpProspero Packager cannot emit
STB_WEAK imports at all (hardcoded GLOBAL at `DynamicWriter.cs:779`; weak-only input
references are dropped and statically zero-bound, `Linker.cs:249` +
`DynamicWriter.cs:1473`), so a smallest patch was designed but not implemented — the
post-packaging `import_fallback.py weakify` rewrite achieves the same dynsym result
with one st_info byte per row. Recommendation (`WEAKIFY-DECISION.md`): STAY STRONG for
composition-5; composition-4's ploader-OK with all 196 strong is direct evidence the 5
bind, sdg2 makes it rigorous, and weakify is the prepared contingency only for a sdg2
load-FAIL branch.

## Key measured findings (details + citations in AUDIT-RESULTS.md)

1. **Audit verdicts.** comp4 packaged (sha `de372302…`, 30,778,567 B, the file staged
   per `worker-staging-pkg-c4/MASTER-STAGING-PACKAGE-V2.md` §A): 196 UND GLOBAL,
   0 WEAK, 2 defined, verified byte-identical dynstr+dynsym inside the SELF vs its
   bare-sdk (unique full-blob matches; surface equality asserted). sdg2 run1==run2
   (`0e5b36bc…`): same 196 GLOBAL. comp5 run-1==run-2 (`34fb620b…`): 192 GLOBAL
   (frozen191 + `kUpgrXIrz7Q` GetModuleInfo). Frozen proven baseline (sha `3b998f09…`,
   pin verified): 191 GLOBAL, 0 weak rows.
2. **Two retained-narrative corrections (evidence-backed).** (a) The "198-import
   candidate" model is wrong for the actual bytes: `clock_gettime`/`getpid` are inside
   the frozen proven 191; there is no 197th/198th row. (b) The "six already-weak dynsym
   rows" (`__dlopen` etc.) exist in NO audited dynsym — the Packager statically
   zero-binds weak-only references instead of emitting them
   (`Linker.cs:114-115,249`; `DynamicWriter.cs:1473,824-826,1044-1045,1552-1555`).
   Also line-corrected: the GLOBAL-hardcoding emission is `DynamicWriter.cs:779` in the
   composition-4 harness copy (retained reports cite `:631` of another revision).
3. **Zero-guard surface is small and data-only.** `.rela.plt` holds 4 JUMP_SLOTs, none
   for the 5 targets — no direct call sites exist anywhere in the runtime. Each target
   has exactly one `R_X86_64_64` into `modbridge_supplier_import_table`
   (`0x1c9dd58`–`0x1c9dd78`, slot order matching `worker-startup-checkpoint/REPORT.md`
   §1) and one GLOB_DAT GOT slot. Weakified-and-unresolved therefore means "pointer
   reads 0 at two known data locations", guarded by `optional_imports.h` rules R1-R4 —
   never an implicit call through a zeroed stub.
4. **Packager verdict.** Cannot emit STB_WEAK; smallest source patch designed
   (2 files, ~4 edits: `Import.Weak` field, Linker weak-only inclusion, emission
   `((imp.Weak?2:1)<<4)|type`, GOT-reloc check) and recorded in `AUDIT-RESULTS.md` §4 —
   strictly worse than the one-byte-per-row post-packaging rewrite for identical output.
5. **Decision.** STAY STRONG (`WEAKIFY-DECISION.md`): stay-strong is supported by
   comp4's ploader-OK, comp5's structural drop of 4/5, and evidence value of the sdg2
   experiment; weakify is the prepared contingency (exact command in
   `AUDIT-RESULTS.md` §5.3) gated on a sdg2 load-FAIL, with a branch matrix for all
   four sdg2 outcomes.

## Deliverables (this directory)

- `CLAIM.json` (written first, updated with results)
- `AUDIT-RESULTS.md` — per-target verdicts, classification table, Packager §4 with
  patch design, weakify procedure §5 with measured guard surface and exact command
- `WEAKIFY-DECISION.md` — decision, evidence, branch matrix, honest limits
- `REPORT.md` (this file, last)
- Raw evidence: `audit-{comp4,sdg2,comp5}-bare-sdk.json`,
  `audit-frozen-proven-bare.json`, `audit-frozen-baseline.json`,
  `self-dynsym-{comp4,sdg2,comp5}-packaged.json`, `comp4-import-map.json`,
  `five-target-rows-comp4.json`, `comp4-vs-comp5-delta.txt`,
  `dynsym_from_packaged_self.py`, `rela_targets.py`,
  `comp4-packaged-inner.elf` (carved analysis copy; original untouched)

## Method notes and limits

- All five packaged `.prx` are fake-SELF (magic `4F153D1D`, inner ELF at 0x1A0,
  `e_shnum=0`); the section-based `import_fallback.py audit` was run on the bare-sdk
  Packager ELFs, and the actual SELF bytes were verified independently (PT_DYNAMIC
  structure + unique full-blob dynstr/dynsym matches + enumerated set equality). The
  SELF wrapper's own integrity structure was never patched.
- The ploader-OK for composition-4 is retained controller evidence (task context); no
  console was contacted to re-verify the raw rc. "Weak binds to 0" is rtld-lineage
  reasoning plus the Packager's own documented model, never yet measured on this
  console — the weakify contingency would need its own staged validation.
- Tool calls used: 36 of 40. Everything outside
  `outputs/2026-10-04-zcode-team/worker-import-audit-weakify/` was only read.
