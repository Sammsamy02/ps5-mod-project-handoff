// B3 host test (Mac-only).
//  A. IterateGameDirectories PS5 flat-layout detection: the exact lambda logic
//     from the B3 LuaMod.cpp hunk, run on temporary directory trees.
//  B. ForEachUObject stack discipline with the REAL Lua C library: the B3
//     per-iteration shape (pushvalue(1), push 3 args, pcall(3,1), settop to the
//     pre-iteration height) over 100k items — the stack must not grow, extra
//     arguments must survive, and a callback returning true must NOT stop the
//     walk (upstream ignores the return value).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace fs = std::filesystem;

// ---- A: copied from the B3 hunk (only the detection; returns the game name) --
static bool detect(const fs::path& game_executable_directory, fs::path& game_name)
{
    const auto igd_iequals = [](const std::string& a, const char* b) -> bool {
        size_t i = 0;
        for (; i < a.size() && b[i] != '\0'; ++i)
        {
            const char ca = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
            const char cb = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
            if (ca != cb) { return false; }
        }
        return i == a.size() && b[i] == '\0';
    };
    const auto igd_has_content_child = [&](const fs::path& dir) -> bool {
        std::error_code ec{};
        fs::directory_iterator sub{dir, ec};
        const fs::directory_iterator sub_end{};
        while (!ec && sub != sub_end)
        {
            std::error_code entry_ec{};
            if (sub->is_directory(entry_ec) && !entry_ec && igd_iequals(sub->path().filename().string(), "content")) { return true; }
            sub.increment(ec);
        }
        return false;
    };
    std::error_code igd_ec{};
    fs::directory_iterator igd_it{game_executable_directory, igd_ec};
    const fs::directory_iterator igd_end{};
    int igd_candidates{};
    fs::path igd_game_name{};
    while (!igd_ec && igd_it != igd_end)
    {
        std::error_code entry_ec{};
        if (igd_it->is_directory(entry_ec) && !entry_ec)
        {
            const auto child_name = igd_it->path().filename();
            if (!igd_iequals(child_name.string(), "engine") && igd_has_content_child(igd_it->path()))
            {
                ++igd_candidates;
                igd_game_name = child_name;
            }
        }
        igd_it.increment(igd_ec);
    }
    if (!igd_ec && igd_candidates == 1 && game_executable_directory != game_executable_directory.root_path())
    {
        game_name = igd_game_name;
        return true;
    }
    return false;
}

static void mk(const fs::path& p) { fs::create_directories(p); }
static void touch(const fs::path& p) { std::ofstream(p).put('x'); }

// ---- B: ForEachUObject shape ------------------------------------------------
static int g_calls = 0;
static int foreach_b3(lua_State* L)
{
    for (int i = 0; i < 100000; ++i)
    {
        const int baseline = lua_gettop(L);
        lua_pushvalue(L, 1);
        lua_pushinteger(L, i);
        lua_pushinteger(L, 0);
        lua_pushinteger(L, i);
        if (lua_pcall(L, 3, 1, 0) != LUA_OK) { return lua_error(L); }
        ++g_calls;
        lua_settop(L, baseline);
        if (lua_gettop(L) != baseline) { return luaL_error(L, "stack drift"); }
    }
    lua_pushinteger(L, lua_gettop(L));
    return 1;
}

int main()
{
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
        fails += !ok;
    };
    // A
    const fs::path base = fs::temp_directory_path() / "b3-igd-host-test";
    fs::remove_all(base);
    fs::path name;
    {
        auto app = base / "case1" / "app0";
        mk(app / "townfall" / "content" / "paks");
        mk(app / "engine" / "content");
        mk(app / "sce_module");
        touch(app / "eboot.bin");
        check(detect(app, name) && name == "townfall", "lower-case /app0/townfall/content -> Game=townfall");
    }
    {
        auto app = base / "case2" / "app0";
        mk(app / "MyGame" / "Content");
        mk(app / "Engine" / "Content");
        check(detect(app, name) && name == "MyGame", "mixed-case /app0/MyGame/Content (Engine skipped)");
    }
    {
        auto app = base / "case3" / "app0";
        mk(app / "a" / "Content");
        mk(app / "b" / "Content");
        check(!detect(app, name), "two candidates -> no guess (nil)");
    }
    {
        auto app = base / "case4" / "app0";
        mk(app / "sce_module");
        touch(app / "eboot.bin");
        check(!detect(app, name), "no candidate -> nil");
    }
    {
        auto app = base / "case5" / "app0";
        mk(app / "engine" / "content");
        check(!detect(app, name), "only Engine/Content -> nil");
    }
    check(!detect(base / "does-not-exist", name), "unreadable root -> nil, no throw");
    fs::remove_all(base);

    // B
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "ForEachB3", foreach_b3);
    const char* script =
        "local seen = 0\n"
        "local h = ForEachB3(function(o, c, i) seen = seen + 1; return true end, 'extra1', 'extra2')\n"
        "return seen, h\n";
    int st = luaL_dostring(L, script);
    check(st == LUA_OK, "100k callbacks run without a Lua stack overflow");
    if (st == LUA_OK)
    {
        long long seen = lua_tointeger(L, -2), h = lua_tointeger(L, -1);
        std::printf("       seen=%lld stack_height_after=%lld\n", seen, h);
        check(seen == 100000, "callback returning true does NOT stop the walk (upstream parity)");
        check(h == 3, "stack height unchanged with 2 extra arguments (callback + 2)");
    }
    else
    {
        std::printf("       error: %s\n", lua_tostring(L, -1));
    }
    lua_close(L);
    std::printf("%s b3 host test (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
