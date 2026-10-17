#include <Unreal/Searcher/ObjectSearcher.hpp>
#include <Unreal/Searcher/ObjectSearcherProfiler.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObjectHashTables.hpp>

namespace RC::Unreal
{
    std::unordered_map<size_t, std::unique_ptr<ObjectSearcherPoolBase>> AllSearcherPools;

    auto IsValidSearcherClass(const UClass* Class) -> bool
    {
        return Class && Class != AnyInstance::StaticClass();
    }

    auto IsValidSearcherSuperStruct(const UStruct* Struct) -> bool
    {
        return Struct && Struct != static_cast<UStruct*>(AnyInstance::StaticClass());
    }

    size_t HashSearcherKey(UClass* Class, UStruct* SuperStruct)
    {
        auto HashClass = std::hash<size_t>()(Class ? Class->HashObject() : 0);
        auto HashSuperStruct = std::hash<size_t>()(SuperStruct ? SuperStruct->HashObject() : 0);
        return HashClass ^ (HashSuperStruct << 1);
    }

    ObjectSearcher FindObjectSearcher(UClass* Class, UStruct* SuperStruct)
    {
        // The hash tables cover any class, not just the four pooled combinations.
        if (IsValidSearcherClass(Class) && UnrealInitializer::ShouldUseHashTables())
        {
            return ObjectSearcher{Class, SuperStruct, nullptr, &ObjectSearcherHashInternal, true};
        }

        if (!UnrealInitializer::StaticStorage::bUObjectArrayCacheRetired)
        {
            if (auto It = AllSearcherPools.find(HashSearcherKey(Class, SuperStruct)); It != AllSearcherPools.end())
            {
                return ObjectSearcher{Class, SuperStruct, It->second.get(), &ObjectSearcherFastInternal};
            }
        }

        return ObjectSearcher{Class, SuperStruct, nullptr, &ObjectSearcherSlowInternal};
    }

    static LoopAction InternalPredicate(UObject* Object, UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate)
    {
        if (IsValidSearcherClass(Class) && !Object->IsA(Class)) { return LoopAction::Continue; }
        if (IsValidSearcherSuperStruct(SuperStruct))
        {
            if (Object->IsA<UClass>())
            {
                if (!static_cast<UClass*>(Object)->IsChildOf(SuperStruct)) { return LoopAction::Continue; }
            }
            else
            {
                if (!Object->GetClassPrivate()->IsChildOf(SuperStruct)) { return LoopAction::Continue; }
            }
        }
        return Predicate(Object);
    }

    void ObjectSearcherFastInternal(UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate, std::vector<const FUObjectItem*>* Pool)
    {
        OBJSEARCHER_PROFILE_SEARCH_FAST()
        for (const auto& Item : *Pool)
        {
            UObject* Object = Item->GetUObject();
            if (!Object || Item->IsUnreachable()) { continue; }
            if (InternalPredicate(Object, Class, SuperStruct, Predicate) == LoopAction::Break) { break; }
        }
    }

    void ObjectSearcherHashInternal(UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate, std::vector<const FUObjectItem*>* Pool)
    {
        OBJSEARCHER_PROFILE_SEARCH_FAST()
        (void)Pool;

        // Snapshot under the table lock. The predicate is arbitrary caller code and must not run while the engine is blocked.
        // Items rather than objects, so a destroyed object resolves to null instead of a dangling pointer.
        std::vector<const FUObjectItem*> Items{};
        {
            const auto& Tables = FUObjectHashTables::Get();
            FUObjectHashTables::FScopedLock Lock(Tables);
            Tables.ForEachObjectOfClassIncludingDerived(Class, [&](UObjectBase* Object) {
                if (Object)
                {
                    Items.emplace_back(Object->GetObjectItem());
                }
            });
        }

        // The class is already guaranteed by the walk, only the super struct filter is left.
        for (const FUObjectItem* Item : Items)
        {
            UObject* Object = Item ? Item->GetUObject() : nullptr;
            if (!Object || Item->IsUnreachable()) { continue; }
            if (InternalPredicate(Object, nullptr, SuperStruct, Predicate) == LoopAction::Break) { break; }
        }
    }

    void ObjectSearcherSlowInternal(UClass* Class, UStruct* SuperStruct, const ObjectSearcherForEachPredicate& Predicate, std::vector<const FUObjectItem*>* Pool)
    {
        OBJSEARCHER_PROFILE_SEARCH_SLOW()
        (void)Pool;

        UObjectGlobals::ForEachUObject([&](UObject* Object, ...) {
            if (!Object || Object->IsUnreachable()) { return LoopAction::Continue; }
            return InternalPredicate(Object, Class, SuperStruct, Predicate);
        });
    }
}
