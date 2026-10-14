# DELTA-REPORT: composed-supplier PRX candidate vs the frozen proven PRX

Frozen proven baseline: `modbridge_runtime_aligned_entry.prx`
sha256 `3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654`,
30,151,623 bytes, checkpoint `native_aligned_entry` (ledger-verified unchanged).

New composed runtime input: `coherent-tick-file-witness-runtime.o`
sha256 `fb2f4dd16a20997455b39e6857fd2bdf45842c691bd0124a448948ae5bcb775c`,
117,624,920 bytes, 363 units (353 retained + 10 fresh), checkpoint
`zcode_supplier_composition_1` (receipt + ledger both verified).

## 1. New composition units (vs frozen combined-full.o)

The frozen combined object was `10 bootstrap packaging units + native-stage27-local-deps.o`
(the stage27 UE4SS runtime partial). The new combined object is
`10 unchanged bootstrap units + typed-import shim + the composed runtime object`.

Fresh units inside the composed object (from the composition receipt):
`unreal-unrealinitializer`, `native-ownedmappedscansource`, `native-psscanbinding`,
`native-ownedmappedcandidateselection`, `native-loadercontext`, `native-programfactory`,
`native-ownedserializedelfsource`, `lmf-live-module-facts`, `lmf-sce-ops`,
`lmf-loader-adapter`.

| Delta | Risk | Assessment |
|---|---|---|
| 10 fresh units replace the stage27-era tail of the runtime closure | Medium | Units were compiled by the composition run under its pinned gate (`build_runtime.py --extra-unit/--extra-define/--extend-undefined-baseline`), all 363 units hash-recorded in its receipt. Risk is runtime behavior, not build provenance. No in-game execution yet. |
| `lmf-sce-ops` compiled with `-DLMF_DISABLE_DL_RETENTION` | Medium | See section 3. Documented degraded mode; supplier yields zero facts instead of retaining handles. |
| Bootstrap units byte-identical (hashes recorded in build receipt) | Low | Same sources, same flags, same pinned toolchain as the frozen build. |

## 2. Extended undefined baseline / import table

Frozen contract: 191 strong imports (187 FUNC + 4 OBJECT) + 6 allowed weak.
The composed object's undefined set (undefined-delta.json, 199 current) was measured
against the frozen contract by the packaging pre-check (first real partial link).
Net effect: the final import table grows 191 -> 196 strong:

| New import | Provider | Evidence | Risk |
|---|---|---|---|
| `sceKernelDlsym` | libkernel (Soname `libkernel.prx`, ModuleVersion 257, LibraryVersion 1; identity cloned from the pinned `sceKernelOpen` row) | untyped GLOBAL UND in composed object (readelf); `sdknm-libkernel.so.stdout` nm match | Low: same provider family as existing kernel imports; the frozen module already imports 59 libkernel symbols |
| `sceKernelGetModuleInfo` | libkernel (same identity) | same; explicitly documented by checkpoint `zcode_supplier_composition_1` | Low |
| `sceKernelLoadStartModule` | libkernel (same identity) | same | Medium: only meaningful if module loading is attempted; harmless as an import |
| `sceKernelStopUnloadModule` | libkernel (same identity) | same | Medium: same as above |
| `qsort` | libSceLibcInternal (identity cloned from the pinned `malloc` row) | untyped GLOBAL UND in composed object; `sdknm-libSceLibcInternal.so.stdout` nm match; NOT documented by the composition checkpoint (stage27 defined qsort internally) | Low-Medium: discovered by the pre-check, not by the receipt delta; provider already carries 100+ libc imports in the frozen contract |

Linker-provided additions (not imports): `_DYNAMIC` is referenced UNTYPED by
`lmf-loader-adapter`. The SharpProspero resolver already classifies it
linker-provided (`Linker.LinkerProvided`), but `DynamicWriter` never gave it a
value, so relocations against it failed ("Unresolved symbol '_DYNAMIC'").
The packaging script mirrors `PayloadWriter`'s existing
`["_DYNAMIC"] = dynamicAddr` definition inside a COPIED SharpProspero source
tree (frozen assemblies untouched) and statically verifies that `_DYNAMIC`
does NOT remain an undefined import in the produced PRX.

Unchanged: the 6 allowed weak unresolved names (`__dladdr`, `__dlclose`,
`__dlerror`, `__dlopen`, `__dlsym`, `kernel_mprotect`) and the boundary
symbols (`__init_array_*`, `__fini_array_*`, `__dso_handle`, `__eh_frame*`).

## 3. Degraded dl retention

The SCE TU of the composed runtime was compiled with `-DLMF_DISABLE_DL_RETENTION`
(checkpoint wording: the target run refuted RTLD_NOLOAD self-retention and the dl
entry points resolve only to the bundled crt1 rtld, not exported to game-module
links). Consequences for the PRX candidate:

- The runtime cannot retain dl handles itself; a supplier that cannot retain
  yields zero facts; no reported address is dereferenced by the adapter.
- The in-game probe (worker-ingame-probe) decides whether a handle bridge exists
  before the route is re-enabled. Risk if this is ignored: silent zero-fact runs
  that look like success. The candidate therefore MUST NOT be promoted on
  "no crash" evidence alone.

## 4. Recipe deltas (all recorded as patches in build-receipt.json)

1. Import contract extended 191 -> 196 strong (5 provider-cloned FUNC rows with
   evidence links; provenance chain `6bfa4272... -> 58d94bc6... -> new sha`
   preserved in `supplier_extension`).
2. Packager harness copy: pinned values adjusted (contract sha, counts 191->196,
   FUNC 187->192, `_DYNAMIC` added to boundary set). No logic changes.
3. SharpProspero copy: one added line `linkerDefined["_DYNAMIC"] = dynamicAddr;`
   in `DynamicWriter.cs` (mirrors existing PayloadWriter behavior).
4. Typed-import shim `supplier_import_extension.S` (4-byte-safe `.quad` refs)
   because the composed object references the five new imports untyped and the
   pinned Packager requires one typed reference per contract import.

Everything else follows the frozen recipe exactly: same bootstrap units, flags,
include paths and unit order; `ld.lld -r -m elf_x86_64` bootstrap-first;
explicit `module_start`/`module_stop` exports with `force_align_arg_pointer`;
DT_INIT/DT_FINI explicit, DT_INIT_ARRAY/DT_FINI_ARRAY tags zero (module owns
array walking); SDK stamp 0x2000009 -> 0x11000000 via the console-proven
`finalize_sdk.py`; fake-self wrap via the pinned `make_fself.py`; static checks
ported from the frozen `check_artifact.py` plus import-table and entry-alignment
evidence.

## 5. Honest status of the produced candidate

- Lineage verification: PASSED (all checks, `verification-evidence/lineage-verification.json`).
- Bootstrap compile + partial link + unresolved pre-check: PASSED with exactly
  the five measured additions and nothing else.
- Contract derivation + harness patching: executed.
- Full link: NOT completed within this worker's call budget — the last executed
  run stopped at the local SharpProspero rebuild because the copied csprojs omit
  `TargetFramework` (they inherited it from their original parent build context).
  The final script injects `<TargetFramework>net10.0</TargetFramework>` into the
  copied csprojs; that exact step was not re-executed here. The controller runs
  `python3 package_prx.py --link` to produce and statically verify the candidate.
  No in-game claim is made or implied.
