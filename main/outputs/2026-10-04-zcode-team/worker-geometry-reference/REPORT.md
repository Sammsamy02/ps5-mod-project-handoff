# REPORT — worker-geometry-reference (2026-10-04)

Honest account of what was verified, what was corrected, and what this reference cannot know.
Local-only: no console, no network, no execution outside this output dir. All reads were of the
retained eboot and pinned logs; all computations are reproducible from the two scripts in this
directory.

## Deliverables

- `GEOMETRY.md` — the authoritative human-readable reference (cite this).
- `GEOMETRY.json` — machine-readable table (phdrs, gaps, slide model, observed fragments, seed
  classifications, GMalloc tests, seed-arithmetic checks, false-claim retractions, unknowns; each
  fact with provenance).
- `parse_phdrs.py` — reusable independent ELF64 phdr parser (per-record byte-offset provenance).
- `derive_geometry.py` — regenerates `GEOMETRY.json` from the pinned inputs (asserts record
  identity at each cited line/byte offset before use).
- `phdrs-derived.json` — raw per-record phdr dump from `parse_phdrs.py`.

## What was independently verified (not trusted)

1. **Image identity:** 194,241,424 bytes, sha256 `3b6cfad4…3622fc` — recomputed this session; matches
   the seed and worker-c2-probe/EXPECTED-VALUES-OFFLINE.json.
2. **Phdrs re-parsed from bytes** with my own parser (after fixing an off-by-4 header-field bug in my
   first draft — disclosed in the script history): phnum=14, 5 PT_LOADs, per-record file offsets
   0x40/0x78/0xe8/0x1c8/0x200. Confirms the seed's LOAD0/1/7/8 numbers exactly; corrects LOAD3 memsz
   (0x1280000, seed said 0x12800000).
3. **Observed spans re-extracted from the raw logs** (not from any worker summary):
   igf3 clean paged dump (igf6-results.jsonl line 67), igf4 anchor records (lines 1103–1109),
   igf7 mapscan/cand_reads/summary (c2-alternative-results.jsonl lines 1122, 1413–1418), launch-9
   entry-log byte offsets @2270807/@2270933/@2269782 (asserted by `derive_geometry.py`), and the
   v1/v2 giant-span literals (byte 2384 in both files).
4. **Every arithmetic claim recomputed in Python** (`derive_geometry.py`), which caught FOUR digit
   errors in the controller seed and one in a pinned ledger verdict (below).

## Corrections to the controller seed (all provenance-stamped in GEOMETRY.json)

| seed claim | verified value | error class |
|---|---|---|
| LOAD7 file+0x400000 "would start 0xd13c000" → "fragment does NOT fit" | **0x953c000**; fragment [0x970c000,0x9a34000) IS LOAD7 BSS page-rounded, exact | 0x4000000 added instead of 0x400000 (digit-16^6 vs 16^5) |
| image extent [0x400000,0xfc602c8) | **0xc0602c8** (bridge computes it correctly; comment typo at runtime_start_bridge_r13.cpp:509) | same digit error |
| LOAD3 memsz 0x12800000 | **0x1280000** | extra zero |
| GMalloc delta 0x970add8−0x9350950 = 0x3B0488 | **0x3ba488** | digit slip |

**Consequence:** the seed's "LOAD7 NOT confirmed for +0x400000" premise dissolves. The uniform
slide `runtime = p_vaddr + 0x400000` is confirmed for L0/L1/L3/L7 by four independent observation
classes (igf3 clean tiling; igf7 byte-matched rodata reads; igf4 GUA read/writeback; launch-9
readable-span count 6 = 7 image spans − 1 exec-only). The "single global bias DISPROVEN" ledger
verdict is reversed; its origin (vaddr-semantics SEGMENT-CORRESPONDENCE §5) contains a digit-width
error (0x8018c000 ≈ 2.0 GiB is not inside L1's ~126–137 MB range).

## The false hole

The retracted claim, its stitch sources, and the refuting evidence are in GEOMETRY.md §8 item 1.
Short form: "continuous hole [0x787c000,0x8018c000)" = exec-span end (real) + giant-span start
(real, non-image, vintage-transient) + "nothing recovered between" (artifact of the 1160-stride
corruption per worker-span-arbitration §2). The igf3 clean walk, igf7 covered reads, igf4 GUA
writeback, and launch-9's 6 readable spans all sit inside the alleged hole. The actual launch-9
code=1 cause was the supplier's containing-base anchor rule (worker-facts-geometry-fix), so any
fix prioritized on the hole model was mis-prioritized.

## Best-supported mapping model (task 3 answer)

Uniform slide +0x400000 for every PT_LOAD; runtime image extent [0x400000, 0xc0602c8). Confidence:
very high for L0/L1/L3/L7 (four independent evidence classes). L8: observed unmapped at two
vintages (igf3 walk; igf7 A refusal) — placement formula still applies if it ever appears.
GMalloc: derived label **0x930add8** (exact fit, unique-store derivation); layout-profile candidate
0x9350950 rejected (would need slide 0x3ba488, fits nothing). Per-LOAD slides: no pinned observation
requires one; all observations that appeared to are explained by the digit errors above.

## Honest limits

- **Vintage coverage:** the igf3 clean walk is one boot at one state. Map churn between boots/runs is
  real (span totals differ across every walk). This reference is authoritative for the STATIC image
  (phdrs never change) and for the slide model (four vintages agree), but any consumer needing live
  spans must still run its own same-invocation mapscan and can use this document to interpret it.
- The igf3 walk's unique-span count is exactly 16,384 (= 2^14), so its tail may be truncated by a
  cap; this does not affect the low image region (covered in part 0), but I did not use it for
  high-address claims.
- The v1/v2 logs carry the known missing-comma writer bug; the giant-span lines are readable raw
  text but not valid JSON. I cite byte offsets, not parsed records, for those.
- igf4's GUA read/write proves address liveness, not FUObjectArray layout: the structural prediction
  configs were refuted in the same records (`gua_validated_any_cfg=0`). GEOMETRY.md says so
  explicitly.
- No new console evidence was collected; nothing here re-runs igf5/igf6-class probes or the xwp
  probe. mprotect/exec-read enforcement remains UNKNOWN.
- My first parse_phdrs.py draft had a header-field offset bug (e_flags unpacked at 0x34 instead of
  0x30) that garbled the record stride; it produced one obviously-wrong table before I caught and
  fixed it. The retained script is the fixed one; the wrong output was never persisted.
- I did not audit the FName-ctor/SCO/Tick/GUA *identity* evidence (byte-level uniqueness arguments);
  this reference classifies addresses geometrically and cites the owning workers for identity.
