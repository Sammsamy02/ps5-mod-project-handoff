#include "guest_read_map.h"
#include "guest_write_map.h"
#include "win_api_shim.h"
#include "win64_bridge.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <sys/mman.h>
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
    void* result = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    return result == MAP_FAILED ? nullptr : result;
}
struct BackendState
{
    size_t page = 0;
    int allocations = 0;
    int releases = 0;
    std::vector<void*> live;
};
bool backend_allocate(size_t minimum, void** memory, size_t* bytes, void* user)
{
    auto& state = *static_cast<BackendState*>(user);
    if (minimum > state.page) return false;
    *memory = slot(state.page);
    if (!*memory) return false;
    *bytes = state.page;
    ++state.allocations;
    state.live.push_back(*memory);
    return true;
}
void backend_release(void* memory, size_t bytes, void* user)
{
    auto& state = *static_cast<BackendState*>(user);
    ++state.releases;
    for (void*& current : state.live)
        if (current == memory) { current = nullptr; break; }
    munmap(memory, bytes);
}
}

int main()
{
    const long page_long = sysconf(_SC_PAGESIZE);
    if (page_long <= 0) return 2;
    const size_t page = static_cast<size_t>(page_long);
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    void* get_slot = slot(page);
    void* alloc_slot = slot(page);
    void* free_slot = slot(page);
    if (!get_slot || !alloc_slot || !free_slot) return 2;
    check(shim.install_builtin_integer_wrapper(
              "kernel32.dll", "GetProcessHeap", get_slot, page, seal,
              nullptr) == cap13::BuiltinInstallStatus::Unsupported,
          "process heap import refuses before guest read/write maps exist");
    cap13::GuestReadMap reads;
    cap13::GuestWriteMap writes;
    check(shim.set_guest_read_map(&reads) && shim.set_guest_write_map(&writes),
          "owned guest read/write maps bind before import publication");
    const bool installed =
        shim.install_builtin_integer_wrapper("kernel32.dll", "GetProcessHeap",
                                             get_slot, page, seal, nullptr) ==
            cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper("kernel32.dll", "HeapAlloc",
                                             alloc_slot, page, seal, nullptr) ==
            cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper("kernel32.dll", "HeapFree",
                                             free_slot, page, seal, nullptr) ==
            cap13::BuiltinInstallStatus::Ok;
    check(installed, "three process heap imports publish sealed Win64 entries");
    if (!installed) return 1;
    const uint64_t heap = cap13_win64_call0(get_slot);
    check(heap && heap == cap13_win64_call0(get_slot),
          "GetProcessHeap returns one stable opaque handle");
    const uint64_t block = cap13_win64_call3(alloc_slot, heap, 8, 32);
    uint8_t zero[32]{};
    uint8_t copied[32]{};
    check(block && !(block & 15) &&
          reads.read(reinterpret_cast<void*>(block), copied, sizeof(copied)) &&
          !std::memcmp(copied, zero, sizeof(zero)),
          "HEAP_ZERO_MEMORY gives aligned readable zeroed guest allocation");
    const uint8_t mark = 0x5a;
    check(writes.write(reinterpret_cast<void*>(block + 31), &mark, 1) &&
          !writes.write(reinterpret_cast<void*>(block + 32), &mark, 1) &&
          reads.read(reinterpret_cast<void*>(block + 31), copied, 1) &&
          copied[0] == mark,
          "owned block admits bounded guest reads and writes only");
    check(cap13_win64_call3(alloc_slot, heap + 1, 0, 16) == 0 &&
          cap13_win64_call3(alloc_slot, heap, 1, 16) == 0 &&
          cap13_win64_call3(alloc_slot, heap, 4, 16) == 0,
          "wrong heap and unsupported flags refuse allocation");
    check(cap13_win64_call3(free_slot, heap + 1, 0, block) == 0 &&
          cap13_win64_call3(free_slot, heap, 1, block) == 0 &&
          reads.read(reinterpret_cast<void*>(block), copied, 1),
          "wrong heap or free flags preserve the owned block");
    check(cap13_win64_call3(free_slot, heap, 0, block) == 1 &&
          !reads.read(reinterpret_cast<void*>(block), copied, 1) &&
          !writes.write(reinterpret_cast<void*>(block), &mark, 1) &&
          cap13_win64_call3(free_slot, heap, 0, block) == 0,
          "HeapFree revokes guest ranges before releasing and double-free refuses");
    const uint64_t empty = cap13_win64_call3(alloc_slot, heap, 0, 0);
    check(empty && cap13_win64_call3(free_slot, heap, 0, empty) == 1 &&
          cap13_win64_call3(free_slot, heap, 0, 0) == 1,
          "zero-byte allocation and null free follow bounded process-heap rules");
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
          lazy.resolve("kernel32.dll", "GetProcessHeap", 0) == nullptr &&
          backend_state.allocations == 0,
          "lazy heap resolution refuses before allocating code without maps");
    cap13::GuestReadMap lazy_reads;
    cap13::GuestWriteMap lazy_writes;
    check(lazy.set_guest_read_map(&lazy_reads) &&
          lazy.set_guest_write_map(&lazy_writes),
          "lazy import route binds both guarded maps");
    void* lazy_get = lazy.resolve("kernel32.dll", "GetProcessHeap", 0);
    void* lazy_alloc = lazy.resolve("kernel32.dll", "HeapAlloc", 0);
    void* lazy_free = lazy.resolve("kernel32.dll", "HeapFree", 0);
    const uint64_t lazy_heap = lazy_get ? cap13_win64_call0(lazy_get) : 0;
    const uint64_t lazy_block = lazy_alloc ?
        cap13_win64_call3(lazy_alloc, lazy_heap, 8, 4) : 0;
    check(lazy_get && lazy_alloc && lazy_free && lazy_heap && lazy_block &&
          lazy.resolve("kernel32.dll", "HeapAlloc", 0) == lazy_alloc &&
          cap13_win64_call3(lazy_free, lazy_heap, 0, lazy_block) == 1 &&
          backend_state.allocations == 3 && backend_state.releases == 0,
          "PE-style lazy resolution publishes and reuses the three imports");
    for (void* memory : backend_state.live)
        if (memory) munmap(memory, page);
    munmap(get_slot, page);
    munmap(alloc_slot, page);
    munmap(free_slot, page);
    std::printf("TOTAL: checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
