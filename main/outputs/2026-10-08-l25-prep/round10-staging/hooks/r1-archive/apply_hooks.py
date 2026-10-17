#!/usr/bin/env python3
"""apply_hooks.py - round10 HOOKS track: data-only delivery of the Lua LoadMap / InitGameState /
BeginPlay hooks ([WORLD-EV]), EndPlay stays refused with an explicit reason.

  python3 apply_hooks.py [ROOT]      ROOT = tree holding src/LuaMod.cpp
                                     (default: the real candidate path)

Every replacement is exact-once: if an anchor is found 0 or >1 times the script exits non-zero
naming the anchor and writes NOTHING. Touches one file: src/LuaMod.cpp."""
import sys
from pathlib import Path

DEFAULT_ROOT = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
HERE = Path(__file__).resolve().parent

root = Path(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ROOT)
target = root / 'src' / 'LuaMod.cpp'
if not target.is_file():
    sys.exit('apply_hooks: missing %s' % target)
src = target.read_text()
if '[WORLD-EV]' in src:
    sys.exit('apply_hooks: %s already contains [WORLD-EV] (already applied?)' % target)

# The [WORLD-EV] block (also kept as world_ev_block.inc next to this script, for review).
WORLD_EV_BLOCK = r"""
    // =====================================================================================
    // [WORLD-EV] (round10 HOOKS track; generic for every UE game, no game/mod names, no new
    // offsets) Data-only delivery of the Lua map-load / InitGameState / BeginPlay hooks
    // (RegisterLoadMapPre/PostHook, RegisterInitGameStatePre/PostHook,
    // RegisterBeginPlayPre/PostHook). On PC these are code detours; on PS5 game code is
    // execute-only, so the detour block is skipped and the registrations used to be accepted
    // but never delivered. Route: the same delete-listener-fed object index that already
    // serves NotifyOnNewObject ([R9-NOTIFY]) reports objects that became live since the last
    // pump. Each new object's class chain is compared (pointer equality) with four engine
    // classes resolved once by path (/Script/Engine.Actor, .GameModeBase, .GameStateBase,
    // .World). A new GameStateBase in the persistent level means "a map finished loading"
    // (UEngine::LoadMap spawns exactly one per load, after the GameMode, inside the load);
    // a new GameModeBase means "InitGameState ran"; a new Actor means "BeginPlay ran".
    // Delivery is on the game thread, from the existing pump (process_pending_notify_on_new_
    // object_callbacks, called by every drain), time-paced (~1 ms per ~16 ms window,
    // [R9-PACE]) and contained per callback (one mod's Lua error never stops the others).
    // Honest timing differences vs PC (also in the one-line registration note):
    //   * everything is delivered AFTER the event, at the first pump that sees the new
    //     objects (pump runs on game event carriers; nothing runs while the engine is inside
    //     LoadMap). "Pre" hooks therefore also run after the event.
    //   * per load, order is kept: LoadMap pre -> InitGameState pre/post -> BeginPlay pre/post
    //     (each new actor of that batch) -> LoadMap post; events wait until the whole batch of
    //     new objects is matched (a big load is matched over several frames).
    //   * LoadMap args: Engine = invalid object (not looked up), World = invalid for pre (the
    //     old world is already gone) / the new world for post, URL = nil, PendingGame =
    //     invalid object, Error = "". Return values are ignored (the load already happened).
    //   * BeginPlay: actors loaded from a package (RF_WasLoaded) are reported only when they
    //     arrive with a map load; actors of a level streamed in later are NOT reported (their
    //     BeginPlay waits for the level to become visible, which this route cannot see).
    //     Actors created and destroyed between two pumps are missed.
    //   * an object that dies before delivery is skipped (never handed to Lua).
    // EndPlay stays REFUSED (counted): by the time the index sees an actor gone it is already
    // freed, so there is no live actor to hand to Lua, and the Blueprint ReceiveEndPlay
    // Func-swap route would consume script-thunk slots the pump carriers need.
    // FAIL CLOSED: a missing engine class or an object index that never becomes usable
    // (60 s) turns the route off with one [WORLD-EV] line + one counted [REFUSED] line.
    // =====================================================================================
    namespace world_ev
    {
        enum class Kind : uint8_t { LoadMapPre, InitGameState, BeginPlay, LoadMapPost };
        struct Ev
        {
            Kind kind;
            int32_t index;
            Unreal::UObject* obj;
        };
        static Unreal::UStruct* s_actor{nullptr};
        static Unreal::UStruct* s_game_mode{nullptr};
        static Unreal::UStruct* s_game_state{nullptr};
        static Unreal::UStruct* s_world{nullptr};
        static bool s_wanted{false}; // set by the feed under m_thread_actions_mutex (game thread)
        static bool s_classes_ok{false};
        static bool s_index_ok{false};
        static int s_resolve_tries{0};
        static int64_t s_first_wanted_ms{0};
        static int64_t s_last_resolve_ms{0};
        // batch under construction (one r9 backlog)
        static std::vector<Ev> s_b_map{}, s_b_gm{}, s_b_actor{}, s_b_loaded{};
        // ordered delivery queue
        static std::vector<Ev> s_queue{};
        static size_t s_queue_pos{0};
        constexpr size_t kMaxQueue = size_t{1} << 16; // ~1 MB of 16-byte events at most (libc heap is small)
        static uint64_t s_maps{0}, s_gms{0}, s_actors{0}, s_delivered{0}, s_skipped_dead{0}, s_dropped{0}, s_loaded_unreported{0};
        static uint32_t s_errors{0};
        static R9Pacer s_pace{};
        static bool s_first_kind[4]{true, true, true, true};

        static auto any_world_callbacks() -> bool // caller holds m_thread_actions_mutex
        {
            return !LuaMod::m_load_map_pre_callbacks.empty() || !LuaMod::m_load_map_post_callbacks.empty() ||
                   !LuaMod::m_init_game_state_pre_callbacks.empty() || !LuaMod::m_init_game_state_post_callbacks.empty() ||
                   !LuaMod::m_begin_play_pre_callbacks.empty() || !LuaMod::m_begin_play_post_callbacks.empty();
        }

        // Caller holds m_thread_actions_mutex. True only in the world where the detour block was
        // skipped (PS5 R3 marshal) and the route is not refused.
        static auto wanted_locked() -> bool
        {
            return refused_log::eager_detour_block_skipped.load(std::memory_order_acquire) &&
                   refused_log::world_route_state.load(std::memory_order_acquire) != 2 && any_world_callbacks();
        }

        static auto release(std::vector<Ev>& v) -> void // drop a big buffer instead of keeping its capacity
        {
            if (v.capacity() > 8192) { std::vector<Ev>{}.swap(v); } else { v.clear(); }
        }

        static auto reset_batch() -> void // game thread (feed / refuse)
        {
            release(s_b_map); release(s_b_gm); release(s_b_actor); release(s_b_loaded);
        }

        static auto refuse(const char* reason) -> void
        {
            if (refused_log::world_route_state.exchange(2, std::memory_order_acq_rel) == 2) { return; }
            try
            {
                StringType wide{};
                for (const char* p = reason; *p != '\0'; ++p) { wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                Output::send<LogLevel::Error>(STR("[WORLD-EV] route refused: reason={} (LoadMap/InitGameState/BeginPlay Lua hooks stay undelivered; everything else unaffected)\n"), wide);
            }
            catch (...)
            {
            }
            refused_log::emit("RegisterLoadMap/InitGameState/BeginPlayHooks", StringType{STR("object-index-world-events")}, StringType{STR("*")}, reason, "never-delivered");
            reset_batch();
            release(s_queue); s_queue_pos = 0;
        }

        // Game thread, from the feed, when the route is wanted. Resolves the four engine classes
        // (once; retried every 2 s) and applies the 60 s fail-closed rule.
        static auto ensure_ready() -> bool
        {
            const int state = refused_log::world_route_state.load(std::memory_order_acquire);
            if (state == 1) { return true; } // live: nothing to do (called on every pump)
            if (state == 2) { return false; }
            const int64_t now = r3_now_ms();
            if (s_first_wanted_ms == 0) { s_first_wanted_ms = now; }
            if (!s_classes_ok && (s_last_resolve_ms == 0 || now - s_last_resolve_ms >= 2000))
            {
                s_last_resolve_ms = now;
                ++s_resolve_tries;
                try
                {
                    s_actor = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.Actor")});
                    s_game_mode = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.GameModeBase")});
                    s_game_state = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.GameStateBase")});
                    s_world = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.World")});
                }
                catch (...)
                {
                    s_actor = s_game_mode = s_game_state = s_world = nullptr;
                }
                s_classes_ok = s_actor && s_game_mode && s_game_state && s_world;
            }
            if (s_classes_ok && s_index_ok)
            {
                if (refused_log::world_route_state.exchange(1, std::memory_order_acq_rel) == 0)
                {
                    try { Output::send(STR("[WORLD-EV] route live: object index + engine classes Actor/GameModeBase/GameStateBase/World resolved (tries={}); LoadMap/InitGameState/BeginPlay Lua hooks deliver after the event on the game-thread pump\n"), s_resolve_tries); } catch (...) {}
                }
                return true;
            }
            if (now - s_first_wanted_ms >= 60000)
            {
                refuse(!s_classes_ok ? "engine-class-lookup-failed" : "object-index-unusable-60s");
            }
            return false;
        }

        // After each collect (outside the lock). Going live here, before the batch is matched,
        // means the very first batch after the index becomes usable is already observed.
        static auto note_index(bool usable) -> void
        {
            if (!usable) { return; }
            s_index_ok = true;
            if (s_wanted && refused_log::world_route_state.load(std::memory_order_acquire) == 0) { (void)ensure_ready(); }
        }

        // Caller holds m_thread_actions_mutex; object is live and pointer-checked by the feed.
        static auto observe(Unreal::UObject* object, int32_t object_index) -> void
        {
            if (refused_log::world_route_state.load(std::memory_order_relaxed) != 1) { return; }
            bool is_actor{false}, is_gm{false}, is_gs{false};
            int depth = 0;
            for (Unreal::UStruct* c = object->GetClassPrivate(); c && depth < 64; c = c->GetSuperStruct(), ++depth)
            {
                if (c == s_actor) { is_actor = true; break; } // Actor is the last class of interest up the chain
                if (c == s_game_state) { is_gs = true; }
                else if (c == s_game_mode) { is_gm = true; }
            }
            if (!is_actor) { return; }
            if (object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_ClassDefaultObject | Unreal::RF_ArchetypeObject))) { return; }
            if (is_gs) { s_b_map.push_back(Ev{Kind::LoadMapPre, object_index, object}); } // map events are never capped
            const size_t pending = s_b_gm.size() + s_b_actor.size() + s_b_loaded.size() + (s_queue.size() - s_queue_pos);
            if (pending >= kMaxQueue) { ++s_dropped; return; }
            if (is_gm && (!LuaMod::m_init_game_state_pre_callbacks.empty() || !LuaMod::m_init_game_state_post_callbacks.empty()))
            {
                s_b_gm.push_back(Ev{Kind::InitGameState, object_index, object});
            }
            if (!LuaMod::m_begin_play_pre_callbacks.empty() || !LuaMod::m_begin_play_post_callbacks.empty())
            {
                if (object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_WasLoaded)))
                {
                    s_b_loaded.push_back(Ev{Kind::BeginPlay, object_index, object});
                }
                else
                {
                    s_b_actor.push_back(Ev{Kind::BeginPlay, object_index, object});
                }
            }
        }

        // Caller holds m_thread_actions_mutex. The whole backlog was matched: queue its events in
        // PC order (LoadMap pre, InitGameState, BeginPlay, LoadMap post).
        static auto end_batch() -> void
        {
            if (s_b_map.empty() && s_b_gm.empty() && s_b_actor.empty() && s_b_loaded.empty()) { return; }
            if (s_queue_pos >= s_queue.size()) { release(s_queue); s_queue_pos = 0; }
            const bool with_map = !s_b_map.empty();
            for (const auto& e : s_b_map) { s_queue.push_back(e); }
            for (const auto& e : s_b_gm) { s_queue.push_back(e); }
            if (with_map)
            {
                // loaded-with-the-map actors began play inside LoadMap; keep index order with spawned ones
                for (const auto& e : s_b_loaded) { s_queue.push_back(e); }
            }
            else
            {
                s_loaded_unreported += s_b_loaded.size(); // streamed-in / async-loaded: BeginPlay timing unknown
            }
            for (const auto& e : s_b_actor) { s_queue.push_back(e); }
            for (const auto& e : s_b_map) { s_queue.push_back(Ev{Kind::LoadMapPost, e.index, e.obj}); }
            s_maps += s_b_map.size();
            s_gms += s_b_gm.size();
            s_actors += s_b_actor.size() + (with_map ? s_b_loaded.size() : 0);
            if (with_map)
            {
                try
                {
                    Output::send(STR("[WORLD-EV] map change seen (object index): maps={} gamemodes={} actors={} -> LoadMap pre, InitGameState, BeginPlay, LoadMap post queued (PS5: all after the load)\n"),
                                 s_b_map.size(), s_b_gm.size(), s_b_actor.size() + s_b_loaded.size());
                }
                catch (...)
                {
                }
            }
            reset_batch();
        }

        static auto kind_name(Kind k) -> StringType
        {
            switch (k)
            {
            case Kind::LoadMapPre: return StringType{STR("LoadMapPre")};
            case Kind::InitGameState: return StringType{STR("InitGameState")};
            case Kind::BeginPlay: return StringType{STR("BeginPlay")};
            case Kind::LoadMapPost: return StringType{STR("LoadMapPost")};
            }
            return StringType{STR("?")};
        }

        static auto short_name(Unreal::UObject* o) -> StringType
        {
            StringType n{};
            if (!o) { return n; }
            try { n = o->GetName(); } catch (...) { return StringType{}; }
            if (n.size() > 64) { n.resize(64); }
            for (auto& c : n) { if (c < 32 || c > 126) { c = '?'; } }
            return n;
        }

        // One contained Lua call (plain try/catch; the Lua stack is always restored). push_args
        // pushes the arguments after the function.
        template <typename PushArgs>
        static auto call_one(const LuaMadeSimple::Lua& lua, int32_t ref, int nargs, Kind k, PushArgs&& push_args) -> void
        {
            lua_State* L = lua.get_lua_state();
            const int top = lua_gettop(L);
            try
            {
                get_function_ref_cxx(lua, ref);
                push_args();
                lua.call_function(nargs, 0);
                lua_settop(L, top);
                ++s_delivered;
            }
            catch (const std::exception& e)
            {
                lua_settop(L, top);
                if (++s_errors <= 20)
                {
                    try { Output::send<LogLevel::Error>(STR("[WORLD-EV] callback error (#{}, kind={}, mod={}): {}\n"), s_errors, kind_name(k), refused_log::mod_name_of(lua), ensure_str(e.what())); } catch (...) {}
                }
            }
            catch (...)
            {
                lua_settop(L, top);
                if (++s_errors <= 20)
                {
                    try { Output::send<LogLevel::Error>(STR("[WORLD-EV] callback threw a non-standard exception (#{}, kind={})\n"), s_errors, kind_name(k)); } catch (...) {}
                }
            }
        }

        static auto snapshot(const std::vector<LuaMod::LuaCallbackData>& v, std::vector<std::pair<const LuaMadeSimple::Lua*, int32_t>>& out) -> void
        {
            for (const auto& cb : v)
            {
                for (const auto& [lua_ptr, registry_index] : cb.registry_indexes)
                {
                    if (lua_ptr) { out.emplace_back(lua_ptr, registry_index.lua_index); }
                }
            }
        }

        static auto report_stats() -> void
        {
            static auto s_last_report = std::chrono::steady_clock::now();
            static uint64_t s_last_sum{0};
            const auto now = std::chrono::steady_clock::now();
            if (now - s_last_report < std::chrono::seconds(10)) { return; }
            s_last_report = now;
            const uint64_t sum = s_maps + s_gms + s_actors + s_delivered + s_skipped_dead + s_dropped + s_loaded_unreported + s_errors;
            if (sum == s_last_sum) { return; }
            s_last_sum = sum;
            try
            {
                Output::send(STR("[WORLD-EV] stats maps={} gamemodes={} actors={} lua_calls={} skipped_dead={} streamed_unreported={} dropped={} errors={} queued={} paced_skips={}\n"),
                             s_maps, s_gms, s_actors, s_delivered, s_skipped_dead, s_loaded_unreported, s_dropped, s_errors,
                             s_queue.size() - s_queue_pos, s_pace.skipped);
            }
            catch (...)
            {
            }
        }

        // Game thread, every pump (inside process_pending_notify_on_new_object_callbacks).
        static auto deliver() -> void
        {
            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                if (s_queue_pos >= s_queue.size()) { return; }
            }
            std::chrono::steady_clock::time_point deadline{};
            if (!s_pace.begin(std::chrono::microseconds(1000), deadline)) { return; }
            const R9PaceEnd pace_end{s_pace};
            // Same lock discipline as the [R9-NOTIFY] executor: held across the Lua calls so a
            // mod uninstall (erases callbacks under this lock) can never free a state mid-call.
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            static std::vector<std::pair<const LuaMadeSimple::Lua*, int32_t>> s_pre{}, s_post{};
            static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
            size_t n = 0;
            while (s_queue_pos < s_queue.size())
            {
                if (n > 0 && std::chrono::steady_clock::now() > deadline) { break; }
                ++n;
                const Ev ev = s_queue[s_queue_pos++];
                Unreal::FUObjectItem* item = Unreal::FUObjectArray::IndexToObject(ev.index);
                const bool alive = item && item->GetUObject() == ev.obj && !item->IsUnreachable() &&
                                   !ev.obj->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_BeginDestroyed | Unreal::RF_FinishDestroyed));
                const bool is_map = ev.kind == Kind::LoadMapPre || ev.kind == Kind::LoadMapPost;
                // A dead actor / game mode is never handed to Lua. A map event still fires (the
                // load did happen); only its World argument becomes invalid.
                if (!alive && !is_map) { ++s_skipped_dead; continue; }
                // A LoadMap detour that really armed delivers itself: never twice.
                if (is_map && refused_log::loadmap_state.load(std::memory_order_acquire) == 2) { continue; }
                s_pre.clear();
                s_post.clear();
                switch (ev.kind)
                {
                case Kind::LoadMapPre: snapshot(LuaMod::m_load_map_pre_callbacks, s_pre); break;
                case Kind::LoadMapPost: snapshot(LuaMod::m_load_map_post_callbacks, s_pre); break;
                case Kind::InitGameState:
                    snapshot(LuaMod::m_init_game_state_pre_callbacks, s_pre);
                    snapshot(LuaMod::m_init_game_state_post_callbacks, s_post);
                    break;
                case Kind::BeginPlay:
                    snapshot(LuaMod::m_begin_play_pre_callbacks, s_pre);
                    snapshot(LuaMod::m_begin_play_post_callbacks, s_post);
                    break;
                }
                const auto ki = static_cast<size_t>(ev.kind);
                if (s_first_kind[ki] && (!s_pre.empty() || !s_post.empty()))
                {
                    s_first_kind[ki] = false;
                    try { Output::send(STR("[WORLD-EV] first delivery kind={} callbacks={} object='{}'\n"), kind_name(ev.kind), s_pre.size() + s_post.size(), short_name(alive ? ev.obj : nullptr)); } catch (...) {}
                }
                if (is_map)
                {
                    Unreal::UObject* engine{nullptr};
                    Unreal::UObject* pending_game{nullptr};
                    Unreal::UObject* world{nullptr};
                    if (ev.kind == Kind::LoadMapPost && alive)
                    {
                        // GameState -> its Level -> that Level's World (class-checked; else invalid)
                        Unreal::UObject* level = ev.obj->GetOuterPrivate();
                        Unreal::UObject* w = level ? level->GetOuterPrivate() : nullptr;
                        if (w && static_cast<Unreal::UStruct*>(w->GetClassPrivate()) == s_world) { world = w; }
                    }
                    for (const auto& [lua_ptr, ref] : s_pre)
                    {
                        const auto& lua = *lua_ptr;
                        call_one(lua, ref, 5, ev.kind, [&]() {
                            LuaType::RemoteUnrealParam::construct(lua, &engine, s_object_property_name);
                            LuaType::RemoteUnrealParam::construct(lua, &world, s_object_property_name);
                            lua.set_nil(); // URL: not available on this route
                            LuaType::RemoteUnrealParam::construct(lua, &pending_game, s_object_property_name);
                            lua.set_string(std::string_view{});
                        });
                    }
                    if (ev.kind == Kind::LoadMapPost)
                    {
                        try { Output::send(STR("[WORLD-EV] LoadMap post delivered: world='{}' callbacks={}\n"), short_name(world), s_pre.size()); } catch (...) {}
                    }
                    continue;
                }
                Unreal::UObject* context = ev.obj;
                for (const auto& [lua_ptr, ref] : s_pre)
                {
                    const auto& lua = *lua_ptr;
                    call_one(lua, ref, 1, ev.kind, [&]() { LuaType::RemoteUnrealParam::construct(lua, &context, s_object_property_name); });
                }
                for (const auto& [lua_ptr, ref] : s_post)
                {
                    const auto& lua = *lua_ptr;
                    call_one(lua, ref, 1, ev.kind, [&]() { LuaType::RemoteUnrealParam::construct(lua, &context, s_object_property_name); });
                }
            }
            if (s_queue_pos >= s_queue.size()) { release(s_queue); s_queue_pos = 0; }
        }
    } // namespace world_ev
"""

edits = []  # (anchor-name, old, new)

# ---- 1. refused_log state + families -------------------------------------------------------
edits.append(('refused_log.loadmap_state',
'''        inline std::atomic<int> loadmap_state{0};                   // 0 eager block ran, 1 deferred arm pending, 2 armed, 3 arm refused
''',
'''        inline std::atomic<int> loadmap_state{0};                   // 0 eager block ran, 1 deferred arm pending, 2 armed, 3 arm refused
        inline std::atomic<int> world_route_state{0};               // [WORLD-EV] object-index route: 0 pending, 1 live, 2 refused
'''))

edits.append(('refused_log.Family',
'''        enum class Family { EagerDetour, LoadMap, CustomEvent };
''',
'''        enum class Family { EagerDetour, LoadMap, CustomEvent, WorldEvent, EndPlay };
'''))

edits.append(('refused_log.undelivered_reason.LoadMap',
'''            case Family::LoadMap:
                switch (loadmap_state.load(std::memory_order_acquire))
                {
                case 1: return "loadmap-deferred-arm-pending";
                case 3: return "loadmap-detour-arm-refused";
                default: return nullptr;
                }
''',
'''            case Family::LoadMap:
                // [WORLD-EV] delivered by an armed detour (state 2), by the eager block (not skipped),
                // or else by the data-only object-index route; a gap only once that route is refused.
                if (loadmap_state.load(std::memory_order_acquire) == 2 || !eager_detour_block_skipped.load(std::memory_order_acquire)) { return nullptr; }
                return world_route_state.load(std::memory_order_acquire) == 2 ? "world-event-route-refused" : nullptr;
            case Family::WorldEvent:
                // [WORLD-EV] InitGameState / BeginPlay: object-index route in the skipped-detour world.
                if (!eager_detour_block_skipped.load(std::memory_order_acquire)) { return nullptr; }
                return world_route_state.load(std::memory_order_acquire) == 2 ? "world-event-route-refused" : nullptr;
            case Family::EndPlay:
                // [WORLD-EV] no data-only route: the object index sees an actor go only after it is
                // freed (no live actor to hand to Lua); ReceiveEndPlay Func swaps would use up the
                // script-thunk slots the pump carriers need.
                return eager_detour_block_skipped.load(std::memory_order_acquire) ? "endplay-no-data-only-route" : nullptr;
'''))

edits.append(('refused_log.accepted.end',
'''                emit(kind, target, mod_name_of(lua), why, f == Family::LoadMap && loadmap_state.load() == 1 ? "queued-pending" : "accepted-undelivered");
            }
        }
''',
'''                emit(kind, target, mod_name_of(lua), why, f == Family::LoadMap && loadmap_state.load() == 1 ? "queued-pending" : "accepted-undelivered");
            }
        }

        // [WORLD-EV] one NON-counting line per registration kind (id 0..31): how the PS5 delivery
        // of this hook differs from PC. Silent in the detour world and once the route is refused.
        inline auto route_note(unsigned id, const char* kind, const char* timing) -> void
        {
            static std::atomic<uint32_t> s_noted{0};
            if (id >= 32 || !eager_detour_block_skipped.load(std::memory_order_acquire) || world_route_state.load(std::memory_order_acquire) == 2) { return; }
            if ((s_noted.fetch_or(1u << id, std::memory_order_relaxed) & (1u << id)) != 0) { return; }
            char line[512];
            std::snprintf(line, sizeof(line), "[WORLD-EV] registration kind=%s route=object-index timing=%s", kind, timing);
            StringType wide{};
            for (const char* p = line; *p != '\\0'; ++p)
            {
                wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p)));
            }
            L22Diagnostics::observe([&]() { Output::send(STR("{}\\n"), wide); });
        }
'''))

# ---- 2. registrations: lock the append (delivery reads these vectors on the game thread),
#         re-family, one route note ----------------------------------------------------------
NOTES = {
    'RegisterLoadMapPreHook': (0, 'after-the-load(PC:before);World=invalid,Engine=invalid,URL=nil;return-ignored'),
    'RegisterLoadMapPostHook': (1, 'first-pump-after-the-load;Engine=invalid,URL=nil;return-ignored'),
    'RegisterInitGameStatePreHook': (2, 'after-InitGameState-ran(PC:before)'),
    'RegisterInitGameStatePostHook': (3, 'first-pump-after-the-load'),
    'RegisterBeginPlayPreHook': (4, 'after-BeginPlay-ran(PC:before);streamed-level-actors-not-reported'),
    'RegisterBeginPlayPostHook': (5, 'next-pump-after-spawn;streamed-level-actors-not-reported'),
}

LOADMAP_OLD = '''            LuaMod::{vec}.emplace_back(LuaMod::LuaCallbackData{{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {{std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{{hook_lua, lua_callback_registry_index}}}}}});

            refused_log::accepted(lua, "{kind}", StringType{{STR("-")}}, refused_log::Family::LoadMap);
'''
LOADMAP_NEW = '''            {{ // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{{LuaMod::m_thread_actions_mutex}};
            LuaMod::{vec}.emplace_back(LuaMod::LuaCallbackData{{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {{std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{{hook_lua, lua_callback_registry_index}}}}}});
            }}

            refused_log::accepted(lua, "{kind}", StringType{{STR("-")}}, refused_log::Family::LoadMap);
            refused_log::route_note({nid}, "{kind}", "{timing}");
'''
OTHER_OLD = '''            LuaMod::{vec}.emplace_back(LuaMod::LuaCallbackData{{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {{std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{{hook_lua, lua_callback_registry_index}}}},
            }});

            refused_log::accepted(lua, "{kind}", StringType{{STR("-")}}, refused_log::Family::EagerDetour);
'''
OTHER_NEW = '''            {{ // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{{LuaMod::m_thread_actions_mutex}};
            LuaMod::{vec}.emplace_back(LuaMod::LuaCallbackData{{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {{std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{{hook_lua, lua_callback_registry_index}}}},
            }});
            }}

            refused_log::accepted(lua, "{kind}", StringType{{STR("-")}}, refused_log::Family::WorldEvent);
            refused_log::route_note({nid}, "{kind}", "{timing}");
'''
for vec, kind in (('m_load_map_pre_callbacks', 'RegisterLoadMapPreHook'), ('m_load_map_post_callbacks', 'RegisterLoadMapPostHook')):
    nid, timing = NOTES[kind]
    edits.append(('register.' + kind, LOADMAP_OLD.format(vec=vec, kind=kind), LOADMAP_NEW.format(vec=vec, kind=kind, nid=nid, timing=timing)))
for vec, kind in (('m_init_game_state_pre_callbacks', 'RegisterInitGameStatePreHook'), ('m_init_game_state_post_callbacks', 'RegisterInitGameStatePostHook'),
                  ('m_begin_play_pre_callbacks', 'RegisterBeginPlayPreHook'), ('m_begin_play_post_callbacks', 'RegisterBeginPlayPostHook')):
    nid, timing = NOTES[kind]
    edits.append(('register.' + kind, OTHER_OLD.format(vec=vec, kind=kind), OTHER_NEW.format(vec=vec, kind=kind, nid=nid, timing=timing)))
for kind in ('RegisterEndPlayPreHook', 'RegisterEndPlayPostHook'):
    edits.append(('register.' + kind,
                  '            refused_log::accepted(lua, "%s", StringType{STR("-")}, refused_log::Family::EagerDetour);\n' % kind,
                  '            refused_log::accepted(lua, "%s", StringType{STR("-")}, refused_log::Family::EndPlay); // [WORLD-EV] stays refused\n' % kind))

# ---- 3. the [WORLD-EV] engine block, right after the [R9-PACE] pacers ----------------------
edits.append(('r9.pacers',
'''    static R9Pacer s_r9_pace_watch{}, s_r9_pace_match{}, s_r9_pace_exec{};
''',
'''    static R9Pacer s_r9_pace_watch{}, s_r9_pace_match{}, s_r9_pace_exec{};
''' + WORLD_EV_BLOCK))

# ---- 4. the [R9-NOTIFY] feed also serves the world events ----------------------------------
edits.append(('r9.feed.consumers',
'''        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            if (LuaMod::m_static_construct_object_lua_callbacks.empty())
            {
                Unreal::UObjectGlobals::r9_index_notify_disarm(); // cheap: stop collecting while nobody listens
                s_backlog_idx.clear();
                s_backlog_obj.clear();
                s_backlog_pos = 0;
                return;
            }
        }
''',
'''        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            world_ev::s_wanted = world_ev::wanted_locked(); // [WORLD-EV] map/InitGameState/BeginPlay hooks also read the index
            if (LuaMod::m_static_construct_object_lua_callbacks.empty() && !world_ev::s_wanted)
            {
                Unreal::UObjectGlobals::r9_index_notify_disarm(); // cheap: stop collecting while nobody listens
                s_backlog_idx.clear();
                s_backlog_obj.clear();
                s_backlog_pos = 0;
                world_ev::reset_batch();
                return;
            }
        }
        if (world_ev::s_wanted) { (void)world_ev::ensure_ready(); } // [WORLD-EV] outside the lock (class lookups)
'''))

edits.append(('r9.feed.collected',
'''            if (!r9_collected)
            {
                static bool s_unusable_logged{false};
''',
'''            world_ev::note_index(r9_collected); // [WORLD-EV]
            if (!r9_collected)
            {
                static bool s_unusable_logged{false};
'''))

edits.append(('r9.feed.observe',
'''            if (!object || static_cast<void*>(object) != expected) { continue; } // slot changed hands: the newer object has its own entry
''',
'''            if (!object || static_cast<void*>(object) != expected) { continue; } // slot changed hands: the newer object has its own entry
            world_ev::observe(object, object_index); // [WORLD-EV]
'''))

edits.append(('r9.feed.end_and_pump',
'''        }
    }

    static auto process_pending_notify_on_new_object_callbacks() -> void
    {
        r9_feed_notify_from_index();
''',
'''        }
        if (s_backlog_pos >= s_backlog_idx.size()) { world_ev::end_batch(); } // [WORLD-EV] whole batch matched (lock held)
    }

    static auto process_pending_notify_on_new_object_callbacks() -> void
    {
        r9_feed_notify_from_index();
        world_ev::deliver();      // [WORLD-EV] paced, contained, game thread
        world_ev::report_stats(); // [WORLD-EV] one stats line per 10 s, only when something changed
'''))

# ---- 5. the futile LoadMap detour refusal no longer claims "undelivered" ------------------
edits.append(('loadmap.deferred.refused',
'''        Output::send<LogLevel::Error>(STR("[R3] LoadMap deferred arm refused (pre id {:X}, post id {:X}): queued Lua RegisterLoadMapPreHook/PostHook callbacks stay undelivered for this process lifetime (no re-arm); mod loading and every other surface are unaffected\\n"), pre_id, post_id);
        refused_log::loadmap_state.store(3, std::memory_order_release);
        refused_log::emit("RegisterLoadMapPre/PostHook", StringType{STR("UEngine::LoadMap")}, StringType{STR("*")}, "loadmap-detour-arm-refused", "never-delivered");
''',
'''        Output::send<LogLevel::Error>(STR("[R3] LoadMap deferred arm refused (pre id {:X}, post id {:X}): the LoadMap detour stays off for this process lifetime (no re-arm); Lua RegisterLoadMapPreHook/PostHook callbacks are delivered by the data-only object-index route instead (see [WORLD-EV] lines); mod loading and every other surface are unaffected\\n"), pre_id, post_id);
        refused_log::loadmap_state.store(3, std::memory_order_release);
        // [WORLD-EV] no counted [REFUSED] line here any more: the object-index route delivers these
        // callbacks; if that route is refused it emits its own counted line.
'''))

# ---- apply (all-or-nothing) ----------------------------------------------------------------
out = src
for name, old, new in edits:
    n = out.count(old)
    if n != 1:
        sys.exit('apply_hooks: anchor %r found %d times (need exactly 1); nothing written' % (name, n))
    out = out.replace(old, new, 1)
target.write_text(out)
print('apply_hooks: %d edits applied to %s' % (len(edits), target))
