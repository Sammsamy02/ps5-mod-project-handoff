#include "win_api_shim.h"
#include "win64_bridge.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>

static int failures = 0;
static int seal_calls = 0;
static bool stamp_seen = false;
static bool visible_during_seal = false;
static cap13::WinApiShim* active_shim = nullptr;
static const char* sealing_symbol = nullptr;

static void check(bool condition, const char* label)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) ++failures;
}

static bool reject_seal(void*, size_t, void*)
{
    ++seal_calls;
    return false;
}

static bool seal_rx(void* memory, size_t bytes, void*)
{
    ++seal_calls;
    visible_during_seal |= active_shim->resolve("kernel32.dll", sealing_symbol, 0) != nullptr;
    const auto* code = static_cast<const uint8_t*>(memory);
    stamp_seen = code[0] == 0x49 && code[1] == 0xBA &&
                 code[10] == 0x49 && code[11] == 0xBB &&
                 code[20] == 0x48 && code[21] == 0xB8 &&
                 code[30] == 0xFF && code[31] == 0xE0;
    return stamp_seen && mprotect(memory, bytes, PROT_READ | PROT_EXEC) == 0;
}

int main()
{
    using cap13::BuiltinInstallStatus;
    const long page_bytes = sysconf(_SC_PAGESIZE);
    if (page_bytes < 4096) return 2;
    auto map_page = [=]() -> void* {
        void* p = mmap(nullptr, static_cast<size_t>(page_bytes),
                       PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        return p == MAP_FAILED ? nullptr : p;
    };
    void* sleep_page = map_page();
    void* tick_page = map_page();
    void* spare_page = map_page();
    void* process_page = map_page();
    void* thread_page = map_page();
    void* filetime_page = map_page();
    if (!sleep_page || !tick_page || !spare_page || !process_page ||
        !thread_page || !filetime_page) return 2;

    cap13::WinApiShim shim;
    active_shim = &shim;
    cap13::WinApiShim empty_shim;
    check(empty_shim.install_builtin_integer_wrapper("kernel32.dll", "Sleep", spare_page,
          static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Unsupported,
          "unregistered catalog cannot publish an import");
    shim.register_builtin_exports();
    check(shim.resolve("kernel32.dll", "Sleep", 0) == nullptr &&
          shim.resolve("kernel32.dll", "GetTickCount", 0) == nullptr &&
          shim.resolve("kernel32.dll", "GetCurrentProcessId", 0) == nullptr &&
          shim.resolve("kernel32.dll", "GetCurrentThreadId", 0) == nullptr &&
          shim.resolve("kernel32.dll", "GetSystemTimeAsFileTime", 0) == nullptr,
          "catalog names remain unresolved before executable sealing");
    check(shim.install_builtin_integer_wrapper("kernel32.dll", "Imaginary", spare_page,
          static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Unsupported,
          "unknown row is refused before stamping or sealing");
    check(shim.install_builtin_integer_wrapper("kernel32.dll", "VirtualAlloc", spare_page,
          static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Unsupported,
          "unreviewed Windows API stays unresolved");
    check(shim.install_builtin_integer_wrapper("kernel32.dll", "Sleep", nullptr,
          static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::InvalidInput &&
          shim.install_builtin_integer_wrapper("kernel32.dll", "Sleep", sleep_page,
          31, seal_rx, nullptr) == BuiltinInstallStatus::InvalidInput &&
          shim.install_builtin_integer_wrapper("kernel32.dll", "Sleep", sleep_page,
          static_cast<size_t>(page_bytes), nullptr, nullptr) ==
          BuiltinInstallStatus::InvalidInput,
          "null, undersized, and unsealable memory are rejected");
    check(seal_calls == 0 && shim.resolve("kernel32.dll", "VirtualAlloc", 0) == nullptr,
          "invalid requests did not seal memory or publish APIs");

    check(shim.install_builtin_integer_wrapper("KERNEL32.DLL", "Sleep", sleep_page,
          static_cast<size_t>(page_bytes), reject_seal, nullptr) ==
          BuiltinInstallStatus::SealFailed &&
          shim.resolve("kernel32.dll", "Sleep", 0) == nullptr,
          "failed executable-memory transition publishes no import");
    sealing_symbol = "Sleep";
    check(shim.install_builtin_integer_wrapper("KERNEL32.DLL", "Sleep", sleep_page,
          static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Ok && stamp_seen && !visible_during_seal &&
          shim.resolve("kernel32.dll", "Sleep", 0) == sleep_page,
          "Sleep remains hidden during sealing, then publishes its trampoline");
    sealing_symbol = "GetTickCount";
    check(shim.install_builtin_integer_wrapper("kernel32.dll", "GetTickCount", tick_page,
          static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Ok && !visible_during_seal &&
          shim.resolve("KERNEL32.DLL", "GetTickCount", 0) == tick_page,
          "second typed time API publishes independently");

    auto* sleep_entry = shim.resolve("kernel32.dll", "Sleep", 0);
    auto* tick_entry = shim.resolve("kernel32.dll", "GetTickCount", 0);
    const auto start = std::chrono::steady_clock::now();
    const uint32_t before = static_cast<uint32_t>(cap13_win64_call0(tick_entry));
    (void)cap13_win64_call1(sleep_entry, 5);
    const uint32_t after = static_cast<uint32_t>(cap13_win64_call0(tick_entry));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    check(elapsed >= 3 && elapsed < 10000 && uint32_t(after - before) < 10000,
          "published Win64 Sleep and GetTickCount cross into SysV handlers");
    const int before_duplicate = seal_calls;
    check(shim.install_builtin_integer_wrapper("kernel32.dll", "Sleep", spare_page,
          static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::AlreadyPublished && seal_calls == before_duplicate &&
          shim.resolve("kernel32.dll", "Sleep", 0) == sleep_page,
          "duplicate installation cannot replace a live IAT target");

    sealing_symbol = "GetCurrentProcessId";
    check(shim.install_builtin_integer_wrapper("kernel32.dll", sealing_symbol,
          process_page, static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Ok && !visible_during_seal,
          "process identity wrapper seals before publication");
    sealing_symbol = "GetCurrentThreadId";
    check(shim.install_builtin_integer_wrapper("kernel32.dll", sealing_symbol,
          thread_page, static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Ok && !visible_during_seal,
          "thread identity wrapper seals before publication");
    sealing_symbol = "GetSystemTimeAsFileTime";
    check(shim.install_builtin_integer_wrapper("kernel32.dll", sealing_symbol,
          filetime_page, static_cast<size_t>(page_bytes), seal_rx, nullptr) ==
          BuiltinInstallStatus::Ok && !visible_during_seal,
          "UTC file-time wrapper seals before publication");

    const bool identity_ready =
        shim.resolve("kernel32.dll", "GetCurrentProcessId", 0) == process_page &&
        shim.resolve("kernel32.dll", "GetCurrentThreadId", 0) == thread_page &&
        shim.resolve("kernel32.dll", "GetSystemTimeAsFileTime", 0) == filetime_page;
    if (identity_ready)
    {
    check(static_cast<uint32_t>(cap13_win64_call0(process_page)) ==
          static_cast<uint32_t>(getpid()),
          "Win64 GetCurrentProcessId returns the host process identity");
    const uint32_t main_tid = static_cast<uint32_t>(cap13_win64_call0(thread_page));
    uint32_t worker_tid = 0;
    std::thread worker([&] {
        worker_tid = static_cast<uint32_t>(cap13_win64_call0(thread_page));
        if (worker_tid != static_cast<uint32_t>(cap13_win64_call0(thread_page)))
            worker_tid = 0;
    });
    worker.join();
    check(main_tid != 0 && worker_tid != 0 && main_tid != worker_tid &&
          main_tid == static_cast<uint32_t>(cap13_win64_call0(thread_page)),
          "Win64 thread IDs stay stable and distinguish live threads");

    struct FileTime { uint32_t low; uint32_t high; };
    FileTime ft{0, 0};
    const auto unix_before = std::chrono::system_clock::now();
    (void)cap13_win64_call1(filetime_page, reinterpret_cast<uint64_t>(&ft));
    const auto unix_after = std::chrono::system_clock::now();
    const uint64_t windows_ticks = (uint64_t(ft.high) << 32) | ft.low;
    const uint64_t epoch_delta = 116444736000000000ull;
    const auto before_ticks = epoch_delta + uint64_t(std::chrono::duration_cast<
        std::chrono::nanoseconds>(unix_before.time_since_epoch()).count() / 100);
    const auto after_ticks = epoch_delta + uint64_t(std::chrono::duration_cast<
        std::chrono::nanoseconds>(unix_after.time_since_epoch()).count() / 100);
    check(windows_ticks >= before_ticks && windows_ticks <= after_ticks,
          "Win64 file-time output has the documented UTC epoch and layout");
    int stopped = 0;
    cap13::WinApiShim::Hooks hooks{};
    hooks.stop_mod = +[](void* user, const char*) { ++*static_cast<int*>(user); };
    hooks.stop_user = &stopped;
    shim.set_hooks(hooks);
    (void)cap13_win64_call1(filetime_page, 0);
    check(stopped == 1, "null FILETIME output stops the mod through its hook");
    }
    else check(false, "all three identity/time wrappers are callable");

    munmap(spare_page, static_cast<size_t>(page_bytes));
    // Published trampolines stay mapped until process exit, as required by
    // the retained-module lifetime rule.
    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
