// Host tests for NativeQueryHarness (no engine code, no network, no console).
// Drives synthetic identity/counts through BOTH GUObjectArray storage-layout
// claims and BOTH S2 axes combinations, every refusal path, the no-Tick-leak
// rule, terminal immutability, and snapshot/thread safety.
//
// Build: clang++ -std=c++20 -Wall -Wextra -DNATIVE_QUERY_HARNESS_HOST_TEST \
//          ../src/NativeQueryHarness.cpp host_test.cpp -o host_test_bin -pthread

#include "../src/NativeQueryHarness.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

using RC::Platform::QueryHarness::CountSourceKind;
using RC::Platform::QueryHarness::EngineBindings;
using RC::Platform::QueryHarness::LayoutGateFacts;
using RC::Platform::QueryHarness::MonotonicVerdict;
using RC::Platform::QueryHarness::Phase;
using RC::Platform::QueryHarness::QueryResultRecord;
using RC::Platform::QueryHarness::QueryStatus;
using RC::Platform::QueryHarness::StorageLayout;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond)                                                                              \
    do                                                                                           \
    {                                                                                            \
        ++g_checks;                                                                              \
        if (!(cond))                                                                             \
        {                                                                                        \
            ++g_failures;                                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                          \
        }                                                                                        \
    } while (false)

// Synthetic engine world: the harness sees exactly what we claim.
struct World
{
    bool identity_known{false};
    bool in_game_thread{false};
    uint64_t key{0};
    int32_t counts[8]{};
    size_t reads{0};
};
static World g_world{};
static World* g_probe_target{&g_world};

static bool probe_known() noexcept { return g_probe_target->identity_known; }
static bool probe_in_game_thread() noexcept { return g_probe_target->in_game_thread; }
static uint64_t probe_key() noexcept { return g_probe_target->identity_known ? g_probe_target->key : 0; }
static int32_t probe_read() { return g_probe_target->counts[g_probe_target->reads < 8 ? g_probe_target->reads++ : 7]; }

static EngineBindings synthetic_bindings(StorageLayout storage)
{
    EngineBindings bindings{};
    bindings.is_game_thread_initialized = &probe_known;
    bindings.is_in_game_thread_raw = &probe_in_game_thread;
    bindings.game_thread_id_key = &probe_key;
    bindings.read_num_elements = &probe_read;
    bindings.count_source_kind = CountSourceKind::Synthetic;
    bindings.storage_layout = storage;
    return bindings;
}

static LayoutGateFacts gate_facts(uint32_t major, uint32_t minor, uint8_t non_shipping, uint8_t stats,
                                  StorageLayout storage, const char* source)
{
    LayoutGateFacts facts{};
    facts.engine_major = major;
    facts.engine_minor = minor;
    facts.non_shipping = non_shipping;
    facts.stats = stats;
    facts.storage = storage;
    std::snprintf(facts.source, sizeof(facts.source), "%s", source);
    return facts;
}

// Dispatch once and prove the refusal contract: distinct status, phase back at
// Idle, no terminal snapshot, no pending work.
static void expect_refusal(RC::Platform::QueryHarness::Harness& harness, QueryStatus expected)
{
    QueryResultRecord record{};
    const auto counters_before = harness.SnapshotDispatchCounters();
    const QueryStatus status = harness.OnEngineTickDispatch();
    CHECK(status == expected);
    CHECK(harness.phase() == Phase::Idle);
    CHECK(!harness.TrySnapshot(record));
    const auto counters = harness.SnapshotDispatchCounters();
    CHECK(counters.ticks_dispatched == counters_before.ticks_dispatched + 1);
    CHECK(counters.ticks_accepted == counters_before.ticks_accepted);
    CHECK(counters.last_refusal == expected);
    switch (expected)
    {
    case QueryStatus::RefusedBindingsAbsent: CHECK(counters.refused_bindings_absent == counters_before.refused_bindings_absent + 1); break;
    case QueryStatus::RefusedLayoutGateClosed: CHECK(counters.refused_layout_gate_closed == counters_before.refused_layout_gate_closed + 1); break;
    case QueryStatus::RefusedIdentityUnknown: CHECK(counters.refused_identity_unknown == counters_before.refused_identity_unknown + 1); break;
    case QueryStatus::RefusedWrongThread: CHECK(counters.refused_wrong_thread == counters_before.refused_wrong_thread + 1); break;
    default: CHECK(false && "not a refusal status");
    }
}

static void fresh_world(bool known = true, bool game_thread = true, uint64_t key = 0x56000001u)
{
    g_world = World{};
    g_world.identity_known = known;
    g_world.in_game_thread = game_thread;
    g_world.key = key;
}

int main()
{
    using RC::Platform::QueryHarness::Global;

    // --- 1. Refusal: bindings absent -------------------------------------
    {
        auto& h = Global();
        h.ResetForHostTest();
        expect_refusal(h, QueryStatus::RefusedBindingsAbsent);
        expect_refusal(h, QueryStatus::RefusedBindingsAbsent); // repeatable, still no leak
    }
    // --- 2. Install guards -------------------------------------------------
    {
        auto& h = Global();
        h.ResetForHostTest();
        bool threw = false;
        try { h.InstallDefaultEngineBindings(); } catch (...) { threw = true; }
        CHECK(threw); // host build must refuse the default engine binding (no silent stub)
        EngineBindings bad{};
        threw = false;
        try { h.InstallEngineBindings(bad); } catch (...) { threw = true; }
        CHECK(threw); // null probes refused
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::Unknown));
        threw = false;
        try { h.InstallEngineBindings(synthetic_bindings(StorageLayout::Unknown)); } catch (...) { threw = true; }
        CHECK(threw); // double install refused
    }
    // --- 3. Refusal: S2 layout gate closed --------------------------------
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        expect_refusal(h, QueryStatus::RefusedLayoutGateClosed);
        bool threw = false;
        try { h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps")); } catch (...) { threw = true; }
        CHECK(!threw);
        threw = false;
        try { h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps")); } catch (...) { threw = true; }
        CHECK(threw); // double arm refused
        threw = false;
        try { h.ArmLayoutGate(gate_facts(0, 6, 1, 0, StorageLayout::Unknown, "x")); } catch (...) { threw = true; }
        CHECK(threw); // unresolved identity refused
        threw = false;
        try { h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::Unknown, "")); } catch (...) { threw = true; }
        CHECK(threw); // empty provenance refused
    }
    // --- 4/5. Refusals: identity unknown, wrong thread ---------------------
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world(false, false, 0);
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        expect_refusal(h, QueryStatus::RefusedIdentityUnknown); // identity not published yet
        g_world.identity_known = true;
        g_world.in_game_thread = false; // e.g. init/update thread attempt (design 2.2)
        expect_refusal(h, QueryStatus::RefusedWrongThread);
        g_world.in_game_thread = true; // publish happened, genuine Tick from here on
    }
    // --- 6. Happy path, chunked storage claim, axes (non_shipping=1,stats=0)
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        g_world.counts[0] = 12000;
        g_world.counts[1] = 12000;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending); // first count staged
        CHECK(h.phase() == Phase::AwaitSecondCount);
        QueryResultRecord record{};
        CHECK(!h.TrySnapshot(record)); // never a partial record mid-sequence
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed);
        CHECK(h.phase() == Phase::Terminal);
        CHECK(h.TrySnapshot(record));
        CHECK(record.status == QueryStatus::Completed);
        CHECK(record.counts_recorded == 2);
        CHECK(record.count_first == 12000);
        CHECK(record.count_second == 12000);
        CHECK(record.count_delta == 0);
        CHECK(record.monotonic == MonotonicVerdict::Equal);
        CHECK(record.identity_checks == 2);
        CHECK(record.engine_major == 5 && record.engine_minor == 6);
        CHECK(record.axes_non_shipping == 1 && record.axes_stats == 0);
        CHECK(record.storage == StorageLayout::ChunkedArray);
        CHECK(record.count_source == CountSourceKind::Synthetic);
        CHECK(record.game_thread_id_key == 0x56000001u);
        CHECK(record.threshold == 10000);
        CHECK(record.ticks_dispatched == 2 && record.ticks_accepted == 2);
        CHECK(record.refusals_between_counts == 0);
        CHECK(std::strcmp(record.gate_source, "PublishStagedNativeLayoutMaps") == 0);
        char line[RC::Platform::QueryHarness::kJsonLineCapacity];
        const size_t length = RC::Platform::QueryHarness::FormatRecordJsonLine(record, line);
        CHECK(length > 0 && length < sizeof(line));
        CHECK(std::strstr(line, "\"status\":\"completed\"") != nullptr);
        CHECK(std::strstr(line, "\"engine\":\"5.6\"") != nullptr);
        CHECK(std::strstr(line, "\"monotonic\":\"equal\"") != nullptr);
        CHECK(std::strstr(line, "\"storage\":\"chunked_array\"") != nullptr);
        CHECK(std::strstr(line, "\"game_thread_id_key\":\"0x0000000056000001\"") != nullptr);
        CHECK(std::strstr(line, "\"gate_source\":\"PublishStagedNativeLayoutMaps\"") != nullptr);
        std::printf("jsonl> %s\n", line);
    }
    // --- 7. Happy path, fixed storage claim, axes (non_shipping=0,stats=1) --
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world(true, true, 0x56000002u);
        g_world.counts[0] = 12000;
        g_world.counts[1] = 12001;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::FixedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 0, 1, StorageLayout::FixedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed);
        QueryResultRecord record{};
        CHECK(h.TrySnapshot(record));
        CHECK(record.monotonic == MonotonicVerdict::NonDecreasing);
        CHECK(record.count_delta == 1);
        CHECK(record.storage == StorageLayout::FixedArray);
        CHECK(record.axes_non_shipping == 0 && record.axes_stats == 1);
    }
    // --- 8. Honest verdict: decreasing counts are recorded, not masked ------
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        g_world.counts[0] = 12000;
        g_world.counts[1] = 11999;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed);
        QueryResultRecord record{};
        CHECK(h.TrySnapshot(record));
        CHECK(record.monotonic == MonotonicVerdict::Decreasing);
        CHECK(record.count_delta == -1);
    }
    // --- 9. Inconclusive at the first count (== 10000 is not > 10000) -------
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        g_world.counts[0] = 10000;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::InconclusiveBelowThreshold);
        CHECK(h.phase() == Phase::Terminal);
        QueryResultRecord record{};
        CHECK(h.TrySnapshot(record));
        CHECK(record.counts_recorded == 1);
        CHECK(record.count_first == 10000);
        CHECK(record.count_second == 0);
        CHECK(record.monotonic == MonotonicVerdict::Unknown);
        CHECK(h.OnEngineTickDispatch() == QueryStatus::InconclusiveBelowThreshold); // terminal sticks
    }
    // --- 10. Inconclusive at the second count (full honest observation) -----
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        g_world.counts[0] = 12000;
        g_world.counts[1] = 9999;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        CHECK(h.OnEngineTickDispatch() == QueryStatus::InconclusiveBelowThreshold);
        QueryResultRecord record{};
        CHECK(h.TrySnapshot(record));
        CHECK(record.counts_recorded == 2);
        CHECK(record.count_first == 12000 && record.count_second == 9999);
        CHECK(record.count_delta == -2001);
        CHECK(record.monotonic == MonotonicVerdict::Decreasing);
    }
    // --- 11. Wrong-thread refusal BETWEEN the two reads: staged count reset --
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        g_world.counts[0] = 12000; // staged, then discarded
        g_world.counts[1] = 12000; // fresh first count after reset
        g_world.counts[2] = 12000; // consecutive second count
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        CHECK(h.phase() == Phase::AwaitSecondCount);
        g_world.in_game_thread = false; // non-game-thread dispatch attempt between the ticks
        expect_refusal(h, QueryStatus::RefusedWrongThread);
        CHECK(h.SnapshotDispatchCounters().staged_resets == 1);
        g_world.in_game_thread = true;
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending); // reads counts[1]
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed); // reads counts[2]
        QueryResultRecord record{};
        CHECK(h.TrySnapshot(record));
        CHECK(record.count_first == g_world.counts[1] && record.count_second == g_world.counts[2]);
        CHECK(record.refusals_between_counts == 0); // consecutive pair by construction
        CHECK(record.ticks_dispatched == 4 && record.ticks_accepted == 3);
        const auto counters = h.SnapshotDispatchCounters();
        CHECK(counters.refused_wrong_thread == 1);
    }
    // --- 12. Published identity key flip between ticks: refuse and reset ----
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world(true, true, 0x5600000Au);
        g_world.counts[0] = 12000;
        g_world.counts[1] = 12000;
        g_world.counts[2] = 12000;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        g_world.key = 0x5600000Bu; // identity changed under us
        expect_refusal(h, QueryStatus::RefusedWrongThread);
        g_world.key = 0x5600000Au;
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed);
        QueryResultRecord record{};
        CHECK(h.TrySnapshot(record));
        CHECK(record.game_thread_id_key == 0x5600000Au);
        CHECK(record.identity_checks == 2);
    }
    // --- 13. Gate disarmed between ticks: refusal + reset, re-arm completes --
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        g_world.counts[0] = 12000;
        g_world.counts[1] = 12000;
        g_world.counts[2] = 12000;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        h.DisarmLayoutGate();
        expect_refusal(h, QueryStatus::RefusedLayoutGateClosed);
        CHECK(h.SnapshotDispatchCounters().staged_resets == 1);
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed);
    }
    // --- 14. Terminal immutability: later ticks change nothing --------------
    {
        auto& h = Global();
        h.ResetForHostTest();
        fresh_world();
        g_world.counts[0] = 12000;
        g_world.counts[1] = 12000;
        h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
        h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Pending);
        CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed);
        QueryResultRecord first{};
        CHECK(h.TrySnapshot(first));
        for (int i = 0; i < 5; ++i) CHECK(h.OnEngineTickDispatch() == QueryStatus::Completed);
        QueryResultRecord again{};
        CHECK(h.TrySnapshot(again));
        CHECK(std::memcmp(&first, &again, sizeof(first)) == 0); // record frozen
        const auto counters = h.SnapshotDispatchCounters();
        CHECK(counters.ticks_dispatched == 7);
        CHECK(counters.ticks_accepted == 2); // no further reads after terminal
        CHECK(g_world.reads == 2);
    }
    // --- 15. ResetForHostTest returns pristine state ------------------------
    {
        auto& h = Global();
        h.ResetForHostTest();
        CHECK(h.phase() == Phase::Idle);
        QueryResultRecord record{};
        CHECK(!h.TrySnapshot(record));
        const auto counters = h.SnapshotDispatchCounters();
        CHECK(counters.ticks_dispatched == 0 && counters.ticks_accepted == 0);
        CHECK(counters.last_refusal == QueryStatus::Pending);
    }
    // --- 16. Concurrent snapshots during a writer lifecycle -----------------
    {
        std::atomic<bool> stop{false};
        std::atomic<int> terminal_snapshots{0};
        std::atomic<int> torn_or_invalid{0};
        auto reader = [&]()
        {
            QueryResultRecord record{};
            while (!stop.load(std::memory_order_relaxed))
            {
                if (Global().TrySnapshot(record))
                {
                    const bool valid = record.magic == RC::Platform::QueryHarness::kRecordMagic &&
                                       record.record_size == sizeof(QueryResultRecord) &&
                                       (record.status == QueryStatus::Completed ||
                                        record.status == QueryStatus::InconclusiveBelowThreshold) &&
                                       record.counts_recorded >= 1;
                    if (valid) terminal_snapshots.fetch_add(1, std::memory_order_relaxed);
                    else torn_or_invalid.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::yield(); // poll like the controller driver, don't spin-starve the writer
            }
        };
        std::thread reader_thread{reader};
        for (int iteration = 0; iteration < 200; ++iteration)
        {
            auto& h = Global();
            h.ResetForHostTest();
            fresh_world();
            g_world.counts[0] = 12000 + iteration;
            g_world.counts[1] = 12000 + iteration;
            h.InstallEngineBindings(synthetic_bindings(StorageLayout::ChunkedArray));
            h.ArmLayoutGate(gate_facts(5, 6, 1, 0, StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"));
            h.OnEngineTickDispatch();
            h.OnEngineTickDispatch();
            CHECK(h.phase() == Phase::Terminal);
            // Keep the terminal record visible for a bounded, realistic
            // window (the controller driver polls at intervals) so the
            // concurrent reader can genuinely observe it.
            for (int pause = 0; pause < 4; ++pause) std::this_thread::yield();
        }
        stop.store(true, std::memory_order_relaxed);
        reader_thread.join();
        CHECK(torn_or_invalid.load() == 0); // every visible record was complete and terminal
        CHECK(terminal_snapshots.load() > 0); // the channel actually delivered records
        std::printf("concurrent> terminal_snapshots=%d torn_or_invalid=%d\n",
                    terminal_snapshots.load(), torn_or_invalid.load());
        Global().ResetForHostTest();
    }

    std::printf("%s: %d checks, %d failures\n", g_failures == 0 ? "HOST_TESTS_PASSED" : "HOST_TESTS_FAILED",
                g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
