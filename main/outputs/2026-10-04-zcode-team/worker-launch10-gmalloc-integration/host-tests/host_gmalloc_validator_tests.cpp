// Host tests for the GMalloc validator addition (V1/V2) — the exact
// SeedValidation.hpp + SeedTable.h compiled into the launch-10 validator
// staging. Pure-check pass/refute fixtures plus a simulated Phase 2b read
// chain (byte-wise volatile reads, base+label arithmetic, then V1 then V2)
// against a fake BSS image laid out exactly as worker-gmalloc-derivation
// GMALLOC-CANDIDATES.json expects at runtime.
// Build: c++ -std=c++23 -Wall -Wextra -Werror -I../staging-l10-gmalloc-validator host_gmalloc_validator_tests.cpp -o gmalloc_validator_tests
#include "SeedTable.h"
#include "SeedValidation.hpp"

#include <cstdio>
#include <vector>

// Same alias shape as the merged unit's SeedArming scope (UnrealInitializer
// namespace chain: `namespace SeedTableNS = SeedTable;`).
namespace SeedTable = RC::Unreal::UnrealInitializer::SeedTable;

namespace
{
    int g_failures = 0;

    void expect_ok(const rc_seed::Check& check, const char* what)
    {
        if (!check.ok)
        {
            std::printf("FAIL %s: unexpectedly refused: %s\n", what, check.failure);
            ++g_failures;
        }
    }

    void expect_refused(const rc_seed::Check& check, const char* what)
    {
        if (check.ok)
        {
            std::printf("FAIL %s: unexpectedly passed\n", what);
            ++g_failures;
        }
    }

    // Byte-wise little-endian u64 read, identical shape to the merged unit's
    // SeedArming::read_u64 (volatile byte deref, LE assemble).
    auto read_u64(const volatile uint8_t* base_ptr, std::uint64_t offset) -> std::uint64_t
    {
        std::uint64_t value{};
        for (std::size_t i = 0; i < 8; ++i)
        {
            value |= static_cast<std::uint64_t>(base_ptr[offset + i]) << (8 * i);
        }
        return value;
    }

    void put_u64(std::vector<uint8_t>& mem, std::size_t offset, std::uint64_t value)
    {
        for (std::size_t i = 0; i < 8; ++i)
        {
            mem[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
        }
    }

    // Simulated runtime image (base + label addressing, observed base).
    constexpr std::uint64_t kBase = SeedTable::kBaseObserved; // 0x400000
    constexpr std::uint64_t kGMallocRuntime = kBase + SeedTable::kGMalloc.label;                 // 0x970add8
    constexpr std::uint64_t kInstanceRuntime = kBase + SeedTable::kGMallocInstanceLabel;          // 0x97089b8
    constexpr std::uint64_t kVtableRuntime = kBase + SeedTable::kGMallocVtableLabel;              // 0x82c26e0

    // A minimal sparse BSS: the GMalloc slot and the instance both live below
    // 0x970c000, so one flat buffer of that size is enough on the host.
    struct FakeImage
    {
        std::vector<uint8_t> mem;

        FakeImage() : mem(0x970c000, 0) {}

        const volatile uint8_t* at(std::uint64_t runtime) const
        {
            return mem.data() + static_cast<std::size_t>(runtime);
        }

        void bind_correctly()
        {
            put_u64(mem, static_cast<std::size_t>(kGMallocRuntime), kInstanceRuntime);
            put_u64(mem, static_cast<std::size_t>(kInstanceRuntime), kVtableRuntime);
        }
    };

    // The exact Phase 2b validator sequence as wired (probes elided — they
    // are unchanged upstream code; V1/V2 are the added gate).
    auto run_v1_v2(const FakeImage& image, std::uint64_t& out_malloc_ptr, std::uint64_t& out_vptr) -> bool
    {
        const std::uint64_t gmalloc_runtime = kGMallocRuntime;
        out_malloc_ptr = read_u64(image.at(gmalloc_runtime), 0);
        if (const auto v1 = rc_seed::check_gmalloc_instance(out_malloc_ptr, kInstanceRuntime); !v1.ok)
        {
            std::printf("refused V1: %s\n", v1.failure);
            return false;
        }
        out_vptr = read_u64(image.at(out_malloc_ptr), 0);
        if (const auto v2 = rc_seed::check_gmalloc_vptr(out_vptr, kVtableRuntime); !v2.ok)
        {
            std::printf("refused V2: %s\n", v2.failure);
            return false;
        }
        return true;
    }
}

int main()
{
    // --- Constant wiring sanity (base arithmetic per image.translation_rule).
    static_assert(kGMallocRuntime == 0x970add8, "GMalloc runtime");
    static_assert(kInstanceRuntime == 0x97089b8, "instance runtime");
    static_assert(kVtableRuntime == 0x82c26e0, "vtable runtime");
    static_assert(SeedTable::kGMalloc.present && SeedTable::kRequiredSetComplete, "armed table");

    // --- Pure V1.
    expect_ok(rc_seed::check_gmalloc_instance(kInstanceRuntime, kInstanceRuntime), "V1 exact match");
    expect_refused(rc_seed::check_gmalloc_instance(0, kInstanceRuntime), "V1 null loaded");
    expect_refused(rc_seed::check_gmalloc_instance(kInstanceRuntime + 8, kInstanceRuntime), "V1 adjacent-instance near miss");
    expect_refused(rc_seed::check_gmalloc_instance(0x12345678, kInstanceRuntime), "V1 foreign heap pointer");
    expect_refused(rc_seed::check_gmalloc_instance(kVtableRuntime, kInstanceRuntime), "V1 swapped operands");

    // --- Pure V2.
    expect_ok(rc_seed::check_gmalloc_vptr(kVtableRuntime, kVtableRuntime), "V2 exact match");
    expect_refused(rc_seed::check_gmalloc_vptr(0, kVtableRuntime), "V2 null vptr");
    expect_refused(rc_seed::check_gmalloc_vptr(kVtableRuntime + 0x10, kVtableRuntime), "V2 adjacent-vtable near miss");
    expect_refused(rc_seed::check_gmalloc_vptr(kInstanceRuntime, kVtableRuntime), "V2 swapped operands");

    // --- Simulated Phase 2b: correct image binds.
    {
        FakeImage image;
        image.bind_correctly();
        std::uint64_t malloc_ptr = 0;
        std::uint64_t vptr = 0;
        if (!run_v1_v2(image, malloc_ptr, vptr))
        {
            std::printf("FAIL correct image: validator refused a correct bind\n");
            ++g_failures;
        }
        else if (malloc_ptr != kInstanceRuntime || vptr != kVtableRuntime)
        {
            std::printf("FAIL correct image: bound 0x%llX -> 0x%llX (vptr 0x%llX)\n",
                        static_cast<unsigned long long>(kGMallocRuntime),
                        static_cast<unsigned long long>(malloc_ptr),
                        static_cast<unsigned long long>(vptr));
            ++g_failures;
        }
    }

    // --- Refusal matrix: every wrong shape must refuse, never bind.
    const auto expect_refusal = [](const char* what, auto mutate) {
        FakeImage image;
        image.bind_correctly();
        mutate(image.mem);
        std::uint64_t malloc_ptr = 0;
        std::uint64_t vptr = 0;
        if (run_v1_v2(image, malloc_ptr, vptr))
        {
            std::printf("FAIL %s: validator accepted a wrong image\n", what);
            ++g_failures;
        }
    };
    // [GMalloc] holds some other live aligned heap pointer (old weak probe passes).
    expect_refusal("foreign heap malloc", [](std::vector<uint8_t>& m) {
        put_u64(m, static_cast<std::size_t>(kGMallocRuntime), 0x81234500);
    });
    // [GMalloc] null (bootstrap store never ran — the single 0xb660 store).
    expect_refusal("null malloc", [](std::vector<uint8_t>& m) {
        put_u64(m, static_cast<std::size_t>(kGMallocRuntime), 0);
    });
    // [GMalloc] points at the right neighborhood but the wrong object.
    expect_refusal("instance off by 8", [](std::vector<uint8_t>& m) {
        put_u64(m, static_cast<std::size_t>(kGMallocRuntime), kInstanceRuntime + 8);
    });
    // Correct instance pointer but corrupted vptr (not the derived vtable).
    expect_refusal("corrupted vptr", [](std::vector<uint8_t>& m) {
        put_u64(m, static_cast<std::size_t>(kInstanceRuntime), kVtableRuntime + 0x8);
    });
    // Correct instance pointer but vptr null (vtable reloc not applied).
    expect_refusal("null vptr", [](std::vector<uint8_t>& m) {
        put_u64(m, static_cast<std::size_t>(kInstanceRuntime), 0);
    });

    if (g_failures == 0)
    {
        std::printf("ALL PASS (V1/V2 pure checks, correct-image bind, 6 refusal shapes; constants: "
                    "GMalloc 0x%llX -> instance 0x%llX -> vtable 0x%llX)\n",
                    static_cast<unsigned long long>(kGMallocRuntime),
                    static_cast<unsigned long long>(kInstanceRuntime),
                    static_cast<unsigned long long>(kVtableRuntime));
    }
    return g_failures == 0 ? 0 : 1;
}
