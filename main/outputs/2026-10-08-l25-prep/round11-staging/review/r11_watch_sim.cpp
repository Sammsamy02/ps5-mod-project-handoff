// R11-WATCH host simulation (reviewer, Mac-only). Copies r9idx::Observe / MergeWatch / Sync and
// r9_index_note_deleted from outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp
// (R11) with a fake object array. Only engine reads (LiveAt, GetClassPrivate), the class lists,
// the name table and the timers are stubbed. Build: clang++ -std=c++20 -O2 r11_watch_sim.cpp
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>

template <typename T> using Vec = std::vector<T>;
using UObject = void;
using UClass = void;

// ---------------- fake engine ----------------
struct FakeObj { uintptr_t Addr; uint64_t Gen; uintptr_t Class; };
struct Engine
{
    Vec<FakeObj> Slot;        // Addr == 0: empty
    Vec<uint8_t> Unreach;
    int32_t PublishedNum{0};  // what GetNumElements() returns (may lag allocations)
    int32_t HighWater{0};
    Vec<int32_t> FreeIdx;     // LIFO (engine reuses the most recently freed index first)
    Vec<uintptr_t> FreeAddr;  // LIFO (same-address reuse)
    uintptr_t NextAddr{0x1000};
    uint64_t NextGen{1};
    std::unordered_map<uintptr_t, int32_t> SlotOf; // live address -> slot
} E;

static auto LiveAt(int32_t Index) -> UObject*
{
    if (Index < 0 || Index >= static_cast<int32_t>(E.Slot.size())) { return nullptr; }
    if (E.Slot[Index].Addr == 0) { return nullptr; }
    if (E.Unreach[Index]) { return nullptr; }
    return reinterpret_cast<UObject*>(E.Slot[Index].Addr);
}
static auto ClassOf(UObject* O) -> UClass*
{
    // the sim's GetClassPrivate: the class of the object currently at that address
    auto it = E.SlotOf.find(reinterpret_cast<uintptr_t>(O));
    if (it == E.SlotOf.end() || E.Slot[it->second].Addr != reinterpret_cast<uintptr_t>(O)) std::abort();
    return reinterpret_cast<UClass*>(E.Slot[it->second].Class);
}

// ---------------- index state (subset of r9idx::State) ----------------
struct State
{
    bool Built{false};
    bool Disabled{false};
    int32_t SyncedNum{0};
    Vec<UObject*> ObjAt{};
    Vec<UClass*> ClassAt{};
    Vec<UClass*> ListedUnder{};
    Vec<int32_t> Dirty{};
    Vec<int32_t> FreeWatch{};
    Vec<int32_t> WatchAdd{};
    Vec<int32_t> WatchTmp{};
    int32_t WatchResume{0};
    Vec<uint8_t> InWatch{};
    bool NotifyWanted{true};
    Vec<int32_t> NewIdx{};
    Vec<UObject*> NewObj{};
    Vec<uint8_t> NameIn{};
} S;
static size_t g_dirty_limit = 4 * 1024 * 1024; // lowered in the sim to exercise the rebuild path
static uint64_t g_cuts = 0, g_rebuilds = 0, g_builds = 0;

static auto NameUnlink(State& S, int32_t Index) -> void { S.NameIn[Index] = 0; }
static auto NameNote(State& S, int32_t Index, UObject*) -> bool { S.NameIn[Index] = 1; return false; }
static auto ClassSlot(State&, UClass*) -> int { return 0; }
static auto Grow(State& S, int32_t Num) -> void
{
    if (static_cast<int32_t>(S.ObjAt.size()) >= Num) { return; }
    size_t NewSize = S.ObjAt.empty() ? 64 : S.ObjAt.size();
    while (NewSize < static_cast<size_t>(Num)) { NewSize *= 2; }
    S.ObjAt.resize(NewSize, nullptr);
    S.ClassAt.resize(NewSize, nullptr);
    S.ListedUnder.resize(NewSize, nullptr);
    S.InWatch.resize(NewSize, 0);
    S.NameIn.resize(NewSize, 0);
}

// ---- copied: Observe ----
static auto Observe(State& S, int32_t Index) -> void
{
    UObject* Object = LiveAt(Index);
    if (Object == nullptr)
    {
        S.ObjAt[Index] = nullptr;
        S.ClassAt[Index] = nullptr;
        NameUnlink(S, Index); // [R9-NAME]
        if (!S.InWatch[Index]) { S.InWatch[Index] = 1; S.WatchAdd.push_back(Index); }
        return;
    }
    UClass* Class = ClassOf(Object);
    (void)NameNote(S, Index, Object);
    if (S.ObjAt[Index] == Object && S.ClassAt[Index] == Class) { return; }
    if (S.Built && S.NotifyWanted && S.ObjAt[Index] != Object && S.NewIdx.size() < (1u << 20))
    {
        S.NewIdx.push_back(Index);
        S.NewObj.push_back(Object);
    }
    S.ObjAt[Index] = Object;
    S.ClassAt[Index] = Class;
    if (Class && S.ListedUnder[Index] != Class) { ClassSlot(S, Class); S.ListedUnder[Index] = Class; }
    else if (Class) { ClassSlot(S, Class); }
}
// ---- copied: MergeWatch ----
static auto MergeWatch(State& S) -> void
{
    if (S.WatchAdd.empty()) { return; }
    if (!std::is_sorted(S.WatchAdd.begin(), S.WatchAdd.end())) { std::sort(S.WatchAdd.begin(), S.WatchAdd.end()); }
    if (S.FreeWatch.empty()) { S.FreeWatch.swap(S.WatchAdd); S.WatchAdd.clear(); return; }
    S.WatchTmp.clear();
    S.WatchTmp.reserve(S.FreeWatch.size() + S.WatchAdd.size());
    std::merge(S.FreeWatch.begin(), S.FreeWatch.end(), S.WatchAdd.begin(), S.WatchAdd.end(), std::back_inserter(S.WatchTmp));
    S.FreeWatch.swap(S.WatchTmp);
    S.WatchAdd.clear();
}
// ---- copied: Sync (timers, GUObjectArray check and class self-index check removed) ----
static auto Sync(State& S, bool (*BudgetExpired)() = nullptr) -> bool
{
    const int32_t Num = E.PublishedNum;
    if (Num < 0 || Num > 64 * 1024 * 1024) { S.Disabled = true; return false; }
    Grow(S, Num);
    if (!S.Built)
    {
        S.FreeWatch.clear();
        S.WatchAdd.clear();
        S.WatchResume = 0;
        S.InWatch.assign(S.InWatch.size(), 0);
        for (int32_t i = 0; i < Num; ++i) { Observe(S, i); }
        MergeWatch(S); // ascending already
        S.SyncedNum = Num;
        S.Built = true;
        S.Dirty.clear();
        ++g_builds;
        return true;
    }
    for (int32_t i : S.Dirty)
    {
        if (i >= 0 && i < Num)
        {
            S.ObjAt[i] = nullptr;
            S.ClassAt[i] = nullptr;
            NameUnlink(S, i);
            if (!S.InWatch[i]) { S.InWatch[i] = 1; S.WatchAdd.push_back(i); }
        }
    }
    S.Dirty.clear();
    MergeWatch(S);
    const size_t n = S.FreeWatch.size();
    size_t Start = 0;
    if (BudgetExpired != nullptr && S.WatchResume > 0)
    {
        Start = static_cast<size_t>(std::lower_bound(S.FreeWatch.begin(), S.FreeWatch.end(), S.WatchResume) - S.FreeWatch.begin());
        if (Start >= n) { Start = 0; }
    }
    bool bCut = false;
    size_t Filled = 0;
    for (size_t Step = 0; Step < n; ++Step)
    {
        size_t Pos = Start + Step;
        if (Pos >= n) { Pos -= n; }
        if (BudgetExpired != nullptr && Step > 0 && (Step & 255) == 0 && BudgetExpired())
        {
            S.WatchResume = S.FreeWatch[Pos];
            bCut = true;
            break;
        }
        const int32_t i = S.FreeWatch[Pos];
        if (i >= Num || LiveAt(i) == nullptr) { continue; }
        S.FreeWatch[Pos] = -1;
        ++Filled;
        S.InWatch[i] = 0;
        Observe(S, i);
    }
    if (Filled > 0) { S.FreeWatch.erase(std::remove(S.FreeWatch.begin(), S.FreeWatch.end(), -1), S.FreeWatch.end()); }
    if (bCut) { ++g_cuts; }
    else { S.WatchResume = 0; }
    for (int32_t i = S.SyncedNum; i < Num; ++i) { Observe(S, i); }
    S.SyncedNum = Num;
    MergeWatch(S);
    return true;
}
// ---- copied: r9_index_note_deleted (lock, timer, DeletesSeen removed) ----
static auto NoteDeleted(int32_t Index) -> void
{
    if (!S.Built || S.Disabled) { return; }
    if (S.Dirty.size() > g_dirty_limit) { S.Built = false; S.Dirty.clear(); S.FreeWatch.clear(); ++g_rebuilds; return; }
    S.Dirty.push_back(Index);
}

// ---------------- driver ----------------
static std::mt19937_64 R{12345};
static double g_cut_p = 0.0;
static bool Budget() { return std::uniform_real_distribution<double>(0, 1)(R) < g_cut_p; }
static uint64_t g_fail = 0;
static uint64_t g_listed_with_entry = 0;
static int32_t g_prev_synced_num = 0;
static std::set<int32_t> g_allowed;
// round-robin fairness: consecutive budgeted passes a filled watched slot stayed unseen
static Vec<uint32_t> g_unseen_age;
static uint64_t g_max_age = 0, g_age_violations = 0, g_max_n_window = 0;
static auto CheckFairness() -> void
{
    if (g_unseen_age.size() < S.ObjAt.size()) g_unseen_age.resize(S.ObjAt.size(), 0);
    // pre-pass list size bounds a lap (entries erased during a pass shrink n afterwards); use the largest
    // size seen since the last exact sync, plus slack
    g_max_n_window = std::max<uint64_t>(g_max_n_window, S.FreeWatch.size() + 1024);
    const uint64_t bound = g_max_n_window / 256 + 3;
    for (int32_t i = 0; i < S.SyncedNum; ++i)
    {
        if (S.ObjAt[i] == nullptr && LiveAt(i) != nullptr) { ++g_unseen_age[i]; }
        else { g_unseen_age[i] = 0; }
        if (g_unseen_age[i] > g_max_age) g_max_age = g_unseen_age[i];
        if (g_unseen_age[i] > 2 * bound + 4) ++g_age_violations;
    }
}
#define CHECK(c, ...) do { if (!(c)) { if (g_fail++ < 20) { std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } } while (0)

// generation of each object reported by notify (collect_new filter semantics)
static std::set<uint64_t> g_reported;
static uint64_t g_build_gen_floor = 0; // objects created before the last (re)build are never reported

static auto Collect() -> void
{
    for (size_t k = 0; k < S.NewIdx.size(); ++k)
    {
        const int32_t i = S.NewIdx[k];
        if (LiveAt(i) == S.NewObj[k] && S.ObjAt[i] == S.NewObj[k]) { g_reported.insert(E.Slot[i].Gen); }
    }
    S.NewIdx.clear();
    S.NewObj.clear();
}

static auto CheckStructure(const char* when) -> void
{
    CHECK(S.WatchAdd.empty(), "%s WatchAdd not empty after Sync (%zu)", when, S.WatchAdd.size());
    for (size_t k = 0; k < S.FreeWatch.size(); ++k)
    {
        CHECK(S.FreeWatch[k] >= 0, "%s FreeWatch[%zu] = %d", when, k, S.FreeWatch[k]);
        if (k) CHECK(S.FreeWatch[k - 1] < S.FreeWatch[k], "%s FreeWatch not strictly ascending at %zu", when, k);
    }
    size_t flagged = 0;
    for (size_t i = 0; i < S.InWatch.size(); ++i) flagged += S.InWatch[i];
    CHECK(flagged == S.FreeWatch.size(), "%s InWatch count %zu != FreeWatch size %zu", when, flagged, S.FreeWatch.size());
    for (int32_t i : S.FreeWatch) if (i >= 0) CHECK(S.InWatch[i] == 1, "%s listed %d without InWatch", when, i);
    // after ANY sync: an index entry never names a different object than the one in the slot
    for (int32_t i = 0; i < S.SyncedNum; ++i)
    {
        if (S.ObjAt[i] == nullptr) continue;
        CHECK(E.Slot[i].Addr == reinterpret_cast<uintptr_t>(S.ObjAt[i]), "%s slot %d stale address", when, i);
        CHECK(reinterpret_cast<uintptr_t>(S.ClassAt[i]) == E.Slot[i].Class, "%s slot %d stale class", when, i);
    }
    // a slot with no index entry must be watched (otherwise a later fill is never seen)
    for (int32_t i = 0; i < S.SyncedNum; ++i)
        if (S.ObjAt[i] == nullptr) CHECK(S.InWatch[i] == 1, "%s slot %d empty in index but not watched", when, i);
    // a watched slot normally holds no entry (the cheap vacant re-check relies on this). Exception found
    // by this sim: a slot freed in [SyncedNum, Num) (Dirty) and refilled before the append loop is listed
    // AND observed. Counted (g_listed_with_entry); its entry is checked for staleness above, and it is
    // harmless as long as that entry always names the slot's current object (checked).
    // (a cut budgeted pass can leave such a slot unvisited, so the state may persist: carried over)
    std::set<int32_t> now_allowed;
    for (int32_t i : S.FreeWatch)
        if (i >= 0 && i < static_cast<int32_t>(S.ObjAt.size()) && S.ObjAt[i] != nullptr)
        {
            ++g_listed_with_entry;
            CHECK(i >= g_prev_synced_num || g_allowed.count(i), "%s watched slot %d has an entry and was below the previous SyncedNum %d", when, i, g_prev_synced_num);
            now_allowed.insert(i);
        }
    g_allowed.swap(now_allowed);
}
static auto CheckExact() -> void
{
    // exact query semantics: every live object in [0, Num) is in the index with its own class
    for (int32_t i = 0; i < E.PublishedNum; ++i)
    {
        UObject* L = LiveAt(i);
        if (L == nullptr) continue;
        CHECK(S.ObjAt[i] == L, "exact: slot %d live object missing from index (in watch=%d)", i, S.InWatch[i]);
        CHECK(reinterpret_cast<uintptr_t>(S.ClassAt[i]) == E.Slot[i].Class, "exact: slot %d wrong class", i);
        CHECK(S.NameIn[i] == 1, "exact: slot %d not in name table", i);
        // notify: every object created after the last build that is still live has been reported
        if (E.Slot[i].Gen > g_build_gen_floor)
            CHECK(g_reported.count(E.Slot[i].Gen) == 1, "exact: slot %d gen %llu never reported as new", i, (unsigned long long)E.Slot[i].Gen);
    }
}

static auto Alloc(int32_t Cap) -> void
{
    int32_t i;
    std::uniform_int_distribution<int> d100(0, 99);
    if (!E.FreeIdx.empty() && d100(R) < 85) { i = E.FreeIdx.back(); E.FreeIdx.pop_back(); }
    else if (E.HighWater < Cap) { i = E.HighWater++; }
    else if (!E.FreeIdx.empty()) { i = E.FreeIdx.back(); E.FreeIdx.pop_back(); }
    else return;
    uintptr_t a;
    if (!E.FreeAddr.empty() && d100(R) < 70) { a = E.FreeAddr.back(); E.FreeAddr.pop_back(); }
    else { a = E.NextAddr; E.NextAddr += 0x40; }
    const uintptr_t cls = 0x100 + 0x10 * (d100(R) % 4);
    E.Slot[i] = FakeObj{a, E.NextGen++, cls};
    E.SlotOf[a] = i;
    E.Unreach[i] = 0;
    // publish Num: usually right away, sometimes lagging (allocations past the published count)
    if (d100(R) < 80) E.PublishedNum = std::max(E.PublishedNum, E.HighWater);
}
static auto Free(int32_t i) -> void
{
    if (E.Slot[i].Addr == 0) return;
    NoteDeleted(i); // engine calls the delete listener, then clears the slot
    E.FreeAddr.push_back(E.Slot[i].Addr);
    E.SlotOf.erase(E.Slot[i].Addr);
    E.Slot[i] = FakeObj{0, 0, 0};
    E.Unreach[i] = 0;
    E.FreeIdx.push_back(i);
}

int main(int argc, char** argv)
{
    const uint64_t Rounds = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 400;
    uint64_t steps = 0, exact = 0, budgeted = 0, sameaddr_reuse = 0;
    for (uint64_t round = 0; round < Rounds; ++round)
    {
        // fresh world per round, with varied sizes / rates
        E = Engine{};
        S = State{};
        g_reported.clear();
        g_allowed.clear();
        g_unseen_age.assign(g_unseen_age.size(), 0);
        g_build_gen_floor = 0;
        const int32_t Cap = (round % 4 == 3) ? 20000 + static_cast<int32_t>(R() % 20000) : 200 + static_cast<int32_t>(R() % 3000);
        E.Slot.assign(Cap, FakeObj{0, 0, 0});
        E.Unreach.assign(Cap, 0);
        g_dirty_limit = (round % 5 == 0) ? 50 + R() % 200 : 4 * 1024 * 1024;
        g_cut_p = (round % 3 == 0) ? 0.0 : (round % 3 == 1 ? 0.3 : 0.9);
        for (int k = 0; k < Cap / 2; ++k) Alloc(Cap);
        E.PublishedNum = E.HighWater;
        Sync(S); // first build
        g_build_gen_floor = E.NextGen - 1;
        Collect();
        CheckStructure("build");
        for (int step = 0; step < 600; ++step, ++steps)
        {
            // engine activity between syncs
            const int ops = 1 + static_cast<int>(R() % (step % 50 == 0 ? Cap : 40)); // occasional mass unload / load
            for (int o = 0; o < ops; ++o)
            {
                const int r = static_cast<int>(R() % 100);
                if (r < 40) Alloc(Cap);
                else if (r < 80)
                {
                    if (E.HighWater == 0) continue;
                    const int32_t i = static_cast<int32_t>(R() % E.HighWater);
                    const uintptr_t a = E.Slot[i].Addr;
                    Free(i);
                    // immediate same-slot same-address reuse (worst case for "seen as new")
                    if (a && R() % 3 == 0 && !E.FreeIdx.empty() && E.FreeIdx.back() == i)
                    {
                        E.FreeIdx.pop_back();
                        E.FreeAddr.pop_back();
                        E.Slot[i] = FakeObj{a, E.NextGen++, (R() % 2) ? E.Slot[i].Class : 0x100};
                        if (E.Slot[i].Class == 0) E.Slot[i].Class = 0x100;
                        E.SlotOf[a] = i;
                        ++sameaddr_reuse;
                    }
                }
                else if (r < 90)
                {
                    if (E.HighWater == 0) continue;
                    const int32_t i = static_cast<int32_t>(R() % E.HighWater);
                    if (E.Slot[i].Addr) E.Unreach[i] = 1; // GC marks unreachable (freed later)
                }
                else if (r < 95)
                {
                    if (E.HighWater == 0) continue;
                    const int32_t i = static_cast<int32_t>(R() % E.HighWater);
                    if (E.Unreach[i]) Free(i);
                }
                else E.PublishedNum = E.HighWater;
            }
            const bool rebuilding = !S.Built;
            g_prev_synced_num = S.Built ? S.SyncedNum : 0;
            const bool budget_phase = (step / 40) % 2 == 1 && step % 40 != 39; // long runs of budgeted passes, then an exact one
            if (budget_phase || R() % 2)
            {
                Sync(S, g_cut_p > 0 ? &Budget : nullptr);
                ++budgeted;
                if (rebuilding) g_build_gen_floor = E.NextGen - 1;
                Collect();
                CheckStructure("budgeted");
                CheckFairness();
            }
            else
            {
                Sync(S);
                ++exact;
                if (rebuilding) g_build_gen_floor = E.NextGen - 1;
                Collect();
                CheckStructure("exact");
                CheckExact();
                std::fill(g_unseen_age.begin(), g_unseen_age.end(), 0);
                g_max_n_window = 0;
            }
        }
    }
    std::printf("rounds=%llu steps=%llu exact_syncs=%llu budgeted_syncs=%llu cuts=%llu builds=%llu rebuild_triggers=%llu same_addr_slot_reuses=%llu listed_with_entry=%llu failures=%llu\n",
                (unsigned long long)Rounds, (unsigned long long)steps, (unsigned long long)exact, (unsigned long long)budgeted,
                (unsigned long long)g_cuts, (unsigned long long)g_builds, (unsigned long long)g_rebuilds,
                (unsigned long long)sameaddr_reuse, (unsigned long long)g_listed_with_entry, (unsigned long long)g_fail);
    std::printf("round-robin: max budgeted passes a filled watched slot waited=%llu (violations of 2*(n/256+3)+4: %llu)\n", (unsigned long long)g_max_age, (unsigned long long)g_age_violations);
    std::printf("%s\n", g_fail ? "SIM: FAIL" : "SIM: PASS");
    return g_fail ? 1 : 0;
}
