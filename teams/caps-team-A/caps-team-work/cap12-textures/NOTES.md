# #12 Leon full look, round 5: derived textures in the PS5 tile layout (Team A, Mac-only)

Everything here is **Mac-only**. It was worked out from the game's own files already on the Mac (read-only,
including the integrator's read-only texture fetch) and Leon's unchanged mod files. Nothing has been seen on
the console yet.

## The short answer

- **10 of 11 Leon parts now use all 3 of Leon's textures** (colour, bumps, and the shine / metal /
  crease-shadow texture "ORM"). Round 4 used 2 of 3.
- **Every Leon texture those parts use is shipped as a corrected copy** (30 textures). Leon's downloaded
  files are never changed; the copies live only in our install overlay.
  - **All 30 are re-ordered into the PS5's own memory layout.** The game's PS5 textures store their pixel
    blocks in a GPU tile order; Leon's PC textures store them row by row, so on the PS5 they would look
    scrambled. The copy only moves whole compressed blocks: nothing is re-compressed, no quality is lost.
  - **The 10 ORM textures also get their red and blue channels swapped**, because the game's shader wants
    the same three channels in the mirror order (MRO). Also exact, on the compressed blocks.
- **Proof, per texture:** the re-ordered copy is byte-identical to Team B's independent reference converter,
  and both converters turn it back into exactly Leon's original bytes (or Leon's bytes with red/blue swapped).
  The red/blue swap is checked pixel by pixel by two independent decoders (36,722,272 pixels each, 0 differences).
- **The hair stays checkered.** Still refused; the exact missing proof is listed below.
- User decisions: only "treat Leon's body and skin as solid" (`accept-unproven-opaque`) is still needed.
  "Partial is OK" is no longer needed.

## What Leon should look like now, per part (plain words)

| Part | Colours | Bumps | Shine, metal, crease shadows | Still different from PC |
|---|---|---|---|---|
| Jacket (UpperBody) | Leon's own | yes | **yes, now** | game cloth lighting (soft fabric sheen) |
| Trousers, boots (LowerBody) | Leon's own | yes | **yes, now** | game cloth lighting |
| Pistol | Leon's own | yes | **yes: metal parts should look metallic** | game cloth lighting |
| Knife | Leon's own | yes | **yes, now** | game cloth lighting |
| Watches | Leon's own | yes | **yes, now** | game cloth lighting |
| Props (belt, holster ...) | Leon's own | yes | **yes, now** | game cloth lighting |
| Face | Leon's own | yes | **yes, now** | no soft "light through skin" glow (PC uses a skin shader) |
| Mouth | Leon's own | yes | **yes, now** | same as face |
| Eyes | Leon's own | yes | **yes, now** | game cloth lighting on the eyes |
| Eyelashes | Leon's own | yes | **yes, now** | **lower-confidence layout** (see below); solid strips, as on PC |
| **Hair** | **checker** | - | - | **unchanged: still the grey/white checker** |

"Yes, now" means: the dark shading in creases and folds comes back, each spot gets Leon's own shininess, and
the parts Leon's artist painted as metal (gun, knife, watch, buckles) are drawn as metal.

Unchanged limits: Leon's own material graphs exist only as PC shader code (their extra settings are lost); the
face and mouth use the game's cloth shader; the "invisible" materials that hide parts of Simon are Team B's
separate route (not in this change).

## Per-texture PS5 layout table (all 30 shipped textures)

Mode = the AMD swizzle mode written as ExtData (9 = SW_64KB_S, 5 = SW_4KB_S). Probe = Team B's pixel probe
reading the derived copy back. Confidence = Team B's estimate for the group.

| Texture | Format | Size | Channels | Mode (ExtData) | NumMipsInTail | Bulk / export bytes | Probe read-back | Confidence |
|---|---|---|---|---|---|---|---|---|
| T_Leon_Eye_D | DXT1 | 512x512 | kept | SW_64KB_S (9) | 9 | 131,072 / 66,474 | pixels agree | ~97% |
| T_Leon_Eye_N | BC5 | 512x512 | kept | SW_64KB_S (9) | 8 | 327,680 / 66,482 | pixels agree | ~98% |
| T_Leon_Eye_ORM | DXT1 | 512x512 | R<->B swap | SW_64KB_S (9) | 9 | 131,072 / 66,480 | pixels agree | ~97% |
| T_Leon_Face_D | DXT1 | 2048x2048 | kept | SW_64KB_S (9) | 9 | 2,752,512 / 66,570 | pixels agree | ~97% |
| T_Leon_Face_N | BC5 | 2048x2048 | kept | SW_64KB_S (9) | 8 | 5,570,560 / 66,578 | pixels agree | ~98% |
| T_Leon_Face_ORM | DXT1 | 2048x2048 | R<->B swap | SW_64KB_S (9) | 9 | 2,752,512 / 66,584 | pixels agree | ~97% |
| T_Leon_Knife_D | DXT1 | 2048x2048 | kept | SW_64KB_S (9) | 9 | 2,752,512 / 66,570 | pixels agree | ~97% |
| T_Leon_Knife_N | BC5 | 2048x2048 | kept | SW_64KB_S (9) | 8 | 5,570,560 / 66,578 | pixels agree | ~98% |
| T_Leon_Knife_ORM | DXT1 | 2048x2048 | R<->B swap | SW_64KB_S (9) | 9 | 2,752,512 / 66,584 | pixels agree | ~97% |
| **T_Leon_Lash_D** | DXT1 | 128x128 | kept | **SW_4KB_S (5)** | 7 | 8,192 / 4,938 | pixels agree | **~90% (lower)** |
| **T_Leon_Lash_N** | BC5 | 128x128 | kept | **SW_4KB_S (5)** | 6 | 20,480 / 4,946 | too small to decide | **~80% (lower: mode inferred)** |
| **T_Leon_Lash_ORM** | DXT1 | 128x128 | R<->B swap | **SW_4KB_S (5)** | 7 | 8,192 / 4,952 | pixels agree | **~90% (lower)** |
| T_Leon_LowerBody_D | DXT1 | 2048x2048 | kept | SW_64KB_S (9) | 9 | 2,752,512 / 66,578 | pixels agree | ~97% |
| T_Leon_LowerBody_N | BC5 | 2048x2048 | kept | SW_64KB_S (9) | 8 | 5,570,560 / 66,586 | pixels agree | ~98% |
| T_Leon_LowerBody_ORM | DXT1 | 2048x2048 | R<->B swap | SW_64KB_S (9) | 9 | 2,752,512 / 66,592 | pixels agree | ~97% |
| T_Leon_Mouth_D | DXT1 | 1024x1024 | kept | SW_64KB_S (9) | 9 | 655,360 / 66,522 | pixels agree | ~97% |
| T_Leon_Mouth_N | BC5 | 1024x1024 | kept | SW_64KB_S (9) | 8 | 1,376,256 / 66,530 | pixels agree | ~98% |
| T_Leon_Mouth_ORM | DXT1 | 1024x1024 | R<->B swap | SW_64KB_S (9) | 9 | 655,360 / 66,536 | pixels agree | ~97% |
| T_Leon_Pistol_D | DXT1 | 2048x2048 | kept | SW_64KB_S (9) | 9 | 2,752,512 / 66,578 | pixels agree | ~97% |
| T_Leon_Pistol_N | BC5 | 2048x2048 | kept | SW_64KB_S (9) | 8 | 5,570,560 / 66,578 | pixels agree | ~98% |
| T_Leon_Pistol_ORM | DXT1 | 2048x2048 | R<->B swap | SW_64KB_S (9) | 9 | 2,752,512 / 66,584 | pixels agree | ~97% |
| T_Leon_Props_D | DXT1 | 2048x2048 | kept | SW_64KB_S (9) | 9 | 2,752,512 / 66,570 | pixels agree | ~97% |
| T_Leon_Props_N | BC5 | 2048x2048 | kept | SW_64KB_S (9) | 8 | 5,570,560 / 66,578 | pixels agree | ~98% |
| T_Leon_Props_ORM | DXT1 | 2048x2048 | R<->B swap | SW_64KB_S (9) | 9 | 2,752,512 / 66,584 | pixels agree | ~97% |
| T_Leon_UpperBody_D | DXT1 | 2048x2048 | kept | SW_64KB_S (9) | 9 | 2,752,512 / 66,578 | pixels agree | ~97% |
| T_Leon_UpperBody_N | BC5 | 2048x2048 | kept | SW_64KB_S (9) | 8 | 5,570,560 / 66,586 | pixels agree | ~98% |
| T_Leon_UpperBody_ORM | DXT1 | 2048x2048 | R<->B swap | SW_64KB_S (9) | 9 | 2,752,512 / 66,592 | pixels agree | ~97% |
| T_Leon_Watches_D | DXT1 | 1024x1024 | kept | SW_64KB_S (9) | 9 | 655,360 / 66,530 | pixels agree | ~97% |
| T_Leon_Watches_N | BC5 | 1024x1024 | kept | SW_64KB_S (9) | 8 | 1,376,256 / 66,538 | pixels agree | ~98% |
| T_Leon_Watches_ORM | DXT1 | 1024x1024 | R<->B swap | SW_64KB_S (9) | 9 | 655,360 / 66,536 | pixels agree | ~97% |

(Also in `evidence/ps5-mode-table.md`.) Not shipped: the 4 hair textures (the hair stays checker).

**Lower-confidence group: the eyelashes (3 textures, 128x128, SW_4KB_S).** The game uses the 4 KB mode for its
own 128x128 DXT1 textures, but only one game 128x128 DXT1 and one 64x64 BC5 texture had readable pixels, and
Leon's 128x128 BC5 is too small for the pixel probe to decide. If anything looks wrong, look at the eyelashes first.

## How the PS5 layout is chosen and proven

**The game's own rule, from two sources:**
1. **The game's header census** (`PATCH/tools/ps5_layout_census.json`, built by `game_layout_census.py` from
   96 IoHash-verified game texture chunks of the integrator's fetch, plus 5 rows from Team B's probe of local
   game chunks): **101 of 101** game textures follow the rule below, mode **and** NumMipsInTail, across 7
   formats and 3 modes. Pixel agreement on game data exists for every mode Leon needs: 8-byte blocks in
   SW_64KB_S (3 textures) and SW_4KB_S (1); 16-byte blocks in SW_64KB_S (4) and SW_4KB_S (1).
2. **Public AMD addrlib:** the tile equations (checked in the tests against the public table), the padding and
   mip-tail placement, and its mode search (larger blocks preferred while padding stays within budget).

**The rule:** take the largest standard-swizzle block (64 KB, then 4 KB) whose tile fits the texture's size
in blocks; otherwise 256 B (not handled: refused). The tail starts at the first mip that fits half a tile
wide with at most 12 (64 KB) / 8 (4 KB) mips left. The game's mip placement is kept: mips above the tail in
the bulk file, the one tail block and the empty later tail mips inline.

**Fail closed when:** the census contradicts the rule anywhere; the census is too small; the needed
(block size, mode) has no pixel-agreeing game texture; the size is not a power of two, not a full chain, or
the whole chain would sit in the tail; the format is not a block format.

**Four gates per texture (Team B spec, section 7), at derive time AND again in paktx at install time:**
1. my own de-tile gives back exactly the linear input;
2. stored sizes and NumMipsInTail equal the rule (Team B's oracle);
3. my output is byte-identical to Team B's oracle `tile_reference.tile_chain`, and the oracle's de-tile gives
   back exactly the linear input;
4. Team B's pixel probe reads the derived package in the mode its header claims ("too small / uniform to
   decide" passes only because gates 1-3 pass).
My own de-tiler also reads the game's own 2048x2048 DXT1 textures as clean half-size mip chains
(similarity 0.995-1.0, including the tail mips; a row-by-row reading gives noise).

## How the ORM -> MRO swap is proven (unchanged from round 5)

- **Channel order, two sources each side:** Leon: parameter `ORM` + texture name `_ORM`. Game: slot `MRO` + all
  7 game textures bound to it named `_MRO` (linear); the slot's own default is `Default_MRO`.
- **The swap:** BC1 colour endpoints' 5-bit red and blue fields are exchanged; where that flips which endpoint
  is larger (629,215 of 2,295,142 blocks), the endpoints are exchanged and indices remapped so every pixel stays
  the same (a naive swap would break those blocks; a planted test shows both decoders catch it).
- **Proof:** every block of every mip decoded before and after by a Python decoder written from the public spec
  and by the Mac's GPU; both must say "after = before with red and blue swapped", same pixel count, and the
  GPU's decode must track the reference decoder (it really decoded the data).
- The per-slot proof that `MRO` takes a plain texture (its own default `Default_MRO` is plain) is unchanged.

## Hair: refused, and exactly why

The only game hair shader is `M_Hair`; it cuts the hair shape from its packed `IDRO Mask`. What is missing
(`evidence/hair-mask-check.json`):
1. **Names vs registry:** `IDRO` has 4 letters, but the registry says all 4 game IDRO textures have no alpha
   channel, so the 4th letter (`O`) has no channel by position.
2. **`O` is ambiguous:** in the game's other packed masks (`MRO`, `ODE`, `ORMH`) `O` is occlusion; `M_Hair` also
   has an ambient-occlusion parameter (`AOAdjust`).
3. **No texture statistics:** the 4 game IDRO textures' pixels are not on the Mac.
4. **Neutral values** for the I, D and R channels: nothing in the cooked game defines them.
Because the names source already fails, fetching the textures cannot make the hair pass under the two-source
rule. The optional read-only fetch plan is in `evidence/fetch-hair-mask-textures.json` (8 chunks, 89.7 MB).
Any mask built in the future would also have to go through the PS5 re-tiling above.

## New / changed tools (all generic, no game or mod names in logic)

| File | What |
|---|---|
| `ps5_tiling.py` (new) | the ONLY place that knows the PS5 tile order: equations, padding, mip tail, tile/detile, the game rule and its gates |
| `game_layout_census.py` (new) + `ps5_layout_census.json` | the game's own texture layouts from IoHash-verified chunks (+ Team B's pixel verdicts) |
| `texture_package.py` (new) | two-source texel layout (PC and PS5 form); `derive_ps5()` rebuilds a package in PS5 form |
| `bcn_swap.py`, `bcn_decode.py`, `metal_bcn_check.swift` (new) | exact R<->B swap; Python and GPU decoders for its proof |
| `material_defaults.py` (new) | per-slot default texture of a base material (second source for "plain sampler") |
| `hair_mask_check.py` (new) | the hair derived-mask gate (fail closed) + read-only fetch plan |
| `param_roles.py`, `reparent_plan.py` | swap route; every bound texture of a shipped instance needs a derived PS5 copy; per-slot proof |
| `derive_substitutes.py` | manifest `paktx-substitute/2`: derived MICs + derived PS5 textures, all proofs and the four tiling gates |
| `paktx.py` | ships the derived texture chunks; re-derives them, re-runs the swap proof and the four tiling gates (with Team B's pinned oracle), consumer gate across all mods |

Without `--derive-textures` the planner output is byte-identical to round 4 (A/B checked on real data).

## Real data (Mac-only)

- Plan `evidence/reparent-plan-r5-textures.json` (sha256 in `evidence/README.md`): 10 x `reparent`
  (coverage 1.0), 30 derived PS5 textures (10 with the swap), 57 known-checker (hair + Leon's own masters and
  parameterless instances, as before).
- Manifest `evidence/manifest/`: 10 material rows + 30 derived textures (60 chunks).
- paktx dry run: 2 targets; pakchunk0-ps5 sidecar 188,388,896 B; 70 derived chunks verified after the build;
  the merged store differs only in the 10 material entries.

## Tests (`sh tests/run_tests.sh`, saved output in `tests/output/`)

Final run: `RUN PASS mf_cap12=51 mf_full=24 cap07=45 cap12=50 textures=47`, `cap07_replay: PASS 155061`,
`tex_real: PASS 60121`.
- `textures` (47): swap exact on every format with both decoders; BC7 tables equal the GPU; planted faults
  (naive swap, wrong channel order, unsupported format, ambiguous blocks, decode mismatch, one-source proofs,
  tampered derived chunk / header / proof / pins, plan disagreements, drawing importers, base override, instance
  pointing at an underived texture); PS5 tiling: equations equal the public table, byte-equal to Team B's oracle
  on random chains, the game's own pixels read coherent, census rule 101/101, Leon textures through the 4 gates,
  planted wrong mode / wrong tail / de-tile mismatch / census faults; paktx install, re-read, re-proof from the
  installed bytes, byte-exact restore.
- `cap12` (50): Team A's round-3 substitute suite, its fixture adapted to round 5b (it now also derives the
  instance's 3 textures); `cap07` (45) + replay: the pak tool itself, unchanged behaviour.
- `mf_cap12` (51) + `mf_full` (24): the planner suites from rounds 3/4, unchanged results.
- `tex_real`: the whole real Leon pipeline (plan, manifest, paktx dry run, independent re-read, every shipped
  texture de-tiled by Team B's oracle and compared with Leon's original bytes, A/B with the integrated paktx).
- Every gate prints its count; the runner fails on zero tests, a zero-count gate, a skip, stale pins, a stale
  diff or a changed read-only input.

## Open risks / not proven

1. **Nothing is console-proven.** In particular whether the PS5 draws the derived textures as intended.
2. The eyelash group (SW_4KB_S) has the least game evidence (see table).
3. For streamed textures the game keeps mips above the tail in the bulk file; for the 128x128 eyelashes we keep
   that streamed split (Leon's own); the game's own small textures are all inline (no bulk file).
4. Leon's ORM sRGB flag is not decoded; the shader sees the same values as on PC.
5. Exactness of the BC1 swap is proven for the two decoders used; a GPU with a non-mirror-symmetric BC1 blend
   could differ by at most one 8-bit step on endpoint-exchanged blocks (invisible).
