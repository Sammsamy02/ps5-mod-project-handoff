#include <bit>

#include <Unreal/ClassListener.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Searcher/ObjectSearcher.hpp>
#include <Unreal/Searcher/ObjectSearcherProfiler.hpp>
#include <DynamicOutput/Output.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>

#if defined(__PROSPERO__)
// L25: shared composition diagnostic sink (diagnostic.c). Same in-link C symbol
// PS5PadSource.cpp already declares; defined in-link by the composition, so
// this adds no new import and no new undefined symbol.
extern "C" void diagnostic_event(const char* tag, unsigned int value);
#endif

namespace RC::Unreal
{
    // =====================================================================================
    // L25 (Claude l25 item 3): throttled first-call markers. These prove from the log
    // whether the ENGINE ever dispatches into our registered delete listeners (a
    // registered-but-never-called listener shifts suspicion to registration itself;
    // calls with no crash shift it to the slot-4 GetAllocatedSize path fixed in
    // UObjectArray.hpp in this same change set). Markers log the FIRST call and then
    // at most once per ~10 seconds; NotifyUObjectDeleted fires per destroyed UObject
    // during GC, so an unthrottled log would flood UE4SS.log and the supplier-diag
    // jsonl sink. All atomics: the engine calls these on the game thread while our
    // init/event-loop threads run concurrently.
    // =====================================================================================
    namespace
    {
        auto l25_now_ms() -> uint64_t
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        class L25CallMarker
        {
        public:
            explicit L25CallMarker(const char* event_tag) : m_event_tag(event_tag) {}

            // Returns true when this invocation should log (first call, or >=10s since
            // the last logged call).
            auto should_log() -> bool
            {
                (void)m_calls.fetch_add(1, std::memory_order_relaxed);
                const uint64_t now = l25_now_ms();
                if (!m_logged_first.exchange(true, std::memory_order_relaxed))
                {
                    m_last_logged_ms.store(now, std::memory_order_relaxed);
                    return true;
                }
                uint64_t last = m_last_logged_ms.load(std::memory_order_relaxed);
                while (now - last >= 10000)
                {
                    if (m_last_logged_ms.compare_exchange_weak(last, now, std::memory_order_relaxed))
                    {
                        return true;
                    }
                }
                return false;
            }

            [[nodiscard]] auto calls() const -> uint64_t
            {
                return 1 + m_calls.load(std::memory_order_relaxed);
            }

            auto emit_event(uint32_t value) const -> void
            {
#if defined(__PROSPERO__)
                diagnostic_event(m_event_tag, value);
#else
                (void)value;
#endif
            }

        private:
            const char* m_event_tag;
            std::atomic<uint64_t> m_calls{0};
            std::atomic<uint64_t> m_last_logged_ms{0};
            std::atomic<bool> m_logged_first{false};
        };

        L25CallMarker L25ClassDeleteNotifyMarker{"l25_dl_first_notify_class"};
        L25CallMarker L25ShutdownDeleteNotifyMarker{"l25_dl_first_notify_shutdown"};
        L25CallMarker L25ClassDeleteShutdownMarker{"l25_dl_shutdown_cb_class"};
        L25CallMarker L25ShutdownDeleteShutdownMarker{"l25_dl_shutdown_cb_shutdown"};
    }

    FClassCreateListener FClassCreateListener::ClassCreateListener{};
    FClassDeleteListener FClassDeleteListener::ClassDeleteListener{};
    FShutdownDeleteListener FShutdownDeleteListener::ShutdownDeleteListener{};

    std::unordered_map<StringType, UObject*> FClassCreateListener::GlobalObjectCache{};
    std::unordered_map<UObject*, StringType> FClassCreateListener::GlobalObjectValueCache{};

    void ClearGlobalObjectCache()
    {
        FClassCreateListener::GlobalObjectCache.clear();
        FClassCreateListener::GlobalObjectValueCache.clear();
    }

    static bool IsGlobalObjectCacheActive()
    {
        return UnrealInitializer::StaticStorage::GlobalConfig.bUseUObjectArrayCache && !UnrealInitializer::StaticStorage::bUObjectArrayCacheRetired;
    }

    void CacheGlobalObject(UObject* Object)
    {
        if (!IsGlobalObjectCacheActive())
        {
            return;
        }
        auto [ValueIt, bWasEmplaced] = FClassCreateListener::GlobalObjectValueCache.emplace(Object, StringType{});
        if (bWasEmplaced)
        {
            auto FullName = Object->GetFullName();
            auto SpaceLocation = FullName.find(STR(" "));
            auto FullNameWithoutType = StringType{FullName.begin() + SpaceLocation + 1, FullName.end()};
            ValueIt->second = FullNameWithoutType;
            FClassCreateListener::GlobalObjectCache.emplace(FullNameWithoutType, Object);
        }
    }

    void InvalidateGlobalObject(UObject* Object)
    {
        if (!IsGlobalObjectCacheActive())
        {
            return;
        }
        if (auto ValueIt = FClassCreateListener::GlobalObjectValueCache.find(Object); ValueIt != FClassCreateListener::GlobalObjectValueCache.end())
        {
            FClassCreateListener::GlobalObjectCache.erase(ValueIt->second);
            FClassCreateListener::GlobalObjectValueCache.erase(ValueIt);
        }
    }

    UObject* GetGlobalObject(const StringType& FullNameWithoutType)
    {
        if (!IsGlobalObjectCacheActive())
        {
            return nullptr;
        }
        if (auto It = FClassCreateListener::GlobalObjectCache.find(FullNameWithoutType); It != FClassCreateListener::GlobalObjectCache.end())
        {
            return It->second;
        }
        else
        {
            return nullptr;
        }
    }

    void FClassCreateListener::NotifyUObjectCreated(const UObjectBase* Object, int32 Index)
    {
    }

    void FClassCreateListener::OnUObjectArrayShutdown()
    {
        // [R9-GUARD] called by the engine: nothing may escape into engine code.
        try
        {
            UObjectArray::RemoveUObjectCreateListener(this);
        }
        catch (...)
        {
        }
    }

    void FClassDeleteListener::NotifyUObjectDeleted(const UObjectBase* Object, int32 Index)
    {
        // [R9-GUARD] called by the engine: nothing may escape into engine code.
        try
        {
            // L25 (item 3): throttled engine-dispatch proof marker.
            if (L25ClassDeleteNotifyMarker.should_log())
            {
                Output::send(STR("[L25][DeleteListener] engine called FClassDeleteListener::NotifyUObjectDeleted (Object={} Index={} calls={})\n"),
                             reinterpret_cast<const void*>(Object), Index, L25ClassDeleteNotifyMarker.calls());
                L25ClassDeleteNotifyMarker.emit_event(static_cast<uint32_t>(Index));
            }

            if (UnrealInitializer::StaticStorage::bUObjectArrayCacheRetired)
            {
                return;
            }

            OBJSEARCHER_PROFILE_REMOVE()
            InvalidateGlobalObject(std::bit_cast<UObject*>(Object));

            auto* ObjectItem = Object->GetObjectItem();

            // All classes.
            {
                std::lock_guard<std::mutex> Lock(ObjectSearcherPool<UClass, AnySuperStruct>::PoolMutex);
                ObjectSearcherPool<UClass, AnySuperStruct>::Remove(ObjectItem);
            }

            // All actor classes.
            {
                std::lock_guard<std::mutex> Lock(ObjectSearcherPool<UClass, AActor>::PoolMutex);
                ObjectSearcherPool<UClass, AActor>::Remove(ObjectItem);
            }

            // All actor instances.
            {
                std::lock_guard<std::mutex> Lock(ObjectSearcherPool<AActor, AnySuperStruct>::PoolMutex);
                ObjectSearcherPool<AActor, AnySuperStruct>::Remove(ObjectItem);
            }

            // All instances.
            {
                std::lock_guard<std::mutex> Lock(ObjectSearcherPool<AnyInstance, AnyInstance>::PoolMutex);
                ObjectSearcherPool<AnyInstance, AnyInstance>::Remove(ObjectItem);
            }
        }
        catch (...)
        {
        }
    }

    void FClassDeleteListener::OnUObjectArrayShutdown()
    {
        // [R9-GUARD] called by the engine: nothing may escape into engine code.
        try
        {
            // L25 (item 3): engine-dispatch proof marker.
            if (L25ClassDeleteShutdownMarker.should_log())
            {
                Output::send(STR("[L25][DeleteListener] engine called FClassDeleteListener::OnUObjectArrayShutdown (calls={})\n"),
                             L25ClassDeleteShutdownMarker.calls());
                L25ClassDeleteShutdownMarker.emit_event(0);
            }
            UObjectArray::RemoveUObjectDeleteListener(this);
        }
        catch (...)
        {
        }
    }

    namespace UObjectGlobals
    {
        auto r9_index_note_deleted(int32_t Index) -> void; // [R9-IDX] UObjectGlobals.cpp
    }
    using UObjectGlobals::r9_index_note_deleted;

    void FShutdownDeleteListener::NotifyUObjectDeleted([[maybe_unused]] const UObjectBase* Object, [[maybe_unused]] int32 Index)
    {
        // [R9-GUARD] called by the engine: nothing may escape into engine code.
        try
        {
            // L25 (item 3): throttled engine-dispatch proof marker (body was empty before;
            // this listener exists for the shutdown callback only).
            r9_index_note_deleted(Index); // [R9-IDX] keep the object index exact
            if (L25ShutdownDeleteNotifyMarker.should_log())
            {
                Output::send(STR("[L25][DeleteListener] engine called FShutdownDeleteListener::NotifyUObjectDeleted (Object={} Index={} calls={})\n"),
                             reinterpret_cast<const void*>(Object), Index, L25ShutdownDeleteNotifyMarker.calls());
                L25ShutdownDeleteNotifyMarker.emit_event(static_cast<uint32_t>(Index));
            }
        }
        catch (...)
        {
        }
    }

    void FShutdownDeleteListener::OnUObjectArrayShutdown()
    {
        // [R9-GUARD] called by the engine: nothing may escape into engine code.
        try
        {
            // L25 (item 3): engine-dispatch proof marker.
            if (L25ShutdownDeleteShutdownMarker.should_log())
            {
                Output::send(STR("[L25][DeleteListener] engine called FShutdownDeleteListener::OnUObjectArrayShutdown (calls={})\n"),
                             L25ShutdownDeleteShutdownMarker.calls());
                L25ShutdownDeleteShutdownMarker.emit_event(0);
            }
            UnrealInitializer::IsInitialized() = false;
            UnrealInitializer::StaticStorage::bIsInitialized = false;
            UObjectArray::RemoveUObjectDeleteListener(this);
        }
        catch (...)
        {
        }
    }

}
