# Shared DDS/BC contract research — 2026-09-30

## Recommendation

Add a read-only DDS inspection path and a lossless neutral container for BC1–BC7 block payloads. Its value beyond the present analyzer is structural evidence: header dialect, format identity, extent, resource dimension, mip/array/cube organization, calculated byte spans, per-subresource hashes, and exact raw compressed bytes suitable for a no-op round trip. It must not decode/re-encode pixels, infer game meaning, retile/swizzle data, or claim a console writer.

This is a bounded portable format module, not an acceptance claim. The current `inspect_binary` recognizes PE/ELF/Unreal package magic and a few extensions; `.dds` currently becomes `kind: data`, `classification_confidence: opaque`, plus `format.data_semantics` unresolved. Inventory streams full content hashes but only passes up to `max_inspection_bytes` (default 8 MiB) to binary inspection. A useful DDS integration therefore needs explicit “header inspected / payload complete” states and cannot silently treat a capped prefix as a complete texture. Existing planner requirements already block an entry whose requirements lack an exactly matching validated target capability; use that for future conversion/runtime claims.

## Facts from Microsoft’s DDS contract

- A DDS consists of the `DDS ` magic, a 124-byte `DDS_HEADER` containing a 32-byte `DDS_PIXELFORMAT`, then pixel data; a `DX10` FourCC adds a 20-byte `DDS_HEADER_DXT10`. The minimum structural lengths are 128 and 148 bytes respectively. Header flags are not reliably populated by every writer, so a reader should not reject a missing mip-count flag alone. [Programming Guide for DDS](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide), [DDS_HEADER](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header), [DDS_PIXELFORMAT](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-pixelformat), [DDS_HEADER_DXT10](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header-dxt10)
- Legacy FourCC names unambiguously map common encodings: DXT1→BC1, DXT2/DXT3→BC2, DXT4/DXT5→BC3; ATI1/BC4U→BC4 UNORM, BC4S→BC4 SNORM; ATI2/BC5U→BC5 UNORM, BC5S→BC5 SNORM. DXT2/DXT4 carry premultiplied-alpha semantics; DXT3/DXT5 do not. Preserve original FourCC separately because aliases and alpha convention are evidence. Do not accept the nonstandard “FourCC equals a numeric D3DFORMAT/DXGI_FORMAT” variant as canonical: Microsoft documents it as ambiguous and recommends DX10. Unknown FourCC is valid opaque data only if the header remains structurally coherent; its payload is unsupported for BC normalization. [DDS guide / common formats](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide), [BC1–BC5 FourCC table](https://learn.microsoft.com/en-us/windows/desktop/direct3d11/texture-block-compression-in-direct3d-11)
- DX10 supplies the DXGI format, resource dimension, cube flag, array size, and alpha mode. DXGI distinguishes typeless, UNORM/SNORM, and sRGB BC1–BC7. BC1 and BC4 blocks are 8 bytes per 4×4 texels; BC2/3/5/6H/7 are 16 bytes. Record the exact typed format; do not erase signedness, sRGB, typelessness, or BC6H signed/unsigned distinction in a shared `BCn` label. [DXGI_FORMAT](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format), [Texture Block Compression in Direct3D 11](https://learn.microsoft.com/en-us/windows/desktop/direct3d11/texture-block-compression-in-direct3d-11)
- BC6H is three-channel HDR with no alpha; BC7 has optional alpha. Microsoft specifies BC6H and BC7 for 2D textures, arrays, 3D textures and cubes, with 4×4 blocks; BC6H/BC7 block size is 16 bytes. Do not infer channel meaning (for example, BC5 means “normal map”), primaries, or scene semantics from the BC format. [BC6H format](https://learn.microsoft.com/en-us/windows/win32/direct3d11/bc6h-format), [BC7 format](https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/bc7-format)
- DX10 resource dimensions are 1D=2, 2D=3, 3D=4. A 2D cube uses the cube misc flag; its `arraySize` counts cubes, and the file has `arraySize × 6` faces. A 3D resource must have `arraySize=1`; depth comes from DDS_HEADER and shrinks with each mip. Legacy cube flags are face masks; legacy partial cubes exist, so retain the exact face mask and classify them as unsupported by a strict all-faces neutral cube operation rather than inventing missing faces. [DDS_HEADER_DXT10](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header-dxt10), [DDS cube layout](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-file-layout-for-cubic-environment-maps), [DDSCAPS2](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/ns-ddraw-ddscaps2)

### Color and semantic metadata

For DX10 BC formats, only an explicit `_SRGB` format establishes sRGB encoding. `_UNORM`, `_SNORM`, typeless and BC6H do not specify a broader color profile/primaries; BC6H does establish HDR floating block interpretation but not a game’s tone mapping. `miscFlags2 & 0x7` records alpha mode (unknown, straight, premultiplied, opaque, custom); preserve unknown/reserved upper bits and report them. Legacy DXT2/DXT4 identify premultiplication, while DXT1 alpha behavior depends on compressed block endpoints and must not be summarized as “opaque.” Record `color_transfer = srgb | linear_or_unspecified | unknown`, `alpha_mode`, and `channel_semantics = unknown`; do not turn these into inferred asset roles. [DDS_HEADER_DXT10 alpha metadata](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header-dxt10), [DXGI_FORMAT modifiers](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)

## Proposed bounded API

Keep public types small and independent of Direct3D headers or an external runtime. Existing `core.hpp` is the natural declaration point; implementation could be a dedicated `src/dds.cpp`, with `inspect_binary` dispatching on magic rather than extension. A suggested interface (names are illustrative):

```cpp
enum class BcFormat { bc1, bc2, bc3, bc4, bc5, bc6h, bc7 };
enum class ResourceDimension { texture1d, texture2d, texture3d };
struct DdsSubresource {
  std::uint32_t item;     // array item; cube faces represented as 6 face items
  std::uint32_t mip;
  std::uint32_t depth;    // volume depth at this mip; 1 for 2D
  std::uint32_t width, height;
  std::uint32_t blocks_x, blocks_y;
  std::uint32_t block_bytes;
  std::uint64_t payload_offset, payload_size;
  std::string sha256;
};
struct DdsInspection;       // exact header provenance + parsed facts + issues
struct NeutralBcTexture;    // typed metadata + tightly packed raw BC blocks
DdsInspection inspect_dds(std::span<const std::uint8_t> bytes,
                          const Limits& limits = {});
NeutralBcTexture normalize_bc_blocks(std::span<const std::uint8_t> bytes,
                                    const DdsInspection& inspection);
std::vector<std::uint8_t> write_dds_lossless(const NeutralBcTexture& texture);
```

Contract details:

1. `DdsInspection` keeps the original 128/148 header bytes (or a structured exact equivalent plus reserved fields), raw FourCC/pixel-format fields, DX10 raw fields, exact format enum value, original face flags, computed extents/offsets, payload end, trailer length/hash, parser version, input SHA-256 and completeness. `inspect_dds` is a facts-only operation. It never treats extension as proof.
2. The neutral form contains only block-compressed payloads and metadata needed to place those blocks: exact BC variant, typed/typeless state, alpha/color tags, dimension, extents, mip count, array/cube topology, depth, and ordered subresources. Each subresource is tight row-major block rows, with no per-row or per-slice padding. No decoded RGBA image is created. Normalization copies blocks byte-for-byte; it does not alter endpoints, indices, component order, or edge blocks.
3. Define neutral ordering explicitly as array item/cube-face item, then mip; 3D mip is one item with depth slices inside its block volume. DDS output must use the DDS subresource order for the selected header family. A lossless no-op writer retains the original header dialect and all reserved/opaque header bytes. To guarantee byte-identical round trip, also retain any opaque trailer bytes verbatim. A canonical new DDS header is a separate writer path and is outside the byte-identical claim.
4. Apply checked 64-bit arithmetic before each multiplication/addition: `ceil(width/4) × ceil(height/4) × depth × bytes_per_block × item_count`, sum all mips/items, then add header size. Guard dimension, mip, array, total byte and work limits before allocation. Use max mip count `1 + floor(log2(max relevant extent))`; 3D includes depth. Reject impossible/truncated claimed subresources; report bytes after the computed payload end as `opaque_trailer` (length/hash, preserved but not interpreted), not as a texture block or silently discarded.
5. Make incomplete evidence first-class: header-only parsing can report format/layout expectations, but it cannot issue complete subresource hashes or a normal-form object. If archive inventory supplied only the configured inspection prefix, return `payload_complete=false`, preserve whole-file hash from existing streamed inventory, and add an unresolved requirement for complete DDS payload evidence. For larger textures, a later streaming `read_at`/slice visitor can hash/copy declared subresources without loading all data; don’t increase memory bounds to obtain evidence.
6. Do not implement image decode/encode in this checkpoint. BC6H/BC7 modes are complicated and a raw normalizer gains no pixels; preserving exact blocks avoids an unnecessary lossy decode/re-encode path. Do not accept unsupported but structurally valid formats as BC; report format facts and opaque payload size, then add an actionable unresolved capability.

### Parser acceptance and unsupported cases

Accept structurally complete 2D and 3D BC1–BC7 DDS files, including 2D arrays and full cube/cube arrays, under strict layout checks. Include 1D metadata inspection only if the format is otherwise recognized; BC on a 1D resource should stay unsupported for block normalization. Keep non-BC formats (uncompressed RGB, float, packed YUV, depth/stencil, planar video, sampler-feedback, vendor/unknown DXGI) as structurally identified but unsupported by this BC-only normalizer. Legacy partial cubemaps are inspectable with face mask and unsupported for full cube neutralization. Unknown FourCC, ambiguous numeric FourCC, invalid DX10 dimension/array/cube combinations, zero extents, impossible mips, bad required header sizes, checked-arithmetic overflow, truncated payload and contradictory topology produce a parse/structure issue and incomplete evidence. Never map these to Supported.

When files contain extra bytes after all computed subresources, keep a distinct trailer span plus its hash and the original bytes for exact round-trip, but do not attribute those bytes to the last mip. The reader should state `payload_layout = exact | has_opaque_trailer | truncated | ambiguous`; exact parsing requires calculated payload end. Do not guess that arbitrary bytes are alignment padding.

## Evidence and planner integration

The existing inventory record already has per-entry `facts`, `requirements`, `issues`, `inventory_complete`, provenance and hashes. Add `kind: dds_texture` on signature match and facts such as:

```json
{
  "container": "dds",
  "header_kind": "legacy | dx10",
  "format": {"family":"BC5", "encoding":"BC5_UNORM", "numeric_value":83, "source_fourcc":"ATI2"},
  "dimension":"2d",
  "width":1024, "height":1024, "depth":1,
  "mip_count":11, "array_size":1, "cube":false,
  "alpha_mode":"unknown", "color_transfer":"linear_or_unspecified",
  "subresources_complete":true,
  "payload_layout":"exact",
  "original_sha256":"...",
  "normalization":"raw_bc_blocks_v1",
  "roundtrip":"byte_identical_verified"
}
```

Manifest evidence should include parser/normal-form versions, input SHA-256, raw-header hash, per-subresource descriptors and SHA-256, unsupported/opaque byte ranges, arithmetic/layout checks, and an operation label such as `inspect`, `normalize_copy`, or `byte_roundtrip`. A digest is evidence of bytes, not engine acceptance.

For planner use, represent separate capabilities/requirements rather than overloading one “texture supported” flag:

- `format.dds.inspect` and `format.bc.raw_blocks` may be marked available only for tested parser/normalizer operations, scoped by supported dialect/formats/dimensions and parser version. These are analyzer/transformer capabilities and do not authorize installing into a game.
- Add `texture.target_layout` / `texture.target_writer` requirements to source/target conversion steps. Without an implemented, validated target layout writer scoped to target fingerprints, planner reports `missing_capability`/`missing_backend`, leaving Needs conversion only if an actual on-console converter can satisfy it; otherwise Unsupported.
- Add `texture.game_acceptance` evidence only after exact game/build validation. Never satisfy it with a host round trip, format-family match, parser pass, matching hash, or desktop Direct3D support.

This fits the present planner: capabilities must be exactly matched, validated, in matching context/scope and target fingerprints; an analyzer-only backend is rejected for preparing/installing; unhandled constraint fields fail closed. No capability should mention a Nexus mod ID. `include/modbridge/backend.h` describes a future module ABI and currently loads nothing; this note does not add an external runtime dependency or imply dynamic backend support exists.

## Compact test matrix for the next design/implementation turn

| Case | Expected evidence/result |
|---|---|
| 128-byte legacy header; DXT1 BC1; 1×1, 4×4, 5×7 edge extents | Valid layout; ceil block counts; exact block byte length; exact no-op byte round trip |
| Legacy DXT2/DXT3/DXT4/DXT5; ATI1/ATI2; BC4S/BC5S | Correct format alias and alpha metadata; original FourCC retained |
| DX10 BC1–BC7 UNORM/SNORM/TYPELESS/sRGB variants + BC6H UF16/SF16 | Exact enum, signedness, sRGB/alpha facts; 8/16-byte blocks; byte-preserving round trip |
| Multiple mips, 2D arrays, cube, cube array; BC6H/BC7 3D | Stable item/mip/depth indexing; per-subresource hashes; exact sum of computed payload |
| Header flags omitted where Microsoft permits tolerant reading; `mipCount=0` | Treat count as one level when no mip chain declared; record source flags and any tolerated omissions |
| DX10 arraySize 0, 3D arraySize >1, cube flag with non-2D resource, impossible mip count, width/height/depth zero | Invalid/incomplete structure with specific reason; no normalization |
| Truncation at header, extended header, block, subresource, final byte | Incomplete payload, precise needed/available range; planner blocked |
| Opaque trailer after complete payload | Report trailer size/hash and preserve it byte-for-byte; no attribution to mip |
| Unsupported DXGI/FourCC and valid non-BC format | Recognize structural header and report unsupported format requirement; never reinterpret as BC |
| Arithmetic overflow, configured size cap, huge arrays/mips | Fail closed before allocation; bounded work/memory |
| Capped archive inspection with DDS header but incomplete body | Header facts only, `payload_complete=false`, whole-input hash retained, normalizer unavailable |
| Mutate one block, metadata field, trailer, or input after manifest | Corresponding per-subresource/header/trailer/input digest changes; stale evidence invalidated |
| Planner receives parser capability only / missing target writer / unvalidated evidence | `analyzer_only`, `missing_capability`, or `missing_backend`; no eligible install step |

Use hand-authored minimal DDS fixtures with independently calculated expected offsets/hashes, plus malformed variants. A separate upstream conformance corpus is useful later, but is not required to define this first narrow contract. Keep all tests and evidence host-side; no console, proprietary runtime, image library, AI, or target acceptance claim is needed.

## Primary Microsoft references

- [Programming Guide for DDS](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide)
- [DDS_HEADER](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header)
- [DDS_PIXELFORMAT](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-pixelformat)
- [DDS_HEADER_DXT10](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header-dxt10)
- [DDS cube-map layout](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-file-layout-for-cubic-environment-maps)
- [DDSCAPS2](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/ns-ddraw-ddscaps2)
- [DXGI_FORMAT](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
- [Texture Block Compression in Direct3D 11](https://learn.microsoft.com/en-us/windows/desktop/direct3d11/texture-block-compression-in-direct3d-11)
- [BC6H format](https://learn.microsoft.com/en-us/windows/win32/direct3d11/bc6h-format)
- [BC7 format](https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/bc7-format)
