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
#include <DynamicOutput/DynamicOutput.hpp>
#include <chrono>
#include <cstdlib>
#include <vector>

#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>

namespace RC::Unreal
{
#include <MemberVariableLayout_SrcWrapper_FUObjectHashTables.hpp>

    static void* GFUObjectHashTablesGetAddress = nullptr;
    static FUObjectHashTables* GFUObjectHashTablesSingleton = nullptr;

    enum class ESelfTestState
    {
        NotRun,
        Passed,
        Failed,
    };
    static ESelfTestState GSelfTestState = ESelfTestState::NotRun;

    // The resolved address is FUObjectHashTables::Get on builds that have one, and the singleton itself
    // on builds where the compiler inlined the accessor away (every 4.8 shipping build does, which is why
    // the 4.8 PDBs carry the local static and its guard but no Get symbol). Executable memory is the
    // function, anything else is the object.
    void FUObjectHashTables::SetupGetAddress(void* Address)
    {
        GFUObjectHashTablesGetAddress = nullptr;
        GFUObjectHashTablesSingleton = nullptr;
        if (!Address)
        {
            return;
        }

        MEMORY_BASIC_INFORMATION Info{};
        constexpr DWORD ExecuteMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        const bool IsCode = VirtualQuery(Address, &Info, sizeof(Info)) == sizeof(Info) && (Info.Protect & ExecuteMask) != 0;
        if (IsCode)
        {
            GFUObjectHashTablesGetAddress = Address;
        }
        else
        {
            GFUObjectHashTablesSingleton = static_cast<FUObjectHashTables*>(Address);
        }
    }

    // Get was found and this engine version has a dumped layout. Does not mean the layout matches the
    // running build: non-shipping builds add fields to TBucketMap that shift the members, so the tables
    // are only actually used once the self test has walked them and matched a GUObjectArray scan.
    bool FUObjectHashTables::IsConfigured()
    {
        // 4.7 has no singleton at all
        return (GFUObjectHashTablesGetAddress != nullptr || GFUObjectHashTablesSingleton != nullptr) &&
               MemberOffsets.contains(STR("ClassToObjectListMap"));
    }

    bool FUObjectHashTables::IsAvailable()
    {
        return IsConfigured() && GSelfTestState == ESelfTestState::Passed;
    }

    // FCriticalSection is a CRITICAL_SECTION on Windows in every supported version, and it is the first member
    void FUObjectHashTables::Lock() const
    {
        EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(const_cast<FUObjectHashTables*>(this)));
    }

    void FUObjectHashTables::Unlock() const
    {
        LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(const_cast<FUObjectHashTables*>(this)));
    }

    namespace
    {
        struct FSelfTest
        {
            bool Failed = false;

            using Clock = std::chrono::steady_clock;
            static double Ms(Clock::time_point Start)
            {
                return std::chrono::duration<double, std::milli>(Clock::now() - Start).count();
            }

            // Objects can be created or destroyed by the game while the two passes run, allow a little drift
            void CompareCounts(const CharType* What, int32 HashCount, double HashMs, int32 ArrayCount, double ArrayMs)
            {
                const int32 Tolerance = std::max(2, ArrayCount / 100);
                const bool Ok = HashCount > 0 && std::abs(HashCount - ArrayCount) <= Tolerance;
                Output::send<LogLevel::Normal>(STR("[HashTables] {}: hash {} in {:.3f} ms, array {} in {:.3f} ms{}\n"), What, HashCount, HashMs, ArrayCount, ArrayMs, Ok ? STR("") : STR("  MISMATCH"));
                Failed |= !Ok;
            }

            void CompareObjects(const CharType* What, UObject* HashResult, UObject* ArrayResult, double HashMs, double ArrayMs)
            {
                const bool Ok = HashResult == ArrayResult && ArrayResult != nullptr;
                Output::send<LogLevel::Normal>(STR("[HashTables] {}: hash {:#x} in {:.3f} ms, array {:#x} in {:.3f} ms{}\n"),
                                               What,
                                               reinterpret_cast<uintptr_t>(HashResult),
                                               HashMs,
                                               reinterpret_cast<uintptr_t>(ArrayResult),
                                               ArrayMs,
                                               Ok ? STR("") : STR("  MISMATCH"));
                Failed |= !Ok;
            }
        };
    }

    namespace
    {
        struct FHashProbe
        {
            int32 NumClasses = 0;
            int32 UClassCount = 0;
            int32 ActorDerivedCount = 0;
            int32 OuterCount = 0;
            UObject* CorePackage = nullptr;
            UObject* ObjectClass = nullptr;
        };

        // Reads every hash table the runtime path touches, with no lock held. A build whose real layout
        // does not match the dumped offsets (a Test or Development build adds ReadOnlyLock to each
        // TBucketMap and shifts the members) faults here instead of returning a wrong answer, and the
        // structured handler turns that into a clean false. No FScopedLock is taken so a fault mid walk
        // cannot leak the engine critical section. Contains no objects that need unwinding.
        static bool ProbeHashTablesGuarded(const FUObjectHashTables& Tables,
                                           UClass* ClassClass,
                                           UClass* ActorClass,
                                           UObject* CorePackageFromArray,
                                           FName CoreName,
                                           FName ObjectName,
                                           FHashProbe& Out)
        {
            __try
            {
                Out.NumClasses = Tables.NumClassEntries();
                Tables.ForEachObjectOfClass(ClassClass, [&](UObjectBase*) { ++Out.UClassCount; });
                Tables.ForEachObjectOfClassIncludingDerived(ActorClass, [&](UObjectBase*) { ++Out.ActorDerivedCount; });
                Out.CorePackage = Tables.StaticFindObjectFastNoLock(nullptr, nullptr, CoreName);
                if (CorePackageFromArray)
                {
                    Tables.ForEachObjectWithOuter(CorePackageFromArray, [&](UObjectBase*) { ++Out.OuterCount; });
                    Out.ObjectClass = Tables.StaticFindObjectFastNoLock(nullptr, CorePackageFromArray, ObjectName);
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
    }

    // Non-shipping 5.0+ adds an int32 ReadOnlyLock (padded to 8) to each TBucketMap, which the dumped
    // shipping offsets lack. A member shifts by 8 per TBucketMap before it. Applied before the offset
    // getters cache; the self test then checks it against a GUObjectArray scan.
    void FUObjectHashTables::ApplyNonShippingBucketShift()
    {
        // Member name and number of TBucketMaps before it, in offset order.
        static const std::pair<const CharType*, int32> PrecedingBuckets[] = {
            {STR("Hash"), 0},
            {STR("HashOuter"), 1},
            {STR("ObjectOuterMap"), 1},
            {STR("ClassToObjectListMap"), 2},
            {STR("ClassToChildListMap"), 3},
            {STR("ClassToChildListMapVersion"), 3},
            {STR("AllClassesVersion"), 3},
            {STR("NativeClassesVersion"), 3},
            {STR("PackageToObjectListMap"), 3},
            {STR("ObjectToPackageMap"), 4},
            {STR("HashId"), 4},
        };
        for (const auto& [Name, Preceding] : PrecedingBuckets)
        {
            if (auto It = MemberOffsets.find(Name); It != MemberOffsets.end())
            {
                It->second += Preceding * static_cast<int32>(sizeof(void*));
            }
        }
    }

    bool FUObjectHashTables::RunSelfTest()
    {
        if (!IsConfigured())
        {
            GSelfTestState = ESelfTestState::Failed;
            Output::send<LogLevel::Warning>(STR("[HashTables] Unavailable: Get {}, layout {}\n"),
                                            (GFUObjectHashTablesGetAddress || GFUObjectHashTablesSingleton) ? STR("found") : STR("not found"),
                                            MemberOffsets.contains(STR("ClassToObjectListMap")) ? STR("dumped") : STR("missing"));
            return false;
        }

        using Clock = FSelfTest::Clock;
        const FUObjectHashTables& Tables = Get();

        // Correct the dumped shipping offsets for a non-shipping 5.0+ build before any getter caches them
        if (Version::NonShipping && Version::IsAtLeast(5, 0))
        {
            ApplyNonShippingBucketShift();
            Output::send<LogLevel::Normal>(STR("[HashTables] Non-shipping build: shifted TBucketMap members for the ReadOnlyLock field\n"));
        }

        // The singleton is a function local static, so it lives in the main module. Anything else means
        // the Get address points at some other accessor and the maps must not be walked.
        {
            HMODULE MainModule = GetModuleHandleW(nullptr);
            MODULEINFO Info{};
            const bool InMainModule = MainModule && K32GetModuleInformation(GetCurrentProcess(), MainModule, &Info, sizeof(Info)) &&
                                      reinterpret_cast<uintptr_t>(&Tables) >= reinterpret_cast<uintptr_t>(Info.lpBaseOfDll) &&
                                      reinterpret_cast<uintptr_t>(&Tables) < reinterpret_cast<uintptr_t>(Info.lpBaseOfDll) + Info.SizeOfImage;
            if (!InMainModule)
            {
                GSelfTestState = ESelfTestState::Failed;
                Output::send<LogLevel::Error>(STR("[HashTables] Get did not return the engine singleton (tables at {:#x} are outside the main module), object lookups fall back to GUObjectArray\n"),
                                              reinterpret_cast<uintptr_t>(&Tables));
                return false;
            }
        }

        // Expected values from a full GUObjectArray scan. This never touches the hash tables, so it is
        // always safe and gives the ground truth the hash side is checked against.
        UClass* ClassClass = UClass::StaticClass();
        UClass* ActorClass = AActor::StaticClass();
        UClass* PackageClass = UPackage::StaticClass();
        const FName CoreName(STR("/Script/CoreUObject"));
        const FName ObjectName(STR("Object"));
        int32 ArrayUClass = 0;
        int32 ArrayActor = 0;
        UObject* CorePackage = nullptr;
        auto ArrayStart = Clock::now();
        UObjectGlobals::ForEachUObject([&](UObject* Object, int32, int32) {
            if (Object->GetClassPrivate() == ClassClass)
            {
                ++ArrayUClass;
            }
            if (Object->IsA(ActorClass))
            {
                ++ArrayActor;
            }
            if (!CorePackage && Object->GetNamePrivate() == CoreName && Object->GetOuterPrivate() == nullptr && Object->IsA(PackageClass))
            {
                CorePackage = Object;
            }
            return LoopAction::Continue;
        });
        int32 ArrayOuter = 0;
        UObject* ArrayObjectClass = nullptr;
        if (CorePackage)
        {
            UObjectGlobals::ForEachUObject([&](UObject* Object, int32, int32) {
                if (Object->GetOuterPrivate() == CorePackage)
                {
                    ++ArrayOuter;
                    if (!ArrayObjectClass && Object->GetNamePrivate() == ObjectName && Object->IsA(ClassClass))
                    {
                        ArrayObjectClass = Object;
                    }
                }
                return LoopAction::Continue;
            });
        }
        const double ArrayMs = FSelfTest::Ms(ArrayStart);

        // Hash side, guarded so a layout mismatch falls back instead of crashing
        FHashProbe Probe{};
        auto HashStart = Clock::now();
        const bool Ran = ProbeHashTablesGuarded(Tables, ClassClass, ActorClass, CorePackage, CoreName, ObjectName, Probe);
        const double HashMs = FSelfTest::Ms(HashStart);

        Output::send<LogLevel::Normal>(STR("[HashTables] {} at {:#x}, tables at {:#x}, package maps {}\n"),
                                       GFUObjectHashTablesSingleton ? STR("Singleton") : STR("Get"),
                                       reinterpret_cast<uintptr_t>(GFUObjectHashTablesSingleton ? static_cast<void*>(GFUObjectHashTablesSingleton)
                                                                                                : GFUObjectHashTablesGetAddress),
                                       reinterpret_cast<uintptr_t>(&Tables),
                                       HasPackageMaps() ? STR("present") : STR("absent"));

        if (!Ran)
        {
            GSelfTestState = ESelfTestState::Failed;
            Output::send<LogLevel::Error>(STR("[HashTables] Faulted while reading the tables. Either the resolved address is not the hash tables or the dumped layout does not match this build (Test/Development builds shift the members). Object lookups fall back to GUObjectArray\n"));
            return false;
        }

        FSelfTest Test{};
        Test.CompareCounts(STR("UClass instances"), Probe.UClassCount, HashMs, ArrayUClass, ArrayMs);
        Test.CompareCounts(STR("AActor instances including derived"), Probe.ActorDerivedCount, HashMs, ArrayActor, ArrayMs);
        Test.CompareCounts(STR("Objects with outer /Script/CoreUObject"), Probe.OuterCount, HashMs, ArrayOuter, ArrayMs);
        Test.CompareObjects(STR("Package /Script/CoreUObject"), Probe.CorePackage, CorePackage, HashMs, ArrayMs);
        Test.CompareObjects(STR("Class /Script/CoreUObject.Object"), Probe.ObjectClass, ArrayObjectClass, HashMs, ArrayMs);

        if (Probe.NumClasses <= 0 || Probe.NumClasses > 4'000'000)
        {
            Output::send<LogLevel::Error>(STR("[HashTables] Implausible class count {}\n"), Probe.NumClasses);
            Test.Failed = true;
        }

        if (Test.Failed)
        {
            GSelfTestState = ESelfTestState::Failed;
            Output::send<LogLevel::Error>(STR("[HashTables] Self test failed, object lookups fall back to GUObjectArray\n"));
            return false;
        }
        GSelfTestState = ESelfTestState::Passed;
        Output::send<LogLevel::Normal>(STR("[HashTables] Self test passed ({} classes with instances)\n"), Probe.NumClasses);
        return true;
    }

    bool FUObjectHashTables::HasPackageMaps()
    {
        return MemberOffsets.contains(STR("PackageToObjectListMap"));
    }

    FUObjectHashTables& FUObjectHashTables::Get()
    {
        if (GFUObjectHashTablesSingleton)
        {
            return *GFUObjectHashTablesSingleton;
        }
        using GetFunc = FUObjectHashTables& (*)();
        auto Func = reinterpret_cast<GetFunc>(GFUObjectHashTablesGetAddress);
        return Func();
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
