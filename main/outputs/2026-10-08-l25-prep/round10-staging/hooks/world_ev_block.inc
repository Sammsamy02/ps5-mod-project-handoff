
    // =====================================================================================
    // [WORLD-EV] (round10 HOOKS track r2; generic for every UE game, no game/mod names, no new
    // offsets) Data-only delivery of the Lua map-load / InitGameState / BeginPlay hooks
    // (RegisterLoadMapPre/PostHook, RegisterInitGameStatePre/PostHook,
    // RegisterBeginPlayPre/PostHook). On PC these are code detours; on PS5 game code is
    // execute-only, so the detour block is skipped and the registrations used to be accepted
    // but never delivered. Route: the same delete-listener-fed object index that already
    // serves NotifyOnNewObject ([R9-NOTIFY]) reports objects that became live since the last
    // pump. Each new object's class chain is compared (pointer equality) with four engine
    // classes resolved once by path (/Script/Engine.Actor, .GameModeBase, .GameStateBase,
    // .World) and checked to be real classes (meta class named "Class", GameModeBase and
    // GameStateBase derive from Actor, World does not). A new GameStateBase means "a map
    // finished loading" (UEngine::LoadMap spawns one per load, after the GameMode, inside
    // the load); a new GameModeBase means "InitGameState ran"; a new Actor means "BeginPlay
    // ran".
    // One map load is NOT one index batch: objects that reuse freed slots are found by the
    // budgeted watch re-check, spread over many collects. So events are tied to a map by the
    // actor's Level (its outer), not by the batch:
    //   * the GameState's outer is the new map's persistent level. When it appears, LoadMap
    //     pre is queued at once and a "map window" opens for that level.
    //   * GameModes and actors whose level is not the current map's level wait in a hold list
    //     (max 5 s). If a GameState of their level appears in time they join its map window;
    //     otherwise GameModes / spawned actors are delivered and package-loaded (RF_WasLoaded)
    //     actors of another level are counted as streamed_unreported (streamed level / World
    //     Partition cell: their BeginPlay waits for the level to become visible, which this
    //     route cannot see).
    //   * the map window collects that level's GameModes and actors; it closes when no
    //     GameMode / loaded actor of the level arrived for 3 s (or 30 s after it opened, or
    //     when the next map's GameState / GameMode appears). Then InitGameState, BeginPlay and
    //     LoadMap post are queued in PC order.
    //   * after the window closed, spawned actors of the current level are queued at once;
    //     loaded ones wait in the hold list (a new map may reuse the level's address) and,
    //     if no GameState claims them, are delivered late (late_after_post counter).
    // Delivery is on the game thread, from the existing pump (process_pending_notify_on_new_
    // object_callbacks, called by every drain), time-paced (~1 ms per ~16 ms window,
    // [R9-PACE]) and contained per callback (one mod's Lua error never stops the others).
    // Honest timing differences vs PC (also in the one-line registration note):
    //   * everything is delivered AFTER the event; "pre" hooks also run after the event.
    //     LoadMap pre comes at the first pump after the GameState is seen; InitGameState,
    //     BeginPlay (of the load) and LoadMap post >= 3 s after the last load object is seen.
    //   * LoadMap args: Engine = invalid object (not looked up), World = invalid for pre (the
    //     old world is already gone) / the new world for post, URL = nil, PendingGame =
    //     invalid object, Error = "". Return values are ignored (the load already happened).
    //   * BeginPlay is PARTIAL (still counted as a gap until console-proven): actors of
    //     streamed levels are not reported; actors created and destroyed between two pumps
    //     are missed; actors spawned outside the current level wait up to 5 s.
    //   * an object that dies (or whose slot now holds another class) before delivery is
    //     skipped, never handed to Lua.
    // EndPlay stays REFUSED (counted): by the time the index sees an actor gone it is already
    // freed, so there is no live actor to hand to Lua, and the Blueprint ReceiveEndPlay
    // Func-swap route would consume script-thunk slots the pump carriers need.
    // FAIL CLOSED: a missing / non-class engine class or an object index that never becomes
    // usable (60 s) turns the route off with one [WORLD-EV] line + one counted [REFUSED] line
    // per registered hook kind.
    // =====================================================================================
    namespace world_ev
    {
        enum class Kind : uint8_t { LoadMapPre, InitGameState, BeginPlay, LoadMapPost };
        struct Ev // 24 bytes
        {
            Unreal::UObject* obj;
            Unreal::UStruct* cls; // class at observe time: re-checked at delivery (slot reuse)
            int32_t index;
            Kind kind;
        };
        struct Held // waits (max kHoldMs) for a GameState of its level
        {
            Ev ev;
            Unreal::UObject* level;
            int64_t t_ms;
            bool loaded;
        };
        static Unreal::UStruct* s_actor{nullptr};
        static Unreal::UStruct* s_game_mode{nullptr};
        static Unreal::UStruct* s_game_state{nullptr};
        static Unreal::UStruct* s_world{nullptr};
        static bool s_wanted{false}; // set by the feed under m_thread_actions_mutex (game thread)
        static bool s_classes_ok{false};
        static bool s_class_check_failed{false};
        static bool s_index_ok{false};
        static int s_resolve_tries{0};
        static int64_t s_first_wanted_ms{0};
        static int64_t s_last_resolve_ms{0};
        // events waiting for their map (FIFO from s_hold_pos)
        static std::vector<Held> s_hold{};
        static size_t s_hold_pos{0};
        // the current map (persistent level from its GameState) and its open load window
        static Unreal::UObject* s_map_level{nullptr};
        static bool s_window_open{false};
        static Ev s_window_gs{};
        static int64_t s_window_open_ms{0}, s_window_last_ms{0};
        static uint64_t s_window_joined{0};
        static std::vector<Ev> s_win_gm{}, s_win_actor{};
        // ordered delivery queue (FIFO from s_queue_pos)
        static std::vector<Ev> s_queue{};
        static size_t s_queue_pos{0};
        constexpr int64_t kHoldMs = 5000;       // hold list: max wait for a GameState of the same level
        constexpr int64_t kQuietMs = 3000;      // map window closes after this long without a load object
        constexpr int64_t kMaxWindowMs = 30000; // ... or this long after it opened
        constexpr size_t kMaxPending = size_t{1} << 15; // hold + window + queue (<= ~1.5 MB; libc heap is small)
        static uint64_t s_maps{0}, s_gms{0}, s_actors{0}, s_delivered{0}, s_skipped_dead{0}, s_dropped{0}, s_streamed_unreported{0}, s_late_after_post{0};
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

        template <typename V>
        static auto release(V& v) -> void // drop a big buffer instead of keeping its capacity
        {
            if (v.capacity() > 8192) { V{}.swap(v); } else { v.clear(); }
        }

        // Never throws (an allocation failure is counted as a drop instead of leaving the pump).
        template <typename T>
        static auto push_safe(std::vector<T>& v, const T& e) -> bool
        {
            try
            {
                v.push_back(e);
                return true;
            }
            catch (...)
            {
                ++s_dropped;
                return false;
            }
        }

        // FIFO compaction: the delivered / expired head never grows without bound.
        template <typename T>
        static auto compact(std::vector<T>& v, size_t& pos) -> void
        {
            if (pos >= v.size()) { release(v); pos = 0; }
            else if (pos >= 4096 && pos * 2 >= v.size())
            {
                v.erase(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(pos));
                pos = 0;
            }
        }

        static auto pending() -> size_t
        {
            return (s_hold.size() - s_hold_pos) + s_win_gm.size() + s_win_actor.size() + (s_queue.size() - s_queue_pos);
        }

        static auto enqueue(const Ev& e) -> bool
        {
            compact(s_queue, s_queue_pos);
            return push_safe(s_queue, e);
        }

        static auto queue_actor_event(const Ev& e) -> void // InitGameState / BeginPlay, counted when queued
        {
            if (!enqueue(e)) { return; }
            if (e.kind == Kind::InitGameState) { ++s_gms; } else { ++s_actors; }
        }

        static auto reset_batch() -> void // game thread, lock held (feed / refuse): forget hold + window
        {
            release(s_hold); s_hold_pos = 0;
            release(s_win_gm); release(s_win_actor);
            s_window_open = false;
            s_map_level = nullptr;
        }

        static auto close_window(const char* why, int64_t now) -> void // lock held
        {
            if (!s_window_open) { return; }
            s_window_open = false;
            const size_t gms = s_win_gm.size(), actors = s_win_actor.size();
            for (const auto& e : s_win_gm) { queue_actor_event(e); }
            for (const auto& e : s_win_actor) { queue_actor_event(e); }
            (void)enqueue(Ev{s_window_gs.obj, s_window_gs.cls, s_window_gs.index, Kind::LoadMapPost}); // map events are never capped
            release(s_win_gm); release(s_win_actor);
            try
            {
                StringType wide{};
                for (const char* p = why; *p != '\0'; ++p) { wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                Output::send(STR("[WORLD-EV] map load window closed after {} ms (reason={}): gamemodes={} actors={} held_joined={} -> InitGameState, BeginPlay, LoadMap post queued\n"),
                             now - s_window_open_ms, wide, gms, actors, s_window_joined);
            }
            catch (...)
            {
            }
        }

        static auto add_to_window(const Ev& e) -> void // lock held
        {
            (void)push_safe(e.kind == Kind::InitGameState ? s_win_gm : s_win_actor, e);
        }

        // A new GameState: its level is the new map's persistent level.
        static auto on_game_state(const Ev& gs, Unreal::UObject* level, int64_t now) -> void // lock held
        {
            if (s_window_open && level == s_map_level) { return; } // a second GameState in the same load: no second map
            close_window("next-map", now);
            s_map_level = level;
            s_window_open = true;
            s_window_gs = gs;
            s_window_open_ms = s_window_last_ms = now;
            s_window_joined = 0;
            ++s_maps;
            (void)enqueue(Ev{gs.obj, gs.cls, gs.index, Kind::LoadMapPre}); // map events are never capped
            // held events of this level (seen before its GameState) join the window, in arrival order
            size_t w = s_hold_pos;
            for (size_t r = s_hold_pos; r < s_hold.size(); ++r)
            {
                if (s_hold[r].level == level)
                {
                    add_to_window(s_hold[r].ev);
                    ++s_window_joined;
                    if (s_hold[r].loaded || s_hold[r].ev.kind == Kind::InitGameState) { s_window_last_ms = now; }
                }
                else
                {
                    s_hold[w++] = s_hold[r];
                }
            }
            s_hold.resize(w);
            compact(s_hold, s_hold_pos);
            try
            {
                Unreal::UObject* world = level ? level->GetOuterPrivate() : nullptr;
                const bool world_ok = world && static_cast<Unreal::UStruct*>(world->GetClassPrivate()) == s_world;
                Output::send(STR("[WORLD-EV] map change seen (object index): world='{}' held_joined={} -> LoadMap pre queued; this map's InitGameState/BeginPlay/LoadMap post follow when its load window closes (PS5: all after the load)\n"),
                             world_ok ? world->GetName() : StringType{STR("?")}, s_window_joined);
            }
            catch (...)
            {
            }
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
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            // one counted line per registered hook kind: registrations made while the route was
            // still pending were not counted as gaps at registration time
            const struct { const char* kind; size_t n; } kinds[] = {
                    {"RegisterLoadMapPreHook", LuaMod::m_load_map_pre_callbacks.size()},
                    {"RegisterLoadMapPostHook", LuaMod::m_load_map_post_callbacks.size()},
                    {"RegisterInitGameStatePreHook", LuaMod::m_init_game_state_pre_callbacks.size()},
                    {"RegisterInitGameStatePostHook", LuaMod::m_init_game_state_post_callbacks.size()},
                    {"RegisterBeginPlayPreHook", LuaMod::m_begin_play_pre_callbacks.size()},
                    {"RegisterBeginPlayPostHook", LuaMod::m_begin_play_post_callbacks.size()},
            };
            for (const auto& k : kinds)
            {
                if (k.n == 0) { continue; }
                char target[96];
                std::snprintf(target, sizeof(target), "object-index-world-events registrations=%u", static_cast<unsigned>(k.n));
                StringType wide_target{};
                for (const char* p = target; *p != '\0'; ++p) { wide_target.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                refused_log::emit(k.kind, wide_target, StringType{STR("*")}, reason, "accepted-undelivered");
            }
            reset_batch();
            release(s_queue); s_queue_pos = 0;
        }

        static auto chain_has(Unreal::UStruct* s, Unreal::UStruct* base) -> bool
        {
            int depth = 0;
            for (Unreal::UStruct* c = s; c && depth < 64; c = c->GetSuperStruct(), ++depth)
            {
                if (c == base) { return true; }
            }
            return false;
        }

        static auto is_class_object(Unreal::UStruct* s) -> bool // its own class is the engine's "Class"
        {
            Unreal::UClass* meta = s ? s->GetClassPrivate() : nullptr;
            return meta && meta->GetName() == StringType{STR("Class")};
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
                    const bool found = s_actor && s_game_mode && s_game_state && s_world;
                    // the path lookup ignores the class: prove these are the real classes, else not found
                    s_classes_ok = found && is_class_object(s_actor) && is_class_object(s_game_mode) && is_class_object(s_game_state) &&
                                   is_class_object(s_world) && s_game_mode != s_actor && s_game_state != s_actor && s_game_mode != s_game_state &&
                                   chain_has(s_game_mode, s_actor) && chain_has(s_game_state, s_actor) && !chain_has(s_world, s_actor) &&
                                   !chain_has(s_game_mode, s_game_state) && !chain_has(s_game_state, s_game_mode);
                    s_class_check_failed = found && !s_classes_ok;
                }
                catch (...)
                {
                    s_classes_ok = false;
                }
                if (!s_classes_ok) { s_actor = s_game_mode = s_game_state = s_world = nullptr; }
            }
            if (s_classes_ok && s_index_ok)
            {
                if (refused_log::world_route_state.exchange(1, std::memory_order_acq_rel) == 0)
                {
                    try { Output::send(STR("[WORLD-EV] route live: object index + engine classes Actor/GameModeBase/GameStateBase/World resolved and class-checked (tries={}); LoadMap/InitGameState/BeginPlay Lua hooks deliver after the event on the game-thread pump\n"), s_resolve_tries); } catch (...) {}
                }
                return true;
            }
            if (now - s_first_wanted_ms >= 60000)
            {
                refuse(!s_classes_ok ? (s_class_check_failed ? "engine-class-check-failed" : "engine-class-lookup-failed") : "object-index-unusable-60s");
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
            // a NEW object at the current map level's address: that level was replaced
            if (object == s_map_level && !s_window_open) { s_map_level = nullptr; }
            bool is_actor{false}, is_gm{false}, is_gs{false};
            int depth = 0;
            Unreal::UStruct* const cls = object->GetClassPrivate();
            for (Unreal::UStruct* c = cls; c && depth < 64; c = c->GetSuperStruct(), ++depth)
            {
                if (c == s_actor) { is_actor = true; break; } // Actor is the last class of interest up the chain
                if (c == s_game_state) { is_gs = true; }
                else if (c == s_game_mode) { is_gm = true; }
            }
            if (!is_actor) { return; }
            if (object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_ClassDefaultObject | Unreal::RF_ArchetypeObject))) { return; }
            const int64_t now = r3_now_ms();
            Unreal::UObject* const level = object->GetOuterPrivate();
            if (is_gs) { on_game_state(Ev{object, cls, object_index, Kind::LoadMapPre}, level, now); } // map events are never capped
            const bool want_gm = is_gm && (!LuaMod::m_init_game_state_pre_callbacks.empty() || !LuaMod::m_init_game_state_post_callbacks.empty());
            const bool want_bp = !LuaMod::m_begin_play_pre_callbacks.empty() || !LuaMod::m_begin_play_post_callbacks.empty();
            const bool loaded = object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_WasLoaded));
            const bool in_window = s_window_open && level == s_map_level;
            if (is_gm && !in_window && level != s_map_level)
            {
                // a GameMode of another level: a new map load is under way (a late GameMode of the
                // current level just waits in the hold list; a reused level address is caught when
                // the new GameState appears)
                close_window("next-map", now);
                s_map_level = nullptr;
            }
            if (!want_gm && !want_bp) { return; }
            if (pending() + 2 > kMaxPending) { s_dropped += (want_gm ? 1 : 0) + (want_bp ? 1 : 0); return; }
            const Ev gm_ev{object, cls, object_index, Kind::InitGameState};
            const Ev bp_ev{object, cls, object_index, Kind::BeginPlay};
            if (in_window)
            {
                if (loaded || is_gm) { s_window_last_ms = now; }
                if (want_gm) { add_to_window(gm_ev); }
                if (want_bp) { add_to_window(bp_ev); }
                return;
            }
            if (!is_gm && !loaded && level != nullptr && level == s_map_level)
            {
                if (want_bp) { queue_actor_event(bp_ev); } // spawned during play in the current map
                return;
            }
            if (want_gm) { (void)push_safe(s_hold, Held{gm_ev, level, now, loaded}); }
            if (want_bp) { (void)push_safe(s_hold, Held{bp_ev, level, now, loaded}); }
        }

        // Lock held, every pump while the route is live: close a quiet window, expire the hold list.
        static auto tick() -> void
        {
            if (refused_log::world_route_state.load(std::memory_order_relaxed) != 1) { return; }
            if (!s_window_open && s_hold_pos >= s_hold.size()) { return; }
            const int64_t now = r3_now_ms();
            if (s_window_open && now - s_window_last_ms >= kQuietMs) { close_window("quiet", now); }
            else if (s_window_open && now - s_window_open_ms >= kMaxWindowMs) { close_window("max-30s", now); }
            while (s_hold_pos < s_hold.size() && now - s_hold[s_hold_pos].t_ms >= kHoldMs)
            {
                const Held& h = s_hold[s_hold_pos++];
                if (h.loaded && h.ev.kind == Kind::BeginPlay)
                {
                    // package-loaded and no GameState claimed it: the current map's level -> late, else streamed
                    if (h.level != nullptr && h.level == s_map_level) { ++s_late_after_post; queue_actor_event(h.ev); }
                    else { ++s_streamed_unreported; }
                }
                else
                {
                    queue_actor_event(h.ev); // GameMode / spawned actor without a map: delivered after the wait
                }
            }
            compact(s_hold, s_hold_pos);
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
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            const uint64_t sum = s_maps + s_gms + s_actors + s_delivered + s_skipped_dead + s_dropped + s_streamed_unreported + s_late_after_post + s_errors;
            if (sum == s_last_sum) { return; }
            s_last_sum = sum;
            try
            {
                Output::send(STR("[WORLD-EV] stats maps={} gamemodes={} actors={} lua_calls={} skipped_dead={} streamed_unreported={} late_after_post={} held={} window_open={} dropped={} errors={} queued={} paced_skips={}\n"),
                             s_maps, s_gms, s_actors, s_delivered, s_skipped_dead, s_streamed_unreported, s_late_after_post,
                             s_hold.size() - s_hold_pos, s_window_open ? 1 : 0, s_dropped, s_errors, s_queue.size() - s_queue_pos, s_pace.skipped);
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
                tick();
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
                // same slot, same address, still reachable, not being destroyed, and still the class
                // it had when observed (a reused slot/address with another class is never handed on)
                const bool alive = item && item->GetUObject() == ev.obj && !item->IsUnreachable() &&
                                   !ev.obj->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_BeginDestroyed | Unreal::RF_FinishDestroyed)) &&
                                   static_cast<Unreal::UStruct*>(ev.obj->GetClassPrivate()) == ev.cls;
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
            compact(s_queue, s_queue_pos);
        }
    } // namespace world_ev
