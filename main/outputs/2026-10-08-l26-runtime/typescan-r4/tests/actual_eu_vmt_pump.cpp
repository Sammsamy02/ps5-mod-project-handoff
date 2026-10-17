// =====================================================================================
// [EU FIX4/FIX5 2026-10-09, agent-EU] Host fixture for the GameEngine Tick VMT
// pump mechanics: the SAME VmtTickPumpCore.inc the target UnrealInitializer
// glue includes, exercised against mock GEngine objects and mock vtables.
// Pattern follows tests/run.py's dedicated suites (plain + asan-ubsan).
//
// Covers (Claude l26 fix 4 acceptance):
//   - bounded scan: extent = highest text-classified entry; null words legal
//     inside the extent; tick slot beyond the extent refuses; empty table
//     refuses; a non-text word INSIDE the copied extent refuses on arm;
//   - copy: offset-to-top + RTTI prefix preserved verbatim, every entry
//     bit-identical except the poked Tick slot, 16-byte-aligned allocation,
//     address point = allocation + 2 words, re-arm refuses;
//   - publish + hook: single aligned store swaps the mock object's vptr; a
//     virtual dispatch through the swapped table calls OUR hook, which
//     forwards to the ORIGINAL exactly once with the exact receiver/args,
//     publishes identity on the first call only, runs the first-tick
//     self-test once, then drains every tick;
//   - publication refusal: first-tick publish=false still forwards the
//     original call, skips self-test and drain, and the next tick retries
//     publication;
//   - restore: retire() puts the original vptr back (dispatch then hits the
//     original directly, hook never fires), second retire is a no-op, and a
//     foreign vptr writer is never clobbered (ownership loss reported);
//   - narrow_ascii_copy (FIX5 u16string-copy discipline): exact ascii copy,
//     NUL termination, truncation at cap, non-ascii downgraded to '?'.
// =====================================================================================
#include "VmtTickPumpCore.inc"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

using namespace RC::Unreal::VmtTickPump;

// ---- mock text segment: two real functions stand in for engine text ----
static int g_original_calls{};
static void* g_original_last_self{};
static float g_original_last_delta{};
static bool g_original_last_idle{};

static void mock_original_tick(void* self, float delta_seconds, bool b_idle)
{
    ++g_original_calls;
    g_original_last_self = self;
    g_original_last_delta = delta_seconds;
    g_original_last_idle = b_idle;
}

static int g_publish_calls{};
static bool g_publish_result{true};
static bool publish_callback() noexcept
{
    ++g_publish_calls;
    return g_publish_result;
}

static int g_selftest_calls{};
static void selftest_callback() noexcept
{
    ++g_selftest_calls;
}

static int g_drain_calls{};
static void drain_callback() noexcept
{
    ++g_drain_calls;
}

static PumpCore* g_active_pump{};
static void fixture_hook_entry(void* self, float delta_seconds, bool b_idle)
{
    assert(g_active_pump);
    static const TickCallbacks callbacks{&publish_callback, &selftest_callback, &drain_callback};
    drive_tick(*g_active_pump, callbacks, self, delta_seconds, b_idle);
}

// Classifier: the two real functions above (and any pointer explicitly
// planted as "code") are text; everything else is not.
static uint64_t g_extra_code_lo{}, g_extra_code_hi{};
static auto mock_in_text(uint64_t word) -> bool
{
    if (word == reinterpret_cast<uintptr_t>(&mock_original_tick)) return true;
    if (word == reinterpret_cast<uintptr_t>(&fixture_hook_entry)) return true;
    if (g_extra_code_lo && word >= g_extra_code_lo && word < g_extra_code_hi) return true;
    return false;
}

// A mock GEngine: pointer-sized vptr storage plus identity.
struct MockEngine
{
    void** vptr{};
};
static auto vptr_of(MockEngine& e) -> void** volatile*
{
    return reinterpret_cast<void** volatile*>(&e.vptr);
}

// Mock vtable allocation: [-2] offset-to-top, [-1] RTTI, then entries.
struct MockTable
{
    static constexpr size_t kMaxEntries = 128;
    void* words[kPrefixWords + kMaxEntries]{};
    MockTable()
    {
        words[0] = reinterpret_cast<void*>(0x1234); // offset-to-top sentinel
        words[1] = nullptr;                        // RTTI: null in this build
        for (size_t i = 0; i < kMaxEntries; ++i)
        {
            entry(i) = reinterpret_cast<void*>(&mock_original_tick);
        }
    }
    auto entry(size_t i) -> void*&
    {
        return words[kPrefixWords + i];
    }
    auto address_point() -> void**
    {
        return words + kPrefixWords;
    }
};

static auto run_dispatch(MockEngine& e, size_t slot, void* self, float delta, bool idle) -> void
{
    reinterpret_cast<TickFn>(e.vptr[slot])(self, delta, idle);
}

#define CHECK(cond)                                                                                                     \
    do                                                                                                                   \
    {                                                                                                                    \
        if (!(cond))                                                                                                     \
        {                                                                                                                \
            std::fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
            std::exit(1);                                                                                                \
        }                                                                                                                \
    } while (0)

static auto test_scan_bounds() -> void
{
    MockTable table{};
    // Entries 0..40 stay text (ctor default); 30 becomes a null reserved
    // slot (legal inside the extent); everything past 40 is non-code.
    table.entry(30) = nullptr;
    for (size_t i = 41; i < MockTable::kMaxEntries; ++i)
    {
        table.entry(i) = reinterpret_cast<void*>(0xDEAD0000ull + i);
    }
    FailReason reason{};
    const size_t count = PumpCore::scan_table(table.address_point(), MockTable::kMaxEntries, 40, mock_in_text, &reason);
    CHECK(reason == FailReason::None);
    CHECK(count == 41); // extent = highest text entry (40) + 1
    // Tick slot beyond the last code entry refuses.
    const size_t refused = PumpCore::scan_table(table.address_point(), MockTable::kMaxEntries, 42, mock_in_text, &reason);
    CHECK(refused == 0 && reason == FailReason::TickSlotOutOfRange);
    // A table with no code entries at all refuses.
    MockTable empty{};
    for (size_t i = 0; i < MockTable::kMaxEntries; ++i)
    {
        empty.entry(i) = reinterpret_cast<void*>(0xBAADF00Dull);
    }
    CHECK(PumpCore::scan_table(empty.address_point(), MockTable::kMaxEntries, 0, mock_in_text, &reason) == 0);
    CHECK(reason == FailReason::NoCodeEntries);
    // Null address point / zero bound refuse.
    CHECK(PumpCore::scan_table(nullptr, 8, 0, mock_in_text, &reason) == 0 && reason == FailReason::NullAddressPoint);
    CHECK(PumpCore::scan_table(table.address_point(), 0, 0, mock_in_text, &reason) == 0 && reason == FailReason::NullAddressPoint);
}

static auto test_arm_copy_and_swap() -> void
{
    MockTable table{};
    MockEngine engine{};
    engine.vptr = table.address_point();
    const size_t tick_slot = 98;
    // Entry 50 is garbage INSIDE the copied extent -> validation refusal.
    table.entry(50) = reinterpret_cast<void*>(0xCAFEBABEull);
    PumpCore pump{};
    FailReason reason{};
    CHECK(!pump.arm(&engine, tick_slot, reinterpret_cast<void*>(&fixture_hook_entry), MockTable::kMaxEntries, mock_in_text, &reason));
    CHECK(reason == FailReason::NonTextEntryInTable);
    CHECK(pump.copy_allocation == nullptr); // failed arm allocates nothing retained
    // Null entries inside the extent are legal reserved slots.
    table.entry(50) = nullptr;
    CHECK(pump.arm(&engine, tick_slot, reinterpret_cast<void*>(&fixture_hook_entry), MockTable::kMaxEntries, mock_in_text, &reason));
    CHECK(reason == FailReason::None);
    CHECK(pump.entry_count == MockTable::kMaxEntries); // entry 127 is the highest text entry
    // Private copy: distinct storage, 16-byte aligned, address point = alloc + 2.
    CHECK(pump.copy_allocation != table.address_point() - 2);
    CHECK((reinterpret_cast<uintptr_t>(pump.copy_allocation) & 0xF) == 0);
    CHECK(pump.copy_address_point() == pump.copy_allocation + 2);
    // Prefix preserved verbatim.
    CHECK(pump.copy_allocation[0] == reinterpret_cast<void*>(0x1234));
    CHECK(pump.copy_allocation[1] == nullptr);
    // Every entry bit-identical except the tick slot; nulls preserved.
    for (size_t i = 0; i < pump.entry_count; ++i)
    {
        if (i == tick_slot)
        {
            CHECK(pump.copy_address_point()[i] == reinterpret_cast<void*>(&fixture_hook_entry));
        }
        else
        {
            CHECK(pump.copy_address_point()[i] == table.entry(i));
        }
    }
    // Re-arm refusal (single swap per core).
    CHECK(!pump.arm(&engine, tick_slot, reinterpret_cast<void*>(&fixture_hook_entry), MockTable::kMaxEntries, mock_in_text, &reason));
    CHECK(reason == FailReason::AlreadyArmed);
    // Publish: single aligned store swaps the object vptr.
    CHECK(pump.publish_swap());
    CHECK(engine.vptr == pump.copy_address_point());
    // The original Tick entry was captured from the ORIGINAL table.
    CHECK(pump.original_tick_entry == reinterpret_cast<void*>(&mock_original_tick));
    // ---- the hook fires through a genuine dispatch ----
    g_active_pump = &pump;
    g_original_calls = 0;
    g_publish_calls = 0;
    g_selftest_calls = 0;
    g_drain_calls = 0;
    run_dispatch(engine, tick_slot, &engine, 1.5f, true);
    CHECK(g_original_calls == 1);
    CHECK(g_original_last_self == &engine);
    CHECK(g_original_last_delta == 1.5f);
    CHECK(g_original_last_idle == true);
    CHECK(g_publish_calls == 1);
    CHECK(g_selftest_calls == 1);
    CHECK(g_drain_calls == 1);
    CHECK(pump.tick_count == 1);
    // Second tick: identity already published -> no second publish/selftest.
    run_dispatch(engine, tick_slot, &engine, 0.25f, false);
    CHECK(g_original_calls == 2);
    CHECK(g_publish_calls == 1);
    CHECK(g_selftest_calls == 1);
    CHECK(g_drain_calls == 2);
    // ---- restore: the original is restorable and then hit DIRECTLY ----
    CHECK(pump.retire() == PumpCore::RestoreOutcome::Restored);
    CHECK(engine.vptr == table.address_point());
    g_original_calls = 0;
    g_drain_calls = 0;
    run_dispatch(engine, tick_slot, &engine, 2.0f, false);
    CHECK(g_original_calls == 1);
    CHECK(g_drain_calls == 0); // hook never fired
    CHECK(pump.retire() == PumpCore::RestoreOutcome::AlreadyRetired);
}

static auto test_publish_refusal_shape() -> void
{
    MockTable table{};
    MockEngine engine{};
    engine.vptr = table.address_point();
    PumpCore pump{};
    FailReason reason{};
    CHECK(pump.arm(&engine, 98, reinterpret_cast<void*>(&fixture_hook_entry), MockTable::kMaxEntries, mock_in_text, &reason));
    CHECK(pump.publish_swap());
    g_active_pump = &pump;
    g_publish_result = false;
    g_original_calls = 0;
    g_publish_calls = 0;
    g_selftest_calls = 0;
    g_drain_calls = 0;
    run_dispatch(engine, 98, &engine, 1.0f, false);
    // Refused publication: original still forwarded exactly once; no
    // self-test, no drain, nothing thrown.
    CHECK(g_original_calls == 1);
    CHECK(g_publish_calls == 1);
    CHECK(g_selftest_calls == 0);
    CHECK(g_drain_calls == 0);
    CHECK(pump.first_tick_aborted_count == 1);
    // Publication succeeds on a later tick; extras run then.
    g_publish_result = true;
    run_dispatch(engine, 98, &engine, 1.0f, false);
    CHECK(g_original_calls == 2);
    CHECK(g_publish_calls == 2);
    CHECK(g_selftest_calls == 1);
    CHECK(g_drain_calls == 1);
    g_publish_result = true;
}

static auto test_foreign_writer_never_clobbered() -> void
{
    MockTable table{};
    MockEngine engine{};
    engine.vptr = table.address_point();
    PumpCore pump{};
    FailReason reason{};
    CHECK(pump.arm(&engine, 98, reinterpret_cast<void*>(&fixture_hook_entry), MockTable::kMaxEntries, mock_in_text, &reason));
    CHECK(pump.publish_swap());
    // A foreign writer replaces the vptr with its own table.
    void** foreign = table.address_point() + 8;
    *vptr_of(engine) = foreign;
    CHECK(pump.retire() == PumpCore::RestoreOutcome::OwnershipLost);
    CHECK(engine.vptr == foreign); // never clobbered
    CHECK(pump.ownership_lost);
}

static auto test_narrow_ascii_copy() -> void
{
    const char16_t src[] = u"ByteProperty";
    char dst[8]{};
    // Truncation at cap: at most 7 chars + NUL.
    CHECK(narrow_ascii_copy(src, 64, dst, sizeof(dst)) == 7);
    CHECK(std::strcmp(dst, "BytePro") == 0);
    char full[16]{};
    CHECK(narrow_ascii_copy(src, 64, full, sizeof(full)) == 12);
    CHECK(std::strcmp(full, "ByteProperty") == 0);
    // Early NUL stops the copy.
    const char16_t prefixed[] = u"AB\0CD";
    char early[16]{};
    CHECK(narrow_ascii_copy(prefixed, 5, early, sizeof(early)) == 2);
    CHECK(std::strcmp(early, "AB") == 0);
    // Non-ascii unit downgraded to '?'.
    const char16_t mixed[] = {u'A', 0x00E9, u'B', 0};
    char down[8]{};
    CHECK(narrow_ascii_copy(mixed, 4, down, sizeof(down)) == 3);
    CHECK(std::strcmp(down, "A?B") == 0);
    // Degenerate caps never write.
    char none[1]{};
    CHECK(narrow_ascii_copy(src, 64, none, 0) == 0);
    CHECK(narrow_ascii_copy(src, 64, nullptr, 16) == 0);
}

int main()
{
    test_scan_bounds();
    test_arm_copy_and_swap();
    test_publish_refusal_shape();
    test_foreign_writer_never_clobbered();
    test_narrow_ascii_copy();
    std::printf("eu-vmt-pump: all checks passed\n");
    return 0;
}
