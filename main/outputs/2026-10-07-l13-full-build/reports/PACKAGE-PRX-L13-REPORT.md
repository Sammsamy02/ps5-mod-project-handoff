# Isolated L13 package script

Owned implementation: `outputs/2026-10-07-l13-full-build/package_prx_l13.py`.

Final SHA-256: `1f12f25949a324c3351aea73d2b1fbe37465331eca429e3f387debd4072daa16`.

Parent: `outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py`, SHA-256 `921d8397957df6fe39e19ca010fdcb6ba469ba0c6e80ef458ea41ad49f08be64`. Parent was read and hash-verified, never edited. No source-unit, runner, driver, composer, manifest, original lineage or console changes were made by this worker.

## Changes

- Preserve original `EXTENDED_IMPORTS` and its pre-bootstrap runtime requirements. Define explicit `BOOTSTRAP_ONLY_IMPORTS` for `_openat` and `sceKernelRead`, with full combined sets for final contract, typed shim, packager cardinality and static import census.
- Add pinned verification of candidate `receipts/l13-import-extension.json`, its L12 base contract and the exact SDK libkernel stub. Compare both identity rows to the pinned donor's complete module/library/version/Soname/published identity. Invoke SDK llvm-nm and match exact exported names.
- Require bootstrap-only symbols absent from the measured pre-bootstrap runtime symbol table. Keep all original lineage checks, including the original runtime extension requirements.
- Require the actual packaged `runtime_start_bridge.o` to be tied to its source/object SHA and expected candidate paths, and to contain both new imports as strong UND. Write `verification-evidence/bootstrap-only-imports.json`.
- Require full combined unresolved-set equality and full final decoded-NID contract equality through the existing checks. The full contract contains 200 strong imports; the two added identities are libkernel imports.
- Preserve the strict C# typed-owned-reference check. Move the existing `supplier_import_extension.o` immediately before `runtime_start_bridge.o` in the partial link, preserving all frozen bootstrap units' relative order. Record partial-link order in packaging evidence.
- Derive extension-provider labels from each pinned donor Soname; `scePadGetHandle` now reports `libScePad` accurately.

## Build evidence requirements

The fixed composer must produce the ordinary `sdknm-libkernel.so.stdout`, `sdknm-libSceLibcInternal.so.stdout` and `sdknm-libScePad.so.stdout` evidence files in its run directory. No extra evidence-file format is needed. New imports use the existing libkernel nm record.

The candidate evidence receipt must retain SHA `7aebba0bec9b7fa60471e49f83a2013157504ecad3ebc653e2afd369d604ccd7`. It pins L12 supplier contract SHA `649b90ce5200203a61f64234dfaef62f1d291a7a619b5eddfc01d069d7802ccc` and SDK libkernel SHA `c6579f2614d8b8f905a673f810a1b562f1432b07397d61c11e876b5648428753`.

The driver must stage the bridge at `PIN['lineage']/lifecycle/runtime_start_bridge.cpp`, compile it to the candidate's `build/runtime_start_bridge.o`, and include both new headers in the overlay. The existing bootstrap proof consumes those exact records after compilation. The script supports the root-owned in-memory driver PIN/bootstrap setup; running it directly against its old defaults is not an L13 full pipeline.

## Focused verification

`receipts/package-prx-l13-focused-checks.json` records ten checks of the initial isolated version, SHA `5af924610e4df838591daf5fe7058790bd03e402b72f23879d514a8998190122`:

- Old script derived 198 strong imports and omitted both new names, the expected red case.
- New full contract adds exactly `_openat` and `sceKernelRead`, removes nothing, and preserves all 198 L12 import identities.
- The old runtime-only gates and new SDK/identity/absent-runtime checks pass against the real L12 runtime; only the pre-existing raw-checkpoint mismatch remains for the driver's stage-aware reconciliation.
- Real candidate bridge object proves both strong UND references.
- Missing bridge, object-hash drift, old L12 bridge, wrong SDK hash and wrong donor identity all refuse.
- Typed shim emits both references; original package file remains byte-identical.

Root's first full attempt exposed the typed-reference order issue. The new calls were NOTYPE in the bridge and remained NOTYPE in combined-supplier.o because ld.lld retained the first declaration's kind. Existing extensions had worked because their runtime references followed the typed shim. Contract `compiled_requirements` metadata was not the cause: the strict harness checks actual ELF symbol types directly.

`receipts/package-prx-l13-typed-reference-check.json` records the final fix against the first attempt's real object inputs, writing only temporary test outputs:

- Before: strict harness exit 2, `Missing exact typed owned import reference: _openat`; both new combined symbols NOTYPE.
- After reordering only the shim: both symbols FUNC UND.
- The already-built, unchanged strict Packager.dll succeeds with 200 imports, 196 functions, four objects and exactly the six allowed weak imports.
- Temporary test artifacts were removed. The full fresh twin build remains root-owned.

The focused checks prove local script behavior and artifact compatibility. They do not claim console provider resolution or runtime behavior. No console contact occurred.

## Read-only runner/driver review after final package pin

No blocking omission was found in the generated attempt-2 runner/driver: outputs and manifest finalization are isolated; full passes supply exact isolated run paths; new headers are replace-receipt covered and hash-staged; new bridge symbols are declared in composer/driver provider admission and checked after packaging; receipt/manifest/driver-log copies are retained for each pass; the package pin is the final hash above.

Remaining bookkeeping observations were sent to the build owner: new headers are attested through edition, replace receipt and overlay manifest, while the copied change-manifest's `changed_headers_staged` list still contains only inherited headers. The shared candidate-owned overlay is moved aside between passes, so its preserved pass-1 path should be identified in final evidence. The carried SeedValidation staging path has the same bytes and pin as the candidate header. The driver's old default RUN6 is not used for full passes by this runner, which supplies explicit isolated paths.
