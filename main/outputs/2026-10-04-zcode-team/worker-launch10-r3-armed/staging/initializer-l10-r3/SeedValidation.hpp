#pragma once
// SeedValidation.hpp — pure, engine-independent validation chain for the
// required-slot seed path of the merged staged UnrealInitializer unit.
//
// Provenance of every constant: FNAME-VERDICT.md section 4 (pool layout
// recovered from callee operand analysis, corroborated by the port's own
// NameTypes.hpp FNamePool constants: FNameBlockOffsetBits=16, entry stride 2,
// FNameEntryHeader bIsWide bit0 / LowercaseProbeHash bits5:1 / Len bits15:6).
//
// This header is deliberately freestanding (<cstdint>/<string_view> only) so
// the same bytes are host-tested with fixtures and compiled into the merged
// unit. All functions are total: they take plain values, never dereference.
// Refusal is the safe direction; a passed check is necessary, not sufficient
// (admission-grade, not identity-grade — FNAME-VERDICT section 5).

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rc_seed
{
    // FNAME-VERDICT.md section 4: FNameEntryAllocator layout and interned ids.
    struct PoolLayout
    {
        uint64_t current_block_offset;        // +0x8    u32 CurrentBlock
        uint64_t current_byte_offset_offset;  // +0xC    u32 CurrentByteOffset
        uint64_t blocks_offset;               // +0x10   u64 Blocks[0]
        uint64_t none_id_offset;              // +0x14040 u32 id("None")
        uint64_t byte_property_id_offset;     // +0x14044 u32 id("ByteProperty")
        uint32_t block_offset_bits;           // 16
        uint32_t max_blocks;                  // 8192 (FNameMaxBlockBits = 13)
    };

    inline constexpr PoolLayout kPoolLayout{
        0x8, 0xC, 0x10, 0x14040, 0x14044, 16, 8192};

    inline constexpr std::string_view kNoneString{"None"};
    inline constexpr std::string_view kBytePropertyString{"ByteProperty"};

    struct Check
    {
        bool ok;
        const char* failure; // nullptr when ok
    };

    // Entry id decomposition (port NameTypes.hpp: block = id >> 16,
    // byte offset = (id & 0xFFFF) * 2, stride 2).
    struct EntryLocation
    {
        uint64_t block_index;
        uint64_t byte_offset;
    };

    [[nodiscard]] inline auto entry_location(uint32_t id) -> EntryLocation
    {
        return EntryLocation{
            static_cast<uint64_t>(id >> kPoolLayout.block_offset_bits),
            static_cast<uint64_t>(id & 0xFFFFu) * 2ull};
    }

    // FNAME-VERDICT.md section 5 steps 1-2: CurrentBlock >= 1; Blocks[0]
    // nonzero and inside the readable heap span.
    [[nodiscard]] inline auto check_pool_shape(uint32_t current_block,
                                               uint64_t blocks0,
                                               uint64_t heap_lo,
                                               uint64_t heap_hi) -> Check
    {
        if (current_block < 1)
        {
            return {false, "CurrentBlock < 1 (pool not initialized or wrong pool/base)"};
        }
        if (blocks0 == 0)
        {
            return {false, "Blocks[0] is null"};
        }
        if (heap_lo >= heap_hi)
        {
            return {false, "heap span is empty"};
        }
        if (blocks0 < heap_lo || blocks0 >= heap_hi)
        {
            return {false, "Blocks[0] outside the readable heap span"};
        }
        return {true, nullptr};
    }

    // Bounds that must hold before any entry byte is touched: the block index
    // must exist, and an entry in the current block must fit inside
    // CurrentByteOffset together with its header and payload.
    [[nodiscard]] inline auto check_entry_bounds(uint32_t id,
                                                 uint32_t current_block,
                                                 uint32_t current_byte_offset,
                                                 size_t needed_bytes) -> Check
    {
        const auto location = entry_location(id);
        if (location.block_index >= kPoolLayout.max_blocks)
        {
            return {false, "entry block index exceeds FNameMaxBlocks"};
        }
        if (location.block_index >= current_block)
        {
            return {false, "entry block index at or past CurrentBlock"};
        }
        if (location.block_index + 1 == current_block)
        {
            if (location.byte_offset + needed_bytes > current_byte_offset)
            {
                return {false, "entry extends past CurrentByteOffset of its block"};
            }
        }
        return {true, nullptr};
    }

    // FNAME-VERDICT.md section 5 step 3: entry = header u16 (LE) then Len
    // ANSI chars, no NUL terminator. bIsWide must be clear for an ASCII
    // expectation. `available` bounds the readable bytes behind `entry_bytes`.
    struct EntryMatch
    {
        bool ok;
        bool wide;
        uint32_t len;
        const char* failure; // nullptr when ok
    };

    [[nodiscard]] inline auto entry_matches_ascii(const uint8_t* entry_bytes,
                                                  size_t available,
                                                  std::string_view expected) -> EntryMatch
    {
        if (!entry_bytes || available < 2)
        {
            return {false, false, 0, "entry too small for the u16 header"};
        }
        const uint16_t header = static_cast<uint16_t>(entry_bytes[0]) |
                                static_cast<uint16_t>(static_cast<uint16_t>(entry_bytes[1]) << 8);
        const bool wide = (header & 0x1u) != 0;
        const uint32_t len = static_cast<uint32_t>(header >> 6);
        if (wide)
        {
            return {false, true, len, "entry is wide; ASCII expected"};
        }
        if (len == 0 || len != expected.size())
        {
            return {false, false, len, "entry length does not match expected string length"};
        }
        if (available - 2 < len)
        {
            return {false, false, len, "entry payload exceeds the readable bytes"};
        }
        for (uint32_t i = 0; i < len; ++i)
        {
            if (entry_bytes[2 + i] != static_cast<uint8_t>(expected[i]))
            {
                return {false, false, len, "entry characters differ from the expected string"};
            }
        }
        return {true, false, len, nullptr};
    }

    // Admission-grade probe for data slots (SLOTS-FEASIBILITY section 4b):
    // non-null and 8-aligned. Explicitly NOT identity proof.
    [[nodiscard]] inline auto check_pointer_probe(uint64_t value) -> Check
    {
        if (value == 0)
        {
            return {false, "seeded address is null"};
        }
        if ((value & 0x7ull) != 0)
        {
            return {false, "seeded address is not 8-aligned"};
        }
        return {true, nullptr};
    }

    // Behavioral probe guard (FNAME-VERDICT.md section 5 step for the call):
    // the FName("ByteProperty", FNAME_Find) id must be nonzero and equal the
    // independently pool-read id at pool+0x14044.
    [[nodiscard]] inline auto check_probe_id(uint32_t probe_id, uint32_t pool_id) -> Check
    {
        if (probe_id == 0)
        {
            return {false, "behavioral probe returned NAME_None (ctor call did not take effect)"};
        }
        if (probe_id != pool_id)
        {
            return {false, "behavioral probe id diverges from the pool-read ByteProperty id"};
        }
        return {true, nullptr};
    }
}
