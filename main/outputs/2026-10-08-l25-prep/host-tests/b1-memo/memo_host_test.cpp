// B1 host test (Mac-only): FindFirstOf/FindAllOf walk memo vs the un-memoized
// reference walk. Mock UObject/UClass with the same predicate shapes as
// UObjectGlobals.cpp (IsValidObjectForFindXOf = per-object flags + per-class
// IsA tests; query match = direct class name, else TSuperStructRange name).
//
//   FIXED  = the B1 helpers (flags per object, class part memoized)
//   STAGED = round5-staging/lookup-speed/03 as staged (whole verdict per class)
//
// Pass criteria: FIXED == REFERENCE on every trial (FindFirstOf result and
// FindAllOf set+order); STAGED is expected to DIVERGE when a CDO is visited
// first (the bug the B1 fix removes) — the test asserts that it does, so the
// bug is demonstrated, not assumed.
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <random>
#include <vector>

struct UClass;
struct UObject
{
    UClass* Class{};
    bool CDO{};
    bool Archetype{};
};
struct UClass
{
    int Name{};
    UClass* Super{};
    bool IsMetaType{};  // stands in for IsA<UClass/UFunction/UPackage/UEnum/UScriptStruct>
};

static bool IsExcluded(UObject* o) { return o->CDO || o->Archetype; }
static bool IsValidClass(UObject* o)
{
    for (UClass* c = o->Class; c; c = c->Super)
        if (c->IsMetaType) return false;
    return true;
}
static bool IsValidObject(UObject* o) { return !IsExcluded(o) && IsValidClass(o); }
static bool ChainContains(UClass* c, int name)
{
    for (UClass* s = c; s; s = s->Super)  // TSuperStructRange(Class) starts at Class
        if (s->Name == name) return true;
    return false;
}

// ---- reference (pre-B1) walk -------------------------------------------------
static UObject* RefFindFirstOf(std::vector<UObject*>& objs, int name)
{
    UObject* found = nullptr;
    for (UObject* o : objs)
    {
        bool direct = o->Class->Name == name;
        if (direct && IsValidObject(o)) return o;
        if (!IsValidObject(o)) continue;
        if (ChainContains(o->Class, name)) found = o;
    }
    return found;
}
static void RefFindAllOf(std::vector<UObject*>& objs, int name, std::vector<UObject*>& out)
{
    for (UObject* o : objs)
    {
        bool direct = o->Class->Name == name;
        if (direct && IsValidObject(o)) { out.push_back(o); continue; }
        if (!IsValidObject(o)) continue;
        if (ChainContains(o->Class, name)) out.push_back(o);
    }
}

// ---- FIXED memo (B1 helpers, same shapes as UObjectGlobals.cpp) ---------------
struct Verdict { UClass* Class{nullptr}; int8_t ClassPartValid{-1}; int8_t ChainMatchesQuery{-1}; };
struct Memo { static constexpr uint32_t TableSize = 2048; Verdict Slots[TableSize]{}; };
static Verdict* Slot(Memo* m, UClass* c)
{
    if (!m) return nullptr;
    uint32_t k = (uint32_t(reinterpret_cast<uintptr_t>(c) >> 6)) & (Memo::TableSize - 1);
    Verdict* s = &m->Slots[k];
    if (s->Class != c) *s = Verdict{c, -1, -1};
    return s;
}
static bool Valid(UObject* o, Verdict* s)
{
    if (!s) return IsValidObject(o);
    if (IsExcluded(o)) return false;
    if (s->ClassPartValid < 0) s->ClassPartValid = IsValidClass(o) ? 1 : 0;
    return s->ClassPartValid != 0;
}
static bool Chain(UClass* c, Verdict* s, int name)
{
    if (s && s->ChainMatchesQuery >= 0) return s->ChainMatchesQuery != 0;
    bool f = ChainContains(c, name);
    if (s) s->ChainMatchesQuery = f ? 1 : 0;
    return f;
}
static UObject* FixedFindFirstOf(std::vector<UObject*>& objs, int name, bool noMemo)
{
    std::unique_ptr<Memo> m{noMemo ? nullptr : new (std::nothrow) Memo{}};
    UObject* found = nullptr;
    for (UObject* o : objs)
    {
        bool direct = o->Class->Name == name;
        Verdict* s = Slot(m.get(), o->Class);
        bool v = Valid(o, s);
        if (direct && v) return o;
        if (!v) continue;
        if (Chain(o->Class, s, name)) found = o;
    }
    return found;
}
static void FixedFindAllOf(std::vector<UObject*>& objs, int name, std::vector<UObject*>& out)
{
    std::unique_ptr<Memo> m{new (std::nothrow) Memo{}};
    for (UObject* o : objs)
    {
        bool direct = o->Class->Name == name;
        Verdict* s = Slot(m.get(), o->Class);
        bool v = Valid(o, s);
        if (direct && v) { out.push_back(o); continue; }
        if (!v) continue;
        if (Chain(o->Class, s, name)) out.push_back(o);
    }
}

// ---- STAGED memo (as in 03-findxof-walk-memo.patch: whole verdict per class) --
struct SVerdict { UClass* Class{nullptr}; int8_t Valid{-1}; int8_t Chain{-1}; };
static UObject* StagedFindFirstOf(std::vector<UObject*>& objs, int name)
{
    std::unique_ptr<SVerdict[]> t{new SVerdict[2048]{}};
    UObject* found = nullptr;
    for (UObject* o : objs)
    {
        UClass* c = o->Class;
        SVerdict* s = &t[(uint32_t(reinterpret_cast<uintptr_t>(c) >> 6)) & 2047];
        if (s->Class != c) *s = SVerdict{c, -1, -1};
        if (s->Valid < 0) s->Valid = IsValidObject(o) ? 1 : 0;
        bool direct = c->Name == name;
        if (direct && s->Valid) return o;
        if (!s->Valid) continue;
        if (s->Chain < 0) s->Chain = ChainContains(c, name) ? 1 : 0;
        if (s->Chain) found = o;
    }
    return found;
}

int main()
{
    std::mt19937 rng(20261012);
    int trials = 0, fixedMismatch = 0, nomemoMismatch = 0, stagedDiverged = 0, allMismatch = 0;
    for (int t = 0; t < 400; ++t)
    {
        // class forest: some meta types, chains up to depth 6
        int nClasses = 50 + int(rng() % 400);
        std::vector<std::unique_ptr<UClass>> classes;
        for (int i = 0; i < nClasses; ++i)
        {
            auto c = std::make_unique<UClass>();
            c->Name = int(rng() % (nClasses / 2 + 1));  // duplicate names happen (BP-style)
            c->Super = (i > 0 && rng() % 5) ? classes[rng() % i].get() : nullptr;
            c->IsMetaType = (rng() % 23) == 0;
            classes.push_back(std::move(c));
        }
        // objects: every class gets its CDO FIRST (engine order), then instances/archetypes
        std::vector<std::unique_ptr<UObject>> store;
        std::vector<UObject*> objs;
        for (auto& c : classes)
        {
            store.push_back(std::make_unique<UObject>(UObject{c.get(), true, false}));
            objs.push_back(store.back().get());
        }
        int nInst = 200 + int(rng() % 4000);
        for (int i = 0; i < nInst; ++i)
        {
            store.push_back(std::make_unique<UObject>(UObject{classes[rng() % nClasses].get(), false, (rng() % 31) == 0}));
            objs.push_back(store.back().get());
        }
        for (int q = 0; q < 8; ++q)
        {
            int name = int(rng() % (nClasses / 2 + 1));
            ++trials;
            UObject* ref = RefFindFirstOf(objs, name);
            if (FixedFindFirstOf(objs, name, false) != ref) ++fixedMismatch;
            if (FixedFindFirstOf(objs, name, true) != ref) ++nomemoMismatch;
            if (StagedFindFirstOf(objs, name) != ref) ++stagedDiverged;
            std::vector<UObject*> a, b;
            RefFindAllOf(objs, name, a);
            FixedFindAllOf(objs, name, b);
            if (a != b) ++allMismatch;
        }
    }
    std::printf("trials=%d fixed_findfirst_mismatch=%d fixed_nomemo_mismatch=%d fixed_findall_mismatch=%d staged_findfirst_diverged=%d\n",
                trials, fixedMismatch, nomemoMismatch, allMismatch, stagedDiverged);
    bool pass = fixedMismatch == 0 && nomemoMismatch == 0 && allMismatch == 0 && stagedDiverged > 0;
    std::printf("%s b1-memo host test (fixed == reference; staged patch diverges: %s)\n", pass ? "PASS" : "FAIL",
                stagedDiverged > 0 ? "yes, bug reproduced" : "NO - bug not reproduced");
    return pass ? 0 : 1;
}
