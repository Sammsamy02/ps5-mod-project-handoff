# AUDIT-RESULTS.md — worker-import-audit-weakify

Date 2026-10-04 · Worker: worker-import-audit-weakify · Scope: read-only ELF/contract
audit plus tool preparation. No artifact was modified, no console contact, no target
execution. Tool: `outputs/2026-10-04-zcode-team/worker-import-fallback/import_fallback.py`
(used UNMODIFIED, `audit` subcommand). Raw outputs in this directory (`audit-*.json`,
`self-dynsym-*.json`).

## 1. Targets audited (exact identities)

| Target | File (absolute under workspace root `what/`) | sha256 | bytes |
|---|---|---|---|
| composition-4 ACTUAL packaged artifact (staged to `/data/plugins/`, per `worker-staging-pkg-c4/MASTER-STAGING-PACKAGE-V2.md` §A order table) | `outputs/2026-10-04-zcode-team/worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx` | `de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f` | 30,778,567 |
| composition-4 bare-sdk ELF (Packager output, sdg2 oracle) | same dir `/bare-sdk/modbridge_supplier_composition.prx` | `c8cb94ae39bee8b648fb2ae9f4406a954287efe9af4663adb1ee24f0900aa3fb` | 30,771,688 |
| sdg2 probe of record (imp variant, run1 == run2 byte-identical) | `outputs/2026-10-04-zcode-team/worker-startup-checkpoint/diag/determinism/imp/run1/modbridge_supplier_composition.prx` | `0e5b36bc03d97619990a061baafd077fb331d3c0fd50249488aaaace50d32d86` | 22,087 |
| comp5 (run-1 == run-2 byte-identical) | `outputs/2026-10-04-zcode-team/worker-composition-5-minimal/composition-5/run-1/modbridge_supplier_composition.prx` | `34fb620bb256a497c157ce548a04a41e112a2ab654ac6d78a66ab71d4e15ad7f` | 21,623 |
| frozen console-proven baseline ("loads in Townfall every time"; pinned at `worker-prx-packaging/package_prx.py:80-91` as frozen_prx, sha verified `3b998f09…`) | `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx` (+ its own `bare-sdk/` ELF twin, audited) | `3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654` | 30,151,623 |

All five packaged `.prx` files are fake-SELF wrappers (magic `4F 15 3D 1D`, ELF64
payload at offset 0x1A0, section headers stripped: inner `e_shnum=0`). The section-based
`import_fallback.py audit` was therefore run on each target's bare-sdk ELF (the exact
Packager output the SELF wraps), and the actual packaged SELF bytes were independently
verified with `dynsym_from_packaged_self.py` / blob-search (this directory): the SELF's
PT_LOAD payload carries the dynstr and dynsym tables byte-identical to the bare-sdk
(dynstr blob and full symtab blob each found at exactly one offset in every SELF, and the
enumerated `(identity, bind)` sets match the section audit exactly):

| SELF | symtab/dynstr blob found (unique) | SELF dynsym surface == bare-sdk audit |
|---|---|---|
| comp4 packaged | symtab `0x1ce0eb8`, dynstr `0x1ce0180` (+0x22c0 vs bare-sdk) | TRUE |
| sdg2 run1 packaged | delta −0xe790 vs bare-sdk | TRUE |
| comp5 run-1 packaged | delta −0xe830 vs bare-sdk | TRUE |

## 2. Verdict per target

### 2.1 composition-4 ACTUAL packaged artifact — 196 imports, ALL STRONG, 0 weak

- dynsym total 199 rows = 1 null + **196 UNDEFINED imports, every one STB_GLOBAL** + 2
  defined (own exports). ZERO STB_WEAK rows.
- The `import_fallback.py DEFAULT_TARGETS` five are present, all GLOBAL, all with
  Packager long-form identities:

| # | friendly | dynstr identity | dynsym idx | bare-sdk st_info byte offset | SELF st_info byte offset |
|---|---|---|---|---|---|
| 1 | sceKernelDlsym | `LwG8g3niqwA#B#C` | 192 | `0x1cdfdfc` | `0x1ce20bc` |
| 2 | sceKernelGetModuleInfo | `kUpgrXIrz7Q#B#C` | 193 | `0x1cdfe14` | `0x1ce20d4` |
| 3 | sceKernelLoadStartModule | `wzvqT4UqKX8#B#C` | 194 | `0x1cdfe2c` | `0x1ce20ec` |
| 4 | sceKernelStopUnloadModule | `QKd0qM58Qes#B#C` | 195 | `0x1cdfe44` | `0x1ce2104` |
| 5 | qsort | `AEJdIVZTEmo#A#B` | 196 | `0x1cdfe5c` | `0x1ce211c` |

(`st_info = 0x12` = GLOBAL|FUNC for all five; the byte to flip for STB_WEAK is the
st_info byte, value `0x12 -> 0x22`.)

- Set difference vs the frozen proven baseline (below): comp4 = frozen191 ∪ **exactly
  these 5**, nothing else missing or added (proven by identity-set diff, `comp4-vs-comp5-delta.txt`).

### 2.2 sdg2 import contract — surface-identical to composition-4

- `expected-imports.json`: `variant=imp`, `und` = 196 rows, bind distribution
  **GLOBAL 196 / WEAK 0**, types FUNC 192 / OBJECT 4; `oracle` = comp4 bare-sdk
  (`c8cb94ae…`, hash re-verified here).
- `import-contract.json` counts: `required_strong=196, required_weak=0,
  identity_covered_strong=196, covered_strong=196, missing_strong=0, conflicts=0`;
  `weak_unresolved_names=[]`, `contract.unresolved_weak_names=[]`.
- Auditing the actual sdg2 run1 PRX gives the same 196 identities and binds, and
  **identity+bind set equality with composition-4 holds exactly** — sdg2 is a faithful
  import-surface twin of the packaged comp4. run1 == run2 (`0e5b36bc…` both).

### 2.3 comp5 — 192 imports, ALL STRONG; 4 of the 5 rows structurally removed

- 195 dynsym rows = 1 null + **192 UND imports all GLOBAL** + 2 defined.
- comp5 = frozen191 ∪ {`sceKernelGetModuleInfo`} exactly. Dropped vs comp4:
  `qsort (AEJdIVZTEmo)`, `sceKernelDlsym (LwG8g3niqwA)`,
  `sceKernelStopUnloadModule (QKd0qM58Qes)`, `sceKernelLoadStartModule (wzvqT4UqKX8)`.
  comp5-only rows: none.
- The single extension is referenced address-taken only
  (`composition-5/run-1/build/supplier_import_extension.S`: one typed `.quad`
  `sceKernelGetModuleInfo` in `modbridge_supplier_import_table`); `module_entry_comp5.c`
  never calls it, it logs the resolved pointer
  (`worker-composition-5-minimal/REPORT.md` §1).
- comp5-zero control (exactly frozen 191): `composition-5-zero/run-1`, sha
  `1cb624817ad7df03b32ceb7bb4c2fe837b7787fdd0b187ea198332e95b6b139e` (per its REPORT).

### 2.4 Frozen console-proven baseline — 191 strong, 0 weak rows

- bare-sdk twin audits: **191 UND imports, all GLOBAL, zero weak dynsym rows**.
- `clock_gettime` and `getpid` are INSIDE the proven 191 (verified by NID mapping) —
  the earlier "198-import candidate = 196 + clock_gettime + getpid" model does not
  describe the actual composition-4 bytes; the real surface is exactly 196.
- The six names `__dladdr __dlclose __dlerror __dlopen __dlsym kernel_mprotect` do NOT
  appear as dynsym rows in any audited artifact. They are weak-only references in input
  objects that the Packager resolves statically to 0 instead of emitting imports
  (see §4). The "six already-weak dynsym rows" retained narrative
  (`worker-import-fallback/import_fallback.py` docstring; `PLOADER-IMPORT-ANALYSIS.md`
  §5) is corrected by this audit: weak BIND-BY-LOADER rows do not exist in this
  toolchain's output.

## 3. Strong / weak / unproven-on-console classification (final)

| Import row | comp4 | sdg2 | comp5 | frozen191 | Classification |
|---|---|---|---|---|---|
| frozen 191 names (incl. clock_gettime, getpid) | GLOBAL | GLOBAL | GLOBAL | GLOBAL | **proven on console** (baseline "loads every time") |
| sceKernelDlsym | GLOBAL | GLOBAL | absent | absent | **unproven on console as a strong bind** |
| sceKernelGetModuleInfo | GLOBAL | GLOBAL | GLOBAL | absent | **unproven on console as a strong bind** |
| sceKernelLoadStartModule | GLOBAL | GLOBAL | absent | absent | **unproven on console as a strong bind** (export existence in-process IS proven: ploader resolves `wzvqT4UqKX8` itself, `ploader…/main.c:33`) |
| sceKernelStopUnloadModule | GLOBAL | GLOBAL | absent | absent | **unproven on console as a strong bind** |
| qsort | GLOBAL | GLOBAL | absent | absent | **unproven on console as a strong bind** (libc NID drift risk, `PLOADER-IMPORT-ANALYSIS.md` §5) |

Context evidence shortening this risk list: composition-4 (all 196 strong) was accepted
by ploader in-game with no abort (controller report; task context). A refused strong
bind aborts the load before init and ploader surfaces FAIL with the kernel rc
(`ploader-return-message-1`), so the OK is direct evidence the five DID bind — sdg2's
DIAG line converts this into rigorous proof (binding precedes module init; DIAG at
module_start ⇒ load+bind completed).

## 4. SharpProspero Packager STB_WEAK capability — CANNOT emit weak imports

Verified from the exact harness copy used to build composition-4
(`worker-composition-4-fix/prx-candidate-composition4-1/harness/tools/SharpProspero.Link/`):

1. **Import dynsym rows are hardwired GLOBAL.** `DynamicWriter.cs:779`:
   `e[4] = (byte)((1 << 4) | importType); // GLOBAL, explicitly evidenced or
   legacy-inferred kind` — inside the import emission loop (`DynamicWriter.cs:768-782`).
   No code path in the file writes `(2 << 4)` (STB_WEAK) for a dynsym row; the
   corresponding export row is also hardcoded GLOBAL (`DynamicWriter.cs:790`). The
   `Import` record class carries no binding field at all (`DynamicWriter.cs:367-382`).
2. **Weak-only references never become imports.** `Linker.cs:114-115` collects import
   names from strong undefined symbols only ("A reference that is only ever weak and
   stays unsatisfied is a legal binding to address zero, not a link error");
   `Linker.cs:249` explicitly drops weak-only names ("A weak-only reference that nothing
   satisfies is left out; the writer binds it to zero"), implemented statically:
   `SymbolValue` returns 0 for unresolved weak input symbols (`DynamicWriter.cs:1473`),
   and GOT slots for unresolved weak targets keep their zero-initialized value with no
   dynamic relocation (comments/logic at `DynamicWriter.cs:824-826, 1044-1045,
   1552-1555`).

So the Packager cannot emit STB_WEAK imports, and cannot represent them. This confirms
and sharpens the retained note "all Packager-emitted import records are GLOBAL"
(cited there as `DynamicWriter.cs:631` of a different revision; line 779 in this copy).

### Smallest Packager patch (DESIGNED, NOT IMPLEMENTED)

Only if weak dynsym rows must come from the toolchain itself (they do not need to —
see §5):

1. `Import` class (`DynamicWriter.cs`, import record): add `public bool Weak;`.
2. `Linker.cs:224-251`: when collecting imports, also include weak-only undefined names
   marked `Weak = true` (today they fall through at line 249). This is the only
   semantic change; name/module/library identity plumbing is unchanged.
3. `DynamicWriter.cs:779`: `e[4] = (byte)(((imp.Weak ? 2 : 1) << 4) | importType);`.
4. Verify GOT emission for weak imports: `ProducesDynReloc` /
   `ResolvesLocally` must still emit the GLOB_DAT record for a Weak import row so the
   loader writes 0 into the slot when unresolved (today the no-reloc zero-slot behavior
   is keyed to "not an import" at `DynamicWriter.cs:824-826, 1044-1045`).

Touch points: 2 files, ~4 edits, plus regression of the 10 recorded `patch_packager()`
count substitutions (`worker-composition-5-minimal/REPORT.md` §2) — materially larger
and riskier than the post-packaging rewrite below, for byte-identical dynsym output.

## 5. Weakify procedure outputs for the 5 rows (PREPARED, NOT APPLIED)

### 5.1 What the STB_WEAK rewrite changes (and nothing else)

`import_fallback.py weakify` rewrites exactly the five st_info bytes in §2.1's table:
`0x12 -> 0x22` (bind nibble GLOBAL→WEAK, type nibble FUNC preserved;
`import_fallback.py:208`). Everything else is verified byte-identical by the tool's
fail-closed contract (single SHT_DYNSYM; every target UNDEFINED; DEFINED targets abort;
byte-diff loop aborts on any change outside the five st_info bytes;
`import_fallback.py:26-34, 173-232`).

Semantics on the PS5 loader (FreeBSD rtld lineage, argued in
`optional_imports.h:5-9`): an unresolved WEAK undefined symbol binds to 0 and the load
continues, where the GLOBAL form aborts the whole load. Relocation records are NOT
rewritten: all five are data-bound (see §5.2), so rtld simply fills the existing
GLOB_DAT / R_X86_64_64 slots with the bound value — nonzero, or 0 when unresolved.

### 5.2 Call sites that need zero-guards (exact, measured)

Relocation audit of comp4 bare-sdk (`rela_targets.py`, this directory):

- **`.rela.plt` has 4 JUMP_SLOT entries, NONE for the five targets.** No direct
  `call` sites exist for any of the five — nothing needs PLT-level rework, and no
  indirect call can pass through a stub that jumps to 0.
- Each of the five has exactly two data bindings:
  - one `R_X86_64_64` into `modbridge_supplier_import_table` slots
    (`0x1c9dd58` Dlsym, `0x1c9dd60` GetModuleInfo, `0x1c9dd68` LoadStartModule,
    `0x1c9dd70` StopUnloadModule, `0x1c9dd78` qsort) — matching the supplier import
    table identified by `worker-startup-checkpoint/REPORT.md` §1 (5 relocs at
    `0x1c9dd58-78`, slot order Dlsym, GetModuleInfo, LoadStartModule, StopUnloadModule,
    qsort);
  - one `GLOB_DAT` GOT slot each (`0x1c9c218` LoadStartModule, `0x1c9c220` Dlsym,
    `0x1c9c228` StopUnloadModule, `0x1c9cbb8` qsort, `0x1c9d0e8` GetModuleInfo).

Therefore the guard surface is exactly the consumers of those two address classes:
the code that reads `modbridge_supplier_import_table` slots and the code that reads the
five GOT slots — each consumer must test the pointer for 0 before any indirect call.
That is precisely rules R1-R4 of `worker-import-fallback/optional_imports.h`
(snapshot once at module_start into capability flags; qsort gets the in-module heapsort
fallback `mb__heapsort`, `optional_imports.h:98-131`; sce* features degrade with reason
codes `OPTIMP_NO_*`, `optional_imports.h:49-55`; no optional reference from fini paths).
comp5's single extension is already compliant (address-taken, never called,
pointer logged — `worker-composition-5-minimal/REPORT.md` §1).

### 5.3 Exact prepared command (contingency only; NOT run)

```
python3 outputs/2026-10-04-zcode-team/worker-import-fallback/import_fallback.py weakify \
  --elf outputs/2026-10-04-zcode-team/worker-composition-5-minimal/composition-5/run-1/bare-sdk/modbridge_supplier_composition.prx \
  --out <NEW-DIR>/modbridge_supplier_composition.weak.bare-sdk.prx \
  --names sceKernelDlsym,sceKernelGetModuleInfo,sceKernelLoadStartModule,sceKernelStopUnloadModule,qsort \
  --report <NEW-DIR>/weakify-report.json
```

- For comp5 only `sceKernelGetModuleInfo` exists in dynsym; the other four names are
  "missing requested imports" and the tool aborts without `--allow-missing` — correct
  fail-closed behavior. For comp5 use `--names sceKernelGetModuleInfo`. For comp4 use
  the full five (all present).
- Pipeline ordering if ever applied: weakify operates on the ELF (bare-sdk) AFTER
  finalize_sdk (it touches no module metadata, so the 0x11000000 stamp bytes are
  untouched) and the output must then be rewrapped with make_fself; never patch the
  packaged SELF in place (the wrapper carries its own integrity structure).
- Application requires: new distinct artifact IDs/paths, the R1-R4 guard review for any
  reachable consumer, and the staging checklist gates. Nothing here was applied.

## 6. Raw evidence files (this directory)

`audit-comp4-bare-sdk.json`, `audit-sdg2-bare-sdk.json`, `audit-comp5-bare-sdk.json`,
`audit-frozen-proven-bare.json`, `audit-frozen-baseline.json` (the 83 KB
`worker-prx-packaging/prx-candidate-20261004-133501/bare-original` — audits as 196
GLOBAL, i.e. it is NOT the frozen proven baseline; recorded to prevent mis-citation),
`self-dynsym-comp4-packaged.json`, `self-dynsym-sdg2-packaged.json`,
`self-dynsym-comp5-packaged.json`, `comp4-import-map.json` (all 196 rows, friendly
names), `five-target-rows-comp4.json`, `comp4-vs-comp5-delta.txt`,
`comp4-packaged-inner.elf` (carved copy, sha `58514643c180c0c97bec4222278b80540d42a58d3418c644d320da651ad45f1a`,
30,769,960 B — analysis copy only; the frozen artifact was not modified),
`dynsym_from_packaged_self.py`, `rela_targets.py`.
