# l22 SCO header ABI candidate

The copied selected header now gives `Above55::FStaticConstructObjectParameters` the required 16-byte alignment. The original actual header is rejected by the alignment contract (`8 == 16`); the candidate passes the target compiler with callback at `0x40`, serial at `0x70`, subobject overrides at `0x78`, size `0x80`, and alignment 16. These are compiler measurements of the real selected structure, not a recreated mock structure. No runtime call, runtime link, PRX build, console action, or network action was performed by this ABI task.

## Candidate and scope

`CHANGES.json` is the integration manifest, with absolute original/candidate paths and both SHA-256 hashes. Original header: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectGlobals.hpp`. Candidate: `candidate/Unreal/UObjectGlobals.hpp` in this directory.

The only header changes are explicit `<cstddef>`, `alignas(16)` on the existing `Above55` 0x30-byte callback placeholder, an empty-callback-only explanation, and five guarded ABI assertions. The assertions target `__PROSPERO__`, Shipping, remote-object-handle disabled, non-case-preserving names. Other version branches and optional remote fields retain their previous declarations. The zero-initialized placeholder remains limited to an empty callback; it does not implement construction, invocation, ownership, or destruction of a nonempty Unreal `TFunction`.

The required model is corroborated by the preceding local source comparison: `outputs/2026-10-08-l21-audit/mod-api/OFFICIAL-SCO-COMPARISON.md`, Epic 5.6.1 source pin `6978b63c8951e57d97048d8424a0bebd637dde1d`. This task does not generalize the five offsets to case-preserving, remote-enabled, editor, or other target configurations.

## Target compile verification

`run_probe.py` reuses the recorded `unreal-uobjectglobals` compile recipe from `outputs/2026-10-08-menu-complete-build/pass1/COMPOSITION-RECEIPT.json`, retaining its SDK compiler, include graph, macros, and LLVM_CONFIG. It replaces only source/output/dependency destinations and compile mode for a no-link test of the actual header. The compiler target is `x86_64-sie-ps5`; it runs locally on the host. The test source is `tests/sco_layout.cpp`.

- `tests/red-original/receipt.json`: expected rejection, compiler exit 1. The original header passes the field-offset and size assertions and fails the required alignment assertion because its alignment is 8.
- `tests/green-target/receipt.json`: compiler exit 0 with the candidate selected by an include-directory override. All five ABI checks pass; LLVM IR records an allocation with alignment 16.
- `tests/green-overlay-target/receipt.json`: compiler exit 0 with the canonical original path mapped to the candidate through `probe-overlay.json`. All five checks pass. `tests/green-overlay-target/sco_layout.ll:185` records the actual structure allocation with `align 16`.

All three compiles emit the same two pre-existing `-Wswitch` warnings in `Core/Templates/Atomic.hpp` (lines 241 and 262); these concern unhandled enum values and are outside this patch. Compiler receipts retain the full commands and diagnostics. The tests do not demonstrate a successful engine call or native callback execution.

Reproduce from the worktree root:

```sh
python3 outputs/2026-10-08-l22-runtime/abi/run_probe.py --name red-original --expect-failure
python3 outputs/2026-10-08-l22-runtime/abi/run_probe.py --name green-target --candidate --emit-ir
python3 outputs/2026-10-08-l22-runtime/abi/run_probe.py --name green-overlay-target --overlay --emit-ir
```

## Dependency invalidation and overlay review

All 374 selected translation-unit dependency receipts exist and match their recorded SHA-256 hashes. Exact dependency membership identifies **166 affected translation units** and 208 unaffected units. `DEPENDENTS.json` and `DEPENDENTS.tsv` list every affected selected source, object, and dependency receipt; `ALL-DEPENDENCY-CHECKS.json` records all 374 checks. This list includes UE wrappers, Lua bindings, mods, and runtime consumers as well as `UObjectGlobals.cpp`.

`HEADER-ALIASES.json` independently examines every dependency token named `UObjectGlobals.hpp`. It finds one distinct path in this selected graph: the canonical coherent-runtime header, shared by all 166 consumers, with the selected original hash. There are no additional selected-header aliases to map for this patch.

The parent's proposed VFS strategy is appropriate provided it recompiles all 166 consumers, unions this set with the dependents of the other changed headers, and uses the same mapping for both fresh affected-compilation passes. Keeping an old object among these 166 could mix the old alignment-8 structure contract with the corrected alignment-16 contract, even where field offsets happen to agree.

The overlay test uses `use-external-names: false`; its generated `.d` therefore names the original virtual header path although candidate bytes were compiled. Integration receipts must pin the overlay file and candidate content hash, not infer compiled contents from the canonical dependency path alone. Retained unaffected objects remain defensible only under the complete changed-header dependency union and pinned original composition. This is a review of the described strategy plus an actual header-overlay compiler check; this agent did not execute or certify the parent's full incremental runtime build.
