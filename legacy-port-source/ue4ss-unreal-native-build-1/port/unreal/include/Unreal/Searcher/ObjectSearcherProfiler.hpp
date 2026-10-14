#pragma once

#ifdef UE4SS_PROFILERS

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <vector>
#include <string>

namespace RC::Unreal
{
    class UClass;
    class UObject;
    class UObjectBase;

    // Only 1 in N Add/Remove calls is timed. 1 times every call at a noticeable cost.
    constexpr uint64_t kAddRemoveSampleRate = 100;

    // Lightweight profiling stats - lock-free
    struct ObjectSearcherStats
    {
        std::atomic<uint64_t> AddCount{0};
        std::atomic<uint64_t> RemoveCount{0};
        std::atomic<uint64_t> SearchCount{0};
        std::atomic<uint64_t> AddTimeNs{0};
        std::atomic<uint64_t> RemoveTimeNs{0};
        std::atomic<uint64_t> SearchTimeNs{0};

        // Track pool sizes at sample time
        std::atomic<uint64_t> LastPoolSizeActors{0};
        std::atomic<uint64_t> LastPoolSizeClasses{0};
        std::atomic<uint64_t> LastPoolSizeActorClasses{0};
        std::atomic<uint64_t> LastPoolSizeInstances{0};

        void Reset()
        {
            AddCount.store(0, std::memory_order_relaxed);
            RemoveCount.store(0, std::memory_order_relaxed);
            SearchCount.store(0, std::memory_order_relaxed);
            AddTimeNs.store(0, std::memory_order_relaxed);
            RemoveTimeNs.store(0, std::memory_order_relaxed);
            SearchTimeNs.store(0, std::memory_order_relaxed);
        }

        // Get average times in microseconds
        // Add/Remove are sampled, so the average is over count / sampleRate timed calls
        double AvgAddTimeUs(uint64_t sampleRate = 1) const
        {
            auto count = AddCount.load(std::memory_order_relaxed);
            auto sampledCount = count / sampleRate;
            return sampledCount > 0 ? (AddTimeNs.load(std::memory_order_relaxed) / 1000.0) / sampledCount : 0.0;
        }
        double AvgRemoveTimeUs(uint64_t sampleRate = 1) const
        {
            auto count = RemoveCount.load(std::memory_order_relaxed);
            auto sampledCount = count / sampleRate;
            return sampledCount > 0 ? (RemoveTimeNs.load(std::memory_order_relaxed) / 1000.0) / sampledCount : 0.0;
        }
        double AvgSearchTimeUs() const
        {
            auto count = SearchCount.load(std::memory_order_relaxed);
            return count > 0 ? (SearchTimeNs.load(std::memory_order_relaxed) / 1000.0) / count : 0.0;
        }

        // Total times in milliseconds
        double TotalAddTimeMs() const
        {
            return AddTimeNs.load(std::memory_order_relaxed) / 1000000.0;
        }
        double TotalRemoveTimeMs() const
        {
            return RemoveTimeNs.load(std::memory_order_relaxed) / 1000000.0;
        }
        double TotalSearchTimeMs() const
        {
            return SearchTimeNs.load(std::memory_order_relaxed) / 1000000.0;
        }
    };

    // RAII timer that accumulates into atomic counter
    class ScopedTimer
    {
    public:
        explicit ScopedTimer(std::atomic<uint64_t>& target)
            : m_Target(target)
            , m_Start(std::chrono::high_resolution_clock::now())
        {}

        ~ScopedTimer()
        {
            auto end = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_Start).count();
            m_Target.fetch_add(static_cast<uint64_t>(ns), std::memory_order_relaxed);
        }

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

    private:
        std::atomic<uint64_t>& m_Target;
        std::chrono::high_resolution_clock::time_point m_Start;
    };

    // GUObjectArray iteration stats
    struct GUObjectArrayStats
    {
        std::atomic<uint64_t> IterationCount{0};
        std::atomic<uint64_t> IterationTimeNs{0};
        std::atomic<uint64_t> ObjectsIterated{0};

        void Reset()
        {
            IterationCount.store(0, std::memory_order_relaxed);
            IterationTimeNs.store(0, std::memory_order_relaxed);
            ObjectsIterated.store(0, std::memory_order_relaxed);
        }

        double AvgIterationTimeUs() const
        {
            auto count = IterationCount.load(std::memory_order_relaxed);
            return count > 0 ? (IterationTimeNs.load(std::memory_order_relaxed) / 1000.0) / count : 0.0;
        }

        double TotalIterationTimeMs() const
        {
            return IterationTimeNs.load(std::memory_order_relaxed) / 1000000.0;
        }

        double AvgObjectsPerIteration() const
        {
            auto count = IterationCount.load(std::memory_order_relaxed);
            return count > 0 ? static_cast<double>(ObjectsIterated.load(std::memory_order_relaxed)) / count : 0.0;
        }
    };

    // FUObjectHashTables search stats (ClassToObjectListMap)
    struct HashTableSearchStats
    {
        std::atomic<uint64_t> SearchCount{0};
        std::atomic<uint64_t> SearchTimeNs{0};
        std::atomic<uint64_t> ObjectsFound{0};
        std::atomic<uint64_t> BucketHits{0};    // How many times we found a bucket for the class
        std::atomic<uint64_t> BucketMisses{0};  // How many times no bucket existed

        void Reset()
        {
            SearchCount.store(0, std::memory_order_relaxed);
            SearchTimeNs.store(0, std::memory_order_relaxed);
            ObjectsFound.store(0, std::memory_order_relaxed);
            BucketHits.store(0, std::memory_order_relaxed);
            BucketMisses.store(0, std::memory_order_relaxed);
        }

        double AvgSearchTimeUs() const
        {
            auto count = SearchCount.load(std::memory_order_relaxed);
            return count > 0 ? (SearchTimeNs.load(std::memory_order_relaxed) / 1000.0) / count : 0.0;
        }

        double TotalSearchTimeMs() const
        {
            return SearchTimeNs.load(std::memory_order_relaxed) / 1000000.0;
        }

        double AvgObjectsPerSearch() const
        {
            auto count = SearchCount.load(std::memory_order_relaxed);
            return count > 0 ? static_cast<double>(ObjectsFound.load(std::memory_order_relaxed)) / count : 0.0;
        }

        double BucketHitRate() const
        {
            auto hits = BucketHits.load(std::memory_order_relaxed);
            auto misses = BucketMisses.load(std::memory_order_relaxed);
            auto total = hits + misses;
            return total > 0 ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
        }
    };

    // Global profiler state - runtime configurable
    struct ObjectSearcherProfiler
    {
        // Enable/disable profiling overhead (default enabled to capture startup)
        static inline std::atomic<bool> bProfilingEnabled{false};

        // Separate stats for fast (pool) and slow (full scan) paths
        static inline ObjectSearcherStats FastStats{};
        static inline ObjectSearcherStats SlowStats{};

        // GUObjectArray iteration stats
        static inline GUObjectArrayStats GUObjectArrayIterStats{};

        // FUObjectHashTables (ClassToObjectListMap) search stats
        static inline HashTableSearchStats HashTableStats{};

        static void EnableProfiling(bool enable)
        {
            bProfilingEnabled.store(enable, std::memory_order_release);
        }

        static bool IsProfilingEnabled()
        {
            return bProfilingEnabled.load(std::memory_order_acquire);
        }

        static void ResetAllStats()
        {
            FastStats.Reset();
            SlowStats.Reset();
            GUObjectArrayIterStats.Reset();
            HashTableStats.Reset();
        }

        // Generate timestamped filename
        static std::string GenerateTimestampedFilename(const std::string& prefix = "profiler")
        {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &time_t_now);
#else
            localtime_r(&time_t_now, &tm_buf);
#endif

            std::ostringstream oss;
            oss << prefix << "_"
                << std::put_time(&tm_buf, "%Y%m%d_%H%M%S")
                << "_" << std::setfill('0') << std::setw(3) << ms.count()
                << ".txt";
            return oss.str();
        }

        // Dump all stats to a file
        static bool DumpStatsToFile(const std::filesystem::path& directory)
        {
            std::filesystem::create_directories(directory);
            auto filepath = directory / GenerateTimestampedFilename("objsearcher_profile");

            std::ofstream file(filepath);
            if (!file.is_open())
            {
                return false;
            }

            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);

            file << "=== Object Searcher Profiler Dump ===" << std::endl;
#ifdef _WIN32
            char time_buf[26];
            ctime_s(time_buf, sizeof(time_buf), &time_t_now);
            file << "Timestamp: " << time_buf;
#else
            file << "Timestamp: " << std::ctime(&time_t_now);
#endif
            file << "Profiling Enabled: " << (IsProfilingEnabled() ? "Yes" : "No") << std::endl;
            file << std::endl;

            // Pool sizes
            file << "=== Pool Sizes (Last Snapshot) ===" << std::endl;
            file << "Actors Pool:       " << FastStats.LastPoolSizeActors.load(std::memory_order_relaxed) << std::endl;
            file << "Classes Pool:      " << FastStats.LastPoolSizeClasses.load(std::memory_order_relaxed) << std::endl;
            file << "Actor Classes Pool:" << FastStats.LastPoolSizeActorClasses.load(std::memory_order_relaxed) << std::endl;
            file << "Instances Pool:    " << FastStats.LastPoolSizeInstances.load(std::memory_order_relaxed) << std::endl;
            file << std::endl;

            // Fast path stats
            file << "=== Fast Path (Pool-based) ===" << std::endl;
            file << std::fixed << std::setprecision(3);
            file << "Add Operations (sampled 1:" << kAddRemoveSampleRate << "):" << std::endl;
            file << "  Count:     " << FastStats.AddCount.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg Time:  " << FastStats.AvgAddTimeUs(kAddRemoveSampleRate) << " us" << std::endl;
            file << "  Total Time:" << FastStats.TotalAddTimeMs() << " ms (sampled)" << std::endl;
            file << "Remove Operations (sampled 1:" << kAddRemoveSampleRate << "):" << std::endl;
            file << "  Count:     " << FastStats.RemoveCount.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg Time:  " << FastStats.AvgRemoveTimeUs(kAddRemoveSampleRate) << " us" << std::endl;
            file << "  Total Time:" << FastStats.TotalRemoveTimeMs() << " ms (sampled)" << std::endl;
            file << "Search Operations:" << std::endl;
            file << "  Count:     " << FastStats.SearchCount.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg Time:  " << FastStats.AvgSearchTimeUs() << " us" << std::endl;
            file << "  Total Time:" << FastStats.TotalSearchTimeMs() << " ms" << std::endl;
            file << std::endl;

            // Slow path stats
            file << "=== Slow Path (Full GUObjectArray Scan) ===" << std::endl;
            file << "Search Operations:" << std::endl;
            file << "  Count:     " << SlowStats.SearchCount.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg Time:  " << SlowStats.AvgSearchTimeUs() << " us" << std::endl;
            file << "  Total Time:" << SlowStats.TotalSearchTimeMs() << " ms" << std::endl;
            file << std::endl;

            // GUObjectArray iteration stats
            file << "=== GUObjectArray Iteration ===" << std::endl;
            file << "Iterations:" << std::endl;
            file << "  Count:           " << GUObjectArrayIterStats.IterationCount.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg Time:        " << GUObjectArrayIterStats.AvgIterationTimeUs() << " us" << std::endl;
            file << "  Total Time:      " << GUObjectArrayIterStats.TotalIterationTimeMs() << " ms" << std::endl;
            file << "  Objects Iterated:" << GUObjectArrayIterStats.ObjectsIterated.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg per Iteration:" << std::setprecision(0) << GUObjectArrayIterStats.AvgObjectsPerIteration() << std::endl;
            file << std::endl;

            // FUObjectHashTables (ClassToObjectListMap) search stats
            file << "=== FUObjectHashTables (ClassToObjectListMap) ===" << std::endl;
            file << std::setprecision(3);
            file << "Search Operations:" << std::endl;
            file << "  Count:           " << HashTableStats.SearchCount.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg Time:        " << HashTableStats.AvgSearchTimeUs() << " us" << std::endl;
            file << "  Total Time:      " << HashTableStats.TotalSearchTimeMs() << " ms" << std::endl;
            file << "  Objects Found:   " << HashTableStats.ObjectsFound.load(std::memory_order_relaxed) << std::endl;
            file << "  Avg per Search:  " << HashTableStats.AvgObjectsPerSearch() << std::endl;
            file << "Bucket Statistics:" << std::endl;
            file << "  Hits:            " << HashTableStats.BucketHits.load(std::memory_order_relaxed) << std::endl;
            file << "  Misses:          " << HashTableStats.BucketMisses.load(std::memory_order_relaxed) << std::endl;
            file << "  Hit Rate:        " << std::setprecision(1) << HashTableStats.BucketHitRate() << " %" << std::endl;
            file << std::endl;

            // Search Method Comparison
            file << "=== Search Method Comparison ===" << std::endl;
            file << std::setprecision(3);
            auto fastAvg = FastStats.AvgSearchTimeUs();
            auto slowAvg = SlowStats.AvgSearchTimeUs();
            auto hashAvg = HashTableStats.AvgSearchTimeUs();
            file << "Average Search Time:" << std::endl;
            file << "  Pool (Fast):     " << fastAvg << " us" << std::endl;
            file << "  GUObjectArray:   " << slowAvg << " us" << std::endl;
            file << "  HashTable:       " << hashAvg << " us" << std::endl;
            if (fastAvg > 0 && slowAvg > 0)
            {
                file << "  Pool vs Slow:    " << std::setprecision(1) << (slowAvg / fastAvg) << "x faster" << std::endl;
            }
            if (hashAvg > 0 && slowAvg > 0)
            {
                file << "  Hash vs Slow:    " << std::setprecision(1) << (slowAvg / hashAvg) << "x faster" << std::endl;
            }
            if (fastAvg > 0 && hashAvg > 0)
            {
                if (fastAvg < hashAvg)
                {
                    file << "  Pool vs Hash:    " << std::setprecision(1) << (hashAvg / fastAvg) << "x faster (Pool wins)" << std::endl;
                }
                else
                {
                    file << "  Hash vs Pool:    " << std::setprecision(1) << (fastAvg / hashAvg) << "x faster (Hash wins)" << std::endl;
                }
            }
            file << std::endl;

            file << "=== End of Dump ===" << std::endl;
            file.close();

            return true;
        }
    };

    // Macros for conditional profiling (zero overhead when disabled at runtime)
    // Fast path (pool-based) macros with sampling to reduce overhead
    #define OBJSEARCHER_PROFILE_ADD() \
        std::optional<RC::Unreal::ScopedTimer> _profTimerAdd; \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            static thread_local uint64_t _addSampleCounter = 0; \
            if ((++_addSampleCounter % RC::Unreal::kAddRemoveSampleRate) == 0) { \
                _profTimerAdd.emplace(RC::Unreal::ObjectSearcherProfiler::FastStats.AddTimeNs); \
            } \
            RC::Unreal::ObjectSearcherProfiler::FastStats.AddCount.fetch_add(1, std::memory_order_relaxed); \
        }

    #define OBJSEARCHER_PROFILE_REMOVE() \
        std::optional<RC::Unreal::ScopedTimer> _profTimerRemove; \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            static thread_local uint64_t _removeSampleCounter = 0; \
            if ((++_removeSampleCounter % RC::Unreal::kAddRemoveSampleRate) == 0) { \
                _profTimerRemove.emplace(RC::Unreal::ObjectSearcherProfiler::FastStats.RemoveTimeNs); \
            } \
            RC::Unreal::ObjectSearcherProfiler::FastStats.RemoveCount.fetch_add(1, std::memory_order_relaxed); \
        }

    #define OBJSEARCHER_PROFILE_SEARCH_FAST() \
        std::optional<RC::Unreal::ScopedTimer> _profTimerSearch; \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            _profTimerSearch.emplace(RC::Unreal::ObjectSearcherProfiler::FastStats.SearchTimeNs); \
            RC::Unreal::ObjectSearcherProfiler::FastStats.SearchCount.fetch_add(1, std::memory_order_relaxed); \
        }

    // Slow path (full GUObjectArray scan) macros
    #define OBJSEARCHER_PROFILE_SEARCH_SLOW() \
        std::optional<RC::Unreal::ScopedTimer> _profTimerSearch; \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            _profTimerSearch.emplace(RC::Unreal::ObjectSearcherProfiler::SlowStats.SearchTimeNs); \
            RC::Unreal::ObjectSearcherProfiler::SlowStats.SearchCount.fetch_add(1, std::memory_order_relaxed); \
        }

    // GUObjectArray iteration profiling
    #define GUOBJECTARRAY_PROFILE_ITER_BEGIN() \
        std::optional<RC::Unreal::ScopedTimer> _profTimerIter; \
        uint64_t _profIterCount = 0; \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            _profTimerIter.emplace(RC::Unreal::ObjectSearcherProfiler::GUObjectArrayIterStats.IterationTimeNs); \
            RC::Unreal::ObjectSearcherProfiler::GUObjectArrayIterStats.IterationCount.fetch_add(1, std::memory_order_relaxed); \
        }

    #define GUOBJECTARRAY_PROFILE_ITER_COUNT() \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { ++_profIterCount; }

    #define GUOBJECTARRAY_PROFILE_ITER_END() \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            RC::Unreal::ObjectSearcherProfiler::GUObjectArrayIterStats.ObjectsIterated.fetch_add(_profIterCount, std::memory_order_relaxed); \
        }

    // FUObjectHashTables (ClassToObjectListMap) search profiling
    #define HASHTABLE_PROFILE_SEARCH_BEGIN() \
        std::optional<RC::Unreal::ScopedTimer> _profTimerHash; \
        uint64_t _profHashObjCount = 0; \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            _profTimerHash.emplace(RC::Unreal::ObjectSearcherProfiler::HashTableStats.SearchTimeNs); \
            RC::Unreal::ObjectSearcherProfiler::HashTableStats.SearchCount.fetch_add(1, std::memory_order_relaxed); \
        }

    #define HASHTABLE_PROFILE_BUCKET_HIT() \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            RC::Unreal::ObjectSearcherProfiler::HashTableStats.BucketHits.fetch_add(1, std::memory_order_relaxed); \
        }

    #define HASHTABLE_PROFILE_BUCKET_MISS() \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            RC::Unreal::ObjectSearcherProfiler::HashTableStats.BucketMisses.fetch_add(1, std::memory_order_relaxed); \
        }

    #define HASHTABLE_PROFILE_OBJ_COUNT() \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { ++_profHashObjCount; }

    #define HASHTABLE_PROFILE_SEARCH_END() \
        if (RC::Unreal::ObjectSearcherProfiler::IsProfilingEnabled()) { \
            RC::Unreal::ObjectSearcherProfiler::HashTableStats.ObjectsFound.fetch_add(_profHashObjCount, std::memory_order_relaxed); \
        }

} // namespace RC::Unreal

#else // UE4SS_PROFILERS not defined - no-op macros

    #define OBJSEARCHER_PROFILE_ADD()
    #define OBJSEARCHER_PROFILE_REMOVE()
    #define OBJSEARCHER_PROFILE_SEARCH_FAST()
    #define OBJSEARCHER_PROFILE_SEARCH_SLOW()
    #define GUOBJECTARRAY_PROFILE_ITER_BEGIN()
    #define GUOBJECTARRAY_PROFILE_ITER_COUNT()
    #define GUOBJECTARRAY_PROFILE_ITER_END()
    #define HASHTABLE_PROFILE_SEARCH_BEGIN()
    #define HASHTABLE_PROFILE_BUCKET_HIT()
    #define HASHTABLE_PROFILE_BUCKET_MISS()
    #define HASHTABLE_PROFILE_OBJ_COUNT()
    #define HASHTABLE_PROFILE_SEARCH_END()

#endif // UE4SS_PROFILERS