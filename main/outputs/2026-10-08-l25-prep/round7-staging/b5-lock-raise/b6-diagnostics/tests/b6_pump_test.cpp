// B6 pump/glue host test (Mac-only). Compiles the shipped GLUE and PUMP slices of the staged
// LuaMod.cpp (b6_pump_1hz, emit_line, on_game_thread, ascii_name) with minimal engine stubs
// and runs the pump. Proves: it compiles with the real declarations' types, the 1 Hz gate,
// the persistent-lock-depth check (the B4 signature), q=busy + "still waiting" when another
// thread holds the lock, and that the pump never takes the dispatch lock.
// Stubs only stand in for engine types (Output, StringType, Unreal::IsInGameThreadRaw, the
// LuaMod queues, query_lua_memory); everything else is the shipped text.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#define RC_LMS_API
#define STR(x) u##x

static std::atomic<int> g_checked{0}, g_failed{0};
#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        ++g_checked;                                                      \
        if (!(cond)) {                                                    \
            ++g_failed;                                                   \
            std::printf("  FAIL  line %d  %s  ", __LINE__, #cond);        \
            std::printf(__VA_ARGS__);                                     \
            std::printf("\n");                                            \
        }                                                                 \
    } while (0)

static std::mutex g_out_mutex;
static std::vector<std::string> g_out;
struct DiagEvent { std::string tag; unsigned value; };
static std::vector<DiagEvent> g_events;
extern "C" void diagnostic_event(const char* tag, unsigned int value)
{
    std::lock_guard<std::mutex> g{g_out_mutex};
    g_events.push_back({tag, value});
}

namespace RC
{
    using StringType = std::u16string;
    using StringViewType = std::u16string_view;   // Mod::get_name() returns StringViewType
    namespace Output
    {
        inline void capture(const StringType& s)
        {
            std::string narrow;
            for (char16_t c : s) { narrow.push_back(static_cast<char>(c)); }
            std::lock_guard<std::mutex> g{g_out_mutex};
            g_out.push_back(narrow);
        }
        template <typename... A>
        void send(const char16_t*, const StringType& s, A&&...) { capture(s); }
    }
    namespace L22Diagnostics
    {
        template <typename F>
        inline void observe(F&& f) noexcept { try { f(); } catch (...) {} }
    }
    static thread_local bool tl_game = true;
    namespace Unreal
    {
        inline bool IsInGameThreadRaw() { return tl_game; }
    }
    class LuaMod
    {
    public:
        static inline std::recursive_mutex m_thread_actions_mutex{};
        static inline std::vector<int> m_game_thread_actions{}, m_pending_game_thread_actions{}, m_delayed_game_thread_actions{},
                m_pending_delayed_game_thread_actions{}, m_engine_tick_actions{}, m_pending_engine_tick_actions{};
    };
    namespace { thread_local bool NativeLuaActionDispatching{}; }

    namespace LuaMadeSimple
    {
        struct LuaMemorySnapshot
        {
            int gc_kb{0};
            size_t live_bytes{4096 * 10}, live_blocks{7}, total_alloc_calls{99}, failed_calls{0}, last_fail_osize{0}, last_fail_nsize{0};
            size_t arena_capacity_bytes{0}, arena_used_bytes{0}, arena_external_bytes{0}, raised_errors{3}, cxx_exceptions{2};
        };
        static auto query_lua_memory(lua_State*) -> LuaMemorySnapshot { return LuaMemorySnapshot{}; }
#include "api.inc"
        static auto copy_text_bounded(char* out, size_t out_size, const char* text) noexcept -> void
        {
            size_t i = 0;
            if (out == nullptr || out_size == 0) { return; }
            if (text != nullptr) { for (; i + 1 < out_size && text[i] != '\0'; ++i) { out[i] = text[i]; } }
            out[i] = '\0';
        }
#include "regionA.inc"
        static auto diag_main_state(lua_State* s) noexcept -> lua_State* { return s; }
    }

#include "regionB.inc"
#include "glue.inc"
#include "pump.inc"

    static auto drain_out() -> std::vector<std::string>
    {
        std::lock_guard<std::mutex> g{g_out_mutex};
        auto copy = g_out;
        g_out.clear();
        return copy;
    }
    static auto count_with(const std::vector<std::string>& v, const char* needle) -> int
    {
        int n = 0;
        for (const auto& l : v) { if (l.find(needle) != std::string::npos) { ++n; } }
        return n;
    }
    static auto find_with(const std::vector<std::string>& v, const char* needle) -> std::string
    {
        for (const auto& l : v) { if (l.find(needle) != std::string::npos) { return l; } }
        return {};
    }

    static void run()
    {
        // ascii_name + emit_line
        char name[8];
        b6diag::ascii_name(name, sizeof(name), StringType{u"ModéLong"});
        CHECK(std::strcmp(name, "Mod?Lon") == 0, "ascii_name keeps ASCII, maps the rest to '?', bounds the length: '%s'", name);
        b6diag::emit_line("hello");
        CHECK(drain_out().size() == 1, "emit_line writes one line");

        // P3 setup: a skipped lock scope (depth 2) on the "game" thread, outside any dispatch
        LuaMadeSimple::diag_scope_enter();
        LuaMadeSimple::diag_scope_enter();
        LuaMod::m_game_thread_actions.assign(3, 0);
        LuaMod::m_delayed_game_thread_actions.assign(1, 0);

        b6_pump_1hz();
        auto out = drain_out();
        std::printf("      pump 1:\n");
        for (const auto& l : out) { std::printf("        %s\n", l.c_str()); }
        CHECK(count_with(out, "[PERF] dt=0ms") == 1, "first pump prints [PERF]");
        CHECK(count_with(out, "q=g3,p0,d1,pd0,e0,pe0") == 1, "queue depths read");
        CHECK(count_with(out, "pump_depth=2") == 1, "pump depth shown");
        CHECK(count_with(out, "[MEM] state=pump gc_kb=0 live_kb=40 blocks=7 allocs=99 fails=0") == 1, "[MEM] from the pump: %s", find_with(out, "[MEM]").c_str());
        CHECK(count_with(out, "src=pump") == 1, "[MEM] tagged src=pump");
        CHECK(count_with(out, "[LOCKLEAK]") == 0, "one sighting of depth 2 is not yet a leak");
        CHECK(b6_pump_mem_recent(), "the old [MEM] path will skip this second");
        bool saw_mem_events = false;
        for (const auto& e : g_events) { saw_mem_events |= (e.tag == "lua_mem_fails"); }
        CHECK(saw_mem_events, "pump also emits the lua_mem_* diagnostic events");

        b6_pump_1hz();
        CHECK(drain_out().empty(), "second call inside the 1 Hz gate prints nothing");

        std::this_thread::sleep_for(std::chrono::milliseconds(950));
        b6_pump_1hz();
        out = drain_out();
        std::printf("      pump 2:\n");
        for (const auto& l : out) { std::printf("        %s\n", l.c_str()); }
        CHECK(count_with(out, "[PERF] dt=9") + count_with(out, "[PERF] dt=10") == 1, "second [PERF] has dt about 950 ms: %s", find_with(out, "[PERF]").c_str());
        CHECK(count_with(out, "[LOCKLEAK] mod=? fn=pump kind=pump extra=2 depth=2") == 1, "persistent depth 2 outside a dispatch = [LOCKLEAK]: %s", find_with(out, "[LOCKLEAK]").c_str());
        LuaMadeSimple::diag_scope_exit();
        LuaMadeSimple::diag_scope_exit();

        // P3b: while this thread IS dispatching, a high depth is normal
        NativeLuaActionDispatching = true;
        LuaMadeSimple::diag_scope_enter();
        std::this_thread::sleep_for(std::chrono::milliseconds(950));
        b6_pump_1hz();
        b6diag::g_mods[b6diag::kMaxMods].leak_last_log_ms = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(950));
        b6_pump_1hz();
        out = drain_out();
        CHECK(count_with(out, "kind=pump") == 0, "no pump [LOCKLEAK] while dispatching");
        NativeLuaActionDispatching = false;
        LuaMadeSimple::diag_scope_exit();

        // P4: another thread holds the lock (tracked, so the holder is known) and a second one waits.
        std::atomic<bool> holder_in{false};
        std::thread holder([&] {
            tl_game = false;
            b6diag::ActionsLock hold{LuaMod::m_thread_actions_mutex, 8888};
            holder_in = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2300));
        });
        while (!holder_in) { std::this_thread::yield(); }
        std::thread waiter([&] {
            tl_game = false;
            b6diag::ActionsLock w{LuaMod::m_thread_actions_mutex, 9999};
        });
        // widget burst: 9 constructions, window ends before the pump below
        for (int i = 0; i < 9; ++i) { b6diag::widget_gate(b6diag::now_ms()); }
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        const auto t0 = b6diag::now_ms();
        b6_pump_1hz();     // pump must run although the dispatch lock is owned elsewhere
        const auto took = b6diag::now_ms() - t0;
        out = drain_out();
        std::printf("      pump while another thread holds the lock:\n");
        for (const auto& l : out) { std::printf("        %s\n", l.c_str()); }
        CHECK(took < 100, "the pump did not block on the held lock (%lld ms)", static_cast<long long>(took));
        CHECK(count_with(out, " q=busy ") == 1, "q=busy while another thread holds the lock");
        CHECK(count_with(out, "[LOCKWAIT] still waiting site_line=9999 thread=other") == 1, "stuck waiter reported: %s", find_with(out, "[LOCKWAIT]").c_str());
        CHECK(count_with(out, "holder_line=8888 holder_thread=other") == 1, "holder identified");
        CHECK(count_with(out, "[l22] widget/object construct summary: 9 constructed in the last window, 4 log lines suppressed") == 1, "burst summary from the pump: %s", find_with(out, "summary").c_str());
        CHECK(count_with(out, "[MEM] state=pump") == 1, "[MEM] still printed with the lock stuck");
        holder.join();
        waiter.join();

        // lock free again: the queue depths come back
        std::this_thread::sleep_for(std::chrono::milliseconds(950));
        b6_pump_1hz();
        out = drain_out();
        CHECK(count_with(out, " q=g3,") == 1, "q= depths again once the lock is free: %s", find_with(out, "[PERF]").c_str());
        CHECK(count_with(out, "lockwait=1/max") == 1 && count_with(out, "/over1 ") == 1, "slow acquisition counted in [PERF]: %s", find_with(out, "[PERF]").c_str());
        CHECK(LuaMod::m_thread_actions_mutex.try_lock(), "pump left the mutex unlocked");
        LuaMod::m_thread_actions_mutex.unlock();
    }
}

int main()
{
    std::printf("b6 pump/glue test\n");
    RC::run();
    std::printf("checked=%d failed=%d\n", g_checked.load(), g_failed.load());
    if (g_checked.load() == 0) { std::printf("FAIL b6 pump test (nothing checked)\n"); return 1; }
    if (g_failed.load() != 0) { std::printf("FAIL b6 pump test (%d failures)\n", g_failed.load()); return 1; }
    std::printf("PASS b6 pump test (0 failures, %d checks)\n", g_checked.load());
    return 0;
}
