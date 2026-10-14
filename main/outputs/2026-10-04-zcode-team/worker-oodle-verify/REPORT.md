# REPORT — worker-oodle-verify (2026-10-04)

Task: definitively close the container track's Oodle gap — does ANY retained
game container require Oodle compression decoding?

## Verdict

**PASS.** No retained PAK, UTOC, or UCAS registers or uses any compression
method. The pipeline can proceed without an Oodle decoder. Full tables and
byte-level evidence: `OODLE-VERDICT.md`.

## What was checked (all retained game containers)

Inventory (read-only sweep of the whole workspace for *.pak / *.utoc / *.ucas,
399 files; game-original vs tool-derived classified in evidence):

- **8 PAKs** (main pakchunk0-ps5.pak 297 MB, Leon 2K + 4K mod wrappers,
  metadata-only pak, 1_999_P x2, 2 ModBridge paks): v11 footer 5x32B
  compression-method name slots are **all zero** in every one. With no method
  names registered, no entry can reference any codec. Parser validated both
  ways with repak fixtures (positive control `pack_v11_compress.pak` shows
  slot[0]="Zlib"; negative control all-zero). Independent check: main pak index
  sum(uncompressed sizes) 297,001,179 ≈ payload span 297,250,173 (stored raw).
- **10 game UTOCs** (townfall chunks 0/1/10/11/12, manifest global, Leon 2K,
  Leon 4K, 1_999_P x2): compression method name table **empty (count 0)** in
  all — extending the prior 8-container header finding; Leon 4K and the second
  1_999 copy were not previously covered. New true block-entry layout decoded:
  `[u32 block_offset][u32 (disk_size<<8 | method_byte)][u32 uncompressed_size]`.
  Across all ~2,010,000 blocks: **disk_size == uncompressed_size everywhere**
  (chunk0 sums equal exactly: 128,239,974,540 B) — zero compressed blocks.
  Region walks EOF-validated byte-exact (residual 0) for all ten.
- **5 UCASes**: Leon 2K (185,446,608), Leon 4K (286,110,336), 1_999 x2,
  scriptobjects global.ucas — TOC block geometry tiles each file exactly
  (global has 5 trailing capture bytes), every block stored uncompressed.

## Key findings beyond the verdict

1. **Schema correction (important for downstream workers).** The prior
   worker-container-headers census read `u32 @ blocks+12i+8 >> 24` as the
   "method index". Under the now-decoded layout that field is the **top byte of
   the uncompressed size** — vacuously zero. Their "all None" conclusion was
   correct, but for the wrong reason. The true method byte is `u32@+4 & 0xFF`;
   it is 0 in every container except the chunk0-family bucket pattern below.
   Their region walk, offsets, and EOF math were byte-proved correct and were
   reused verbatim.
2. **chunk0 low-byte 1..29 pattern is not a codec selector.** ~1.9 M blocks
   carry rolling bucket ids 1..29 (~66k blocks each). Proof it is not a
   compression method: (a) empty method table would make indices 1..29
   unreadable by the game itself; (b) disk==usz for every block (no compression
   overhead exists anywhere in 128 GB of declared content); (c) our own
   Oodle-incapable writers reproduce the identical pattern in derived
   containers (pass-through metadata).
3. **worker-container-headers-v2 does not exist** at
   `outputs/2026-10-04-zcode-team/worker-container-headers-v2/` — the "v2
   confirmed" note in the gap list refers to a directory that is not present
   (only worker-container-headers exists). Flagging for the tracker.
4. Two 1_999_P captures differ (ucas 46,349,508 vs 46,349,564 = 56 B); each is
   internally consistent with its own utoc (blocks tile each exactly), so both
   are distinct retained captures, not corruption.

## Bounds and hygiene

- 29 tool calls (≤ 30 budget), no nested subagents, no network.
- Writes confined to `outputs/2026-10-04-zcode-team/worker-oodle-verify/`
  (CLAIM.json, tools/oodle_probe.py, evidence/oodle-probe.json,
  evidence/all-utoc-census.json, OODLE-VERDICT.md, REPORT.md). All reads of
  game containers were read-only; nothing else touched.
- Honest limitation: corpus pakchunk ucas payloads are not retained (TOC-only
  saves), so those five are proven via their surviving TOCs, not payload reads;
  every container with a retained payload was verified against the payload.

## Files

- `OODLE-VERDICT.md` — definitive verdict + per-container evidence tables
- `CLAIM.json` — task claim (written first)
- `tools/oodle_probe.py` — read-only probe (footer slots, TOC walk, block census)
- `evidence/oodle-probe.json` — full machine-readable results (10 utocs, 5 ucas
  pairings, 10 pak footers incl. controls, main-pak index sum)
- `evidence/all-utoc-census.json` — census of the 58 non-game utocs
