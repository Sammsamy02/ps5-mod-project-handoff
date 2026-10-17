// B2 host test (Mac-only): the TRY / throw_error wrapper against the REAL Lua
// 5.4 C sources compiled as C (setjmp/longjmp — the port's configuration,
// luaraw-ldo.o imports setjmp/longjmp, no C++ personality).
//
// OLD = the pre-B2 shape: luaL_error(L, msg) called from INSIDE the catch block.
// NEW = the B2 shape: copy the text to a fixed buffer in the handler, raise after.
//
// Checks:
//  1. NEW: the caught exception object is destroyed (dtor count == ctor count);
//     OLD: it is not (longjmp out of the handler skips __cxa_end_catch) — the
//     test asserts OLD leaks, so the defect is demonstrated, not assumed.
//  2. NEW keeps the real exception text in the Lua error.
//  3. A '%s' inside an error message is passed literally (no format reads).
//  4. If formatting the error throws (simulated std::bad_alloc), NEW still
//     raises the raw text instead of letting a C++ exception into Lua's C frames.
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

static int g_ctor = 0, g_dtor = 0;
struct CountedError : std::runtime_error
{
    explicit CountedError(const char* m) : std::runtime_error(m) { ++g_ctor; }
    CountedError(const CountedError& o) : std::runtime_error(o) { ++g_ctor; }
    ~CountedError() override { ++g_dtor; }
};

static bool g_format_throws = false;
// stands in for LuaMadeSimple::handle_error (allocates a std::string)
static std::string handle_error(const char* m)
{
    if (g_format_throws) throw std::bad_alloc{};
    return std::string{"[handled] "} + m;
}

static void copy_text_bounded(char* out, size_t n, const char* t)
{
    size_t i = 0;
    if (t) for (; i + 1 < n && t[i]; ++i) out[i] = t[i];
    out[i] = '\0';
}

// ---- NEW (B2) ---------------------------------------------------------------
static void throw_error_impl(lua_State* L, const char* msg)
{
    char final_text[4096];
    try
    {
        auto f = handle_error(msg);
        copy_text_bounded(final_text, sizeof final_text, f.c_str());
    }
    catch (...)
    {
        copy_text_bounded(final_text, sizeof final_text, msg);
    }
    luaL_error(L, "%s", final_text);
}
template <typename F> static int TRY_NEW(lua_State* L, F f)
{
    char message[1024];
    message[0] = '\0';
    try
    {
        return f();
    }
    catch (std::exception& e)
    {
        copy_text_bounded(message, sizeof message, e.what());
    }
    throw_error_impl(L, message);
    return 0;
}

// ---- OLD (pre-B2) -----------------------------------------------------------
static void throw_error_old(lua_State* L, const std::string& m)
{
    auto f = handle_error(m.c_str());
    luaL_error(L, f.c_str());
}
template <typename F> static int TRY_OLD(lua_State* L, F f)
{
    try
    {
        return f();
    }
    catch (std::exception& e)
    {
        throw_error_old(L, e.what());
        return 0;
    }
}

static int b_new(lua_State* L) { return TRY_NEW(L, []() -> int { throw CountedError("boom-new"); }); }
static int b_old(lua_State* L) { return TRY_OLD(L, []() -> int { throw CountedError("boom-old"); }); }
static int b_pct(lua_State* L) { return TRY_NEW(L, []() -> int { throw CountedError("path %s %d %%"); }); }

static std::string run(lua_State* L, lua_CFunction fn)
{
    lua_pushcfunction(L, fn);
    int st = lua_pcall(L, 0, 0, 0);
    std::string out = st == LUA_OK ? "OK" : (lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
    if (st != LUA_OK) lua_pop(L, 1);
    return out;
}

int main()
{
    lua_State* L = luaL_newstate();
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
        fails += !ok;
    };

    g_ctor = g_dtor = 0;
    for (int i = 0; i < 1000; ++i) run(L, b_new);
    std::string m = run(L, b_new);
    check(g_ctor == g_dtor, "NEW: every caught exception object is destroyed");
    check(m.find("[handled] boom-new") != std::string::npos, "NEW: real exception text kept in the Lua error");

    g_ctor = g_dtor = 0;
    for (int i = 0; i < 1000; ++i) run(L, b_old);
    std::printf("       OLD: ctor=%d dtor=%d (exceptions never released: %d)\n", g_ctor, g_dtor, g_ctor - g_dtor);
    check(g_ctor > g_dtor, "OLD: leaks the exception object (defect reproduced)");

    m = run(L, b_pct);
    check(m.find("path %s %d %%") != std::string::npos, "NEW: '%s' in a message is passed literally");

    g_format_throws = true;
    m = run(L, b_new);
    g_format_throws = false;
    check(m.find("boom-new") != std::string::npos && m.find("[handled]") == std::string::npos,
          "NEW: formatting failure raises the raw text (no C++ exception into Lua)");

    lua_close(L);
    std::printf("%s b2-error-wrapper host test (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
