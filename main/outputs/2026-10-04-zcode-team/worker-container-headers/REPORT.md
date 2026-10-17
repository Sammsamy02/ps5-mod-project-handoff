# REPORT — worker-container-headers (2026-10-04)

User correction implemented: `-==-`×4 is the GENERIC IoStore TOC magic; handler selection is now
keyed on the COMPLETE parsed header/version/structure. Worker-6's outputs were read and left
untouched (preserved provenance); all v2 artifacts live in this directory.

## Deliverables

| File | sha256 (first 16) | Content |
|---|---|---|
| `HEADER-SPEC.md` | 2ce6cb15f8632ee3 | Complete 144-byte utoc header + trailing array region, every field cited to a retained source (CUE4Parse pinned subset S1–S3, retoc runner S4, console receipts S5–S6); unprovable items explicitly listed unknown |
| `tools/utoc_header_probe.py` | 8ae9e9e6e776965a | Complete-header parser + block-level compression census + structural EOF check + verdict engine with exact per-field reasons; 5/5 selftest cases pass (synthetic bytes only) |
| `evidence/utoc-header-probe.json` | 366c4d5a61afa298 | Full parse receipt for all 8 corpus utocs |
| `CLASSIFICATION.md` | 6dfbb1ea67f726ad | Per-container handler selection; Zen-vs-desktop re-classification; pairing rule; open-delta status |
| `PROFILE-RULES-V2.md` | e202dcb7665461ad | Corrected admission set keyed on (magic, version, header size, block entry size, flags, referenced methods, EOF closure, partition count, sibling); schema v2; diff vs worker-6 rules |

## Key findings

1. **Magic correction proven from sources.** `-==--==--==--==-` is CUE4Parse's single
   `TOC_MAGIC` constant (S1) and the pinned retoc runner's only magic check (S4).
   `0xCAECA5A9` appears in NO retained source — only in worker-6's own documents; the
   "Zen-vs-desktop two-magic" claim is retracted. All 8 saved utocs are one format family:
   version 8 (ReplaceIoChunkHashWithIoHash), header 144, block entry 12.
2. **Structure fully proved by EOF closure.** For all 8 files the array walk
   (ids→offlen→seeds→blocks→names→dirindex→metas) lands exactly on the file size, and
   pakchunk0-ps5 reproduces receipt offsets 713880/1427616 (S6) byte-exactly.
3. **Oodle question closed** (worker-6 open #5): every name table is empty and every
   compression block references method 0 (None). No Oodle anywhere in the saved corpus;
   nothing saved needs a codec beyond None.
4. **Verdicts:** Leon trio utoc and 1_999_P = `supported` (complete pair, ucas + 347 B wrapper
   pak); the five main-game utocs + global.utoc = `supported-header-only`
   (`missing-ucas-sibling`); zero unsupported-by-format.
5. **ucas pairing resolved:** no ucas carries the TOC magic (verified first-16 bytes of all 3
   saved ucas); classification rides the same-stem utoc only.
6. **4699-vs-4544 delta:** still open, but NO new capture is needed — the pak's full directory
   index (offset 297250226, 56502 B) is inside the already-saved pak; a pak-index decode pass
   (plus optionally the utoc dir-index blob, string-table encoding unknown) settles it.

## Bounds compliance

Read-only on all corpus/captures; no network; no nested subagents; worker-6 files unmodified;
only tiny synthetic bytes created (inside selftest, not written to disk); CLAIM.json written
first, this REPORT last. Tool calls used ≈ 33 of 45.
