// cap #4 host simulation: the PATCHED world_ev + r9_feed_notify_from_index code (extracted verbatim by
// extract.py) driven through startup scenarios. Mac-only; no engine, no console. One scenario per
// process run (the code under test keeps function-local statics). Exit 0 = scenario PASS.
#include "stubs.hpp"

namespace RC
{
#include "world_ev_extract.inc"
}

using namespace RC;
using namespace RC::Unreal;

namespace sim
{
    enum Ref : int32_t { kLoadMapPre = 1, kLoadMapPost = 2, kIgsPre = 3, kIgsPost = 4, kBpPre = 5, kBpPost = 6 };
    static LuaMadeSimple::Lua g_lua{};
    static int g_fail = 0;
    static int g_checks = 0;

    static auto check(bool ok, const std::string& what) -> void
    {
        ++g_checks;
        if (!ok) { ++g_fail; std::printf("  CHECK FAILED: %s\n", what.c_str()); }
    }

    static UClass* mk_class(const char* name, UStruct* super, UClass* meta)
    {
        auto* c = new UClass{};
        c->name = name;
        c->super = super;
        c->cls = meta;
        return c;
    }

    struct Classes
    {
        UClass* meta;
        UClass* object;
        UClass* actor;
        UClass* gm;
        UClass* gs;
        UClass* world;
        UClass* level;
        UClass* my_gm;
        UClass* my_gs;
        UClass* pawn;
    };
    static Classes C{};

    static auto setup_classes() -> void
    {
        C.meta = mk_class("Class", nullptr, nullptr);
        C.meta->cls = C.meta;
        C.object = mk_class("Object", nullptr, C.meta);
        C.actor = mk_class("Actor", C.object, C.meta);
        C.gm = mk_class("GameModeBase", C.actor, C.meta);
        C.gs = mk_class("GameStateBase", C.actor, C.meta);
        C.world = mk_class("World", C.object, C.meta);
        C.level = mk_class("Level", C.object, C.meta);
        C.my_gm = mk_class("BP_GameMode_C", C.gm, C.meta);
        C.my_gs = mk_class("BP_GameState_C", C.gs, C.meta);
        C.pawn = mk_class("BP_Pawn_C", C.actor, C.meta);
        UObjectGlobals::g_paths["/Script/Engine.Actor"] = C.actor;
        UObjectGlobals::g_paths["/Script/Engine.GameModeBase"] = C.gm;
        UObjectGlobals::g_paths["/Script/Engine.GameStateBase"] = C.gs;
        UObjectGlobals::g_paths["/Script/Engine.World"] = C.world;
        // the startup block (patched on_program_start) on PS5
        refused_log::loadmap_state.store(3);
        refused_log::eager_detour_block_skipped.store(true);
    }

    static int32_t g_next_slot = 10;
    static auto place(UObject* o, bool synced) -> int32_t
    {
        const int32_t i = g_next_slot++;
        g_items[static_cast<size_t>(i)].obj = o;
        g_items[static_cast<size_t>(i)].unreachable = false;
        UObjectGlobals::g_synced[static_cast<size_t>(i)] = synced ? 1 : 0;
        return i;
    }
    static auto obj(UClass* cls, UObject* outer, const char* name, int32_t flags = 0) -> UObject*
    {
        auto* o = new UObject{};
        o->cls = cls;
        o->outer = outer;
        o->name = name;
        o->flags = flags;
        return o;
    }

    struct Map
    {
        UObject* world;
        UObject* level;
        UObject* gs;
        UObject* gm;
        std::vector<UObject*> actors;
        std::vector<int32_t> slots;
    };
    // baseline=true: alive before the index is first used (never reported as new); else a new batch.
    static auto make_map(const char* wname, int loaded_actors, int spawned_actors, bool with_gs, bool baseline) -> Map
    {
        Map m{};
        m.world = obj(C.world, nullptr, wname);
        m.level = obj(C.level, m.world, "PersistentLevel");
        place(m.world, baseline);
        place(m.level, baseline);
        auto add = [&](UObject* o) {
            const int32_t s = place(o, baseline);
            m.slots.push_back(s);
            if (!baseline) { UObjectGlobals::g_new_idx.push_back(s); UObjectGlobals::g_synced[static_cast<size_t>(s)] = 1; }
        };
        for (int i = 0; i < loaded_actors; ++i)
        {
            auto* a = obj(C.pawn, m.level, "LoadedActor", RF_WasLoaded);
            m.actors.push_back(a);
            add(a);
        }
        if (with_gs)
        {
            m.gm = obj(C.my_gm, m.level, "GameMode");
            m.gs = obj(C.my_gs, m.level, "GameState");
            add(m.gm);
            add(m.gs);
        }
        for (int i = 0; i < spawned_actors; ++i)
        {
            auto* a = obj(C.pawn, m.level, "SpawnedActor");
            m.actors.push_back(a);
            add(a);
        }
        // CDO of the pawn class: must never be delivered
        place(obj(C.pawn, nullptr, "Default__BP_Pawn_C", RF_ClassDefaultObject), baseline);
        return m;
    }

    static auto reg(std::vector<LuaMod::LuaCallbackData>& v, int32_t ref) -> void
    {
        std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};
        v.push_back(LuaMod::LuaCallbackData{&g_lua, nullptr, {{&g_lua, LuaMod::LuaCallbackData::RegistryIndex{ref}}}});
    }
    static auto reg_all() -> void
    {
        reg(LuaMod::m_load_map_pre_callbacks, kLoadMapPre);
        reg(LuaMod::m_load_map_post_callbacks, kLoadMapPost);
        reg(LuaMod::m_init_game_state_pre_callbacks, kIgsPre);
        reg(LuaMod::m_init_game_state_post_callbacks, kIgsPost);
        reg(LuaMod::m_begin_play_pre_callbacks, kBpPre);
        reg(LuaMod::m_begin_play_post_callbacks, kBpPost);
    }

    // the start gate: every mod's main script returned (UE4SSProgram -> early4_mods_started)
    static auto mods_started() -> void { world_ev::s_mods_started.store(true); }

    // one game-thread pump, with fresh pacer windows (one frame)
    static auto pump() -> void
    {
        s_r9_pace_watch.spent = {};
        s_r9_pace_match.spent = {};
        world_ev::s_pace.spent = {};
        r9_feed_notify_from_index();
        world_ev::deliver();
    }
    static auto run_ms(int64_t ms) -> void
    {
        for (int64_t t = 0; t < ms; t += 100)
        {
            g_now_ms += 100;
            pump();
        }
    }

    // ---------------------------------------------------------------- analysis helpers
    static auto calls_of(int32_t ref, void* o) -> int
    {
        int n = 0;
        for (const auto& c : g_calls)
        {
            if (c.ref == ref && !c.objs.empty() && c.objs[0] == o) { ++n; }
        }
        return n;
    }
    static auto map_calls(int32_t ref, void* world) -> int // LoadMap args: engine, world, URL(nil), pending
    {
        int n = 0;
        for (const auto& c : g_calls)
        {
            if (c.ref == ref && (world == nullptr || (c.objs.size() > 1 && c.objs[1] == world))) { ++n; }
        }
        return n;
    }
    static auto first_index(int32_t ref, void* o, bool map_arg = false) -> int
    {
        for (size_t i = 0; i < g_calls.size(); ++i)
        {
            const auto& c = g_calls[i];
            if (c.ref != ref) { continue; }
            if (o == nullptr) { return static_cast<int>(i); }
            const size_t k = map_arg ? 1 : 0;
            if (c.objs.size() > k && c.objs[k] == o) { return static_cast<int>(i); }
        }
        return -1;
    }
    static auto last_index_any(const std::vector<UObject*>& objs) -> int
    {
        int last = -1;
        for (size_t i = 0; i < g_calls.size(); ++i)
        {
            for (UObject* o : objs)
            {
                if (!g_calls[i].objs.empty() && g_calls[i].objs[0] == o) { last = static_cast<int>(i); }
            }
        }
        return last;
    }
    static auto has_line(const std::string& needle) -> bool
    {
        for (const auto& l : Output::g_lines)
        {
            if (l.find(needle) != std::string::npos) { return true; }
        }
        return false;
    }
    static auto line_index(const std::string& needle) -> int
    {
        for (size_t i = 0; i < Output::g_lines.size(); ++i)
        {
            if (Output::g_lines[i].find(needle) != std::string::npos) { return static_cast<int>(i); }
        }
        return -1;
    }
    static auto dump() -> void
    {
        for (const auto& l : Output::g_lines) { std::printf("    log| %s\n", l.c_str()); }
        std::printf("    calls:");
        for (const auto& c : g_calls) { std::printf(" %d", c.ref); }
        std::printf("\n");
    }

    // ================================================================ scenarios
    // S1: the startup map is fully loaded when mods start (GameState in the baseline) -> replayed in
    //     PC order, before a later live map; each actor exactly once; the CDO never.
    static auto s1() -> void
    {
        Map a = make_map("MapA", 3, 2, true, true);
        reg_all();
        mods_started();
        UObjectGlobals::g_index_usable = true;
        pump();
        check(has_line("[EARLY] stage=startup-hooks"), "startup-hooks line");
        check(has_line("[EARLY] stage=world-route-live") && has_line("[EARLY] stage=replay-request") && has_line("replay=requested"), "route live + replay requested");
        check(has_line("decision=replay") && has_line("gamestates=1"), "startup-replay decision=replay gamestates=1");
        check(has_line("[EARLY] stage=first-map-seen") && has_line("src=replay world='MapA'"), "first-map-seen src=replay MapA");
        check(map_calls(kLoadMapPre, nullptr) == 1, "LoadMapPre delivered once right away");
        run_ms(3500); // quiet window closes
        check(map_calls(kLoadMapPost, a.world) == 1, "LoadMapPost(MapA) with the World argument");
        check(calls_of(kIgsPre, a.gm) == 1 && calls_of(kIgsPost, a.gm) == 1, "InitGameState pre/post for MapA's GameMode once");
        for (UObject* x : a.actors) { check(calls_of(kBpPre, x) == 1 && calls_of(kBpPost, x) == 1, "BeginPlay pre/post once per MapA actor"); }
        check(calls_of(kBpPre, a.gs) == 1, "BeginPlay for the GameState actor once");
        int cdo_calls = 0;
        for (const auto& c : g_calls) { if (!c.objs.empty() && c.objs[0] && static_cast<UObject*>(c.objs[0])->HasAnyFlags(RF_ClassDefaultObject)) { ++cdo_calls; } }
        check(cdo_calls == 0, "no CDO delivered");
        // PC order inside the map: LoadMapPre < InitGameState < BeginPlay < LoadMapPost
        const int i_pre = first_index(kLoadMapPre, nullptr);
        const int i_igs = first_index(kIgsPre, a.gm);
        const int i_bp = first_index(kBpPre, a.actors[0]);
        const int i_post = first_index(kLoadMapPost, a.world, true);
        check(i_pre >= 0 && i_pre < i_igs && i_igs < i_bp && i_bp < i_post, "MapA order pre<IGS<BP<post");
        // a later live map B
        Map b = make_map("MapB", 2, 0, true, false);
        pump();
        run_ms(3500);
        std::vector<int> pres{};
        for (size_t i = 0; i < g_calls.size(); ++i) { if (g_calls[i].ref == kLoadMapPre) { pres.push_back(static_cast<int>(i)); } }
        const int i_b_pre = pres.size() == 2 ? pres[1] : -1;
        check(i_b_pre > i_post, "MapB LoadMapPre after MapA LoadMapPost");
        check(map_calls(kLoadMapPost, b.world) == 1, "MapB LoadMapPost once");
        for (UObject* x : b.actors) { check(calls_of(kBpPre, x) == 1, "BeginPlay once per MapB actor"); }
        check(last_index_any(a.actors) < first_index(kBpPre, b.actors[0]), "every MapA actor event before MapB's");
        check(has_line("[EARLY] stage=first-delivery kind=LoadMapPre"), "first-delivery marker");
    }

    // S2: mods start while the map is still loading: loaded actors are in the baseline, the GameState
    //     arrives live a moment later -> the baseline actors join that map's window (not lost).
    static auto s2() -> void
    {
        reg_all();
        mods_started();
        Map m{};
        m.world = obj(C.world, nullptr, "MapL");
        m.level = obj(C.level, m.world, "PersistentLevel");
        place(m.world, true);
        place(m.level, true);
        for (int i = 0; i < 4; ++i)
        {
            auto* x = obj(C.pawn, m.level, "LoadedActor", RF_WasLoaded);
            m.actors.push_back(x);
            place(x, true);
        }
        UObjectGlobals::g_index_usable = true;
        pump();
        check(has_line("decision=replay") && has_line("gamestates=0"), "replay with no GameState yet");
        check(map_calls(kLoadMapPre, nullptr) == 0, "no map event before the GameState");
        g_now_ms += 1000; // the load finishes: GameMode + GameState spawn (new objects)
        m.gm = obj(C.my_gm, m.level, "GameMode");
        m.gs = obj(C.my_gs, m.level, "GameState");
        for (UObject* x : {m.gm, m.gs})
        {
            const int32_t s = place(x, false);
            UObjectGlobals::g_new_idx.push_back(s);
            UObjectGlobals::g_synced[static_cast<size_t>(s)] = 1;
        }
        pump();
        check(has_line("src=live world='MapL'"), "first map seen live");
        check(has_line("held_joined=6"), "the 4 baseline actors (BeginPlay) + the GameMode (InitGameState + BeginPlay) joined the live map window");
        run_ms(3500);
        for (UObject* x : m.actors) { check(calls_of(kBpPre, x) == 1, "BeginPlay once for each baseline actor of the loading map"); }
        check(calls_of(kIgsPre, m.gm) == 1, "InitGameState once");
        check(map_calls(kLoadMapPost, m.world) == 1, "LoadMapPost once");
    }

    // S3: two live GameStates in the baseline (map change in progress) -> ambiguous -> no replay.
    static auto s3() -> void
    {
        Map a = make_map("MapA", 2, 0, true, true);
        Map b = make_map("MapB", 2, 0, true, true);
        reg_all();
        mods_started();
        UObjectGlobals::g_index_usable = true;
        pump();
        run_ms(6000);
        check(has_line("decision=skip-ambiguous") && has_line("gamestates=2"), "skip-ambiguous gamestates=2 logged");
        check(line_index("W [EARLY] stage=startup-replay") >= 0, "ambiguous decision is a Warning");
        check(g_calls.empty(), "nothing delivered for an ambiguous baseline");
        Map c = make_map("MapC", 1, 0, true, false);
        pump();
        run_ms(3500);
        check(map_calls(kLoadMapPost, c.world) == 1 && calls_of(kBpPre, c.actors[0]) == 1, "the next live map is delivered normally");
        (void)a; (void)b;
    }

    // S4: no world hook at the first pump; a mod registers later -> no replay (PC: a late registration
    //     does not see maps that already loaded).
    static auto s4() -> void
    {
        Map a = make_map("MapA", 2, 0, true, true);
        LuaMod::m_static_construct_object_lua_callbacks.push_back(LuaMod::StaticConstructCb{FName{"X"}, FName{"Y"}, 1}); // keeps the feed alive
        mods_started();
        UObjectGlobals::g_index_usable = true;
        pump();
        pump();
        check(has_line("[EARLY] stage=startup-hooks") && has_line("loadmap_pre=0"), "startup-hooks line with no world hooks");
        reg_all();
        g_now_ms += 5000;
        pump();
        run_ms(4000);
        check(has_line("replay=not-startup"), "route live marked not-startup");
        check(g_calls.empty(), "no replay to a late registration");
        Map b = make_map("MapB", 1, 0, true, false);
        pump();
        run_ms(3500);
        check(map_calls(kLoadMapPost, b.world) == 1, "later live map delivered to the late registration");
        (void)a;
    }

    // S5: the route goes live in the same collect that reports new objects -> those new objects are
    //     observed live only (excluded from the replay): each actor exactly once.
    static auto s5() -> void
    {
        Map a = make_map("MapA", 2, 0, true, true);
        reg_all();
        mods_started();
        auto* late = obj(C.pawn, a.level, "SpawnedJustNow");
        const int32_t s = place(late, true); // synced by the collect's Sync AND reported as new
        UObjectGlobals::g_new_idx.push_back(s);
        UObjectGlobals::g_index_usable = true;
        pump();
        check(has_line("excluded_new=1"), "the new batch object is excluded from the replay");
        run_ms(4000);
        check(calls_of(kBpPre, late) == 1 && calls_of(kBpPost, late) == 1, "the new object gets BeginPlay exactly once");
        for (UObject* x : a.actors) { check(calls_of(kBpPre, x) == 1, "baseline actor once"); }
    }

    // S6: replay cut by the budget resumes where it stopped; an object freed in between is skipped.
    static auto s6() -> void
    {
        Map a = make_map("MapA", 300, 0, true, true);
        reg_all();
        mods_started();
        {
            std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};
            world_ev::s_wanted = world_ev::wanted_locked();
            world_ev::note_feed_locked();
        }
        UObjectGlobals::g_index_usable = true;
        world_ev::note_index(true); // route goes live (classes resolve on the first try)
        world_ev::maybe_start_replay(std::vector<int32_t>{}, 0);
        check(world_ev::replay_running(), "replay running after start");
        const size_t total = world_ev::s_rp_idx.size();
        {
            std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};
            const bool done = world_ev::replay_step(std::chrono::steady_clock::now() - std::chrono::seconds(1));
            check(!done && world_ev::s_rp_pos == 64, "an expired budget cuts the replay after 64 entries");
        }
        UObject* victim = a.actors[150];
        g_items[static_cast<size_t>(a.slots[150])].unreachable = true; // freed between two steps
        {
            std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};
            const bool done = world_ev::replay_step(std::chrono::steady_clock::now() + std::chrono::seconds(10));
            check(done && !world_ev::replay_running(), "the replay finishes on the next step");
        }
        check(world_ev::s_rp_fed == total - 1, "fed = snapshot - 1 freed object");
        check(has_line("[EARLY] stage=startup-replay-done"), "replay-done line");
        run_ms(4000);
        check(calls_of(kBpPre, victim) == 0, "the freed object is never delivered");
        check(calls_of(kBpPre, a.actors[0]) == 1 && calls_of(kBpPre, a.actors[299]) == 1, "first and last actor delivered once");
    }

    // S7: launch-12 shape: world hooks at startup, but the index is not usable for a while -> the replay
    //     still happens (startup registrations) once the route goes live.
    static auto s7() -> void
    {
        Map a = make_map("MapA", 2, 0, true, true);
        reg_all();
        mods_started();
        UObjectGlobals::g_index_usable = false;
        for (int i = 0; i < 20; ++i) { g_now_ms += 1000; pump(); }
        check(!has_line("[EARLY] stage=world-route-live"), "no route while the index is unusable");
        UObjectGlobals::g_index_usable = true;
        pump();
        run_ms(3500);
        check(has_line("replay=requested") && has_line("decision=replay"), "replay once the index is usable");
        check(map_calls(kLoadMapPost, a.world) == 1, "startup map delivered after the wait");
    }

    // S8: only a LoadMapPre hook at startup (launch-14 shape: one mod) -> replay delivers LoadMapPre for
    //     the current map; nothing is queued for BeginPlay (no BeginPlay hooks registered).
    static auto s8() -> void
    {
        Map a = make_map("MapA", 3, 0, true, true);
        reg(LuaMod::m_load_map_pre_callbacks, kLoadMapPre);
        mods_started();
        UObjectGlobals::g_index_usable = true;
        pump();
        run_ms(3500);
        check(map_calls(kLoadMapPre, nullptr) == 1, "LoadMapPre once for the current map");
        check(g_calls.size() == 1, "nothing else delivered");
        check(has_line("[WORLD-EV] map load window closed") && has_line("actors=0"), "window closed with 0 BeginPlay events queued");
        (void)a;
    }

    // S9: the first pump fires while mods are still starting (no world hook yet); a mod registers its
    //     hooks afterwards in its main script; the start gate passes -> still a startup registration.
    static auto s9() -> void
    {
        Map a = make_map("MapA", 2, 0, true, true);
        LuaMod::m_static_construct_object_lua_callbacks.push_back(LuaMod::StaticConstructCb{FName{"X"}, FName{"Y"}, 1});
        UObjectGlobals::g_index_usable = true;
        pump(); // mods still starting
        check(!has_line("[EARLY] stage=startup-hooks"), "startup window still open before the start gate");
        reg_all();
        pump(); // hooks now, but startup not closed: the route must NOT go live yet (review finding 1)
        check(!has_line("[EARLY] stage=world-route-live") && !has_line("[EARLY] stage=replay-request"), "route not live and no replay decision before the start gate");
        mods_started();
        pump();
        run_ms(3500);
        check(has_line("replay=requested") && has_line("decision=replay"), "replay once the start gate passed");
        check(map_calls(kLoadMapPost, a.world) == 1 && calls_of(kBpPre, a.actors[0]) == 1, "startup map delivered");
    }

    // S10: the start gate passes with no world hook; a hook registered 1 s later (from a delayed
    //      callback) is NOT a startup registration -> no replay.
    static auto s10() -> void
    {
        Map a = make_map("MapA", 2, 0, true, true);
        LuaMod::m_static_construct_object_lua_callbacks.push_back(LuaMod::StaticConstructCb{FName{"X"}, FName{"Y"}, 1});
        mods_started();
        UObjectGlobals::g_index_usable = true;
        pump();
        check(has_line("[EARLY] stage=startup-hooks"), "startup closed at the first feed after the gate");
        g_now_ms += 1000;
        reg_all();
        pump();
        run_ms(3500);
        check(has_line("replay=not-startup") && g_calls.empty(), "late registration: no replay");
        (void)a;
    }

    // S11 (review finding 1): a carrier pump runs while mods are still starting, a mod has already
    //      registered its world hooks and the index is usable, and a new map's objects arrive before the
    //      start gate. Nothing may be delivered live before the gate; after it, the startup replay
    //      delivers that map exactly once (before the fix: live + replay = the whole map twice).
    static auto s11() -> void
    {
        reg_all();
        UObjectGlobals::g_index_usable = true;
        pump(); // mods still starting
        Map b = make_map("MapB", 2, 0, true, false); // new objects, collected before the start gate
        pump();
        run_ms(500);
        check(!has_line("[EARLY] stage=world-route-live"), "route not live before the start gate");
        check(g_calls.empty(), "nothing delivered live before the start gate");
        mods_started();
        pump();
        run_ms(3500);
        check(has_line("replay=requested") && has_line("decision=replay") && has_line("src=replay world='MapB'"), "MapB delivered by the startup replay");
        check(map_calls(kLoadMapPre, nullptr) == 1 && map_calls(kLoadMapPost, b.world) == 1, "LoadMap pre/post exactly once");
        check(calls_of(kIgsPre, b.gm) == 1 && calls_of(kIgsPost, b.gm) == 1, "InitGameState once");
        for (UObject* x : b.actors) { check(calls_of(kBpPre, x) == 1 && calls_of(kBpPost, x) == 1, "BeginPlay once per MapB actor"); }
        check(calls_of(kBpPre, b.gs) == 1, "GameState BeginPlay once");
        check(g_calls.size() == 12, "exactly one map sequence (12 Lua calls)");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::printf("usage: sim <s1..s11>\n"); return 2; }
    sim::setup_classes();
    const std::string s = argv[1];
    if (s == "s1") { sim::s1(); }
    else if (s == "s2") { sim::s2(); }
    else if (s == "s3") { sim::s3(); }
    else if (s == "s4") { sim::s4(); }
    else if (s == "s5") { sim::s5(); }
    else if (s == "s6") { sim::s6(); }
    else if (s == "s7") { sim::s7(); }
    else if (s == "s8") { sim::s8(); }
    else if (s == "s9") { sim::s9(); }
    else if (s == "s10") { sim::s10(); }
    else if (s == "s11") { sim::s11(); }
    else { std::printf("unknown scenario %s\n", s.c_str()); return 2; }
    if (sim::g_fail || sim::g_checks == 0) { sim::dump(); }
    std::printf("%s %s: %d checks, %d failed\n", (sim::g_fail == 0 && sim::g_checks > 0) ? "PASS" : "FAIL", s.c_str(), sim::g_checks, sim::g_fail);
    return (sim::g_fail == 0 && sim::g_checks > 0) ? 0 : 1;
}
