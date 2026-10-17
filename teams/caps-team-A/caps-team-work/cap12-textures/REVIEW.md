# Independent review (Team A, cap12-textures)

The coordinator added the PS5 tile-layout requirement after the first review had finished, so the new
work got its own independent review at the end (no review of a review). Both reviewers worked read-only
on the Mac and wrote only to scratch folders.

## Review 1: R<->B swap, decoders, texture layout, generator and paktx texture rows

**Verdict: APPROVE WITH FIXES.** 0 blockers, 1 major, 5 minor, 4 nits.

The reviewer fuzzed the writer against the reference decoder and an own decoder with different rounding
(60k BC1/BC3 blocks, 20k BC7 blocks in all modes): 0 mismatches. They checked every BC7 field layout, the
mode 4/5 rotation mapping, and the partition/anchor tables. They also re-derived 3 real Leon textures:
every changed byte lies inside a mip payload.

| # | Severity | Finding (short) |
|---|---|---|
| 1 | major | The two re-checks treated "importer has no shader maps" as "cannot draw". But an instance draws with its parent's shaders. Importers in other installed mods were not checked. The test accepted three refusal codes. |
| 2 | minor | The GPU proof could say "exact" if both inputs decoded to the same garbage. Nothing checked that the GPU really decoded the data. |
| 3 | minor | The BC1-3 exactness claim is too broad. It holds for decoders whose 1/3 and 2/3 colour blends are mirror-symmetric; the D3D spec does not require bit-exact BC1-3. |
| 4 | minor | The per-slot default check only compared texture class, so a same-class swap in the pairing could pass. The pairing is not recorded in the plan. |
| 5 | minor | Nothing checks that the mod's own mask texture is linear (sRGB off). |
| 6 | minor | Hair statistics used only the smallest mip, and the cut-out test was too loose. |
| 7 | nit | The BC2/BC3 refusals were broader than needed. |
| 8 | nit | No guard for a pixel-format string in the first 48 bytes; `struct.error` was not caught. |
| 9 | nit | The GPU tool was compiled from disk after the pin check, and the texture tools were imported normally. |
| 10 | nit | `--derive-textures` also turns on the per-slot rule for every mapping. |

## Review 2 (final): PS5 tile layout, census rule, oracle gates, and the folded fixes

**Verdict: APPROVE WITH FIXES.** 0 blockers, 2 major, 6 minor, 2 nits.

The reviewer checked all 30 shipped Leon textures independently of our gates:
- every derived chunk is byte-equal to Team B's oracle `tile_reference.tile_chain`;
- `detile_chain` gives back exactly Leon's linear mips (red and blue swapped for the 10 ORM textures);
- the only changed header bytes are PackedData bit 30, the SkipOffset, the 8 inserted bytes, the bulk map and the export size;
- the bulk/inline flags are the game's own (0x10501 / 0x48).

| # | Severity | Finding (short) |
|---|---|---|
| 1 | major | The "largest tile that fits" rule is not singled out by the census. An addrlib-style budget rule also matches 101/101 rows but disagrees on sizes such as DXT1 256x256 and BC7 128x256. SETTLED needed only one row of the same (block size, mode) of any shape. MIN_CENSUS_ROWS counts rows, not variety. Leon is unaffected. |
| 2 | major | A `paktx-substitute/1` manifest still shipped instances that point at Leon's linear PC textures. The "every bound texture is a derived PS5 copy" gate ran only for /2 manifests. |
| 3 | minor | The only 16-byte SW_64KB_S evidence came from probe rows not tied to a TOC IoHash. |
| 4 | minor | Conflicting probe verdicts for one texture counted as agreement. |
| 5 | minor | 8-byte SW_4KB_S is settled only by a game texture whose whole chain sits in the tail. Leon's eyelashes have mip 0 above the tail. |
| 6 | minor | Gate 4 accepted any "undecided" verdict; the spec allows only "too uniform". |
| 7 | minor | The "can draw" walk treated unresolved packages as "cannot draw". |
| 8 | minor | The instance-import check covered this mod's store only. |
| 9 | nit | Hair statistics ran on tiled bytes (padding and tail neighbours included). |
| 10 | nit | Running the oracle left its folder on `sys.path`. |

All findings were fixed except review 1 #5 (disclosed). See `FIXES.md`.
