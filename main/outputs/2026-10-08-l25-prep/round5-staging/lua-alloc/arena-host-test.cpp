// Host-side functional test of the [IS-LUA-ALLOC] arena allocator logic.
// Extracted verbatim from staged LuaMadeSimple.cpp (arena region) by the
// staging generator; battery-safe local test only.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <vector>
#include <cassert>
#include <unordered_set>

struct lua_State;
static const char* lua_tostring(lua_State*, int) { return "panic-stub"; }

struct LuaMemorySnapshot
{
    int gc_kb{0};
    size_t live_bytes{0};
    size_t live_blocks{0};
    size_t total_alloc_calls{0};
    size_t failed_calls{0};
    size_t last_fail_osize{0};
    size_t last_fail_nsize{0};
    size_t arena_capacity_bytes{0};
    size_t arena_used_bytes{0};
    size_t arena_external_bytes{0};
};

namespace lms {
    struct LuaAllocStats
    {
        std::atomic<size_t> live_bytes{0};
        std::atomic<size_t> live_blocks{0};
        std::atomic<size_t> total_alloc_calls{0};
        std::atomic<size_t> failed_calls{0};
        std::atomic<size_t> last_fail_osize{0};
        std::atomic<size_t> last_fail_nsize{0};
    };
    static LuaAllocStats g_lua_alloc_stats;

    // Mirrors lauxlib.c's panic/warnfoff so lua_newstate stays luaL_newstate-
    // equivalent (we no longer call luaL_newstate; see new_state below).
    static auto lms_lua_panic(lua_State* lua_state) -> int
    {
        // Narrow printf only: this file predates DynamicOutput (first layer).
        std::fprintf(stderr, "PANIC: unprotected error in call to Lua API (%s)\n",
                     lua_tostring(lua_state, -1) ? lua_tostring(lua_state, -1) : "unknown");
        return 0;
    }

    static void lms_warn_off([[maybe_unused]] void* ud, [[maybe_unused]] const char* message, [[maybe_unused]] int tocont)
    {
        // Default lauxlib behavior: warnings off.
    }

    // =================================================================================
    // [IS-LUA-ALLOC 0001] counting passthrough allocator (default path).
    // [IS-LUA-ALLOC 0002] opt-in dedicated arena for small blocks
    // (env UE4SS_LUA_ARENA=1, size via UE4SS_LUA_ARENA_MB, 8..512, default 64).
    //
    // When the arena is OFF (default) every block is plain realloc/free memory
    // and NO header is ever read or written: byte-identical behavior to the
    // previous luaL_newstate/l_alloc, plus counters.
    //
    // When the arena is ON, every block gets a 16-byte header immediately
    // before the user pointer (so user pointers stay 16-byte aligned). Small
    // payloads (<= 2048 B) come from the reservation and recycle through
    // per-class intrusive free lists; larger payloads are malloc-backed WITH a
    // header (so ownership checks are always in-bounds). Resize-in-place is
    // honored whenever the header capacity already fits the request.
    //
    // LAWS: no new link-time imports (frozen 207-import contract; only
    // malloc/realloc/free/getenv, all already imported); disabled by default;
    // reservation taken once, before the first Lua state exists, while the
    // game-process libc heap is healthy - after that, engine save/load
    // pressure cannot starve small Lua allocations.
    // =================================================================================
    constexpr size_t kArenaHeaderSize = 16; // keeps 16-byte user alignment
    constexpr size_t kArenaMaxSmall = 2048;
    // 32 canonical size classes: 16..256 step 16, 320..1024 step 64,
    // 1280..2048 step 256. Larger requests never use the reservation.
    constexpr size_t kArenaBinSizes[32] = {
        16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256,
        320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960, 1024,
        1280, 1536, 1792, 2048};

    struct ArenaHeader
    {
        uint64_t capacity;   // payload bytes available in this block
        uint32_t arena_owned; // 1 = reservation block, 0 = malloc-backed
        uint32_t bin_index;   // valid when arena_owned == 1
    };
    static_assert(sizeof(ArenaHeader) <= kArenaHeaderSize, "header must fit its slot");

    struct LuaArena
    {
        std::once_flag init_once{};
        bool enabled{false};
        unsigned char* base{nullptr};
        size_t capacity{0};
        size_t bump{0};
        std::mutex mutex{};
        void* free_lists[32]{};
        size_t free_counts[32]{0};
        std::atomic<size_t> capacity_bytes{0};
        std::atomic<size_t> used_bytes{0};
        std::atomic<size_t> external_bytes{0};
    };
    static LuaArena g_arena_state;

    static size_t arena_bin_for(size_t n)
    {
        for (size_t i = 0; i < 32; ++i)
        {
            if (kArenaBinSizes[i] >= n) { return i; }
        }
        return SIZE_MAX;
    }

    static void arena_init()
    {
        const char* toggle = getenv("UE4SS_LUA_ARENA");
        if (toggle == nullptr || toggle[0] != '1') { return; }
        size_t want_mb = 64;
        if (const char* mb = getenv("UE4SS_LUA_ARENA_MB"); mb != nullptr)
        {
            const long parsed = strtol(mb, nullptr, 10);
            if (parsed >= 8 && parsed <= 512) { want_mb = static_cast<size_t>(parsed); }
        }
        // Reserve while the process is healthy; halve down to 8 MiB before
        // giving up (disabled = exactly the 0001 behavior).
        for (size_t attempt = want_mb; attempt >= 8; attempt /= 2)
        {
            const size_t bytes = attempt * 1024 * 1024;
            void* reservation = malloc(bytes);
            if (reservation != nullptr)
            {
                g_arena_state.base = static_cast<unsigned char*>(reservation);
                g_arena_state.capacity = bytes;
                g_arena_state.capacity_bytes.store(bytes, std::memory_order_relaxed);
                g_arena_state.enabled = true;
                return;
            }
        }
    }

    static void arena_ensure_init()
    {
        std::call_once(g_arena_state.init_once, arena_init);
    }

    // Intrusive free list: a free reservation block stores the next pointer in
    // its own payload (blocks are >= 16 B, so pointer storage is guaranteed).
    static void* arena_take_bin(size_t bin)
    {
        void* block = g_arena_state.free_lists[bin];
        if (block != nullptr)
        {
            g_arena_state.free_lists[bin] = *static_cast<void**>(block);
            g_arena_state.free_counts[bin] -= 1;
        }
        return block;
    }

    static void arena_push_bin(size_t bin, void* block)
    {
        *static_cast<void**>(block) = g_arena_state.free_lists[bin];
        g_arena_state.free_lists[bin] = block;
        g_arena_state.free_counts[bin] += 1;
    }

    // Returns a 16-byte-aligned user pointer with a populated header, or
    // nullptr when the reservation cannot serve it (caller falls back).
    static void* arena_alloc_small(size_t n)
    {
        const size_t bin = arena_bin_for(n);
        if (bin == SIZE_MAX) { return nullptr; }
        const size_t csize = kArenaBinSizes[bin];

        void* user = nullptr;
        {
            std::lock_guard<std::mutex> guard{g_arena_state.mutex};
            user = arena_take_bin(bin);
            if (user == nullptr)
            {
                const size_t needed = kArenaHeaderSize + csize;
                if (g_arena_state.bump + needed > g_arena_state.capacity) { return nullptr; }
                user = g_arena_state.base + g_arena_state.bump + kArenaHeaderSize;
                g_arena_state.bump += needed;
                g_arena_state.used_bytes.fetch_add(needed, std::memory_order_relaxed);
            }
        }
        auto* header = reinterpret_cast<ArenaHeader*>(static_cast<unsigned char*>(user) - kArenaHeaderSize);
        header->capacity = csize;
        header->arena_owned = 1;
        header->bin_index = static_cast<uint32_t>(bin);
        return user;
    }

    static void* arena_alloc_large(size_t n)
    {
        void* raw = malloc(kArenaHeaderSize + n);
        if (raw == nullptr) { return nullptr; }
        auto* header = static_cast<ArenaHeader*>(raw);
        header->capacity = n;
        header->arena_owned = 0;
        header->bin_index = 0;
        return static_cast<unsigned char*>(raw) + kArenaHeaderSize;
    }

    static void arena_free_user(void* user)
    {
        auto* header = reinterpret_cast<ArenaHeader*>(static_cast<unsigned char*>(user) - kArenaHeaderSize);
        if (header->arena_owned == 1)
        {
            std::lock_guard<std::mutex> guard{g_arena_state.mutex};
            arena_push_bin(header->bin_index, user);
        }
        else
        {
            free(header);
        }
    }

    static void* lms_counting_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
    {
        (void)ud;
        arena_ensure_init();
        g_lua_alloc_stats.total_alloc_calls.fetch_add(1, std::memory_order_relaxed);

        const auto note_failure = [&]()
        {
            // Lua 5.4 core already ran its emergency full GC and retried once
            // (lmem.c luaM_realloc_) before this returned NULL, so a NULL here
            // is a real backing-store failure, not collectable garbage.
            g_lua_alloc_stats.failed_calls.fetch_add(1, std::memory_order_relaxed);
            g_lua_alloc_stats.last_fail_osize.store(osize, std::memory_order_relaxed);
            g_lua_alloc_stats.last_fail_nsize.store(nsize, std::memory_order_relaxed);
        };
        const auto account_success = [&]()
        {
            if (ptr == nullptr)
            {
                g_lua_alloc_stats.live_blocks.fetch_add(1, std::memory_order_relaxed);
                g_lua_alloc_stats.live_bytes.fetch_add(nsize, std::memory_order_relaxed);
            }
            else
            {
                const long long delta = static_cast<long long>(nsize) - static_cast<long long>(osize);
                g_lua_alloc_stats.live_bytes.fetch_add(static_cast<size_t>(delta), std::memory_order_relaxed);
            }
        };

        if (nsize == 0)
        {
            if (ptr != nullptr)
            {
                if (g_arena_state.enabled)
                {
                    arena_free_user(ptr);
                }
                else
                {
                    free(ptr);
                }
                g_lua_alloc_stats.live_blocks.fetch_sub(1, std::memory_order_relaxed);
                g_lua_alloc_stats.live_bytes.fetch_sub(osize, std::memory_order_relaxed);
            }
            return nullptr;
        }

        if (!g_arena_state.enabled)
        {
            // ===== 0001 default: byte-identical realloc path, plus counters =====
            void* nptr = realloc(ptr, nsize);
            if (nptr == nullptr)
            {
                note_failure();
                return nullptr;
            }
            account_success();
            return nptr;
        }

        // ===== 0002 arena path (every block has a header) =====
        if (ptr == nullptr)
        {
            void* user = nullptr;
            if (nsize <= kArenaMaxSmall)
            {
                user = arena_alloc_small(nsize);
            }
            if (user == nullptr)
            {
                user = arena_alloc_large(nsize);
                if (user != nullptr)
                {
                    g_arena_state.external_bytes.fetch_add(nsize, std::memory_order_relaxed);
                }
            }
            if (user == nullptr)
            {
                note_failure();
                return nullptr;
            }
            account_success();
            return user;
        }

        auto* header = reinterpret_cast<ArenaHeader*>(static_cast<unsigned char*>(ptr) - kArenaHeaderSize);
        if (nsize <= header->capacity)
        {
            // Fits the existing block (same-class grow or any shrink): no copy.
            account_success();
            return ptr;
        }
        // Growth beyond capacity: new block, copy old logical bytes, free old.
        void* fresh = nullptr;
        if (nsize <= kArenaMaxSmall)
        {
            fresh = arena_alloc_small(nsize);
        }
        if (fresh == nullptr)
        {
            fresh = arena_alloc_large(nsize);
            if (fresh != nullptr)
            {
                g_arena_state.external_bytes.fetch_add(nsize, std::memory_order_relaxed);
            }
        }
        if (fresh == nullptr)
        {
            note_failure();
            return nullptr;
        }
        memcpy(fresh, ptr, osize < nsize ? osize : nsize);
        if (header->arena_owned == 0)
        {
            g_arena_state.external_bytes.fetch_sub(osize, std::memory_order_relaxed);
        }
        arena_free_user(ptr);
        account_success();
        return fresh;
    }


static LuaMemorySnapshot host_query()
{
    LuaMemorySnapshot snap{};
    snap.live_bytes = g_lua_alloc_stats.live_bytes.load(std::memory_order_relaxed);
    snap.live_blocks = g_lua_alloc_stats.live_blocks.load(std::memory_order_relaxed);
    snap.total_alloc_calls = g_lua_alloc_stats.total_alloc_calls.load(std::memory_order_relaxed);
    snap.failed_calls = g_lua_alloc_stats.failed_calls.load(std::memory_order_relaxed);
    snap.arena_capacity_bytes = g_arena_state.capacity_bytes.load(std::memory_order_relaxed);
    snap.arena_used_bytes = g_arena_state.used_bytes.load(std::memory_order_relaxed);
    snap.arena_external_bytes = g_arena_state.external_bytes.load(std::memory_order_relaxed);
    return snap;
}
} // namespace lms

using namespace lms;

int main()
{
    arena_init(); // env unset -> disabled; force a small arena for the test
    assert(!g_arena_state.enabled && "env-off must stay disabled");
    g_arena_state.base = static_cast<unsigned char*>(malloc(1 << 20));
    assert(g_arena_state.base);
    g_arena_state.capacity = 1 << 20;
    g_arena_state.capacity_bytes.store(1 << 20, std::memory_order_relaxed);
    g_arena_state.enabled = true;

    std::vector<void*> live;
    std::unordered_set<void*> unique;
    srand(1234);
    for (int round = 0; round < 200000; ++round)
    {
        int op = rand() % 3;
        if (op != 0 && !live.empty())
        {
            size_t idx = rand() % live.size();
            void* p = live[idx];
            memset(p, 0xAB, 1); // must still be owned
            live.erase(live.begin() + idx);
            unique.erase(p);
            lms_counting_alloc(nullptr, p, 64, 0);
        }
        else
        {
            size_t n = 1 + static_cast<size_t>(rand() % 3000);
            void* p = lms_counting_alloc(nullptr, nullptr, 0, n);
            if (p)
            {
                memset(p, 0xCD, n);
                assert((reinterpret_cast<uintptr_t>(p) % 16) == 0 && "user alignment");
                assert(unique.find(p) == unique.end() && "duplicate pointer");
                unique.insert(p);
                live.push_back(p);
            }
        }
    }

    // resize semantics
    void* p1 = lms_counting_alloc(nullptr, nullptr, 0, 100);
    assert(p1);
    void* p2 = lms_counting_alloc(nullptr, p1, 100, 150); // may be in-place or moved
    assert(p2);
    memset(p2, 0x5A, 150);
    void* p3 = lms_counting_alloc(nullptr, p2, 150, 3000); // beyond arena max -> malloc-backed w/ header
    assert(p3);
    memset(p3, 0x5A, 3000);
    void* p4 = lms_counting_alloc(nullptr, p3, 3000, 10); // shrink must stay in place
    assert(p4 == p3 && "shrink keeps block");

    for (void* p : live) lms_counting_alloc(nullptr, p, 64, 0);
    lms_counting_alloc(nullptr, p4, 10, 0);

    auto snap = host_query();
    std::printf("PASS arena-churn blocks=%zu live_kb=%zu arena_used_kb=%zu ext_kb=%zu fails=%zu allocs=%zu\n",
                snap.live_blocks, snap.live_bytes / 1024, snap.arena_used_bytes / 1024,
                snap.arena_external_bytes / 1024, snap.failed_calls, snap.total_alloc_calls);
    assert(snap.live_blocks == 0 && "everything freed");
    return 0;
}
