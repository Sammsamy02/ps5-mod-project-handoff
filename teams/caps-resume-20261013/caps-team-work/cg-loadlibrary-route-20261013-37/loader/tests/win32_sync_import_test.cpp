#include "win_api_shim.h"
#include "win64_bridge.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>

static int failures = 0;
static void check(bool ok, const char* label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}

static bool allocate_writable(size_t minimum, void** address, size_t* bytes, void*)
{
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* mapping = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) return false;
    *address = mapping;
    *bytes = page;
    return true;
}
static bool seal_executable(void* address, size_t bytes, void*)
{
    return mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}
static void release_mapping(void* address, size_t bytes, void*)
{
    munmap(address, bytes);
}
static bool wait_until(std::atomic<int>& value, int goal)
{
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (value.load() < goal)
    {
        if (std::chrono::steady_clock::now() > limit) return false;
        std::this_thread::yield();
    }
    return true;
}

struct StopContext
{
    std::atomic<int> calls{0};
    void* wake_entry = nullptr;
    uint64_t condition = 0;
    bool reenter = false;
};

int main()
{
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = allocate_writable;
    backend.seal_executable = seal_executable;
    backend.release = release_mapping;
    check(shim.set_executable_backend(backend), "executable-memory backend accepted");
    StopContext stop{};
    std::atomic<int>& stopped = stop.calls;
    cap13::WinApiShim::Hooks hooks{};
    hooks.stop_mod = +[](void* user, const char*) {
        auto& state = *static_cast<StopContext*>(user);
        state.calls.fetch_add(1);
        if (state.reenter)
            (void)cap13_win64_call1(state.wake_entry, state.condition);
    };
    hooks.stop_user = &stop;
    shim.set_hooks(hooks);

    void* acquire = shim.resolve("kernel32.dll", "AcquireSRWLockExclusive", 0);
    void* release = shim.resolve("kernel32.dll", "ReleaseSRWLockExclusive", 0);
    void* acquire_shared = shim.resolve("kernel32.dll", "AcquireSRWLockShared", 0);
    void* release_shared = shim.resolve("kernel32.dll", "ReleaseSRWLockShared", 0);
    void* sleep_cv = shim.resolve("kernel32.dll", "SleepConditionVariableSRW", 0);
    void* wake_all = shim.resolve("kernel32.dll", "WakeAllConditionVariable", 0);
    void* get_error = shim.resolve("kernel32.dll", "GetLastError", 0);
    void* initialize_slist = shim.resolve("kernel32.dll", "InitializeSListHead", 0);
    check(acquire && release && acquire_shared && release_shared &&
          sleep_cv && wake_all && get_error && initialize_slist,
          "six synchronization rows, GetLastError, and S-list init resolve");
    if (!acquire || !release || !acquire_shared || !release_shared ||
        !sleep_cv || !wake_all || !get_error || !initialize_slist) return 1;

    alignas(8) uint64_t lock_word = 0;
    alignas(8) uint64_t cv_word = 0;
    const uint64_t lock = reinterpret_cast<uint64_t>(&lock_word);
    const uint64_t cv = reinterpret_cast<uint64_t>(&cv_word);
    stop.wake_entry = wake_all;
    stop.condition = cv;
    (void)cap13_win64_call1(acquire, lock);
    std::atomic<int> entered{0};
    std::thread blocked([&] {
        (void)cap13_win64_call1(acquire, lock);
        entered = 1;
        (void)cap13_win64_call1(release, lock);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    const bool stayed_blocked = entered.load() == 0;
    (void)cap13_win64_call1(release, lock);
    blocked.join();
    check(stayed_blocked && entered == 1 && stopped == 0,
          "exclusive SRW lock blocks another thread until release");

    std::atomic<int> waiting{0};
    std::atomic<int> woke{0};
    auto waiter = [&] {
        (void)cap13_win64_call1(acquire, lock);
        waiting.fetch_add(1);
        if (cap13_win64_call4(sleep_cv, cv, lock, 0xffffffffu, 0) != 0)
            woke.fetch_add(1);
        (void)cap13_win64_call1(release, lock);
    };
    std::thread one(waiter);
    std::thread two(waiter);
    const bool both_entered = wait_until(waiting, 2);
    if (both_entered)
    {
        (void)cap13_win64_call1(acquire, lock);
        (void)cap13_win64_call1(release, lock);
        (void)cap13_win64_call1(wake_all, cv);
    }
    one.join();
    two.join();
    check(both_entered && woke == 2 && stopped == 0,
          "wake-all releases and reacquires SRW lock for two waiters");

    (void)cap13_win64_call1(acquire, lock);
    const bool timed_out = cap13_win64_call4(sleep_cv, cv, lock, 0, 0) == 0;
    const uint32_t timeout_error = static_cast<uint32_t>(cap13_win64_call0(get_error));
    (void)cap13_win64_call1(release, lock);
    check(timed_out && timeout_error == 1460 && stopped == 0,
          "zero timeout returns FALSE and ERROR_TIMEOUT with lock reacquired");

    (void)cap13_win64_call1(acquire_shared, lock);
    std::atomic<int> shared_entered{0};
    std::thread reader([&] {
        (void)cap13_win64_call1(acquire_shared, lock);
        shared_entered = 1;
        (void)cap13_win64_call1(release_shared, lock);
    });
    const bool concurrent_readers = wait_until(shared_entered, 1);
    (void)cap13_win64_call1(release_shared, lock);
    reader.join();
    check(concurrent_readers && stopped == 0,
          "shared SRW acquisition allows a second reader concurrently");

    (void)cap13_win64_call1(acquire_shared, lock);
    const bool shared_timeout = cap13_win64_call4(sleep_cv, cv, lock, 0, 1) == 0;
    (void)cap13_win64_call1(release_shared, lock);
    check(shared_timeout && stopped == 0 &&
          static_cast<uint32_t>(cap13_win64_call0(get_error)) == 1460,
          "shared condition timeout reacquires the shared lock");

    const int before_unowned = stopped;
    stop.reenter = true;
    const bool unowned_refused = cap13_win64_call4(sleep_cv, cv, lock, 0, 0) == 0;
    stop.reenter = false;
    check(unowned_refused && stopped == before_unowned + 1,
          "invalid ownership stop hook can reenter condition wake");

    const int before_bad = stopped;
    (void)cap13_win64_call1(acquire, 0);
    (void)cap13_win64_call1(acquire, lock);
    const bool invalid_flags = cap13_win64_call4(sleep_cv, cv, lock, 0, 2) == 0;
    (void)cap13_win64_call1(release, lock);
    check(invalid_flags && stopped == before_bad + 2,
          "null lock and unsupported condition flags stop this mod");

    alignas(16) uint8_t slist_bytes[48];
    std::memset(slist_bytes, 0xa5, sizeof(slist_bytes));
    uint8_t* const slist_head = slist_bytes + 16;
    (void)cap13_win64_call1(initialize_slist,
                            reinterpret_cast<uint64_t>(slist_head));
    bool bounded_zero = true;
    for (size_t i = 0; i < sizeof(slist_bytes); ++i)
        bounded_zero &= slist_bytes[i] == ((i >= 16 && i < 32) ? 0 : 0xa5);
    check(bounded_zero, "S-list init zeroes exactly one aligned 16-byte x64 header");
    std::memset(slist_head, 0x5a, 16);
    (void)cap13_win64_call1(initialize_slist,
                            reinterpret_cast<uint64_t>(slist_head));
    bool cleared_again = true;
    for (size_t i = 0; i < 16; ++i) cleared_again &= slist_head[i] == 0;
    check(cleared_again, "S-list head can be reinitialized");
    uint8_t before_reject[sizeof(slist_bytes)];
    std::memcpy(before_reject, slist_bytes, sizeof(slist_bytes));
    const int before_misaligned = stopped;
    (void)cap13_win64_call1(initialize_slist,
                            reinterpret_cast<uint64_t>(slist_head + 1));
    (void)cap13_win64_call1(initialize_slist, 0);
    check(stopped == before_misaligned + 2 &&
          std::memcmp(before_reject, slist_bytes, sizeof(slist_bytes)) == 0,
          "null and misaligned S-list heads are refused before writing");

    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
