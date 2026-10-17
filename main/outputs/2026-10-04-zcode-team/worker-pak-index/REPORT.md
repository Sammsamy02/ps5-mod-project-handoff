# REPORT — worker-pak-index (2026-10-04)

Decode of the saved townfall `pakchunk0-ps5.pak` primary index (v11), from already-saved
bytes only (bounded seeks: footer tail + index region + the two regions its header points
at). CLAIM.json first; this file last; nothing outside this output dir written.

## Deliverables

| File | Content |
|---|---|
| `tools/pak_index_decode.py` | Bounded decoder: footer anchor, primary index, 96-B region table (SHA-1-verified), 12-B entry records, path-hash table head, full name-directory walk |
| `evidence/pak-index-decode.json` | Per-field values with byte ranges/hex, cross-checks vs pinned probe, reconciliation block |
| `REPORT.md` | This verdict |

## Verdict on 4699 vs 4544

**4699 is NOT a raw-record count hiding duplicates/deletes, and 4544 is NOT its
deduplicated residue. The two numbers count two different files.**

Exact arithmetic (saved pak, sha `ee145cbc...`, 297,572,462 B):

- Index count field (byte 14, `0x0000125B`) = **4699** raw 12-B entry records.
- Entry array `[114, 56502)` = 4699 × 12 B fills the region **exactly** (114 + 56388 = 56502).
- The 4699 records have **4699 distinct data offsets** (0 duplicates), monotonic, all
  < index start (data span ends 297,250,173 vs index at 297,250,226).
- The name directory (region C, 119,026 B at 297,453,215, SHA-1 verified vs region table)
  walks to exactly its end: 461 dirs, **4699 file names, all distinct**, 4699 distinct
  refs in `[0, 56376]`, **0 negative/sentinel refs** (refs are byte offsets into the
  12-B array; last record 56,376 = 56,388 − 12).
- Path-hash table (region B, SHA-1 verified) also carries count field **4699**.

So for the saved pak: 4699 names = 4699 entries = 4699 distinct data regions — nothing is
removed by name resolution. **4699 is simultaneously the raw and the resolved count.**

The 4544 figure belongs to the ORIGINAL on-disc pak, a different artifact: logical size
1,273,878,395 B (vs 297,572,462), index SHA-1 `8c3b85b6...` (vs `770bdb1d...`), full
directory index of 270 dirs / **4544** file records / 4544 distinct encoded offsets /
0 tombstones (frozen receipt cited below). That receipt's own arithmetic is also
duplicate-free — 4544 is its raw AND resolved count.

**4699 − 4544 = 155 is a build/corpus difference** (the saved/repacked corpus contains 155
more entries than the original disc pak), not an encoding artifact. Which 155 files were
added/changed is NOT determinable from saved bytes (the original's name list was never
saved, only its counts).

## Decoded layout of the saved pak's index (byte-proved)

```
footer (magic-anchored, last ~221 B): magic 0x5A6F12E1, version 11,
  index_offset 297250226, index_size 56502, sha1 770bdb1d..., 5x32B empty method
  slots; NO v10 path-hash footer fields (compact layout; bytes after idx fields = 160)
primary index @297250226:
  [0,14)     FString "../../../"   (i32 10 + 9 chars + NUL)
  [14,18)    i32 4699
  [18,114)   region table: lead u32 636187772 (semantics unknown), then 2 descriptors
             {i32 present=1, u64 off, u64 size, sha1[20]} + u32 56388 (blob size
             = 4699*12) + u32 0xE0000000 (master flags)
  [114, end) 4699 x [u32 data_offset][u32 size][u32 flags]
             flags: 0xE0000000 x4698, 0x0 x1 (record 0: offset 0, size 0 — observed
             anomaly, semantics unknown; no retained-source v11 sentinel matches)
region B @297306728 (146487 B, sha1 match): i32 4699, then (u64 hash, u32 12-step
  ref) pairs; 4+4699*12 = 56392; trailing 90095 B NOT decoded — unknown content,
  not needed for counts (region C closes the region exactly on its own)
region C @297453215 (119026 B, sha1 match): i32 461 dirs; per dir FString name +
  i32 n; per file FString name + u32 ref. Sample: Engine/GlobalShaderCache-SP_PS5.bin
  (ref 50928) ... Townfall/Plugins/Townfall.upluginmanifest (ref 56364)
```

## Source citations (encoding knowledge + 4544 provenance)

- Encoding: the retained CUE4Parse subset (`unreal-library-container-survey-1/parser/
  source/CUE4Parse/`) contains ONLY `UE4/IO/IoStoreReader.cs` and `UE4/IO/FIoStoreEncryptionIV.cs`
  — `FPakFile.cs`/`FPakInfo.cs` are **not** retained, so no retained source documents the
  v11 primary-index entry encoding; all per-record semantics above are labeled
  byte-proved/inferred in the evidence JSON, and duplicate/deleted sentinels are marked
  unknown-by-spec (absent-by-bytes). The retained repak copy is a stub:
  `mount-research/repak/repak/src/lib.rs:38-39,108-109` maps `Version::V11 ->
  VersionMajor::Fnv64BugFix` only.
- 4544 provenance: frozen receipt `unreal-library-container-survey-1/parser/
  actual-metadata-classification.json` (container `pakchunk0-ps5.pak`, ordinal 14:
  `file_records: 4544`, `encoded_entry_count: 4544`, `distinct_encoded_offsets: 4544`,
  `deleted_tombstones: 0`; `source_schemas` pins CUE4Parse@28f385e / repak@355b5f6) and
  `unreal-library-container-survey-1/reader-gap/scratch/receipt.json` (`file_count: 4544`).
  Note: `worker-2-layout-facts/REPORT.md` itself does not contain the figure — it points
  to the frozen surveys; the counts live in the receipts above (worker-6's
  PROFILE-RULES.md:38 and CAPABILITY-MATRIX.md:29 quote them).
- Inputs: `worker-6-containers/evidence/pak-probe.json` (pak path, sha, index offset/size,
  declared 4699). Probe bookkeeping note: it recorded `footer_size: 204`; region C ends at
  297,572,241 = file end − 221, so the footer region actually spans the last 221 B —
  harmless, does not affect any count.

## Integration note for the intake planner

Use **4699** as the authoritative entry count for the saved pak: enumerating names from
region C (offset 297,453,215, 119,026 B, verify SHA-1 `5db3bef0...`) yields exactly 4699
distinct names with a 1:1 mapping to the 12-B records at index bytes [114, 56502). Do NOT
use 4544 for this artifact — it is the true count only of the original on-disc pak. The
155-entry delta is a corpus difference between builds, still open only in the sense of
*which* files differ (requires original name data that was never saved).

## Bounds compliance

≤25 tool calls (24 used); no network; no subagents; read-only outside this dir; pak reads
limited to footer tail + three bounded regions (≈322 KB of 297 MB); no full-file hashing
(corpus sha pinned by probe; only read regions hashed for verification).
