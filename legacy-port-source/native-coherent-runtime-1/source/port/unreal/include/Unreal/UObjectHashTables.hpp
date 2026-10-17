#pragma once

#include <Unreal/Common.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/Core/Containers/Map.hpp>
#include <Unreal/Core/Containers/Set.hpp>
#include <Unreal/Core/Templates/Atomic.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/NameTypes.hpp>
#include <Helpers/Casting.hpp>

#include <atomic>

namespace RC::Unreal
{
    class UClass;
    class UObject;
    class UObjectBase;
    class UPackage;

    struct FHashBucket;

    // 5.0 wraps the maps in TBucketMap, which only adds a read lock in non-shipping builds, and
    // 5.6 renames FHashBucket to FSetHashBucket. Both are layout identical to the older types.
    template<typename K, typename V = FHashBucket>
    using TBucketMap = TMap<K, V>;
    using FSetHashBucket = FHashBucket;

    // Overflow storage of a bucket from 5.8: TCompactSet<UObjectBase*>, contiguous elements followed by
    // a separate compact hash table. Only the element array is needed to walk it.
    struct FCompactObjectSetView
    {
        UObjectBase** Data;
        int32 Num;
        int32 Max;
        void* HashData;
        int32 HashNum;
        int32 HashMax;
    };

    // Engine bucket used by the object hash maps from 4.8 (per map: ObjectOuterMap from 4.16,
    // ClassToObjectListMap from 4.22). Two pointers: up to two objects stored inline, or
    // Elements[0] null and Elements[1] pointing at the overflow set once there are more.
    // The overflow set is a TSet<UObjectBase*> up to 5.7 and a TCompactSet<UObjectBase*> from 5.8.
    struct RC_UE_API FHashBucket
    {
        void* Elements[2];

        bool HasSet() const
        {
            return Elements[1] != nullptr && Elements[0] == nullptr;
        }

        static bool UsesCompactSet()
        {
            return Version::IsAtLeast(5, 8);
        }

        // The game thread mutates buckets without taking the critical section from 5.0 on, so an
        // overflow pointer read from another thread can be stale. Refuse headers that cannot be live.
        TSet<UObjectBase*>* GetSet()
        {
            if (!HasSet() || UsesCompactSet())
            {
                return nullptr;
            }
            auto* Set = reinterpret_cast<TSet<UObjectBase*>*>(Elements[1]);
            const int32 Num = Set->Num();
            const int32 Max = Set->GetMaxIndex();
            if (Num < 0 || Max < Num || Max > (1 << 26))
            {
                return nullptr;
            }
            return Set;
        }

        const TSet<UObjectBase*>* GetSet() const
        {
            return const_cast<FHashBucket*>(this)->GetSet();
        }

        const FCompactObjectSetView* GetCompactSet() const
        {
            if (!HasSet() || !UsesCompactSet())
            {
                return nullptr;
            }
            const auto* View = reinterpret_cast<const FCompactObjectSetView*>(Elements[1]);
            if (View->Num < 0 || View->Max < View->Num || View->Max > (1 << 26) || (View->Num > 0 && View->Data == nullptr))
            {
                return nullptr;
            }
            return View;
        }

        int32 Num() const
        {
            if (HasSet())
            {
                if (const FCompactObjectSetView* View = GetCompactSet())
                {
                    return View->Num;
                }
                if (const TSet<UObjectBase*>* Set = GetSet())
                {
                    return Set->Num();
                }
                return 0;
            }
            return (Elements[0] ? 1 : 0) + (Elements[1] ? 1 : 0);
        }

        bool Contains(UObjectBase* Object) const
        {
            if (HasSet())
            {
                if (const FCompactObjectSetView* View = GetCompactSet())
                {
                    for (int32 Index = 0; Index < View->Num; ++Index)
                    {
                        if (View->Data[Index] == Object)
                        {
                            return true;
                        }
                    }
                    return false;
                }
                if (const TSet<UObjectBase*>* Set = GetSet())
                {
                    return Set->Contains(Object);
                }
                return false;
            }
            return Object == Elements[0] || Object == Elements[1];
        }

        template<typename Func>
        void ForEach(Func&& Callback) const
        {
            if (HasSet())
            {
                if (const FCompactObjectSetView* View = GetCompactSet())
                {
                    for (int32 Index = 0; Index < View->Num; ++Index)
                    {
                        Callback(View->Data[Index]);
                    }
                }
                else if (const TSet<UObjectBase*>* Set = GetSet())
                {
                    for (UObjectBase* Obj : *Set)
                    {
                        Callback(Obj);
                    }
                }
                return;
            }
            if (Elements[0])
            {
                Callback(static_cast<UObjectBase*>(Elements[0]));
            }
            if (Elements[1])
            {
                Callback(static_cast<UObjectBase*>(Elements[1]));
            }
        }
    };

    // Wrapper for the engine's FUObjectHashTables singleton (4.8+). The member layout comes from UVTD,
    // the map value types changed over time so the versioned getters are dispatched on the engine version:
    //   ObjectOuterMap: TSet<UObjectBase*> below 4.16, FHashBucket from 4.16
    //   ClassToObjectListMap: TSet<UObjectBase*> below 4.22, FHashBucket from 4.22
    //   HashOuter: UObjectBase* values below 5.0, GUObjectArray indices from 5.0
    //   PackageToObjectListMap and ObjectToPackageMap only exist from 4.26
    struct RC_UE_API FUObjectHashTables
    {
    #include <MemberVariableLayout_HeaderWrapper_FUObjectHashTables.hpp>

    public:
        static FUObjectHashTables& Get();

        // Get was found and this engine version has a dumped layout. Does not prove the layout matches
        // the running build, so this only means the self test is worth attempting.
        static bool IsConfigured();
        // True once the self test has walked the tables and matched a GUObjectArray scan. Callers that
        // want to use the tables must check this, not IsConfigured.
        static bool IsAvailable();
        static bool HasPackageMaps();
        // Takes either FUObjectHashTables::Get or the singleton itself. Builds that inlined the
        // accessor away have no Get to point at, so a signature can only supply the object.
        static void SetupGetAddress(void* Address);

        // Compares hash table lookups against GUObjectArray scans and logs the results.
        // On a mismatch the tables are marked unavailable so callers fall back to the array. Returns true on success.
        static bool RunSelfTest();

        // Shifts the dumped shipping offsets for a non-shipping 5.0+ build (TBucketMap ReadOnlyLock).
        static void ApplyNonShippingBucketShift();

        // The engine guards the tables with the critical section at offset 0, take it while reading them from another thread
        void Lock() const;
        void Unlock() const;

        struct FScopedLock
        {
            const FUObjectHashTables& Tables;
            explicit FScopedLock(const FUObjectHashTables& InTables) : Tables(InTables) { Tables.Lock(); }
            ~FScopedLock() { Tables.Unlock(); }
        };

        // Number of classes that currently have at least one instance
        int32 NumClassEntries() const;

        template<typename Func>
        void ForEachObjectOfClass(UClass* Class, Func&& Callback) const
        {
            if (Version::IsAtLeast(4, 22))
            {
                ForEachInMap(GetClassToObjectListMap422(), Class, Callback);
            }
            else
            {
                ForEachInMap(GetClassToObjectListMapBase(), Class, Callback);
            }
        }

        template<typename Func>
        void ForEachObjectOfClassIncludingDerived(UClass* Class, Func&& Callback) const
        {
            ForEachObjectOfClass(Class, Callback);
            if (const TSet<UClass*>* ChildSet = FindDerivedClasses(Class))
            {
                for (UClass* ChildClass : *ChildSet)
                {
                    ForEachObjectOfClassIncludingDerived(ChildClass, Callback);
                }
            }
        }

        template<typename Func>
        void ForEachObjectWithOuter(UObjectBase* Outer, Func&& Callback) const
        {
            if (Version::IsAtLeast(4, 16))
            {
                ForEachInMap(GetObjectOuterMap416(), Outer, Callback);
            }
            else
            {
                ForEachInMap(GetObjectOuterMapBase(), Outer, Callback);
            }
        }

        template<typename Func>
        void ForEachObjectWithOuterRecursive(UObjectBase* Outer, Func&& Callback) const
        {
            ForEachObjectWithOuter(Outer, [&](UObjectBase* Child) {
                Callback(Child);
                ForEachObjectWithOuterRecursive(Child, Callback);
            });
        }

        template<typename Func>
        void ForEachObjectInPackage(UPackage* Package, Func&& Callback) const
        {
            if (HasPackageMaps())
            {
                ForEachInMap(GetPackageToObjectListMap(), Package, Callback);
            }
        }

        // External package of an object, nullptr if it has none or the engine predates package maps
        UPackage* GetExternalPackage(UObjectBase* Object) const;

        template<typename Func>
        void ForEachObjectWithNameHash(int32 Hash, Func&& Callback) const
        {
            ForEachInMap(GetHash(), Hash, Callback);
        }

        TSet<UClass*>* FindDerivedClasses(UClass* Class);
        const TSet<UClass*>* FindDerivedClasses(UClass* Class) const;

        template<typename Func>
        void ForEachDerivedClass(UClass* Class, Func&& Callback) const
        {
            if (const TSet<UClass*>* ChildSet = FindDerivedClasses(Class))
            {
                for (UClass* ChildClass : *ChildSet)
                {
                    Callback(ChildClass);
                    ForEachDerivedClass(ChildClass, Callback);
                }
            }
        }

        // Hash table lookup by name. With an outer this uses HashOuter, without one it only matches
        // top level packages through Hash, mirroring StaticFindObjectFastInternal.
        UObject* StaticFindObjectFast(UClass* ObjectClass, UObject* ObjectOuter, FName ObjectName, bool bExactClass = false) const;

        // Same lookup without taking the table lock. Only for callers already holding it, or the self
        // test probe which must not hold a lock it could leak if a mismatched layout faults.
        UObject* StaticFindObjectFastNoLock(UClass* ObjectClass, UObject* ObjectOuter, FName ObjectName, bool bExactClass = false) const;

        // Same lookup with a GUObjectArray scan fallback, used when the hash tables are unavailable or for comparison
        static UObject* StaticFindObject(UClass* ObjectClass, UObject* ObjectOuter, FName ObjectName, bool bExactClass = false, bool bUseHashTables = true);

        // Resolves a full path such as /Script/Engine.PlayerController or /Script/Engine.PlayerController:Function
        static UObject* StaticFindObjectByPath(UClass* ObjectClass, const CharType* InPath, bool bExactClass = false, bool bUseHashTables = true);

    private:
        template<typename Func>
        static void ForEachInBucket(const FHashBucket& Bucket, Func&& Callback)
        {
            Bucket.ForEach(Callback);
        }

        template<typename Func>
        static void ForEachInBucket(const TSet<UObjectBase*>& Bucket, Func&& Callback)
        {
            for (UObjectBase* Obj : Bucket)
            {
                Callback(Obj);
            }
        }

        template<typename MapType, typename KeyType, typename Func>
        static void ForEachInMap(const MapType& Map, KeyType Key, Func&& Callback)
        {
            if (const auto* Bucket = Map.Find(Key))
            {
                ForEachInBucket(*Bucket, Callback);
            }
        }
    };

    // Name hash used as the key into Hash and HashOuter. Must match the engine's GetObjectHash.
    // 4.8 to 4.15: (index ^ number) masked to OBJECT_HASH_BINS
    // 4.16 to 4.22: index ^ number
    // 4.23+: GetTypeHash(FName)
    inline int32 GetObjectHash(FName ObjName)
    {
        if (Version::IsAtLeast(4, 23))
        {
            return GetTypeHash(ObjName);
        }
        const int32 Hash = static_cast<int32>(ObjName.GetComparisonIndex().ToUnstableInt() ^ ObjName.GetNumber());
        if (Version::IsAtLeast(4, 16))
        {
            return Hash;
        }
        return Hash & ((1024 * 1024) - 1);
    }

    // Name plus outer hash used as the key into HashOuter. Must match the engine's GetObjectOuterHash.
    // 4.8 to 4.15: ((index ^ number) ^ (outer >> 4)) masked to OBJECT_HASH_BINS
    // 4.16 to 4.22: (index ^ number) ^ (outer >> 6)
    // 4.23+: GetTypeHash(FName) + (outer >> 6)
    inline int32 GetObjectOuterHash(FName ObjName, PTRINT Outer)
    {
        if (Version::IsAtLeast(4, 23))
        {
            return GetTypeHash(ObjName) + static_cast<int32>(Outer >> 6);
        }
        const int32 NameHash = static_cast<int32>(ObjName.GetComparisonIndex().ToUnstableInt() ^ ObjName.GetNumber());
        if (Version::IsAtLeast(4, 16))
        {
            return NameHash ^ static_cast<int32>(Outer >> 6);
        }
        return (NameHash ^ static_cast<int32>(Outer >> 4)) & ((1024 * 1024) - 1);
    }
}
