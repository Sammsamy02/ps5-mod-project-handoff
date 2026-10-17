# Classic v11 / None PAK byte preparation

`pak_writer.h` exposes `mb_pak_write_v11_none`: a trusted versioned mount/path-hash-seed profile, immutable virtual paths and payload spans, tightening limits, typed caller scratch and output capacity produce one complete owning archive result. The implementation has no heap, filesystem, network, engine or retained-pointer interface. No title or mod identifiers are compiled into it.

This slice admits printable ASCII relative slash paths and classic v11 unencrypted None entries only. It rejects unsafe names, casefold duplicates, file/ancestor conflicts, case-only directory aliases and path-hash collisions. Mount is bounded trusted metadata; it grants no filesystem authority. Ceilings are 256 files, 2048 generated directories, 1024 bytes per path/mount, 64 KiB aggregate file-name bytes, 4 MiB per payload and 16 MiB complete output. The 64 Mi weighted work ceiling measures admitted validation/comparison/byte work, not elapsed CPU time. Limits only tighten.

ABI/backing/alignment and known-alias preflight leave every output untouched. After admission, operational failure clears owning positive facts and scratch. Validation/capacity/limit refusal leaves archive bytes untouched. A capacity query returns `BUFFER_TOO_SMALL` and required size only after validated geometry. Only `OK` publishes archive length; a backend failure after writes clears the proposed span. Caller backing must remain immutable and disjoint from output/scratch/result throughout the synchronous call.

Serialization follows retained repak commit `355b5f62f51959c7cc6dd5a51708646ef483065d`: 53-byte physical None headers, 12-byte compact records, LE32 primary presence flags, FNV64 lowercase ASCII UTF-16LE with the supplied seed, and a 221-byte footer. Mbed3.6.7 uses the unchanged canonical reader's SHA1-only config. Both secondary spans contain complete directory maps, including generated empty parents; an independent fixture parser checks trailing-map exhaustion and references. Pinned repak normally writes an empty pruned map, so this choice is not evidence of retail-engine enumeration or mounting. Stored SHA1 is corruption checking, not authenticity.

Focused host command (fresh output required):

```sh
python3 -B outputs/ps5-mod-bridge/native-host/format/pak_writer_tests/verify.py \
  --mbedtls-source .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5 \
  --zlib-source .superpowers/native-dependency-spike/native-zlib-codec-experiment-1/upstream/zlib-1.3.2 \
  --output-dir YOUR_FRESH_IGNORED_OUTPUT
```

The strict C11 host closure runs 270 behavior checks, then independently parses three synthetic archives, lists/extracts eight payloads using the retained pinned Mac repak, and validates all metadata/selected payloads with the canonical candidate. One changed stored payload must fail its SHA1 with no published payload. Empty inventory, zero-byte payload, deterministic permutations, late input rejection, alias/alignment, exact/short output, size/work ceilings and scratch/output guards are included. Tests have host fixture I/O; the writer does not.

Target mode replaces Zlib/reader tests with one C caller and the writer/SHA1/platform-util closure:

```sh
python3 -B outputs/ps5-mod-bridge/native-host/format/pak_writer_tests/verify.py \
  --mbedtls-source .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5 \
  --output-dir YOUR_FRESH_IGNORED_NATIVE_OUTPUT \
  --ps5-sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk \
  --llvm-config outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config \
  --resource-dir .superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21
```

This checks pinned tools, actually used resource bytes, dependencies, FreeBSD x86_64 ET_DYN/BIND_NOW and imports. It never executes the target. Writer object undefined symbols are SHA1 and memory functions; the complete upstream platform-util closure also imports `free`. No entry-point/runtime integration is proved.

Retained development evidence lives in `.superpowers/native-observations/native-pak-writer-2026-10-01/`: compiled missing-writer RED, host `host-2` GREEN, native `native-2` link-only, and a private profile-driven roundtrip of three already captured original payloads. Raw original assets/profiles are not product inputs or fixtures. This primitive does not translate PC cooked assets, prepare files above its caps, install an archive, implement a runtime loader or prove game support.
