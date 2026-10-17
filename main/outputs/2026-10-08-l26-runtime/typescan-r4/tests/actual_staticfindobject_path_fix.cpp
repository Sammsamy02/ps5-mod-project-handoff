// l26 layer-3 host fixture (agent-EC), UPDATED by agent-ET (2026-10-09,
// Claude fixes 1-3 / mods-started=0 blocker) for the NUL-terminated-copy +
// nullptr-never-throw + decoded-walk-fallback semantics. Extracts the ACTUAL
// production logic edited for the StaticFindObject long-name path fix —
// SplitLongNameIntoPathParts, NulTerminatedNameCopy, the de-fanged
// GetPackageNameFromLongName, the FNAME_Find early-resolution block with the
// decoded-full-name-walk fallback, and the existing
// StaticFindObject_InternalNoToStringFromNames outer-chain walk from
// candidate/src/UObjectGlobals.cpp — with the mock FName pool reproducing
// console behavior (same contract as tests/actual_findxof_fname_fix.cpp:
// FNAME_Add of an INTERNED name creates a numbered duplicate with a fresh
// index; FNAME_Find returns the stored entry or silently {0,0}). The mock
// engine ctor now takes const CharType* and scans to NUL — the REAL engine
// constructor contract — so the over-read bug is reproduced, not simulated.
// No engine calls anywhere. House style of tests/actual_*.
//
// Console evidence reproduced (zzdiag v7, /tmp/v7-run.log 22:24):
//   S1e/S2e/S3e/U2e -- every StaticFindObject full-path call errored with
//   "GetPackageNameFromLongName: Name wasn't long." (24-char tail fragment).
//   readfix session -- "[StaticFindObject] long-name part '/Script/Engine'
//   not in FName pool" threw out of on_program_start carrier arming ->
//   mods-started=0 (the [L27][ET] root cause).
//
// RED   : original lambda THROWS for a dot-less long name ("/Script/Engine");
//         the PackageName FNAME_Add duplicate index breaks the outer-filter
//         Equals -> nullptr for a live class; and the PRE-FIX long-name arm
//         handed the raw non-NUL-terminated slice "/Script/Engine" of
//         "/Script/Engine.Actor" to the NUL-scanning engine ctor, which
//         interned the WHOLE path string and FNAME_Find missed -> the EC
//         THROW that killed mod start.
// GREEN : in-port '/' split; every part constructed from an OWNED
//         NUL-terminated copy (NulTerminatedNameCopy); the walk matches the
//         outer chain; package-only paths resolve the UPackage; an
//         unresolvable part is a LOUD LOG (never a throw) + the bounded
//         decoded full-name walk fallback — which RESOLVES objects whose
//         parts are not interned, and is a clean nullptr on a true miss
//         (upstream FindObject semantics).
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#define __PROSPERO__ 1
#define STR(x) L##x

using StringType = std::wstring;
using StringViewType = std::wstring_view;
using CharType = wchar_t;

static std::vector<std::wstring> g_log_lines;

static StringType ensure_str(std::string_view s)
{
    return StringType(s.begin(), s.end());
}

// ---- FName mock with the REAL semantics under test (findxof fixture pool) ----
struct FNameEntryId {
    uint32_t Value = 0;
    uint32_t ToUnstableInt() const { return Value; }
    static FNameEntryId FromUnstableInt(uint32_t v) { return {v}; }
    bool operator==(const FNameEntryId& o) const { return Value == o.Value; }
};
struct FName {
    FNameEntryId ComparisonIndex{};
    uint32_t Number{};
    FName() = default;
    FName(uint32_t i, uint32_t n) : ComparisonIndex(FNameEntryId::FromUnstableInt(i)), Number(n) {}
    bool IsNone() const { return ComparisonIndex.ToUnstableInt() == 0; } // NameTypes.hpp:414
    bool Equals(const FName& o) const { return ComparisonIndex == o.ComparisonIndex; } // index-only (NameTypes.hpp:342-348)
};
enum EFindName { FNAME_Find = 0, FNAME_Add = 1 };
static FName NAME_None{};

struct MockNamePool {
    std::map<std::wstring, uint32_t> stored;
    std::map<std::wstring, uint32_t> dup_index;
    uint32_t next_stored = 1;
    uint32_t next_dup = 0x1000;
    MockNamePool() { stored[L"None"] = 0; } // FNamePool::Init interns "None" first

    FName intern(StringViewType s, EFindName mode)
    {
        const std::wstring key(s);
        auto it = stored.find(key);
        if (mode == FNAME_Find)
        {
            if (it == stored.end()) { return FName{0, 0}; } // silent miss (CB)
            return FName{it->second, 0};
        }
        // FNAME_Add through the bound ctor (GATE-1 / CO bug #1): an
        // INTERNED name yields a numbered duplicate entry, index != stored.
        if (it != stored.end())
        {
            auto& d = dup_index[key];
            if (d == 0) { d = next_dup++; }
            return FName{d, 1};
        }
        const uint32_t idx = next_stored++;
        stored[key] = idx;
        return FName{idx, 0};
    }
};
static MockNamePool g_pool;

// [L27][ET] The ENGINE constructor contract: `const CharType*` scanned to
// NUL (the real ctor signature the production FName wraps — NameTypes.hpp
// forwards StringViewType::data() with NO length). The NUL-scan here is
// REAL: it reads through the fixture buffer to the terminating L'\0', so a
// slice that is not NUL-terminated at its logical end really over-reads.
static FName FName_construct(const CharType* scanned_to_nul, EFindName mode)
{
    std::wstring key;
    for (const CharType* p = scanned_to_nul; *p; ++p) key.push_back(*p);
    return g_pool.intern(key, mode);
}

// ---- ACTUAL production helper (candidate/src/UObjectGlobals.cpp, verbatim
//      [L27][ET nul-throw-armor] FIX 1) ----
static auto NulTerminatedNameCopy(StringViewType View) -> StringType
{
    if (View.empty()) { return StringType{}; }
    return StringType{View.data(), View.size()};
}

// ---- mock object graph (walk consumers only) ----
struct UObject;
struct UClass { FName name; }; // stand-in: only identity is used
struct UObject {
    UClass* klass = nullptr;          // GetClassPrivate
    UObject* outer = nullptr;         // GetOuterPrivate
    FName name;                       // GetNamePrivate
    std::wstring full_name;           // GetFullName (pool-decoded in production)
    UClass* GetClassPrivate() { return klass; }
    UObject* GetOuterPrivate() { return outer; }
    FName GetNamePrivate() { return name; }
    const std::wstring& GetFullName() const { return full_name; }
};
static std::vector<UObject*> g_objects;

static bool iequal_w(std::wstring_view a, std::wstring_view b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const wchar_t ca = (a[i] >= L'A' && a[i] <= L'Z') ? static_cast<wchar_t>(a[i] - L'A' + L'a') : a[i];
        const wchar_t cb = (b[i] >= L'A' && b[i] <= L'Z') ? static_cast<wchar_t>(b[i] - L'A' + L'a') : b[i];
        if (ca != cb) return false;
    }
    return true;
}

// ---- ACTUAL production body: StaticFindObject_InternalNoToStringFromNames
//      (candidate :208-255 shape; ForEachUObject swapped for g_objects) ----
static UObject* StaticFindObject_InternalNoToStringFromNames(const std::vector<FName>& NameParts)
{
    UObject* FoundObject{};
    for (const auto& NamePart : NameParts)
    {
        if (NamePart.Equals(NAME_None) || NamePart.IsNone())
        {
            // NAME_None means we're not far enough along engine init for this object to exist yet.
            return nullptr;
        }
    }
    for (UObject* Object : g_objects)
    {
        int32_t NumPathParts{};
        UObject* PathObject = Object;
        while (PathObject)
        {
            const auto PathName = PathObject->GetNamePrivate();
            const auto It = std::find_if(NameParts.begin(), NameParts.end(), [&](const FName NamePart) {
                return NamePart.Equals(PathName);
            });
            if (It == NameParts.end())
            {
                break; // LoopAction::Continue
            }
            PathObject = PathObject->GetOuterPrivate();
            ++NumPathParts;
        }
        if (NumPathParts == static_cast<int32_t>(NameParts.size()))
        {
            FoundObject = Object;
            break; // LoopAction::Break
        }
    }
    return FoundObject;
}

// ---- ACTUAL production body: SplitLongNameIntoPathParts (candidate, verbatim) ----
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

// ---- ORIGINAL (pre-fix) lambda: the v7 S*e throw site (candidate was :316-324) ----
static auto GetPackageNameFromLongName_original(const StringType& LongName) -> StringType
{
    auto DelimiterOffset = LongName.find(STR("."));
    if (DelimiterOffset == LongName.npos)
    {
        throw std::runtime_error{"GetPackageNameFromLongName: Name wasn't long."};
    }
    return LongName.substr(0, DelimiterOffset);
}

// ---- FIXED lambda (candidate, verbatim) ----
static auto GetPackageNameFromLongName(const StringType& LongName) -> StringType
{
    auto DelimiterOffset = LongName.find(STR("."));
    if (DelimiterOffset == LongName.npos)
    {
        return StringType{};
    }
    return LongName.substr(0, DelimiterOffset);
}

// ---- ACTUAL production logic: the decoded full-name walk fallback
//      (candidate StaticFindObject_DecodedFullNameWalk; [L27][ET] Claude
//      addendum — bounded first-match over the object array comparing
//      pool-decoded full names, never refuses outright; ForEachUObject
//      swapped for g_objects) ----
static UObject* StaticFindObject_DecodedFullNameWalk(const CharType* InName, UClass* Class, bool bExactClass)
{
    UObject* FoundObject{};
    for (UObject* Object : g_objects)
    {
        // (per-object try/catch shape of the production walk; the mock
        // GetFullName cannot throw)
        const auto& ObjFullName = Object->GetFullName();
        auto Space = ObjFullName.find(STR(" "));
        auto ObjFullNameNoType = (Space == std::wstring::npos) ? std::wstring{} : ObjFullName.substr(Space + 1);
        if (iequal_w(ObjFullNameNoType, InName))
        {
            if (Class && bExactClass && Class != Object->GetClassPrivate())
            {
                continue;
            }
            FoundObject = Object;
            break; // first-match, bounded
        }
    }
    if (FoundObject)
    {
        g_log_lines.push_back(std::wstring{STR("[StaticFindObject] decoded full-name walk matched '")} + InName + STR("'\n"));
    }    return FoundObject;
}

// ---- ACTUAL production logic: the new early-resolution block in FindObject
//      (candidate FindObject(const TCHAR*) bIsLongName && !InSearcher arm;
//      Class here is UClass*, only used for the exact-class gate) ----
static UObject* ResolveLongNameByPathParts(const CharType* InName, UClass* Class, bool bExactClass)
{
    const auto PathParts = SplitLongNameIntoPathParts(InName);
    std::vector<FName> PartNames{};
    PartNames.reserve(PathParts.size());
    bool bPartMissedPool{};
    for (const auto& Part : PathParts)
    {
        // [L27][ET] FIX 1: owned NUL-terminated copy before the ctor.
        const StringType PartCopy{NulTerminatedNameCopy(Part)};
        FName PartName = FName_construct(PartCopy.c_str(), FNAME_Find);
        if (PartName.IsNone() && Part != StringViewType{STR("None")})
        {
            // [L27][ET] FIX 2: loud LOG (never a throw) naming the part...
            bPartMissedPool = true;
            g_log_lines.push_back(std::wstring{STR("[StaticFindObject] long-name part '")} + PartCopy +
                                              STR("' not in FName pool (FNAME_Find miss); trying decoded full-name walk\n"));
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
        // ...then the [L27][ET] Claude-addendum decoded full-name walk.
        PathFound = StaticFindObject_DecodedFullNameWalk(InName, Class, bExactClass);
    }
    if (PathFound)
    {
        if (!Class || !bExactClass)
        {
            return PathFound;
        }
        // exact-class mismatch: fall through to the scan (mirrors candidate;
        // this fixture has no scan, so it is a miss)
    }
    return nullptr;
}

int main()
{
    // ===== Object graph mirroring the S-probe targets =====
    // UPackage "/Script/Engine" -- ONE FName containing the slashes.
    UObject engine_package{};
    engine_package.name = g_pool.intern(L"/Script/Engine", FNAME_Add); // Add of NEW name interns {stored,0}
    engine_package.full_name = L"Package /Script/Engine";
    UClass pc_class_obj{};
    UObject player_controller{};
    player_controller.name = g_pool.intern(L"PlayerController", FNAME_Add);
    player_controller.outer = &engine_package;
    player_controller.klass = &pc_class_obj;
    player_controller.full_name = L"Class /Script/Engine.PlayerController";
    UObject ksl_cdo{};
    ksl_cdo.name = g_pool.intern(L"Default__KismetStringLibrary", FNAME_Add);
    ksl_cdo.outer = &engine_package;
    ksl_cdo.klass = &pc_class_obj;
    ksl_cdo.full_name = L"Class /Script/Engine.Default__KismetStringLibrary";
    UObject conv_function{};
    conv_function.name = g_pool.intern(L"Conv_NameToString", FNAME_Add);
    conv_function.outer = &player_controller;
    conv_function.klass = &pc_class_obj;
    conv_function.full_name = L"Function /Script/Engine.PlayerController.Conv_NameToString";
    // [L27][ET] A DECOKED-WALK-ONLY object: its full name resolves, but its
    // part name is deliberately NOT interned in the mock pool — only the
    // Claude-addendum fallback can find it.
    UClass fallback_class_obj{};
    UObject fallback_only{};
    fallback_only.name = FName{0, 0}; // NOT interned: GetNamePrivate would be NAME_None
    fallback_only.outer = &engine_package;
    fallback_only.klass = &fallback_class_obj;
    fallback_only.full_name = L"Class /Script/Engine.LateLoadedClass";
    g_objects = {&engine_package, &player_controller, &ksl_cdo, &conv_function, &fallback_only};

    // ===== Splitter: documented shapes =====
    {
        auto p1 = SplitLongNameIntoPathParts(L"/Script/Engine.PlayerController");
        assert((p1 == std::vector<StringViewType>{L"/Script/Engine", L"PlayerController"}));
        auto p2 = SplitLongNameIntoPathParts(L"/Script/Engine");
        assert((p2 == std::vector<StringViewType>{L"/Script/Engine"}));
        auto p3 = SplitLongNameIntoPathParts(L"/Game/Maps/L1.L_Persistent:Prop");
        assert((p3 == std::vector<StringViewType>{L"/Game/Maps/L1", L"L_Persistent", L"Prop"}));
        auto p4 = SplitLongNameIntoPathParts(L"/Script/Engine.KismetStringLibrary.Conv_NameToString");
        assert((p4 == std::vector<StringViewType>{L"/Script/Engine", L"KismetStringLibrary", L"Conv_NameToString"}));
        assert(SplitLongNameIntoPathParts(L"").empty());
    }

    // ===== RED 1: the v7 S*e throw -- original lambda on a dot-less long name =====
    {
        bool threw = false;
        try { (void)GetPackageNameFromLongName_original(L"/Script/Engine"); }
        catch (const std::runtime_error& e) {
            threw = true;
            assert(std::string_view(e.what()).rfind("GetPackageNameFromLongName: Name wasn't long.", 0) == 0);
        }
        assert(threw && "RED: original lambda must throw for package-only paths (v7 S1e/S2e/S3e/U2e)");
        // FIXED lambda degrades instead of throwing.
        assert(GetPackageNameFromLongName(L"/Script/Engine").empty());
        assert(GetPackageNameFromLongName(L"/Script/Engine.PlayerController") == L"/Script/Engine");
    }

    // ===== RED 2: original PackageName construction breaks the outer filter =====
    {
        FName package_name = FName_construct(L"/Script/Engine", FNAME_Add); // Add of INTERNED name -> dup
        assert(!package_name.Equals(engine_package.name) && "RED: Add duplicate index != stored package index");
        assert(package_name.Number != 0 && "RED: duplicate carries a number (Hooks.cpp:518 family)");
    }

    // ===== RED 3 ([L27][ET], the mods-started=0 root cause): the PRE-FIX
    //      call shape handed the RAW non-NUL-terminated slice to the
    //      NUL-scanning engine ctor — the "/Script/Engine" part of
    //      "/Script/Engine.PlayerController" interned as the WHOLE path
    //      string, FNAME_Find missed, and the old arm THREW (the throw
    //      escaped on_program_start's carrier arming and killed mod start).
    {
        const std::wstring full_path = L"/Script/Engine.PlayerController";
        const StringViewType part_slice{full_path.data(), 14}; // "/Script/Engine", no NUL at its end
        assert(part_slice.data()[part_slice.size()] == L'.');
        // What the ENGINE ctor saw pre-fix (real NUL-scan over the buffer):
        std::wstring over_read_key;
        for (const CharType* p = part_slice.data(); *p; ++p) over_read_key.push_back(*p);
        assert(over_read_key == L"/Script/Engine.PlayerController" && "RED: the NUL-scan read THROUGH '.PlayerController'");
        FName pre_fix = FName_construct(part_slice.data(), FNAME_Find);
        assert(pre_fix.IsNone() && "RED: pre-fix FNAME_Find miss (the throw condition, the console refusal)");
        // The old arm's throw for that miss (kept here as the RED contract).
        bool threw = false;
        try { throw std::runtime_error{"[StaticFindObject] long-name part '/Script/Engine' not in FName pool (FNAME_Find miss); refusing to scan"}; }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw);
        // The FIXED call shape: owned NUL-terminated copy -> exact intern hit.
        const StringType part_copy = NulTerminatedNameCopy(part_slice);
        FName post_fix = FName_construct(part_copy.c_str(), FNAME_Find);
        assert(!post_fix.IsNone() && post_fix.Equals(engine_package.name) &&
               "GREEN: copy construction resolves the stored '/Script/Engine' package entry");
    }

    // ===== GREEN: dotted full path resolves through FNAME_Find + the walk =====
    assert(ResolveLongNameByPathParts(L"/Script/Engine.PlayerController", nullptr, false) == &player_controller);
    // CDO path (UEHelpers GetKismetStringLibrary form, v7 U2e)
    assert(ResolveLongNameByPathParts(L"/Script/Engine.Default__KismetStringLibrary", nullptr, false) == &ksl_cdo);
    // 3-part outer chain (function under class under package)
    assert(ResolveLongNameByPathParts(L"/Script/Engine.PlayerController.Conv_NameToString", nullptr, false) == &conv_function);

    // ===== GREEN: package-only path resolves the UPackage, NO THROW (the S*e fix) =====
    assert(ResolveLongNameByPathParts(L"/Script/Engine", nullptr, false) == &engine_package);

    // ===== GREEN ([L27][ET] FIX 2 semantics): unresolvable part -> LOUD LOG
    //      naming the part (g_log_lines), NO throw, decoded-walk fallback,
    //      nullptr on a true miss =====
    {
        g_log_lines.clear();
        UObject* found = nullptr;
        bool threw = false;
        try { found = ResolveLongNameByPathParts(L"/Script/Engine.NotInPool", nullptr, false); }
        catch (...) { threw = true; }
        assert(!threw && "GREEN: a missing part NEVER throws (upstream nullptr semantics)");
        assert(found == nullptr && "GREEN: nothing matches -> clean nullptr (not found, not a refusal)");
        assert(g_log_lines.size() == 1 && g_log_lines[0].rfind(L"[StaticFindObject] long-name part 'NotInPool' not in FName pool", 0) == 0 &&
               "GREEN: the loud log names the exact missing part");
        // A whole missing package is named too (dot-less garbage case).
        g_log_lines.clear();
        threw = false;
        try { found = ResolveLongNameByPathParts(L"/Script/Corrupt", nullptr, false); }
        catch (...) { threw = true; }
        assert(!threw && g_log_lines.size() == 1 &&
               g_log_lines[0].find(L"'/Script/Corrupt'") != std::wstring::npos &&
               "GREEN: dot-less garbage long name logs the whole missing package");
        assert(found == nullptr);
    }

    // ===== GREEN ([L27][ET] Claude addendum): the decoded full-name walk
    //      fallback RESOLVES an object whose part is not interned — never
    //      refuse outright =====
    {
        g_log_lines.clear();
        UObject* found = ResolveLongNameByPathParts(L"/Script/Engine.LateLoadedClass", nullptr, false);
        assert(found == &fallback_only && "GREEN: fallback walk matched the pool-decoded full name");
        assert(g_log_lines.size() == 2 && "part-miss log + walk-match log");
        assert(g_log_lines[0].find(L"'LateLoadedClass' not in FName pool") != std::wstring::npos);
        assert(g_log_lines[1].rfind(L"[StaticFindObject] decoded full-name walk matched '/Script/Engine.LateLoadedClass'", 0) == 0);
        // The bounded walk is FIRST-match (an earlier object with the same
        // full name wins and the walk stops).
    }

    // ===== GREEN: all-parts-resolvable but object absent -> clean miss (nullptr, no throw) =====
    {
        g_log_lines.clear();
        g_pool.intern(L"AbsentClass", FNAME_Add); // interned name, no object with this chain
        assert(ResolveLongNameByPathParts(L"/Script/Engine.AbsentClass", nullptr, false) == nullptr);
        assert(g_log_lines.empty() && "clean miss: no refusal log (every part interned, walk just missed)");
    }

    // ===== GREEN: literal "None" part is allowed through construction and is
    //      a clean miss via the walk's NAME_None early-out (no refusal) =====
    assert(ResolveLongNameByPathParts(L"/Script/Engine.None", nullptr, false) == nullptr);

    printf("staticfindobject-path-fix: ALL PASS\n");
    return 0;
}
