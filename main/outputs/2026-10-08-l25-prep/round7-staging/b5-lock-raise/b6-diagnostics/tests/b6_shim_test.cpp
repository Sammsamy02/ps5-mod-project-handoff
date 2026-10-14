// B6 shim host test (Mac-only): the SHIPPED note_backend_failure + try_original text (sliced
// from the staged LuaPathShim.cpp) against the REAL Lua 5.4 io.open on this Mac. Checks that the
// errno and message of a failed sandbox write are captured (ENOENT, EACCES), that the raised-error
// branch is captured, that a success returns the file, and that later attempts refresh the values.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

static int g_checked = 0, g_failed = 0;
#define CHECK(cond, ...) do { ++g_checked; if (!(cond)) { ++g_failed; std::printf("  FAIL  line %d  %s  ", __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

namespace
{
#include "shimfail.inc"

    // A wrapper shaped like io_open_wrapper's first attempt: upvalue 1 = original io.open.
    int g_last_results = 0;
    std::string g_target;
    int wrapper(lua_State* L)
    {
        const int argc = lua_gettop(L);
        g_last_results = try_original(L, argc, g_target);
        return g_last_results > 0 ? g_last_results : 0;
    }
}

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_getglobal(L, "io");
    lua_getfield(L, -1, "open");
    lua_pushcclosure(L, wrapper, 1);   // upvalue 1 = the real io.open
    lua_setglobal(L, "shim_open");
    lua_pop(L, 1);

    // Temp dir: $B6_TEST_TMP, else $TMPDIR, else /tmp (no machine-specific path baked in).
    const char* tmp_env = std::getenv("B6_TEST_TMP");
    if (tmp_env == nullptr || *tmp_env == '\0') { tmp_env = std::getenv("TMPDIR"); }
    std::string tmpl_str = std::string((tmp_env != nullptr && *tmp_env != '\0') ? tmp_env : "/tmp") + "/b6shimXXXXXX";
    std::vector<char> tmpl(tmpl_str.begin(), tmpl_str.end());
    tmpl.push_back('\0');
    const char* root = mkdtemp(tmpl.data());
    CHECK(root != nullptr, "temp dir");
    const std::string base = root;

    auto run = [&](const std::string& target, const char* mode) {
        g_target = target;
        std::string chunk = std::string("local f = shim_open('ignored', '") + mode + "'); if f then f:close() end";
        luaL_dostring(L, chunk.c_str());
        lua_settop(L, 0);
    };

    // 1: directory does not exist -> ENOENT (the console's documented post-module_start reality)
    run(base + "/nodir/file.txt", "w");
    CHECK(g_last_results == 0, "failed open returns 0");
    CHECK(tl_backend_errno == ENOENT, "errno = ENOENT (%d), got %d", ENOENT, tl_backend_errno);
    CHECK(std::strstr(tl_backend_text, "No such file or directory") != nullptr, "message carries strerror text: '%s'", tl_backend_text);
    CHECK(std::strcmp(std::strerror(tl_backend_errno), "No such file or directory") == 0, "strerror(errno) is the same text");

    // 2: read-only directory -> EACCES
    const std::string ro = base + "/ro";
    mkdir(ro.c_str(), 0555);
    run(ro + "/file.txt", "w");
    CHECK(tl_backend_errno == EACCES || geteuid() == 0, "errno = EACCES (%d), got %d", EACCES, tl_backend_errno);
    CHECK(g_last_results == 0 || geteuid() == 0, "failed open returns 0");

    // 3: success returns the file and does not touch the previous failure record
    const int before = tl_backend_errno;
    run(base + "/ok.txt", "w");
    CHECK(g_last_results >= 1, "success returns the file handle");
    CHECK(tl_backend_errno == before, "success leaves the last failure record alone");
    struct stat st;
    CHECK(stat((base + "/ok.txt").c_str(), &st) == 0, "file really created");

    // 4: backend RAISES (invalid mode) -> raised branch: message captured, errno 0
    run(base + "/x.txt", "zzz");
    CHECK(g_last_results == 0, "raised error returns 0");
    CHECK(tl_backend_errno == 0, "no errno from a raised error");
    CHECK(std::strstr(tl_backend_text, "invalid mode") != nullptr, "raised text captured: '%s'", tl_backend_text);

    // 5: the next failure refreshes both
    run(base + "/again/none.txt", "w");
    CHECK(tl_backend_errno == ENOENT && std::strstr(tl_backend_text, "No such file") != nullptr, "refreshed after a later failure");

    // 6: a very long path: the message is bounded, ends with the OS text, and says it was cut
    std::string deep = base;
    for (int i = 0; i < 40; ++i) { deep += "/aaaaaaaaaaaaaaaaaaaa"; }
    run(deep + "/f.txt", "w");
    CHECK(std::strlen(tl_backend_text) < sizeof(tl_backend_text), "message bounded (%zu < %zu)", std::strlen(tl_backend_text), sizeof(tl_backend_text));
    CHECK(std::strstr(tl_backend_text, "No such file or directory") != nullptr, "tail keeps the OS text: '%s'", tl_backend_text);
    CHECK(std::strncmp(tl_backend_text, "...", 3) == 0, "cut message starts with '...'");
    CHECK(tl_backend_errno == ENOENT || tl_backend_errno == ENAMETOOLONG, "errno still captured (%d)", tl_backend_errno);

    // 7: a path of ~150 chars (typical console depth) keeps its whole message (no cut)
    std::string mid = base;
    while (mid.size() < 130) { mid += "/dirdirdir"; }
    run(mid + "/f.txt", "w");
    CHECK(std::strstr(tl_backend_text, "No such file or directory") != nullptr && std::strncmp(tl_backend_text, "...", 3) != 0,
          "mid-length message not cut: '%s'", tl_backend_text);

    lua_close(L);
    std::string cleanup = std::string("chmod 755 '") + ro + "'; rm -rf '" + base + "'";
    (void)!system(cleanup.c_str());
    std::printf("checked=%d failed=%d\n", g_checked, g_failed);
    if (g_checked == 0) { std::printf("FAIL b6 shim test (nothing checked)\n"); return 1; }
    if (g_failed) { std::printf("FAIL b6 shim test (%d failures)\n", g_failed); return 1; }
    std::printf("PASS b6 shim test (0 failures, %d checks)\n", g_checked);
    return 0;
}
