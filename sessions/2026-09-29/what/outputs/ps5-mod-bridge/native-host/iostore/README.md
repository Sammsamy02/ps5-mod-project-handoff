# Bounded IoStore v8 metadata reader

`toc_reader.h` provides three pure C operations: `mb_iostore_read_header_v1`, `mb_iostore_read_chunk_v1`, and `mb_iostore_read_block_v1`. Caller-supplied immutable header/selected row spans carry absolute TOC offsets. Results own scalar metadata/raw ID bytes; no pointers persist. The implementation performs no filesystem/network access, allocation, table scan/copy, payload decoding, runtime loading or target selection.

The supported inspection profile is a 144-byte v8 header, 12-byte chunk IDs, 10-byte big-endian 40-bit offset/length pairs, 12-byte little-endian 40/24/24-bit compression block rows and 24-byte entry metadata. Seed and overflow arrays precede the block table. None/Compressed/Indexed flags are admitted; encrypted, signed, on-demand and unknown layouts are explicitly unsupported. Method index 0 is built-in None; another in-range index is declared metadata without method-name or decoder validation. Reserved v8 bytes are uninterpreted; raw IDs/flags/GUID never establish platform/build or signature policy.

Default ceilings only tighten: 1,048,576 chunks/seeds/overflow rows, 4,194,304 blocks, 16 partitions/method slots, 64-byte method-name slots and 256 MiB logical TOC. An operation examines a bounded header and one selected row/pair, not all declared tables. Paired UCAS lengths and chunk descriptor sizes have no payload-buffer cap. Length observations and coherent captured backing are caller assertions; this API does not discover or read files. It checks a selected block's stored extent, without validating runtime padding reads, contents or UCAS identity.

Header success means declared table intervals fit the declared TOC length. Chunk success means its logical span fits block address capacity; the operation does not verify every involved block or holes. Block success means that descriptor's stored extent fits its partition and the caller-provided partition length. Directory/meta/hash-seed contents and any TOC trailer remain unparsed. Full TOC/logical coverage/payload/authenticity/runtime/install flags stay zero. No cooked asset conversion, engine mounting or PC-mod acceptance is implied.

All inputs need valid immutable C backing. Input/control spans must be disjoint from the owning result. ABI/backing/alignment and known-alias preflight errors leave output untouched. After admission, failure clears every positive fact and writes only ABI/status/reason. The selected API re-parses the header and requires exact selected offset/width. Raw chunk type/index/padding remain metadata, not title-specific semantics.

Exact primary sources and hashes are retained in `schema-lock.json`: CUE4Parse `28f385e622468c4ab84997d596b46bb1c3adfcfa` header/resource/offset-length/block/chunk-ID/meta definitions establish byte geometry. Retoc writers at `885a8dae740cb1ce1e41ff2e74f67f9f0c118237` and release `d034ade1ae8117d4786eaf6b0418d4cf48474d7f` establish None construction/logical block addressing. Retoc TOC type definitions were not locally retained and are not claimed read. No third-party source or raw captured asset is copied into this module.

Focused host verification uses an external metadata fixture profile (up to eight rows with path, bytes, SHA256, paired_length and selected_chunk):

```sh
python3 -B outputs/ps5-mod-bridge/native-host/iostore/verify.py \
  --fixture-profile YOUR_RETAINED_METADATA_PROFILE.json \
  --output-dir YOUR_FRESH_IGNORED_OUTPUT
```

The strict C11 caller covers malformed/truncated geometry, mixed endianness, seed/overflow displacement, selected binding, tightened caps, partition boundaries, None-size/method errors, aliases/ABI/alignment and failure clearing. It inspects all rows of small captured TOCs and three sampled rows plus the separately requested chunk of larger TOCs. All actual geometry is cross-checked by an independent offline scalar oracle. Only local captured TOC metadata is read; no UCAS payload is read. Names and target lengths arrive through fixture data, with no title/mod branches.

Target compile/link only:

```sh
python3 -B outputs/ps5-mod-bridge/native-host/iostore/verify.py \
  --output-dir YOUR_FRESH_IGNORED_NATIVE_OUTPUT \
  --ps5-sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk \
  --llvm-config outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config \
  --resource-dir .superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21
```

This verifies pinned tools and actually used resource bytes, retains dependency/ELF/import evidence and never executes the target. It does not wire the module into a native service, installer or game runtime. Makefile integration belongs to the root task.
