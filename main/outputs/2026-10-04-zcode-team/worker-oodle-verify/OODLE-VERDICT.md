# OODLE-VERDICT

## VERDICT: PASS — no retained game container requires an Oodle decoder.

Every retained container was checked at the byte level. Zero compression of any
kind (Oodle, Zlib, or custom) is registered or exercised anywhere in the corpus.
The compression-method gap in the container track's list can be closed.

---

## Evidence

### 1. PAK containers (v11 footer, 5x32B compression-method name slots)

| Container | Version | Method slots (5x32B) |
|---|---|---|
| townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak (297,572,462 B) | 11 | all zero |
| leon-real-mod-intake extracted zzz_Leon_Townfall_2K_P.pak | 11 | all zero |
| leon-real-mod-intake base-pak-metadata townfall-pakchunk0-metadata-only.pak | 11 | all zero |
| leon-iostore-adapter-4k-trial-1 source zzz_Leon_Townfall_4K_P.pak | 11 | all zero |
| townfall-62-rar-intake-1 bundle-input 1_999_P.pak | 11 | all zero |
| controller-bundle-adapter-1 trial-1 bundle 1_999_P.pak | 11 | all zero |
| townfall-extra-pak-test ModBridgeOriginals_P.pak (ours) | 11 | all zero |
| townfall-extra-pak-test ModBridgeRenderDiag_P.pak (ours) | 11 | all zero |

Parser validation (both directions):
- Positive control repak `pack_v11_compress.pak`: slot[0] = `Zlib` (nonzero detected).
- Negative control repak `pack_v11.pak`: all zero.

An empty method table makes any nonzero compression method index unreadable;
no entry can reference a method the footer does not name. Oodle is impossible in
these paks by construction.

Independent size check on the main pak index (4699 entries, 12 B records, pinned
layout from worker-pak-index): sum(uncompressed sizes) = 297,001,179 B vs payload
span 297,250,173 B (99.92 %; remainder is per-file alignment). Compressed data
would make the stored span far smaller than the size sum. Payload is stored raw.

### 2. UTOC containers (compression method name table + per-block method byte)

True 12-byte block-entry layout (decoded this session, replacing the prior
workers' unverified assumption):
`[u32 block_offset][u32 (on_disk_size << 8 | method_byte)][u32 uncompressed_size]`

| Container | Method name table | Blocks | Blocks with disk < uncompressed |
|---|---|---|---|
| townfall pakchunk0-ps5.utoc | 0 names | 1,998,522 | 0 (disk == usz for all; sums equal exactly: 128,239,974,540 B) |
| townfall pakchunk1-ps5.utoc | 0 names | 1,128 | 0 |
| townfall pakchunk10-ps5.utoc | 0 names | 1,740 | 0 |
| townfall pakchunk11-ps5.utoc | 0 names | 687 | 0 |
| townfall pakchunk12-ps5.utoc | 0 names | 2,827 | 0 |
| townfall-manifest-capture global.utoc | 0 names | 53 | 0 |
| zzz_Leon_Townfall_2K_P.utoc | 0 names | 2,948 | 0 |
| zzz_Leon_Townfall_4K_P.utoc (previously uncovered) | 0 names | 4,484 | 0 |
| 1_999_P.utoc (rar intake) | 0 names | 730 | 0 |
| 1_999_P.utoc (controller-bundle copy) | 0 names | 730 | 0 |

*Region walk EOF-validated byte-exact for all ten (residual 0).

With zero method names registered, slot 0 ("None") is the only addressable
method. Every block additionally stores exactly its uncompressed size, so no
compression — Oodle or otherwise — occurs even in principle.

Note on the low byte: pakchunk0 and its derived containers carry low-byte values
1..29 in a uniform rolling pattern (~66k blocks each). This is NOT a method
index: (a) the table is empty, so indices 1..29 would be unreadable; (b) every
block still stores disk_size == uncompressed_size; (c) our own tool-written
copies of chunk0 (generated-final/derived utocs), produced by writers that
cannot emit Oodle, reproduce the identical 1..29 pattern — it is pass-through
non-compression metadata (bucket/shard id), not a codec selector.

### 3. UCAS containers (per-block compressed vs uncompressed sizes)

| ucas | Size (B) | TOC blocks end at | Blocks compressed |
|---|---|---|---|
| zzz_Leon_Townfall_2K_P.ucas | 185,446,608 | 185,446,608 (exact) | 0 / 2,948 |
| zzz_Leon_Townfall_4K_P.ucas | 286,110,336 | 286,110,336 (exact) | 0 / 4,484 |
| 1_999_P.ucas (rar intake) | 46,349,508 | 46,349,508 (exact) | 0 / 730 |
| 1_999_P.ucas (controller copy) | 46,349,564 | 46,349,564 (exact) | 0 / 730 |
| townfall-global-scriptobjects-1 global.ucas | 3,412,992 | 3,412,987 (+5 trailing capture bytes) | 0 / 53 |

Every block spans exactly its uncompressed size; no block header/geometry
anywhere indicates compression. The corpus pakchunk ucas siblings were not
retained, but their TOCs (which survived) already prove stored blocks.

### 4. All other saved containers

The remaining 58 .utoc files in the workspace are tool-generated/derived
(generated-*, packed/, derived/, rebuilt/) or 17–19 byte stubs. Full census
(evidence/all-utoc-census.json): zero method name tables, zero blocks with
disk < uncompressed. Nothing in the retained corpus depends on Oodle.

---

## Conclusion

No PAK, UTOC, or UCAS in the retained corpus registers or uses any compression
method. The project can proceed without an Oodle decoder.
