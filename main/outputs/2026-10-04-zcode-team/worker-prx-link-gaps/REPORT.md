# worker-prx-link-gaps — REPORT

Date: 2026-10-04 · Scope: static packaging verification only; no in-game/native-execution claim.

## Result

**Passing candidate produced.** `prx-candidate-composition2-1/modbridge_supplier_composition.prx`
(30,745,815 bytes, sha256 `bc67f73c2e99c5c662c9b93824c2445c00c49486d481fbff59293e0d6267cadd`)
— `STATIC_CHECKS_PASSED`, 20/20 checks, zero gaps, composed from checkpoint
**zcode_supplier_composition_2** (`6701258c…`, 117,635,936 B, 364 units).
Fixed script: `package_prx.py` (vendored copy here) + `package_prx_composition2.patch`
(unified diff for the controller original at `outputs/2026-10-04-zcode-team/worker-prx-packaging/package_prx.py`).

## 1. Which object did the script consume?

The failing candidate `worker-prx-packaging/prx-candidate-20261004-133501` consumed
**composition-1**, not composition-2: its `build-receipt.json` ld.lld argv references
`ue4ss-native-supplier-composition-1/run-1/coherent-tick-file-witness-runtime.o`, and its
lineage verification pinned `fb2f4dd1…` / 117,624,920 B (composition-1). The pin was simply
stale relative to the current supplier state (composition-2, checkpoint
`zcode_supplier_composition_2`, ledger `6701258cbf432407` prefix, 117,635,936 B, 364 units
= 353 retained + 11 fresh incl. lmf-base-scan; full digest in the run receipt.json).

## 2. Gap diagnosis (all three are check-representation false negatives; the pipeline output was structurally correct)

Ground truth: the **frozen console-proven lineage PRX**
(`native-full-runtime-aligned-entry-1/build-1/bare-sdk/...`) publishes its entire dynsym in the
on-device **sce-nid long form `<nid>#<library>#<module>`** (e.g. `tsvEmnenz48#A#B`,
`malloc`→`gQX+4GDQjpM#A#B`) and exports `module_start`/`module_stop` only as
`BaOKcng8g88#F#A` / `KpDMrPHvt3Q#F#A`. The candidate (133501) has the identical representation.
The new static checks added by worker-prx-packaging compared raw dynstr text against friendly
contract names — a category error, not a packaging defect.

- **imports_equal_extended_contract** — the candidate's 196 UND dynsym entries are exactly the
  NID encodings of the derived 196-import contract (proved by implementing SharpProspero's
  `SceNid.Compute` in Python: SHA-1(name+16-byte salt), reverse first 8 bytes, base64 → 11 chars
  `/`→`-`; verified against 4 ground-truth pairs from the encoder's own documentation plus the
  frozen PRX). Raw-text compare produced 180 "extra" NID names vs 180 "missing" friendly names.
- **imports_resolve_to_sdk_or_linker** — same decoding issue; additionally the friendly-name
  check compared against the composition-1 baseline. After decoding: only
  `sceKernelOpen/sceKernelWrite/sceKernelClose/__cxa_finalize` fall outside the composition-2
  baseline (198 entries) — the frozen contract's typed bootstrap-shim references, identical in
  the proven lineage. `unaccounted=[]`.
- **entry_alignment_evidence** — two bugs: (a) it looked up `module_start`/`module_stop` by
  friendly name in .symtab/.dynsym, but the Packager exports them under NIDs; (b) its
  vaddr→file-offset mapping used phdr fields `p[2]`/`p[4]` (p_offset/p_paddr) instead of
  `p[3]`/`p[5]` (p_vaddr/p_filesz). With the fix, module_start vaddr 0x15a0 → file 0x55a0 shows
  `55 4889e5 4157 4156 4154 53 4883e4f0` — the `force_align_arg_pointer` prologue
  (`and rsp,-16` at +11); module_stop has it at +5. Byte evidence was always present.

Composition-2 vs composition-1 undefined-set difference is exactly one symbol: **`_DYNAMIC`
is dropped** (delta current 199 → 198; composition-2 delta `added=[sceKernelGetModuleInfo]`,
`removed=[]`, `changed_bindings={}`; ledger note "…_DYNAMIC already in composition-1
extension"). All 5 measured extensions and the other 191 contract names are unchanged, so the
196-import contract derivation carries over as-is.

## 3. Fixes applied (vendored copy; diff `package_prx_composition2.patch`, 346 lines)

1. **Pin → composition-2**: `composition_dir`, sha `6701258c…`, bytes 117,635,936, checkpoint
   `zcode_supplier_composition_2`. Ledger check accepts the composition_2 entry's
   `runtime_sha256_prefix` (16-hex) when no full digest is present.
2. **Delta expectations**: `added == [sceKernelGetModuleInfo]`; new checks
   `undefined_delta_drops_dynamic` and `undefined_delta_covers_extended_imports`;
   `linker_provided_dynamic_undefined` replaced by `composed_object_no_dynamic_reference`
   (asserts `_DYNAMIC` is *not* an undefined symbol of composition-2). The `_DYNAMIC`
   boundary/harness patches are retained as a documented no-op safety net.
3. **NID-aware static verification**: `sce_nid()` mirror + long-form regex; import-set equality
   and SDK/linker resolution checked on decoded NID identities with suffix histogram, malformed/
   unknown-nid accounting, and contract-backed outside-baseline breakdown.
4. **Entry-evidence fix**: resolve `module_start`/`module_stop` via dynsym NID exports (symtab
   friendly-name fallback), corrected phdr mapping; evidence now records the export long name,
   file offset and prologue bytes.
5. Housekeeping: ROOT discovery walks up to the `.superpowers` root (vendored copy sits deeper
   than the controller original); stale "195-import" reason string → 196; "10 fresh units" → 11;
   module docstring documents the NID long form.

## 4. End-to-end re-run

- `--verify-only` → `LINEAGE_VERIFIED`, 0 failed checks (dir `verify-composition2/`).
- `--link --out prx-candidate-composition2-1` → full pipeline (11 bootstrap objects, ld.lld -r
  partial link with the composition-2 object, dotnet harness, Packager, finalize_sdk 0x11000000,
  make_fself) → **STATIC_CHECKS_PASSED**, `build-receipt.json` gaps `[]`. Log: `link-run-1.log`.

No iteration on the candidate itself was needed — the first fixed-script run passed everything.

## 5. Limits (stated honestly)

Static verification only. "Passing" means: wrapper structure, ELF module type, PT_SCE_MODULE,
SDK stamp, explicit DT_INIT/DT_FINI, loader owns no init arrays, init/fini slots, unwinding
sections, NID-decoded import set equal to the derived 196-import contract, all imports
SDK-provider/contract-backed, and alignment prologue bytes at both NID exports. It is **not** an
in-game execution, load, or promotion claim (`promotion_allowed: false` in the package profile).
The candidate was built from composition-2 but the underlying gameplay runtime remains the
supplier prototype. The 211 MB candidate directory is preserved intact as evidence; the failing
133501 candidate and all worker-prx-packaging files were left untouched.

## Deliverables (all under `outputs/2026-10-04-zcode-team/worker-prx-link-gaps/`)

- `CLAIM.json`, `RECEIPT.json`, this `REPORT.md`
- `package_prx.py` — fixed vendored copy (runnable from this directory)
- `package_prx_composition2.patch` — unified diff against `worker-prx-packaging/package_prx.py`
- `prx-candidate-composition2-1/` — passing candidate + `build-receipt.json` +
  `verification-evidence/static-checks.json` + `supplier-contract.json`
- `verify-composition2/`, `link-run-1.log` — run evidence
