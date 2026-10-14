#include "win_api_shim.h"
#include "win64_bridge.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>

static int failures = 0;
static void check(bool ok, const char* label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}

struct MemoryState
{
    cap13::WinApiShim* shim = nullptr;
    const char* active_symbol = nullptr;
    int allocated = 0;
    int sealed = 0;
    int released = 0;
    bool reject_seal = false;
    bool hidden_during_seal = true;
};

static bool allocate_writable(size_t minimum, void** address, size_t* bytes, void* user)
{
    auto& state = *static_cast<MemoryState*>(user);
    ++state.allocated;
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* mapping = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) return false;
    *address = mapping;
    *bytes = page;
    return true;
}

static bool seal_executable(void* address, size_t bytes, void* user)
{
    auto& state = *static_cast<MemoryState*>(user);
    ++state.sealed;
    if (state.shim->resolve("KERNEL32.DLL", state.active_symbol, 0))
        state.hidden_during_seal = false;
    if (state.reject_seal) return false;
    return mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}

static void release_mapping(void* address, size_t bytes, void* user)
{
    auto& state = *static_cast<MemoryState*>(user);
    ++state.released;
    munmap(address, bytes);
}

static uint64_t registered_sleep() { return 0; }
static bool registered_lookup(cap13::WinModule, const char* symbol, void** result)
{
    if (symbol && __builtin_strcmp(symbol, "Sleep") == 0)
    {
        *result = reinterpret_cast<void*>(&registered_sleep);
        return true;
    }
    return false;
}

struct RaceState
{
    std::atomic<int> allocated{0};
    std::atomic<int> sealing{0};
    std::atomic<int> released{0};
};
static bool race_allocate(size_t minimum, void** address, size_t* bytes, void* user)
{
    auto& state = *static_cast<RaceState*>(user);
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* mapping = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) return false;
    state.allocated.fetch_add(1);
    *address = mapping;
    *bytes = page;
    return true;
}
static bool race_seal(void* address, size_t bytes, void* user)
{
    auto& state = *static_cast<RaceState*>(user);
    state.sealing.fetch_add(1);
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (state.sealing.load() < 2)
    {
        if (std::chrono::steady_clock::now() > limit) return false;
        std::this_thread::yield();
    }
    return mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}
static void race_release(void* address, size_t bytes, void* user)
{
    auto& state = *static_cast<RaceState*>(user);
    state.released.fetch_add(1);
    munmap(address, bytes);
}

int main()
{
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    check(shim.resolve("kernel32.dll", "Sleep", 0) == nullptr,
          "without a platform backend reviewed imports remain unresolved");
    MemoryState state{};
    state.shim = &shim;
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = allocate_writable;
    backend.seal_executable = seal_executable;
    backend.release = release_mapping;
    backend.user = &state;
    check(shim.set_executable_backend(backend),
          "complete executable-memory backend is accepted");
    check(shim.resolve("kernel32.dll", "VirtualAlloc", 0) == nullptr &&
          shim.resolve("kernel32.dll", "Imaginary", 0) == nullptr &&
          state.allocated == 0,
          "unreviewed and unknown imports do not allocate executable memory");

    state.active_symbol = "GetCurrentThreadId";
    void* thread_entry = shim.resolve("KERNEL32.DLL", state.active_symbol, 0);
    check(thread_entry && state.allocated == 1 && state.sealed == 1 &&
          state.released == 0 && state.hidden_during_seal,
          "first resolution seals a trampoline while its import is hidden");
    if (thread_entry)
    {
        const uint32_t first = static_cast<uint32_t>(cap13_win64_call0(thread_entry));
        check(first != 0 && first == cap13_win64_call0(thread_entry),
              "lazy Win64 import calls its typed SysV handler");
    }
    check(shim.resolve("kernel32.dll", "GetCurrentThreadId", 0) == thread_entry &&
          state.allocated == 1,
          "repeated import resolution returns one retained trampoline");

    state.active_symbol = "GetCurrentProcessId";
    state.reject_seal = true;
    check(shim.resolve("kernel32.dll", state.active_symbol, 0) == nullptr &&
          state.allocated == 2 && state.sealed == 2 && state.released == 1,
          "failed executable sealing leaves import unresolved and releases memory");
    state.reject_seal = false;
    void* process_entry = shim.resolve("kernel32.dll", state.active_symbol, 0);
    check(process_entry && state.allocated == 3 && state.sealed == 3 &&
          state.released == 1 && state.hidden_during_seal &&
          static_cast<uint32_t>(cap13_win64_call0(process_entry)) ==
          static_cast<uint32_t>(getpid()),
          "a later successful resolve installs the previously refused import");

    cap13::WinApiShim registered;
    registered.register_builtin_exports();
    MemoryState registered_state{};
    registered_state.shim = &registered;
    backend.user = &registered_state;
    check(registered.set_executable_backend(backend), "second backend is accepted");
    registered.register_module("KERNEL32.DLL", nullptr, registered_lookup);
    check(registered.resolve("kernel32.dll", "Sleep", 0) ==
          reinterpret_cast<void*>(&registered_sleep) &&
          registered_state.allocated == 0,
          "registered module export retains priority over lazy built-ins");

    cap13::WinApiShim racing;
    racing.register_builtin_exports();
    RaceState race{};
    backend.allocate_writable = race_allocate;
    backend.seal_executable = race_seal;
    backend.release = race_release;
    backend.user = &race;
    check(racing.set_executable_backend(backend), "race backend is accepted");
    void* first_entry = nullptr;
    void* second_entry = nullptr;
    std::thread first([&] { first_entry = racing.resolve("kernel32.dll", "Sleep", 0); });
    std::thread second([&] { second_entry = racing.resolve("KERNEL32.DLL", "Sleep", 0); });
    first.join();
    second.join();
    check(first_entry && first_entry == second_entry &&
          racing.resolve("kernel32.dll", "Sleep", 0) == first_entry &&
          race.allocated.load() == 2 && race.sealing.load() == 2 &&
          race.released.load() == 1,
          "concurrent install keeps one published trampoline and releases the loser");
    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
