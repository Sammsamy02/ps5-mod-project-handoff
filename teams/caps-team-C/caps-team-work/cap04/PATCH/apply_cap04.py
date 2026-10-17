#!/usr/bin/env python3
"""apply_cap04.py -- capability #4 (early loading proof / deferred arm) staged source change.

Mac-only. Applies a fixed list of EXACT-ONCE text replacements to a COPY of the runtime candidate
tree. It never writes the real candidate tree unless --in-place is given AND the target is the
candidate path (the integrator's choice, after review).

  python3 apply_cap04.py --tree <copy-of-candidate>            # patch a copy (default: dry run)
  python3 apply_cap04.py --tree <copy> --write                 # write the patched files in that copy
  python3 apply_cap04.py --tree <candidate> --write --in-place # integrator only

Refuses (exit 1) when the input files match no pinned base (R11, or the live candidate with the R12
follow-ups) unless --allow-drift, when any anchor
is missing or matches more than once, or when the patch was already applied. Prints how many
replacements it checked; zero checked = failure.
"""
import argparse
import hashlib
import sys
from pathlib import Path

CANDIDATE = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate')

# Known bases. R11 = the last clean build (bfd95db6, snapshot 4513a2c6) = the rollback.
# A tree matching no known base is refused unless --allow-drift; the exact-once anchors are checked
# in every case (a drifted tree that still has every anchor exactly once patches identically).
BASES = {
    'R11-bfd95db6': {
        'src/LuaMod.cpp': '95591da5f530758dd65f80801a58369c8142e09e3fc15bff250a36e0c8e73f3c',
        'src/UObjectGlobals.cpp': '78e48b3a751827d3ba4757f3f2aacefb3028972f04650f595d72d42d9744b420',
        'src/UE4SSProgram.cpp': '121f2383e02e77e638dff31cd0e4b75fdd58df1715f77e7f3a1b6cacc01123da',
    },
    # The live candidate = R11 + the R12 follow-ups (LuaMod: refuse() split into emit_counted, plus the
    # [R12-WORLD] counted "object-index-not-usable-yet-60s" pending line; UObjectGlobals: one comment).
    # This is the base the cap #4 build is made from. A tree that moved further is refused (re-stage).
    'candidate-R12-20261012T2159': {
        'src/LuaMod.cpp': '96e8ff4d5eb8c2a98ec37f1d54282687e169fca8dfac816b46f0299e1c38999e',
        'src/UObjectGlobals.cpp': '695264769f1f4235e7d5085529eb1aa46a58928be6359bc74ff05babf2fd4088',
        'src/UE4SSProgram.cpp': '121f2383e02e77e638dff31cd0e4b75fdd58df1715f77e7f3a1b6cacc01123da',
    },
}
FILES = ('src/LuaMod.cpp', 'src/UObjectGlobals.cpp', 'src/UE4SSProgram.cpp')

EDITS = []  # (file, tag, old, new)


def edit(f, tag, old, new):
    EDITS.append((f, tag, old, new))


# ---------------------------------------------------------------------------------------------
# E1  LuaMod.cpp: the PS5 deferred LoadMap DETOUR arm is removed (never reachable on PS5).
# ---------------------------------------------------------------------------------------------
edit('src/LuaMod.cpp', 'E1-remove-deferred-detour-arm',
'''    // One-shot deferred arm; called ONLY from funcptr_marshal_hook after the marshal
    // world is proven (identity published + published game thread + dispatch owner).
    // PROSPERO-only state + entry point (both callers are inside __PROSPERO__ blocks);
    // the delivery bodies above compile in every world (the eager block uses them).
#ifdef __PROSPERO__
    static std::atomic<bool> m_loadmap_deferred_arm_pending{false};
    static std::atomic<bool> m_loadmap_deferred_arm_attempted{false};
    static auto try_deferred_loadmap_arm() -> void
    {
        if (!m_loadmap_deferred_arm_pending.load(std::memory_order_acquire)) return; // W-works world (or nothing deferred)
        if (m_loadmap_deferred_arm_attempted.exchange(true, std::memory_order_acq_rel)) return; // exactly one attempt per process
        m_loadmap_deferred_arm_pending.store(false, std::memory_order_release);
        const Unreal::Hook::FCallbackOptions deferred_opts{false, false, STR("UE4SS"), STR("LuaModImpl")};
        const auto pre_id = Unreal::Hook::RegisterLoadMapPreCallback(&lua_load_map_pre_callback, deferred_opts);
        const auto post_id = Unreal::Hook::RegisterLoadMapPostCallback(&lua_load_map_post_callback, deferred_opts);
        if (pre_id != Unreal::Hook::ERROR_ID && post_id != Unreal::Hook::ERROR_ID)
        {
            Output::send(STR("[R3] LoadMap detour armed (deferred arm; marshal world proven): queued Lua RegisterLoadMapPreHook/PostHook callbacks deliver on real LoadMap events\\n"));
            refused_log::loadmap_state.store(2, std::memory_order_release);
            return;
        }
        // Fail-closed: the failed side above already emitted the composed layer's loud
        // refusal pair (InstallHook Warning + AddCallback "[UE4SS.LoadMap.LuaModImpl]
        // Failed to add hook, detour installation likely failed!" Error). Never a silent
        // skip: name exactly what stays undelivered, once, for the process lifetime.
        Output::send<LogLevel::Error>(STR("[R3] LoadMap deferred arm refused (pre id {:X}, post id {:X}): the LoadMap detour stays off for this process lifetime (no re-arm); Lua RegisterLoadMapPreHook/PostHook callbacks are delivered by the data-only object-index route instead (see [WORLD-EV] lines); mod loading and every other surface are unaffected\\n"), pre_id, post_id);
        refused_log::loadmap_state.store(3, std::memory_order_release);
        // [WORLD-EV] no counted [REFUSED] line here any more: the object-index route delivers these
        // callbacks; if that route is refused it emits its own counted line.
    }
#endif
''',
'''    // [EARLY-4] (cap #4) The PS5 one-shot deferred LoadMap DETOUR arm that used to sit here is
    // removed. On PS5 game code is execute-only and the rules forbid code detours, so that arm
    // could only ever be refused (launch 14: "[R3] LoadMap deferred arm refused (pre id 0, post id 0)"
    // after two "Failed to add hook" lines), and a half-success would have left a partial native
    // registration. LoadMap / InitGameState / BeginPlay Lua hooks on PS5 are delivered ONLY by the
    // data-only [WORLD-EV] object-index route; refused_log::loadmap_state is set to 3 at startup
    // and never changes. The delivery bodies above stay for the non-PS5 eager block.
#ifdef __PROSPERO__
    // [EARLY-4] one-shot startup timeline markers (same steady clock as r3_now_ms; the value
    // passed to the diagnostic sink is the low 32 bits of that clock, so the host verifier can
    // prove the UE4SS log clock and the sink's CLOCK_MONOTONIC t_ms are the same clock).
    namespace early4
    {
        static auto now_ms() -> int64_t
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        static std::atomic<bool> s_first_pump_noted{false};
        static auto note_first_pump(bool vmt_tick) -> void
        {
            if (s_first_pump_noted.exchange(true, std::memory_order_acq_rel)) { return; }
            const int64_t t = now_ms();
            diagnostic_event("early_first_pump", static_cast<unsigned int>(static_cast<uint64_t>(t) & 0xFFFF'FFFFu));
            try { Output::send(STR("[EARLY] stage=first-pump t_ms={} src={}\\n"), t, vmt_tick ? STR("vmt-tick") : STR("marshal")); } catch (...) {}
        }
    }
#endif
''')

edit('src/LuaMod.cpp', 'E1-marshal-call-site',
'''            // [R3][worker-gate5-loadmap r3] The marshal world is now PROVEN live (identity
            // published, this IS the published game thread, dispatch owned): land the
            // deferred LoadMap arm exactly once. No-op unless the r3 gate queued it in
            // on_program_start; if the arm is refused it refuses LOUDLY (fail-closed).
            try_deferred_loadmap_arm();
''',
'''            // [EARLY-4] the marshal world is now PROVEN live (identity published, this IS the
            // published game thread, dispatch owned): record the first game-thread pump once.
            // (The deferred LoadMap detour arm that ran here is removed: no detours on PS5.)
            early4::note_first_pump(false);
''')

edit('src/LuaMod.cpp', 'E1-vmt-call-site',
'''                    // The marshal world is PROVEN live on the genuine game thread
                    // here (stronger than the carrier proof): land the deferred
                    // LoadMap arm exactly once, same as the marshal drain.
                    try_deferred_loadmap_arm();
''',
'''                    // [EARLY-4] first game-thread pump (the deferred LoadMap detour arm
                    // that ran here is removed: no detours on PS5).
                    early4::note_first_pump(true);
''')

edit('src/LuaMod.cpp', 'E1-maybe-unused-pre',
'''    static auto lua_load_map_pre_callback(Unreal::Hook::TCallbackIterationData<bool>& CallbackIterationData,''',
'''    [[maybe_unused]] static auto lua_load_map_pre_callback(Unreal::Hook::TCallbackIterationData<bool>& CallbackIterationData,''')

edit('src/LuaMod.cpp', 'E1-maybe-unused-post',
'''    static auto lua_load_map_post_callback(Unreal::Hook::TCallbackIterationData<bool>& CallbackIterationData,''',
'''    [[maybe_unused]] static auto lua_load_map_post_callback(Unreal::Hook::TCallbackIterationData<bool>& CallbackIterationData,''')

# The startup gate: on PS5 the data-only branch is unconditional and the eager detour block is
# compiled out (#else), so no code-detour registration is reachable on PS5 in any world.
edit('src/LuaMod.cpp', 'E1-startup-branch-head',
'''        if (Unreal::R3MarshalPumpPending())
        {
            // [R3][worker-gate5-loadmap r3] narrows r2's all-lifetime loss: the sibling
            // registrations below stay unregistered for this process lifetime (unchanged
            // r2 law), while the LoadMap pre/post Lua callbacks are QUEUED and a one-shot
            // deferred arm is attempted once the FuncPtr-marshal world is proven live
            // (funcptr_marshal_hook -> try_deferred_loadmap_arm, declared above). If that
            // arm is refused, the refusal is LOUD (the composed layer's InstallHook
            // Warning + AddCallback Error pair plus the arm's own explicit Error) —
            // never a silent skip. The flag is set BEFORE the marshal consumer below can
            // arm the pump, so no drain can precede it.
            m_loadmap_deferred_arm_pending.store(true, std::memory_order_release);
            refused_log::eager_detour_block_skipped.store(true, std::memory_order_release);
            refused_log::loadmap_state.store(1, std::memory_order_release);
''',
'''        // [EARLY-4] (cap #4) PS5: this data-only branch is UNCONDITIONAL (the eager detour block
        // below is compiled out with #else), whatever the initializer's pump decision was; a
        // world where the pump is not pending (another game / engine version) logs it here and
        // still never reaches a code-detour registration.
        {
            if (!Unreal::R3MarshalPumpPending())
            {
                Output::send<LogLevel::Warning>(STR("[EARLY] pump-not-pending world: the eager detour block stays compiled out on PS5; the game-thread pump is the carrier marshal\\n"));
            }
            // Publish the final LoadMap state BEFORE the skipped flag: WORLD-EV owns LoadMap
            // delivery for the whole process (3 = no native delivery, never changes on PS5).
            refused_log::loadmap_state.store(3, std::memory_order_release);
            refused_log::eager_detour_block_skipped.store(true, std::memory_order_release);
            Output::send(STR("[EARLY] stage=loadmap-native state=off reason=ps5-no-code-detours owner=world-ev t_ms={}\\n"), early4::now_ms());
''')

edit('src/LuaMod.cpp', 'E1-startup-warning-text',
'''Lua-side LoadMap/InitGameState/BeginPlay callbacks are instead delivered by the data-only [WORLD-EV] object-index route (after the event; BeginPlay partial); a one-shot deferred LoadMap detour arm is still attempted when the marshal world is proven live and, only if it arms, takes over LoadMap delivery\\n"));
        }
        else
#endif
        {''',
'''Lua-side LoadMap/InitGameState/BeginPlay callbacks are instead delivered by the data-only [WORLD-EV] object-index route (after the event; BeginPlay partial; the map that is already loaded when mods start is replayed once, see [EARLY] stage=startup-replay); no LoadMap detour is ever attempted on PS5\\n"));
        }
#else
        {''')

edit('src/LuaMod.cpp', 'E1-eager-block-end',
'''        } // [R3][worker-l11-loadmap-guard r2] end of the eager detour-arm block (see gate above)
''',
'''        } // [R3][worker-l11-loadmap-guard r2] end of the eager detour-arm block (see gate above)
#endif // [EARLY-4] eager detour block: non-PS5 only
''')

# ---------------------------------------------------------------------------------------------
# E2  LuaMod.cpp: [WORLD-EV] startup replay (the map already loaded when mods start).
# ---------------------------------------------------------------------------------------------
edit('src/LuaMod.cpp', 'E2-snapshot-decl',
'''        auto r9_index_disabled() -> bool; // [R11-WORLD] UObjectGlobals.cpp
    }
''',
'''        auto r9_index_disabled() -> bool; // [R11-WORLD] UObjectGlobals.cpp
        auto r9_index_snapshot_live(std::vector<int32_t>& OutIdx, std::vector<void*>& OutObj, bool (*Want)(Unreal::UClass*)) -> bool; // [EARLY-4] UObjectGlobals.cpp
    }
''')

edit('src/LuaMod.cpp', 'E2-state',
'''        static R9Pacer s_pace{};
        static bool s_first_kind[4]{true, true, true, true};
''',
'''        static R9Pacer s_pace{};
        static bool s_first_kind[4]{true, true, true, true};
        // [EARLY-4] startup replay. On PC UE4SS the mods start during engine init, before the first
        // map, so a hook registered in main.lua sees that map's LoadMap / InitGameState / BeginPlay.
        // On PS5 the loader injects the runtime late (launch 14: ~69 s after the game process started),
        // and objects alive when the index is first used are never reported as new. So, once, when
        // the route first goes live AND world hooks were registered during startup (mods' main scripts and
        // any ExecuteInGameThread callback that ran before the first pump after the start gate), every
        // live Actor that already existed is fed
        // through observe() exactly like a new one, BEFORE any newer object: the map that was already
        // loaded (or loading) is delivered in the normal order (LoadMap pre -> InitGameState ->
        // BeginPlay -> LoadMap post), ahead of every later map. Objects of the unprocessed new batch are
        // excluded (they are observed live), so no object is fed twice. The route never goes live before
        // startup closes (feed + note_index check s_startup_closed), so nothing is delivered live while
        // mods are still starting and then replayed again. More than one live GameState
        // (a map change in progress) = ambiguous order: the replay is skipped, loudly (fail closed).
        // Not filtered: actors already marked PendingKill/Garbage but not yet collected. That internal
        // flag bit is engine-version dependent and has no two-source layout value yet (cap #4 NOTES).
        static std::atomic<bool> s_mods_started{false}; // set once by UE4SSProgram after the start gate
        static bool s_startup_closed{false};
        static bool s_startup_wanted{false};
        static int s_replay_state{0}; // 0 idle, 1 requested, 2 running, 3 finished/skipped
        static std::vector<int32_t> s_rp_idx{};
        static std::vector<void*> s_rp_obj{};
        static size_t s_rp_pos{0};
        static uint64_t s_rp_fed{0};
        static int64_t s_rp_start_ms{0};
        static bool s_in_replay{false};
        static bool s_first_map_noted{false};
''')

edit('src/LuaMod.cpp', 'E2-reset-batch-abort',
'''        static auto reset_batch() -> void // game thread, lock held (feed / refuse): forget hold + window
        {
            release(s_hold); s_hold_pos = 0;''',
'''        static auto reset_batch() -> void // game thread, lock held (feed / refuse): forget hold + window
        {
            if (s_replay_state == 1 || s_replay_state == 2) // [EARLY-4] nobody listens / route refused: no replay
            {
                s_replay_state = 3;
                release(s_rp_idx); release(s_rp_obj); s_rp_pos = 0;
                try { Output::send<LogLevel::Warning>(STR("[EARLY] stage=startup-replay decision=aborted fed={} (route refused or no world hooks left)\\n"), s_rp_fed); } catch (...) {}
            }
            release(s_hold); s_hold_pos = 0;''')

edit('src/LuaMod.cpp', 'E2-first-map-marker',
'''                Output::send(STR("[WORLD-EV] map change seen (object index): world='{}' held_joined={} -> LoadMap pre queued; this map's InitGameState/BeginPlay/LoadMap post follow when its load window closes (PS5: all after the load)\\n"),
                             world_ok ? world->GetName() : StringType{STR("?")}, s_window_joined);''',
'''                Output::send(STR("[WORLD-EV] map change seen (object index): world='{}' held_joined={} -> LoadMap pre queued; this map's InitGameState/BeginPlay/LoadMap post follow when its load window closes (PS5: all after the load)\\n"),
                             world_ok ? world->GetName() : StringType{STR("?")}, s_window_joined);
                if (!s_first_map_noted)
                {
                    s_first_map_noted = true;
                    Output::send(STR("[EARLY] stage=first-map-seen t_ms={} src={} world='{}'\\n"), r3_now_ms(),
                                 s_in_replay ? STR("replay") : STR("live"), world_ok ? world->GetName() : StringType{STR("?")});
                }''')

edit('src/LuaMod.cpp', 'E2-route-live-request',
'''                if (refused_log::world_route_state.exchange(1, std::memory_order_acq_rel) == 0)
                {
                    try { Output::send(STR("[WORLD-EV] route live: object index + engine classes Actor/GameModeBase/GameStateBase/World resolved and class-checked (tries={}, waited_ms={}); LoadMap/InitGameState/BeginPlay Lua hooks deliver after the event on the game-thread pump\\n"), s_resolve_tries, now - s_first_wanted_ms); } catch (...) {}
                }''',
'''                if (refused_log::world_route_state.exchange(1, std::memory_order_acq_rel) == 0)
                {
                    try { Output::send(STR("[WORLD-EV] route live: object index + engine classes Actor/GameModeBase/GameStateBase/World resolved and class-checked (tries={}, waited_ms={}); LoadMap/InitGameState/BeginPlay Lua hooks deliver after the event on the game-thread pump\\n"), s_resolve_tries, now - s_first_wanted_ms); } catch (...) {}
                    try { Output::send(STR("[EARLY] stage=world-route-live t_ms={} waited_ms={}\\n"), now, now - s_first_wanted_ms); } catch (...) {} // [EARLY-4]
                }''')

edit('src/LuaMod.cpp', 'E2-replay-functions',
'''        // Lock held, every pump while the route is live: close a quiet window, expire the hold list.
        static auto tick() -> void''',
'''        // [EARLY-4] every feed call until startup closes (game thread, m_thread_actions_mutex held):
        // a world hook registered before the first feed AFTER the start gate counts as a startup
        // registration (mods' main scripts, and ExecuteInGameThread callbacks, which run before the
        // feed in the same pump: the PS5 equivalent of "before the first map"). A pump that fires
        // while mods are still starting keeps the window open.
        static auto note_feed_locked() -> void
        {
            if (s_startup_closed) { return; }
            if (any_world_callbacks()) { s_startup_wanted = true; }
            if (!s_mods_started.load(std::memory_order_acquire)) { return; }
            s_startup_closed = true;
            try
            {
                Output::send(STR("[EARLY] stage=startup-hooks t_ms={} loadmap_pre={} loadmap_post={} initgamestate={} beginplay={}\\n"), r3_now_ms(),
                             LuaMod::m_load_map_pre_callbacks.size(), LuaMod::m_load_map_post_callbacks.size(),
                             LuaMod::m_init_game_state_pre_callbacks.size() + LuaMod::m_init_game_state_post_callbacks.size(),
                             LuaMod::m_begin_play_pre_callbacks.size() + LuaMod::m_begin_play_post_callbacks.size());
            }
            catch (...)
            {
            }
        }

        // Snapshot predicate (runs under the index lock, once per distinct class of a live object).
        static auto replay_want(Unreal::UClass* cls) -> bool
        {
            return cls != nullptr && s_actor != nullptr && chain_has(static_cast<Unreal::UStruct*>(cls), s_actor);
        }

        // Decided once: the route is live and startup has closed. Startup registrations -> replay requested.
        static auto maybe_request_replay() -> void
        {
            if (s_replay_state != 0 || !s_startup_closed || refused_log::world_route_state.load(std::memory_order_acquire) != 1) { return; }
            s_replay_state = s_startup_wanted ? 1 : 3;
            try { Output::send(STR("[EARLY] stage=replay-request t_ms={} startup_hooks={} replay={}\\n"), r3_now_ms(), s_startup_wanted ? 1 : 0,
                               s_replay_state == 1 ? STR("requested") : STR("not-startup")); } catch (...) {}
        }

        // Game thread, NOT holding m_thread_actions_mutex (takes the index lock). pending_idx[from..]
        // are new objects collected but not matched yet: they are observed live, never replayed.
        static auto maybe_start_replay(const std::vector<int32_t>& pending_idx, size_t from) -> void
        {
            maybe_request_replay();
            if (s_replay_state != 1) { return; }
            s_replay_state = 2;
            s_rp_pos = 0;
            s_rp_fed = 0;
            s_rp_start_ms = r3_now_ms();
            size_t baseline{0}, excluded{0}, gamestates{0};
            bool ok{false};
            try
            {
                ok = Unreal::UObjectGlobals::r9_index_snapshot_live(s_rp_idx, s_rp_obj, &replay_want);
                if (ok)
                {
                    baseline = s_rp_idx.size();
                    std::vector<int32_t> unmatched{};
                    if (from < pending_idx.size()) { unmatched.assign(pending_idx.begin() + static_cast<std::ptrdiff_t>(from), pending_idx.end()); }
                    std::sort(unmatched.begin(), unmatched.end());
                    size_t w = 0;
                    for (size_t r = 0; r < s_rp_idx.size(); ++r)
                    {
                        if (std::binary_search(unmatched.begin(), unmatched.end(), s_rp_idx[r])) { ++excluded; continue; }
                        s_rp_idx[w] = s_rp_idx[r];
                        s_rp_obj[w] = s_rp_obj[r];
                        ++w;
                    }
                    s_rp_idx.resize(w);
                    s_rp_obj.resize(w);
                    for (void* p : s_rp_obj)
                    {
                        auto* o = static_cast<Unreal::UObject*>(p);
                        if (o->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_ClassDefaultObject | Unreal::RF_ArchetypeObject))) { continue; }
                        if (chain_has(static_cast<Unreal::UStruct*>(o->GetClassPrivate()), s_game_state)) { ++gamestates; }
                    }
                }
            }
            catch (...)
            {
                ok = false;
            }
            const char* decision = !ok ? "skip-index-unusable" : (gamestates > 1 ? "skip-ambiguous" : (s_rp_idx.empty() ? "none" : "replay"));
            if (!ok || gamestates > 1 || s_rp_idx.empty())
            {
                release(s_rp_idx); release(s_rp_obj);
                s_replay_state = 3;
            }
            try
            {
                StringType wide{};
                for (const char* p = decision; *p != '\\0'; ++p) { wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                const int64_t t = r3_now_ms();
                if (gamestates > 1 || !ok)
                {
                    Output::send<LogLevel::Warning>(STR("[EARLY] stage=startup-replay t_ms={} decision={} candidates={} baseline={} excluded_new={} gamestates={} snapshot_ms={}\\n"),
                                                    t, wide, s_rp_idx.size(), baseline, excluded, gamestates, t - s_rp_start_ms);
                }
                else
                {
                    Output::send(STR("[EARLY] stage=startup-replay t_ms={} decision={} candidates={} baseline={} excluded_new={} gamestates={} snapshot_ms={}\\n"),
                                 t, wide, s_rp_idx.size(), baseline, excluded, gamestates, t - s_rp_start_ms);
                }
            }
            catch (...)
            {
            }
        }

        // Game thread, m_thread_actions_mutex held. Feeds replayed actors through observe() under the
        // matcher's budget. true = nothing (left) to replay: newer objects may be matched now.
        static auto replay_step(std::chrono::steady_clock::time_point deadline) -> bool
        {
            if (s_replay_state != 2) { return true; }
            s_in_replay = true;
            while (s_rp_pos < s_rp_idx.size())
            {
                if ((s_rp_pos & 63) == 0 && s_rp_pos > 0 && std::chrono::steady_clock::now() > deadline) { s_in_replay = false; return false; }
                const int32_t idx = s_rp_idx[s_rp_pos];
                void* const expected = s_rp_obj[s_rp_pos];
                ++s_rp_pos;
                Unreal::FUObjectItem* item = Unreal::FUObjectArray::IndexToObject(idx);
                if (!item || item->IsUnreachable()) { continue; }
                Unreal::UObject* object = item->GetUObject();
                if (!object || static_cast<void*>(object) != expected) { continue; } // freed / slot reused since the snapshot
                observe(object, idx);
                ++s_rp_fed;
            }
            s_in_replay = false;
            s_replay_state = 3;
            release(s_rp_idx); release(s_rp_obj); s_rp_pos = 0;
            try
            {
                const int64_t t = r3_now_ms();
                Output::send(STR("[EARLY] stage=startup-replay-done t_ms={} fed={} took_ms={} window_open={} held={}\\n"), t, s_rp_fed, t - s_rp_start_ms,
                             s_window_open ? 1 : 0, s_hold.size() - s_hold_pos);
            }
            catch (...)
            {
            }
            return true;
        }

        static auto replay_running() -> bool { return s_replay_state == 2; }

        // Lock held, every pump while the route is live: close a quiet window, expire the hold list.
        static auto tick() -> void''')

edit('src/LuaMod.cpp', 'E2-first-delivery-marker',
'''                    try { Output::send(STR("[WORLD-EV] first delivery kind={} callbacks={} object='{}'\\n"), kind_name(ev.kind), s_pre.size() + s_post.size(), short_name(alive ? ev.obj : nullptr)); } catch (...) {}''',
'''                    try { Output::send(STR("[WORLD-EV] first delivery kind={} callbacks={} object='{}'\\n"), kind_name(ev.kind), s_pre.size() + s_post.size(), short_name(alive ? ev.obj : nullptr)); } catch (...) {}
                    try { Output::send(STR("[EARLY] stage=first-delivery kind={} t_ms={}\\n"), kind_name(ev.kind), r3_now_ms()); } catch (...) {} // [EARLY-4]''')

edit('src/LuaMod.cpp', 'E2-mods-started-setter',
'''    } // namespace world_ev

    static auto r9_feed_notify_from_index() -> void
''',
'''    } // namespace world_ev

    // [EARLY-4] called once by UE4SSProgram right after the "[L25] start gate" line (every mod's main
    // script has returned): the startup-registration window closes at the next feed.
    auto early4_mods_started() noexcept -> void
    {
        world_ev::s_mods_started.store(true, std::memory_order_release);
    }

    static auto r9_feed_notify_from_index() -> void
''')

edit('src/LuaMod.cpp', 'E2-feed-first-call',
'''            world_ev::s_wanted = world_ev::wanted_locked(); // [WORLD-EV] map/InitGameState/BeginPlay hooks also read the index
''',
'''            world_ev::s_wanted = world_ev::wanted_locked(); // [WORLD-EV] map/InitGameState/BeginPlay hooks also read the index
            world_ev::note_feed_locked(); // [EARLY-4] startup registrations = those present at the first feed after the start gate
''')

edit('src/LuaMod.cpp', 'E2-feed-replay-start-top',
'''        if (world_ev::s_wanted) { (void)world_ev::ensure_ready(); } // [WORLD-EV] outside the lock (class lookups)
''',
'''        // [WORLD-EV] outside the lock (class lookups). [EARLY-4] never before startup closes (first feed after
        // the start gate): objects matched while mods are still starting are not delivered live, the startup
        // replay delivers them once instead (a live delivery + the replay would deliver that map twice).
        if (world_ev::s_wanted && world_ev::s_startup_closed) { (void)world_ev::ensure_ready(); }
        world_ev::maybe_start_replay(s_backlog_idx, s_backlog_pos); // [EARLY-4] went live above: unmatched backlog stays live-only
''')

edit('src/LuaMod.cpp', 'E2-note-index-startup-gate',
'''            if (s_wanted && refused_log::world_route_state.load(std::memory_order_acquire) == 0) { (void)ensure_ready(); }
''',
'''            // [EARLY-4] same rule as the feed: the route never goes live before startup closes
            if (s_wanted && s_startup_closed && refused_log::world_route_state.load(std::memory_order_acquire) == 0) { (void)ensure_ready(); }
''')

edit('src/LuaMod.cpp', 'E2-feed-replay-start-collect',
'''            world_ev::note_index(r9_collected); // [WORLD-EV]
''',
'''            world_ev::note_index(r9_collected); // [WORLD-EV]
            if (r9_collected) { world_ev::maybe_start_replay(s_backlog_idx, 0); } // [EARLY-4] went live in note_index: this batch is live-only
''')

edit('src/LuaMod.cpp', 'E2-feed-empty-batch',
'''            if (s_backlog_idx.empty()) { return; }
            static bool s_logged{false};
            if (!s_logged)
''',
'''            if (s_backlog_idx.empty() && !world_ev::replay_running()) { return; } // [EARLY-4] a replay may run with no new objects
            static bool s_logged{false};
            if (!s_backlog_idx.empty() && !s_logged)
''')

edit('src/LuaMod.cpp', 'E2-feed-replay-step',
'''        if (s_backlog_pos >= s_backlog_idx.size()) { return; }
        std::chrono::steady_clock::time_point budget_end{}; // ~1 ms per ~16 ms window ([R9-PACE])
        if (!s_r9_pace_match.begin(std::chrono::microseconds(1000), budget_end)) { return; }
        const R9PaceEnd r9_pace_end{s_r9_pace_match};
        std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
''',
'''        if (s_backlog_pos >= s_backlog_idx.size() && !world_ev::replay_running()) { return; }
        std::chrono::steady_clock::time_point budget_end{}; // ~1 ms per ~16 ms window ([R9-PACE])
        if (!s_r9_pace_match.begin(std::chrono::microseconds(1000), budget_end)) { return; }
        const R9PaceEnd r9_pace_end{s_r9_pace_match};
        std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
        // [EARLY-4] the startup replay goes first (older map before newer objects); newer objects wait
        if (!world_ev::replay_step(budget_end)) { return; }
''')

# ---------------------------------------------------------------------------------------------
# E3  UObjectGlobals.cpp: read-only snapshot of the index's live objects (for the replay).
# ---------------------------------------------------------------------------------------------
edit('src/UObjectGlobals.cpp', 'E3-snapshot-live',
'''    // [R9-NOTIFY] no registrations left: stop collecting.
    auto r9_index_notify_disarm() -> void''',
'''    // [EARLY-4] (cap #4) read-only: every slot the index currently holds whose object is still live
    // (same slot, same pointer, reachable), restricted to classes Want() accepts. Want is called at most
    // once per distinct class, and only with the class the LIVE object names right now: a slot freed and
    // refilled at the same address since the last sync (ClassAt may still name the old, possibly unloaded
    // class) is skipped; the next collect reports it as new.
    // Index order. false = index unusable (not built / switched off): nothing is returned.
    auto r9_index_snapshot_live(std::vector<int32_t>& OutIdx, std::vector<void*>& OutObj, bool (*Want)(UClass*)) -> bool
    {
        OutIdx.clear();
        OutObj.clear();
        if (Want == nullptr) { return false; }
        r9idx::State& S = *r9idx::g_state;
        std::lock_guard Lock{S.Mutex};
        if (S.Disabled || !S.Built) { return false; }
        try
        {
            std::vector<int8_t> Verdict(S.Classes.size(), int8_t{-1}); // per class position: -1 unknown
            const size_t Mask = S.HashKeys.empty() ? 0 : S.HashKeys.size() - 1;
            const int32_t Num = S.SyncedNum;
            for (int32_t i = 0; i < Num; ++i)
            {
                UObject* const Object = S.ObjAt[i];
                if (Object == nullptr || r9idx::LiveAt(i) != Object) { continue; }
                UClass* const Class = S.ClassAt[i];
                if (Class == nullptr || Object->GetClassPrivate() != Class) { continue; } // stale class entry: never hand it to Want
                int32_t c = -1;
                if (!S.HashKeys.empty())
                {
                    size_t p = r9idx::HashPos(Class, Mask);
                    while (S.HashKeys[p] != nullptr)
                    {
                        if (S.HashKeys[p] == Class) { c = S.HashVals[p]; break; }
                        p = (p + 1) & Mask;
                    }
                }
                bool bWant;
                if (c >= 0 && static_cast<size_t>(c) < Verdict.size())
                {
                    if (Verdict[c] < 0) { Verdict[c] = Want(Class) ? int8_t{1} : int8_t{0}; }
                    bWant = Verdict[c] == 1;
                }
                else
                {
                    bWant = Want(Class); // not listed (never expected): decide without memo
                }
                if (!bWant) { continue; }
                OutIdx.push_back(i);
                OutObj.push_back(Object);
            }
            return true;
        }
        catch (...)
        {
            OutIdx.clear();
            OutObj.clear();
            return false;
        }
    }

    // [R9-NOTIFY] no registrations left: stop collecting.
    auto r9_index_notify_disarm() -> void''')

# ---------------------------------------------------------------------------------------------
# E4  UE4SSProgram.cpp: [EARLY] mods-started marker (log + diagnostic sink, same clock check).
# ---------------------------------------------------------------------------------------------
edit('src/UE4SSProgram.cpp', 'E4-mods-started',
'''            Output::send(STR("[L25] start gate: {} started mod(s){}\\n"),
                         started_mod_count, start_ok ? STR("") : STR(" (start exception)"));
''',
'''            Output::send(STR("[L25] start gate: {} started mod(s){}\\n"),
                         started_mod_count, start_ok ? STR("") : STR(" (start exception)"));
#ifdef __PROSPERO__
            {
                // [EARLY-4] (cap #4) one-shot timeline marker: steady-clock ms in the log, its low 32 bits
                // into the diagnostic sink (whose own t_ms is CLOCK_MONOTONIC) -> same-run clock proof.
                const int64_t early_t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                diagnostic_event("early_mods_started", static_cast<unsigned int>(static_cast<uint64_t>(early_t) & 0xFFFF'FFFFu));
                Output::send(STR("[EARLY] stage=mods-started t_ms={} mods={}\\n"), early_t, started_mod_count);
                early4_mods_started(); // LuaMod.cpp [WORLD-EV]: the startup-registration window closes at the next feed
            }
#endif
''')


edit('src/UE4SSProgram.cpp', 'E4-decl',
'''extern "C" void diagnostic_event(const char* tag, unsigned int value);
''',
'''extern "C" void diagnostic_event(const char* tag, unsigned int value);
namespace RC
{
    auto early4_mods_started() noexcept -> void; // [EARLY-4] LuaMod.cpp (PS5 world-event route)
}
''')

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', required=True)
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--in-place', action='store_true', help='allow writing the real candidate tree (integrator only)')
    ap.add_argument('--allow-drift', action='store_true', help='tree matches no known base: patch by exact-once anchors only')
    a = ap.parse_args()
    tree = Path(a.tree).resolve()
    if tree == CANDIDATE.resolve() and a.write and not a.in_place:
        print('REFUSED: --write on the real candidate tree needs --in-place (integrator decision)')
        return 1
    texts = {}
    hashes = {}
    for rel in FILES:
        p = tree / rel
        if not p.is_file():
            print('REFUSED: missing %s' % p)
            return 1
        hashes[rel] = sha(p)
        texts[rel] = p.read_text()
    base = next((name for name, pins in BASES.items() if all(hashes[f] == pins[f] for f in FILES)), None)
    if base is None and not a.allow_drift:
        for rel in FILES:
            print('  %s sha256 %s' % (rel, hashes[rel]))
        print('REFUSED: tree matches no known base (%s); re-run with --allow-drift to patch by exact-once anchors, then rerun tests/run_tests.sh and the syntax check' % ', '.join(BASES))
        return 1
    print('base: %s' % (base or 'DRIFTED (anchors only)'))
    checked = 0
    for f, tag, old, new in EDITS:
        t = texts[f]
        n_old = t.count(old)
        if n_old != 1:
            already = t.count(new) >= 1
            print('REFUSED: %s %s anchor count=%d%s' % (f, tag, n_old, ' (already applied?)' if already else ''))
            return 1
        texts[f] = t.replace(old, new, 1)
        checked += 1
        print('ok %-36s %s' % (tag, f))
    print('checked %d replacements in %d files' % (checked, len(texts)))
    if checked == 0:
        print('FAIL: zero replacements checked')
        return 1
    if a.write:
        for rel, t in texts.items():
            (tree / rel).write_text(t)
            print('wrote %s sha256 %s' % (rel, sha(tree / rel)))
    else:
        print('dry run (no --write): nothing written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
