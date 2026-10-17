#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

//#include <Unreal/UClass.hpp>
#include <mutex>
#include <Constructs/Loop.hpp>

namespace RC::Unreal
{
    struct FUObjectItem;
    class UObject;
    class UStruct;
    class UClass;

    using ObjectSearcherForEachPredicate = std::function<LoopAction(UObject*)>;
    using ObjectSearcherInternalSignature = void(*)(UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate, std::vector<const FUObjectItem*>*);
    void ObjectSearcherFastInternal(UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate, std::vector<const FUObjectItem*>* Pool);
    void ObjectSearcherSlowInternal(UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate, std::vector<const FUObjectItem*>* Pool);
    // Walks ClassToObjectListMap instead of a pool, so it works for any class rather than the four pooled ones.
    void ObjectSearcherHashInternal(UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate, std::vector<const FUObjectItem*>* Pool);

    struct EmptyStaticClass
    {
        static UClass* StaticClass() { return nullptr; }
    };

    // Special value used specifically for any instance.
    struct InstancesStaticClass
    {
        static UClass* StaticClass() { return reinterpret_cast<UClass*>(0x333333); }
    };

    using AnyClass = EmptyStaticClass;
    using AnySuperStruct = EmptyStaticClass;
    using AnyInstance = InstancesStaticClass;


    auto IsValidSearcherClass(const UClass* Class) -> bool;
    auto IsValidSearcherSuperStruct(const UStruct* Struct) -> bool;

    struct ObjectSearcherPoolBase
    {
    public:
        virtual ~ObjectSearcherPoolBase() = default;
        virtual std::vector<const FUObjectItem*>* GetPool() = 0;
        virtual std::mutex& GetMutex() = 0;

        // Delete copy constructor and copy assignment operator
        ObjectSearcherPoolBase(const ObjectSearcherPoolBase&) = delete;
        ObjectSearcherPoolBase& operator=(const ObjectSearcherPoolBase&) = delete;

        // Delete move constructor and move assignment operator
        ObjectSearcherPoolBase(ObjectSearcherPoolBase&&) = delete;
        ObjectSearcherPoolBase& operator=(ObjectSearcherPoolBase&&) = delete;

    protected:
        ObjectSearcherPoolBase() = default;
    };

    template<typename ClassType, typename SuperStructType>
    struct ObjectSearcherPool : ObjectSearcherPoolBase
    {
    public:
        static inline std::vector<const FUObjectItem*> Pool{};
        static inline std::unordered_map<const FUObjectItem*, typename decltype(Pool)::size_type> PoolMap{};
        // Pointer to the default generic slow searcher.
        static inline std::unique_ptr<ObjectSearcherPoolBase> UnderlyingSearcher;
        static inline std::mutex PoolMutex;

        std::vector<const FUObjectItem*>* GetPool() override { return &Pool; }
        std::mutex& GetMutex() override { return PoolMutex; }

        static void Add(const FUObjectItem* Item)
        {
            PoolMap[Item] = Pool.size();
            Pool.emplace_back(Item);
        }

        // Drops the pool contents. The pool object itself stays alive so in flight searchers keep a valid pointer.
        static void Clear()
        {
            std::lock_guard<std::mutex> Lock(PoolMutex);
            decltype(Pool){}.swap(Pool);
            decltype(PoolMap){}.swap(PoolMap);
        }

        static void Remove(const FUObjectItem* Item)
        {
            const auto PoolElemIt = PoolMap.find(Item);
            if (PoolElemIt == PoolMap.end()) { return; }

            const auto Index = PoolElemIt->second;
            const auto LastIndex = Pool.size() - 1;

            if (Index != LastIndex)
            {
                // Swap with last element
                const auto LastItem = Pool[LastIndex];
                Pool[Index] = LastItem;
                PoolMap[LastItem] = Index;
            }

            Pool.pop_back();
            PoolMap.erase(PoolElemIt);
        }
    };

    struct ObjectSearcher
    {
    protected:
        UClass* Class{};
        UStruct* SuperStruct{};
        ObjectSearcherPoolBase* PoolPtr;
        ObjectSearcherInternalSignature InternalSearcher;
        bool bIsHashSearcher{};

    public:
        ObjectSearcher(UClass* C, UStruct* S, ObjectSearcherPoolBase* P, ObjectSearcherInternalSignature IS, bool bInIsHashSearcher = false)
            : Class(C), SuperStruct(S), PoolPtr(P), InternalSearcher(IS), bIsHashSearcher(bInIsHashSearcher) {}

        size_t PoolSize()
        { 
            size_t Size = 0;
            if (PoolPtr) {
                std::lock_guard<std::mutex> Lock(PoolPtr->GetMutex());
                Size = PoolPtr->GetPool()->size();
            }
            return Size;
        }

        bool IsFast()
        { 
            if (bIsHashSearcher) { return true; }
            bool bIsFast = false;
            if (PoolPtr) {
                std::lock_guard<std::mutex> Lock(PoolPtr->GetMutex());
                bIsFast = !PoolPtr->GetPool()->empty();
            }
            return bIsFast;
        }

        void ForEach(const ObjectSearcherForEachPredicate& Predicate)
        {
            if (PoolPtr)
            {
                std::lock_guard<std::mutex> Lock(PoolPtr->GetMutex());
                InternalSearcher(Class, SuperStruct, Predicate, PoolPtr->GetPool());
            }
            else
            {
                InternalSearcher(Class, SuperStruct, Predicate, nullptr);
            }
        }
    };

    size_t HashSearcherKey(UClass* Class, UStruct* SuperStruct);

    template<typename ClassType, typename SuperStructType>
    size_t HashSearcherKey()
    {
        return HashSearcherKey(ClassType::StaticClass(), SuperStructType::StaticClass());
    }

    extern std::unordered_map<size_t, std::unique_ptr<ObjectSearcherPoolBase>> AllSearcherPools;

    ObjectSearcher FindObjectSearcher(UClass* Class, UStruct* SuperStruct);

    template<typename ClassType, typename SuperStructType = AnySuperStruct>
    ObjectSearcher FindObjectSearcher()
    {
        return FindObjectSearcher(ClassType::StaticClass(), SuperStructType::StaticClass());
    }
}


