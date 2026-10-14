// B5 sites host test (Mac-only, NOT console evidence).
// Real Lua 5.4.7 C sources compiled as C (setjmp/longjmp, no C++ personality) =
// the port's configuration. The wrapper pieces are line-for-line copies of the
// B2 shapes in candidate LuaMadeSimple.cpp / .hpp:
//   copy_text_bounded / capture_cxx_exception_text   (cpp:70-83)
//   handle_error                                      (cpp:1527-1550, luaL_traceback + settop(0))
//   throw_error_impl (pcall-active branch only)       (cpp:1564-1597)
//   TRY(lua_state, code)                              (hpp:843-857)
//   process_lua_function: upvalue id -> function table -> TRY  (cpp:1699-1730)
// The lock + raise bodies copy the shape of LuaMod.cpp RegisterHook (5947 ff.),
// RegisterCustomEvent (3063 ff.) and the get_mod_ref / get_mod_ref_cxx pair.
//
// Checks (each increments `checked`; zero checked => exit 1):
//  A. OLD shape (guard alive, then luaL_error path) leaves the recursive mutex
//     owned by the raising thread: another thread cannot lock it (defect reproduced).
//  B. NEW shape (guard alive, then C++ throw, TRY raises afterwards) leaves it free.
//  C. Lua-visible result is identical OLD vs NEW for: pcall status, error type,
//     full message text including the traceback, stack balance after pcall.
//  D. get_mod_ref (OLD helper raise under the lock) vs get_mod_ref_cxx (NEW).
//  E. Known difference: TRY's 1023-byte message buffer vs 4095 for the old path.
//  F. Native (non-TRY) callback with a local catch (script_hook, site 8695):
//     OLD leaks the guard when nested in a Lua pcall; NEW is caught locally.
//  G. Raw lua_CFunction (no TRY) must unlock then raise: the correct form frees
//     the mutex; raising with the guard alive leaks it.
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// ---------------------------------------------------------------- shared pieces
static std::recursive_mutex g_m;  // stands in for LuaMod::m_thread_actions_mutex

static void copy_text_bounded(char* out, size_t out_size, const char* text) noexcept
{
    if (out == nullptr || out_size == 0) return;
    size_t i = 0;
    if (text != nullptr) for (; i + 1 < out_size && text[i] != '\0'; ++i) out[i] = text[i];
    out[i] = '\0';
}
static void capture_cxx_exception_text(char* buffer, size_t n, const char* what) noexcept
{
    copy_text_bounded(buffer, n, what != nullptr ? what : "(C++ exception without text)");
}
static std::string handle_error(lua_State* L, const std::string& error_message)
{
    luaL_traceback(L, L, error_message.c_str(), 0);
    std::string final_message;
    if (lua_isstring(L, -1)) final_message = lua_tostring(L, -1);
    if (final_message.empty() || final_message == error_message + "\nstack traceback:")
        final_message = error_message + "\nNo traceback";
    lua_settop(L, 0);
    return final_message;
}
static char g_last[512];
static void throw_error_impl(lua_State* L, const char* error_message)  // pcall-active branch
{
    char final_text[4096];
    try
    {
        auto fm = handle_error(L, std::string{error_message});
        copy_text_bounded(final_text, sizeof(final_text), fm.c_str());
    }
    catch (...) { copy_text_bounded(final_text, sizeof(final_text), error_message); }
    copy_text_bounded(g_last, sizeof(g_last), final_text);
    luaL_error(L, "%s", final_text);
}
// lua.throw_error(msg) as used by the OLD code at the 15 sites
static void lua_throw_error(lua_State* L, const std::string& m) { throw_error_impl(L, m.c_str()); }

template <typename Code> static int TRY(lua_State* L, Code code)  // hpp:843-857
{
    char message[1024];
    message[0] = '\0';
    try { return code(); }
    catch (std::exception& e) { capture_cxx_exception_text(message, sizeof(message), e.what()); }
    throw_error_impl(L, message);  // throw_error_from_buffer
    return 0;
}

using LuaFn = int (*)(lua_State*);
static std::vector<LuaFn> g_functions;
static int process_lua_function(lua_State* L)  // cpp:1699-1730
{
    const size_t id = (size_t)lua_tointeger(L, lua_upvalueindex(1));
    return TRY(L, [&] { return g_functions[id](L); });
}
static void register_function(lua_State* L, const char* name, LuaFn fn)
{
    g_functions.push_back(fn);
    lua_pushinteger(L, (lua_Integer)g_functions.size() - 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, &process_lua_function, 2);
    lua_setglobal(L, name);
}

// ---------------------------------------------------------------- bodies
static bool g_new = false;  // selects OLD (false) / NEW (true) body, mirrors the diff
static std::string g_msg;   // message the body raises

// RegisterHook / RegisterCustomEvent / UnregisterHook shape: guard first, raise while it is alive
static int body_lock_then_raise(lua_State* L)
{
    std::lock_guard<std::recursive_mutex> guard{g_m};
    std::string overload{g_msg};  // std::string local, like error_overload_not_found
    if (lua_type(L, 1) != LUA_TSTRING)  // is_string(): numbers do not count
    {
        if (g_new) throw std::runtime_error(overload);
        else lua_throw_error(L, overload);
    }
    return 0;
}
// get_mod_ref (OLD) / get_mod_ref_cxx (NEW) called under the guard
static void get_mod_ref_old(lua_State* L)
{
    if (lua_getglobal(L, "ModRef") == LUA_TNIL) lua_throw_error(L, "[get_mod_ref] Tried retrieving 'ModRef' global variable but it was nil, please do not override this global");
}
static void get_mod_ref_cxx(lua_State* L)
{
    if (lua_getglobal(L, "ModRef") == LUA_TNIL) throw std::runtime_error("[get_mod_ref] Tried retrieving 'ModRef' global variable but it was nil, please do not override this global");
}
static int body_get_mod_ref(lua_State* L)
{
    std::lock_guard<std::recursive_mutex> guard{g_m};
    if (g_new) get_mod_ref_cxx(L); else get_mod_ref_old(L);
    return 0;
}
// raw lua_CFunction, NOT registered through process_lua_function (no TRY)
static int raw_leaky(lua_State* L)
{
    std::lock_guard<std::recursive_mutex> guard{g_m};
    return luaL_error(L, "%s", "raw raise with guard alive");
}
static int raw_unlock_then_raise(lua_State* L)
{
    {
        std::lock_guard<std::recursive_mutex> guard{g_m};
        // ... work under the lock ...
    }
    return luaL_error(L, "%s", "raw raise after unlock");
}
// native hook with a local catch (script_hook, 8695), entered NESTED inside a Lua pcall
static int g_hook_logged = 0;
static void native_script_hook(lua_State* L)
{
    std::lock_guard<std::recursive_mutex> guard{g_m};
    try
    {
        if (g_new) throw std::runtime_error("[script_hook] Tried accessing unreal property without a registered handler.");
        lua_throw_error(L, "[script_hook] Tried accessing unreal property without a registered handler.");
    }
    catch (std::exception&) { ++g_hook_logged; }
}
static int calls_native_hook(lua_State* L) { native_script_hook(L); return 0; }

// ---------------------------------------------------------------- harness
static bool other_thread_can_lock()
{
    bool got = false;
    std::thread t([&] { if (g_m.try_lock()) { got = true; g_m.unlock(); } });
    t.join();
    return got;
}
struct Result { int status; int type; std::string msg; int top_after; };
static Result run(lua_State* L, const char* global_call_chunk)
{
    lua_settop(L, 0);
    luaL_loadstring(L, global_call_chunk);
    Result r{};
    r.status = lua_pcall(L, 0, 0, 0);
    r.type = lua_gettop(L) > 0 ? lua_type(L, -1) : -1;
    r.msg = (r.status != LUA_OK && lua_tostring(L, -1)) ? lua_tostring(L, -1) : "";
    lua_settop(L, 0);
    r.top_after = lua_gettop(L);
    return r;
}
static int checked = 0, fails = 0;
static void check(bool ok, const char* what)
{
    ++checked; fails += !ok;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
}
static void drain_owner()  // the main thread keeps ownership after a leaked guard; release it for the next case
{
    for (int i = 0; i < 100000 && !other_thread_can_lock(); ++i) g_m.unlock();
}

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    register_function(L, "RegisterHook", body_lock_then_raise);
    register_function(L, "RegisterCustomEvent", body_lock_then_raise);
    register_function(L, "NeedsModRef", body_get_mod_ref);
    lua_pushcfunction(L, raw_leaky); lua_setglobal(L, "RawLeaky");
    lua_pushcfunction(L, raw_unlock_then_raise); lua_setglobal(L, "RawOk");
    lua_pushcfunction(L, calls_native_hook); lua_setglobal(L, "NativeHook");

    const std::vector<std::string> msgs = {
        "\nNo overload found for function 'RegisterHook'.\nOverloads:\n#1: RegisterHook(string UFunction_Name, LuaFunction Callback, LuaFunction PostCallback))",
        "Tried to register a hook with Lua function 'RegisterHook' but no UFunction with the specified name was found.\nFunction Name: /Game/Foo.Foo_C:Bar",
        "path with %s %d %% in it",
        "Was unable to register a hook with Lua function 'RegisterHook', information:\nFunctionName: /Game/X.X_C:Y\nUFunction::Func: 0x1\nProcessInternal: 0x2\nFUNC_Native: 0\n",
    };

    // A. OLD leaves the mutex owned
    g_new = false; g_msg = msgs[0];
    Result old0 = run(L, "return RegisterHook(5)");
    check(old0.status == LUA_ERRRUN, "A. OLD: pcall reports a Lua runtime error");
    check(!other_thread_can_lock(), "A. OLD: mutex STILL OWNED after the error (guard skipped by longjmp) - defect reproduced");
    drain_owner();
    check(other_thread_can_lock(), "A. (cleanup) leaked count drained for the next case");

    // B. NEW frees the mutex, 1000 iterations
    g_new = true;
    int leaked = 0;
    for (int i = 0; i < 1000; ++i)
    {
        g_msg = msgs[i % msgs.size()];
        run(L, "return RegisterHook(5)");
        leaked += !other_thread_can_lock();
    }
    check(leaked == 0, "B. NEW: mutex free after each of 1000 refused calls (4 message shapes)");

    // C. identical Lua-visible result, per message, per entry point
    for (size_t k = 0; k < msgs.size(); ++k)
    {
        for (const char* chunk : {"return RegisterHook(5)", "return RegisterCustomEvent(5)", "return RegisterHook({})", "return RegisterHook()"})
        {
            g_msg = msgs[k];
            g_new = false; Result o = run(L, chunk); drain_owner();
            g_new = true;  Result n = run(L, chunk);
            bool same = o.status != LUA_OK && o.status == n.status && o.type == n.type && o.msg == n.msg && o.top_after == n.top_after;
            if (!same) std::printf("      OLD='%s'\n      NEW='%s'\n", o.msg.c_str(), n.msg.c_str());
            char buf[200]; std::snprintf(buf, sizeof buf, "C. msg#%zu %-40s status/type/text/traceback/stack identical", k, chunk);
            check(same, buf);
        }
    }
    for (size_t k = 0; k < msgs.size(); ++k)  // what the MOD sees through its own pcall: (ok, type(e), e)
    {
        std::string seen[2];
        for (int mode = 0; mode < 2; ++mode)
        {
            g_msg = msgs[k]; g_new = mode == 1;
            luaL_loadstring(L, "local ok, e = pcall(RegisterHook, 5) return tostring(ok) .. '|' .. type(e) .. '|' .. e");
            lua_pcall(L, 0, 1, 0);
            seen[mode] = lua_tostring(L, -1) ? lua_tostring(L, -1) : "?";
            lua_settop(L, 0);
            if (mode == 0) drain_owner();
        }
        char buf[120]; std::snprintf(buf, sizeof buf, "C. msg#%zu mod-side pcall(RegisterHook, 5) -> (false, 'string', text) identical OLD/NEW", k);
        check(seen[0] == seen[1] && seen[0].rfind("false|string|", 0) == 0, buf);
    }

    // D. get_mod_ref helper-raise under the lock (global ModRef deliberately absent)
    g_new = false; Result od = run(L, "return NeedsModRef()"); bool leaked_old = !other_thread_can_lock(); drain_owner();
    g_new = true;  Result nd = run(L, "return NeedsModRef()"); bool leaked_new = !other_thread_can_lock();
    check(leaked_old, "D. OLD get_mod_ref under the lock: mutex leaked (helper raise is a longjmp)");
    check(!leaked_new, "D. NEW get_mod_ref_cxx under the lock: mutex free");
    check(od.msg == nd.msg && od.status == nd.status && od.msg.find("[get_mod_ref] Tried retrieving 'ModRef'") != std::string::npos,
          "D. get_mod_ref / get_mod_ref_cxx: identical Lua error text and status");

    // E. known difference: message longer than 1023 bytes
    g_msg = std::string(1500, 'x'); g_new = false; Result oe = run(L, "return RegisterHook(5)"); drain_owner();
    g_new = true; Result ne = run(L, "return RegisterHook(5)");
    const auto count_x = [](const std::string& m) { size_t c = 0; for (char ch : m) c += ch == 'x'; return c; };
    std::printf("      INFO message payload kept: OLD=%zu bytes, NEW=%zu bytes\n", count_x(oe.msg), count_x(ne.msg));
    check(count_x(oe.msg) == 1500 && count_x(ne.msg) == 1023,
          "E. KNOWN DIFFERENCE reproduced: NEW truncates the text to 1023 bytes (OLD kept 1500); longest real site message is far below 1023");

    // F. native hook nested in a Lua pcall (script_hook 8695)
    g_new = false; g_hook_logged = 0; run(L, "return NativeHook()"); bool f_old_leak = !other_thread_can_lock(); int f_old_log = g_hook_logged; drain_owner();
    g_new = true;  g_hook_logged = 0; Result fn = run(L, "return NativeHook()"); bool f_new_leak = !other_thread_can_lock(); int f_new_log = g_hook_logged;
    check(f_old_leak && f_old_log == 0, "F. OLD nested native hook: lua.throw_error longjmps over the guard AND the local catch (leak, no log)");
    check(!f_new_leak && f_new_log == 1 && fn.status == LUA_OK, "F. NEW nested native hook: caught by the local catch, logged once, mutex free, Lua call completes");

    // G. raw lua_CFunction
    run(L, "return RawLeaky()"); bool g_leak = !other_thread_can_lock(); drain_owner();
    Result gok = run(L, "return RawOk()"); bool g_ok_leak = !other_thread_can_lock();
    check(g_leak, "G. raw CFunction raising with the guard alive: mutex leaked (a C++ throw would NOT be caught here)");
    check(!g_ok_leak && gok.msg.find("raw raise after unlock") != std::string::npos, "G. raw CFunction unlock-then-raise: mutex free, same Lua error");

    lua_close(L);
    std::printf("b5_sites_host_test: checks=%d failures=%d\n", checked, fails);
    if (checked == 0) { std::printf("FAIL (nothing checked)\n"); return 1; }
    std::printf("%s b5 sites host test\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
