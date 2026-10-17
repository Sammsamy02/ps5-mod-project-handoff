#include <L22Diagnostics/Logging.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/UPackage.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/PackageName.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/VersionedContainer/Container.hpp>
#include <Unreal/Searcher/ObjectSearcher.hpp>
#include <Unreal/Searcher/ObjectSearcherProfiler.hpp>
#include <Unreal/ClassListener.hpp>
#include <Unreal/UObjectHashTables.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <DynamicOutput/Output.hpp>
#include <memory>
#include <new>

namespace RC::Unreal::UObjectGlobals
{
    RC_UE_API Function<UObject*(StaticConstructObject_Internal_Params_Base)> GlobalState::StaticConstructObjectInternalBase{};
    RC_UE_API Function<UObject*(StaticConstructObject_Internal_Params_411)> GlobalState::StaticConstructObjectInternal411{};
    RC_UE_API Function<UObject*(const FStaticConstructObjectParameters&)> GlobalState::StaticConstructObjectInternal426{};

    auto SetupStaticConstructObjectInternalAddress(void* FunctionAddress) -> void
    {
        GlobalState::StaticConstructObjectInternal426.assign_address(FunctionAddress);
        GlobalState::StaticConstructObjectInternal411.assign_address(FunctionAddress);
        GlobalState::StaticConstructObjectInternalBase.assign_address(FunctionAddress);
    }

    namespace Below411
    {
        static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
        {
            // Pre-4.11: No EInternalObjectFlags, no bAssumeTemplateIsArchetype, no ExternalPackage
            return GlobalState::StaticConstructObjectInternalBase(
                    const_cast<UClass*>(Params.Class),
                    Params.Outer,
                    Params.Name,
                    Params.SetFlags,
                    Params.Template,
                    Params.bCopyTransientsFromClassDefaults,
                    Params.InstanceGraph
            );
        }
    }

    namespace Below426
    {
        static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
        {
            // 4.11-4.25: Has EInternalObjectFlags, bAssumeTemplateIsArchetype, ExternalPackage
            return GlobalState::StaticConstructObjectInternal411(
                    Params.Class,
                    Params.Outer,
                    Params.Name,
                    Params.SetFlags,
                    Params.InternalSetFlags,
                    Params.Template,
                    Params.bCopyTransientsFromClassDefaults,
                    Params.InstanceGraph,
                    Params.bAssumeTemplateIsArchetype,
                    Params.ExternalPackage
            );
        }
    }
    namespace Below56
    {
        static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
        {
            static Function<UObject*(const FStaticConstructObjectParameters&)> StaticConstructObjectInternal = [&]() {
                return GlobalState::StaticConstructObjectInternal426.get_function_address();
            }();

            if (!StaticConstructObjectInternal.is_ready())
            {
                L22Diagnostics::error("StaticConstructObject", "native function is not ready");
                return nullptr;
            }
            if (Params.Class == nullptr)
            {
                L22Diagnostics::error("StaticConstructObject", "class is null");
                return nullptr;
            }

            try
            {
                auto* result = StaticConstructObjectInternal(Params);
                if (!result) L22Diagnostics::error("StaticConstructObject", "native function returned null");
                return result;
            }
            catch (const std::exception& error)
            {
                L22Diagnostics::error("StaticConstructObject", error.what());
                throw;
            }
            catch (...)
            {
                L22Diagnostics::error("StaticConstructObject", "non-standard C++ exception");
                throw;
            }
        }
    }
    namespace Above55
    {
        static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
        {
            static Function<UObject*(const FStaticConstructObjectParameters&)> StaticConstructObjectInternal = [&]() {
                return GlobalState::StaticConstructObjectInternal426.get_function_address();
            }();

            if (!StaticConstructObjectInternal.is_ready())
            {
                L22Diagnostics::error("StaticConstructObject", "native function is not ready");
                return nullptr;
            }
            if (Params.Class == nullptr)
            {
                L22Diagnostics::error("StaticConstructObject", "class is null");
                return nullptr;
            }

            try
            {
                auto* result = StaticConstructObjectInternal(Params);
                if (!result) L22Diagnostics::error("StaticConstructObject", "native function returned null");
                return result;
            }
            catch (const std::exception& error)
            {
                L22Diagnostics::error("StaticConstructObject", error.what());
                throw;
            }
            catch (...)
            {
                L22Diagnostics::error("StaticConstructObject", "non-standard C++ exception");
                throw;
            }
        }
    }

    auto StaticConstructObject(const FStaticConstructObjectParameters& GenericParams) -> UObject*
    {
        if (Version::IsBelow(4, 11))
        {
            return Below411::StaticConstructObject(GenericParams);
        }
        else if (Version::IsBelow(4, 26))
        {
            return Below426::StaticConstructObject(GenericParams);
        }
        else if (Version::IsBelow(5, 6))
        {
            Below56::FStaticConstructObjectParameters Params{};
            Params.Class = GenericParams.Class;
            Params.Outer = GenericParams.Outer;
            Params.Name = GenericParams.Name;
            Params.SetFlags = GenericParams.SetFlags;
            Params.InternalSetFlags = GenericParams.InternalSetFlags;
            Params.Template = GenericParams.Template;
            Params.bCopyTransientsFromClassDefaults = GenericParams.bCopyTransientsFromClassDefaults;
            Params.InstanceGraph = GenericParams.InstanceGraph;
            Params.bAssumeTemplateIsArchetype = GenericParams.bAssumeTemplateIsArchetype;
            Params.ExternalPackage = GenericParams.ExternalPackage;
            return Below56::StaticConstructObject(Params);
        }
        else
        {
            Above55::FStaticConstructObjectParameters Params{};
            Params.Class = GenericParams.Class;
            Params.Outer = GenericParams.Outer;
            Params.Name = GenericParams.Name;
            Params.SetFlags = GenericParams.SetFlags;
            Params.InternalSetFlags = GenericParams.InternalSetFlags;
            Params.Template = GenericParams.Template;
            Params.bCopyTransientsFromClassDefaults = GenericParams.bCopyTransientsFromClassDefaults;
            Params.InstanceGraph = GenericParams.InstanceGraph;
            Params.bAssumeTemplateIsArchetype = GenericParams.bAssumeTemplateIsArchetype;
            Params.ExternalPackage = GenericParams.ExternalPackage;
            return Above55::StaticConstructObject(Params);
        }
    }

    // [L27][ET nul-throw-armor] (2026-10-09, Claude fixes 1-3 for the
    // mods-started=0 blocker): the engine-side FName constructor takes
    // `const CharType*` and scans to NUL — NameTypes.hpp's StringViewType
    // overload forwards `str_name.data()` with NO length. Every string_view
    // SLICE handed to an FName ctor is therefore over-read: the
    // "/Script/Engine" part sliced from "/Script/Engine.Actor" was interned
    // as the WHOLE "/Script/Engine.Actor" string, FNAME_Find missed, and the
    // miss THREW out of the first long-path lookup in on_program_start (R3
    // carrier arming), aborting install/start of every Lua mod. This helper
    // is the call-site law: an owned, explicitly-sized (NUL-terminated by
    // construction) copy before EVERY FName construction from a view.
    static auto NulTerminatedNameCopy(StringViewType View) -> StringType
    {
        if (View.empty()) { return StringType{}; }
        return StringType{View.data(), View.size()};
    }

    auto StaticFindObject_InternalSlow([[maybe_unused]]UClass* ObjectClass, [[maybe_unused]]UObject* InObjectPackage, const CharType* OrigInName, [[maybe_unused]]bool bExactClass) -> UObject*
    {
        UObject* FoundObject{};

        UObjectGlobals::ForEachUObject([&](UObject* Object, [[maybe_unused]]int32_t ChunkIndex, [[maybe_unused]]int32_t ObjectIndex) {
            // This call to 'get_full_name' is a problem because it relies on offsets already being found
            // This function is called before offsets have been found as a way to check if required objects have been initialized
            auto ObjFullName = Object->GetFullName();
            auto ObjFullNameNoType = ObjFullName.substr(ObjFullName.find(STR(" ")) + 1);

            if (String::iequal(ObjFullNameNoType, OrigInName))
            {
                FoundObject = static_cast<UObject*>(Object);
                return LoopAction::Break;
            }
            else
            {
                return LoopAction::Continue;
            }
        });

        return FoundObject;
    }

    auto StaticFindObject_InternalNoToStringFromNames(const std::vector<FName>& NameParts) -> UObject*
    {
        UObject* FoundObject{};

        for (const auto& NamePart : NameParts)
        {
            if (NamePart == NAME_None)
            {
                // NAME_None means we're not far enough along engine init for this object to exist yet.
                return nullptr;
            }
        }


        UObjectGlobals::ForEachUObject([&](UObject* Object, [[maybe_unused]]int32_t ChunkIndex, [[maybe_unused]]int32_t ObjectIndex) {
            // In order to remain safe to use early in init before we've hooked FName::ToString up to KismetStringLibrary:Conv_NameToString, we have to
            // compare FNames directly instead of using GetFullName.
            int32_t NumPathParts{};
            auto PathObject = Object;
            while (PathObject)
            {
                const auto PathName = PathObject->GetNamePrivate();
                const auto It = std::ranges::find_if(NameParts, [&](const FName NamePart) {
                    return NamePart.Equals(PathName);
                });
                if (It == NameParts.end())
                {
                    return LoopAction::Continue;
                }
                else
                {
                    PathObject = PathObject->GetOuterPrivate();
                    ++NumPathParts;
                }
            }
            if (NumPathParts == NameParts.size())
            {
                FoundObject = Object;
                return LoopAction::Break;
            }
            else
            {
                return LoopAction::Continue;
            }
        });

        return FoundObject;
    }

    auto StaticFindObject_InternalNoToStringFromStrings(const std::vector<StringViewType>& NameParts) -> UObject*
    {
        std::vector<FName> Names{};
        for (const auto& NamePart : NameParts)
        {
            // [L27][ET nul-throw-armor] FIX 1: NUL-terminated owned copy
            // before the ctor — a view part is never over-read into the
            // following path text (the in-tree callers pass whole literals,
            // but this seam accepts arbitrary slices).
            Names.emplace_back(NulTerminatedNameCopy(NamePart), FNAME_Find);
        }
        return StaticFindObject_InternalNoToStringFromNames(Names);
    }

    // [B1 memo fix] Split into its per-OBJECT part (flags) and its per-CLASS
    // part (IsA chain tests) so the FindXOf walk memo can cache only the
    // class part. IsValidObjectForFindXOf is unchanged in result and order.
    static auto IsExcludedObjectForFindXOf(UObject* object) -> bool
    {
        return object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject));
    }

    static auto IsValidClassForFindXOf(UObject* object) -> bool
    {
        return !object->IsA<UClass>() && !object->IsA<UFunction>() && !object->IsA<UPackage>() && !object->IsA<UEnum>() &&
            !object->IsA<UScriptStruct>();
    }

    auto IsValidObjectForFindXOf(UObject* object) -> bool
    {
        return !IsExcludedObjectForFindXOf(object) && IsValidClassForFindXOf(object);
    }

    // [L27][ET nul-throw-armor] Claude addendum fallback: when a long-name
    // part still isn't found after the NUL fix, do NOT refuse — run a bounded
    // GUObjectArray walk comparing pool-decoded full names
    // (StaticFindObject_InternalSlow's proven mechanism; GetFullName is now
    // the console-verified in-port pool decoder). First-match only, the match
    // is logged, and a decode failure for ONE object ends only that object's
    // comparison. Returns nullptr on a clean miss (upstream FindObject
    // semantics — never throws into callers).
    static auto StaticFindObject_DecodedFullNameWalk(const CharType* InName, UClass* Class, bool bExactClass) -> UObject*
    {
        UObject* FoundObject{};
        UObjectGlobals::ForEachUObject([&](UObject* Object, [[maybe_unused]]int32_t ChunkIndex, [[maybe_unused]]int32_t ObjectIndex) {
            try
            {
                auto ObjFullName = Object->GetFullName();
                auto ObjFullNameNoType = ObjFullName.substr(ObjFullName.find(STR(" ")) + 1);
                if (String::iequal(ObjFullNameNoType, InName))
                {
                    if (Class && bExactClass && Class != Object->GetClassPrivate())
                    {
                        return LoopAction::Continue;
                    }
                    FoundObject = Object;
                    return LoopAction::Break;
                }
            }
            catch (...)
            {
                // One object's decode failure ends only that comparison.
            }
            return LoopAction::Continue;
        });
        if (FoundObject)
        {
            Output::send(STR("[StaticFindObject] decoded full-name walk matched '{}'\n"), StringViewType{InName});
        }
        return FoundObject;
    }

    // l26 layer-3 (agent-EC; 2026-10-08): in-port long-name splitter for the
    // StaticFindObject console-safe path. Pure string work, no engine calls.
    // The package part KEEPS its slashes -- a UPackage FName is
    // "/Script/Engine" as a single name (the part shape the existing
    // StaticFindObject_InternalNoToStringFromStrings call sites in
    // UnrealInitializer.cpp already use) -- and the object/subobject tail is
    // split on '.' and ':' into outer-chain parts.
    //   "/Script/Engine.PlayerController"       -> {"/Script/Engine", "PlayerController"}
    //   "/Script/Engine"                        -> {"/Script/Engine"}   (package itself; no throw)
    //   "/Game/Maps/L1.L_Persistent:Prop"       -> {"/Game/Maps/L1", "L_Persistent", "Prop"}
    static auto SplitLongNameIntoPathParts(const CharType* InName) -> std::vector<StringViewType>
    {
        const StringViewType Name{InName};
        if (Name.empty()) { return {}; }
        const auto LastSlash = Name.find_last_of(STR("/\\"));
        const auto ObjectStart = (LastSlash == StringViewType::npos) ? static_cast<std::size_t>(0) : (LastSlash + 1);
        // First '.' or ':' after the last '/' separates package from object.
        const auto Sep = Name.find_first_of(STR(".:"), ObjectStart);
        if (Sep == StringViewType::npos)
        {
            // Package-only path (or a corrupted dot-less long name): a single
            // part; the walk resolves the package object or the loud refusal
            // below names the part.
            return {Name};
        }
        std::vector<StringViewType> Parts{};
        Parts.reserve(2);
        Parts.emplace_back(Name.substr(0, Sep));
        const auto Tail = Name.substr(Sep + 1);
        for (std::size_t Begin = 0; Begin < Tail.size();)
        {
            const auto Next = Tail.find_first_of(STR(".:"), Begin);
            const auto End = (Next == StringViewType::npos) ? Tail.size() : Next;
            if (End > Begin) { Parts.emplace_back(Tail.substr(Begin, End - Begin)); }
            if (Next == StringViewType::npos) { break; }
            Begin = Next + 1;
        }
        return Parts;
    }

    UObject* FindObject(UClass* Class, UObject* InOuter, File::StringViewType InName, bool bExactClass, ObjectSearcher* InSearcher)
    {
        return FindObject(Class, InOuter, FromCharTypePtr<TCHAR>(InName.data()), bExactClass, InSearcher);
    }

    UObject* FindObject(UClass* Class, UObject* InOuter, const TCHAR* InName, bool bExactClass, ObjectSearcher* InSearcher)
    {
        bool bObjectIsCached{};
        if (!Class && !InOuter && InName && !bExactClass)
        {
            if (auto CachedObject = GetGlobalObject(ToCharTypePtr(InName)); CachedObject)
            {
                return CachedObject;
            }
        }

        if (!InName)
        {
            throw std::runtime_error{"Call to FindObject with no InName is not allowed"};
        }

        // The engine hash tables resolve a full path, or a name under a known outer, without a scan.
        // A miss falls through to the scan below because the scan also matches objects nested deeper than InOuter.
        if (!InSearcher && UnrealInitializer::ShouldUseHashTables())
        {
            const bool bAnyOuter = !InOuter || InOuter == ANY_PACKAGE;
            const bool bShortName = FPackageName::IsShortPackageName(ToCharTypePtr(InName));
            if (bAnyOuter && !bShortName)
            {
                if (auto* Found = FUObjectHashTables::StaticFindObjectByPath(Class, ToCharTypePtr(InName), bExactClass))
                {
                    return Found;
                }
            }
            else if (!bAnyOuter && bShortName)
            {
                if (auto* Found = FUObjectHashTables::StaticFindObject(Class, InOuter, FName(ToCharTypePtr(InName), FNAME_Add), bExactClass))
                {
                    return Found;
                }
            }
        }

        auto GetPackageNameFromLongName = [](const File::StringType& LongName) -> File::StringType
        {
            // l26 layer-3 (agent-EC): a long name without '.' (package-only
            // paths like "/Script/Engine", or a name whose dot was lost on
            // the console bridge) used to hard-throw here -- every Lua
            // StaticFindObject full-path call surfaced
            // "GetPackageNameFromLongName: Name wasn't long." (zzdiag v7
            // S1e/S2e/S3e/U2e). The scan-side package filter now degrades to
            // an empty string; long names resolve through
            // SplitLongNameIntoPathParts + the FNAME_Find walk below.
            auto DelimiterOffset = LongName.find(STR("."));
            if (DelimiterOffset == LongName.npos)
            {
                return File::StringType{};
            }
            return LongName.substr(0, DelimiterOffset);
        };

        UObject* FoundObject{nullptr};
        const bool bAnyPackage = InOuter == ANY_PACKAGE;
        UObject* ObjectPackage = bAnyPackage ? nullptr : InOuter;
        const bool bIsLongName = !FPackageName::IsShortPackageName(ToCharTypePtr(InName));

        // l26 layer-3 (agent-EC): console-safe long-name resolution. The old
        // scan leaned on two mechanisms that do not work on the PS5 target:
        // FNAME_Add admission for PackageName/ShortName (GATE-1
        // numbered-duplicate indices break index-only Equals) and
        // GetFullName() (FName::ToString through the KismetStringLibrary CDO,
        // which itself needs this very lookup). Instead: split the class-less
        // full name in-port, resolve every part FNAME_Find-only against the
        // live pool (readable per the GATE-1 pin notes), refuse LOUDLY when a
        // part is not in the pool, and match the outer chain with the
        // existing StaticFindObject_InternalNoToStringFromNames walk.
        // [L27][ET nul-throw-armor] FIX 1: every part is constructed from an
        // OWNED NUL-terminated copy (NulTerminatedNameCopy) — the engine ctor
        // takes const CharType* and scans to NUL, so the raw slice
        // "/Script/Engine" of "/Script/Engine.Actor" was previously interned
        // as the whole "/Script/Engine.Actor" string (the console refusal
        // "'/Script/Engine' not in FName pool"). FIX 2: a part miss is now a
        // loud LOG + the decoded full-name walk fallback (Claude addendum),
        // NEVER a throw into the caller — the throw propagated out of the
        // first long-path lookup in on_program_start (R3 carrier arming) and
        // aborted install/start of every Lua mod (mods-started=0). Clean
        // not-found is nullptr (upstream FindObject semantics).
        if (bIsLongName && !InSearcher)
        {
            const auto PathParts = SplitLongNameIntoPathParts(ToCharTypePtr(InName));
            std::vector<FName> PartNames{};
            PartNames.reserve(PathParts.size());
            bool bPartMissedPool{};
            for (const auto& Part : PathParts)
            {
                const StringType PartCopy{NulTerminatedNameCopy(Part)};
                FName PartName{PartCopy, FNAME_Find};
                if (PartName.IsNone() && Part != StringViewType{STR("None")})
                {
                    // Loud refusal (log only): "this exact part is not in the
                    // FName pool" (corrupted / dot-less / adversarial input)
                    // is different from a clean miss — named, then the
                    // decoded-walk fallback below still tries to resolve the
                    // object; never refuses outright, never throws.
                    bPartMissedPool = true;
                    Output::send<LogLevel::Error>(
                            STR("[StaticFindObject] long-name part '{}' not in FName pool (FNAME_Find miss); trying decoded full-name walk\n"), PartCopy);
                    break;
                }
                PartNames.emplace_back(PartName);
            }
            UObject* PathFound{};
            if (!bPartMissedPool)
            {
                PathFound = StaticFindObject_InternalNoToStringFromNames(PartNames);
            }
            else
            {
                // [L27][ET] Claude addendum: bounded GUObjectArray walk
                // comparing pool-decoded full names; first-match, logged.
                PathFound = StaticFindObject_DecodedFullNameWalk(ToCharTypePtr(InName), Class, bExactClass);
            }
            if (PathFound)
            {
                if (!Class || !bExactClass || Class == PathFound->GetClassPrivate())
                {
                    CacheGlobalObject(PathFound);
                    return PathFound;
                }
                // Exact-class mismatch on a path-named object: not the
                // caller's object; the scan below may still find one.
            }
            // Clean miss: fall through to the scan below; its package
            // extraction no longer throws (empty package degrades the outer
            // filter, it never reaches GetFullName for name-gated objects).
        }

        FName ShortName = bIsLongName ? NAME_None : FName(ToCharTypePtr(InName), FNAME_Add);
        FName PackageName = bIsLongName ? FName(GetPackageNameFromLongName(ToCharTypePtr(InName)), FNAME_Add) : NAME_None;

        if (bIsLongName)
        {
            auto NameView = StringViewType{InName};
            auto LastColonDelimiter = NameView.find_last_of(STR(':'));
            auto LastDotDelimiter = NameView.find_last_of(STR('.'));
            if (LastDotDelimiter == NameView.npos && LastColonDelimiter == NameView.npos)
            {
                // Name only contains path. (Full view over the NUL-terminated
                // InName buffer — safe without a copy; the sites below are
                // mid-string SLICES and get the [L27][ET] NUL-terminated
                // NulTerminatedNameCopy before the ctor.)
                ShortName = FName(NameView, FNAME_Add);
            }
            else if (LastColonDelimiter == NameView.npos)
            {
                // Only dots, so the short name should be after the last dot.
                ShortName = FName(NulTerminatedNameCopy(NameView.substr(LastDotDelimiter + 1)), FNAME_Add);
            }
            else if (LastDotDelimiter == NameView.npos)
            {
                // Only colons, so the short name should be after the last colon.
                ShortName = FName(NulTerminatedNameCopy(NameView.substr(LastColonDelimiter + 1)), FNAME_Add);
            }
            else
            {
                // Mix of dots and colons.
                if (LastColonDelimiter > LastDotDelimiter)
                {
                    // Last colon is after the last dot, so the short name should be after the last colon.
                    ShortName = FName(NulTerminatedNameCopy(NameView.substr(LastColonDelimiter + 1)), FNAME_Add);
                }
                else
                {
                    // Last dot is after the last colon, so the short name should be after the last dot.
                    ShortName = FName(NulTerminatedNameCopy(NameView.substr(LastDotDelimiter + 1)), FNAME_Add);
                }
            }
        }

        auto Searcher = [&InSearcher, &Class]() -> ObjectSearcher {
            return InSearcher ? *InSearcher : FindObjectSearcher(Class, nullptr);
        }();

        bool bQuickSearch = Searcher.IsFast();

        // [IT lookup-speed 03, Claude round-13 item 2] Name-first walk order.
        //
        // The old predicate ran, PER OBJECT and BEFORE ever comparing the
        // name: (a) Object->IsA(Class) — a full SuperStruct chain walk, and
        // (b) for the no-outer short-name shape, Object->GetOutermost() — a
        // full OuterPrivate chain walk — compared against PackageName, which
        // is NAME_None for every short-name query. GetOutermost() of a live
        // object is its UPackage (never NAME_None), so that branch
        // continued for EVERY object: the walk could never match anything
        // (the name compare at the bottom only ran for ObjectPackage /
        // bAnyPackage shapes), yet it still scanned all ~35K objects —
        // exactly the measured StaticFindObject-short = 55ms self-test line
        // (PASS valid=false: a guaranteed, expensive miss).
        //
        // Fix, preserving every shape that could match before:
        //   1. Integer name compare FIRST (FName::Equals), so the common
        //      reject costs 1 read + 1 compare.
        //   2. The class checks (exact pointer, then IsA) only on a name
        //      hit — a quick-search guarantee still short-circuits IsA.
        //   3. Outer verification only on a name hit:
        //        - explicit ObjectPackage: same outer-chain walk as before;
        //        - ANY_PACKAGE: no check (same as before);
        //        - no outer at all (the dead branch): match TOP-LEVEL
        //          objects only (GetOuterPrivate() == nullptr). That is the
        //          engine's own StaticFindObjectFastInternal semantics for a
        //          name-only lookup (the Hash table stores name-hits with
        //          outer==null), so short-name lookups now actually resolve
        //          (e.g. "GameEngine" -> the /Script/Engine GameEngine
        //          package) instead of always missing.
        //   4. Long names keep the exact GetFullName verification on the
        //      name-matched candidate only (unchanged behavior, now rare).
        Searcher.ForEach([&](UObject* Object) {
            if (!Object->GetNamePrivate().Equals(ShortName))
            {
                return LoopAction::Continue;
            }

            if (Class && bExactClass && Class != Object->GetClassPrivate()) { return LoopAction::Continue; }

            // If this is a quick search, then the object is guaranteed to be of the specified class.
            // Otherwise, we're searching the entirety of GUObjectArray, and we need to check that the class matches.
            if (Class && !bQuickSearch && !Object->IsA(Class)) { return LoopAction::Continue; }

            if (!bAnyPackage && ObjectPackage)
            {
                bool bIsInOuter{};
                UObject* Outer = Object->GetOuterPrivate();
                do
                {
                    if (Outer == ObjectPackage)
                    {
                        bIsInOuter = true;
                        break;
                    }
                    Outer = Outer->GetOuterPrivate();
                } while (Outer);

                if (!bIsInOuter) { return LoopAction::Continue; }
            }
            else if (!bAnyPackage && !ObjectPackage)
            {
                if (bIsLongName)
                {
                    // Long name: same package filter as the old order — the
                    // outermost package's name must equal the queried
                    // package part (runs now only on the name-matched
                    // candidate, not per object).
                    if (!Object->GetOutermost()->GetNamePrivate().Equals(PackageName)) { return LoopAction::Continue; }
                }
                else
                {
                    // Short name, no outer: the engine's name-only hash
                    // lookup only ever returns top-level objects.
                    if (Object->GetOuterPrivate() != nullptr) { return LoopAction::Continue; }
                }
            }

            if (bIsLongName)
            {
                auto FullName = Object->GetFullName();
                auto ClassLessFullName = FullName.substr(FullName.find(STR(" ")) + 1);
                if (ToCharTypePtr(InName) == ClassLessFullName)
                {
                    FoundObject = Object;
                    return LoopAction::Break;
                }
            }
            else
            {
                FoundObject = Object;
                return LoopAction::Break;
            }

            return LoopAction::Continue;
        });

        if (FoundObject && !bObjectIsCached)
        {
            CacheGlobalObject(FoundObject);
        }

        return FoundObject;
    }

    UObject* FindObject(struct ObjectSearcher& Searcher, UClass* Class, UObject* InOuter, File::StringViewType InName, bool bExactClass)
    {
        return FindObject(Searcher, Class, InOuter, FromCharTypePtr<TCHAR>(InName.data()), bExactClass);
    }

    UObject* FindObject(struct ObjectSearcher& Searcher, UClass* Class, UObject* InOuter, const TCHAR* InName, bool bExactClass)
    {
        return FindObject(Class, InOuter, InName, bExactClass, &Searcher);
    }

    // Classes with this short name, minus any already covered by another match through inheritance.
    static auto FindRootClassesByName(FName ClassName, std::vector<UClass*>& OutRoots) -> void
    {
        std::vector<UClass*> Matches{};
        FindObjectSearcher<UClass, AnySuperStruct>().ForEach([&](UObject* Object) {
            auto* Class = static_cast<UClass*>(Object);
            if (Class->GetNamePrivate().Equals(ClassName))
            {
                Matches.emplace_back(Class);
            }
            return LoopAction::Continue;
        });

        for (UClass* Candidate : Matches)
        {
            bool bCovered{};
            for (UClass* Other : Matches)
            {
                if (Other != Candidate && Candidate->IsChildOf(Other))
                {
                    bCovered = true;
                    break;
                }
            }
            if (!bCovered)
            {
                OutRoots.emplace_back(Candidate);
            }
        }
    }

#ifdef __PROSPERO__
    // l26 layer-1 fix (agent-CP; CO bug #1, CB section 1(a), the
    // AddRequiredObject Find-first workaround precedent in Hooks.cpp):
    // the FindXOf string overloads constructed FName(ClassName) with the
    // default FNAME_Add, which through the bound ctor creates a NEW
    // numbered pool entry instead of retrieving the existing one (the
    // documented FNAME_Add duplicate-name bug). Equals compares
    // ComparisonIndex only, so the duplicated entry never matches any
    // stored class name and the walk returns nullptr for every type.
    // Construct Find-first (the mode init/type-scan proves on console);
    // fall back to Add only on NAME_None so names not yet interned keep
    // the upstream add-new-name semantics (every call site in the tree is
    // a pure lookup: LuaMod/UE4SSProgram/Dumpers/initializer). Host builds
    // keep the original one-line construction.
    // Diagnostic: '[FFO] add-fallback [n:X]' logs only when the Add
    // fallback runs (Find missed), deduped per resulting name index and
    // bounded at 128 sites; a Find hit logs nothing.
    static uint32_t g_ffo_add_fallback_indices[128]{};
    static int32_t g_ffo_add_fallback_count = 0;
    static auto LogFindXOfAddFallback(FName Result) -> void
    {
        try
        {
            const uint32_t add_index = Result.GetComparisonIndex().ToUnstableInt();
            for (int32_t i = 0; i < g_ffo_add_fallback_count; ++i)
            {
                if (g_ffo_add_fallback_indices[i] == add_index) { return; }
            }
            if (g_ffo_add_fallback_count >= 128) { return; }
            g_ffo_add_fallback_indices[g_ffo_add_fallback_count++] = add_index;
            Output::send<LogLevel::Error>(STR("[FFO] add-fallback [n:{:X}]\n"), add_index);
        }
        catch (...)
        {
        }
    }
    static auto MakeFindXOfClassName(StringViewType ClassName) -> FName
    {
        // [L27][ET nul-throw-armor] FIX 1: owned NUL-terminated copy before
        // both constructions — the view's data() is handed to the engine
        // ctor that scans to NUL, so a slice caller would over-read.
        const StringType ClassNameCopy{NulTerminatedNameCopy(ClassName)};
        auto Name = FName(ClassNameCopy, FNAME_Find);
        if (Name.IsNone())
        {
            Name = FName(ClassNameCopy, FNAME_Add);
            LogFindXOfAddFallback(Name);
        }
        return Name;
    }

    // l26 layer-2 verified-query fix (agent-DA; CR N1 rank-1 + CW sections
    // 5/6.1, CY TEMPLATE-C string-mode variant): the CP Find-first
    // construction resolves (non-None, so '[FFO] add-fallback' stays
    // silent) yet the resulting ComparisonIndex can still never equal any
    // stored NamePrivate index once the engine pool has grown past
    // asset-load (the ctor-thunk Find/shard decode is the suspect; the
    // root adapter fix is separate). Round-trip the constructed FName
    // through the port's own FName::ToString (the same machinery
    // GetFullName and the init type-scan prove on console) and compare it
    // to the original query string; on mismatch, run the init-proven
    // string-compare walk instead of FName::Equals. The chunked walk, the
    // seeds, the GUObjectArray binding, the exclusion filters and the
    // adapter thunk are untouched; the fast Equals path is kept whenever
    // the round-trip matches. The fallback fires only on mismatch
    // (slower but correct; mod workloads do a handful of lookups).
    // Diagnostic: '[FFO] query-fallback [n:X]' logs once per constructed
    // name index when the fallback engages, so names-resolve-but-mismatch
    // is visible instead of silent.
    // l27 (agent-EB, fname-rootfix): the marker now also prints the
    // constructed Number and a bounded ToString rendering. Byte-level ground
    // truth (DECISION-MEMO.md): slot 2's sret decode is CORRECT, Find/Add
    // share one id space, and a suffix-less Find can only return {0,0} or
    // {id,0}. Therefore on the next launch exactly ONE of these shapes is
    // possible, and each names the real bug:
    //   ts:'<other name>'  -> the ctor INPUT string was corrupted before the
    //                          call (ToCharTypePtr/ensure_str family; v7's
    //                          S1/S2/S3/U2 all threw GetPackageNameFromLongName
    //                          on dotted paths, proving that layer mangles).
    //   ts:'' or garbage   -> n:X is likely a VALID id and the legacy
    //                          Conv_NameToString route (the mismatch
    //                          detector's ToString) is the liar.
    //   num:!=0            -> the only decode-side failure shape (a rebind
    //                          of slot 2 to the raw core 0x242470, whose
    //                          return IS id|(id<<32)); expected 0 here.
    // l27-supersede (agent-ER, Claude item 2): with the PURE IN-PORT pool
    // decoder owning FName::ToString, the ts field is the REAL decoded
    // string of the constructed index (first 96 chars — practically every
    // UE path name renders whole). The marker only fires when the
    // round-trip MISMATCHED the query, so the fast-path-vs-fallback
    // decision is now directly visible:
    //   marker NEVER fires on a launch -> the round-trip passed everywhere;
    //     the indices were always right and ToString WAS THE ONLY BUG
    //     (Claude's suspected resolution of the N1 theory).
    //   marker fires with ts == a real other name -> the constructed index
    //     really was wrong (genuine N1) and the string fallback did its job.
    //   marker fires with ts == '' -> the decoder was refused/unpublished
    //     and the legacy Kismet route lied (see the '[FN] pool-decoder ...'
    //     markers for which and why).
    static uint32_t g_ffo_query_fallback_indices[128]{};
    static int32_t g_ffo_query_fallback_count = 0;
    // [L27][EV string-seam] (agent-EV 2026-10-09, Claude task 5): the
    // query-side tripwire. A stride-corrupted wide buffer carries embedded
    // NUL units ('P',0,'l',0,...); the FName construction seam now refuses
    // such input loudly, and this audit does the same for the QUERY VIEW
    // ITSELF — before any construction — so the next console log directly
    // discriminates 'query arrived corrupt' (this marker) from 'ctor input
    // corrupt' ([FN] seam refused) from 'indices wrong' (plain
    // query-fallback with clean ts). The string-walk fallback also can never
    // match an embedded-NUL query, so the marker explains the nil honestly.
    static uint32_t g_ffo_query_nul_notified = 0;
    static auto LogFindXOfQueryEmbeddedNul(StringViewType Query) -> void
    {
        try
        {
            if (g_ffo_query_nul_notified >= 8) { return; }
            ++g_ffo_query_nul_notified;
            uint32_t first_nul = 0;
            while (first_nul < Query.size() && Query[first_nul] != 0) { ++first_nul; }
            // First up-to-8 units as hex, NUL included — the byte-level shape.
            uint32_t units[8]{};
            const size_t shown = Query.size() < 8 ? Query.size() : 8;
            for (size_t i = 0; i < shown; ++i) { units[i] = static_cast<uint32_t>(Query[i]); }
            Output::send<LogLevel::Error>(STR("[FFO] query-embedded-NUL (len={} at={} units={:X} {:X} {:X} {:X} {:X} {:X} {:X} {:X}; the wide query buffer carries the stride-corruption signature)\n"),
                                          Query.size(), first_nul,
                                          units[0], units[1], units[2], units[3], units[4], units[5], units[6], units[7]);
        }
        catch (...)
        {
        }
    }
    static auto FindXOfQueryHasEmbeddedNul(StringViewType Query) -> bool
    {
        for (const auto unit : Query)
        {
            if (unit == 0) { return true; }
        }
        return false;
    }
    static auto LogFindXOfQueryFallback(FName Result, StringViewType Query) -> void
    {
        try
        {
            const uint32_t query_index = Result.GetComparisonIndex().ToUnstableInt();
            for (int32_t i = 0; i < g_ffo_query_fallback_count; ++i)
            {
                if (g_ffo_query_fallback_indices[i] == query_index) { return; }
            }
            if (g_ffo_query_fallback_count >= 128) { return; }
            g_ffo_query_fallback_indices[g_ffo_query_fallback_count++] = query_index;
            auto rendered = ensure_str(Result.ToString());
            // Claude item 2: carry the DECODED string (the round-trip's
            // ToString output, real via the in-port decoder) — 96 chars
            // covers every real path name; only hostile input truncates.
            if (rendered.size() > 96) { rendered = rendered.substr(0, 96); }
            // [EI] auto& not char&: on Prospero ensure_str returns the WIDE
            // StringType (engine-compiled fixup of the EB host-checked draft;
            // char& cannot bind to wchar_t elements there).
            for (auto& c : rendered)
            {
                if (c == '\n' || c == '\r') { c = ' '; } // one line per marker (wide-char log writer law)
            }
            // [L27][EV string-seam] (task 5): also carry the ORIGINAL query
            // length and first units — never only the decoded constructed
            // name, which may itself be one character from the same seam
            // corruption this marker exists to expose.
            uint32_t q0 = 0, q1 = 0, q2 = 0, q3 = 0;
            if (Query.size() > 0) { q0 = static_cast<uint32_t>(Query[0]); }
            if (Query.size() > 1) { q1 = static_cast<uint32_t>(Query[1]); }
            if (Query.size() > 2) { q2 = static_cast<uint32_t>(Query[2]); }
            if (Query.size() > 3) { q3 = static_cast<uint32_t>(Query[3]); }
            Output::send<LogLevel::Error>(STR("[FFO] query-fallback [n:{:X} num:{} ts:'{}' qlen={} q={:X} {:X} {:X} {:X}]\n"),
                                          query_index, Result.GetNumber(), rendered, Query.size(), q0, q1, q2, q3);
        }
        catch (...)
        {
        }
    }
    static auto FindXOfQueryMismatch(FName Name, StringViewType Query) -> bool
    {
        try
        {
            return !String::iequal(StringViewType{Name.ToString()}, Query);
        }
        catch (...)
        {
            // ToString unavailable/unusable (e.g. pre-init): treat as
            // mismatch so the string walk (which catches per object) runs
            // rather than trusting an unverified index.
            return true;
        }
    }
    // Init-scan comparator shape (StaticFindObject_InternalSlow's proven
    // mechanism): compare the STORED name's string, never an FName index.
    static auto FindXOfStoredNameMatchesQuery(UStruct* Struct, StringViewType Query) -> bool
    {
        try
        {
            return String::iequal(StringViewType{Struct->GetNamePrivate().ToString()}, Query);
        }
        catch (...)
        {
            return false;
        }
    }
    // String-compare mirror of the FindFirstOf(FName) walk body (direct
    // class match + super-struct chain, same IsValidObjectForFindXOf
    // placement); only the compare mode differs.
    static auto FindFirstOfStringWalk(StringViewType Query) -> UObject*
    {
        UObject* ObjectFound{nullptr};

        UObjectGlobals::ForEachUObject([&](UObject* Object, [[maybe_unused]]int32_t ChunkIndex, [[maybe_unused]]int32_t ObjectIndex) {
            UClass* Class = Object->GetClassPrivate();

            if (FindXOfStoredNameMatchesQuery(Class, Query) && IsValidObjectForFindXOf(Object))
            {
                ObjectFound = Object;
                return LoopAction::Break;

            }

            if (!IsValidObjectForFindXOf(Object)) { return LoopAction::Continue; }

            for (UStruct* super_struct : TSuperStructRange(Class))
            {
                if (FindXOfStoredNameMatchesQuery(super_struct, Query))
                {
                    ObjectFound = Object;
                    break;
                }
            }

            return LoopAction::Continue;
        });

        return ObjectFound;
    }
    // String-compare mirror of the FindAllOf walk bodies. With the filter,
    // mirrors FindAllOf(FName) (IsValidObjectForFindXOf gates both direct
    // and super matches); without it, mirrors the cache-scan arm which
    // applies no FindXOf exclusion.
    static auto FindAllOfStringWalk(StringViewType Query, std::vector<UObject*>& OutStorage, bool bApplyFindXOfFilter) -> void
    {
        UObjectGlobals::ForEachUObject([&](UObject* Object, [[maybe_unused]]int32_t ChunkIndex, [[maybe_unused]]int32_t ObjectIndex) {
            if (!Object) { return LoopAction::Continue; }

            UClass* Class = Object->GetClassPrivate();
            if (!Class) { return LoopAction::Continue; }

            if (!bApplyFindXOfFilter || IsValidObjectForFindXOf(Object))
            {
                if (FindXOfStoredNameMatchesQuery(Class, Query))
                {
                    OutStorage.emplace_back(Object);
                    return LoopAction::Continue;
                }

                for (UStruct* SuperStruct : TSuperStructRange(Class))
                {
                    if (FindXOfStoredNameMatchesQuery(SuperStruct, Query))
                    {
                        OutStorage.emplace_back(Object);
                        break;
                    }
                }
            }

            return LoopAction::Continue;
        });
    }
    // Verified wrappers: construct (CP Find-first), ToString-verify, then
    // either the fast Equals walk (round-trip matched) or the string walk
    // above. The public FName overloads are unchanged.
    static auto FindFirstOfVerified(StringViewType ClassName) -> UObject*
    {
        // [L27][EV string-seam]: query-side audit first — the comparison
        // below stays against the ORIGINAL query view (never a decoded
        // constructed name), so a corrupt query must be named, not chased.
        if (FindXOfQueryHasEmbeddedNul(ClassName))
        {
            LogFindXOfQueryEmbeddedNul(ClassName);
            return nullptr;
        }
        const auto Name = MakeFindXOfClassName(ClassName);
        if (FindXOfQueryMismatch(Name, ClassName))
        {
            LogFindXOfQueryFallback(Name, ClassName);
            return FindFirstOfStringWalk(ClassName);
        }
        return FindFirstOf(Name);
    }
    static auto FindAllOfVerified(StringViewType ClassName, std::vector<UObject*>& OutStorage) -> void
    {
        if (FindXOfQueryHasEmbeddedNul(ClassName))
        {
            LogFindXOfQueryEmbeddedNul(ClassName);
            return;
        }
        const auto Name = MakeFindXOfClassName(ClassName);
        if (FindXOfQueryMismatch(Name, ClassName))
        {
            LogFindXOfQueryFallback(Name, ClassName);
            return FindAllOfStringWalk(ClassName, OutStorage, true);
        }
        FindAllOf(Name, OutStorage);
    }
#endif

#ifdef __PROSPERO__
    // [FFO2] (agent-FE 2026-10-09, Claude find-path oracle for the next
    // launch): one-shot per-distinct-query summary of the FindXOf
    // FName-overload walks — the path the Lua FName bridge ([LFB]) and
    // the verified string wrappers both land in. Fields per the directive:
    //   scanned=0            -> object-array iteration problem;
    //   scanned>0, matched=0 -> compare (index) or SuperStruct-chain
    //                            problem. NOTE: the walk predicate is
    //                            FName::Equals = ComparisonIndex equality
    //                            ONLY (NameTypes.hpp) — Number is NEVER
    //                            compared in these walks, so only an index
    //                            mismatch can lose a match here.
    // exact counts DIRECT class-name Equals hits regardless of the
    // IsValidObjectForFindXOf acceptance filter; viaSuper counts matches
    // found only through the TSuperStructRange chain; first is the first
    // accepted object pointer or 0. Fields added beyond the directive's
    // literal shape: num= (query FName Number — rules the '+Number'
    // variant in/out), found= (FindAllOf appended count; FindFirstOf
    // lines omit it), arm= (walk|hash|cache — the hash arm's exact counts
    // matched ROOT CLASSES via FindRootClassesByName and never walks a
    // per-object super chain, so its viaSuper is structurally 0).
    // q= is rendered by the PURE in-port pool decoder
    // (FNamePoolDecoder::try_tostring, RC_UE_API in NameTypes.hpp — the
    // same primitive the [LFB] bridge uses; NEVER engine ToString); on a
    // decoder refusal q='' while id= still carries the comparison index.
    // Deduped per distinct (ComparisonIndex, Number) per session, bounded
    // 16-slot fixed table (ring-refuses past 16), counters are locals.
    struct FindXOfFfo2Stats
    {
        uint32_t scanned{0};
        uint32_t exact{0};
        uint32_t via_super{0};
        const char* arm{"walk"};
    };
    struct FindXOfFfo2Key
    {
        uint32_t index{0};
        uint32_t number{0};
    };
    static FindXOfFfo2Key g_ffo2_logged[16]{};
    static int32_t g_ffo2_logged_count = 0;
    static auto LogFindXOfFfo2(FName ClassName, const FindXOfFfo2Stats& Stats, UObject* First, int64_t FoundCount) -> void
    {
        try
        {
            const uint32_t id = ClassName.GetComparisonIndex().ToUnstableInt();
            const uint32_t number = static_cast<uint32_t>(ClassName.GetNumber());
            for (int32_t i = 0; i < g_ffo2_logged_count; ++i)
            {
                if (g_ffo2_logged[i].index == id && g_ffo2_logged[i].number == number) { return; }
            }
            if (g_ffo2_logged_count >= 16) { return; }
            g_ffo2_logged[g_ffo2_logged_count++] = FindXOfFfo2Key{id, number};

            StringType rendered{};
            if (!FNamePoolDecoder::try_tostring(ClassName, rendered)) { rendered.clear(); }
            if (rendered.size() > 64) { rendered.resize(64); }
            for (auto& c : rendered)
            {
                if (c == '\n' || c == '\r') { c = ' '; }
                else if (c < 32 || c > 126) { c = '?'; }
            }

            if (FoundCount >= 0)
            {
                Output::send<LogLevel::Error>(STR("[FFO2] q='{}' id=0x{:X} num={} scanned={} exact={} viaSuper={} found={} first=0x{:X} arm={}\n"),
                                              rendered, id, number, Stats.scanned, Stats.exact, Stats.via_super,
                                              static_cast<uint32_t>(FoundCount), reinterpret_cast<uintptr_t>(First), ensure_str(Stats.arm));
            }
            else
            {
                Output::send<LogLevel::Error>(STR("[FFO2] q='{}' id=0x{:X} num={} scanned={} exact={} viaSuper={} first=0x{:X} arm={}\n"),
                                              rendered, id, number, Stats.scanned, Stats.exact, Stats.via_super,
                                              reinterpret_cast<uintptr_t>(First), ensure_str(Stats.arm));
            }
        }
        catch (...)
        {
        }
    }
#endif

    // =====================================================================================
    // [IT lookup-speed 02, Claude round-13 item 2] Per-walk class-verdict memo.
    //
    // COST MEASURED (self-test 2026-10-11, evidence/acceptance-pass2-20261012.log):
    //   FindFirstOf 78ms, FindAllOf 78-86ms over [FFO2] scanned=35479 objects
    //   = ~2.2us per object. The per-object FName compare is NOT the cost:
    //   FName::Equals is ComparisonIndex equality (NameTypes.hpp:471-474, a
    //   u32 compare; no string decode anywhere in the FName-overload walk —
    //   decoding only happens per CALL in FindXOfQueryMismatch, and per
    //   object only in the string-walk FALLBACK arm). The cost is
    //   IsValidObjectForFindXOf: SIX full UStruct::SuperStruct chain walks
    //   per object (IsA<UClass/UFunction/UPackage/UEnum/UScriptStruct> +
    //   HasAnyFlags' virtual versioned-container call), plus the
    //   TSuperStructRange query walk re-walking the SAME chain a 7th time.
    //   ~30-40 dependent, scattered loads per object; at ~60-100ns per
    //   dependent miss that is the whole 2.2us.
    //
    // FIX: both predicates depend ONLY on the object's class pointer. A
    // session has a few thousand DISTINCT classes for ~35K objects, so a
    // direct-mapped {UClass*, verdict} table turns the six chain walks into
    // one hot probe and the query-chain walk into one more probe. The walk
    // keeps upstream's exact control flow (first direct match breaks; a
    // super-chain match records and keeps scanning), only the two predicate
    // evaluations are memoized. Collision in the direct-mapped table just
    // recomputes (correct, never wrong).
    // =====================================================================================
    //
    // [B1 memo fix, 2026-10-12] The staged patch memoized the WHOLE
    // IsValidObjectForFindXOf verdict per class, but its first half is
    // per-OBJECT (RF_ClassDefaultObject | RF_ArchetypeObject): a class's CDO
    // (usually visited before any instance) would have cached "invalid" for
    // the class and hidden every real instance. Only the class part (the IsA
    // chain tests) is memoized now; the flags are read for every object.
    // The table also moved off the stack (32KB is too much for a game or mod
    // thread frame) to a nothrow heap block; if that allocation fails the
    // walk runs un-memoized (the pre-B1 path), it never throws.
    // =====================================================================================
    struct FFindXofClassVerdict
    {
        UClass* Class{nullptr};
        int8_t ClassPartValid{-1};      // -1 unknown, 0 invalid, 1 valid (IsValidClassForFindXOf)
        int8_t ChainMatchesQuery{-1};   // -1 unknown, 0 no, 1 yes (this walk's query)
    };
    struct FFindXofClassMemo
    {
        // 2048 entries * 16B = 32KB (heap); power of two for the mask. A
        // collision recompute costs one un-memoized verdict, never a wrong one.
        static constexpr uint32_t TableSize = 2048;
        FFindXofClassVerdict Slots[TableSize]{};
    };

    // Slot for Class, reset when the slot held another class; nullptr when
    // the walk has no memo (allocation failed).
    static auto FindXofMemoSlot(FFindXofClassMemo* Memo, UClass* Class) -> FFindXofClassVerdict*
    {
        if (!Memo) { return nullptr; }
        // UObject heap pointers are 16B-aligned+ and scattered; >>6 mixes.
        const uint32_t Key = (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Class) >> 6)) & (FFindXofClassMemo::TableSize - 1);
        FFindXofClassVerdict* Slot = &Memo->Slots[Key];
        if (Slot->Class != Class) { *Slot = FFindXofClassVerdict{Class, -1, -1}; }
        return Slot;
    }

    // Same result as IsValidObjectForFindXOf(Object), same evaluation order:
    // the per-object flags every time, the class part from the memo.
    static auto FindXofValid(UObject* Object, FFindXofClassVerdict* Slot) -> bool
    {
        if (!Slot) { return IsValidObjectForFindXOf(Object); }
        if (IsExcludedObjectForFindXOf(Object)) { return false; }
        if (Slot->ClassPartValid < 0) { Slot->ClassPartValid = IsValidClassForFindXOf(Object) ? 1 : 0; }
        return Slot->ClassPartValid != 0;
    }

    // Memoized "does Class's TSuperStructRange contain ClassName" (the exact
    // loop the walk ran per object before).
    static auto FindXofChainContainsQuery(UClass* Class, FFindXofClassVerdict* Slot, FName ClassName) -> bool
    {
        if (Slot && Slot->ChainMatchesQuery >= 0) { return Slot->ChainMatchesQuery != 0; }
        bool bFound = false;
        for (UStruct* SuperStruct : TSuperStructRange(Class))
        {
            if (SuperStruct->GetNamePrivate().Equals(ClassName))
            {
                bFound = true;
                break;
            }
        }
        if (Slot) { Slot->ChainMatchesQuery = bFound ? 1 : 0; }
        return bFound;
    }

    // =====================================================================================
    // [R9-IDX] Object index for FindFirstOf/FindAllOf (generic; no new engine address).
    // The full walk reads every UObject (~160k on Townfall, ~30-40 ms, memory bound); PC
    // UE4SS answers from hash tables / a listener-fed cache in ~1 ms. This index stays
    // EXACT for new objects using only console-proven mechanisms:
    //   - a new object either APPENDS (index >= the NumElements we last synced) or REUSES
    //     an index the engine freed, and every free is reported first to the engine-registered
    //     delete listener (FShutdownDeleteListener::NotifyUObjectDeleted -> r9_index_note_deleted);
    //   - each sync therefore scans only [last NumElements, current NumElements) + the freed
    //     indices (re-checked until reused). Never the whole array again.
    // Results are re-validated at query time exactly like the walk (slot still holds the object,
    // not unreachable, IsValidObjectForFindXOf, class chain contains the query) and returned in
    // index order (the walk's order). Storage lives on the engine allocator (GMalloc), never
    // the small libc heap. Any inconsistency disables the index and the walk is used (fail closed).
    // =====================================================================================
    static uint64_t g_r9_sync_calls_fwd{0}, g_r9_sync_ticks_fwd{0}; // [R9-PERF] Sync() cost (see r9_take_index_stats)
    static uint64_t g_r9_watch_fwd{0}; // [R9-PERF] freed/empty slots re-checked per sync (last sync)
    static uint64_t g_r9_watch_cut_fwd{0}; // [R9-PERF] budgeted syncs that stopped before re-checking every watched slot
    namespace r9idx
    {
        template <typename T>
        struct EngineAlloc
        {
            using value_type = T;
            EngineAlloc() noexcept = default;
            template <typename U>
            EngineAlloc(const EngineAlloc<U>&) noexcept
            {
            }
            auto allocate(size_t n) -> T*
            {
                if (GMalloc == nullptr || *GMalloc == nullptr) { throw std::bad_alloc{}; }
                void* p = (*GMalloc)->Malloc(n * sizeof(T), 0);
                if (p == nullptr) { throw std::bad_alloc{}; }
                return static_cast<T*>(p);
            }
            auto deallocate(T* p, size_t) noexcept -> void
            {
                if (p != nullptr && GMalloc != nullptr && *GMalloc != nullptr) { (*GMalloc)->Free(p); }
            }
            template <typename U>
            auto operator==(const EngineAlloc<U>&) const noexcept -> bool { return true; }
            template <typename U>
            auto operator!=(const EngineAlloc<U>&) const noexcept -> bool { return false; }
        };
        template <typename T>
        using Vec = std::vector<T, EngineAlloc<T>>;

        struct State
        {
            std::mutex Mutex{};
            bool Built{false};
            bool Disabled{false};
            uint64_t DeletesSeen{0};   // >0 proves the engine delete listener is registered and live
            int32_t SyncedNum{0};
            uint32_t QueryStamp{0};
            Vec<UObject*> ObjAt{};
            Vec<UClass*> ClassAt{};
            Vec<uint32_t> SeenStamp{}; // per index: last query that kept it (de-duplication)
            Vec<UClass*> ListedUnder{}; // per index: class list it is currently pushed to
            Vec<int32_t> Dirty{};      // freed indices reported since the last sync
            Vec<int32_t> FreeWatch{};  // empty / unreachable slots, re-checked every sync until filled
            Vec<uint8_t> InWatch{};    // per index: 1 while the index is listed in FreeWatch (at most once)
            Vec<UClass*> Classes{};
            Vec<int32_t> ClassSelfIndex{}; // the class object's own array index (liveness check)
            Vec<uint64_t> ClassNameKey{};  // the class's own FName (comparison + number) at insert
            Vec<uint8_t> ClassDead{};
            Vec<Vec<int32_t>> ClassIndices{};
            Vec<UClass*> HashKeys{};
            Vec<int32_t> HashVals{};
            // [R9-IDX2] per query name: the class positions that match it (exact or via a super),
            // extended incrementally for classes added since; cleared when a class address is reused.
            struct NameEntry
            {
                uint32_t Comparison{0};
                int32_t Number{0};
                size_t Upto{0};
                Vec<int32_t> Matches{};
                Vec<uint8_t> Exact{};
                Vec<int32_t> Pending{}; // classes unreachable when examined: re-examined next query
            };
            Vec<NameEntry> Names{};
            // [R9-NOTIFY] indices that received a NEW object since the last collect (only after
            // the first build, only while a NotifyOnNewObject consumer exists)
            bool NotifyWanted{false};
            Vec<int32_t> NewIdx{};
            Vec<UObject*> NewObj{}; // the object seen in that slot (a reused slot is re-checked by pointer)
        };
        static State* const g_state = new State{};
        static bool g_logged{false};

        static auto ItemAt(int32_t Index) -> FUObjectItem*
        {
            const auto& ObjObjects = GUObjectArray->GetObjObjects();
            static const auto ItemSize = FUObjectItem::UEP_TotalSize();
            const int32_t ChunkIndex = Index / TUObjectArray::NumElementsPerChunk;
            if (Index < 0 || ChunkIndex >= ObjObjects.GetNumChunks()) { return nullptr; }
            const auto& ChunksPtr = ObjObjects.GetObjects();
            return std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(ChunksPtr[ChunkIndex])[(Index % TUObjectArray::NumElementsPerChunk) * ItemSize]);
        }
        // [R9-IDX3] direct slot reads for the index (launch 9: after a level change ~tens of thousands
        // of freed slots are re-checked every sync, and the by-name accessor lookups cost ~4 ms per
        // frame). FUObjectItem::Object / ::Flags offsets are read once from the proven layout data;
        // only for the item shape where GetUObject() is Object and the internal flags are Flags
        // (4.13 .. 5.6, no item packing). Any other version or a missing offset keeps the accessor
        // path. Refreshed at the start of every Sync; used only under the index mutex.
        struct FastSlots
        {
            bool Ok{false};
            uint8_t* const* Chunks{nullptr};
            int32_t NumChunks{0};
            int32_t ItemSize{0};
            int32_t OffObject{-1};
            int32_t OffFlags{-1};
        };
        static FastSlots g_fast{};
        static auto RefreshFastSlots() -> void
        {
            g_fast.Ok = false;
#if !UE_ENABLE_FUOBJECT_ITEM_PACKING
            static const int32_t s_off_object = [] { auto It = FUObjectItem::MemberOffsets.find(STR("Object")); return It == FUObjectItem::MemberOffsets.end() ? -1 : It->second; }();
            static const int32_t s_off_flags = [] { auto It = FUObjectItem::MemberOffsets.find(STR("Flags")); return It == FUObjectItem::MemberOffsets.end() ? -1 : It->second; }();
            if (!Version::IsAtLeast(4, 13) || Version::IsAtLeast(5, 7) || s_off_object < 0 || s_off_flags < 0) { return; }
            const auto& ObjObjects = GUObjectArray->GetObjObjects();
            g_fast.ItemSize = FUObjectItem::UEP_TotalSize();
            g_fast.NumChunks = ObjObjects.GetNumChunks();
            g_fast.Chunks = std::bit_cast<uint8_t* const*>(ObjObjects.GetObjects());
            g_fast.OffObject = s_off_object;
            g_fast.OffFlags = s_off_flags;
            g_fast.Ok = g_fast.Chunks != nullptr && g_fast.ItemSize > 0 && g_fast.NumChunks >= 0 &&
                        s_off_object + static_cast<int32_t>(sizeof(void*)) <= g_fast.ItemSize &&
                        s_off_flags + static_cast<int32_t>(sizeof(int32_t)) <= g_fast.ItemSize;
#endif
        }
        // Live object in the slot, or nullptr when empty or unreachable (never touch freed memory).
        static auto LiveAt(int32_t Index) -> UObject*
        {
            if (g_fast.Ok)
            {
                const int32_t ChunkIndex = Index / TUObjectArray::NumElementsPerChunk;
                if (Index < 0 || ChunkIndex >= g_fast.NumChunks) { return nullptr; }
                const uint8_t* Chunk = g_fast.Chunks[ChunkIndex];
                if (Chunk == nullptr) { return nullptr; }
                const uint8_t* Item = Chunk + static_cast<size_t>(Index % TUObjectArray::NumElementsPerChunk) * static_cast<size_t>(g_fast.ItemSize);
                UObject* Object = *reinterpret_cast<UObject* const*>(Item + g_fast.OffObject);
                if (Object == nullptr) { return nullptr; }
                const int32_t Flags = *reinterpret_cast<const int32_t*>(Item + g_fast.OffFlags);
                if (Flags & static_cast<int32_t>(EInternalObjectFlags::Unreachable)) { return nullptr; }
                return Object;
            }
            FUObjectItem* Item = ItemAt(Index);
            if (Item == nullptr) { return nullptr; }
            UObject* Object = Item->GetUObject();
            if (Object == nullptr || Item->IsUnreachable()) { return nullptr; }
            return Object;
        }
        static auto NameKey(UClass* Class) -> uint64_t
        {
            const FName N = Class->GetNamePrivate();
            return (static_cast<uint64_t>(N.GetComparisonIndex().ToUnstableInt()) << 32) | static_cast<uint32_t>(N.GetNumber());
        }
        static auto HashPos(UClass* Class, size_t Mask) -> size_t
        {
            uint64_t h = reinterpret_cast<uintptr_t>(Class);
            h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 29;
            return static_cast<size_t>(h) & Mask;
        }
        static auto Rehash(State& S, size_t NewSize) -> void
        {
            S.HashKeys.assign(NewSize, nullptr);
            S.HashVals.assign(NewSize, -1);
            for (size_t i = 0; i < S.Classes.size(); ++i)
            {
                size_t p = HashPos(S.Classes[i], NewSize - 1);
                while (S.HashKeys[p] != nullptr) { p = (p + 1) & (NewSize - 1); }
                S.HashKeys[p] = S.Classes[i];
                S.HashVals[p] = static_cast<int32_t>(i);
            }
        }
        static auto ClassSlot(State& S, UClass* Class) -> Vec<int32_t>&
        {
            if (S.HashKeys.empty()) { Rehash(S, 16384); }
            const size_t Mask = S.HashKeys.size() - 1;
            size_t p = HashPos(Class, Mask);
            while (S.HashKeys[p] != nullptr)
            {
                if (S.HashKeys[p] == Class)
                {
                    const size_t c = static_cast<size_t>(S.HashVals[p]);
                    // the address may now belong to a different class: dead, moved, or renamed -> match lists stale
                    const int32_t SelfIndex = Class->GetInternalIndex_Private();
                    const uint64_t Key = NameKey(Class);
                    if (S.ClassDead[c] || S.ClassSelfIndex[c] != SelfIndex || S.ClassNameKey[c] != Key)
                    {
                        S.Names.clear();
                    }
                    S.ClassDead[c] = 0; // seen alive again (an object of it is live right now)
                    S.ClassSelfIndex[c] = SelfIndex;
                    S.ClassNameKey[c] = Key;
                    return S.ClassIndices[c];
                }
                p = (p + 1) & Mask;
            }
            S.Classes.push_back(Class);
            S.ClassSelfIndex.push_back(Class->GetInternalIndex_Private()); // alive now: its object is live
            S.ClassNameKey.push_back(NameKey(Class));
            S.ClassDead.push_back(0);
            S.ClassIndices.emplace_back();
            S.HashKeys[p] = Class;
            S.HashVals[p] = static_cast<int32_t>(S.Classes.size() - 1);
            if (S.Classes.size() * 2 > S.HashKeys.size()) { Rehash(S, S.HashKeys.size() * 2); }
            return S.ClassIndices.back();
        }
        static auto Grow(State& S, int32_t Num) -> void
        {
            if (static_cast<int32_t>(S.ObjAt.size()) >= Num) { return; }
            size_t NewSize = S.ObjAt.empty() ? 65536 : S.ObjAt.size();
            while (NewSize < static_cast<size_t>(Num)) { NewSize *= 2; }
            S.ObjAt.resize(NewSize, nullptr);
            S.ClassAt.resize(NewSize, nullptr);
            S.SeenStamp.resize(NewSize, 0);
            S.ListedUnder.resize(NewSize, nullptr);
            S.InWatch.resize(NewSize, 0);
        }
        // Record the slot's live object, or watch it when empty/unreachable.
        static auto Observe(State& S, int32_t Index) -> void
        {
            UObject* Object = LiveAt(Index);
            if (Object == nullptr)
            {
                S.ObjAt[Index] = nullptr;
                S.ClassAt[Index] = nullptr;
                if (!S.InWatch[Index]) { S.InWatch[Index] = 1; S.FreeWatch.push_back(Index); }
                return;
            }
            UClass* Class = Object->GetClassPrivate();
            if (S.ObjAt[Index] == Object && S.ClassAt[Index] == Class) { return; }
            if (S.Built && S.NotifyWanted && S.ObjAt[Index] != Object && S.NewIdx.size() < (1u << 20))
            {
                S.NewIdx.push_back(Index); // a newly appearing object (append or reused slot)
                S.NewObj.push_back(Object);
            }
            S.ObjAt[Index] = Object;
            S.ClassAt[Index] = Class;
            if (Class && S.ListedUnder[Index] != Class)
            {
                ClassSlot(S, Class).push_back(Index);
                S.ListedUnder[Index] = Class;
            }
            else if (Class)
            {
                ClassSlot(S, Class); // refresh liveness bookkeeping (dead flag, self index)
            }
        }
        // BudgetExpired == nullptr: exact (every watched slot re-checked). Otherwise the watched-slot
        // re-check stops once it returns true (checked every 256 slots); the rest stays watched, first in
        // line for the next sync. Appended indices are always observed.
        static auto Sync(State& S, bool (*BudgetExpired)() = nullptr) -> bool
        {
            const uint64_t r9_t0 = __builtin_readcyclecounter();
            struct R9SyncTimer
            {
                uint64_t t0;
                ~R9SyncTimer()
                {
                    __atomic_fetch_add(&g_r9_sync_calls_fwd, uint64_t{1}, __ATOMIC_RELAXED);
                    __atomic_fetch_add(&g_r9_sync_ticks_fwd, __builtin_readcyclecounter() - t0, __ATOMIC_RELAXED);
                }
            } r9_sync_timer{r9_t0};
            if (!GUObjectArray) { return false; }
            const int32_t Num = GUObjectArray->GetObjObjects().GetNumElements();
            if (Num < 0 || Num > 64 * 1024 * 1024) { S.Disabled = true; return false; }
            RefreshFastSlots(); // after Num: the engine grows NumChunks before NumElements, so every index < Num is covered
            Grow(S, Num);
            if (!S.Built)
            {
                S.FreeWatch.clear();
                S.InWatch.assign(S.InWatch.size(), 0);
                for (int32_t i = 0; i < Num; ++i) { Observe(S, i); }
                S.SyncedNum = Num;
                S.Built = true;
                S.Dirty.clear();
                // safety net: the class self-index must locate the class object; else never use the index
                size_t Checked = 0, Bad = 0;
                for (size_t c = 0; c < S.Classes.size() && Checked < 32; ++c)
                {
                    ++Checked;
                    FUObjectItem* Item = ItemAt(S.ClassSelfIndex[c]);
                    if (Item == nullptr || Item->GetUObject() != static_cast<UObject*>(static_cast<void*>(S.Classes[c]))) { ++Bad; }
                }
                if (Checked == 0 || Bad > 0)
                {
                    S.Disabled = true;
                    try { Output::send<LogLevel::Error>(STR("[R9-IDX] object index disabled: class self-index check failed ({} of {}); using the full walk\n"), Bad, Checked); } catch (...) {}
                    return false;
                }
                __atomic_store_n(&g_r9_watch_fwd, static_cast<uint64_t>(S.FreeWatch.size()), __ATOMIC_RELAXED);
                return true;
            }
            // re-check watched slots first (they are re-added by Observe if still empty). Newly freed
            // indices lead (the engine reuses the most recently freed index first), then the watch list
            // in order (unvisited ones from a budgeted pass first). A freed slot's entry is cleared at
            // once: a new object reusing the same address and class is then still seen as new, however
            // long the slot waits for its re-check.
            Vec<int32_t> Watch{};
            Watch.reserve(S.Dirty.size() + S.FreeWatch.size());
            for (int32_t i : S.Dirty)
            {
                if (i >= 0 && i < Num)
                {
                    S.ObjAt[i] = nullptr;
                    S.ClassAt[i] = nullptr;
                    if (!S.InWatch[i]) { S.InWatch[i] = 1; Watch.push_back(i); }
                }
            }
            S.Dirty.clear();
            Watch.insert(Watch.end(), S.FreeWatch.begin(), S.FreeWatch.end());
            S.FreeWatch.clear(); // keeps its capacity: no per-frame regrowth
            size_t k = 0;
            for (; k < Watch.size(); ++k)
            {
                if (BudgetExpired != nullptr && k > 0 && (k & 255) == 0 && BudgetExpired()) { break; }
                const int32_t i = Watch[k];
                if (i < Num) { S.InWatch[i] = 0; Observe(S, i); }
                else { S.FreeWatch.push_back(i); }
            }
            if (k < Watch.size())
            {
                // budget reached: unvisited slots (still flagged InWatch) go first next time
                Vec<int32_t> Rest{};
                Rest.reserve(Watch.size() - k + S.FreeWatch.size());
                Rest.insert(Rest.end(), Watch.begin() + static_cast<std::ptrdiff_t>(k), Watch.end());
                Rest.insert(Rest.end(), S.FreeWatch.begin(), S.FreeWatch.end());
                S.FreeWatch.swap(Rest);
                __atomic_fetch_add(&g_r9_watch_cut_fwd, uint64_t{1}, __ATOMIC_RELAXED);
            }
            for (int32_t i = S.SyncedNum; i < Num; ++i) { Observe(S, i); }
            S.SyncedNum = Num;
            __atomic_store_n(&g_r9_watch_fwd, static_cast<uint64_t>(S.FreeWatch.size()), __ATOMIC_RELAXED);
            return true;
        }
        // FirstOnly: walk semantics (lowest-index EXACT class match, else the LAST subclass match).
        template <typename Accept>
        static auto Query(FName ClassName, bool FirstOnly, Accept&& Out) -> bool
        {
            State& S = *g_state;
            std::lock_guard Lock{S.Mutex};
            if (S.Disabled || S.DeletesSeen == 0) { return false; } // no proven delete listener -> walk
            try
            {
                if (!Sync(S)) { return false; }
                uint32_t Stamp = ++S.QueryStamp;
                if (Stamp == 0 || Stamp == 0xFFFFFFFFu) { S.QueryStamp = 1; Stamp = 1; } // 0 = unset, ~0 = collect sentinel
                Vec<int32_t> Hits{};
                int32_t BestExact = -1, BestSuper = -1;
                // classes that match this name (cached; only new / pending classes are examined)
                const uint32_t Cmp = ClassName.GetComparisonIndex().ToUnstableInt();
                const int32_t Nr = ClassName.GetNumber();
                State::NameEntry* E = nullptr;
                for (auto& N : S.Names)
                {
                    if (N.Comparison == Cmp && N.Number == Nr) { E = &N; break; }
                }
                if (E == nullptr)
                {
                    if (S.Names.size() > 512) { S.Names.clear(); }
                    S.Names.emplace_back();
                    E = &S.Names.back();
                    E->Comparison = Cmp;
                    E->Number = Nr;
                }
                bool NeedNamesClear = false;
                // 0 = live (examined), 1 = gone (dead), 2 = unreachable right now (retry later)
                const auto ClassState = [&](size_t c) -> int {
                    if (S.ClassDead[c]) { return 1; }
                    FUObjectItem* ClassItem = ItemAt(S.ClassSelfIndex[c]);
                    if (ClassItem == nullptr || ClassItem->GetUObject() != static_cast<UObject*>(static_cast<void*>(S.Classes[c])))
                    {
                        S.ClassDead[c] = 1;
                        for (int32_t i : S.ClassIndices[c]) { if (S.ListedUnder[i] == S.Classes[c]) { S.ListedUnder[i] = nullptr; } }
                        S.ClassIndices[c].clear();
                        return 1;
                    }
                    if (ClassItem->IsUnreachable()) { return 2; }
                    if (NameKey(S.Classes[c]) != S.ClassNameKey[c])
                    {
                        // same address and slot, different class: treat the cached one as gone
                        S.ClassDead[c] = 1;
                        for (int32_t i : S.ClassIndices[c]) { if (S.ListedUnder[i] == S.Classes[c]) { S.ListedUnder[i] = nullptr; } }
                        S.ClassIndices[c].clear();
                        NeedNamesClear = true;
                        return 1;
                    }
                    return 0;
                };
                const auto Examine = [&](size_t c) {
                    const int st = ClassState(c);
                    if (st == 1) { return; }
                    if (st == 2) { E->Pending.push_back(static_cast<int32_t>(c)); return; }
                    UClass* Class = S.Classes[c];
                    const bool bExact = Class->GetNamePrivate().Equals(ClassName);
                    bool bMatch = bExact;
                    if (!bMatch)
                    {
                        for (UStruct* Super : TSuperStructRange(Class))
                        {
                            if (Super->GetNamePrivate().Equals(ClassName)) { bMatch = true; break; }
                        }
                    }
                    if (bMatch)
                    {
                        E->Matches.push_back(static_cast<int32_t>(c));
                        E->Exact.push_back(bExact ? 1 : 0);
                    }
                };
                {
                    Vec<int32_t> Retry{};
                    Retry.swap(E->Pending);
                    for (int32_t c : Retry) { Examine(static_cast<size_t>(c)); }
                    for (size_t c = E->Upto; c < S.Classes.size(); ++c) { Examine(c); }
                    E->Upto = S.Classes.size();
                }
                for (size_t m = 0; m < E->Matches.size(); ++m)
                {
                    const size_t c = static_cast<size_t>(E->Matches[m]);
                    const int st = ClassState(c);
                    if (st != 0) { continue; } // gone, or GC in flight (skip this query only)
                    UClass* Class = S.Classes[c];
                    const bool bExact = E->Exact[m] != 0;
                    auto& Indices = S.ClassIndices[c];
                    size_t keep = 0;
                    for (size_t k = 0; k < Indices.size(); ++k)
                    {
                        const int32_t i = Indices[k];
                        if (S.ClassAt[i] != Class || S.ObjAt[i] == nullptr)
                        {
                            if (S.ListedUnder[i] == Class) { S.ListedUnder[i] = nullptr; }
                            continue; // stale: drop
                        }
                        if (S.SeenStamp[i] == Stamp) { continue; } // duplicate: drop
                        S.SeenStamp[i] = Stamp;
                        Indices[keep++] = i;
                        UObject* O = S.ObjAt[i];
                        if (LiveAt(i) != O) { continue; }
                        if (!IsValidObjectForFindXOf(O)) { continue; }
                        if (FirstOnly)
                        {
                            if (bExact) { if (BestExact < 0 || i < BestExact) { BestExact = i; } }
                            else if (i > BestSuper) { BestSuper = i; }
                        }
                        else
                        {
                            Hits.push_back(i);
                        }
                    }
                    Indices.resize(keep);
                }
                if (NeedNamesClear) { S.Names.clear(); } // E is not used after this point
                if (FirstOnly)
                {
                    const int32_t Pick = BestExact >= 0 ? BestExact : BestSuper;
                    if (Pick >= 0) { Out(S.ObjAt[Pick]); }
                }
                else
                {
                    std::sort(Hits.begin(), Hits.end());
                    for (int32_t i : Hits) { Out(S.ObjAt[i]); }
                }
                if (!g_logged)
                {
                    g_logged = true;
                    Output::send(STR("[R9-IDX] object index active: built from {} slots, {} classes (FindFirstOf/FindAllOf no longer walk the whole array) direct-slots={}\n"), S.SyncedNum, S.Classes.size(), g_fast.Ok ? 1 : 0);
                }
                return true;
            }
            catch (...)
            {
                S.Disabled = true;
                try { Output::send<LogLevel::Error>(STR("[R9-IDX] object index disabled after an exception; FindFirstOf/FindAllOf use the full walk\n")); } catch (...) {}
                return false;
            }
        }
    } // namespace r9idx

    // [R9-NOTIFY] game thread, once per pump: indices of objects that appeared since the last call.
    // false = index unusable (no notifications this frame). Objects alive at the first build are
    // never reported (upstream notifies constructions only).
    // BudgetExpired: the per-frame time budget for re-checking freed slots (see r9idx::Sync).
    auto r9_index_collect_new(std::vector<int32_t>& Out, std::vector<void*>& OutObj, bool (*BudgetExpired)()) -> bool
    {
        r9idx::State& S = *r9idx::g_state;
        std::lock_guard Lock{S.Mutex};
        if (S.Disabled || !S.NotifyWanted) { return false; } // armed only by a registration
        try
        {
            if (!r9idx::Sync(S, BudgetExpired)) { return false; }
            if (S.DeletesSeen == 0) { S.NewIdx.clear(); S.NewObj.clear(); return false; } // delete listener not proven live yet
            Out.clear();
            OutObj.clear();
            for (size_t k = 0; k < S.NewIdx.size(); ++k)
            {
                // keep only entries whose slot still holds exactly that object, reachable (no duplicates
                // for a slot that changed hands between two collects; the later object has its own entry)
                const int32_t i = S.NewIdx[k];
                if (r9idx::LiveAt(i) == S.NewObj[k] && S.ObjAt[i] == S.NewObj[k] && S.SeenStamp[i] != 0xFFFFFFFFu)
                {
                    S.SeenStamp[i] = 0xFFFFFFFFu; // de-dup within this collect
                    Out.push_back(i);
                    OutObj.push_back(S.NewObj[k]);
                }
            }
            for (int32_t i : Out) { S.SeenStamp[i] = 0; }
            S.NewIdx.clear();
            S.NewObj.clear();
            return true; // creation order (index order is not construction order)
        }
        catch (...)
        {
            S.Disabled = true;
            return false;
        }
    }

    // [R9-NOTIFY] called when a NotifyOnNewObject registration is added: absorb everything that
    // exists right now (never reported), then report only objects that appear after this point.
    auto r9_index_notify_arm() -> void
    {
        r9idx::State& S = *r9idx::g_state;
        std::lock_guard Lock{S.Mutex};
        if (S.Disabled) { return; }
        try
        {
            if (S.NotifyWanted) { return; } // already collecting for earlier registrations: keep their queue
            (void)r9idx::Sync(S);
            S.NewIdx.clear();
            S.NewObj.clear();
            S.NotifyWanted = true;
        }
        catch (...)
        {
            S.Disabled = true;
        }
    }

    // [R9-NOTIFY] no registrations left: stop collecting.
    auto r9_index_notify_disarm() -> void
    {
        r9idx::State& S = *r9idx::g_state;
        std::lock_guard Lock{S.Mutex};
        S.NotifyWanted = false;
        S.NewIdx.clear();
        S.NewObj.clear();
    }

    // Called by the engine-registered delete listener for every freed object index.
    // [R9-PERF] listener / index maintenance cost (cycle ticks; read and reset by the [PERF] line)
    static uint64_t g_r9_del_calls{0}, g_r9_del_ticks{0};
    auto r9_take_index_stats(uint64_t& del_calls, uint64_t& del_ticks, uint64_t& sync_calls, uint64_t& sync_ticks, uint64_t& watch, uint64_t& cuts) -> void
    {
        cuts = __atomic_exchange_n(&g_r9_watch_cut_fwd, uint64_t{0}, __ATOMIC_RELAXED);
        watch = __atomic_load_n(&g_r9_watch_fwd, __ATOMIC_RELAXED);
        del_calls = __atomic_exchange_n(&g_r9_del_calls, uint64_t{0}, __ATOMIC_RELAXED);
        del_ticks = __atomic_exchange_n(&g_r9_del_ticks, uint64_t{0}, __ATOMIC_RELAXED);
        sync_calls = __atomic_exchange_n(&g_r9_sync_calls_fwd, uint64_t{0}, __ATOMIC_RELAXED);
        sync_ticks = __atomic_exchange_n(&g_r9_sync_ticks_fwd, uint64_t{0}, __ATOMIC_RELAXED);
    }
    auto r9_index_note_deleted(int32_t Index) -> void
    {
        const uint64_t r9_t0 = __builtin_readcyclecounter();
        struct R9DelTimer
        {
            uint64_t t0;
            ~R9DelTimer()
            {
                __atomic_fetch_add(&g_r9_del_calls, uint64_t{1}, __ATOMIC_RELAXED);
                __atomic_fetch_add(&g_r9_del_ticks, __builtin_readcyclecounter() - t0, __ATOMIC_RELAXED);
            }
        } r9_del_timer{r9_t0};
        r9idx::State& S = *r9idx::g_state;
        std::lock_guard Lock{S.Mutex};
        ++S.DeletesSeen;
        if (!S.Built || S.Disabled) { return; }
        try
        {
            if (S.Dirty.size() > 4 * 1024 * 1024) { S.Built = false; S.Dirty.clear(); S.FreeWatch.clear(); return; } // rebuild (InWatch reset by the build)
            S.Dirty.push_back(Index);
        }
        catch (...)
        {
            S.Disabled = true; // never silently stale
        }
    }

    // [R9-PERF] lookup cost counters (read and reset by the per-second [PERF] line in LuaMod.cpp)
    static uint64_t g_r9_walk_calls{0};
    static uint64_t g_r9_walk_ns{0};
    struct R9WalkTimer
    {
        int64_t t0;
        R9WalkTimer() : t0(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()) {}
        ~R9WalkTimer()
        {
            const int64_t t1 = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            __atomic_fetch_add(&g_r9_walk_calls, uint64_t{1}, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_r9_walk_ns, static_cast<uint64_t>(t1 - t0), __ATOMIC_RELAXED);
        }
    };
    auto r9_take_walk_stats(uint64_t& calls, uint64_t& ns) -> void
    {
        calls = __atomic_exchange_n(&g_r9_walk_calls, uint64_t{0}, __ATOMIC_RELAXED);
        ns = __atomic_exchange_n(&g_r9_walk_ns, uint64_t{0}, __ATOMIC_RELAXED);
    }

    auto FindFirstOf(FName ClassName) -> UObject*
    {
        const R9WalkTimer r9_walk_timer{};
        UObject* ObjectFound{nullptr};
        if (r9idx::Query(ClassName, true, [&](UObject* O) { ObjectFound = O; }))
        {
            return ObjectFound;
        }
        // [B1 memo fix] heap, nothrow: on allocation failure the walk runs un-memoized.
        std::unique_ptr<FFindXofClassMemo> Memo{new (std::nothrow) FFindXofClassMemo{}};
#ifdef __PROSPERO__
        FindXOfFfo2Stats FFO2{};
#endif

        if (UnrealInitializer::ShouldUseHashTables())
        {
            std::vector<UClass*> Roots{};
            FindRootClassesByName(ClassName, Roots);
            for (UClass* Root : Roots)
            {
                FindObjectSearcher(Root, nullptr).ForEach([&](UObject* Object) {
#ifdef __PROSPERO__
                    ++FFO2.scanned;
#endif
                    if (!IsValidObjectForFindXOf(Object)) { return LoopAction::Continue; }
                    ObjectFound = Object;
                    return LoopAction::Break;
                });
                if (ObjectFound) { break; }
            }
#ifdef __PROSPERO__
            // Hash arm: exact = matched ROOT classes (FindRootClassesByName
            // Equals hits over UClass objects); no per-object super walk.
            FFO2.exact = static_cast<uint32_t>(Roots.size());
            FFO2.arm = "hash";
            LogFindXOfFfo2(ClassName, FFO2, ObjectFound, -1);
#endif
            return ObjectFound;
        }

        UObjectGlobals::ForEachUObject([&](UObject* Object, [[maybe_unused]]int32_t ChunkIndex, [[maybe_unused]]int32_t ObjectIndex) {
#ifdef __PROSPERO__
            ++FFO2.scanned;
#endif
            UClass* Class = Object->GetClassPrivate();

            // [IT lookup-speed 02] O(1) integer compare first (Equals is
            // ComparisonIndex equality); the expensive filter verdict is
            // memoized per DISTINCT class pointer, so the common case per
            // object is: 1 class read + 1 int compare + 1 memo probe.
            const bool bDirectClassMatch = Class->GetNamePrivate().Equals(ClassName);
#ifdef __PROSPERO__
            if (bDirectClassMatch) { ++FFO2.exact; }
#endif
            FFindXofClassVerdict* MemoSlot = FindXofMemoSlot(Memo.get(), Class);
            const bool bValidForFindXOf = FindXofValid(Object, MemoSlot);
            if (bDirectClassMatch && bValidForFindXOf)
            {
                ObjectFound = Object;
                return LoopAction::Break;

            }

            if (!bValidForFindXOf) { return LoopAction::Continue; }

            if (FindXofChainContainsQuery(Class, MemoSlot, ClassName))
            {
#ifdef __PROSPERO__
                ++FFO2.via_super;
#endif
                ObjectFound = Object;
            }

            return LoopAction::Continue;
        });

#ifdef __PROSPERO__
        LogFindXOfFfo2(ClassName, FFO2, ObjectFound, -1);
#endif
        return ObjectFound;
    }

    auto FindFirstOf(const CharType* ClassName) -> UObject*
    {
#ifdef __PROSPERO__
        return FindFirstOfVerified(ClassName);
#else
        return FindFirstOf(FName(ClassName));
#endif
    }

    auto FindFirstOf(StringViewType ClassName) -> UObject*
    {
#ifdef __PROSPERO__
        return FindFirstOfVerified(ClassName);
#else
        return FindFirstOf(FName(ClassName));
#endif
    }

    auto FindFirstOf(const StringType& ClassName) -> UObject*
    {
#ifdef __PROSPERO__
        return FindFirstOfVerified(ClassName);
#else
        return FindFirstOf(FName(ClassName));
#endif
    }

    auto FindFirstOf(std::string_view ClassName) -> UObject*
    {
#ifdef __PROSPERO__
        return FindFirstOfVerified(ensure_str(ClassName));
#else
        return FindFirstOf(FName(ensure_str(ClassName)));
#endif
    }

    auto FindFirstOf(const std::string& ClassName) -> UObject*
    {
#ifdef __PROSPERO__
        return FindFirstOfVerified(ensure_str(ClassName));
#else
        return FindFirstOf(FName(ensure_str(ClassName)));
#endif
    }

    auto FindAllOf(const StringType& InClassName, std::vector<UObject*>& OutStorage, bool bUseCache) -> void
    {
#ifdef __PROSPERO__
        if (FindXOfQueryHasEmbeddedNul(InClassName))
        {
            LogFindXOfQueryEmbeddedNul(InClassName);
            return;
        }
        const auto ClassName = MakeFindXOfClassName(InClassName);
        if (FindXOfQueryMismatch(ClassName, InClassName))
        {
            LogFindXOfQueryFallback(ClassName, InClassName);
            // Mirror the arm selection's exclusion semantics below: the
            // 2-arg FindAllOf(FName) walk applies IsValidObjectForFindXOf;
            // the cache scan does not.
            return FindAllOfStringWalk(InClassName, OutStorage, !bUseCache || UnrealInitializer::ShouldUseHashTables());
        }
#else
        const auto ClassName = FName(InClassName);
#endif
        if (!bUseCache || UnrealInitializer::ShouldUseHashTables())
        {
            return FindAllOf(ClassName, OutStorage);
        }
        else
        {
#ifdef __PROSPERO__
            FindXOfFfo2Stats FFO2{};
            UObject* FFO2First{nullptr};
            const size_t FFO2FoundBefore = OutStorage.size();
#endif
            FindObjectSearcher<AnyInstance, AnyInstance>().ForEach([&](UObject* Object) {
#ifdef __PROSPERO__
                ++FFO2.scanned;
#endif
                if (!Object) { return LoopAction::Continue; }

                UClass* Class = Object->GetClassPrivate();
                if (!Class) { return LoopAction::Continue; }

                const bool bDirectClassMatch = Class->GetNamePrivate().Equals(ClassName);
#ifdef __PROSPERO__
                if (bDirectClassMatch) { ++FFO2.exact; }
#endif
                if (bDirectClassMatch)
                {
                    OutStorage.emplace_back(Object);
#ifdef __PROSPERO__
                    if (!FFO2First) { FFO2First = Object; }
#endif
                    return LoopAction::Continue;
                }

                for (UStruct* SuperStruct : TSuperStructRange(Class))
                {
                    if (SuperStruct->GetNamePrivate().Equals(ClassName))
                    {
                        OutStorage.emplace_back(Object);
#ifdef __PROSPERO__
                        ++FFO2.via_super;
                        if (!FFO2First) { FFO2First = Object; }
#endif
                        break;
                    }
                }

                return LoopAction::Continue;
            });
#ifdef __PROSPERO__
            // Cache-scan arm of the 3-arg string overload: same FName
            // Equals predicate, no IsValidObjectForFindXOf filter here
            // (mirrors upstream arm semantics; [FFO2] names the arm so
            // its counters are never misread as the 2-arg walk's).
            FFO2.arm = "cache";
            LogFindXOfFfo2(ClassName, FFO2, FFO2First, static_cast<int64_t>(OutStorage.size() - FFO2FoundBefore));
#endif
        }
    }

    auto FindAllOf(std::string_view ClassName, std::vector<UObject*>& OutStorage, bool bUseCache)-> void
    {
        return FindAllOf(ensure_str(ClassName), OutStorage, bUseCache);
    }

    auto FindAllOf(FName ClassName, std::vector<UObject*>& OutStorage) -> void
    {
        const R9WalkTimer r9_walk_timer{};
        const size_t r9_start_size = OutStorage.size();
        if (r9idx::Query(ClassName, false, [&](UObject* O) { OutStorage.push_back(O); }))
        {
            return;
        }
        OutStorage.resize(r9_start_size); // a failed index query leaves nothing behind
        // [B1 memo fix] heap, nothrow: on allocation failure the walk runs un-memoized.
        std::unique_ptr<FFindXofClassMemo> Memo{new (std::nothrow) FFindXofClassMemo{}};
#ifdef __PROSPERO__
        FindXOfFfo2Stats FFO2{};
        UObject* FFO2First{nullptr};
        const size_t FFO2FoundBefore = OutStorage.size();
#endif
        if (UnrealInitializer::ShouldUseHashTables())
        {
            std::vector<UClass*> Roots{};
            FindRootClassesByName(ClassName, Roots);
            for (UClass* Root : Roots)
            {
                FindObjectSearcher(Root, nullptr).ForEach([&](UObject* Object) {
#ifdef __PROSPERO__
                    ++FFO2.scanned;
#endif
                    if (IsValidObjectForFindXOf(Object))
                    {
                        OutStorage.emplace_back(Object);
#ifdef __PROSPERO__
                        if (!FFO2First) { FFO2First = Object; }
#endif
                    }
                    return LoopAction::Continue;
                });
            }
#ifdef __PROSPERO__
            // Hash arm: exact = matched ROOT classes; no per-object super
            // walk (root-class instances cover inheritance).
            FFO2.exact = static_cast<uint32_t>(Roots.size());
            FFO2.arm = "hash";
            LogFindXOfFfo2(ClassName, FFO2, FFO2First, static_cast<int64_t>(OutStorage.size() - FFO2FoundBefore));
#endif
            return;
        }

        UObjectGlobals::ForEachUObject([&](UObject* Object, [[maybe_unused]]int32_t ChunkIndex, [[maybe_unused]]int32_t ObjectIndex) {
#ifdef __PROSPERO__
            ++FFO2.scanned;
#endif
            if (!Object) { return LoopAction::Continue; }

            UClass* Class = Object->GetClassPrivate();
            if (!Class) { return LoopAction::Continue; }

            const bool bDirectClassMatch = Class->GetNamePrivate().Equals(ClassName);
#ifdef __PROSPERO__
            if (bDirectClassMatch) { ++FFO2.exact; }
#endif
            // [IT lookup-speed 02] memoized per-class verdicts; same control
            // flow as before, ~30-40 scattered loads per object -> 2 probes.
            FFindXofClassVerdict* MemoSlot = FindXofMemoSlot(Memo.get(), Class);
            const bool bValidForFindXOf = FindXofValid(Object, MemoSlot);
            if (bDirectClassMatch && bValidForFindXOf)
            {
                OutStorage.emplace_back(Object);
#ifdef __PROSPERO__
                if (!FFO2First) { FFO2First = Object; }
#endif
                return LoopAction::Continue;
            }

            if (!bValidForFindXOf) { return LoopAction::Continue; }

            if (FindXofChainContainsQuery(Class, MemoSlot, ClassName))
            {
                OutStorage.emplace_back(Object);
#ifdef __PROSPERO__
                ++FFO2.via_super;
                if (!FFO2First) { FFO2First = Object; }
#endif
            }

            return LoopAction::Continue;
        });
#ifdef __PROSPERO__
        LogFindXOfFfo2(ClassName, FFO2, FFO2First, static_cast<int64_t>(OutStorage.size() - FFO2FoundBefore));
#endif
    }

    auto FindAllOf(const CharType* ClassName, std::vector<UObject*>& OutStorage) -> void
    {
#ifdef __PROSPERO__
        FindAllOfVerified(ClassName, OutStorage);
#else
        FindAllOf(FName(ClassName), OutStorage);
#endif
    }

    auto FindAllOf(StringViewType ClassName, std::vector<UObject*>& OutStorage) -> void
    {
#ifdef __PROSPERO__
        FindAllOfVerified(ClassName, OutStorage);
#else
        FindAllOf(FName(ClassName), OutStorage);
#endif
    }

    auto FindAllOf(const StringType& ClassName, std::vector<UObject*>& OutStorage) -> void
    {
#ifdef __PROSPERO__
        FindAllOfVerified(ClassName, OutStorage);
#else
        FindAllOf(FName(ClassName), OutStorage);
#endif
    }

    auto FindAllOf(std::string_view ClassName, std::vector<UObject*>& OutStorage) -> void
    {
#ifdef __PROSPERO__
        FindAllOfVerified(ensure_str(ClassName), OutStorage);
#else
        FindAllOf(FName(ensure_str(ClassName)), OutStorage);
#endif
    }

    auto FindAllOf(const std::string& ClassName, std::vector<UObject*>& OutStorage) -> void
    {
#ifdef __PROSPERO__
        FindAllOfVerified(ensure_str(ClassName), OutStorage);
#else
        FindAllOf(FName(ensure_str(ClassName)), OutStorage);
#endif
    }

    auto FindObjects(size_t NumObjectsToFind, const FName ClassName, const FName ObjectShortName, std::vector<UObject*>& OutStorage, int32 RequiredFlags, int32 BannedFlags, bool bExactClass) -> void
    {
        bool bCareAboutClass = ClassName != FName(0u, 0u);
        bool bCareAboutName = ObjectShortName != FName(0u, 0u);

        if (!bCareAboutClass && !bCareAboutName)
        {
            throw std::runtime_error{"[UObjectGlobals::find_objects] Must supply class_name, object_short_name, or both"};
        }

        size_t NumObjectsFound{};

        auto ConsiderObject = [&](UObject* Object) -> LoopAction {
            bool bNameMatches{};
            // Intentionally not using the 'Equals' function here because names can have an instance number that we care about.
            if (bCareAboutName && Object->GetNamePrivate() == ObjectShortName)
            {
                bNameMatches = true;
            }

            bool bClassMatches{};
            if (bCareAboutClass)
            {
                auto* ObjClass = Object->GetClassPrivate();
                if (bExactClass)
                {
                    if (ObjClass->GetNamePrivate().Equals(ClassName))
                    {
                        bClassMatches = true;
                    }
                }
                else
                {
                    while (ObjClass)
                    {
                        if (ObjClass->GetNamePrivate().Equals(ClassName))
                        {
                            bClassMatches = true;
                            break;
                        }

                        ObjClass = ObjClass->GetSuperClass();
                    }
                }
            }

            if ((bCareAboutClass && bClassMatches && bCareAboutName && bNameMatches) ||
                (!bCareAboutName && bCareAboutClass && bClassMatches) ||
                (!bCareAboutClass && bCareAboutName && bNameMatches))
            {
                bool bRequiredFlagsPasses = RequiredFlags == EObjectFlags::RF_NoFlags || Object->HasAllFlags(static_cast<EObjectFlags>(RequiredFlags));
                bool bBannedFlagsPasses = BannedFlags == EObjectFlags::RF_NoFlags || !Object->HasAnyFlags(static_cast<EObjectFlags>(BannedFlags));

                if (bRequiredFlagsPasses && bBannedFlagsPasses)
                {
                    OutStorage.emplace_back(Object);
                    ++NumObjectsFound;
                }
            }

            if (NumObjectsToFind != 0 && NumObjectsFound >= NumObjectsToFind)
            {
                return LoopAction::Break;
            }
            else
            {
                return LoopAction::Continue;
            }
        };

        // Every object the class filter can match lives under a class of that name, so walk those instead of the whole array.
        if (bCareAboutClass && UnrealInitializer::ShouldUseHashTables())
        {
            std::vector<UClass*> Roots{};
            FindRootClassesByName(ClassName, Roots);
            for (UClass* Root : Roots)
            {
                bool bStop{};
                FindObjectSearcher(Root, nullptr).ForEach([&](UObject* Object) {
                    const auto Action = ConsiderObject(Object);
                    bStop = Action == LoopAction::Break;
                    return Action;
                });
                if (bStop) { break; }
            }
            return;
        }

        ForEachUObject([&](UObject* Object, int32, int32) {
            return ConsiderObject(Object);
        });
    }

    auto FindObjects(size_t NumObjectsToFind, const CharType* ClassName, const CharType* ObjectShortName, std::vector<UObject*>& OutStorage, int32 RequiredFlags, int32 BannedFlags, bool bExactClass) -> void
    {
        FindObjects(NumObjectsToFind, FName(ClassName), FName(ObjectShortName), OutStorage, RequiredFlags, BannedFlags, bExactClass);
    }

    auto FindObject(const FName ClassName, const FName ObjectShortName, int32 RequiredFlags, int32 BannedFlags) -> UObject*
    {
        std::vector<UObject*> FoundObject{};
        FindObjects(1, ClassName, ObjectShortName, FoundObject, RequiredFlags, BannedFlags);

        if (FoundObject.empty())
        {
            return nullptr;
        }
        else
        {
            return FoundObject[0];
        }
    };

    auto FindObjects(const FName ClassName, const FName ObjectShortName, std::vector<UObject*>& OutStorage, int32 RequiredFlags, int32 BannedFlags, bool bExactClass) -> void
    {
        FindObjects(0, ClassName, ObjectShortName, OutStorage, RequiredFlags, BannedFlags, bExactClass);
    }

    auto FindObjects(const CharType* ClassName, const CharType* ObjectShortName, std::vector<UObject*>& OutStorage, int32 RequiredFlags, int32 BannedFlags, bool bExactClass) -> void
    {
        FindObjects(0, ClassName, ObjectShortName, OutStorage, RequiredFlags, BannedFlags, bExactClass);
    }

    auto FindObject(const CharType* ClassName, const CharType* ObjectShortName, int32 RequiredFlags, int32 BannedFlags) -> UObject*
    {
        return FindObject(FName(ClassName), FName(ObjectShortName), RequiredFlags, BannedFlags);
    }

    using ForEachUObjectCallback = std::function<LoopAction(UObject*, int32, int32)>;

    static auto ForEachUObject_IndirectArray(const ForEachUObjectCallback& Callable) -> void
    {
        // [FIELDS] explicit version gate (round 10): TUObjectArray::Chunks is the 4.8-4.10
        // indirect-array layout (FFixedUObjectArray). It does not exist on 4.11+ (5.x uses the
        // chunked FUObjectItem array, ForEachUObject_Chunked), so this walk must never read it
        // there. The callers already dispatch on Version::IsAtMost(4, 10); this makes the gate
        // local, so field_gate.py proves the read is dead on the target engine version.
        if (!Version::IsAtMost(4, 10))
        {
            Output::send<LogLevel::Error>(STR("[FIELDS] indirect-array walk refused: TUObjectArray::Chunks exists only on UE 4.8-4.10\n"));
            return;
        }
        GUOBJECTARRAY_PROFILE_ITER_BEGIN()
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        const auto& Chunks = ObjObjects.GetChunks();
        const auto NumChunks = ObjObjects.GetNumChunks();
        const auto NumElements = ObjObjects.GetNumElements();

        for (int32_t ItemIndex = 0; ItemIndex < TUObjectArray::MaxTotalElements_Indirect; ++ItemIndex)
        {
            if (ItemIndex > NumElements) { break; }
            const int32 ChunkIndex = ItemIndex / TUObjectArray::NumElementsPerChunk_Indirect;
            if (ChunkIndex >= NumChunks) { break; }
            const int32 WithinChunkIndex = ItemIndex % TUObjectArray::NumElementsPerChunk_Indirect;
            const auto ChunkPtr = Chunks[ChunkIndex];
            const auto Object = *(ChunkPtr + WithinChunkIndex);
            if (!Object) { continue; }
            GUOBJECTARRAY_PROFILE_ITER_COUNT()
            Action = Callable(static_cast<UObject*>(Object), ChunkIndex, ItemIndex);
            if (Action == LoopAction::Break) { break; }
        }
        GUOBJECTARRAY_PROFILE_ITER_END()
    }

    // 4.7 and earlier: ObjObjects is a plain TArray<UObjectBase*>. The 'Objects' offset resolves
    // to the allocator's data pointer, giving a contiguous array of UObjectBase* slots.
    static auto ForEachUObject_TArray(const ForEachUObjectCallback& Callable) -> void
    {
        GUOBJECTARRAY_PROFILE_ITER_BEGIN()
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        const auto Slots = std::bit_cast<UObjectBase**>(ObjObjects.GetObjects());
        const auto NumElements = ObjObjects.GetNumElements();

        for (int32_t ItemIndex = 0; ItemIndex < NumElements; ++ItemIndex)
        {
            const auto Object = Slots[ItemIndex];
            if (!Object) { continue; }
            GUOBJECTARRAY_PROFILE_ITER_COUNT()
            Action = Callable(static_cast<UObject*>(Object), 0, ItemIndex);
            if (Action == LoopAction::Break) { break; }
        }
        GUOBJECTARRAY_PROFILE_ITER_END()
    }

    static auto ForEachUObject_TArrayInRange(int32_t Start, int32_t End, const ForEachUObjectCallback& Callable) -> void
    {
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        const auto Slots = std::bit_cast<UObjectBase**>(ObjObjects.GetObjects());
        const auto NumElements = ObjObjects.GetNumElements();
        const int32_t EndItemIndex = End < NumElements ? End : NumElements;

        for (int32_t ItemIndex = Start; ItemIndex < EndItemIndex; ++ItemIndex)
        {
            const auto Object = Slots[ItemIndex];
            if (!Object) { continue; }
            Action = Callable(static_cast<UObject*>(Object), 0, ItemIndex);
            if (Action == LoopAction::Break) { break; }
        }
    }

    static auto ForEachUObject_IndirectArrayInRange(int32_t Start, int32_t End, const ForEachUObjectCallback& Callable) -> void
    {
        // [FIELDS] explicit version gate (round 10): TUObjectArray::Chunks is the 4.8-4.10
        // indirect-array layout (FFixedUObjectArray). It does not exist on 4.11+ (5.x uses the
        // chunked FUObjectItem array, ForEachUObject_Chunked), so this walk must never read it
        // there. The callers already dispatch on Version::IsAtMost(4, 10); this makes the gate
        // local, so field_gate.py proves the read is dead on the target engine version.
        if (!Version::IsAtMost(4, 10))
        {
            Output::send<LogLevel::Error>(STR("[FIELDS] indirect-array walk refused: TUObjectArray::Chunks exists only on UE 4.8-4.10\n"));
            return;
        }
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        const auto& Chunks = ObjObjects.GetChunks();
        const auto NumChunks = ObjObjects.GetNumChunks();
        const auto NumElements = ObjObjects.GetNumElements();
        const int32_t EndItemIndex = End < NumElements ? End : NumElements;

        for (int32_t ItemIndex = Start; ItemIndex < EndItemIndex; ++ItemIndex)
        {
            const int32 ChunkIndex = ItemIndex / TUObjectArray::NumElementsPerChunk_Indirect;
            if (ChunkIndex >= NumChunks) { break; }
            const int32 WithinChunkIndex = ItemIndex % TUObjectArray::NumElementsPerChunk_Indirect;
            const auto ChunkPtr = Chunks[ChunkIndex];
            const auto Object = *(ChunkPtr + WithinChunkIndex);
            if (!Object) { continue; }
            Action = Callable(static_cast<UObject*>(Object), ChunkIndex, ItemIndex);
            if (Action == LoopAction::Break) { break; }
        }
    }

    static auto ForEachUObject_NonChunked(const ForEachUObjectCallback& Callable) -> void
    {
        GUOBJECTARRAY_PROFILE_ITER_BEGIN()
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        static const auto ItemSize = FUObjectItem::UEP_TotalSize();

        for (int32_t ItemIndex = 0; ItemIndex < ObjObjects.GetNumElements(); ++ItemIndex)
        {
            const auto& ChunkPtr = ObjObjects.GetObjects();
            const auto ObjectItem = std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(ChunkPtr)[ItemIndex * ItemSize]);
            const auto Object = ObjectItem->GetUObject();
            if (ObjectItem->IsUnreachable() || !Object) { continue; }
            GUOBJECTARRAY_PROFILE_ITER_COUNT()
            Action = Callable(Object, 0, ItemIndex);
            if (Action == LoopAction::Break) { break; }
        }
        GUOBJECTARRAY_PROFILE_ITER_END()
    }

    static auto ForEachUObject_Chunked(const ForEachUObjectCallback& Callable) -> void
    {
        GUOBJECTARRAY_PROFILE_ITER_BEGIN()
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        const auto NumChunks = ObjObjects.GetNumChunks();
        const auto NumElements = ObjObjects.GetNumElements();
        static const auto ItemSize = FUObjectItem::UEP_TotalSize();

        // [IT lookup-speed 01, Claude round-13 item 2] The old loop ran the
        // FULL 64*1024 slots of every chunk, including the unwritten tail of
        // the last chunk past GetNumElements() — with ~35K live elements in
        // chunk 0 that was ~30K dead flag-reads per walk (2 loads each,
        // [FFO2] scanned=35479 vs the 65536-slot scan). Slots at or beyond
        // NumElements are unallocated and can never hold a live UObject, so
        // clamping the final chunk to the remainder is semantics-identical.
        int32_t RemainingElements = NumElements;
        for (int32_t ChunkIndex = 0; ChunkIndex < NumChunks && RemainingElements > 0; ++ChunkIndex)
        {
            const int32_t MaxInChunk = RemainingElements < TUObjectArray::NumElementsPerChunk ? RemainingElements : TUObjectArray::NumElementsPerChunk;
            RemainingElements -= MaxInChunk;
            // [R9-PERF] the walk is memory-latency bound (one cache miss per UObject, scattered
            // across the heap): prefetch the UObject a few items ahead so the misses overlap.
            // A prefetch never faults (null/stale pointers are harmless), so semantics are unchanged.
            const auto& ChunksPtr = ObjObjects.GetObjects();
            uint8_t* const ChunkBase = std::bit_cast<uint8_t*>(ChunksPtr[ChunkIndex]);
            constexpr int32_t PrefetchDistance = 16;
            for (int32_t ItemIndex = 0; ItemIndex < MaxInChunk; ++ItemIndex)
            {
                if (ItemIndex + PrefetchDistance < MaxInChunk)
                {
                    const auto Ahead = std::bit_cast<FUObjectItem*>(&ChunkBase[(ItemIndex + PrefetchDistance) * ItemSize]);
                    __builtin_prefetch(static_cast<const void*>(Ahead->GetUObject()));
                }
                const auto ObjectItem = std::bit_cast<FUObjectItem*>(&ChunkBase[ItemIndex * ItemSize]);
                const auto Object = ObjectItem->GetUObject();
                if (ObjectItem->IsUnreachable() || !Object) { continue; }
                GUOBJECTARRAY_PROFILE_ITER_COUNT()
                Action = Callable(Object, ChunkIndex, ItemIndex);
                if (Action == LoopAction::Break) { break; }
            }
            if (Action == LoopAction::Break) { break; }
        }
        GUOBJECTARRAY_PROFILE_ITER_END()
    }

    static auto ForEachUObject_NonChunkedInRange(int32_t Start, int32_t End, const ForEachUObjectCallback& Callable) -> void
    {
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        const auto NumElements = ObjObjects.GetNumElements();
        static const auto ItemSize = FUObjectItem::UEP_TotalSize();

        const int32_t StartItemIndex = Start;
        const int32_t EndItemIndex = End < NumElements ? End : NumElements;

        for (int32_t ItemIndex = StartItemIndex; ItemIndex < EndItemIndex; ++ItemIndex)
        {
            const auto& ChunkPtr = ObjObjects.GetObjects();
            const auto ObjectItem = std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(ChunkPtr)[ItemIndex * ItemSize]);
            const auto Object = ObjectItem->GetUObject();
            if (ObjectItem->IsUnreachable() || !Object) { continue; }
            Action = Callable(Object, 0, ItemIndex);
            if (Action == LoopAction::Break) { break; }
        }
    }

    static auto ForEachUObject_ChunkedInRange(int32_t Start, int32_t End, const ForEachUObjectCallback& Callable) -> void
    {
        if (!GUObjectArray)
        {
            return;
        }

        LoopAction Action{};

        const auto& ObjObjects = GUObjectArray->GetObjObjects();
        const auto NumElements = ObjObjects.GetNumElements();
        const auto NumChunks = ObjObjects.GetNumChunks();
        static const auto ItemSize = FUObjectItem::UEP_TotalSize();

        const int32_t EndClamped = End < NumElements ? End : NumElements;
        const int32_t StartChunk = Start / TUObjectArray::NumElementsPerChunk;
        const int32_t StartItemIndex = Start % TUObjectArray::NumElementsPerChunk;

        int32_t CurrentTotalItem = Start;
        for (int32_t ChunkIndex = StartChunk; ChunkIndex < NumChunks; ++ChunkIndex)
        {
            bool ShouldBreak{};
            for (int32_t ItemIndex = StartItemIndex; ItemIndex < TUObjectArray::NumElementsPerChunk; ++ItemIndex)
            {
                const auto& ChunksPtr = ObjObjects.GetObjects();
                const auto ObjectItem = std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(ChunksPtr[ChunkIndex])[ItemIndex * ItemSize]);
                const auto Object = ObjectItem->GetUObject();
                if (ObjectItem->IsUnreachable() || !Object) { continue; }
                Action = Callable(Object, ChunkIndex, ItemIndex);
                if (Action == LoopAction::Break || CurrentTotalItem >= EndClamped)
                {
                    ShouldBreak = true;
                    break;
                }
            }
            ++CurrentTotalItem;
            if (ShouldBreak) { break; }
        }
    }

    auto ForEachUObject(const ForEachUObjectCallback& Callable) -> void
    {
        // TODO: Expose whether FUObjectArray is chunked in ini.
        //       This is just in case there's a game with custom changes where they pulled the chunked array into a non-chunked UE version.
        if (Version::IsAtMost(4, 7))
        {
            ForEachUObject_TArray(Callable);
        }
        else if (Version::IsAtMost(4, 10))
        {
            ForEachUObject_IndirectArray(Callable);
        }
        else if (Version::IsAtMost(4, 19))
        {
            ForEachUObject_NonChunked(Callable);
        }
        else
        {
            ForEachUObject_Chunked(Callable);
        }
    }

    auto ForEachUObjectInChunk(int32_t ChunkIndex, const std::function<LoopAction(UObject*, int32)>& Callable) -> void
    {
        if (Version::IsAtMost(4, 7))
        {
            // Chunkless era: the whole TArray acts as chunk 0
            ForEachUObject_TArray([&](UObject* Object, int32_t, int32_t ObjectIndex) {
                return Callable(Object, ObjectIndex);
            });
        }
        else if (Version::IsAtMost(4, 10))
        {
            // Indirect array: iterate the requested chunk of UObjectBase* slots
            ForEachUObject_IndirectArrayInRange(ChunkIndex * TUObjectArray::NumElementsPerChunk_Indirect,
                                                (ChunkIndex + 1) * TUObjectArray::NumElementsPerChunk_Indirect,
                                                [&](UObject* Object, int32_t, int32_t ObjectIndex) {
                                                    return Callable(Object, ObjectIndex);
                                                });
        }
        else if (Version::IsAtMost(4, 19))
        {
            ForEachUObject_NonChunked([&](UObject* Object, int32_t, int32_t ObjectIndex) {
                return Callable(Object, ObjectIndex);
            });
        }
        else
        {
            if (!GUObjectArray || ChunkIndex >= GUObjectArray->GetObjObjects().GetNumChunks())
            {
                return;
            }

            LoopAction Action{};

            const auto& ObjObjects = GUObjectArray->GetObjObjects();
            static const auto ItemSize = FUObjectItem::UEP_TotalSize();

            for (int32_t ItemIndex = 0; ItemIndex < TUObjectArray::NumElementsPerChunk; ++ItemIndex)
            {
                const auto& ChunksPtr = ObjObjects.GetObjects();
                const auto ObjectItem = std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(ChunksPtr[ChunkIndex])[ItemIndex * ItemSize]);
                const auto Object = ObjectItem->GetUObject();
                if (ObjectItem->IsUnreachable() || !Object) { continue; }
                Action = Callable(Object, ItemIndex);
                if (Action == LoopAction::Break) { break; }
            }
        }
    }

    auto ForEachUObjectInRange(int32_t Start, int32_t End, const std::function<LoopAction(UObject*, int32, int32)>& Callable) -> void
    {
        if (Version::IsAtMost(4, 7))
        {
            ForEachUObject_TArrayInRange(Start, End, Callable);
        }
        else if (Version::IsAtMost(4, 10))
        {
            ForEachUObject_IndirectArrayInRange(Start, End, Callable);
        }
        else if (Version::IsAtMost(4, 19))
        {
            ForEachUObject_NonChunkedInRange(Start, End, Callable);
        }
        else
        {
            ForEachUObject_ChunkedInRange(Start, End, Callable);
        }
    }

    struct GlobalHooksInternal
    {
        struct CallableData
        {
            struct InternalData
            {
                UnrealScriptFunctionCallable CallablePre{};
                UnrealScriptFunctionCallable CallablePost{};
                void* CustomData{};
                int32_t PreId{};
                int32_t PostId{};

                InternalData() = default;
                InternalData(UnrealScriptFunctionCallable PreCallable, UnrealScriptFunctionCallable PostCallable, void* CustomData, int32_t PreId, int32_t PostId) :
                      CallablePre(PreCallable),
                      CallablePost(PostCallable),
                      CustomData(CustomData),
                      PreId(PreId),
                      PostId(PostId) {}
            };
            std::vector<InternalData> Callables{};
        };
        static inline std::unordered_map<StringType, CallableData> GlobalScriptHooks{};
        static inline bool bIsHookEnabled{};
        static inline int32_t LastGenericHookId{};
        static inline std::unordered_map<int32_t, int32_t> GenericHookIdToNativeHookId{};
    };

    static auto GlobalScriptHookPre([[maybe_unused]]Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]]Unreal::UObject* Context, Unreal::FFrame& Stack, [[maybe_unused]]void* RESULT_DECL) -> void
    {
        if (GlobalHooksInternal::GlobalScriptHooks.empty()) { return; }
        if (auto it = GlobalHooksInternal::GlobalScriptHooks.find(Stack.Node()->GetFullName()); it != GlobalHooksInternal::GlobalScriptHooks.end())
        {
            UnrealScriptFunctionCallableContext CallableContext{Context, Stack, RESULT_DECL};
            for (const auto& Callable : it->second.Callables)
            {
                Callable.CallablePre(CallableContext, Callable.CustomData);
            }
        }
    }

    static auto GlobalScriptHookPost([[maybe_unused]]Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]]Unreal::UObject* Context, Unreal::FFrame& Stack, [[maybe_unused]]void* RESULT_DECL) -> void
    {
        if (GlobalHooksInternal::GlobalScriptHooks.empty()) { return; }
        if (auto it = GlobalHooksInternal::GlobalScriptHooks.find(Stack.Node()->GetFullName()); it != GlobalHooksInternal::GlobalScriptHooks.end())
        {
            UnrealScriptFunctionCallableContext CallableContext{Context, Stack, RESULT_DECL};
            for (const auto& Callable : it->second.Callables)
            {
                Callable.CallablePost(CallableContext, Callable.CustomData);
            }
        }
    }

    auto RegisterHook(UFunction* Function, UnrealScriptFunctionCallable PreCallback, UnrealScriptFunctionCallable PostCallback, void* CustomData) -> std::pair<int, int>
    {
        auto NativeFunction = Function->GetFunc();
        if (NativeFunction &&
            NativeFunction != UObject::ProcessInternalInternal.get_function_address() &&
            Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Native))
        {
            auto PreId = Function->RegisterPreHook(PreCallback, CustomData);
            auto PostId = Function->RegisterPostHook(PostCallback, CustomData);
            GlobalHooksInternal::GenericHookIdToNativeHookId.emplace(++GlobalHooksInternal::LastGenericHookId, PreId);
            auto GenericPreId = GlobalHooksInternal::LastGenericHookId;
            GlobalHooksInternal::GenericHookIdToNativeHookId.emplace(++GlobalHooksInternal::LastGenericHookId, PostId);
            auto GenericPostId = GlobalHooksInternal::LastGenericHookId;
            return {GenericPreId, GenericPostId};
        }
        else if (NativeFunction &&
                 NativeFunction == UObject::ProcessInternalInternal.get_function_address() &&
                 !Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Native))
        {
            if (!GlobalHooksInternal::bIsHookEnabled)
            {
                const Hook::FCallbackOptions GlobalScriptHookOptions {false, false, STR("UE4SS"), STR("GlobalScriptHook")};
                if (UObject::ProcessLocalScriptFunctionInternal.is_ready() && Version::IsAtLeast(4, 22))
                {
                    Hook::RegisterProcessLocalScriptFunctionPreCallback(GlobalScriptHookPre, GlobalScriptHookOptions);
                    Hook::RegisterProcessLocalScriptFunctionPostCallback(GlobalScriptHookPost, GlobalScriptHookOptions);
                }
                else if (UObject::ProcessInternalInternal.is_ready() && Version::IsBelow(4, 22))
                {
                    Hook::RegisterProcessInternalPreCallback(GlobalScriptHookPre, GlobalScriptHookOptions);
                    Hook::RegisterProcessInternalPostCallback(GlobalScriptHookPost, GlobalScriptHookOptions);
                }
                GlobalHooksInternal::bIsHookEnabled = true;
            }
            ++GlobalHooksInternal::LastGenericHookId;
            auto GenericPreId = GlobalHooksInternal::LastGenericHookId;
            auto GenericPostId = GlobalHooksInternal::LastGenericHookId;
            auto [Data, _] = GlobalHooksInternal::GlobalScriptHooks.emplace(Function->GetFullName(), GlobalHooksInternal::CallableData{});
            Data->second.Callables.emplace_back(PreCallback, PostCallback, CustomData, GenericPreId, GenericPostId);
            return {GenericPreId, GenericPostId};
        }
        else
        {
            std::string error_message{"Was unable to register a UFunction hook, information:\n"};
            error_message.append(fmt::format("UFunction::Func: {}\n", std::bit_cast<void*>(NativeFunction)));
            error_message.append(fmt::format("ProcessInternal: {}\n", UObject::ProcessInternalInternal.get_function_address()));
            error_message.append(fmt::format("FUNC_Native: {}\n", static_cast<uint32_t>(Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Native))));
            throw std::runtime_error{error_message};
        }
    }

    auto RegisterHook(const StringType& FunctionFullNameNoType, UnrealScriptFunctionCallable PreCallback, UnrealScriptFunctionCallable PostCallback, void* CustomData) -> std::pair<int, int>
    {
        auto Function = StaticFindObject<UFunction*>(nullptr, nullptr, FunctionFullNameNoType);
        return RegisterHook(Function, PreCallback, PostCallback, CustomData);
    }

    auto UnregisterHook(class UFunction* Function, std::pair<int, int> Ids) -> void
    {
        Output::send(STR("Unregistering hook\n"));
        auto NativeFunction = Function->GetFunc();
        if (NativeFunction &&
            NativeFunction != UObject::ProcessInternalInternal.get_function_address() &&
            Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Native))
        {
            Output::send(STR("Unregistering native hook ({}, {})\n"), Ids.first, Ids.second);
            if (auto PreNativeId = GlobalHooksInternal::GenericHookIdToNativeHookId.find(Ids.first); PreNativeId != GlobalHooksInternal::GenericHookIdToNativeHookId.end())
            {
                Function->UnregisterHook(PreNativeId->second);
                Output::send(STR("Native hook unregistered\n"));
            }
            if (auto PostNativeId = GlobalHooksInternal::GenericHookIdToNativeHookId.find(Ids.second); PostNativeId != GlobalHooksInternal::GenericHookIdToNativeHookId.end())
            {
                Function->UnregisterHook(PostNativeId->second);
            }
        }
        else if (NativeFunction &&
                 NativeFunction == UObject::ProcessInternalInternal.get_function_address() &&
                 !Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Native))
        {
            if (auto CallbackDataIt = GlobalHooksInternal::GlobalScriptHooks.find(Function->GetFullName()); CallbackDataIt != GlobalHooksInternal::GlobalScriptHooks.end())
            {
                for (const auto& CallbackData : CallbackDataIt->second.Callables)
                {
                }
                auto& Callbacks = CallbackDataIt->second.Callables;
                Callbacks.erase(std::remove_if(Callbacks.begin(), Callbacks.end(), [&](GlobalHooksInternal::CallableData::InternalData& CallbackData) -> bool {
                    return Ids.first == CallbackData.PreId && Ids.second == CallbackData.PostId;
                }), Callbacks.end());
            }
        }
        else
        {
            std::string error_message{"Was unable to unregister a UFunction hook, information:\n"};
            error_message.append(fmt::format("UFunction::Func: {}\n", std::bit_cast<void*>(NativeFunction)));
            error_message.append(fmt::format("ProcessInternal: {}\n", UObject::ProcessInternalInternal.get_function_address()));
            error_message.append(fmt::format("FUNC_Native: {}\n", static_cast<uint32_t>(Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Native))));
            throw std::runtime_error{error_message};
        }
    }

    auto UnregisterHook(const StringType& FunctionFullNameNoType, std::pair<int, int> Ids) -> void
    {
        auto Function = StaticFindObject<UFunction*>(nullptr, nullptr, FunctionFullNameNoType);
        if (!Function) { throw std::runtime_error{fmt::format("Unable to find function: {}", to_string(FunctionFullNameNoType))}; }
        UnregisterHook(Function, Ids);
    }
}

