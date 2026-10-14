// cap #4 host test of r9_index_snapshot_live (extracted verbatim from the patched UObjectGlobals.cpp)
// against a fake r9idx state with the same field names. Mac-only.
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace RC::Unreal
{
    struct UClass;
    struct UObject
    {
        UClass* cls{nullptr};
        bool live{true};
        auto GetClassPrivate() const -> UClass* { return cls; }
    };
    struct UClass : UObject
    {
        int id{0};
    };
    namespace UObjectGlobals
    {
        namespace r9idx
        {
            template <typename T>
            using Vec = std::vector<T>;
            struct State
            {
                std::mutex Mutex{};
                bool Built{false};
                bool Disabled{false};
                int32_t SyncedNum{0};
                Vec<UObject*> ObjAt{};
                Vec<UClass*> ClassAt{};
                Vec<UClass*> Classes{};
                Vec<UClass*> HashKeys{};
                Vec<int32_t> HashVals{};
            };
            static State* const g_state = new State{};
            static auto LiveAt(int32_t Index) -> UObject*
            {
                UObject* o = g_state->ObjAt[static_cast<size_t>(Index)];
                return (o && o->live) ? o : nullptr;
            }
            static auto HashPos(UClass* Class, size_t Mask) -> size_t
            {
                uint64_t h = reinterpret_cast<uintptr_t>(Class);
                h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 29;
                return static_cast<size_t>(h) & Mask;
            }
        }
#include "snapshot_extract.inc"
    }
}

using namespace RC::Unreal;
using namespace RC::Unreal::UObjectGlobals;

static int g_fail = 0, g_checks = 0;
static void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_fail; std::printf("  CHECK FAILED: %s\n", what); }
}

static int g_want_calls = 0;
static UClass* g_wanted = nullptr;
static UClass* g_forbidden = nullptr; // a stale (old, possibly unloaded) class: Want must never see it
static int g_forbidden_calls = 0;
static bool want(UClass* c)
{
    ++g_want_calls;
    if (c != nullptr && c == g_forbidden) { ++g_forbidden_calls; }
    return c == g_wanted;
}

int main()
{
    auto& S = *r9idx::g_state;
    std::vector<int32_t> idx;
    std::vector<void*> obj;
    // not built -> false, nothing returned
    check(!r9_index_snapshot_live(idx, obj, &want), "unbuilt index refuses");
    // build: 3 classes (A wanted, B not, C not hashed), 12 slots
    UClass A{}, B{}, Cn{};
    A.id = 1; B.id = 2; Cn.id = 3;
    S.Classes = {&A, &B};
    S.HashKeys.assign(16, nullptr);
    S.HashVals.assign(16, -1);
    for (int c = 0; c < 2; ++c)
    {
        size_t p = r9idx::HashPos(S.Classes[static_cast<size_t>(c)], 15);
        while (S.HashKeys[p] != nullptr) { p = (p + 1) & 15; }
        S.HashKeys[p] = S.Classes[static_cast<size_t>(c)];
        S.HashVals[p] = c;
    }
    std::vector<UObject> objs(12);
    S.ObjAt.assign(16, nullptr);
    S.ClassAt.assign(16, nullptr);
    for (int i = 0; i < 12; ++i)
    {
        UClass* c = (i % 3 == 0) ? &A : ((i % 3 == 1) ? &B : &Cn);
        objs[static_cast<size_t>(i)].cls = c;
        S.ObjAt[static_cast<size_t>(i)] = &objs[static_cast<size_t>(i)];
        S.ClassAt[static_cast<size_t>(i)] = c;
    }
    objs[3].live = false;        // slot 3 (class A) died: skipped
    S.ObjAt[12] = nullptr;       // beyond SyncedNum anyway
    S.SyncedNum = 12;
    S.Built = true;
    g_wanted = &A;
    g_want_calls = 0;
    check(r9_index_snapshot_live(idx, obj, &want), "built index answers");
    check(idx.size() == 3 && idx[0] == 0 && idx[1] == 6 && idx[2] == 9, "A objects 0,6,9 (3 skipped: dead)");
    check(obj.size() == idx.size() && obj[1] == &objs[6], "object pointers match slots");
    // Want: once per hashed class (A, B) + once per unhashed-class object (C: 4 objects)
    check(g_want_calls == 2 + 4, "Want memoized per hashed class; unhashed class asked per object");
    // review finding 3: slot 6 freed and refilled at the SAME address since the last sync, so
    // ClassAt[6] is stale. (a) stale entry names the wanted class A, the object now names B: skipped.
    objs[6].cls = &B;
    check(r9_index_snapshot_live(idx, obj, &want), "built index answers (refilled slot, a)");
    check(idx.size() == 2 && idx[0] == 0 && idx[1] == 9, "refilled slot 6 skipped (current class B != ClassAt A)");
    // (b) stale entry names an old class that no live object names any more: Want never sees it.
    UClass Old{};
    Old.id = 4;
    objs[6].cls = &A;
    S.ClassAt[6] = &Old;
    g_forbidden = &Old;
    g_forbidden_calls = 0;
    check(r9_index_snapshot_live(idx, obj, &want), "built index answers (refilled slot, b)");
    check(g_forbidden_calls == 0, "Want never called with a stale class pointer");
    check(idx.size() == 2 && idx[0] == 0 && idx[1] == 9, "slot 6 left to the next collect (reported as new there)");
    S.ClassAt[6] = &A;
    g_forbidden = nullptr;
    // nullptr Want / disabled -> false and cleared outputs
    check(!r9_index_snapshot_live(idx, obj, nullptr) && idx.empty() && obj.empty(), "null predicate refuses and clears");
    S.Disabled = true;
    check(!r9_index_snapshot_live(idx, obj, &want) && idx.empty(), "disabled index refuses");
    std::printf("%s snapshot: %d checks, %d failed\n", (g_fail == 0 && g_checks > 0) ? "PASS" : "FAIL", g_checks, g_fail);
    return (g_fail == 0 && g_checks > 0) ? 0 : 1;
}
