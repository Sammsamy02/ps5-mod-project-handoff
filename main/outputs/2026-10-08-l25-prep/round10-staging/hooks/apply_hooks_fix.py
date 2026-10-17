#!/usr/bin/env python3
"""apply_hooks_fix.py — hooks track r3 (Claude, after the re-review BLOCK). Apply AFTER apply_hooks.py.

1. (medium) Spawned actors of the current map are not handed to BeginPlay callbacks at once: they settle
   >= 50 ms first (a nested carrier pump inside SpawnActor, e.g. BlueprintUpdateAnimation from InitAnim,
   could otherwise deliver before ExecuteConstruction / BeginPlay). Route notes and stats updated.
2. (low) Any GameModeBase seen while no load window is open starts a new load (catches a same-map reload
   that reuses the old level address); a different GameState object at the same level opens a new window.
3. (low) Engine class lookup capped at 3 tries (2 s apart), then refused at once (native classes exist from
   boot or never); the 60 s limit stays for the object-index-unusable case.
Exact-once replacements; argv[1] = root (default: the real candidate)."""
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
P = ROOT + '/src/LuaMod.cpp'

R = [
    # 1. settle FIFO
    ("""        // ordered delivery queue (FIFO from s_queue_pos)
        static std::vector<Ev> s_queue{};
        static size_t s_queue_pos{0};""",
     """        // ordered delivery queue (FIFO from s_queue_pos)
        static std::vector<Ev> s_queue{};
        static size_t s_queue_pos{0};
        // actors spawned during play in the current map wait kSettleMs before BeginPlay delivery (a nested
        // carrier pump inside SpawnActor must never hand a half-constructed actor to a mod); t_ms = seen time
        static std::vector<Held> s_settle{};
        static size_t s_settle_pos{0};
        constexpr int64_t kSettleMs = 50;
        constexpr int kMaxResolveTries = 3;"""),
    ("""            return (s_hold.size() - s_hold_pos) + s_win_gm.size() + s_win_actor.size() + (s_queue.size() - s_queue_pos);""",
     """            return (s_hold.size() - s_hold_pos) + s_win_gm.size() + s_win_actor.size() + (s_queue.size() - s_queue_pos) +
                   (s_settle.size() - s_settle_pos);"""),
    ("""            release(s_hold); s_hold_pos = 0;
            release(s_win_gm); release(s_win_actor);
            s_window_open = false;
            s_map_level = nullptr;""",
     """            release(s_hold); s_hold_pos = 0;
            release(s_settle); s_settle_pos = 0;
            release(s_win_gm); release(s_win_actor);
            s_window_open = false;
            s_map_level = nullptr;"""),
    ("""                if (want_bp) { queue_actor_event(bp_ev); } // spawned during play in the current map
                return;""",
     """                if (want_bp) // spawned during play in the current map: settle first (see s_settle)
                {
                    compact(s_settle, s_settle_pos);
                    (void)push_safe(s_settle, Held{bp_ev, level, now, false});
                }
                return;"""),
    ("""            if (!s_window_open && s_hold_pos >= s_hold.size()) { return; }
            const int64_t now = r3_now_ms();""",
     """            if (!s_window_open && s_hold_pos >= s_hold.size() && s_settle_pos >= s_settle.size()) { return; }
            const int64_t now = r3_now_ms();
            while (s_settle_pos < s_settle.size() && now - s_settle[s_settle_pos].t_ms >= kSettleMs)
            {
                queue_actor_event(s_settle[s_settle_pos++].ev); // alive + class re-checked at delivery
            }
            compact(s_settle, s_settle_pos);"""),
    ("""                Output::send(STR("[WORLD-EV] stats maps={} gamemodes={} actors={} lua_calls={} skipped_dead={} streamed_unreported={} late_after_post={} held={} window_open={} dropped={} errors={} queued={} paced_skips={}\\n"),
                             s_maps, s_gms, s_actors, s_delivered, s_skipped_dead, s_streamed_unreported, s_late_after_post,
                             s_hold.size() - s_hold_pos, s_window_open ? 1 : 0, s_dropped, s_errors, s_queue.size() - s_queue_pos, s_pace.skipped);""",
     """                Output::send(STR("[WORLD-EV] stats maps={} gamemodes={} actors={} lua_calls={} skipped_dead={} streamed_unreported={} late_after_post={} held={} settling={} window_open={} dropped={} errors={} queued={} paced_skips={}\\n"),
                             s_maps, s_gms, s_actors, s_delivered, s_skipped_dead, s_streamed_unreported, s_late_after_post,
                             s_hold.size() - s_hold_pos, s_settle.size() - s_settle_pos, s_window_open ? 1 : 0, s_dropped, s_errors,
                             s_queue.size() - s_queue_pos, s_pace.skipped);"""),
    ("""            refused_log::route_note(4, "RegisterBeginPlayPreHook", "after-BeginPlay-ran(PC:before);PARTIAL:streamed-level-actors-not-reported(counted-gap)");""",
     """            refused_log::route_note(4, "RegisterBeginPlayPreHook", "after-the-event(PC:before);spawned:>=50ms-after-spawn;map-load-actors:when-the-load-window-closes;PARTIAL:streamed-level-actors-not-reported(counted-gap)");"""),
    ("""            refused_log::route_note(5, "RegisterBeginPlayPostHook", "next-pump-after-spawn(map-load-actors:when-the-load-window-closes);PARTIAL:streamed-level-actors-not-reported(counted-gap)");""",
     """            refused_log::route_note(5, "RegisterBeginPlayPostHook", "spawned:>=50ms-after-spawn;map-load-actors:when-the-load-window-closes;PARTIAL:streamed-level-actors-not-reported(counted-gap)");"""),
    # 2. GameMode with no open window = a new load; a different GameState object at the same level = a new map
    ("""            if (is_gm && !in_window && level != s_map_level)
            {""",
     """            if (is_gm && !in_window)
            {"""),
    ("""            if (s_window_open && level == s_map_level) { return; } // a second GameState in the same load: no second map""",
     """            if (s_window_open && level == s_map_level && gs.obj == s_window_gs.obj && gs.index == s_window_gs.index) { return; } // same GameState again: no second map"""),
    # r3 review lows: flush settling spawns of the old map before the new map's LoadMap pre; fix the comment
    ("""            close_window("next-map", now);
            s_map_level = level;
            s_window_open = true;""",
     """            close_window("next-map", now);
            while (s_settle_pos < s_settle.size()) { queue_actor_event(s_settle[s_settle_pos++].ev); } // old map's spawns first
            compact(s_settle, s_settle_pos);
            s_map_level = level;
            s_window_open = true;"""),
    ("""                // a GameMode of another level: a new map load is under way (a late GameMode of the
                // current level just waits in the hold list; a reused level address is caught when
                // the new GameState appears)""",
     """                // a GameMode with no open load window: a new map load is under way (also catches a
                // same-map reload that reuses the old level address); until its GameState arrives,
                // its actors wait in the hold list"""),
    # 3. cap the class lookup
    ("""                if (!s_classes_ok) { s_actor = s_game_mode = s_game_state = s_world = nullptr; }
            }""",
     """                if (!s_classes_ok) { s_actor = s_game_mode = s_game_state = s_world = nullptr; }
                if (!s_classes_ok && s_resolve_tries >= kMaxResolveTries)
                {
                    // native engine classes exist from boot or never: more full-array lookups cannot help
                    refuse(s_class_check_failed ? "engine-class-check-failed" : "engine-class-lookup-failed");
                    return false;
                }
            }"""),
]

s = open(P).read()
for old, new in R:
    n = s.count(old)
    if n != 1:
        sys.exit('apply_hooks_fix: anchor count %d:\n%s' % (n, old[:160]))
    s = s.replace(old, new)
open(P, 'w').write(s)
print('apply_hooks_fix: %d edits applied to %s' % (len(R), P))
