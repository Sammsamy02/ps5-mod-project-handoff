# Shared DDS block module design

## Intent and authorization

The user wants one private Nexus manager which runs entirely on the PS5 and reuses format/engine modules across games. A build computer is allowed during development. Anything requiring another device or unverified target behavior is Unsupported and cannot install. This module is an architectural addition to the portable preparation core: it supplies real texture layout and lossless block handling, not a PS5 texture writer.

The user explicitly requested unattended progress and delegated routine decisions. Accordingly the controller will review this written design and plan and use implementer/reviewer agents without pausing for another approval. These are controller decisions under that authorization, not a claim that the user reviewed these new files.

## Approach

Choose a dependency-free C++20 DDS reader and an owning neutral block object. Alternatives were importing DirectXTex (adds Windows-oriented implementation/dependencies) or continuing to classify DDS as opaque (provides no reusable conversion input). Microsoft DDS documentation and the independent audit in `2026-09-30-dds-design-audit.md` define the selected strict subset. DirectXTex is comparison evidence, not a console acceptance oracle.

Keep this a single bounded subsystem. Do not decode pixels, recompress images, infer channel use, emit PS5 tiling, add a GPU hook, or install a mod. Its outputs must continue to require a verified target writer and game/build acceptance.

## Files and interfaces

Create `outputs/ps5-mod-bridge/include/modbridge/dds.hpp` and `src/dds.cpp`; add parser/neutral tests in `tests/dds_tests.cpp`. Integrate recognition in `src/inspection.cpp` and actual total size in `src/inventory.cpp`; update build dependencies/explicit targets in the Makefile and add integration tests in `tests/inventory_tests.cpp`.

Public interfaces in namespace `modbridge`:

```cpp
inline constexpr std::uint64_t dds_max_subresources = 65536;
inline constexpr std::uint64_t dds_max_normal_bytes = 268435456;
Json inspect_dds(std::span<const std::uint8_t> bytes,
                 const std::string& path, std::uint64_t total_size);
struct DdsBlockPayload { Json description; std::vector<std::uint8_t> bytes; };
class DdsNeutral {
public:
    const Json& metadata() const;
    std::span<const std::uint8_t> header() const;
    const std::vector<DdsBlockPayload>& subresources() const;
    std::span<const std::uint8_t> trailer() const;
    std::vector<std::uint8_t> lossless_dds() const;
    // Private storage; constructed only by normalize_dds.
};
std::optional<DdsNeutral> normalize_dds(std::span<const std::uint8_t> bytes,
                                      std::string* reason = nullptr);
```

The neutral object owns exact header, ordered payload slices and opaque trailer. Metadata includes format identity, topology, dimensions, offsets, sizes and SHA-256 digests. Accessors expose immutable views. `lossless_dds` reconstructs identical source bytes; it is not a converted texture. Normalization only accepts complete supported input <=256 MiB, with bounded layout work. On failure return empty and a stable reason code, never fabricate missing bytes. Use existing SHA-256 implementation.

Extend `inspect_binary` with an optional third `std::optional<std::uint64_t> total_size = std::nullopt` argument. Existing callers retain full-span semantics. Inventory passes its streamed true size; its existing whole-file digest stays separate. A contradictory total smaller than the available span is an explicit input error. DDS recognition uses `DDS ` magic before extension classification; an extension alone is insufficient. The ordinary non-DDS paths retain their current behavior.

## Reader rules

Read little-endian fields by checked byte offsets, never reinterpret unaligned host structs. Require magic and canonical header/pixel-format sizes 124/32; DX10 adds 20 bytes. Record optional flags/reserved fields without requiring every customary writer flag. Zero mip count means one effective level; do not silently clamp an overlong chain. Nonzero width and height are required for the accepted 2D/3D subset. Validate mip maximum from the largest extent.

Explicit legacy encodings: DXT1/2/3/4/5, ATI1/ATI2, BC4U/BC4S/BC5U/BC5S. Preserve raw FourCC, DXT2/4 premultiplied alpha, and leave DXT1 alpha unknown. Other aliases and numeric FourCC stay opaque/unsupported. DX10 accepts enumerated BC1-BC7 DXGI IDs 70-84, 94-99 with the correct 8/16-byte block sizes; preserve exact enum, typelessness, UNORM/SNORM/UF16/SF16, sRGB and raw alpha bits. A non-sRGB enum says `no_explicit_srgb_tag`, not a proven linear color space. Reserved alpha bits are preserved and block normal-form eligibility if their semantics are unresolved.

Supported BC topology: 2D single/array, complete legacy cube, DX10 cube array, and a single 3D volume. DX10 arraySize counts cubes before six-face expansion. Cube data order is +X,-X,+Y,-Y,+Z,-Z within each cube; each face/item contains its mip chain. A 3D mip contains all depth slices; depth halves to one. Require DX10 3D depth declaration and arraySize=1. Cube on a non-2D resource, simultaneous cube/volume, zero array or extent, illegal dimension and impossible mip chains are explicit failures. BC/1D and partial legacy cubes are recognized unsupported topology. Do not invent faces or reinterpret 1D as 2D. For contradictory legacy/DX10 flags preserve facts and reject/mark unsupported conservatively.

For each mip calculate row_pitch=ceil(width/4)*block_bytes, slice_pitch=row_pitch*ceil(height/4), size=slice_pitch*depth. Use checked uint64 add/multiply throughout and enforce the subresource limit before allocation or iteration. Each span records item/cube/face as applicable, mip, width/height/depth, source offset, pitches and byte size.

Unknown/non-BC formats are recognized DDS with opaque payload layout, not generic malformed binaries. Canonical-size failures, missing bytes, unsupported format/topology, resource-limit failures and trailers have distinct stable statuses/reasons. No automatic permissive-header repair, bad-mip-tail fix or assumed padding.

## Completeness and evidence

Return an ordinary entry-shaped JSON (`kind: dds_texture`, facts, requirements, issues). Facts independently describe header_status, layout_status, input_total_size, input_bytes_available, payload_expected_end when known, payload_complete, subresources_hashed, normal_form_eligible, and ordered subresources. A capped prefix can establish expected layout and whether total size is sufficient; it cannot claim payload inspection or compute a missing slice hash. `payload_complete` means all declared bytes are available in this span. Record actual truncation separately from a capped prefix.

When expected BC end < total_size, record a separate opaque trailer range. Hash it only when all trailer bytes are available. Preserve complete trailers in lossless normalization, without assigning them meaning or granting target conversion. Each fully available subresource can be hashed; `subresources_hashed` counts those actually hashed. Whole-input/header/trailer hashes must never be computed from a prefix and presented as complete evidence.

For unsupported or incomplete DDS add unresolved preparation requirements appropriate to layout/inspection. Every DDS adds unresolved `texture.ps5_writer` (preparation) and `texture.game_acceptance` (game_process), even if parsed and normalized. `facts.inspection_complete` stays false when header/layout/payload evidence is incomplete; semantic_inventory_complete remains false. Existing inventory-wide conservative cap handling remains in force. A valid DDS alone must not make a real install plan ready.

## Verification

Owned synthetic fixtures use independently calculated byte offsets and expected pitches/sizes, not production parser helpers as an oracle. Cover BC edge dimensions, all exact typed formats, zero/overlong mips, 2D arrays/cube arrays, volume mip depth, partial cubes, BC/1D, unknown/non-BC formats, flags, truncated headers/body, prefix versus true truncation, opaque trailers, overflow/work limits and immutable lossless round trips. Inventory tests exercise magic recognition and true total size under a small inspection cap, and planner tests prove parsing does not authorize installation. Run focused core tests and sanitizer verification; existing host/account/UI suites provide the final regression checkpoint.

Success here means a reusable tested DDS input module. PS5 execution, PS5-format output and acceptance by Stellar Blade or another game remain separate milestones.
