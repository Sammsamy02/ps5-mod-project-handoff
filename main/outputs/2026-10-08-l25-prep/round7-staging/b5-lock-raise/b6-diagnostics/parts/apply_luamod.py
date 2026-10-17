#!/usr/bin/env python3
"""Builds work/src/LuaMod.cpp from base/src/LuaMod.cpp (B5-applied) by the B6 edits.
Every edit asserts its anchor count; prints counts; exits 1 on any mismatch."""
import re, sys, os
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
src = open(os.path.join(root, 'base/src/LuaMod.cpp'), encoding='utf-8').read()
region = open(os.path.join(here, 'regionB.inc.txt'), encoding='utf-8').read()
s = src
counts = {}

def sub_once(old, new, label):
    global s
    n = s.count(old)
    if n != 1:
        print('FAIL anchor %s count=%d' % (label, n)); sys.exit(1)
    s = s.replace(old, new); counts[label] = 1

# 1. lock sites
pat = re.compile(r'std::lock_guard<std::recursive_mutex> guard\{((?:LuaMod::)?m_thread_actions_mutex)\};')
s, n = pat.subn(r'b6diag::ActionsLock guard{\1, __LINE__};', s)
counts['lock_guard sites'] = n
old_u = 'std::unique_lock<std::recursive_mutex> Lock{LuaMod::m_thread_actions_mutex};'
sub_once(old_u, 'b6diag::ActionsLock Lock{LuaMod::m_thread_actions_mutex, __LINE__};', 'dispatch scope unique_lock')

# 2. region B + glue, before L23GameThreadProbe (first thing inside namespace RC)
glue = '''
    // [B6-GLUE-BEGIN]
    namespace
    {
    namespace b6diag
    {
        // Glue for the CORE region above (kept outside it: needs the engine/logging headers).
        auto emit_line(const char* line) -> void
        {
            StringType wide{};
            for (const char* p = line; p != nullptr && *p != '\\0'; ++p)
            {
                wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p)));
            }
            L22Diagnostics::observe([&]() { Output::send(STR("{}\\n"), wide); });
        }
        auto on_game_thread() -> bool
        {
#if defined(__PROSPERO__)
            return Unreal::IsInGameThreadRaw();
#else
            // Host-only branch: GetGameThreadId() can throw before engine init, and this runs
            // inside ActionsLock's constructor after the lock is taken, so it must not throw.
            try { return std::this_thread::get_id() == Unreal::GetGameThreadId(); } catch (...) { return false; }
#endif
        }
        // Mod names are plain ASCII; anything else prints as '?'.
        auto ascii_name(char* out, size_t n, StringViewType name) -> void
        {
            size_t i = 0;
            for (; i + 1 < n && i < name.size(); ++i)
            {
                const auto c = static_cast<unsigned long>(name[i]);
                out[i] = (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '?';
            }
            out[i] = '\\0';
        }
    }
    }
    // [B6-GLUE-END]
'''
anchor = 'namespace RC\n{\n#if defined(__PROSPERO__)\n    namespace L23GameThreadProbe {'
sub_once(anchor, 'namespace RC\n{' + region + glue + '#if defined(__PROSPERO__)\n    namespace L23GameThreadProbe {', 'region B anchor')

# 3. constructor registration
sub_once('''        , m_lua(LuaMadeSimple::new_state())
    {
        if (m_native_source)''', '''        , m_lua(LuaMadeSimple::new_state())
    {
        {
            // [B6] diagnostics only: name this mod's Lua state for [LOCKLEAK] / [PERF].
            b6diag::install_hooks_once();
            char b6_name[40];
            b6diag::ascii_name(b6_name, sizeof(b6_name), get_name());
            b6diag::register_mod(m_lua.get_lua_state(), b6_name);
        }
        if (m_native_source)''', 'ctor')

# 4. FindFirstOf / FindAllOf timers
sub_once('''        lua.register_function("FindFirstOf", [](const LuaMadeSimple::Lua& lua) -> int {
''', '''        lua.register_function("FindFirstOf", [](const LuaMadeSimple::Lua& lua) -> int {
            b6diag::FindTimer b6_find_timer{b6diag::g_find_first_calls, b6diag::g_find_first_us}; // [B6] diagnostics only
''', 'FindFirstOf')
sub_once('''        lua.register_function("FindAllOf", [](const LuaMadeSimple::Lua& lua) -> int {
''', '''        lua.register_function("FindAllOf", [](const LuaMadeSimple::Lua& lua) -> int {
            b6diag::FindTimer b6_find_timer{b6diag::g_find_all_calls, b6diag::g_find_all_us}; // [B6] diagnostics only
''', 'FindAllOf')

# 5. widget line rate limit
sub_once('''            if (created_object)
            {
                L22Diagnostics::observe([&]() {
                    Output::send(STR("[l22] widget/object constructed via StaticConstructObject class={} object={} (non-null return)\\n"),
                                 static_cast<void*>(param_class), static_cast<void*>(created_object));
                });
            }''', '''            if (created_object)
            {
                // [B6] at most kWidgetLinesPerSecond of these lines per second; the rest are
                // counted and reported by one "[l22] widget/object construct summary" line.
                const auto b6_widget_verdict = b6diag::widget_gate(b6diag::now_ms());
                b6diag::emit_widget_summary(b6_widget_verdict);
                if (b6_widget_verdict.show)
                {
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] widget/object constructed via StaticConstructObject class={} object={} (non-null return)\\n"),
                                     static_cast<void*>(param_class), static_cast<void*>(created_object));
                    });
                }
            }''', 'widget')

# 6. pump glue before process_simple_actions
pump = '''
    // [B6] The 1 Hz game-thread line: [PERF], the pump-side [MEM], stuck-lock waiters, the
    // widget summary and the persistent-lock-depth check. Called from the two pumps BEFORE
    // they take the dispatch lock, so it keeps running when another lock scope is stuck.
    // It makes no Lua call and takes no tracked lock (queue sizes are read under a plain
    // try_lock; "q=busy" when another thread holds it).
    // [B6-PUMP-BEGIN]
    static std::atomic<int> g_b6_gc_kb_cache{0};      // last gc_kb printed by the ExecuteInGameThread [MEM] path
    static std::atomic<b6diag::i64> g_b6_perf_last_ms{0};
    static std::atomic<b6diag::i64> g_b6_mem_pump_ms{0};
    [[maybe_unused]] static auto b6_pump_mem_recent() -> bool
    {
        const b6diag::i64 at = g_b6_mem_pump_ms.load(std::memory_order_relaxed);
        return at != 0 && b6diag::now_ms() - at < 2500;
    }
    [[maybe_unused]] static auto b6_pump_1hz() -> void
    {
        try
        {
            const b6diag::i64 now = b6diag::now_ms();
            if (!b6diag::perf_gate(g_b6_perf_last_ms, now, 900))
            {
                return;
            }
            static b6diag::i64 s_report_ms = 0;
            static int s_last_depth = 0;
            const int depth = LuaMadeSimple::diag_scope_depth();
#if defined(__PROSPERO__)
            const bool dispatching = NativeLuaActionDispatching;
#else
            const bool dispatching = false;
#endif
            if (b6diag::pump_depth_stuck(depth, dispatching, s_last_depth))
            {
                b6diag::on_leak(nullptr, "pump", "pump", "game-thread lock depth unchanged between two pumps and no dispatch is running", depth, depth);
            }
            b6diag::QueueDepths queues{};
            if (LuaMod::m_thread_actions_mutex.try_lock())
            {
                queues.ok = true;
                queues.game = LuaMod::m_game_thread_actions.size();
                queues.game_pending = LuaMod::m_pending_game_thread_actions.size();
                queues.delayed = LuaMod::m_delayed_game_thread_actions.size();
                queues.delayed_pending = LuaMod::m_pending_delayed_game_thread_actions.size();
                queues.tick = LuaMod::m_engine_tick_actions.size();
                queues.tick_pending = LuaMod::m_pending_engine_tick_actions.size();
                LuaMod::m_thread_actions_mutex.unlock();
            }
            b6diag::PerfInput perf{};
            b6diag::collect_perf(perf, now, s_report_ms, queues, depth);
            char line[900];
            b6diag::format_perf(line, sizeof(line), perf);
            b6diag::emit_line(line);

            char waits[4][260];
            const int stuck = b6diag::scan_stuck_waiters(now, waits, 4);
            for (int i = 0; i < stuck; ++i)
            {
                b6diag::emit_line(waits[i]);
            }
            b6diag::emit_widget_summary(b6diag::g_widget_limiter.flush(now, 1000));

            // [MEM] from the pump: no Lua call (gc_kb is the last value the other path saw).
            const auto snap = LuaMadeSimple::query_lua_memory(nullptr);
            char mem[520];
            std::snprintf(mem, sizeof(mem),
                          "[MEM] state=pump gc_kb=%d live_kb=%zu blocks=%zu allocs=%zu fails=%zu fail_o=%zu fail_n=%zu arena_cap_kb=%zu arena_used_kb=%zu "
                          "arena_ext_kb=%zu errs=%zu cxx=%zu src=pump",
                          g_b6_gc_kb_cache.load(std::memory_order_relaxed), snap.live_bytes / 1024, snap.live_blocks, snap.total_alloc_calls,
                          snap.failed_calls, snap.last_fail_osize, snap.last_fail_nsize, snap.arena_capacity_bytes / 1024, snap.arena_used_bytes / 1024,
                          snap.arena_external_bytes / 1024, snap.raised_errors, snap.cxx_exceptions);
            b6diag::emit_line(mem);
            g_b6_mem_pump_ms.store(now, std::memory_order_relaxed);
            diagnostic_event("lua_mem_gc_kb", static_cast<unsigned int>(g_b6_gc_kb_cache.load(std::memory_order_relaxed) < 0 ? 0 : g_b6_gc_kb_cache.load(std::memory_order_relaxed)));
            diagnostic_event("lua_mem_fails", static_cast<unsigned int>(snap.failed_calls));
        }
        catch (...)
        {
        }
    }
    // [B6-PUMP-END]

    static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void
'''
sub_once('''
    static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void
''', pump, 'pump glue')

# 7. pump call sites
sub_once('''                diagnostic_event("r3_marshal_pumped", static_cast<unsigned int>(pump_now_ms & 0xFFFF'FFFF));
            }
        }''', '''                diagnostic_event("r3_marshal_pumped", static_cast<unsigned int>(pump_now_ms & 0xFFFF'FFFF));
                b6_pump_1hz(); // [B6] diagnostics only; runs before the dispatch lock below
            }
        }''', 'marshal pump call')
sub_once('''                if (!Unreal::IsInGameThreadRaw())
                {
                    return;
                }
                NativeLuaActionDispatchScope action_dispatch;
                if (!action_dispatch.owns_dispatch())
                {
                    return;
                }
                // The marshal world is PROVEN live''', '''                if (!Unreal::IsInGameThreadRaw())
                {
                    return;
                }
                b6_pump_1hz(); // [B6] diagnostics only; own 1 Hz gate, runs before the dispatch lock
                NativeLuaActionDispatchScope action_dispatch;
                if (!action_dispatch.owns_dispatch())
                {
                    return;
                }
                // The marshal world is PROVEN live''', 'vmt pump call')

# 8. old [MEM] path: cache gc_kb, skip when the pump printed this second
sub_once('''                        const int gc_kb = lua_gc(lua.get_lua_state(), LUA_GCCOUNT);
                        const auto snap = LuaMadeSimple::query_lua_memory(lua.get_lua_state());''', '''                        const int gc_kb = lua_gc(lua.get_lua_state(), LUA_GCCOUNT);
                        g_b6_gc_kb_cache.store(gc_kb, std::memory_order_relaxed); // [B6]
                        if (b6_pump_mem_recent())
                        {
                            return; // [B6] the pump already printed [MEM] (and the events) this second
                        }
                        const auto snap = LuaMadeSimple::query_lua_memory(lua.get_lua_state());''', 'old MEM')

open(os.path.join(root, 'work/src/LuaMod.cpp'), 'w', encoding='utf-8').write(s)
for k, v in counts.items():
    print('%-32s %d' % (k, v))
if counts['lock_guard sites'] != 45:
    print('FAIL expected 45 lock_guard sites'); sys.exit(1)
print('apply_luamod: OK')
