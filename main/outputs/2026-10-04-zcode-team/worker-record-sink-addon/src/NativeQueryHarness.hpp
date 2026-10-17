#pragma once

// NativeQueryHarness — harmless-query milestone probe (MILESTONE-TEST-DESIGN.md,
// primary candidate): read-only GUObjectArray element-count query executed on
// the first genuine game-thread Tick, twice on consecutive accepted Ticks.
//
// Plug-in unit contract (ue4ss-native-tick-file-owner-1/build_runtime.py
// --extra-unit): this header is self-contained (standard library only). The
// engine seams are INJECTABLE function pointers installed once by controller
// wiring BEFORE runtime start; the default engine binding lives in
// NativeQueryHarness.cpp and resolves RC::Unreal identity probes plus
// UObjectArray::GetNumElements inside the composed object. Host tests compile
// the same two files with -DNATIVE_QUERY_HARNESS_HOST_TEST and inject
// synthetic identity/counts, so no host build ever links engine code.
//
// Honesty rules mirrored from the milestone design:
//  * A refusal NEVER emits a partial query record and NEVER leaves pending
//    Tick work: the phase returns to Idle (a staged first count is discarded
//    so both recorded counts always come from consecutive accepted Ticks).
//  * A count that is not strictly greater than kMinimumPlausibleElementCount
//    (10000) is INCONCLUSIVE-NOT-REFUTED (the initializer's own post-
//    construction heuristic; UnrealInitializer.cpp waits for >= 10000 — the
//    brief's strict > is used here and 10000 exactly is inconclusive).
//  * The Tick path performs no allocation and no I/O; the JSONL formatter is
//    an off-thread helper the controller's driver runs on a snapshot.
//  * Nothing here proves engine identity, layout correctness or target
//    execution; acceptance requires the rung-1->2 evidence in REPORT.md.

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace RC::Platform::QueryHarness
{
    inline constexpr uint32_t kRecordMagic = 0x4E514831u; // "NQH1"
    inline constexpr uint32_t kMinimumPlausibleElementCount = 10000;
    inline constexpr size_t kSourceLabelCapacity = 32;
    inline constexpr size_t kJsonLineCapacity = 640;
    inline constexpr int kSnapshotMaxRetries = 128;

    // Terminal outcome of the query sequence. Values 1..4 are dispatch
    // refusals (diagnostic-counter channel, non-terminal for the harness so a
    // wrong-thread probe does not poison the later genuine Tick); 5..6 are
    // terminal record states; 0 means no terminal record exists yet.
    enum class QueryStatus : uint8_t
    {
        Pending = 0,
        RefusedBindingsAbsent = 1,
        RefusedLayoutGateClosed = 2,
        RefusedIdentityUnknown = 3,
        RefusedWrongThread = 4,
        InconclusiveBelowThreshold = 5,
        Completed = 6,
    };
    inline constexpr uint32_t kRefusalStatusCount = 4;

    enum class MonotonicVerdict : uint8_t
    {
        Unknown = 0,
        Equal = 1,
        NonDecreasing = 2,
        Decreasing = 3,
    };

    // Which GUObjectArray storage layout the validated profile claims the
    // count traverses. INFORMATIONAL: the fixed/chunked dispatch lives inside
    // the engine's TUObjectArray::GetNumElements() driven by the resolved
    // engine version, never by this harness.
    enum class StorageLayout : uint8_t
    {
        Unknown = 0,
        FixedArray = 1,
        ChunkedArray = 2,
    };

    enum class CountSourceKind : uint8_t
    {
        Unbound = 0,
        EngineUObjectArray = 1,
        Synthetic = 2,
    };

    // S2 gate facts, armed by controller wiring only after BOTH seam
    // resolutions succeeded (S1 admission passed and
    // PublishStagedNativeLayoutMaps returned). Deviation from the brief,
    // recorded in REPORT.md: NativeLayoutBinding.hpp exposes no publication
    // query, so the gate cannot be re-checked at Tick time; arming in the
    // same serialized startup path is the honest substitute.
    struct LayoutGateFacts
    {
        uint32_t engine_major{};
        uint32_t engine_minor{};
        uint8_t non_shipping{}; // 0/1
        uint8_t stats{};        // 0/1
        StorageLayout storage{StorageLayout::Unknown};
        char source[kSourceLabelCapacity]{}; // e.g. "PublishStagedNativeLayoutMaps"
    };

    // One complete, fixed-size result record. Published through a single-
    // writer seqlock; a snapshot is only handed out for TERMINAL records
    // (Inconclusive/Completed), so a visible record is never partial.
    struct QueryResultRecord
    {
        uint32_t magic{kRecordMagic};
        uint32_t record_size{sizeof(QueryResultRecord)};
        QueryStatus status{QueryStatus::Pending};
        MonotonicVerdict monotonic{MonotonicVerdict::Unknown};
        StorageLayout storage{StorageLayout::Unknown};
        CountSourceKind count_source{CountSourceKind::Unbound};
        uint8_t counts_recorded{0};  // 0, 1 or 2
        uint8_t identity_checks{0};  // accepted ticks whose identity key matched
        uint8_t reserved_a[2]{};
        uint32_t engine_major{};
        uint32_t engine_minor{};
        uint32_t axes_non_shipping{};
        uint32_t axes_stats{};
        uint32_t threshold{kMinimumPlausibleElementCount};
        uint32_t reserved_b{};
        uint64_t game_thread_id_key{}; // stable fold of the published identity, 0 == unknown
        uint64_t ticks_dispatched{};
        uint64_t ticks_accepted{};
        uint64_t refusals_between_counts{};
        int64_t count_first{};
        int64_t count_second{};
        int64_t count_delta{};
        char gate_source[kSourceLabelCapacity]{};
    };
    static_assert(std::is_standard_layout_v<QueryResultRecord>, "record must be a POD");
    static_assert(std::is_trivially_copyable_v<QueryResultRecord>, "record must snapshot by value");
    static_assert(sizeof(QueryResultRecord) == 128, "fixed-size record");

    // Diagnostic dispatch counters (design 2.2: refusal evidence lives in a
    // debug counter channel). Monotonic relaxed loads; NOT a coherent instant.
    struct DispatchCounters
    {
        uint64_t ticks_dispatched{};
        uint64_t ticks_accepted{};
        uint64_t refused_bindings_absent{};
        uint64_t refused_layout_gate_closed{};
        uint64_t refused_identity_unknown{};
        uint64_t refused_wrong_thread{};
        uint64_t staged_resets{}; // first counts discarded by interleaved refusals
        QueryStatus last_refusal{QueryStatus::Pending};
        uint64_t last_refusal_sequence{};
    };

    enum class Phase : uint8_t
    {
        Idle = 0,            // no staged count; nothing pending
        AwaitSecondCount = 1, // first count staged, waiting for the consecutive Tick
        Terminal = 2,        // Inconclusive or Completed record published
    };

    // Engine seams. Installed exactly once, before any Tick can dispatch.
    // The Tick path only ever calls through these pointers.
    struct EngineBindings
    {
        bool (*is_game_thread_initialized)() noexcept{nullptr}; // RC::Unreal::IsGameThreadInitialized
        bool (*is_in_game_thread_raw)() noexcept{nullptr};      // RC::Unreal::IsInGameThreadRaw
        uint64_t (*game_thread_id_key)() noexcept{nullptr};     // fold of RC::Unreal::GetGameThreadIdRaw, 0 == unknown
        int32_t (*read_num_elements)();                          // RC::Unreal::UObjectArray::GetNumElements (the sole engine read)
        CountSourceKind count_source_kind{CountSourceKind::Unbound};
        StorageLayout storage_layout{StorageLayout::Unknown};
    };

    // Process-lifetime harness handle for the controller wiring and the Tick
    // dispatch route. First call performs constant initialization only.
    class Harness final
    {
    public:
        Harness() = default;
        Harness(const Harness&) = delete;
        auto operator=(const Harness&) -> Harness& = delete;

        // ---- wiring time (serialized startup, before ticks) ----
        // Throws std::logic_error on null members or double install.
        auto InstallEngineBindings(const EngineBindings& bindings) -> void;
        // Convenience for target wiring (absent in host-test builds).
        auto InstallDefaultEngineBindings() -> void;
        // Throws std::logic_error on double arm; DisarmLayoutGate/ResetForHostTest clear it.
        auto ArmLayoutGate(const LayoutGateFacts& facts) -> void;
        auto DisarmLayoutGate() noexcept -> void;

        // ---- game-thread Tick entry (noexcept, no allocation, no I/O) ----
        // Order of checks: terminal -> bindings -> layout gate -> identity
        // known -> game thread -> count read. Each refusal is distinct, is
        // counted, and leaves the phase Idle with no staged work.
        auto OnEngineTickDispatch() noexcept -> QueryStatus;

        // ---- controller side (any thread) ----
        // True only for a TERMINAL record (Inconclusive/Completed).
        [[nodiscard]] auto TrySnapshot(QueryResultRecord& out) const noexcept -> bool;
        [[nodiscard]] auto SnapshotDispatchCounters() const noexcept -> DispatchCounters;
        [[nodiscard]] auto phase() const noexcept -> Phase;
        // Host tests / teardown only: full reset to pristine Idle.
        auto ResetForHostTest() noexcept -> void;

    private:
        auto record_refusal(QueryStatus refusal) noexcept -> void;
        auto publish_terminal(QueryStatus status, uint8_t counts_recorded, int64_t first, int64_t second,
                              MonotonicVerdict verdict, uint64_t key, uint8_t identity_checks) noexcept -> void;
        auto begin_write() noexcept -> uint32_t;
        auto end_write(uint32_t version) noexcept -> void;

        // Seqlock: single writer (game thread), bounded-retry readers.
        mutable std::atomic<uint32_t> version_{0};
        QueryResultRecord record_{};
        // Staged first count lives outside the published record so refusals
        // can discard it without ever exposing a partial record.
        int64_t staged_count_{0};
        uint64_t staged_key_{0};
        uint64_t staged_tick_{0};

        std::atomic<Phase> phase_{Phase::Idle};
        std::atomic<QueryStatus> status_{QueryStatus::Pending};
        EngineBindings bindings_{};
        bool bindings_installed_{false};
        LayoutGateFacts gate_facts_{};
        bool gate_armed_{false};

        std::atomic<uint64_t> ticks_dispatched_{0};
        std::atomic<uint64_t> ticks_accepted_{0};
        std::atomic<uint64_t> refusals_[kRefusalStatusCount]{};
        std::atomic<uint64_t> staged_resets_{0};
        std::atomic<QueryStatus> last_refusal_{QueryStatus::Pending};
        std::atomic<uint64_t> last_refusal_sequence_{0};
    };

    // Process-lifetime instance used by the wiring and the dispatch route.
    auto Global() -> Harness&;

    // Off-thread JSONL formatter (controller driver). Bounded, allocation
    // free; returns the line length (NUL terminated) or 0 if the buffer is
    // too small. Runs on a SNAPSHOT, never on the live record.
    auto FormatRecordJsonLine(const QueryResultRecord& record, char (&out)[kJsonLineCapacity]) noexcept -> size_t;

    // Human-readable status names shared by the formatter and the driver.
    auto StatusName(QueryStatus status) noexcept -> const char*;
    auto MonotonicName(MonotonicVerdict verdict) noexcept -> const char*;
    auto StorageName(StorageLayout storage) noexcept -> const char*;
    auto CountSourceName(CountSourceKind kind) noexcept -> const char*;

#if !defined(NATIVE_QUERY_HARNESS_HOST_TEST)
    // Default target binding (NativeQueryHarness.cpp): identity probes from
    // UnrealInitializer.hpp and the single read UObjectArray::GetNumElements.
    auto MakeDefaultEngineBindings() noexcept -> EngineBindings;
#endif
}
