// l26 layer-1 host fixture (agent-CP). Extracts the ACTUAL production logic
// edited for the FindXOf Find-first name-construction fix —
// MakeFindXOfClassName + LogFindXOfAddFallback and the eleven string-overload
// construction sites from candidate/src/UObjectGlobals.cpp — with the mock
// FName pool reproducing CO's predicted engine behavior (FNAME_Add through
// the bound ctor creates a NEW numbered duplicate pool entry instead of
// retrieving the existing stored one; FNAME_Find returns the stored entry or
// silently {0,0}). No engine calls anywhere. House style of tests/actual_*.
//
// RED   : original default-Add construction -> duplicated ComparisonIndex ->
//         Equals (index-only) miss -> walk returns nullptr for a live class.
// GREEN : Find-first construction -> stored index -> Equals match -> object.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
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

// ---- FName mock with the REAL semantics under test ----
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
    // index+number form (stored engine names: Number==0)
    FName(uint32_t i, uint32_t n) : ComparisonIndex(FNameEntryId::FromUnstableInt(i)), Number(n) {}
    bool IsNone() const { return ComparisonIndex.ToUnstableInt() == 0; } // NameTypes.hpp:414
    // Equals: index only (NameTypes.hpp:342-348) -- the FindXOf comparator
    bool Equals(const FName& o) const { return ComparisonIndex == o.ComparisonIndex; }
    FNameEntryId GetComparisonIndex() const { return ComparisonIndex; }
};
enum EFindName { FNAME_Find = 0, FNAME_Add = 1 };

// ---- mock name pool: CO bug #1 engine behavior ----
// Find: stored entry or silent {0,0} miss (CB: "miss -> 0 silently").
// Add of an INTERNED name: numbered duplicate entry with a fresh index
// (Hooks.cpp:518: "a new name is created with a number instead of
// retrieving the existing one") -- the original stored entry remains what
// the walk's GetNamePrivate() returns. Add of a NEW name: interns it and
// returns the stored index (upstream add-new-name semantics).
struct MockNamePool {
    std::map<std::wstring, uint32_t> stored;
    std::map<std::wstring, uint32_t> dup_index;
    uint32_t next_stored = 1;
    uint32_t next_dup = 0x1000;
    MockNamePool() { stored[L"None"] = 0; } // engine FNamePool::Init interns "None" first (CB section 4)

    FName intern(StringViewType s, EFindName mode)
    {
        const std::wstring key(s);
        auto it = stored.find(key);
        if (mode == FNAME_Find)
        {
            if (it == stored.end()) { return FName{0, 0}; } // silent miss
            return FName{it->second, 0};
        }
        // FNAME_Add through the bound ctor (the bug under test)
        if (it != stored.end())
        {
            auto& d = dup_index[key];
            if (d == 0) { d = next_dup++; }
            return FName{d, 1}; // numbered duplicate, index != stored
        }
        const uint32_t idx = next_stored++;
        stored[key] = idx;
        return FName{idx, 0};
    }
};
static MockNamePool g_pool;
static FName FName_construct(StringViewType s, EFindName mode)
{
    return g_pool.intern(s, mode);
}

// ---- mock object graph (walk consumers only) ----
struct UClass { FName name; FName GetNamePrivate() { return name; } };
struct UObject { UClass* klass = nullptr; UClass* GetClassPrivate() { return klass; } };
static bool IsValidObjectForFindXOf(UObject*) { return true; }
static std::vector<UObject*> g_objects;

// ---- ACTUAL production body: FindFirstOf(FName) linear walk (:501-525 shape) ----
static UObject* FindFirstOf(FName ClassName)
{
    UObject* ObjectFound = nullptr;
    for (UObject* Object : g_objects)
    {
        UClass* Class = Object->GetClassPrivate();
        if (Class->GetNamePrivate().Equals(ClassName) && IsValidObjectForFindXOf(Object))
        {
            ObjectFound = Object;
            break;
        }
    }
    return ObjectFound;
}
// ---- ACTUAL production body: FindAllOf(FName) collection walk (:612-636 shape) ----
static void FindAllOf(FName ClassName, std::vector<UObject*>& OutStorage)
{
    for (UObject* Object : g_objects)
    {
        UClass* Class = Object->GetClassPrivate();
        if (Class->GetNamePrivate().Equals(ClassName) && IsValidObjectForFindXOf(Object))
        {
            OutStorage.emplace_back(Object);
        }
    }
}

// ---- ACTUAL production body: LogFindXOfAddFallback (candidate verbatim,
//      Output::send swapped for g_log_lines) ----
static uint32_t g_ffo_add_fallback_indices[128]{};
static int32_t g_ffo_add_fallback_count = 0;
static void LogFindXOfAddFallback(FName Result)
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
        wchar_t buf[64];
        swprintf(buf, 64, L"[FFO] add-fallback [n:%X]\n", add_index);
        g_log_lines.emplace_back(buf);
    }
    catch (...)
    {
    }
}
// ---- ACTUAL production body: MakeFindXOfClassName (candidate verbatim) ----
static FName MakeFindXOfClassName(StringViewType ClassName)
{
    auto Name = FName_construct(ClassName, FNAME_Find);
    if (Name.IsNone())
    {
        Name = FName_construct(ClassName, FNAME_Add);
        LogFindXOfAddFallback(Name);
    }
    return Name;
}

// ---- ACTUAL production bodies: the Prospero (#ifdef) arms of the eleven
//      string-overload construction sites in candidate/src/UObjectGlobals.cpp ----
static UObject* FindFirstOf(const CharType* ClassName) { return FindFirstOf(MakeFindXOfClassName(ClassName)); }
static UObject* FindFirstOf(StringViewType ClassName) { return FindFirstOf(MakeFindXOfClassName(ClassName)); }
static UObject* FindFirstOf(const StringType& ClassName) { return FindFirstOf(MakeFindXOfClassName(ClassName)); }
static UObject* FindFirstOf(std::string_view ClassName) { return FindFirstOf(MakeFindXOfClassName(ensure_str(ClassName))); }
static UObject* FindFirstOf(const std::string& ClassName) { return FindFirstOf(MakeFindXOfClassName(ensure_str(ClassName))); }
static FName FindAllOfMakeClassName(const StringType& InClassName) { return MakeFindXOfClassName(InClassName); } // :553 construction
static void FindAllOf(const CharType* ClassName, std::vector<UObject*>& s) { FindAllOf(MakeFindXOfClassName(ClassName), s); }
static void FindAllOf(StringViewType ClassName, std::vector<UObject*>& s) { FindAllOf(MakeFindXOfClassName(ClassName), s); }
static void FindAllOf(const StringType& ClassName, std::vector<UObject*>& s) { FindAllOf(MakeFindXOfClassName(ClassName), s); }
static void FindAllOf(std::string_view ClassName, std::vector<UObject*>& s) { FindAllOf(MakeFindXOfClassName(ensure_str(ClassName)), s); }
static void FindAllOf(const std::string& ClassName, std::vector<UObject*>& s) { FindAllOf(MakeFindXOfClassName(ensure_str(ClassName)), s); }

// ---- ORIGINAL (pre-fix) construction: the #else / upstream arm, default
//      FNAME_Add through FName(StringViewType) -- used for the RED case ----
static FName FNameOriginalConstruction(StringViewType s) { return FName_construct(s, FNAME_Add); }
static UObject* FindFirstOfOriginal(StringViewType ClassName) { return FindFirstOf(FNameOriginalConstruction(ClassName)); }

int main()
{
    // Live object of a class whose name is interned in the pool (stored index 1):
    // interning a NEW name via Add registers the stored entry ({idx,0}).
    UClass player_controller_class{};
    player_controller_class.name = g_pool.intern(L"PlayerController", FNAME_Add);
    UObject pc_instance{};
    pc_instance.klass = &player_controller_class;
    UClass other_class{};
    other_class.name = g_pool.intern(L"SomethingElse", FNAME_Add);
    UObject other_instance{};
    other_instance.klass = &other_class;
    UObject pc_instance2{};
    pc_instance2.klass = &player_controller_class;
    g_objects = {&other_instance, &pc_instance, &pc_instance2};

    // ===== RED: CO's predicted failure with the original default-Add construction =====
    // Add("PlayerController") -> numbered duplicate {dup,1}, index != stored 1 ->
    // Equals (index-only) never true -> nullptr despite two live instances.
    UObject* red = FindFirstOfOriginal(L"PlayerController");
    assert(red == nullptr && "RED: original Add-duplicate construction must miss (CO bug #1)");
    FName red_name = FNameOriginalConstruction(L"PlayerController");
    assert(red_name.GetComparisonIndex().ToUnstableInt() != 1 && "RED: duplicate index must differ from stored");
    assert(red_name.Number != 0 && "RED: duplicate carries a number (Hooks.cpp:518)");
    assert(!red_name.Equals(player_controller_class.name) && "RED: Equals(index-only) must miss");
    // ...and FindAllOf with the original construction collects nothing.
    {
        std::vector<UObject*> out;
        FindAllOf(FNameOriginalConstruction(L"PlayerController"), out);
        assert(out.empty() && "RED: FindAllOf original construction collects nothing");
    }

    // ===== GREEN: Find-first construction resolves via the stored index =====
    assert(FindFirstOf((const CharType*)L"PlayerController") == &pc_instance);
    assert(FindFirstOf(StringViewType(L"PlayerController")) == &pc_instance);
    assert(FindFirstOf(StringType(L"PlayerController")) == &pc_instance);
    assert(FindFirstOf(std::string_view("PlayerController")) == &pc_instance);
    assert(FindFirstOf(std::string("PlayerController")) == &pc_instance);
    {
        std::vector<UObject*> out;
        FindAllOf((const CharType*)L"PlayerController", out);
        assert(out.size() == 2 && out[0] == &pc_instance && out[1] == &pc_instance2);
    }
    {
        std::vector<UObject*> out;
        FindAllOf(StringViewType(L"PlayerController"), out);
        assert(out.size() == 2);
    }
    {
        std::vector<UObject*> out;
        FindAllOf(StringType(L"PlayerController"), out);
        assert(out.size() == 2);
    }
    {
        std::vector<UObject*> out;
        FindAllOf(std::string_view("PlayerController"), out);
        assert(out.size() == 2);
    }
    {
        std::vector<UObject*> out;
        FindAllOf(std::string("PlayerController"), out);
        assert(out.size() == 2);
    }
    // The bUseCache overload's :553 construction site resolves identically.
    assert(FindAllOfMakeClassName(StringType(L"PlayerController")).Equals(player_controller_class.name));

    // ===== Marker semantics: Find hits logged nothing so far =====
    assert(g_log_lines.empty() && "GREEN: Find hits must not log");

    // ===== Marker: Add fallback for a NOT-interned name, deduped per name =====
    {
        UObject* miss = FindFirstOf(StringType(L"NeverInterned"));
        assert(miss == nullptr && "unknown class still finds nothing (no regression)");
        assert(g_log_lines.size() == 1 && "exactly one fallback line");
        assert(g_log_lines[0].rfind(STR("[FFO] add-fallback [n:"), 0) == 0);
        assert(g_log_lines[0].size() < 60 && "marker line < 60 chars (wide-char writer)");
        // Fallback preserved add-new-name semantics: the name is now interned...
        assert(g_pool.stored.count(L"NeverInterned") == 1);
        // ...so a SECOND lookup resolves via Find with NO further logging.
        assert(FindFirstOf(StringType(L"NeverInterned")) == nullptr);
        assert(g_log_lines.size() == 1 && "deduped: same name does not log again");
        // A different unknown name logs its own line (dedupe is per name).
        (void)FindFirstOf(StringType(L"AlsoNeverInterned"));
        assert(g_log_lines.size() == 2);
        // Original path agreed for never-interned names both before and after.
        assert(FindFirstOfOriginal(L"AlsoNeverInterned") == nullptr);
    }

    // ===== Literal "None" query: Find returns the stored index 0, which IS
    // NAME_None, so the fallback runs; Add duplicates it ({dup,1}) exactly as
    // the original one-line construction would; one [n:dup] line, no crash. =====
    {
        (void)FindFirstOf(StringType(L"None"));
        assert(g_log_lines.size() == 3);
        assert(g_log_lines[2].rfind(STR("[FFO] add-fallback [n:"), 0) == 0);
        assert(g_log_lines[2].find(STR("[n:0]")) == std::wstring::npos && "dup index is nonzero");
        (void)FindFirstOf(StringType(L"None"));
        assert(g_log_lines.size() == 3 && "same name does not log again");
    }

    printf("findxof-fname-fix: ALL PASS\n");
    return 0;
}
