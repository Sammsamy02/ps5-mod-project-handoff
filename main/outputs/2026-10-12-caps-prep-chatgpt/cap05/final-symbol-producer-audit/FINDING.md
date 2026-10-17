# Final symbol producer seam — Mac-only source stage

The missing final-symbol source is a **producer gap inside the actual final writer**, not a missing retained unstripped ELF that a conventional final linker emitted and later discarded. The existing normal build runs `ld.lld -r` to form combined-supplier.o, then a generated C# Packager invokes `DynamicWriter.Write` to determine final ELF addresses. That writer emits dynamic imports plus the two requested entry/exit exports, but no complete final .symtab or final-layout function map.

Source-backed flow in the current retained files:

| Edge | Source and lines |
| --- | --- |
| Partial linker only | package-A/package_prx_l19.py:837 runs ld.lld -r on bootstrap objects plus composed runtime. Its map/object offsets are not final layout. |
| Actual final-link invocation | package_prx_l19.py:879–889 builds/runs harness/bin/Release/net10.0/Packager.dll with combined-supplier.o, supplier-contract.json and bare-original/modbridge_supplier_composition.prx. |
| Final writer and artifact lifetime | pass-1/harness/Program.cs:96–108 passes the resolved graph to DynamicWriter.Write, checks the original object/contract stayed unchanged, then writes the returned bytes to the retained bare-original PRX. |
| Real address resolver | DynamicWriter.cs:997–1015 maps retained sections into final text/RO/RW/RELRO/TLS addresses. SymbolValue at1452–1475 resolves final values; defined retained symbols use final SectionAddr+symbol.Value. Relocations use this same resolver. |
| Final output | DynamicWriter.cs:1212 returns WriteFile's bytes. WriteFile emits .dynsym at1781 and returns the final byte array at1958. There is no final internal-function map output. |
| Later transformations | package_prx_l19.py:905–911 derives bare-sdk from bare-original, then wraps it into the packaged PRX. Both bare versions and packaged artifact remain in pass directories. No extra conventional final-link command is present in this traced route. |
| Current sidecar gap | l26_packaging_driver.py:482–495 extracts dynsym anchors and handles -r map reconstruction separately. This audit does not promote reconstructed offsets. |

`source-receipt.json` pins the actual files and lists artifact names/sizes in package-A pass1/pass2 and l26-A only. It found the retained bare-original/bare-sdk/packaged PRXs and runtime.maps; no final .elf/final-layout map/symtab-named artifact. The previously verified bare PRX lacks .symtab; that is historical metadata evidence, not a new binary scan. No map/object,39MB sidecar or PRX content was reread.

## Concrete staged producer

`stage.py` creates only this folder's `staged/DynamicWriter.cs` and `staged/Program.cs`. It refuses any source-hash drift or nonunique anchor. Seven exact replacements preserve the original files byte-for-byte; inverse transformation is verified.

The optional writer sink runs only after successful final WriteFile materialization and uses the same SymbolValue resolver as relocation. It emits named, defined, retained, allocated executable FUNC symbols with nonzero sizes contained in their own sections; overflow refuses. Undefined/discarded/absolute/zero-size/linker-generated functions are excluded and aliases remain. This bounded coverage is explicit, not a claim of every executable byte.

The staged harness collects those actual final ranges, persists the exact final ELF bytes, then writes a separate `bare-original/<PRX>.final-symbols.json` producer receipt containing the returned ELF SHA256/length, object/contract SHA256, executing Packager and Link writer assembly SHA256, address convention and ranges. Existing producer output refuses overwrite. The receipt explicitly remains UNBOUND to the packaged PRX. No current symbol artifact was generated.

Normal-build integration recipe for the owner, later:

1. Integrate/rebase the hash-pinned stage after package_prx_l19.patch_packager has generated Program.cs and applied its existing DynamicWriter patch, before its ordinary tool/harness builds. The current stage pins retained generated source; a different future B7 generated source must refuse until intentionally rebased. Do not add a separate build or make current B7 depend on this.
2. Let the existing normal Packager invocation produce bare-original and its real final-symbol producer receipt. Pin staged-source hashes to the actual normal build records and assembly hashes. Compilation/behavior are still unverified here.
3. After normal SDK-finalize/wrap, validate the actual bare-original→bare-sdk→packaged relation, including executable PT_LOAD bytes and VMAs through the transformations; record all three SHA256 values and transformation/source identities. Establish the module-relative address convention rather than assuming it.
4. Only then extend the existing sidecar emitter to consume the exact bounded producer rows, bind its existing prx_sha256 key to the exact packaged PRX, retain bare/source provenance and validate every range against executable layout. This package-binding adapter is **not implemented by this source-only stage**. Existing dynamic-anchor sidecar behavior remains untouched.

The exact remaining interfaces are therefore the normal-build source-stage application and final packaged-layout validator/sidecar admission. The source producer seam is concrete; final-range correctness, transformation relation and capture/build association remain unproved. No runtime/capture proof or shipping values follow from lexical source checks.

Replay only the source stage and guards:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap05/final-symbol-producer-audit/tmp" python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap05/final-symbol-producer-audit -p test_stage.py -v
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap05/final-symbol-producer-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap05/final-symbol-producer-audit/stage.py
```

Fresh checks:6 source transformation/refusal tests PASS;2 pinned inputs,7 unique replacements,2 exact inverse/source-preservation checks PASS. TDD-RED.log records the missing-stage failure before implementation; TDD-GREEN.log records passing guards. No C# compiler/build/execution, console, network, SDK/platform-extension research, original/script/runtime mutations or generated real symbol map. All writes are in this folder.
