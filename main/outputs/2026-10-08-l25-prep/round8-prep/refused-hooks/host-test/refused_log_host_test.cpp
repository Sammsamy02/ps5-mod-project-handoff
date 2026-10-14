// Host test (Mac, no engine) for refused_log_helper.inc: mocks only the types the helper touches.
// Checks: exact line format, quoting/sanitising of target+mod, line cap, counter, gate predicates,
// mod_name_of leaves the Lua stack unchanged, and that a gate that checked nothing fails.
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

using StringType = std::u16string;
#define STR(x) u##x
struct lua_State { int top = 0; bool has_modref = true; };
constexpr int LUA_TNIL = 0;
static int lua_gettop(lua_State* L) { return L->top; }
static void lua_settop(lua_State* L, int t) { L->top = t; }
static int lua_getglobal(lua_State* L, const char*) { L->top++; return L->has_modref ? 7 : LUA_TNIL; }
struct FakeMod { StringType name = u"Town'fall Mod\n"; StringType get_name() const { return name; } };
namespace LuaType { struct LuaModRef { FakeMod m; FakeMod* get_remote_cpp_object() { return &m; } }; }
namespace LuaMadeSimple { struct Lua { lua_State* L; lua_State* get_lua_state() const { return L; }
    template <class T> T& get_userdata(int) const { static T t; return t; } }; }
namespace Unreal { namespace Version { inline bool plsf_ok = false; inline bool IsAtLeast(int, int) { return true; } inline bool IsBelow(int, int) { return false; } }
  struct Fn { bool r; bool is_ready() const { return r; } };
  struct UObject { static inline Fn ProcessLocalScriptFunctionInternal{false}; static inline Fn ProcessInternalInternal{false}; }; }
std::vector<std::string> g_out;
namespace Output { template <class... A> void send(const StringType&, const StringType& w) { std::string s; for (auto c : w) s.push_back((char)c); g_out.push_back(s); } }
namespace L22Diagnostics { template <class F> void observe(F&& f) { f(); } }
#include "refused_log_helper.inc"
using namespace refused_log;

static int fails = 0, checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++fails; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main() {
    lua_State L; LuaMadeSimple::Lua lua{&L};
    // 1. exact format + sanitising (space allowed, quote/newline replaced)
    throws(lua, "RegisterHook", StringType(u"/Game/BP_X_C:On Interact Event"), "bp-script-function-needs-processinternal-hook");
    CHECK(g_out.size() == 1);
    CHECK(g_out[0] == "[REFUSED] kind=RegisterHook target='/Game/BP_X_C:On Interact Event' mod='Town?fall Mod?' reason=bp-script-function-needs-processinternal-hook effect=throws");
    CHECK(L.top == 0);                       // Lua stack untouched by mod_name_of
    L.has_modref = false;
    throws(lua, "RegisterHook", StringType(u"t"), "ufunction-not-found");
    CHECK(g_out.back().find("mod='?'") != std::string::npos);
    CHECK(L.top == 0);
    L.has_modref = true;
    // 2. accepted(): silent while deliverable, one line when not
    size_t before = g_out.size();
    accepted(lua, "RegisterBeginPlayPreHook", StringType(u"-"), Family::EagerDetour);
    CHECK(g_out.size() == before);           // deliverable (gate not set) => no line
    eager_detour_block_skipped = true;
    accepted(lua, "RegisterBeginPlayPreHook", StringType(u"-"), Family::EagerDetour);
    CHECK(g_out.size() == before + 1);
    CHECK(g_out.back().find("reason=detour-block-skipped-r3-marshal effect=accepted-undelivered") != std::string::npos);
    // 3. LoadMap tri-state
    loadmap_state = 0; before = g_out.size();
    accepted(lua, "RegisterLoadMapPostHook", StringType(u"-"), Family::LoadMap);
    CHECK(g_out.size() == before);
    loadmap_state = 1; accepted(lua, "RegisterLoadMapPostHook", StringType(u"-"), Family::LoadMap);
    CHECK(g_out.back().find("reason=loadmap-deferred-arm-pending effect=queued-pending") != std::string::npos);
    loadmap_state = 2; before = g_out.size(); accepted(lua, "RegisterLoadMapPostHook", StringType(u"-"), Family::LoadMap);
    CHECK(g_out.size() == before);           // armed => silent
    loadmap_state = 3; accepted(lua, "RegisterLoadMapPostHook", StringType(u"-"), Family::LoadMap);
    CHECK(g_out.back().find("reason=loadmap-detour-arm-refused effect=accepted-undelivered") != std::string::npos);
    // 4. custom events follow ProcessLocalScriptFunction readiness
    before = g_out.size(); accepted(lua, "RegisterCustomEvent", StringType(u"Evt"), Family::CustomEvent);
    CHECK(g_out.size() == before + 1 && g_out.back().find("process-local-script-function-unavailable") != std::string::npos);
    Unreal::UObject::ProcessLocalScriptFunctionInternal.r = true; before = g_out.size();
    accepted(lua, "RegisterCustomEvent", StringType(u"Evt"), Family::CustomEvent);
    CHECK(g_out.size() == before);
    // 5. very long target is truncated, never overflows
    throws(lua, "RegisterHook", StringType(1000, u'a'), "x");
    CHECK(g_out.back().size() < 640);
    // 6. line cap: counter keeps counting, output stops with one suppression line
    const uint32_t start_total = total.load();
    for (int i = 0; i < 400; ++i) throws(lua, "RegisterHook", StringType(u"t"), "r");
    CHECK(total.load() == start_total + 400);
    size_t cap_lines = 0; for (auto& s : g_out) if (s.find("reason=line-cap-") != std::string::npos) ++cap_lines;
    CHECK(cap_lines == 1);
    CHECK(g_out.size() <= kMaxLines + 1);
    std::printf("checks=%d fails=%d lines_captured=%zu\n", checks, fails, g_out.size());
    if (checks == 0) { std::printf("GATE FAIL: nothing checked\n"); return 2; }
    return fails ? 1 : 0;
}
