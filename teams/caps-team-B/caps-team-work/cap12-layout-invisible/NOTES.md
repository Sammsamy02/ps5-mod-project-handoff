# #12 support: texture layout and the invisible route (Team B, Mac-only)

Everything here is **Mac-only**. It comes from the game's own files already on the Mac, Leon's PC files (read-only) and public AMD GPU documentation (Mesa addrlib, MIT licence). No console contact. No Sony or Epic material.

## Short answers

**Q1 (texture layout): FINAL after the read-only fetch.** The game's PS5 textures store their pixels in AMD's public "standard swizzle" **tile order**. This is a different pixel memory layout, not extra metadata. Leon's PC textures store their pixels **row by row** (linear).
- **Verdict (b):** Leon's textures will load, but they will **not look right as they are**.
  - About 85% that they load without trouble.
  - About 85% that they look wrong without conversion. The fetch found **no** game texture without a layout block: 96 of 96 have one.
- **The tile order is now proven for every Leon format,** from the game's own files plus public AMD docs. A lossless conversion spec is in section 7. A reference implementation proves it byte for byte:
  - 101 game textures round-trip **byte-equal**;
  - all 34 Leon textures round-trip **byte-equal**.

| Leon format (count) | Mode to write | Confidence that the spec gives the console layout |
|---|---|---|
| DXT1, 512² and up (20) | ExtData 9 (64 KB) | **~97%**. 3 game DXT1 textures: header, pixels on every mip pair (bulk and tail), a table-free search picks the public order, byte-equal round trip |
| BC5, 512² and up (10) | ExtData 9 | **~98%**. 5 game 16-byte textures (1 BC5, 4 BC7): pixels (0.999), table-free search, tail position, byte-equal round trip |
| DXT5, 1024×2048 (1) | ExtData 9 | **~95%**. Same 16-byte element rule as BC5/BC7 (the order depends only on element size); no ExtData-9 DXT5 sample in the game |
| DXT1 128² (2) | ExtData 5 (4 KB) | **~90%**. The game's 128² DXT1 textures use mode 5 (2 of 2); mode-5 rules hold on 48 of 48 headers |
| BC5 128² (1) | ExtData 5 | **~80%**. No game 128² 16-byte texture to copy from: inferred from 64² BC5/BC7 (mode 5) and 256² BC5 (mode 9) |

**Q2 (invisible route).**
- **The game material:** `/Game/MetaHumans/Common/Materials/M_Hide`. It is proven "draws nothing" and proven compiled for skeletal meshes.
- **The rule:** generic and staged. It covers **7 rows** that really get drawn: 3 full, 4 partial. Those rows are the enemy "TheWall" Simon head's eye parts, Simon's glasses, the arm canula liquid, and Leon's own face at LOD1.
- **The other 36 invisible rows:**
  - 21 are never drawn: nothing the game or the mod loads imports them;
  - 15 are drawn only by things not proven safe (Blueprints, cinematics, static meshes alone, particles), so they stay checker (fail closed).
- **The 8 `Simon_Slot` materials are NOT invisible.** Their own settings say opaque. So they are not routed; they stay checker.

---

## Q1: what the "platform layout data" really is

### 1. The serialized fields (exact bytes)

Leon's `T_Leon_UpperBody_N` was compared with the game's `T_BodywarmerBeanieTag_Deluxe_Deluxe_N` (both BC5). The full dump is in `evidence/q1-field-sequence.txt`. Every field is the same in kind and order, except these:

| Field | Game (PS5 cook) | Leon (PC cook) |
|---|---|---|
| PackedData (u32 before the pixel-format string) | `01 00 00 40` = 0x40000001: bit 30 **HasOptData** set | `01 00 00 00` = 0x00000001 |
| Optional layout block (8 bytes right after `PF_BC5\0`) | `09 00 00 00 08 00 00 00` = **ExtData 9, NumMipsInTail 8** | absent |
| Mip list | 13 mips. Mips 0-4 in the bulk file, sizes 16 MiB / 4 MiB / 1 MiB / 256 KiB / 64 KiB. Mip 5 is **one 65,536-byte inline block**. Mips 6-12 have size 0. | 12 mips. Every mip has its own exact linear size (4 MiB … 16 B). |
| One property byte, and SkipOffset | different values only (e.g. a texture-group-like byte `04` vs `01`) | same structure |

So the "platform layout data" is exactly two things: the HasOptData bit plus the 8-byte block {ExtData, NumMipsInTail}. There are no other extra fields. All 12 game texture headers on the Mac carry it, all with ExtData = 9 (`evidence/q1-probe-local.json`, and `scratch/texsurvey.py`).

### 2. What ExtData = 9 means, and proof that the pixels really are tiled

- **Source 1, public enum.** In AMD's public addrlib (`evidence/public-addrlib/addrtypes.h`, lines 227-254), value 9 is `ADDR_SW_64KB_S`. It is the 64 KB "standard swizzle". It has no pipe/bank XOR, so it is the same on every AMD GPU configuration (checked against every pipe row of the public table: `tests/test_layout_tools.py`).
- **Source 2, the game's own pixels.** I read the game's 4096² BC5 normal map as linear, and under each public swizzle. Then I checked whether each mip looks like a half-size copy of the one above it (correlation 1.0 = perfect):

| Layout tried | mip 2 → 3 | mip 3 → 4 |
|---|---|---|
| linear (PC order) | 0.08 | 0.09 |
| SW_4KB_S | 0.02 | 0.03 |
| SW_64KB_D | 0.47 | 0.39 |
| SW_64KB_S, axes swapped (control) | 0.49 | 0.57 |
| **SW_64KB_S** | **0.997** | **0.999** |

(`evidence/q1-bc5-layout-scores.json`.) Three more checks back this up:
  - **No table needed.** Without using the AMD table at all, I tried all 924 possible bit orders of a 64 KB tile. The game's pixels pick exactly one: `Y0 Y1 X0 X1 Y2 X2 Y3 X3 Y4 X4 Y5 X5` (0.999; next best 0.565; median 0.065). That is the public SW_64KB_S equation, bit for bit (`evidence/q1-tablefree-search-bc5.txt`).
  - **The 64 KiB tail block.** It holds mips 5-12 exactly where public addrlib puts them. Mips 5/6/7/8 match the mip above at 0.999 / 0.999 / 0.998 / 0.987. Read at the wrong place: about 0. The public rule also predicts NumMipsInTail = 8 for both the 4096² and the 256² game textures, as in their headers (`evidence/q1-miptail-check-bc5.txt`).
  - **All 5 game textures with pixels on the Mac** (4 BC7, 1 BC5) are tiled the same way (`evidence/q1-all5-linear-vs-64KS.json`). Across the game's whole TOC, **754 of 754** non-virtual textures that carry a 64 KiB tail block have bulk sizes that equal the public SW_64KB_S size rule exactly (`evidence/q1-toc-bulk-size-census.txt`). The other 6,595 are virtual textures.
- **Leon's textures, the mirror check.** Read as linear they match 0.98-0.9998. Read as SW_64KB_S they turn to noise: 0.04-0.26 (`evidence/q1-leon-linear-vs-64KS.json`). Leon is linear PC order.

So the difference is **(b) a different pixel memory layout**, not only extra metadata. The 8-byte block simply tells the console which layout the bytes are in.

Note on the linear-size check in the #8 intake: for power-of-two textures of 256 px and up, a 64 KB tile holds whole rows of blocks. Tiled mips are then the **same size** as linear ones, so size alone cannot reveal tiling. Only the tail block (64 KiB instead of about 21 KB) and the pixel order show it.

### 3. Verdict for Leon: (b) loads, but will not look right as-is

- **It loads.** The mod's field sequence is a valid cook: the layout block is optional, and the bit says it is absent. Leon has already been installed and launched with these textures (earlier sessions, where the user saw the checker), and the game did not crash. That is an earlier session's observation, not a new console proof.
- **It cannot look fully right as-is.** Either way the console reads it, Leon's bytes are wrong for it:
  - **It treats "no block" as the default tiled layout:** Leon's linear bytes are read in tile order, and every mip is scrambled.
  - **It treats "no block" as linear** (ExtData 0 = `ADDR_SW_LINEAR` in the public enum): public addrlib pads every linear row to 256 bytes (`gfx10addrlib.cpp` line 5079, `pitchAlign = 256 / elementBytes`). Leon's rows are not padded. Large mips would then be right. Mips narrower than 128 px (DXT1) or 64 px (BC5/DXT5) would be garbled, so the textures go wrong at a distance.
- Which of the two happens is decided by the console's own texture code. I cannot read that (platform code is off limits). The console test will tell.

### 4. What to look for in game (after Team A's material fix makes the textures visible)

| What you see on Leon | Means |
|---|---|
| Real detail up close and far away: seams, patterns, face | (a): no conversion needed |
| Right colours, but shuffled into fine "TV static" inside square patches. Each patch is a 64 KiB tile: 256 × 256 texels on normal maps and the hair (BC5/DXT5), 512 × 256 texels on colour/ORM maps (DXT1). Lighting looks speckled. | (b) everywhere: tile conversion needed |
| Fine up close, noise or streaks only when far away or small on screen | (b) for small mips only: the console reads "no block" as linear |
| Crash, or texture errors in the log when Leon appears | (c) |

### 5. Fetch results (integrator's read-only fetch, 2026-10-13; Mac-only analysis)

- **Integrity.** All 127 chunks match both the plan's IoHash and the pristine TOC: 127 OK, 0 bad (`evidence/q1-fetch-20261013/verify.txt`, `SHA256SUMS.fetched`).
- **Headers.** 96 game texture headers. **Every one has the layout block.**
  - ExtData 1 (256B_S): 45
  - ExtData 5 (4KB_S): 48
  - ExtData 9 (64KB_S): 3
  - Only the public mode numbers occur. The pixels agree with the header wherever they are decodable, with no contradiction (`probe.json`).
- **The public rules, checked against every header.** NumMipsInTail and **every stored mip's byte size** match for **96 of 96** textures (`tail-size-rules.txt`). That covers 3 modes and element sizes of 1, 4, 8 and 16 bytes.
  - 256B modes have no tail (public addrlib uses its micro-tiled path).
  - A cooked chain starts at FirstMipToSerialize; `T_RTBrder` starts at mip 1.
- **DXT1 (8-byte blocks), SW_64KB_S proven.** Three 2048² game textures were checked.
  - The header says ExtData 9 and NumMipsInTail 9, which is the public rule.
  - **Pixels, de-tiled with the public rule.** Every mip pair 0→1 … 7→8 (bulk mips, then the tail at its public positions) is a coherent half-size copy: `T_Sky_Blue` 0.995-0.999, `T_Blockout_Grid` 0.97-0.998, and the noise texture 0.59-0.93. Read as linear they score 0.02-0.35 (`roundtrip.txt`, `probe.json`).
  - **Table-free search.** Over all 1,716 orders of a 128 × 64-block tile, each of the 3 textures picks exactly the public equation `X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4 Y4 X5 Y5 X6` as best. The noise texture is decisive: 0.887, next 0.527, median 0.04 (`tablefree-search-dxt1.txt`).
- **4KB_S is also seen on game pixels:** `T_Gray_N` BC5 (0.88 against 0.03 for linear) and `T_RTBrder` DXT1 (whole chain in one 4 KiB tail).
- **Byte-exact proof.** De-tiling and then re-tiling the game's stored bytes reproduces them exactly, every mip including padding and tail blocks, for **101 of 101** game textures. That covers DXT1, BC5, BC7, DXT5, BGRA8, G8 and FloatRGBA in modes 1/5/9 (`roundtrip.txt`).
  - The game fills unused bytes with 0.
  - A round trip alone holds for any reordering; the pixel tests above are what prove the order is right.
- **Bulk vs inline (game rule, 14 of 14).** In streamed textures the mips above the tail are in the bulk file (flags `0x10501`). The tail block and the zero-size tail mips are inline in the export (`0x48`). Small textures without a bulk file are all inline (`bulk-inline-split.txt`).

### 6. Can it be converted from allowed sources only? Yes: fully specified and proven (below)

This reorders texture bytes losslessly. Team A's notes say "texture bytes are never changed", so the **user must OK** this translation before it ships.

### 7. Conversion spec: lossless linear → PS5-tiled (for Team A, cap12-textures)

Executable reference: `PATCH/tools/tile_reference.py` (`layout`, `tile_chain`, `detile_chain`, `header_fields`). It works in memory only and never writes packages.

**Elements.**
- BCn: one element = one 4 × 4 block (DXT1 = 8 bytes; BC5/DXT5 = 16 bytes).
- The stored mip i has `max(1, w0 >> i) × max(1, h0 >> i)` elements. `w0` and `h0` belong to the first stored mip: SizeX >> FirstMipToSerialize, then ÷4 rounded up.
- Linear input = Leon's mip buffers: elements row-major, unpadded.

**Mode per texture** (copies the game's own choice for Leon's sizes; write it as ExtData):
- 512² and larger → **9 (SW_64KB_S)**. Every game texture of 256² or larger with a header on the Mac uses 9.
- 128² → **5 (SW_4KB_S)**, as the game's 128² DXT1 textures do.
- Every Leon texture is 512² or larger, except the three 128² eyelash textures.
- This is not a general rule for other sizes: the game picks 1, 5 or 9 per size and format.

**Tile order.** The element at (x, y) of a tile goes to tile-element index Σ bit_k << k:

| Mode, element size | Tile (blocks) | Bits k = 0.. (from the element-size bit upward) |
|---|---|---|
| 9, 8 B (DXT1) | 128 × 64 | `X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4 Y4 X5 Y5 X6` |
| 9, 16 B (BC5/DXT5) | 64 × 64 | `Y0 Y1 X0 X1 Y2 X2 Y3 X3 Y4 X4 Y5 X5` |
| 5, 8 B | 32 × 16 | `X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4` |
| 5, 16 B | 16 × 16 | `Y0 Y1 X0 X1 Y2 X2 Y3 X3` |

(Public `gfx10SwizzlePattern.h`, GFX10_SW_64K_S / 4K_S rows. They are the same for every pipe count, so no XOR.)

**Mips above the tail.** Each mip is its own surface:
- pitch and height are rounded up to whole tiles;
- tiles go row-major;
- byte offset = (tile_row × pitch_tiles + tile_col) × tile_bytes + index × element_bytes;
- padding = 0;
- stored size = padded pitch × padded height × element bytes. For Leon's power-of-two sizes it equals the linear size, except for small mips.

**Tail** (modes 5 and 9):
- **Where it starts:** the first mip i with `w ≤ tile_w/2`, `h ≤ tile_h` and `mips_left ≤ maxMipsInTail`. maxMipsInTail is 12 for 64 KB and 8 for 4 KB.
- **Leon results:**
  - DXT1 2048²: mips 0-2 above the tail, mip 3 starts it, NumMipsInTail 9;
  - BC5 2048²: mips 0-3 above, NumMipsInTail 8;
  - DXT1 128² at mode 5: NumMipsInTail 7;
  - BC5 128² at mode 5: NumMipsInTail 6.
- **Packing.** All tail mips go into **one** block of 65,536 bytes (or 4,096 for mode 5):
  - For tail mip j, m = maxMipsInTail − 1 − (j − first) and off = (m > 6) ? 16 << m : m << 8.
  - Origin X: bit k of X is bit 9 + 2k of off. Multiply by 8 for DXT1, by 4 for 16-byte formats.
  - Origin Y: bit k of Y is bit 8 + 2k of off. Multiply by 4.
  - Each element (x, y) is written at the tile index of (x + origin_x, y + origin_y). Unused bytes = 0.
  - The first tail mip stores that block; later tail mips store 0 bytes.

**Header fields** (the serialized platform data; nothing else changes structure):
- **PackedData:** set bit 30 (HasOptData), keep the other bits. Leon `0x00000001` becomes `0x40000001`.
- **Insert 8 bytes** directly after the pixel-format FString: u32 ExtData (9 or 5), then u32 NumMipsInTail. Example for Leon DXT1 2048²: `09 00 00 00 09 00 00 00`.
- **FirstMipToSerialize and NumMips:** unchanged.
- **Per-mip rows:** the bulk-map size is the stored size above. The first tail mip is 65,536 / 4,096, and the later tail mips are 0.
- **Placement** (game rule): mips above the tail stay in the bulk file (flags `0x10501`, offsets packed back to back). The tail mip and the later tail mips go inline in the export (flags `0x48`), with inline offsets relative to the export start.
- **Rebuild:** SkipOffset (+8 bytes, plus the changed inline payload), the export SerialSize, the package header, the bulk chunk, and the TOC/store rows (sizes and IoHashes).
- Write a derived package, never edit the mod file (same pattern as `mic_reparent.py`).

**Round-trip proof method** (every converted texture must pass all four; one failure means refuse):
1. **Lossless.** `detile_chain(tile_chain(L)) == L` byte-equal for every mip (Leon 34/34 pass).
2. **Rule.** The tiled mip sizes equal `layout(...)['sizes']`, and NumMipsInTail equals the header written.
3. **Oracle agreement.** Team A's converter output equals `tile_reference.tile_chain(L)` byte for byte. The oracle itself reproduces the game's stored bytes for 101 of 101 game textures (`detile → tile == stored`).
4. **Read-back gate.** `texture_tiling_probe.py` on the derived package must print `SW_64KB_S (header and pixels agree)`, or `SW_4KB_S …` for the 128² textures. A texture too uniform to decide counts as a pass only if gates 1-3 pass.

---

## Q2: invisible route

### The game material: `/Game/MetaHumans/Common/Materials/M_Hide`

| Check | Evidence (all Mac-only, game files) |
|---|---|
| Name says hide | token `hide` |
| Draws nothing: blend from three readers | asset registry `BLEND_Masked`, `MSM_Unlit`, `MD_Surface`; its own compiled PS5 shader types (5 peer materials agree: surface / masked / unlit); its own property block (reader C, decoder learned **without** it): `BLEND_Masked`, `MSM_Unlit` |
| Constant graph (nothing can make it visible) | its package holds no name but its own and imports nothing: no texture, no parameter, no function |
| PS5 shaders | 1 shader map in the game's pakchunk0 store (31 shaders) |
| Used for hiding by the game itself | imported by exactly 4 game packages, all skeletal face meshes: Simon, Simon side-step, Richard, Zoe. Simon's face mesh has 11 material slots but only 10 materials. 9 slots have a matching face material, so by elimination `M_Hide` fills the `cartilage` and `eyeEdge` slots (inference from names) |
| Compiled for skeletal meshes, source 1 | the 4 skeletal meshes above (store import graph and fetched, IoHash-verified headers) |
| Compiled for skeletal meshes, source 2 | its own property block carries property index 53. Index 53 was **learned from data**: it is the only index present in all 20 skeletal-used game masters and absent from 32 of 37 static-only ones. Learning it again without `M_Hide` gives the same index (and the reviewer re-checked every leave-one-out: 20 of 20) |
| Overlay-ready | header fetched whole and IoHash-verified (round 2); not replaced by the mod |
| Extra support (not counted as a source) | its 7 PS5 pixel shaders are byte-identical to MetaHuman `M_Cartilage`'s. 3 of them are also identical to the game's own `M_InvisbleMesh`, a different author's material that cinematics swap in to hide things. Only 4 of 5,139 shader-mapped materials have those 3 |

Not chosen:
- `M_InvisbleMesh`: misspelled name, so no name source; used only by cinematics, so no skeletal proof.
- `NaniteHiddenSectionMaterial`: header not fetched; no skeletal users.
- `M_FluxSurfaceInvisible`: opaque water; header not fetched.

"Opacity 0" itself is not stored in cooked files (the material graph is stripped). The "draws nothing" claim therefore rests on: its name, masked blend from three readers, a constant graph, and the game's own use of it to fill face-mesh slots that have no section material. The in-game check below confirms it.

### The mod parents: both `M_Invisible` (MetaHumans and Townfall folders)
- Mod-only, PC shaders only.
- **Source 1, name:** `invisible`.
- **Source 2, own settings:** reader C reads masked / unlit.
- Reader A (compiled PC shader types) keeps only "surface / masked", which agrees.
- Constant graph: yes.

### The 8 `Simon_Slot*` materials are opaque, not invisible
- Their own property block has no BlendMode entry, so the blend is the default, Opaque (reader C: `BLEND_Opaque`, `MSM_DefaultLit`).
- They are plain Materials used directly by 11 Leon body meshes, not instances.
- On PC they draw a plain surface if their sections have any triangles. Re-pointing them to `M_Hide` would change the PC look, so the rule refuses them.
- They stay checker. A fix needs a mesh-section decode (do these sections have triangles?), which is out of scope here.

### Which of the 43 invisible rows are covered

"Users" means who loads the instance at run time by hard import: game packages the mod does not replace, plus mod packages that import it, through instance chains. All of this comes from the game's own store graph.

| Result | Rows | Which |
|---|---|---|
| **reparent** (all users are skeletal meshes) | 3 | `MI_EyeR_Baked`, `MI_Face_EyeShell`, `MI_Face_LacrimalFluid`, all used by the enemy `SK_TheWall_SimonHead` |
| **reparent-partial** (skeletal plus other users; needs the existing user decision `allow-partial`) | 4 | `MI_Face_Skin_Baked_LOD1` (Leon's own face mesh and 2 Simon face meshes, plus 1 Blueprint); `MI_Simon_Accessories` and `_Glass` (`SK_SimonGlasses`, plus 4 static glasses meshes); `M_Medical_Liquid` (`SK_Canula`, plus `SM_Canula`) |
| refused: no run-time user | 21 | e.g. `MI_HeadSynthesized_Baked*`, beards, eyelashes, teeth. The mod replaced the meshes that used them, so nothing draws them and no checker can come from them |
| refused: no skeletal user | 15 | hair / peach fuzz (Blueprint only, probably grooms), `MI_Face_SideStep_Skin_*` (cinematics only), `MI_Face_Skin_Baked_LOD3`/`LOD5to7` (Blueprint only), IV-bag exteriors (Blueprint + static), oil liquid (Blueprint + Niagara), beanie tags |

For partial rows, the hide material is proven only for skeletal meshes. On the other users the engine may still draw the checker, exactly as it does today, so nothing gets worse.
- **User decision.** These rows ride on the existing `allow-partial` decision. **Tell the user that this decision now also covers "hidden on skeletal meshes, unproven elsewhere"** (the row fields are `coverage` 1.0, since no parameter is lost, and `user_coverage` for the proven share).
- **The rule needs a matching blend group:** a masked mod parent only goes to a masked game material.
- **Overlay checks.** All 7 rows pass Team A's own `mic_reparent.py` (export bytes identical, one parent import changed; receipts in `evidence/mic-reparent-dryrun/`), and Team A's `derive_substitutes.py` derives all 7 (test).

### In-game check (Leon launch with the overlay)
- **Enemy "TheWall" Simon head:** eye shells, right eye and tear film now show nothing where they showed the checker.
- **Simon's glasses** (if shown) and the **arm canula liquid:** gone, not checker.
- **Leon's face at middle distance (LOD1):** no checker band. If any of these still show the checker, the hide material lacks that usage; report which.

## Files
- `PATCH/tools/invisible_route.py` — the rule (adds rows to Team A's reparent plan).
- `PATCH/tools/texture_tiling_probe.py` — Q1 header + pixel layout probe.
- `PATCH/tools/layout_fetch_plan.py` — Q1 fetch plan.
- `PATCH/tools/tile_reference.py` — Q1 conversion spec as an executable oracle (tile/de-tile, tail, header fields).
- `scripts/q1_fetch_analysis.py`, `scripts/q1_roundtrip.py`, `scripts/q1_search_8bpe.py` — the fetch analyses (outputs in `evidence/q1-fetch-20261013/`).
- `tests/test_invisible_route.py` (59 tests), `tests/test_layout_tools.py` (18 tests).
- `REVIEW.md` (one independent reviewer: APPROVE WITH FIXES, no blockers), `FIXES.md` (all 12 findings fixed).
- `run_invisible.sh` — real-data run.
- `evidence/` — all outputs named above, plus the public addrlib copy with its source and checksums.
- `scratch/` — the analysis scripts behind the Q1 tables (pass `evidence/public-addrlib` as the addrlib folder).
