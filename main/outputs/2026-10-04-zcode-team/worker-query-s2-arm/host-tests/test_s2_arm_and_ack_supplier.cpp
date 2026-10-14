// Host tests for worker-query-s2-arm: the S2 arm/disarm gate wiring facts and
// the module_start RuntimeAckConfig supplier. Compiles the REAL canonical
// runtime_owner.cpp (bundle-01 bytes == composition-3 staged bytes, sha
// 54c56c49...) and the REAL vendored modforge_activation.c against the
// supplier's host-visible half. Nothing here touches a console, the network,
// or target execution.
#include "runtime_ack_supplier.hpp"

#include <NativeQueryHarness.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

namespace
{
    int g_failures = 0;

    void check(bool ok, const char* what)
    {
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++g_failures;
        }
        else
        {
            std::fprintf(stderr, "ok: %s\n", what);
        }
    }

    std::string slurp(const std::filesystem::path& p)
    {
        std::ifstream in(p, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::size_t count_of(const std::string& hay, const std::string& needle)
    {
        std::size_t n = 0, pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos) { ++n; pos += needle.size(); }
        return n;
    }

    // Blocks until request_stop, like the real backend contract ("initialize
    // blocks until shutdown"); ready() drives the real RuntimeOwner's
    // engine-ready acknowledgment.
    struct StubBackend final : modbridge::native::RuntimeBackend
    {
        std::atomic<int> initialized{0};
        std::atomic<bool> stop_requested{false};
        void initialize(std::function<void()> ready) override
        {
            ++initialized;
            ready();
            while (!stop_requested.load(std::memory_order_acquire))
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        void request_stop() noexcept override { stop_requested.store(true, std::memory_order_release); }
    };

    std::filesystem::path scratch(const char* tag)
    {
        auto root = std::filesystem::temp_directory_path()
                  / (std::string("s2arm-") + tag + "-" + std::to_string(getpid()));
        std::filesystem::create_directories(root);
        return root;
    }
}

using modbridge::native::MakeRuntimeAckConfig;
using modbridge::native::RuntimeAckConfig;
using modbridge::native::RuntimeAckSink;
using modbridge::native::RuntimeOwner;
using modbridge::native::StopRuntimeWithAck;
namespace QH = RC::Platform::QueryHarness;

void test_ack_config_birth()
{
    const long self = getpid();
    const RuntimeAckConfig cfg = MakeRuntimeAckConfig("/data/modbridge/PPSA33286");
    check(cfg.status_root == std::filesystem::path("/data/modbridge/PPSA33286"),
          "status_root is the EXACT mfa_config::root string");
    check(cfg.pid == self, "pid comes from getpid()");

    const auto wall_ms = std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    check(cfg.now_ms > 0, "now_ms nonzero (clock_gettime succeeded)");
    check(cfg.now_ms <= wall_ms, "monotonic ms <= wall ms (boot-referenced clock sanity)");
    check(cfg.now_ms >= 1000ull, "monotonic ms is a plausible uptime (>= 1s since boot)");

    const RuntimeAckConfig second = MakeRuntimeAckConfig("/data/modbridge/PPSA33286");
    check(second.now_ms >= cfg.now_ms, "now_ms non-decreasing (CLOCK_MONOTONIC)");
    check(second.pid == self, "pid stable across births");

    check(MakeRuntimeAckConfig("").status_root.empty(), "empty mod_root yields empty status_root");
    check(MakeRuntimeAckConfig(nullptr).status_root.empty(), "null mod_root yields empty status_root");
}

void test_sink_file_chain()
{
    const auto root = scratch("sink");
    const RuntimeAckConfig cfg = MakeRuntimeAckConfig(root.string().c_str());
    auto sink = RuntimeAckSink::Open(cfg);
    check(sink != nullptr, "RuntimeAckSink::Open returns a sink");
    sink->activated();
    sink->engine_ready();
    sink->mods_started();

    const std::string body = slurp(root / "runtime-status.txt");
    check(body.rfind("modforge-runtime-status\t1\n", 0) == 0, "status file header present");
    check(body.find("session\t" + std::to_string(cfg.pid) + "\t" + std::to_string(cfg.now_ms)) != std::string::npos,
          "session line carries the supplied pid and now_ms");
    check(body.find("\tactivated\t0\t") != std::string::npos, "activated line recorded");
    check(body.find("\tengine-ready\t0\t") != std::string::npos, "engine-ready line recorded");
    check(body.find("\tmods-started\t0\t") != std::string::npos, "mods-started line recorded");
    check(count_of(body, "\tstopped\t") == 0, "no stopped line before stop");

    sink->stopped();
    sink->stopped();  // T2/A3: at most one stopped line per session
    const std::string after = slurp(root / "runtime-status.txt");
    check(count_of(after, "\tstopped\t") == 1, "exactly one stopped line after double stop");
    sink->close();
    std::filesystem::remove_all(root);
}

void test_owner_stop_with_ack()
{
    const auto root = scratch("owner");
    auto sink = RuntimeAckSink::Open(MakeRuntimeAckConfig(root.string().c_str()));
    auto heap_owner = std::make_unique<RuntimeOwner>(std::make_unique<StubBackend>(), sink);
    check(heap_owner->start(), "RuntimeOwner::start on stub backend");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // let init thread run ready()

    const bool stopped = StopRuntimeWithAck(heap_owner);
    check(stopped, "StopRuntimeWithAck returns stop() result");
    check(heap_owner == nullptr, "StopRuntimeWithAck destroys the owner");
    check(StopRuntimeWithAck(heap_owner) == false, "StopRuntimeWithAck on empty owner is false");

    const std::string body = slurp(root / "runtime-status.txt");
    // engine-ready is emitted by the REAL UE4SS backend (ue4ss_backend.cpp) when
    // the actual engine callback fires; the stub backend cannot emit it. The
    // direct sink test above covers that line's format.
    check(body.find("\tengine-ready\t0\t") == std::string::npos, "stub backend emits no engine-ready (backend-owned line)");
    check(count_of(body, "\tstopped\t") == 1, "exactly one stopped line from stop+destroy");
    check(body.find("\tfailed\t") == std::string::npos, "no failed line on the clean path");
    std::filesystem::remove_all(root);
}

void test_gate_arm_line_facts()
{
    // Mirrors the EXACT patched initializer arm-line field expression: casts,
    // storage Unknown (validated profile supplies no storage claim),
    // provenance label naming the publish call.
    QH::Global().ResetForHostTest();
    QH::EngineBindings bindings;
    bindings.is_game_thread_initialized = []() noexcept { return true; };
    bindings.is_in_game_thread_raw = []() noexcept { return true; };
    bindings.game_thread_id_key = []() noexcept { return 42ull; };
    bindings.read_num_elements = []() noexcept { return 25000; };
    bindings.count_source_kind = QH::CountSourceKind::Synthetic;
    bindings.storage_layout = QH::StorageLayout::Unknown;
    QH::Global().InstallEngineBindings(bindings);

    QH::Global().ArmLayoutGate({static_cast<uint32_t>(5), static_cast<uint32_t>(6), static_cast<uint8_t>(true ? 1 : 0), static_cast<uint8_t>(false ? 1 : 0), QH::StorageLayout::Unknown, "PublishStagedNativeLayoutMaps"});

    check(QH::Global().OnEngineTickDispatch() == QH::QueryStatus::Pending,
          "first accepted dispatch stages the first count");
    check(QH::Global().OnEngineTickDispatch() == QH::QueryStatus::Completed,
          "second accepted dispatch completes the record");
    QH::QueryResultRecord record;
    check(QH::Global().TrySnapshot(record), "terminal record snapshot");
    check(record.count_source == QH::CountSourceKind::Synthetic, "record source fact");
    check(record.storage == QH::StorageLayout::Unknown, "record storage fact (Unknown)");
    check(record.engine_major == 5 && record.engine_minor == 6, "record engine identity facts");
    check(record.axes_non_shipping == 1 && record.axes_stats == 0, "record axis facts (0/1)");
    check(std::string(record.gate_source) == "PublishStagedNativeLayoutMaps",
          "record provenance label is the publish call");
    check(record.count_first == 25000 && record.count_second == 25000,
          "both counts from consecutive accepted ticks");
    QH::Global().ResetForHostTest();
}

void test_gate_disarm_on_throw_semantics()
{
    QH::Global().ResetForHostTest();
    QH::EngineBindings bindings;
    bindings.is_game_thread_initialized = []() noexcept { return true; };
    bindings.is_in_game_thread_raw = []() noexcept { return true; };
    bindings.game_thread_id_key = []() noexcept { return 7ull; };
    bindings.read_num_elements = []() noexcept { return 20000; };
    bindings.count_source_kind = QH::CountSourceKind::Synthetic;
    bindings.storage_layout = QH::StorageLayout::Unknown;
    QH::Global().InstallEngineBindings(bindings);
    QH::Global().ArmLayoutGate({5u, 6u, 1, 0, QH::StorageLayout::Unknown, "PublishStagedNativeLayoutMaps"});

    check(QH::Global().OnEngineTickDispatch() == QH::QueryStatus::Pending,
          "armed gate: first count staged");

    // The exact catch-all body as inserted in ScanGame()/Initialize().
    QH::Global().DisarmLayoutGate();
    check(QH::Global().OnEngineTickDispatch() == QH::QueryStatus::RefusedLayoutGateClosed,
          "post-disarm dispatch refused RefusedLayoutGateClosed");
    check(QH::Global().SnapshotDispatchCounters().refused_layout_gate_closed == 1,
          "refusal counted (negative-evidence channel)");
    QH::QueryResultRecord record;
    check(!QH::Global().TrySnapshot(record), "no partial record published after disarm");
    check(QH::Global().phase() == QH::Phase::Idle, "staged count discarded, phase back to Idle");

    // Disarm cleared the armed flag: a later legitimate arm succeeds, and a
    // double arm still refuses with std::logic_error.
    bool rearmed = true;
    try
    {
        QH::Global().ArmLayoutGate({5u, 6u, 1, 0, QH::StorageLayout::Unknown, "PublishStagedNativeLayoutMaps"});
    }
    catch (const std::logic_error&)
    {
        rearmed = false;
    }
    check(rearmed, "re-arm after disarm succeeds (disarm is not sticky)");
    bool double_refused = false;
    try
    {
        QH::Global().ArmLayoutGate({5u, 6u, 1, 0, QH::StorageLayout::Unknown, "PublishStagedNativeLayoutMaps"});
    }
    catch (const std::logic_error&)
    {
        double_refused = true;
    }
    check(double_refused, "double arm refused with std::logic_error");
    QH::Global().ResetForHostTest();
}

int main()
{
    test_ack_config_birth();
    test_sink_file_chain();
    test_owner_stop_with_ack();
    test_gate_arm_line_facts();
    test_gate_disarm_on_throw_semantics();
    if (g_failures)
    {
        std::fprintf(stderr, "HOST_TESTS_FAILED: %d failures\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "HOST_TESTS_PASSED: s2-arm gate + ack supplier\n");
    return 0;
}
