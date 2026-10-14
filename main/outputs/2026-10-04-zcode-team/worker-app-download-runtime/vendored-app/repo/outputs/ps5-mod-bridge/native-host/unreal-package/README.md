# Bounded Zen package metadata readers

`summary.h` exposes one pure C operation, `mb_zen_read_summary_names_v1`. It consumes a caller-supplied contiguous prefix of an already decoded package chunk. It does not open IoStore files, assemble blocks, decompress data, or retain pointers. The caller provides the source/layout profile and owning name-record/byte storage.

## Supported profile

The only v1 layout is an explicitly selected UE5.3+ Zen summary. The caller must also state whether the UE object version has `VERSE_CELLS`; this controls the optional pair of cell offsets after the summary. Those are trusted external engine-layout facts, not detected from the package and not title/mod identity. An unknown layout returns Unsupported; a null profile is an invalid argument. The package's `bHasVersioningInfo` must be zero; a nonzero field requires the separate variable versioning/custom-version parser and is Unsupported here.

The parser returns the seven raw signed summary offsets and optional cell offsets after checking they are nonnegative and no greater than `HeaderSize`. It does not parse the referenced hashes/import/export/dependency tables or verify their inner interval geometry. `CookedHeaderSize` remains an independent raw scalar and is not constrained by the header or logical package length.

The batched local name map is bounded to 4,096 records and 262,144 encoded string bytes. Name headers are two bytes; bit 7 of the first selects UTF-16LE, with the lower 15 bits giving byte length for UTF-8 or code-unit count for UTF-16LE. As in the pinned `LoadNameBatch(FArchive)` source path, all name headers precede all name bytes and there is no per-name alignment. The result preserves each 64-bit hash, the declared aggregate string-byte field, and hash-version value as unverified metadata. It copies UTF-8 or UTF-16LE bytes verbatim and performs no Unicode validation, normalization, hash recomputation, or transcoding. Only mapped-name type `Package` resolves into this local table; `Container` and `Global` remain unresolved and type 3 is Unsupported.

Hard ceilings are 256 KiB captured header, 4,096 names, and 256 KiB name bytes; per-call limits may only tighten these. `HeaderSize` must fit the captured bytes and caller-supplied logical package length. The logical package length itself has no reader cap. Caller initializes result `struct_size` and `abi_version`; input backing and output spans must be valid and disjoint. ABI/pointer/known-alias preflight failure leaves outputs untouched. After preflight, outputs are cleared; any later failure publishes status/reason only, never a completed prefix.

## Evidence and use

The exact CUE4Parse schema files at commit `28f385e622468c4ab84997d596b46bb1c3adfcfa` and their hashes are locked in `schema-lock.json`. The focused host verifier runs synthetic malformed/bounds/alias controls, then reads two exact method-0 slices from the retained local Leon UCAS and compares them with `tests/actual_header_profile.json`. The actual profile is fixture data only; no game or mod identifiers appear in the parser.

```sh
python3 -B outputs/ps5-mod-bridge/native-host/unreal-package/verify.py \
  --output-dir .superpowers/native-dependency-spike/zen-summary-host-run-1
```

The optional pinned PS5 path compiles and links the same parser and a small caller, then inspects the ELF. It never executes the target.

```sh
python3 -B outputs/ps5-mod-bridge/native-host/unreal-package/verify.py \
  --output-dir .superpowers/native-dependency-spike/zen-summary-native-link-1 \
  --ps5-sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk \
  --llvm-config outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config \
  --resource-dir .superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21
```

Success establishes only that this bounded summary/name-map reader parsed the supplied explicit profile and slices. It does not discover an engine version, establish package authenticity/platform compatibility, parse serialized objects, translate assets, recook, or grant runtime/install support.

## Raw import-map inspection

`import_map.h` exposes `mb_zen_read_import_map_v1`. It consumes the same immutable header view and a successful summary result, independently rebinds the summary's size and hash/import/export offsets to the raw bytes, and copies each eight-byte object-index record into caller-owned storage. Package-import public-export-hash indices must fit the preceding hash table. Script references and imported-package indices remain unresolved external identities. The exact all-ones null sentinel is recorded separately from the top two kind bits.

The operation allocates nothing and performs no I/O. It caps references at 8,192 independently of the header byte ceiling, accepts only aligned eight-byte table spans, rejects known input/output aliases before writing, and clears all supplied output records after an admitted failure. No partial map or platform/install authority is published.

Retained capture facts and comparison pairs are stored in `tests/import_map_actual_header_profile.json`; `--actual-profile` selects another fixture profile. The shared reader contains no game or mod paths. The focused verifier passed 32 checks and read four retained headers. Matching raw words alone cannot establish dependency closure or asset compatibility.

```sh
python3 -B outputs/ps5-mod-bridge/native-host/unreal-package/import_map_verify.py \
  --output-dir .superpowers/native-observations/unreal-package-import-map-host-new-run
```

## Imported-package names and references

`package_imports.h` exposes `mb_zen_read_package_imports_v1`. It joins raw package-import indices with the imported name batch and requested public-export hashes in the same captured header. It copies hashes, encoded names, signed name numbers, and bindings into caller-owned storage. It preserves UTF-8/UTF-16LE bytes and opaque hashes; it does not manufacture engine or package-store identities.

The reader rebinds all seven summary offsets and import words against the immutable header, checks hash/import/name section ordering and exact tail exhaustion, and rejects known aliases before writing. Its ceilings are 256 KiB header/name bytes, 4,096 imported names and 8,192 bindings. Unknown or larger layouts require another admitted profile or reader revision. These limits describe current implementation coverage, not mod compatibility.

The focused host suite includes 100 original checks, 76 shared-cap checks and five independent retained-header comparisons. The large retained capture declares a 120,108-byte header with 2,903 local names using 90,712 actual encoded bytes; those are fixture facts, not compatibility rules. A separate corpus check matched all 367 imported names and 378 package bindings in 127 unchanged mod packages. The pinned PS5 compile/link passed; this parser's probe has not been executed on the console. Neither result verifies serialized assets or in-game loading.

Capture facts live in `tests/package_imports_actual_header_profile.json`; another profile can be supplied with `--actual-profile`. Choose a new evidence directory for each run; existing receipts are never overwritten.

```sh
python3 -B outputs/ps5-mod-bridge/native-host/unreal-package/package_imports_verify.py \
  --output-dir .superpowers/native-observations/package-imports-host-new-run
```
