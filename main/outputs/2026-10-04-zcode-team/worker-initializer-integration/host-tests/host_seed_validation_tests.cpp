// Host tests for SeedValidation.hpp — the exact header compiled into the
// merged staged UnrealInitializer unit. Fixtures build fake FNamePool memory
// and drive every pure check through pass AND refute paths.
// Build: c++ -std=c++23 -Wall -Wextra -Werror -I../staging host_seed_validation_tests.cpp -o seed_validation_tests
#include "SeedValidation.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    int g_failures = 0;

    auto expect_ok(const rc_seed::Check& check, const char* what) -> void
    {
        if (!check.ok)
        {
            std::printf("FAIL %s: unexpectedly refused: %s\n", what, check.failure);
            ++g_failures;
        }
    }

    auto expect_refused(const rc_seed::Check& check, const char* what) -> void
    {
        if (check.ok)
        {
            std::printf("FAIL %s: unexpectedly passed\n", what);
            ++g_failures;
        }
    }

    auto expect_entry(const rc_seed::EntryMatch& match, const char* what) -> void
    {
        if (!match.ok)
        {
            std::printf("FAIL %s: unexpectedly refused: %s\n", what, match.failure);
            ++g_failures;
        }
    }

    auto expect_entry_refused(const rc_seed::EntryMatch& match, const char* what) -> void
    {
        if (match.ok)
        {
            std::printf("FAIL %s: unexpectedly matched\n", what);
            ++g_failures;
        }
    }

    // Places an ANSI FNameEntry (header u16 LE + chars) at `offset` in `block`.
    auto place_entry(std::vector<uint8_t>& block, size_t offset, std::string_view text, bool wide = false) -> void
    {
        const uint16_t header = static_cast<uint16_t>((static_cast<uint16_t>(text.size()) << 6) | (wide ? 1u : 0u));
        block[offset] = static_cast<uint8_t>(header & 0xFF);
        block[offset + 1] = static_cast<uint8_t>(header >> 8);
        for (size_t i = 0; i < text.size(); ++i)
        {
            block[offset + 2 + i] = wide ? static_cast<uint8_t>(0) : static_cast<uint8_t>(text[i]);
        }
    }

    // A fake pool laid out exactly as the merged unit reads it at runtime.
    struct FakePool
    {
        std::vector<uint8_t> pool;  // the FNamePool singleton
        std::vector<uint8_t> block; // Blocks[0] payload
        uint32_t none_id{};
        uint32_t byte_property_id{};

        // The fixture span wraps the host pointer (host addresses sit far
        // outside the target's literal 0x8018c000-0x100118000 span; that
        // literal span is exercised separately below as a refute case).
        uint64_t heap_lo{};
        uint64_t heap_hi{};

        FakePool()
        {
            // Layout constants straight from rc_seed::kPoolLayout.
            pool.assign(0x14050, 0);
            block.assign(0x100, 0);

            place_entry(block, 0x20, rc_seed::kNoneString);
            place_entry(block, 0x40, rc_seed::kBytePropertyString);
            none_id = 0x10;         // block 0, byte offset 0x20
            byte_property_id = 0x20; // block 0, byte offset 0x40

            const auto block_addr = reinterpret_cast<uint64_t>(block.data());
            heap_lo = block_addr & ~0xFFFFFFull;
            heap_hi = heap_lo + 0x1000000ull;
            const auto put32 = [&](size_t at, uint32_t value) {
                std::memcpy(pool.data() + at, &value, 4);
            };
            const auto put64 = [&](size_t at, uint64_t value) {
                std::memcpy(pool.data() + at, &value, 8);
            };
            put32(rc_seed::kPoolLayout.current_block_offset, 2);
            put32(rc_seed::kPoolLayout.current_byte_offset_offset, 0x8000);
            put64(rc_seed::kPoolLayout.blocks_offset, block_addr);
            put32(rc_seed::kPoolLayout.none_id_offset, none_id);
            put32(rc_seed::kPoolLayout.byte_property_id_offset, byte_property_id);
        }

        [[nodiscard]] auto u32(size_t at) const -> uint32_t
        {
            uint32_t value; std::memcpy(&value, pool.data() + at, 4); return value;
        }
        [[nodiscard]] auto u64(size_t at) const -> uint64_t
        {
            uint64_t value; std::memcpy(&value, pool.data() + at, 8); return value;
        }
        [[nodiscard]] auto entry_bytes(uint32_t id, size_t needed) const -> std::vector<uint8_t>
        {
            const auto location = rc_seed::entry_location(id);
            std::vector<uint8_t> bytes(needed, 0);
            std::memcpy(bytes.data(), block.data() + location.byte_offset, needed);
            return bytes;
        }
    };
}

auto main() -> int
{
    using namespace rc_seed;

    // --- entry_location decomposition (NameTypes.hpp block bits) ---
    {
        const auto loc = entry_location(0x00010010u);
        if (loc.block_index != 1 || loc.byte_offset != 0x20)
        {
            std::printf("FAIL entry_location decomposition\n");
            ++g_failures;
        }
    }

    // --- pool shape: pass and every refute direction ---
    {
        FakePool fix;
        expect_ok(check_pool_shape(fix.u32(kPoolLayout.current_block_offset),
                                   fix.u64(kPoolLayout.blocks_offset),
                                   fix.heap_lo, fix.heap_hi),
                  "pool shape (healthy fixture)");
        // A real host pointer sits outside the TARGET's literal heap span —
        // exactly the wrong-image refute direction on console.
        expect_refused(check_pool_shape(1, fix.u64(kPoolLayout.blocks_offset),
                                        0x8018c000ull, 0x100118000ull),
                       "pool shape refutes Blocks[0] outside the target span");
        expect_refused(check_pool_shape(0, fix.u64(kPoolLayout.blocks_offset), fix.heap_lo, fix.heap_hi),
                       "pool shape refutes CurrentBlock=0");
        expect_refused(check_pool_shape(1, 0, fix.heap_lo, fix.heap_hi),
                       "pool shape refutes null Blocks[0]");
        expect_refused(check_pool_shape(1, fix.u64(kPoolLayout.blocks_offset), 0xF000000000ull, 0xF100000000ull),
                       "pool shape refutes Blocks[0] outside a foreign span");
        expect_refused(check_pool_shape(1, 0x1000, 0x2000, 0x1000),
                       "pool shape refutes empty span");
    }

    // --- entry bounds ---
    {
        // Block 0 with CurrentBlock=1 IS the active block: valid when the
        // entry fits inside CurrentByteOffset (this is the normal state for
        // "None"/"ByteProperty", interned by FNamePool::Init at startup).
        expect_ok(check_entry_bounds(0x10, 2, 0x8000, 6), "entry bounds (older block)");
        expect_ok(check_entry_bounds(0x10, 1, 0x8000, 6), "entry bounds (active block, inside offset)");
        expect_refused(check_entry_bounds(0x20010u, 2, 0x8000, 6), "entry bounds refutes block == CurrentBlock");
        expect_refused(check_entry_bounds(0x10, 0, 0x8000, 6), "entry bounds refutes block past CurrentBlock");
        expect_refused(check_entry_bounds(0x7FFF10u, 2, 0x8000, 6), "entry bounds refutes huge block index");
        expect_refused(check_entry_bounds(0x10, 1, 0x21, 6), "entry bounds refutes past CurrentByteOffset");
    }

    // --- entry matching: pass + every refute direction ---
    {
        FakePool fix;
        const auto none = fix.entry_bytes(fix.none_id, 2 + kNoneString.size());
        expect_entry(entry_matches_ascii(none.data(), none.size(), kNoneString), "entry matches None");
        const auto bp = fix.entry_bytes(fix.byte_property_id, 2 + kBytePropertyString.size());
        expect_entry(entry_matches_ascii(bp.data(), bp.size(), kBytePropertyString), "entry matches ByteProperty");

        expect_entry_refused(entry_matches_ascii(none.data(), none.size(), kBytePropertyString),
                             "entry refutes wrong expectation");
        expect_entry_refused(entry_matches_ascii(bp.data(), 2, kBytePropertyString),
                             "entry refutes truncated payload");
        expect_entry_refused(entry_matches_ascii(nullptr, 10, kNoneString),
                             "entry refutes null bytes");

        // Length-mismatch direction: a len-4 entry against a 5-char
        // expectation must refuse before any character compare.
        expect_entry_refused(entry_matches_ascii(none.data(), none.size(), "Nones"),
                             "entry refutes length mismatch");

        std::vector<uint8_t> wide_entry(2 + 4, 0);
        place_entry(wide_entry, 0, "Wide", true);
        expect_entry_refused(entry_matches_ascii(wide_entry.data(), wide_entry.size(), "Wide"),
                             "entry refutes wide flag");

        std::vector<uint8_t> corrupt(2 + 4, 0);
        place_entry(corrupt, 0, "N0ne");
        expect_entry_refused(entry_matches_ascii(corrupt.data(), corrupt.size(), kNoneString),
                             "entry refutes character mismatch");
    }

    // --- full read chain against the fake pool (the merged unit's sequence) ---
    {
        FakePool fix;
        const auto current_block = fix.u32(kPoolLayout.current_block_offset);
        const auto cbo = fix.u32(kPoolLayout.current_byte_offset_offset);
        const auto blocks0 = fix.u64(kPoolLayout.blocks_offset);
        expect_ok(check_pool_shape(current_block, blocks0, fix.heap_lo, fix.heap_hi),
                  "chain: pool shape");

        const auto none_id = fix.u32(kPoolLayout.none_id_offset);
        const auto bp_id = fix.u32(kPoolLayout.byte_property_id_offset);
        expect_ok(check_entry_bounds(none_id, current_block, cbo, 2 + kNoneString.size()),
                  "chain: None bounds");
        expect_ok(check_entry_bounds(bp_id, current_block, cbo, 2 + kBytePropertyString.size()),
                  "chain: ByteProperty bounds");
        const auto none_bytes = fix.entry_bytes(none_id, 2 + kNoneString.size());
        const auto bp_bytes = fix.entry_bytes(bp_id, 2 + kBytePropertyString.size());
        expect_entry(entry_matches_ascii(none_bytes.data(), none_bytes.size(), kNoneString),
                     "chain: None string");
        expect_entry(entry_matches_ascii(bp_bytes.data(), bp_bytes.size(), kBytePropertyString),
                     "chain: ByteProperty string");
        expect_ok(check_probe_id(0x20, bp_id), "chain: behavioral probe equals pool id");
    }

    // --- garbage pool (wrong label/base simulation) refutes somewhere ---
    {
        std::vector<uint8_t> garbage(0x14050, 0xEE);
        const auto at32 = [&](size_t at) { uint32_t v; std::memcpy(&v, garbage.data() + at, 4); return v; };
        const auto at64 = [&](size_t at) { uint64_t v; std::memcpy(&v, garbage.data() + at, 8); return v; };
        const bool all_refused =
            !check_pool_shape(at32(kPoolLayout.current_block_offset),
                              at64(kPoolLayout.blocks_offset),
                              0x8018c000ull, 0x100118000ull).ok;
        if (!all_refused)
        {
            std::printf("FAIL garbage pool: some check unexpectedly passed\n");
            ++g_failures;
        }
    }

    // --- data-slot probes and probe-id guard ---
    {
        expect_ok(check_pointer_probe(0x97ba110ull), "pointer probe accepts aligned");
        expect_refused(check_pointer_probe(0), "pointer probe refuses null");
        expect_refused(check_pointer_probe(0x97ba111ull), "pointer probe refuses misaligned");
        expect_ok(check_probe_id(0x1234, 0x1234), "probe id accepts equality");
        expect_refused(check_probe_id(0, 0x1234), "probe id refuses zero");
        expect_refused(check_probe_id(0x1235, 0x1234), "probe id refuses divergence");
    }

    if (g_failures == 0)
    {
        std::printf("seed_validation_tests: ALL PASS\n");
        return 0;
    }
    std::printf("seed_validation_tests: %d FAILURES\n", g_failures);
    return 1;
}
