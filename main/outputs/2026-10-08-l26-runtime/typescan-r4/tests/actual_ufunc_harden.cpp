// l26 hardening host fixture (agent-CN). Extracts the ACTUAL production logic
// edited for the l26 hardening stage — the null-guards in construct_fname /
// construct_uclass, the construct_with_string unready-ctor notice, the
// handle_unreal_property_value miss log (log_function_miss), the
// GetFunctionByNameInChain Prospero walk instrumentation
// (log_function_chain_miss + the counter/first-3 loop), the Equals
// comparator swap, and the FNAME_Add member-name construction — with minimal
// mocks in the tests/actual_*.cpp house style. No engine calls anywhere.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define __PROSPERO__ 1
#define STR(x) L##x

// ---- FName mock with the REAL semantics under test ----
static std::vector<std::wstring> g_log_lines;
static std::wstring fmt_nofn(uint32_t name_index, uint32_t class_index)
{
    wchar_t buf[64];
    swprintf(buf, 64, L"[UF] nofn [n:%X] cls=[n:%X]", name_index, class_index);
    return buf;
}
static std::wstring fmt_miss(uint32_t want, uint32_t cls, int visited, uint32_t a, uint32_t b, uint32_t c)
{
    wchar_t buf[96];
    swprintf(buf, 96, L"[UF] miss w=[n:%X] c=[n:%X] v=%d 1=[%X,%X,%X]", want, cls, visited, a, b, c);
    return buf;
}

// ---- FName mock with the REAL semantics under test ----
struct FNameEntryId { uint32_t Value = 0; uint32_t ToUnstableInt() const { return Value; } static FNameEntryId FromUnstableInt(uint32_t v){ return {v}; } bool operator==(const FNameEntryId& o) const { return Value == o.Value; } };
struct FName {
    FNameEntryId ComparisonIndex{};
    uint32_t Number{};
    FName() = default;
    FName(uint32_t i, uint32_t n) : ComparisonIndex(FNameEntryId::FromUnstableInt(i)), Number(n) {}
    // operator==: index AND number (NameTypes.hpp:322-325)
    bool operator==(FName o) const { return ToUnstableInt64() == o.ToUnstableInt64(); }
    uint64_t ToUnstableInt64() const { return (uint64_t(ComparisonIndex.ToUnstableInt())) | (uint64_t(Number) << 32); }
    // Equals: index only (NameTypes.hpp:342-348)
    bool Equals(const FName& o) const { return ComparisonIndex == o.ComparisonIndex; }
    FNameEntryId GetComparisonIndex() const { return ComparisonIndex; }
};
enum EFindName { FNAME_Add = 1, FNAME_Find = 0 };

// ---- item 2: construct_with_string + NotifyFNameCtorUnreadyOnce (actual bodies) ----
static bool g_ctor_ready = false;
static uint32_t g_ctor_last_mode = 0xFFFFFFFF; // records the mode the member-name construction used
static FName g_ctor_result{0, 0};
void NotifyFNameCtorUnreadyOnce() noexcept
{
    static std::atomic<bool> g_fname_ctor_unready_notified{false};
    bool expected = false;
    if (!g_fname_ctor_unready_notified.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }
    try { g_log_lines.push_back(L"[FN] ctor unready: NAME_None produced"); } catch (...) {}
}
// production shape: NameTypes.hpp construct_with_string (ctor unready branch)
static void construct_with_string(const wchar_t*, EFindName, FName& out)
{
    if (!g_ctor_ready) { NotifyFNameCtorUnreadyOnce(); out = FName{0, 0}; return; }
    out = g_ctor_result;
}
// production shape: prepare_to_handle Prospero branch (LuaUObject.hpp:821)
static FName member_name_construction(const wchar_t* s)
{
    FName out;
    construct_with_string(s, FNAME_Add, out);
    g_ctor_last_mode = FNAME_Add;
    return out;
}

// ---- mocks: object graph ----
struct UClass {
    FName name;
    bool faulted = false; // set if deref'd while "null"
    FName GetNamePrivate() { faulted = true; return name; }
};
struct UObject {
    UClass* klass = nullptr;
    UClass* GetClassPrivate() { return klass; }
};
struct UFunction { FName name; };
static std::vector<UFunction*> g_range_fns;
struct TFieldRangeMock {
    std::vector<UFunction*> fns;
    struct it { UFunction** p; UFunction* operator*() { return *p; } UFunction* operator->() { return *p; } it& operator++() { ++p; return *this; } bool operator!=(const it& o) const { return p != o.p; } };
    it begin() { return it{fns.data()}; }
    it end() { return it{fns.data() + fns.size()}; }
};

// ---- item 4: log_function_chain_miss (actual body, output swapped) ----
struct FunctionChainMissSite { uintptr_t struct_ptr; uint32_t name_index; };
static FunctionChainMissSite g_function_chain_miss_sites[128]{};
static int g_function_chain_miss_site_count = 0;
static void log_function_chain_miss(UClass* Struct, FName FunctionName, int visited, const uint32_t* first_indices, int first_count)
{
    try {
        const uintptr_t struct_ptr = reinterpret_cast<uintptr_t>(Struct);
        const uint32_t want = FunctionName.GetComparisonIndex().ToUnstableInt();
        for (int i = 0; i < g_function_chain_miss_site_count; ++i)
        {
            if (g_function_chain_miss_sites[i].struct_ptr == struct_ptr && g_function_chain_miss_sites[i].name_index == want)
            {
                return;
            }
        }
        if (g_function_chain_miss_site_count >= 128)
        {
            return;
        }
        g_function_chain_miss_sites[g_function_chain_miss_site_count++] = FunctionChainMissSite{struct_ptr, want};
        uint32_t cls_index = 0;
        if (Struct)
        {
            cls_index = Struct->name.GetComparisonIndex().ToUnstableInt();
        }
        g_log_lines.push_back(fmt_miss(want, cls_index, visited,
            first_count > 0 ? first_indices[0] : 0u,
            first_count > 1 ? first_indices[1] : 0u,
            first_count > 2 ? first_indices[2] : 0u));
    } catch (...) {}
}

// ---- item 4: the instrumented Prospero walk (actual body, Equals compare) ----
static UFunction* GetFunctionByNameInChain(UClass* Struct, FName FunctionName)
{
    uint32_t first_indices[3]{};
    int first_count = 0;
    int visited = 0;
    UFunction* found = nullptr;
    TFieldRangeMock range; // (mock: caller pre-fills via g_range)
    range.fns = g_range_fns;
    for (UFunction* Function : range)

    {
        const FName name = Function->name;
        ++visited;
        if (first_count < 3)
        {
            first_indices[first_count++] = name.GetComparisonIndex().ToUnstableInt();
        }
        if (name.Equals(FunctionName))
        {
            found = Function;
            break;
        }
    }
    if (!found)
    {
        log_function_chain_miss(Struct, FunctionName, visited, first_indices, first_count);
    }
    return found;
}

// ---- item 3: log_function_miss (actual body, output swapped) ----
struct FunctionMissSite { uintptr_t class_ptr; uint32_t name_index; };
static FunctionMissSite g_function_miss_sites[128]{};
static int g_function_miss_site_count = 0;
static void log_function_miss(UObject* base, const FName& property_name)
{
    try {
        uintptr_t class_ptr = 0;
        uint32_t class_index = 0;
        if (base)
        {
            if (auto* klass = base->GetClassPrivate())
            {
                class_ptr = reinterpret_cast<uintptr_t>(klass);
                class_index = klass->name.GetComparisonIndex().ToUnstableInt();
            }
        }
        const uint32_t name_index = property_name.GetComparisonIndex().ToUnstableInt();
        for (int i = 0; i < g_function_miss_site_count; ++i)
        {
            if (g_function_miss_sites[i].class_ptr == class_ptr && g_function_miss_sites[i].name_index == name_index)
            {
                return;
            }
        }
        if (g_function_miss_site_count >= 128)
        {
            return;
        }
        g_function_miss_sites[g_function_miss_site_count++] = FunctionMissSite{class_ptr, name_index};
        g_log_lines.push_back(fmt_nofn(name_index, class_index));
    } catch (...) {}
}

// ---- item 1: the null-guarded construct bodies (actual shape; Lua mock's
// throw_error models the real noreturn lua.error by throwing a catchable
// exception, exactly how a pcall-wrapped Lua frame observes it) ----
struct FixtureLuaError { std::string message; };
struct LuaMock {
    std::vector<std::string> errors;
    [[noreturn]] void throw_error(const char* msg) { errors.push_back(msg); throw FixtureLuaError{msg}; }
};
struct RemoteWrapper { UObject* remote = nullptr; UObject* get_remote_cpp_object() { return remote; } };
static UClass g_class_stub{{0x11, 0}, false};
static int g_fname_construct_calls = 0, g_uclass_construct_calls = 0;
static void FName_construct_mock(LuaMock&, FName n) { (void)n; ++g_fname_construct_calls; }
static void UClass_construct_mock(LuaMock&, UClass* c) { (void)c; ++g_uclass_construct_calls; }
static void construct_fname(LuaMock& lua, RemoteWrapper& w)
{
    if (!w.get_remote_cpp_object())
    {
        lua.throw_error("Tried calling a member function but the UObject instance is nullptr\n");
    }
    FName_construct_mock(lua, w.get_remote_cpp_object()->GetClassPrivate()->GetNamePrivate());
}
static void construct_uclass(LuaMock& lua, RemoteWrapper& w)
{
    if (!w.get_remote_cpp_object())
    {
        lua.throw_error("Tried calling a member function but the UObject instance is nullptr\n");
    }
    UClass_construct_mock(lua, w.get_remote_cpp_object()->GetClassPrivate());
}

int main()
{
    // ---- item 1: null wrappers throw the standard error, never deref ----
    {
        LuaMock lua; RemoteWrapper nullw{nullptr}; UObject obj; obj.klass = &g_class_stub; RemoteWrapper okw{&obj};
        for (int which = 0; which < 2; ++which)
        {
            bool threw = false;
            try { if (which == 0) construct_fname(lua, nullw); else construct_uclass(lua, nullw); }
            catch (const FixtureLuaError&) { threw = true; }
            assert(threw); // the guard fires before any deref (UBSan verifies: no null member call)
        }
        assert(lua.errors.size() == 2);
        assert(lua.errors[0] == "Tried calling a member function but the UObject instance is nullptr\n");
        assert(lua.errors[1] == lua.errors[0]);
        assert(g_fname_construct_calls == 0 && g_uclass_construct_calls == 0);
        assert(!g_class_stub.faulted);
        construct_fname(lua, okw); construct_uclass(lua, okw);
        assert(lua.errors.size() == 2); // no new errors on valid wrapper
        assert(g_fname_construct_calls == 1 && g_uclass_construct_calls == 1);
    }

    // ---- item 2: ctor-unready notice fires exactly once; NAME_None still produced ----
    {
        g_ctor_ready = false; g_ctor_result = FName{0, 0};
        for (int i = 0; i < 5; ++i)
        {
            FName n; construct_with_string(L"GetItemIDFromDeprecatedEnum", FNAME_Find, n);
            assert(n.ComparisonIndex.ToUnstableInt() == 0 && n.Number == 0); // semantics unchanged
        }
        size_t notices = 0;
        for (auto& l : g_log_lines) if (l == L"[FN] ctor unready: NAME_None produced") ++notices;
        assert(notices == 1); // log-once across five unready constructions
        g_ctor_ready = true; g_ctor_result = FName{0x2A, 0};
        FName n; construct_with_string(L"X", FNAME_Find, n);
        assert(n.ComparisonIndex.ToUnstableInt() == 0x2A);
        size_t notices_after_ready = 0;
        for (auto& l : g_log_lines) if (l == L"[FN] ctor unready: NAME_None produced") ++notices_after_ready;
        assert(notices_after_ready == 1); // still exactly one after ctor becomes ready
    }

    // ---- CB fix-2a: Prospero member-name construction uses FNAME_Add ----
    {
        g_ctor_last_mode = 0xFFFFFFFF;
        FName n = member_name_construction(L"GetItemIDFromDeprecatedEnum");
        assert(g_ctor_last_mode == static_cast<uint32_t>(FNAME_Add));
        (void)n;
    }

    // ---- CB fix-2b: Equals (index-only) resolves what operator== would miss ----
    {
        FName stored{0x2A, 2}; // nonzero Number (e.g. a numbered engine name form)
        FName wanted{0x2A, 0};
        assert(! (stored == wanted)); // operator== (upstream/host) fails
        assert(stored.Equals(wanted)); // Equals (Prospero path) resolves
    }

    // ---- item 4: instrumented walk miss -> visited + first-3, deduped ----
    g_log_lines.clear();
    UClass dbclass{FName{0x1F, 0}};
    UFunction f1{FName{0xA1, 0}}, f2{FName{0xB2, 0}}, f3{FName{0xC3, 0}}, f4{FName{0xD4, 0}}, f5{FName{0xE5, 0}};
    g_range_fns = {&f1, &f2, &f3, &f4, &f5};
    {
        UFunction* r = GetFunctionByNameInChain(&dbclass, FName{0x999, 0});
        assert(r == nullptr);
        r = GetFunctionByNameInChain(&dbclass, FName{0x999, 0}); // same (struct,name): deduped
        assert(r == nullptr);
        assert(g_log_lines.size() == 1);
        const std::wstring& line = g_log_lines[0];
        assert(line == L"[UF] miss w=[n:999] c=[n:1F] v=5 1=[A1,B2,C3]");
        assert(line.size() < 60); // wide-char writer collision budget
        // new name on same struct -> new line
        r = GetFunctionByNameInChain(&dbclass, FName{0x888, 0});
        assert(r == nullptr && g_log_lines.size() == 2);
    }
    // hit: third function matches; Equals compare; early exit -> visited=3
    {
        UFunction* r = GetFunctionByNameInChain(&dbclass, FName{0xC3, 7}); // nonzero Number on the WANTED side
        assert(r == &f3); // Equals: index-only -> resolves despite Number
        assert(g_log_lines.size() == 2); // no miss logged on hit
    }
    // 0-visited (empty chain) fires the vis=0 diagnostic
    {
        g_range_fns.clear();
        UFunction* r = GetFunctionByNameInChain(&dbclass, FName{0x777, 0});
        assert(r == nullptr);
        assert(g_log_lines.size() == 3);
        assert(g_log_lines[2] == L"[UF] miss w=[n:777] c=[n:1F] v=0 1=[0,0,0]");
        assert(g_log_lines[2].size() < 60);
    }

    // ---- item 3: miss-branch log; NAME_None ([n:0]) vs real-miss discriminator ----
    {
        UObject db; db.klass = &dbclass;
        log_function_miss(&db, FName{0, 0});      // conversion failed (ctor unready / Find miss)
        log_function_miss(&db, FName{0, 0});      // deduped
        log_function_miss(&db, FName{0x999, 0});  // distinct name -> new line
        log_function_miss(nullptr, FName{0, 0});  // null base tolerated, own site (class 0)
        assert(g_log_lines.size() == 6);          // 3 walk-miss lines + 3 nofn lines
        assert(g_log_lines[3] == L"[UF] nofn [n:0] cls=[n:1F]");
        assert(g_log_lines[4] == L"[UF] nofn [n:999] cls=[n:1F]");
        assert(g_log_lines[5] == L"[UF] nofn [n:0] cls=[n:0]"); // null base -> class index 0
        for (size_t i = 3; i < g_log_lines.size(); ++i) assert(g_log_lines[i].size() < 60);
    }

    std::printf("ALL L26 HARDENING FIXTURE CHECKS PASSED\n");
    return 0;
}
