#include <Unreal/FOutputDevice.hpp>
#include <Unreal/UObjectHashTables.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/UPackage.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/VersionedContainer/Container.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <String/StringType.hpp>
#ifdef _WIN32
#include <DynamicOutput/DynamicOutput.hpp>
#endif
#include <chrono>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#endif

namespace RC::Unreal
{
#include <MemberVariableLayout_SrcWrapper_FUObjectHashTables.hpp>

#ifdef _WIN32
#include "UObjectHashTablesWindows.inl"
#endif

    bool FUObjectHashTables::HasPackageMaps()
    {
        return MemberOffsets.contains(STR("PackageToObjectListMap"));
    }

    int32 FUObjectHashTables::NumClassEntries() const
    {
        return Version::IsAtLeast(4, 22) ? GetClassToObjectListMap422().Num() : GetClassToObjectListMapBase().Num();
    }

    UPackage* FUObjectHashTables::GetExternalPackage(UObjectBase* Object) const
    {
        if (!HasPackageMaps())
        {
            return nullptr;
        }
        auto* Result = GetObjectToPackageMap().Find(Object);
        return Result ? *Result : nullptr;
    }

    TSet<UClass*>* FUObjectHashTables::FindDerivedClasses(UClass* Class)
    {
        return GetClassToChildListMap().Find(Class);
    }

    const TSet<UClass*>* FUObjectHashTables::FindDerivedClasses(UClass* Class) const
    {
        return const_cast<FUObjectHashTables*>(this)->FindDerivedClasses(Class);
    }

    static bool MatchesClass(UObject* Object, UClass* ObjectClass, bool bExactClass)
    {
        if (!ObjectClass)
        {
            return true;
        }
        return bExactClass ? Object->GetClassPrivate() == ObjectClass : Object->IsA(ObjectClass);
    }

    UObject* FUObjectHashTables::StaticFindObjectFast(UClass* ObjectClass, UObject* ObjectOuter, FName ObjectName, bool bExactClass) const
    {
        FScopedLock ScopedLock(*this);
        return StaticFindObjectFastNoLock(ObjectClass, ObjectOuter, ObjectName, bExactClass);
    }

    UObject* FUObjectHashTables::StaticFindObjectFastNoLock(UClass* ObjectClass, UObject* ObjectOuter, FName ObjectName, bool bExactClass) const
    {
        if (ObjectOuter != nullptr)
        {
            const int32 Hash = GetObjectOuterHash(ObjectName, reinterpret_cast<PTRINT>(ObjectOuter));
            auto Matches = [&](UObject* Object) {
                return Object && Object->GetNamePrivate() == ObjectName && Object->GetOuterPrivate() == ObjectOuter && MatchesClass(Object, ObjectClass, bExactClass);
            };

            if (Version::IsAtLeast(5, 0))
            {
                // HashOuter values are GUObjectArray indices
                for (auto It = GetHashOuter500().CreateConstKeyIterator(Hash); It; ++It)
                {
                    FUObjectItem* Item = FUObjectArray::IndexToObject(static_cast<int32>(It.Value()));
                    UObject* Object = Item ? Item->GetUObject() : nullptr;
                    if (Matches(Object))
                    {
                        return Object;
                    }
                }
            }
            else
            {
                // HashOuter values are the objects themselves
                for (auto It = GetHashOuterBase().CreateConstKeyIterator(Hash); It; ++It)
                {
                    auto* Object = static_cast<UObject*>(It.Value());
                    if (Matches(Object))
                    {
                        return Object;
                    }
                }
            }
            return nullptr;
        }

        // No outer means a top level package, which only Hash can find
        UObject* Result = nullptr;
        ForEachObjectWithNameHash(GetObjectHash(ObjectName), [&](UObjectBase* ObjBase) {
            if (Result)
            {
                return;
            }
            auto* Object = static_cast<UObject*>(ObjBase);
            if (Object->GetNamePrivate() == ObjectName && Object->GetOuterPrivate() == nullptr && MatchesClass(Object, ObjectClass, bExactClass))
            {
                Result = Object;
            }
        });
        return Result;
    }

    UObject* FUObjectHashTables::StaticFindObject(UClass* ObjectClass, UObject* ObjectOuter, FName ObjectName, bool bExactClass, bool bUseHashTables)
    {
        if (bUseHashTables && IsAvailable())
        {
            return Get().StaticFindObjectFast(ObjectClass, ObjectOuter, ObjectName, bExactClass);
        }

        UObject* Result = nullptr;
        UObjectGlobals::ForEachUObject([&](UObject* Object, int32, int32) {
            if (Object->GetNamePrivate() != ObjectName || Object->GetOuterPrivate() != ObjectOuter || !MatchesClass(Object, ObjectClass, bExactClass))
            {
                return LoopAction::Continue;
            }
            Result = Object;
            return LoopAction::Break;
        });
        return Result;
    }

    UObject* FUObjectHashTables::StaticFindObjectByPath(UClass* ObjectClass, const CharType* InPath, bool bExactClass, bool bUseHashTables)
    {
        if (!InPath || InPath[0] == '\0')
        {
            return nullptr;
        }

        File::StringType Path(InPath);

        // Strip a class prefix: Class'/Script/Engine.PlayerController' or Border /Engine/Transient.Object
        size_t QuoteStart = Path.find(STR('\''));
        if (QuoteStart != File::StringType::npos)
        {
            size_t QuoteEnd = Path.rfind(STR('\''));
            if (QuoteEnd != File::StringType::npos && QuoteEnd > QuoteStart)
            {
                Path = Path.substr(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
            }
        }
        else
        {
            size_t SpaceSlash = Path.find(STR(" /"));
            if (SpaceSlash != File::StringType::npos)
            {
                Path = Path.substr(SpaceSlash + 1);
            }
        }

        // The rightmost . or : separates the object name from its outer path
        size_t LastDot = Path.rfind(STR('.'));
        size_t LastColon = Path.rfind(STR(':'));
        size_t LastDelim = File::StringType::npos;
        if (LastDot != File::StringType::npos && LastColon != File::StringType::npos)
        {
            LastDelim = (LastDot > LastColon) ? LastDot : LastColon;
        }
        else if (LastDot != File::StringType::npos)
        {
            LastDelim = LastDot;
        }
        else if (LastColon != File::StringType::npos)
        {
            LastDelim = LastColon;
        }

        if (LastDelim == File::StringType::npos)
        {
            return StaticFindObject(ObjectClass, nullptr, FName(Path.c_str()), bExactClass, bUseHashTables);
        }

        File::StringType OuterPath = Path.substr(0, LastDelim);
        File::StringType ObjectNameStr = Path.substr(LastDelim + 1);
        if (ObjectNameStr.empty())
        {
            return nullptr;
        }

        UObject* Outer = StaticFindObjectByPath(nullptr, OuterPath.c_str(), false, bUseHashTables);
        if (!Outer)
        {
            return nullptr;
        }
        return StaticFindObject(ObjectClass, Outer, FName(ObjectNameStr.c_str()), bExactClass, bUseHashTables);
    }
}
