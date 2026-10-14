# Container classification by complete header — worker-container-headers, 2026-10-04

All values below are parsed by `tools/utoc_header_probe.py` from the saved corpus bytes
(`evidence/utoc-header-probe.json`); every file closed its structural EOF check exactly
(computed array-region end == file size), so these are full-structure identities, not magic sniffs.

## 1. Per-container parse results and handler selection

| Container | ver (name) | hdr | entries | blocks | dir-index | container_id | flags | compression referenced | siblings | Intake handler | Why |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc` | 8 (ReplaceIoChunkHashWithIoHash) | 144 | 162 | 2948 | 7392 B (Indexed) | 0x9039560806956a15 | Indexed | None only (table empty) | .ucas + 347 B wrapper .pak | **IoStore reader (full)** | Complete structure + ucas pair + retoc verify exit 0 (S7); wrapper pak must NOT be treated as loose-file content |
| `townfall-manifest-capture-2026-10-01/global.utoc` | 8 | 144 | 1 | 53 | none (not Indexed, size 0) | 0xffffffffffffffff | (none) | None only | none saved | **IoStore header-only** | Structure valid; chunk work blocked: missing-ucas-sibling |
| `townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc` | 8 | 144 | 59478 | 1998522 | 2870210 B (Indexed) | 0x8f8bb2ccc07563e0 | Indexed | None only | none saved | **IoStore header-only** | Cross-checks receipt offsets 713880 / 1427616 exactly (S6); missing ucas (console lists it at 128240377200 B, uncaptured) |
| `.../pakchunk1-ps5.utoc` | 8 | 144 | 197 | 1128 | 9691 B | 0xf4c0cfa40e8d6087 | Indexed | None only | none | **IoStore header-only** | missing-ucas-sibling |
| `.../pakchunk10-ps5.utoc` | 8 | 144 | 16 | 1740 | 1014 B | 0x8d988b9bf60b165b | Indexed | None only | none | **IoStore header-only** | missing-ucas-sibling |
| `.../pakchunk11-ps5.utoc` | 8 | 144 | 27 | 687 | 1501 B | 0x34b343b8c75a63f1 | Indexed | None only | none | **IoStore header-only** | missing-ucas-sibling |
| `.../pakchunk12-ps5.utoc` | 8 | 144 | 30 | 2827 | 1273 B | 0x8099615cdecd56d6 | Indexed | None only | none | **IoStore header-only** | missing-ucas-sibling |
| `townfall-62-rar-intake-1/bundle-input/1_999_P.utoc` | 8 | 144 | 27 | 730 | 922 B | 0x75b4f107f45fab23 | Indexed | None only | .ucas + 347 B wrapper .pak | **IoStore reader (full)** | Complete pair; retoc verify exit 0 (S7); byte roundtrip receipt corpus |

Common parsed facts across the whole corpus: `block_entry_size=12`, `partition_count=1`,
`partition_size=0xFFFFFFFFFFFFFFFF`, `encryption_key_guid` all-zero, not Compressed / not
Encrypted / not Signed / not OnDemand, name-table empty and every compression block references
method index 0 (None).

## 2. Re-classification of the earlier "Zen-vs-desktop" claim (per the user correction)

- The earlier claim (worker-6 `DETECTION-NOTES.md` §1, `CAPABILITY-MATRIX.md` §3): "`-==-` =
  Zen variant; desktop uses `0xCAECA5A9`" — **retracted and corrected**.
- Correction confirmed by retained sources: `-==--==--==--==-` is the single generic IoStore
  TOC magic constant of the format itself (CUE4Parse `FIoStoreTocHeader.TOC_MAGIC`, S1; the
  pinned retoc runner checks exactly `data[:16]`, S4). `0xCAECA5A9` appears **nowhere** in any
  retained source — it existed only in worker-6's own documents. There was never a two-magic
  Zen-vs-desktop dispatch problem; there was one magic and zero decoded fields.
- The real identity of every saved container is the complete tuple: **version 8
  (ReplaceIoChunkHashWithIoHash), header size 144, block entry size 12, flags {Indexed} (global:
  none), compression {None}, partition_count 1** — proven by exact structural EOF closure on all
  8 files. A desktop-cooked utoc of the same version would parse identically through this header;
  any platform distinction must come from cooked-payload evidence (the separate
  `platform_compatible: false` receipts for Windows-cooked packages), not from the TOC magic.
- Receipt name `townfall-original-zen-headers-1` is historical naming; its actual content
  (profile.json) records TOC offsets that my parse reproduces byte-exactly — it is evidence about
  THIS build's version-8 TOCs, not about a distinct "Zen format".

## 3. utoc/ucas pairing (resolved as a rule)

- Pairing is **by same-stem sibling only**. Empirically, no saved `.ucas` begins with the TOC
  magic: `zzz_Leon_Townfall_2K_P.ucas` starts `000000007c020000...`, `1_999_P.ucas`
  `00000000bc280000...`, `global.ucas` `aabe000003381300...` (first 16 bytes each). A bare
  `.ucas` is therefore not independently classifiable; it rides with its `.utoc`.
- Both complete pairs additionally carry a same-stem 347 B v11 wrapper `.pak` (worker-6
  detection rule retained): wrapper paks contain the sidecars, not loose-file content.
- retoc's own pairing behavior corroborates: `verify` panics file-not-found on every utoc whose
  ucas sibling is absent (S7 runs).

## 4. Compression/Oodle question (worker-6 open question #5) — CLOSED

The complete-block census (method index = high byte of each block's final u32, S3) shows every
compression block in every saved utoc references method 0 (None), and every name table is empty
(`CompressionMethodNameCount = 0`). **No Oodle is referenced anywhere in the saved corpus.**
The mod-side containers (Leon, 1_999_P) are stored uncompressed; chunk decoding needs no codec
beyond None for everything currently saved. Evidence: `evidence/utoc-header-probe.json`
(`referenced_methods_resolved` per file).

## 5. 4699-vs-4544 pak entry-count delta — still open, but no new capture is needed

- The utoc headers cannot resolve it directly: pakchunk0's TOC counts 59478 IoStore chunk
  entries (12-byte chunk-id namespace incl. uexp/ubulk/etc. variants), while the 4699/4544
  numbers are pak primary-index facts — different namespaces.
- Settle it from **already-saved bytes**: (a) decode the pak's full encoded directory index at
  offset 297250226, size 56502 inside the fully-saved `pakchunk0-ps5.pak` (297572462 B, sha
  `ee145cbc...`) and diff its unique names against the survey's 4544; and/or (b) decode the
  utoc directory-index blob (2870210 B at offset 25409880) once a retained string-table source
  exists — its encoding is unknown from the current pinned subset (HEADER-SPEC §4.4).
- If neither local decode is built, the next console capture that would settle it is nothing
  new — the bytes are already on this host; only decoder work is missing.
