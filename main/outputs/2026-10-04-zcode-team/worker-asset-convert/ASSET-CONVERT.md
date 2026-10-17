# ASSET-CONVERT — PC→PS5 asset conversion pipeline design

Worker: `worker-asset-convert` · 2026-10-06 · offline research + design only.
No code changed, no console contact, no game/runtime execution, no installation,
no completion or acceptance claims. Everything below is a design backed by the
retained local corpus plus cited public format research; per this repo's law,
any "supported" verdict requires a validated capability on the exact target
fingerprint, and nothing here provides one.

**Question answered (ZCode priority #8):** can a pipeline automatically
translate standard PC mod formats into PS5 formats, and what should be built
first to unlock the most mods?

**One-paragraph answer.** Yes for the highest-value asset class: BC1–BC7
textures. The BC block bytes in a PC DDS are **the same bytes the PS5 GPU
decodes**; what changes is (a) the container — PS5 textures are described by a
GNF `GnmTexture` register descriptor rather than a DDS header — and (b) the
block **order** — PS5 (RDNA2/GFX10) stores block-compressed payloads in the
AMD `SW_64KB_S` macro-tiled swizzle, a pure bit-permutation at 4×4-block
granularity. Conversion is therefore a **lossless header rewrite + block
permutation**: no pixel decode/re-encode, O(payload) time, deterministic and
host-testable. A bare header swap is **not** sufficient (the payload must be
re-ordered per mip), but no transcoding is needed either. In Unreal games on
PS5 the texture does not live in a loose `.gnf` file — it lives inside
`.uasset` (descriptor + platform data) + `.ubulk` (swizzled payload) in
PAK/IoStore containers — so the shipping pipeline is:
unpack → convert texture payloads + patch `Texture2D` platform data → repack
(PAK v11 writer and IoStore merge components already exist in this repo).
Ranked by mods unlocked: (1) `Texture2D` payload conversion inside
PAK/IoStore, (2) the standalone DDS→GNF primitive that makes (1) testable,
(3) meshes/animations (blocked on platform cook research — the real Leon 2K
model mod needs this class too), (4) DLLs (separate Windows-translation
project, refused here).

---

## 1. PC mod format inventory

### 1.1 Ground truth from the retained real-mod corpus

Five real Nexus packages are retained under
`.superpowers/native-observations/nexus-real-mods-2026-10-01/` (all for
*Silent Hill: The Townfall* PC). Actual contents inventoried from the bytes:

| Package | Contents (measured) | Format class |
|---|---|---|
| `minimap-v3.1.zip` (47 KB) | `ue4ss/Mods/TownfallMiniMap/Scripts/main.lua` (197 KB), `enabled.txt` | UE4SS Lua |
| `mod-menu-v6.zip` (27 KB) | `TownfallModMenu/Scripts/main.lua` (128 KB), `enabled.txt` | UE4SS Lua |
| `infinite-stamina-v1.zip` (1.4 KB) | `Infinite Stamina/Scripts/main.lua` (2.8 KB), `enabled.txt` | UE4SS Lua |
| `leon-2k-v1.1.zip` (105.6 MB) | `zzz_Leon_Townfall_2K_P.pak` (347 B stub) + `.ucas` (185.4 MB) + `.utoc` (50 KB) + README | **IoStore asset mod** (character model + 2K textures + re-targeted face anims) |
| `ue4ss-v1.zip` (8.7 MB) | `dwmapi.dll` (PE, 279 KB) + UE4SS runtime + stock Lua mods + INIs | Windows DLL + Lua |

The Leon bundle is the pivotal data point: **the only real asset mod in the
corpus ships as an unencrypted, uncompressed IoStore v8 trio**. Per the
retained receipts (`outputs/2026-10-01-native-iostore-reader.md`,
`2026-10-01-townfall-mod-trial.md`, `2026-10-01-native-package-header-reader.md`):

- 162 chunk descriptors + 2,948 block descriptors verified by the shared C
  IoStore reader; largest chunk ~8.7 MB.
- 127 asset packages walked by the native package-header reader; independent
  reference headers captured for a 688-byte **texture** package and a
  15,249-byte **mesh** package.
- 68 asset paths/package IDs match the installed PS5 target's own indexes —
  i.e. the mod overrides real game assets, and the *staged unchanged-PC-bytes
  trial proved path matching but explicitly not platform-cook compatibility*.

### 1.2 Format inventory table (corpus + PC UE4 modding ecosystem)

| PC mod format | Evidence | PS5 status today | Disposition in this pipeline |
|---|---|---|---|
| UE4SS Lua (`main.lua`) | 3 of 5 real packages | Native PS5 UE4SS runtime port exists (see `worker-mod-compat/MOD-COMPAT.md`) | Pass-through, no conversion |
| INI/config/txt (`enabled.txt`, `load_order.txt`, UE4SS settings) | corpus + `2026-10-02-native-downloaded-ini-preparation.md` | Natively prepared | Pass-through |
| Legacy PAK v8/v11 | Leon's 347-byte companion PAK; Stellar Blade PS5 ships v11 itself | Reader (v11, real-index verified) **and** deterministic writer (`mb_pak_write_v11_none`, repak-cross-checked) exist | Container layer — unpack/repack available |
| IoStore `.pak+.ucas+.utoc` (v3/v8) | Leon (v8, uncompressed); Stellar Blade PS5 game files (v3 TOCs) | Reader verified on both; composer/merge/pack components exist (`iostore/none_packer.c`, `container_header_merge.c`, vendored BLAKE3) | Container layer — unpack/repack available |
| `.uasset/.uexp/.ubulk` inside containers | Leon's 127 packages | Package **header** reader only; no export serialization, no platform recook, no bulk decode | **This design** (textures); meshes blocked (§7) |
| Loose DDS/BC textures | standard PC texture-mod interchange; not loose in this corpus (they ride inside containers) | Full parser design done (`2026-09-30-shared-texture-contract-research.md`), no PS5 writer | **This design** (DDS→GNF primitive) |
| PE DLL (`dwmapi.dll`, C++ mods) | corpus | Refused with named requirement | Separate Windows-translation-layer project |
| Meshes / animations / materials (cooked UE) | Leon mesh packages | Nothing platform-side | Design note only (§7) |
| Audio (Wwise `.wem/.bnk`), video (`.bk2`) | ecosystem; not in corpus | Nothing | Out of scope, documented as unsupported |

**Mod-impact note.** In the real corpus the mod classes split 3 Lua : 1
IoStore-asset : 1 DLL-runtime. On Nexus at large, asset mods (textures/Models/UI
replacement inside PAK/IoStore) dominate UE4/UE5 game mod pages by count; Lua
logic mods dominate UE4SS-supported games. The asset pipeline therefore
unlocks a disjoint, high-volume class from the (already-ported) Lua path —
the two together cover everything in the corpus except the DLL runtime.

---

## 2. What already exists in this repo (capability map)

The conversion pipeline is **not** started from zero; these verified pieces
are its foundation:

| Capability | Evidence artifact | Status |
|---|---|---|
| DDS/BC facts-only parser contract (BC1–BC7, mips/arrays/cubes, neutral block form, checked arithmetic) | `outputs/2026-09-30-shared-texture-contract-research.md` (+ dds-module impl/review docs) | Designed + reviewed; the neutral-BC normalizer is exactly the pipeline's stage 2 |
| Legacy PAK v11 reader (real Stellar Blade indexes, 4,544 records) | `2026-09-30-unreal-container-survey.md` | Verified |
| PAK v11 writer (`mb_pak_write_v11_none`) | `2026-10-01-native-pak-writer.md` | 270 host checks + repak roundtrip + PS5 compile |
| IoStore reader v8/v3 | `2026-10-01-native-iostore-reader.md` | 9,788 checks; Leon + game TOCs verified |
| IoStore merge/pack components (`none_packer`, `container_header_merge`, `directory_merge`, vendored BLAKE3) | `ps5-mod-bridge/native-host/iostore/` | Present, exercised by preparation receipts |
| Download→prepare pipeline with wrong-game refusal, container-group detection | `2026-10-02-native-downloaded-pak-preparation.md`, `...-iostore-preparation.md` | Verified on real Leon ZIP (162 chunks, oracle-matched) |
| Package (uasset) header reader | `2026-10-01-native-package-header-reader.md` | 127/127 Leon headers structurally coherent |
| Staged install transaction (temp+final names, full SHA-256 readback, rollback) | `2026-10-01-townfall-mod-trial.md` | Verified on console; to be consumed by `worker-file-overlay`'s overlay system |

**Gap this design fills:** the platform-conversion stage between "unpack
PC-cooked payload" and "repack for PS5" — i.e. texture payload conversion +
`Texture2D` platform-data patching. No code for it exists yet anywhere in the
tree (verified by search: no GNF/swizzle implementation; only a false-positive
`Assi…gnF…unction` symbol match).

---

## 3. GNF format research

No GNF research existed in the tree before this worker; the following is
assembled from public sources (§3.5) and cross-checked across at least two
independent implementations where possible.

### 3.1 GNF container layout (version 2)

GNF ("GNM Native Format") is the PS4/PS5 native texture container — a small
header followed by raw, tile-mode-ordered texture payloads. Two independent
implementations agree on this layout (GFDLibrary `GNFTexture.cs`; OpenGNM
`compat/gnf.h`):

```
0x00  u32  magic          "GNF " (0x20464E47)
0x04  u32  contentsSize   size of the contents block (0xF8 for 1 texture)
--- contents block ---
0x08  u8   version        2
0x09  u8   textureCount
0x0A  u8   alignment      typically 8
0x0B  u8   unused
0x0C  u32  streamSize     total file size
0x10  per-texture descriptors (8 × u32 each, see 3.2)
      per-texture u32 payload sizes / metadata offsets follow the descriptors
--- payload area ---
      aligned to 0x100 for a single-texture file (payload at 0x100,
      matching community tooling observations); multi-texture files use the
      per-texture offsets (descriptor word[0], relative to contents end)
trailer: optional "USER" (0x52455355) user-data chunks (magic + size + bytes)
```

Key confirmation from OpenGNM: descriptor `word[0]` = payload **offset**
(relative to `sizeof(GnfHeader) + contentsSize`), `word[7]` = payload size.
GFDLibrary writes the same thing (`mWord0` left opaque + trailing
`metadataOffset`/`Data.Length`) — the two sources reconcile to one schema.

### 3.2 Per-texture descriptor (GnmTexture, 8 × u32 T#-style registers)

Bitfield layout from GFDLibrary `GNFTexture.cs` (little-endian words):

| Word | Bits | Field |
|---|---|---|
| 0 | all | payload offset (see 3.1) |
| 1 | 8–19 | MinLodClamp |
| 1 | 20–25 | **SurfaceFormat** (BC1=0x23, BC2=0x24, BC3=0x25, BC4=0x26, BC5=0x27, BC6=0x28, BC7=0x29) |
| 1 | 26–29 | ChannelType (0=UNorm, 1=SNorm, … sRGB variant pin needed, §3.4) |
| 2 | 0–13 | Width−1 |
| 2 | 14–27 | Height−1 |
| 2 | 28–30 | SamplerModulationFactor |
| 3 | 0–2 / 3–5 / 6–8 / 9–11 | ChannelOrder X/Y/Z/W |
| 3 | 12–15 / 16–19 | BaseMipLevel / LastMipLevel |
| 3 | 20–24 | **TileMode** |
| 3 | 25 | IsPaddedToPow2 |
| 3 | 28–31 | TextureType (2D=0x9, Cubemap=0xB, 2DArray=0xD, 3D=0xA, …) |
| 4 | 0–12 / 13–31 | Depth−1 / Pitch−1 |
| 5 | 0–12 / 13–26 | BaseArraySlice / LastArraySlice |
| 6 | 0–11, 12–19, 20–21, 22–23, 24 | MinLodWarning, MipStats index/enable, MetadataCompression, DCC fields, UseAltTileMode |
| 7 | all | payload size |

Relevant TileMode values: `0x08` Display_LinearAligned ("easily accessed on
the CPU"), `0x0D` Thin_1DThin (recommended read-only 2D; what GFDLibrary
writes), `0x0E` Thin_2DThin, `0x0A` Display_2DThin, thick/PRT variants above
0x13. Note these enum values are the **PS4/GNM tile-mode numbering**; PS5
RDNA2 surfaces are described by AMD GFX10 swizzle modes (below), so a PS5
target fingerprint must be respected when choosing the descriptor the game
expects — do not assume a PS4 tool's defaults are right for PS5 (§3.4).

### 3.3 BC payload identity: same blocks, different order

This is the crux for feasibility, and it is well-supported:

1. **Block bytes are identical.** DDS BC1–BC7 blocks are 8 or 16 bytes per
   4×4 texels; GNM surface formats BC1–BC7 (0x23–0x29) are the same
   block-compressed encodings. GFDLibrary's DDS→GNF path is literally a
   FourCC→SurfaceFormat table (DXT1→BC1, DXT2/3→BC2, DXT4/5→BC3, ATI1→BC4,
   ATI2/3Dc→BC5, + BC7 and RGBA8) with **no re-encoding** — only a swizzle
   permutation of the blocks.
2. **The permutation differs per console generation:**
   - **PS4 (GCN, GFX9):** 8×8-block Morton micro-tiles. One nested loop:
     for each 8×8 block tile, emit blocks in Morton order (daemon1's RawTex
     algorithm; GFDLibrary `PS4SwizzleAlgorithm`; CUE4Parse
     `DeswizzleXBPS4`). Copy granularity = one block (8 B for BC1/BC4,
     16 B for BC2/3/5/6/7).
   - **PS5 (RDNA2, GFX10):** AMD addrlib **`SW_64KB_S`** standard swizzle
     (tile_mode 9) is what real PS5 titles use for BC material textures
     (prosper project verified a live 1024×512 BC1_SRGB texture and BC7_SRGB
     atlases in this mode). Addressing is at 4×4-**block** element
     granularity; 64 KB macro blocks are 64×64 blocks for 16 B formats
     (BC2/3/5/6/7) and effectively 128×64 blocks for 8 B formats
     (BC1/BC4, two blocks packed per element). Within a macro block the
     element order is a fixed bit-permutation (XOR-separable
     `off(x,y)=fx(x)^fy(y)`), specified by AMD's Mesa addrlib
     `gfx10SwizzlePattern` tables. DrSwizzler's `PS5Swizzler`
     (RawTex-derived) implements the block-compressed path and is the
     implementation CUE4Parse/FModel ship for PS5 UE4 games.
3. **Per-mip application.** The swizzle is per mip level (each mip is its own
   surface with its own block grid). GFDLibrary's convenience path swizzles
   the whole buffer loosely; a correct converter must iterate mips and
   swizzle each independently, appending in mip order. (PS5 volume-texture
   slices additionally bleed into each other per DrSwizzler's docs — 3D
   textures stay out of scope for v1.)

### 3.4 Verdict on "simple header swap"

- **DDS→standalone GNF on PS4-era layout:** header rewrite + PS4 Morton
  block permutation. Not a pure header swap, but still lossless,
  block-granularity, and cheap.
- **DDS→PS5 texture payload:** header/descriptor rewrite + **GFX10
  `SW_64KB_S` block permutation** per mip. Again lossless; no decode.
- **A true header-only swap would require a linear tile mode**
  (e.g. `Display_LinearAligned`), leaving DDS's linear block order in place.
  This is *nominally* representable in the descriptor, but hardware sampling
  of linear-aligned BC surfaces has mode restrictions, costs bandwidth, and —
  decisively — **the engine's loader/serializer path for the target game
  expects the platform-standard swizzle**, so a linear-mode texture is an
  engine-acceptance risk we cannot validate offline. Verdict: **do not offer
  the header-only path**; implement the permutation. It is small, testable,
  and matches what every shipping PS5 modding tool does.
- **In-UE reality (the actual product form):** UE4/UE5 PS5 games do not load
  loose `.gnf` files. A modded `Texture2D` is: `.uasset` summary+export with
  platform cooked data (pixel format enum + platform texture metadata) and
  `.ubulk` holding the swizzled payload. So the DDS→GNF primitive is used
  *inside* a uasset patcher: convert payload, map `EPixelFormat` PC→PS5,
  rewrite the platform-data block, splice the new bulk, and repack the
  container. Exactly this shape is what FModel/CUE4Parse do in reverse
  (deswizzle PS5 mip → DDS) — our converter is the inverse operation, which
  is why their code is the right reference.

### 3.5 GNF research sources

Public, no SDK bytes required:

- [RE wiki — PlayStation GNF Image](https://rewiki.miraheze.org/wiki/PlayStation_GNF_Image) — container overview, tool list.
- [GFDLibrary (in D3fau4/image2gnf)](https://github.com/D3fau4/image2gnf) — complete DDS→GNF writer, descriptor bitfields, PS4 swizzle (BSD-style community code).
- [OpenGNM compat/gnf.h](https://github.com/PS4-OpenGNM/opengnm) — independent container schema, offset/size word placement.
- [DrSwizzler (Shadowth117)](https://github.com/Shadowth117/DrSwizzler) — PS4/PS5 swizzle library, PS5 volume-slice caveat.
- [CUE4Parse PlatformDeswizzlers.cs](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse-Conversion/Textures/PlatformDeswizzlers.cs) — the PS5 + PS4 deswizzlers FModel ships for UE console games; our roundtrip oracle.
- [prosper — GFX10 SW_4KB_S tiling](https://github.com/mattias800/prosper/blob/main/prosper/docs/gpu/GFX10_SW_4KB_S_TILING.md) and [GFX10 SW_64KB tiling](https://github.com/mattias800/prosper/blob/main/prosper/docs/gpu/GFX10_SW_64KB_TILING.md) — PS5/RDNA2 swizzle equations, tile_mode 9/27, BC block granularity, live-texture verification.
- Mesa AMD addrlib (`gfx10SwizzlePattern.h`, `ComputeOffsetFromSwizzlePattern`) — authoritative GFX10 pattern tables (cited by prosper).
- [texconv issue / UABEA #297 thread](https://github.com/nesrak1/UABEA/issues/297) — community Morton-pattern experiments for PS4/PS5 DDS↔GNF.
- [UTextureTools](https://hypermodule.github.io/utexturetools/) + [retoc](https://github.com/trumank/retoc) + [repak](https://github.com/trumank/repak) — the PC-side workflow this pipeline mirrors (uasset+ubulk editing, IoStore/pak repacking).
- Sony's `image2gnf` (SDK tool wrapped by e.g. [SSE-Fallout-4-Texture-Converter](https://github.com/Backporter/SSE-Fallout-4-Texture-Converter)) proves DDS/DDS-family→GNF is the vendor-intended conversion, but the SDK binary is not usable/redistributable here; the pipeline must be clean-room from the open implementations.

---

## 4. Pipeline design

```
PC mod archive (zip/rar/7z/loose files)
   │  1. INTAKE & INVENTORY      (exists: archive inventory, container-group
   │     detection, download_prepare; wrong-game refusal via profiles)
   ▼
   2. DETECT FORMAT              magic+structure, never extension-only
   │     "DDS " │ PAK v8/v11 footer │ utoc "-==-"/ucas │ uasset package magic
   │     │ PE "MZ" (refuse→Windows layer) │ Lua/INI text (pass-through)
   ▼
   3. UNPACK CONTAINER           (exists: PAK v11 reader, IoStore v3/v8
   │     reader, block assembly for uncompressed/None entries)
   ▼
   4. PER-ASSET ROUTE
   │     ├─ Texture2D (.uasset+.ubulk):
   │     │    a. parse uasset summary/exports (header reader exists;
   │     │       export-level Texture2D walker = new, small)
   │     │    b. extract PC bulk payload (linear BC blocks from PC-cooked
   │     │       texture, or source DDS if the mod ships one)
   │     │    c. neutral BC form (shared-texture-contract normalizer)
   │     │    d. per-mip GFX10 SW_64KB_S block permutation  ← core new code
   │     │    e. patch platform data: PC→PS5 EPixelFormat map, GnmTexture
   │     │       descriptor fields, bulk offsets/sizes
   │     │    f. verify: deswizzle(PS5 payload) == neutral blocks
   │     │       (per-mip SHA-256), then byte-budget/alignment checks
   │     ├─ Mesh/Anim/Material: v1 refuses with named requirement
   │     │    "texture.target_layout/mesh.platform_cook missing" (planner-
   │     │    style fail-closed), payload passed through untouched only
   │     │    when byte-identical to an original PS5 asset (no-op case)
   │     └─ Lua/INI/other loose files: pass-through
   ▼
   5. REPACKAGE                  (exists: mb_pak_write_v11_none for pak path;
   │     IoStore merge+pack + BLAKE3 block rehash for utoc/ucas path;
   │     preserve priority prefixes like "zzz_" and mount points)
   ▼
   6. INSTALL via overlay        (worker-file-overlay's system; built on the
   │     proven staged transaction: owned temp names → final names, full
   │     SHA-256 readback, rollback; originals never overwritten)
   ▼
   7. ACCEPTANCE GATE            engine mount + visible in-game effect —
         always a separate, explicitly-run verification; never inferred
         from roundtrip/hashes (repo law, per shared-texture-contract §planner)
```

Design rules carried over from the repo's established contracts:

- **Facts before transforms.** Stage 4c produces the neutral BC form with
  per-subresource hashes; the manifest records original/converted digests,
  parser + swizzler versions, and every tolerated-vs-rejected field.
- **Fail closed on unmapped inputs.** Unknown FourCC/DXGI, non-BC formats,
  3D/volume textures (v1), partial cubemaps, sRGB variants whose PS5 mapping
  is not pinned by target fingerprint → `missing_capability`, not "supported".
- **Lossless-by-construction.** No decode/re-encode exists anywhere in the
  texture path; every converted payload must deswizzle back to the exact
  neutral blocks. A digest match is evidence of bytes, not engine acceptance.
- **Bounded work.** Same checked-arithmetic discipline as the DDS contract
  (ceil-div block grids, 64-bit sizes, caps before allocation).
- **Determinism.** Same input + same profile ⇒ byte-identical output (the
  PAK writer already guarantees this for the pak path).

---

## 5. Priority ranking by mods-unlocked

Ranked by (real-corpus + ecosystem coverage) × feasibility ÷ risk:

**P1 — `Texture2D` payload conversion inside PAK/IoStore (uasset+ubulk patch).**
Unlocks the entire visual-replacement mod class: UI retextures, environment
retextures, character texture packs — the most common UE4/UE5 asset mod on
Nexus. Feasibility is the strongest of any asset class (§3.3–3.4). Honest
boundary: full model mods like Leon 2K additionally contain SkeletalMesh +
anim packages, which remain refused until P3 — a texture-only "retexture of
original characters" converts while "replace the character model" does not.

**P2 — Standalone DDS→GNF primitive (build first as the enabling core).**
Smallest self-contained deliverable (container writer + descriptor encoder +
swizzler + roundtrip tests). Directly unlocks nothing on UE4 PS5 (engines
don't mount loose `.gnf`), but it is the testable heart of P1 and would
serve future non-UE PS5 titles that do load GNF. Ship it as the first PR.

**P3 — Mesh/animation platform recook (design-only now).**
Leon-class model mods need `SkeletalMesh` render data re-serialized for PS5
(vertex buffers, GNM vertex declarations, platform inline data). Serialized
export layouts differ per platform and per engine version; this needs a
separate research pass over original PS5 mesh packages (the 15,249-byte mesh
reference header already captured is the entry point). Blocked, documented,
not estimated.

**P4 — DLL translation (refuse, name the project).**
`dwmapi.dll`/ASI class stays with the Windows translation layer workstream;
the pipeline detects PE magic and refuses with a pointer to that project.

*(Lua/INI mods need nothing from this pipeline — already native.)*

---

## 6. Implementation plan

**Phase 0 — plumbing audit (host, no new code):** pin the target game's PS5
fingerprints from already-captured TOC/ucas metadata: utoc version (v3
observed), IoStore compression (None observed), and — from the game's own
texture packages — the expected PS5 `EPixelFormat` values, TileMode/CT
fields, and whether bulk payloads carry any embedded descriptor prefix.
This is the "target fingerprint" the converter's mapping tables are scoped
to. *No console contact required; uses retained captures.*

**Phase 1 — neutral BC normalizer (host, C):** implement the
`shared-texture-contract` `inspect_dds`/`normalize_bc_blocks` API verbatim
(BC1–BC7, mips/arrays/cubes, checked arithmetic, trailer preservation) with
its published test matrix. Pure functions, no I/O.

**Phase 2 — swizzler + GNF primitive (host, C):**
- `swizzle_ps5_sw64kb_s(neutral, mip)` / inverse — port the DrSwizzler/Mesa
  equations; block-granularity copies only; per-mip application; reject 3D.
- GNF v2 writer/reader (container of §3.1, descriptor of §3.2) with
  DXGI↔SurfaceFormat↔EPixelFormat tables; unpinned sRGB mappings fail closed.
- Tests: hand-built fixtures with independently computed permutations; for
  every fixture, `deswizzle(swizzle(x)) == x` per-mip by hash; a public
  roundtrip corpus (self-made BC1–BC7 files; never game bytes).

**Phase 3 — uasset Texture2D patcher + container roundtrip (host):**
export-level `Texture2D` walker (extends the existing header reader), bulk
splice, platform-data rewrite; then repack through the existing PAK writer /
IoStore merge and verify with the existing readers (full container-level
roundtrip on synthetic mods; Leon's texture packages as read-only fixtures —
converted output is a *candidate*, never claimed installed).

**Phase 4 — integration + overlay handoff:** wire stages into the
download→prepare flow (as a new prepare kind beside `download_prepare_pak/
iostore`), emit conversion manifests, hand final artifacts to
worker-file-overlay's install path. In-game acceptance remains a separately
authorized, explicitly-run gate.

**Test matrix essentials (Phase 2/3):** BC1/BC4 (8 B) and BC3/BC7 (16 B);
square, non-square, non-multiple-of-8 extents (edge-block handling in the
permutation — padded tiles must not emit blocks for out-of-range coords);
mip chains incl. sub-8×8-block mips (macro-tile degenerates correctly);
cube/array metadata (PS5 payload pass-through for v1 if unproven — fail
closed); byte-identical determinism across runs; corrupt/truncated inputs.

---

## 7. Honest boundaries

- Nothing here has touched a console, executed engine code, or validated a
  single converted byte on PS5. "Feasible" means *supported by public format
  evidence + existing verified repo capabilities*, not proven.
- GNF/PS5 findings are from public community/open-source implementations
  (§3.5), which themselves carry caveats: prosper marks `SW_4KB_S` as
  empirically derived and `SW_64KB_R_X` pipe count as medium confidence;
  DrSwizzler notes PS5 volume-slice bleed; GFDLibrary's own multi-mip
  handling is loose. Our design pins per-mip `SW_64KB_S` (the mode prosper
  verified against live BC textures) and fails closed on everything else.
- The PS5-side `EPixelFormat`/descriptor values a specific target game
  expects **must come from that game's own captured packages** (Phase 0);
  PS4-era constants (e.g. TileMode 0x0D) are not assumed to transfer.
- Leon 2K cannot be fully converted by this pipeline even at P1 completion
  (meshes/anims are P3); only its texture subset is a P1 candidate.
- The unchanged-PC-bytes Leon staging on console proved path matching only;
  if that trial later shows the engine loading PC-cooked assets unchanged,
  the platform-conversion premise for that game would need re-examination
  (unlikely — cooking is platform-specific — but the pending visible check
  is the arbiter, not this document).
- No Sony SDK tools or bytes are used or needed; all equations are from
  Mesa addrlib (MIT), DrSwizzler, GFDLibrary, CUE4Parse, prosper docs.
