#include "guest_read_map.h"
#include "guest_write_map.h"
#include "win_api_shim.h"
#include "win64_bridge.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

namespace {
int checks = 0;
int failures = 0;
void check(bool ok, const char* name)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
bool seal(void* memory, size_t length, void*)
{
    return mprotect(memory, length, PROT_READ | PROT_EXEC) == 0;
}
void* slot(size_t page)
{
    void* p = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}
void stopped(void* user, const char*)
{
    static_cast<std::atomic<int>*>(user)->fetch_add(1);
}
struct BackendState
{
    size_t page = 0;
    void* slots[5]{};
    int allocations = 0;
};
bool backend_allocate(size_t minimum, void** memory, size_t* bytes, void* user)
{
    auto& state = *static_cast<BackendState*>(user);
    if (minimum > state.page || state.allocations >= 5) return false;
    *memory = slot(state.page);
    if (!*memory) return false;
    *bytes = state.page;
    state.slots[state.allocations++] = *memory;
    return true;
}
void backend_release(void* memory, size_t bytes, void*)
{
    munmap(memory, bytes);
}
}

int main()
{
    const long host_page = sysconf(_SC_PAGESIZE);
    if (host_page <= 0) return 2;
    const size_t page = static_cast<size_t>(host_page);
    cap13::WinApiShim shim;
    std::atomic<int> stops{0};
    cap13::WinApiShim::Hooks hooks{};
    hooks.stop_mod = stopped;
    hooks.stop_user = &stops;
    shim.set_hooks(hooks);
    shim.register_builtin_exports();
    void* init_slot = slot(page);
    void* init_ex_slot = slot(page);
    void* enter_slot = slot(page);
    void* leave_slot = slot(page);
    void* delete_slot = slot(page);
    if (!init_slot || !init_ex_slot || !enter_slot || !leave_slot || !delete_slot)
        return 2;
    check(shim.install_builtin_integer_wrapper(
              "kernel32.dll", "InitializeCriticalSectionEx", init_ex_slot,
              page, seal, nullptr) == cap13::BuiltinInstallStatus::Unsupported,
          "critical-section import refuses before guest maps bind");
    cap13::GuestReadMap reads;
    cap13::GuestWriteMap writes;
    check(shim.set_guest_read_map(&reads) &&
          shim.set_guest_write_map(&writes),
          "guarded guest maps bind before critical-section publication");
    const bool installed =
        shim.install_builtin_integer_wrapper(
            "kernel32.dll", "InitializeCriticalSection", init_slot,
            page, seal, nullptr) == cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper(
            "kernel32.dll", "InitializeCriticalSectionEx", init_ex_slot,
            page, seal, nullptr) == cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper(
            "kernel32.dll", "EnterCriticalSection", enter_slot,
            page, seal, nullptr) == cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper(
            "kernel32.dll", "LeaveCriticalSection", leave_slot,
            page, seal, nullptr) == cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper(
            "kernel32.dll", "DeleteCriticalSection", delete_slot,
            page, seal, nullptr) == cap13::BuiltinInstallStatus::Ok;
    check(installed, "five critical-section imports publish sealed Win64 wrappers");
    if (!installed) return 1;
    alignas(8) uint8_t object[40];
    std::memset(object, 0xA5, sizeof(object));
    const uint64_t address = reinterpret_cast<uint64_t>(object);
    check(reads.add(object, object, sizeof(object)) &&
          writes.add(object, object, sizeof(object)),
          "guest critical-section object has bounded read/write ownership");
    check(cap13_win64_call3(init_ex_slot, address, 4000, 0) == 1 &&
          stops.load() == 0 && object[0] != 0xA5,
          "InitializeCriticalSectionEx prepares an owned x64 object");
    cap13_win64_call1(enter_slot, address);
    cap13_win64_call1(enter_slot, address);
    check(stops.load() == 0,
          "the owner can enter the same critical section recursively");
    std::atomic<bool> started{false};
    std::atomic<bool> acquired{false};
    std::thread contender([&] {
        started.store(true);
        cap13_win64_call1(enter_slot, address);
        acquired.store(true);
        cap13_win64_call1(leave_slot, address);
    });
    while (!started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    check(!acquired.load(),
          "another thread blocks while the recursive owner holds the section");
    cap13_win64_call1(leave_slot, address);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    check(!acquired.load(),
          "one leave keeps the section owned at recursion depth one");
    cap13_win64_call1(leave_slot, address);
    contender.join();
    check(acquired.load() && stops.load() == 0,
          "final leave releases the section to a waiting thread");
    const int before_bad = stops.load();
    cap13_win64_call1(leave_slot, address);
    check(stops.load() == before_bad + 1,
          "unowned leave reports a fail-closed lifecycle error");
    cap13_win64_call1(enter_slot, address);
    const int before_delete = stops.load();
    cap13_win64_call1(delete_slot, address);
    check(stops.load() == before_delete + 1,
          "deleting an owned section refuses without losing ownership");
    cap13_win64_call1(leave_slot, address);
    cap13_win64_call1(delete_slot, address);
    check(stops.load() == before_delete + 1,
          "unowned deletion succeeds without an error");
    check(cap13_win64_call3(init_ex_slot, address, 0, 1) == 0 &&
          cap13_win64_call3(init_ex_slot, address, 0, 0x01000000) == 1,
          "unknown flags refuse and documented no-debug flag initializes");
    cap13_win64_call1(delete_slot, address);
    cap13_win64_call1(init_slot, address);
    cap13_win64_call1(enter_slot, address);
    cap13_win64_call1(leave_slot, address);
    cap13_win64_call1(delete_slot, address);
    check(stops.load() == before_delete + 1,
          "void initializer supports the same guarded enter/leave/delete lifecycle");
    check(reads.remove(object) && writes.remove(object),
          "guest object registrations revoke after deletion");
    const int before_stale = stops.load();
    cap13_win64_call1(enter_slot, address);
    check(stops.load() == before_stale + 1,
          "entry after deletion and map revocation fails closed");

    cap13::WinApiShim lazy;
    lazy.register_builtin_exports();
    BackendState backend_state{};
    backend_state.page = page;
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = backend_allocate;
    backend.seal_executable = seal;
    backend.release = backend_release;
    backend.user = &backend_state;
    check(lazy.set_executable_backend(backend) &&
          lazy.resolve("kernel32.dll", "InitializeCriticalSectionEx", 0) ==
              nullptr && backend_state.allocations == 0,
          "lazy PE import refuses before maps without allocating a thunk");
    cap13::GuestReadMap lazy_reads;
    cap13::GuestWriteMap lazy_writes;
    const bool bound = lazy.set_guest_read_map(&lazy_reads) &&
                       lazy.set_guest_write_map(&lazy_writes);
    alignas(8) uint8_t lazy_object[40]{};
    const uint64_t lazy_address = reinterpret_cast<uint64_t>(lazy_object);
    const bool mapped = bound &&
        lazy_reads.add(lazy_object, lazy_object, sizeof(lazy_object)) &&
        lazy_writes.add(lazy_object, lazy_object, sizeof(lazy_object));
    void* lazy_init = mapped ?
        lazy.resolve("kernel32.dll", "InitializeCriticalSectionEx", 0) :
        nullptr;
    void* lazy_enter = mapped ?
        lazy.resolve("kernel32.dll", "EnterCriticalSection", 0) : nullptr;
    void* lazy_leave = mapped ?
        lazy.resolve("kernel32.dll", "LeaveCriticalSection", 0) : nullptr;
    void* lazy_delete = mapped ?
        lazy.resolve("kernel32.dll", "DeleteCriticalSection", 0) : nullptr;
    const bool initialized = lazy_init &&
        cap13_win64_call3(lazy_init, lazy_address, 0, 0) == 1;
    if (initialized && lazy_enter && lazy_leave && lazy_delete)
    {
        cap13_win64_call1(lazy_enter, lazy_address);
        cap13_win64_call1(lazy_leave, lazy_address);
        cap13_win64_call1(lazy_delete, lazy_address);
    }
    check(initialized && lazy_enter && lazy_leave && lazy_delete &&
          lazy.resolve("kernel32.dll", "EnterCriticalSection", 0) ==
              lazy_enter && backend_state.allocations == 4,
          "PE-style lazy resolution publishes and reuses critical-section imports");
    if (mapped)
    {
        lazy_reads.remove(lazy_object);
        lazy_writes.remove(lazy_object);
    }
    for (void* p : backend_state.slots)
        if (p) munmap(p, page);
    for (void* p : {init_slot, init_ex_slot, enter_slot, leave_slot, delete_slot})
        munmap(p, page);
    std::printf("TOTAL: checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
