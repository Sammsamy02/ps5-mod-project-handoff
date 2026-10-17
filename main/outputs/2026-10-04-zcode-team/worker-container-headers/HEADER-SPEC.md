# .utoc / .ucas header specification — provable-from-retained-sources (worker-container-headers, 2026-10-04)

Scope: the IoStore TOC (`.utoc`) header and its trailing array region, plus what is and is not
provable about `.ucas`. Every field below is cited to a file retained in this workspace. Fields we
cannot prove are marked **unknown** — nothing here is guessed.

## Provenance key (all paths relative to workspace root)

- **S1** `.superpowers/native-dependency-spike/unreal-library-container-survey-1/parser/source/CUE4Parse/CUE4Parse/UE4/IO/Objects/FIoStoreTocHeader.cs` — pinned CUE4Parse source subset: `FIoStoreTocHeader` field order, `SIZE = 144`, `TOC_MAGIC` constant, `EIoStoreTocVersion`, `EIoContainerFlags`.
- **S2** same tree, `.../UE4/IO/Objects/FIoStoreTocResource.cs` — array-region order and version gates.
- **S3** same tree: `Objects/FIoChunkId.cs`, `Objects/FIoOffsetAndLength.cs`, `Objects/FIoStoreTocCompressedBlockEntry.cs`, `Objects/FIoStoreTocEntryMeta.cs`, `Objects/FIoDirectoryIndexEntry.cs`, `Objects/FIoFileIndexEntry.cs`, `IO/FIoStoreEncryptionIV.cs`.
- **S4** `.superpowers/native-dependency-spike/retoc-v0.1.5-raw-roundtrip-1/repack_runner.py` — pinned retoc corpus runner: magic check `data[:16] == b'-==--==--==--=='`, `version = data[16]`, `container_id = data[56:64]`.
- **S5** `.superpowers/native-observations/townfall-manifest-capture-2026-10-01/container-header-receipt.json` — original-console bytes of `global.utoc`: prefix hex `2d3d3d2d`×4 then `08000000`, `iostore_version: 8`, `declared_header_size: 144`.
- **S6** `.superpowers/native-observations/townfall-original-zen-headers-1/profile.json` — original-console TOC facts for `pakchunk0-ps5.utoc`: `header_bytes: 144`, `chunk_offset_lengths_offset: 713880`, `compression_blocks_offset: 1427616`, `partition_count: 1`.
- **S7** `outputs/2026-10-04-zcode-team/worker-6-containers/evidence/utoc-retoc-verify.json` — magic observed `0x2d3d3d2d`×4 on all saved utocs; retoc v0.1.5 `verify` exit 0 on Leon trio and `1_999_P` utocs.

## 0. The magic correction (user-directed, confirmed by sources)

`-==--==--==--==-` (LE u32 `0x2d3d3d2d` repeated 4×) is the **generic IoStore TOC magic** — the
single `TOC_MAGIC` constant of the format itself [S1]. It is not a Zen-only signature and not a
per-platform marker. Worker-6's earlier claim of a separate "desktop magic `0xCAECA5A9`" is
**retracted**: a workspace-wide search finds `0xCAECA5A9` only in worker-6's own outputs
(`DETECTION-NOTES.md`, `PROFILE-RULES.md`, `CAPABILITY-MATRIX.md`, `tests/utoc_probe.py`,
`REPORT.md`, `evidence/utoc-retoc-verify.json`) and in the checkpoint entry that corrects it
(`transport-experiments-progress.json` → `zcode_worker_6_containers.controller_correction_2026_10_04`).
**No retained source defines `0xCAECA5A9` as any container magic.** Handler selection therefore
cannot be magic-based at all beyond one-format recognition; it must key on the complete
header/version/structure parsed below.

## 1. FIoStoreTocHeader (utoc, bytes 0..144) — field order per S1, offsets computed from it

| Offset | Size | Field | Type / notes | Source |
|---|---|---|---|---|
| 0 | 16 | TocMagic | bytes `-==--==--==--==-` | S1 (const), S4 (runtime check), S5/S7 (observed) |
| 16 | 1 | Version | u8, `EIoStoreTocVersion`: Invalid=0, Initial=1, DirectoryIndex=2, PartitionSize=3, PerfectHash=4, PerfectHashWithOverflow=5, OnDemandMetaData=6, RemovedOnDemandMetaData=7, ReplaceIoChunkHashWithIoHash=8, AddedSourceHashes=9, ContainerEncryptionMethod=10, Latest=10 | S1; observed value 8 on `global.utoc` in S5, S4 reads `data[16]` |
| 17 | 1 | reserved0 | u8, unread | S1 |
| 18 | 2 | reserved1 | u16, unread | S1 |
| 20 | 4 | TocHeaderSize | u32; `FIoStoreTocHeader.SIZE = 144` is the sanity value | S1; observed 144 in S5, S6 |
| 24 | 4 | TocEntryCount | u32 (chunk entries) | S1 |
| 28 | 4 | TocCompressedBlockEntryCount | u32 | S1 |
| 32 | 4 | TocCompressedBlockEntrySize | u32 "For sanity checking"; entry size is 12 [S3] | S1 + S3 |
| 36 | 4 | CompressionMethodNameCount | u32 | S1 |
| 40 | 4 | CompressionMethodNameLength | u32 (fixed-name slot length) | S1 |
| 44 | 4 | CompressionBlockSize | u32 | S1 |
| 48 | 4 | DirectoryIndexSize | u32 | S1 |
| 52 | 4 | PartitionCount | u32; observed 1 on pakchunk0-ps5 | S1, S6 |
| 56 | 8 | ContainerId | u64 — size proven by S4 (`data[56:64]`) and by arithmetic closure to 144 (S1 SIZE) | S1 (field order), S4 (size) |
| 64 | 16 | EncryptionKeyGuid | FGuid 4×u32 = 16 B — size proven by arithmetic closure to `SIZE = 144`; struct definition file not in the pinned subset (**layout definition itself not retained**) | S1 order + closure |
| 80 | 1 | ContainerFlags | u8 flags: None=0, Compressed=1<<0, Encrypted=1<<1, Signed=1<<2, Indexed=1<<3, OnDemand=1<<4 | S1 |
| 81 | 1 | EncryptionMethod | u8 (only serialized as of version 10; for version < 10 derived: AES iff Encrypted flag, else None) | S1 field, S2 lines 76–80 derivation |
| 82 | 2 | reserved4 | u16, unread | S1 |
| 84 | 4 | TocChunkPerfectHashSeedsCount | u32 | S1 |
| 88 | 8 | PartitionSize | u64 | S1 |
| 96 | 4 | TocChunksWithoutPerfectHashCount | u32 | S1 |
| 100 | 4 | reserved7 / TocSourceHashCount | u32 — reserved (skip) for `Game < UE6_0`; TocSourceHashCount for UE6_0+ | S1 |
| 104 | 40 | CustomGameData u64[5] — OR — (UE6_0+) EncryptionIVCount u32 @104, reserved8 u32 @108, reserved9 u64[4] @112 | both readings close to 144 | S1 |

Both serializations total exactly 144 bytes [S1 `SIZE = 144`]. For the UE5-line files observed here
(version 8) the `< UE6_0` reading applies; the `Game` gate itself is a CUE4Parse version-map
concept — **which gate applies to a given version byte is not fully provable from the retained
subset**; the structural EOF check below is the empirical disambiguator.

**Encryption IV count note**: for Version < ContainerEncryptionMethod(10), S2 sets
`EncryptionMethod` from the Encrypted flag and `EncryptionIVCount = 0` — i.e. version-8 files carry
no IV array unless encrypted (none observed are).

## 2. Trailing array region (immediately after byte 144, order and gates per S2)

| # | Array | Element size | Count | Gate |
|---|---|---|---|---|
| 1 | ChunkIds (`FIoChunkId`) | 12 (u64 ChunkId + u16 chunkIndex(network order) + u8 padding + u8 ChunkType) | TocEntryCount | always |
| 2 | ChunkOffsetLengths (`FIoOffsetAndLength`) | 10 (40-bit BE offset ‖ 40-bit BE length, per accessor math) | TocEntryCount | always |
| 3 | ChunkPerfectHashSeeds | 4 (int32) | TocChunkPerfectHashSeedsCount | Version ≥ 4 (PerfectHash) |
| 4 | ChunkIndicesWithoutPerfectHash | 4 (int32) | TocChunksWithoutPerfectHashCount | Version ≥ 5 (PerfectHashWithOverflow) |
| 5 | CompressionBlocks (`FIoStoreTocCompressedBlockEntry`) | 12 (u64 Offset‖CompressedSize with 40/24 bit split; u32 UncompressedSize(24b)‖CompressionMethodIndex(8b)) | TocCompressedBlockEntryCount | always |
| 6 | EncryptionIVs (`FIoStoreEncryptionIV`) | 12 | EncryptionIVCount | AES_CTR only (S2 check) |
| 7 | Compression method names | CompressionMethodNameLength each, NUL-padded strings | CompressionMethodNameCount | always |
| 8 | Signatures | i32 hashSize; tocSignature[hashSize]; blockSignature[hashSize]; SHA1[20]×blockCount | — | ContainerFlags.Signed |
| 9 | Directory index buffer | blob | DirectoryIndexSize bytes | Version ≥ 2 AND Indexed flag AND DirectoryIndexSize > 0 |
| 10 | Chunk metas (`FIoStoreTocEntryMeta`) | version ≥ 8: 20 (SHA-1) + 1 flags + 3 pad = 24; version < 8: FIoChunkHash + 1 — **FIoChunkHash size is not retained → unknown** | TocEntryCount | optional (ReadTocMeta) |
| 11 | OnDemand metas | 2×SHA-1 regions (chunk + block) | TocEntryCount / blockCount | only Version == 6 AND OnDemand flag |

Structural identity check (used by the probe): for an unencrypted, unsigned, indexed version-8
TOC, `144 + 22·E + 4·S + 4·W + 12·B + nameBytes + DirectoryIndexSize + 24·E (+ on-demand if v6)`
must equal the file size. Cross-check anchors already in receipts: pakchunk0-ps5.utoc
`chunk_offset_lengths_offset = 713880` ⇒ `E = (713880 − 144)/12 = 59478`; blocks at `1427616`
[S6] — the probe recomputes these from bytes and compares.

Per-entry layouts inside the directory-index blob (S3): `FIoDirectoryIndexEntry` = 4×u32
(Name, FirstChildEntry, NextSiblingEntry, FirstFileEntry); `FIoFileIndexEntry` = 3×u32
(Name, NextFileEntry, UserData). The string table layout of that blob is **unknown from the
retained subset** (CUE4Parse keeps it opaque as a buffer) — only its size and presence are
provable here.

## 3. .ucas side — what is provable

- The `.ucas` is the chunk payload store; it carries **no TOC-magic header**. Its first content is
  the `ContainerHeader` chunk (EIoChunkType/5 `ContainerHeader = 6` [S3]) whose parse layout is
  **not retained in the pinned subset** (no FIoStoreContainerHeader source in tree) → **unknown**.
- Pairing is by same-stem sibling, enforced by tools (retoc `verify` panics with file-not-found
  when the `.ucas` sibling is absent — observed for all five main-game utocs and manifest
  `global.utoc` in S7).
- Original-console pairing/size facts: `townfall-original-zen-headers-1/profile.json` lists
  `/content/paks/pakchunk0-ps5.ucas` at 128240377200 bytes (not captured); the saved
  `townfall-global-scriptobjects-1/global.ucas` is 3412992 bytes (S7).

## 4. Explicitly unknown / not provable from retained sources

1. Any meaning of `0xCAECA5A9` (appears only in worker-6 outputs; no source).
2. FGuid field internals (size 16 proven by closure only); FIoContainerId struct internals.
3. FIoChunkHash size (affects version < 8 metas).
4. Directory-index string-table encoding inside the blob.
5. `.ucas` ContainerHeader chunk layout (no retained source).
6. Which CUE4Parse `Game` gate (UE5 vs UE6 trailing reading) applies per version byte; the EOF
   structural check is the empirical arbiter on real files.
7. Signature verification semantics (S2 skips verification; hashSize read only).
