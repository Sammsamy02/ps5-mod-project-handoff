// R9-MEM host test (Mac-only). Exercises the STAGED allocator code of
// round9-staging/memory/sites/src/LuaMadeSimple/LuaMadeSimple.cpp, extracted
// verbatim by run.sh into build/extracted_alloc.inc (+ the staged header's
// LuaMemorySnapshot / LuaBackingAllocator into build/extracted_types.inc),
// against the port's REAL Lua 5.4 C objects (built as C = PS5 configuration).
//
// The "engine allocator" is a fake that records every pointer it owns, so the
// test can prove that no block ever crosses stores (libc <-> backing).
//
// Checks (each prints PASS/FAIL; exit 0 only when all pass and count > 0):
//  1. No backing installed: new state is a libc state (be_states 0, libc 1).
//  2. Install works once; second install refused; null callbacks refused.
//  3. State created after install: EVERY alloc/free of that state goes to the
//     backing store (fake sees allocs > 0, never sees a foreign free) and
//     lua_close returns the fake to 0 live blocks / 0 live bytes.
//  4. The old (pre-install) libc state keeps working and never reaches the fake.
//  5. Coroutines (lua_newthread) of a backing state stay on the backing store.
//  6. ForEachUObject-shaped burst (200k userdata + GC) on the backing store:
//     live bytes come back near baseline after a full GC.
//  7. Backing store failure -> LUA_ERRMEM through pcall ("not enough memory"),
//     no crash, be_fails and fails both increase, state usable afterwards.
//  8. Replica (NOT the real LuaMod code): old flag pattern stays stuck when
//     the TRY handler throws; the staged RAII guard resets it.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#define RC_LMS_API

namespace RC::LuaMadeSimple
{
#include "extracted_types.inc"
#include "extracted_alloc.inc"

    // Replica of the staged new_state() body minus the Lua wrapper object
    // (run.sh asserts the staged file contains these exact statements).
    static auto test_new_state() -> lua_State*
    {
        void* const backing_ud = backing_for_new_state();
        auto new_lua_state = lua_newstate(lms_counting_alloc, backing_ud);
        if (new_lua_state != nullptr)
        {
            (backing_ud != nullptr ? g_backing_states : g_libc_states).fetch_add(1);
            lua_atpanic(new_lua_state, &lms_lua_panic);
            lua_setwarnf(new_lua_state, lms_warnfoff, new_lua_state);
        }
        return new_lua_state;
    }
} // namespace RC::LuaMadeSimple

using namespace RC::LuaMadeSimple;

// ---------------- fake engine allocator ----------------
static std::mutex g_fake_mutex;
static std::unordered_map<void*, size_t> g_fake_live;
static size_t g_fake_allocs = 0, g_fake_frees = 0, g_fake_foreign = 0, g_fake_bytes = 0;
static long g_fake_fail_after = -1; // >= 0: fail when this many more allocations were served

static void* fake_realloc(void* ptr, size_t nsize) noexcept
{
    std::lock_guard<std::mutex> g{g_fake_mutex};
    if (nsize == 0) { ++g_fake_foreign; return nullptr; } // contract: never 0
    if (ptr != nullptr && g_fake_live.find(ptr) == g_fake_live.end()) { ++g_fake_foreign; return nullptr; }
    if (g_fake_fail_after == 0) { return nullptr; }
    if (g_fake_fail_after > 0) { --g_fake_fail_after; }
    void* n = std::realloc(ptr, nsize);
    if (n == nullptr) { return nullptr; }
    if (ptr != nullptr) { g_fake_bytes -= g_fake_live[ptr]; g_fake_live.erase(ptr); }
    g_fake_live[n] = nsize; g_fake_bytes += nsize; ++g_fake_allocs;
    return n;
}
static void fake_free(void* ptr) noexcept
{
    std::lock_guard<std::mutex> g{g_fake_mutex};
    auto it = g_fake_live.find(ptr);
    if (it == g_fake_live.end()) { ++g_fake_foreign; return; } // would be a cross-store free
    g_fake_bytes -= it->second; g_fake_live.erase(it); ++g_fake_frees;
    std::free(ptr);
}

static int g_checks = 0, g_fail = 0;
static void check(bool ok, const char* what)
{
    ++g_checks; if (!ok) { ++g_fail; }
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
}

static bool run(lua_State* L, const char* code)
{
    if (luaL_loadstring(L, code) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        std::printf("    lua error: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); return false;
    }
    return true;
}

// ---------------- replica for check 8 ----------------
static bool g_flag = false;
struct ReplicaGuard { ReplicaGuard() noexcept { g_flag = true; } ~ReplicaGuard() { g_flag = false; } };
template <typename F> static void replica_try(F f)
{
    try { f(); }
    catch (std::exception&) { throw std::bad_alloc{}; } // the "Error: {}" log line itself fails to allocate
}

int main()
{
    std::printf("R9-MEM host test (Mac-only; staged allocator vs real Lua 5.4 C objects)\n");

    // 1. default: libc
    lua_State* L_old = test_new_state();
    luaL_openlibs(L_old);
    auto s1 = query_lua_memory(L_old);
    check(L_old != nullptr && s1.be_installed == 0 && s1.libc_states == 1 && s1.be_states == 0, "1 no backing installed -> libc state");
    check(run(L_old, "local t={} for i=1,10000 do t[i]=tostring(i) end"), "1 libc state runs Lua");

    // 2. install once
    check(!set_lua_backing_allocator(LuaBackingAllocator{nullptr, &fake_free}), "2 null realloc refused");
    check(set_lua_backing_allocator(LuaBackingAllocator{&fake_realloc, &fake_free}), "2 first install accepted");
    check(!set_lua_backing_allocator(LuaBackingAllocator{&fake_realloc, &fake_free}), "2 second install refused");

    // 3. new state on backing
    const size_t libc_live_before = query_lua_memory(nullptr).live_bytes - query_lua_memory(nullptr).be_live_bytes;
    lua_State* L_new = test_new_state();
    luaL_openlibs(L_new);
    auto s3 = query_lua_memory(L_new);
    check(L_new != nullptr && s3.be_installed == 1 && s3.be_states == 1 && s3.libc_states == 1, "3 state after install is a backing state");
    check(run(L_new, "local t={} for i=1,50000 do t[i]={i, tostring(i)} end t=nil collectgarbage()"), "3 backing state runs Lua");
    check(g_fake_allocs > 1000 && g_fake_foreign == 0, "3 backing store served the allocations, no foreign pointer");
    check(query_lua_memory(nullptr).be_live_bytes == g_fake_bytes, "3 be_live_bytes == bytes the backing store holds");

    // 4. old libc state never touches the fake
    const size_t fake_allocs_before_old = g_fake_allocs;
    check(run(L_old, "local t={} for i=1,20000 do t[i]={i} end t=nil collectgarbage()"), "4 old libc state still runs");
    check(g_fake_allocs == fake_allocs_before_old && g_fake_foreign == 0, "4 old state never reached the backing store");

    // 5. coroutines
    const size_t before_co = g_fake_allocs;
    check(run(L_new, "local co=coroutine.create(function() local s='' for i=1,2000 do s=s..'x' coroutine.yield(#s) end end) for i=1,2000 do coroutine.resume(co) end"), "5 coroutine runs on backing state");
    check(g_fake_allocs > before_co && g_fake_foreign == 0, "5 coroutine allocations stayed on the backing store");

    // 6. ForEachUObject-shaped burst: 200k userdata with user values, then GC
    lua_gc(L_new, LUA_GCCOLLECT);
    const size_t base6 = g_fake_bytes;
    for (int i = 0; i < 200000; ++i)
    {
        void* ud = lua_newuserdatauv(L_new, 48, 5);
        std::memset(ud, 0, 48);
        lua_pushinteger(L_new, i); lua_setiuservalue(L_new, -2, 2);
        lua_pop(L_new, 1);
    }
    lua_gc(L_new, LUA_GCCOLLECT);
    std::printf("    burst: base=%zu after-gc=%zu bytes\n", base6, g_fake_bytes);
    check(g_fake_bytes <= base6 + 64 * 1024 && g_fake_foreign == 0, "6 200k-userdata burst returns to baseline after GC");

    // 7. backing failure -> LUA_ERRMEM, no crash, counters
    const auto s7a = query_lua_memory(nullptr);
    g_fake_fail_after = 50;
    luaL_loadstring(L_new, "local t={} for i=1,1e7 do t[i]=tostring(i)..'pad' end");
    const int st = lua_pcall(L_new, 0, 0, 0);
    const char* msg = lua_tostring(L_new, -1);
    std::printf("    pcall status=%d msg=%s\n", st, msg ? msg : "(nil)");
    const bool errmem = st == LUA_ERRMEM && msg && std::strstr(msg, "not enough memory");
    lua_pop(L_new, 1);
    g_fake_fail_after = -1;
    const auto s7b = query_lua_memory(nullptr);
    check(errmem, "7 backing failure surfaces as LUA_ERRMEM 'not enough memory'");
    check(s7b.be_failed_calls > s7a.be_failed_calls && s7b.failed_calls > s7a.failed_calls, "7 be_fails and fails counted");
    check(run(L_new, "collectgarbage() local x = {1,2,3}"), "7 state usable after the failure");

    // 3 (end). close returns everything
    lua_close(L_new);
    check(g_fake_live.empty() && g_fake_bytes == 0 && g_fake_foreign == 0, "3 lua_close frees every backing block (0 live, 0 foreign)");
    check(query_lua_memory(nullptr).be_live_bytes == 0, "3 be_live_bytes back to 0 after close");
    lua_close(L_old);
    (void)libc_live_before;
    check(query_lua_memory(nullptr).live_bytes == 0, "4 total live_bytes back to 0 after both closes");

    // 8. replica: old pattern vs guard
    g_flag = false;
    try { g_flag = true; replica_try([] { throw std::runtime_error{"lua_pcall returned LUA_ERRMEM"}; }); g_flag = false; } catch (...) {}
    const bool old_stuck = g_flag;
    g_flag = false;
    try { const ReplicaGuard g{}; replica_try([] { throw std::runtime_error{"lua_pcall returned LUA_ERRMEM"}; }); } catch (...) {}
    check(old_stuck, "8 replica OLD: flag stays TRUE after the handler throws (defect reproduced)");
    check(!g_flag, "8 replica NEW: RAII guard resets the flag");

    std::printf("%s r9-mem host test (%d checks, %d failures)\n", (g_fail == 0 && g_checks > 0) ? "PASS" : "FAIL", g_checks, g_fail);
    return (g_fail == 0 && g_checks > 0) ? 0 : 1;
}
