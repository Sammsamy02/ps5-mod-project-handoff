#!/usr/bin/env python3
"""apply_b6p.py — B6p: per-frame budget for the object index's freed-slot re-check.

- The per-frame NotifyOnNewObject feed passes a budget (0.5 ms, steady_clock in LuaMod.cpp, so no new
  import in UObjectGlobals.cpp); unvisited watched slots keep their place at the FRONT for the next pass
  (round robin, no starvation). FindFirstOf/FindAllOf and notify arming pass no budget = exact full sync.
- Watch de-dup: a slot is in FreeWatch at most once (InWatch flag), so a slot that is filled and freed
  between two syncs cannot accumulate duplicates over a long session.
- Reviewer blocker fix: a freed (Dirty) slot's ObjAt/ClassAt are cleared immediately, so an object that
  reuses the same address and class is still reported as new; newly freed slots are re-checked first.
- [PERF] idx_cut= (syncs that hit the budget this second).
Applies to the candidate tree; refuses if any anchor is missing (exact-once replacement)."""
import sys

C = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate'


def patch(path, pairs):
    s = open(path).read()
    for old, new in pairs:
        n = s.count(old)
        if n != 1:
            sys.exit('anchor count %d in %s:\n%s' % (n, path, old[:200]))
        s = s.replace(old, new)
    open(path, 'w').write(s)


UOG = [
    ("""    static uint64_t g_r9_watch_fwd{0}; // [R9-PERF] freed/empty slots re-checked per sync (last sync)""",
     """    static uint64_t g_r9_watch_fwd{0}; // [R9-PERF] freed/empty slots re-checked per sync (last sync)
    static uint64_t g_r9_watch_cut_fwd{0}; // [R9-PERF] budgeted syncs that stopped before re-checking every watched slot"""),
    ("""            Vec<int32_t> FreeWatch{};  // empty / unreachable slots, re-checked every sync until filled""",
     """            Vec<int32_t> FreeWatch{};  // empty / unreachable slots, re-checked every sync until filled
            Vec<uint8_t> InWatch{};    // per index: 1 while the index is listed in FreeWatch (at most once)"""),
    ("""            S.ListedUnder.resize(NewSize, nullptr);
        }""",
     """            S.ListedUnder.resize(NewSize, nullptr);
            S.InWatch.resize(NewSize, 0);
        }"""),
    ("""                S.ObjAt[Index] = nullptr;
                S.ClassAt[Index] = nullptr;
                S.FreeWatch.push_back(Index);
                return;""",
     """                S.ObjAt[Index] = nullptr;
                S.ClassAt[Index] = nullptr;
                if (!S.InWatch[Index]) { S.InWatch[Index] = 1; S.FreeWatch.push_back(Index); }
                return;"""),
    ("""        static auto Sync(State& S) -> bool
        {""",
     """        // BudgetExpired == nullptr: exact (every watched slot re-checked). Otherwise the watched-slot
        // re-check stops once it returns true (checked every 256 slots); the rest stays watched, first in
        // line for the next sync. Appended indices are always observed.
        static auto Sync(State& S, bool (*BudgetExpired)() = nullptr) -> bool
        {"""),
    ("""            if (!S.Built)
            {
                S.FreeWatch.clear();""",
     """            if (!S.Built)
            {
                S.FreeWatch.clear();
                S.InWatch.assign(S.InWatch.size(), 0);"""),
    ("""            // re-check watched slots first (they are re-added by Observe if still empty)
            Vec<int32_t> Watch{};
            Watch.swap(S.FreeWatch);
            for (int32_t i : S.Dirty)
            {
                if (i >= 0 && i < Num) { Watch.push_back(i); }
            }
            S.Dirty.clear();
            for (int32_t i : Watch)
            {
                if (i < Num) { Observe(S, i); }
                else { S.FreeWatch.push_back(i); }
            }""",
     """            // re-check watched slots first (they are re-added by Observe if still empty). Newly freed
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
            }"""),
    ("""            if (S.Dirty.size() > 4 * 1024 * 1024) { S.Built = false; S.Dirty.clear(); S.FreeWatch.clear(); return; } // rebuild""",
     """            if (S.Dirty.size() > 4 * 1024 * 1024) { S.Built = false; S.Dirty.clear(); S.FreeWatch.clear(); return; } // rebuild (InWatch reset by the build)"""),
    ("""    auto r9_index_collect_new(std::vector<int32_t>& Out, std::vector<void*>& OutObj) -> bool
    {""",
     """    // BudgetExpired: the per-frame time budget for re-checking freed slots (see r9idx::Sync).
    auto r9_index_collect_new(std::vector<int32_t>& Out, std::vector<void*>& OutObj, bool (*BudgetExpired)()) -> bool
    {"""),
    ("""            if (!r9idx::Sync(S)) { return false; }
            if (S.DeletesSeen == 0) { S.NewIdx.clear(); S.NewObj.clear(); return false; } // delete listener not proven live yet""",
     """            if (!r9idx::Sync(S, BudgetExpired)) { return false; }
            if (S.DeletesSeen == 0) { S.NewIdx.clear(); S.NewObj.clear(); return false; } // delete listener not proven live yet"""),
    ("""    auto r9_take_index_stats(uint64_t& del_calls, uint64_t& del_ticks, uint64_t& sync_calls, uint64_t& sync_ticks, uint64_t& watch) -> void
    {""",
     """    auto r9_take_index_stats(uint64_t& del_calls, uint64_t& del_ticks, uint64_t& sync_calls, uint64_t& sync_ticks, uint64_t& watch, uint64_t& cuts) -> void
    {
        cuts = __atomic_exchange_n(&g_r9_watch_cut_fwd, uint64_t{0}, __ATOMIC_RELAXED);"""),
]

LUAMOD = [
    ("""auto r9_take_index_stats(uint64_t& del_calls, uint64_t& del_ticks, uint64_t& sync_calls, uint64_t& sync_ticks, uint64_t& watch) -> void;""",
     """auto r9_take_index_stats(uint64_t& del_calls, uint64_t& del_ticks, uint64_t& sync_calls, uint64_t& sync_ticks, uint64_t& watch, uint64_t& cuts) -> void;"""),
    ("""uint64_t del_calls{0}, del_ticks{0}, sync_calls{0}, sync_ticks{0}, idx_watch{0};
                Unreal::UObjectGlobals::r9_take_index_stats(del_calls, del_ticks, sync_calls, sync_ticks, idx_watch);""",
     """uint64_t del_calls{0}, del_ticks{0}, sync_calls{0}, sync_ticks{0}, idx_watch{0}, idx_cut{0};
                Unreal::UObjectGlobals::r9_take_index_stats(del_calls, del_ticks, sync_calls, sync_ticks, idx_watch, idx_cut);"""),
    ("""idx_syncs={} idx_ms={:.1f} idx_watch={} | prop calls""", """idx_syncs={} idx_ms={:.1f} idx_watch={} idx_cut={} | prop calls"""),
    ("""sync_calls, tk(sync_ticks), idx_watch,\n""", """sync_calls, tk(sync_ticks), idx_watch, idx_cut,\n"""),
    ("""auto r9_index_collect_new(std::vector<int32_t>& Out, std::vector<void*>& OutObj) -> bool; // [R9-NOTIFY] UObjectGlobals.cpp""",
     """auto r9_index_collect_new(std::vector<int32_t>& Out, std::vector<void*>& OutObj, bool (*BudgetExpired)()) -> bool; // [R9-NOTIFY] UObjectGlobals.cpp"""),
    ("""            if (!Unreal::UObjectGlobals::r9_index_collect_new(s_backlog_idx, s_backlog_obj))""",
     """            // [R9-IDX] per-frame budget (~0.5 ms) for re-checking freed slots: a huge unload can never cost
            // more than that per frame; FindFirstOf/FindAllOf still sync exactly
            static std::chrono::steady_clock::time_point s_watch_budget_end{};
            s_watch_budget_end = std::chrono::steady_clock::now() + std::chrono::microseconds(500);
            if (!Unreal::UObjectGlobals::r9_index_collect_new(s_backlog_idx, s_backlog_obj,
                                                              +[]() -> bool { return std::chrono::steady_clock::now() > s_watch_budget_end; }))"""),
]

if __name__ == '__main__':
    if len(sys.argv) > 1: C = sys.argv[1]
    patch(C + '/src/UObjectGlobals.cpp', UOG)
    patch(C + '/src/LuaMod.cpp', LUAMOD)
    print('B6p applied: UObjectGlobals.cpp %d edits, LuaMod.cpp %d edits' % (len(UOG), len(LUAMOD)))
