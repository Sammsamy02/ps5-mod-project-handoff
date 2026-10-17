#include "guest_read_map.h"
#include "guest_write_map.h"
#include "win_api_shim.h"
#include "win64_bridge.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
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
    void* p = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}
struct BackendState
{
    size_t page = 0;
    void* slots[2]{};
    int allocations = 0;
};
bool backend_allocate(size_t minimum, void** memory, size_t* bytes, void* user)
{
    auto& state = *static_cast<BackendState*>(user);
    if (minimum > state.page || state.allocations >= 2) return false;
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
    constexpr uint64_t MEM_COMMIT = 0x1000;
    constexpr uint64_t MEM_RESERVE = 0x2000;
    constexpr uint64_t MEM_RELEASE = 0x8000;
    constexpr uint64_t PAGE_READONLY = 0x02;
    constexpr uint64_t PAGE_READWRITE = 0x04;
    constexpr uint64_t PAGE_EXECUTE_READWRITE = 0x40;
    const long host_page = sysconf(_SC_PAGESIZE);
    if (host_page <= 0) return 2;
    const size_t page = static_cast<size_t>(host_page);
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    void* alloc_slot = slot(page);
    void* free_slot = slot(page);
    void* protect_slot = slot(page);
    if (!alloc_slot || !free_slot || !protect_slot) return 2;
    check(shim.install_builtin_integer_wrapper("kernel32.dll", "VirtualAlloc",
                                               alloc_slot, page, seal, nullptr) ==
              cap13::BuiltinInstallStatus::Unsupported,
          "virtual allocation refuses import publication before maps bind");
    cap13::GuestReadMap reads;
    cap13::GuestWriteMap writes;
    check(shim.set_guest_read_map(&reads) && shim.set_guest_write_map(&writes),
          "guest read and write maps bind");
    const bool installed =
        shim.install_builtin_integer_wrapper("kernel32.dll", "VirtualAlloc",
                                             alloc_slot, page, seal, nullptr) ==
            cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper("kernel32.dll", "VirtualFree",
                                             free_slot, page, seal, nullptr) ==
            cap13::BuiltinInstallStatus::Ok &&
        shim.install_builtin_integer_wrapper("kernel32.dll", "VirtualProtect",
                                             protect_slot, page, seal, nullptr) ==
            cap13::BuiltinInstallStatus::Ok;
    check(installed, "virtual-memory trio publishes sealed Win64 wrappers");
    if (!installed) return 1;

    const uint64_t block = cap13_win64_call4(
        alloc_slot, 0, 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t zeros[32]{};
    uint8_t output[32]{};
    check(block && !(block % page) &&
          reads.read(reinterpret_cast<void*>(block), output, 32) &&
          !std::memcmp(output, zeros, 32),
          "combined reserve and commit yields aligned zeroed readable memory");
    const uint8_t mark = 0x5a;
    check(writes.write(reinterpret_cast<void*>(block + page - 1), &mark, 1) &&
          !writes.write(reinterpret_cast<void*>(block + page), &mark, 1) &&
          reads.read(reinterpret_cast<void*>(block + page - 1), output, 1) &&
          output[0] == mark,
          "committed read/write page has guarded bounds");
    check(cap13_win64_call4(alloc_slot, 0, 0, MEM_RESERVE | MEM_COMMIT,
                            PAGE_READWRITE) == 0 &&
          cap13_win64_call4(alloc_slot, 0, std::numeric_limits<size_t>::max(),
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == 0 &&
          cap13_win64_call4(alloc_slot, 0, page, MEM_RESERVE | MEM_COMMIT,
                            PAGE_EXECUTE_READWRITE) == 0,
          "zero, oversized, and executable allocation refuse");
    check(cap13_win64_call4(alloc_slot, block, page,
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == 0 &&
          cap13_win64_call4(alloc_slot, 0, page, MEM_RESERVE | 1,
                            PAGE_READWRITE) == 0,
          "fixed address and unsupported allocation modes refuse");
    check(cap13_win64_call3(free_slot, block, page, MEM_RELEASE) == 0 &&
          cap13_win64_call3(free_slot, block + 1, 0, MEM_RELEASE) == 0 &&
          reads.read(reinterpret_cast<void*>(block), output, 1),
          "invalid release leaves owned mapping registered");
    check(cap13_win64_call3(free_slot, block, 0, MEM_RELEASE) == 1 &&
          !reads.read(reinterpret_cast<void*>(block), output, 1) &&
          !writes.write(reinterpret_cast<void*>(block), &mark, 1) &&
          cap13_win64_call3(free_slot, block, 0, MEM_RELEASE) == 0,
          "release revokes both guest maps and refuses double release");

    const uint64_t reserved = cap13_win64_call4(
        alloc_slot, 0, 2 * page, MEM_RESERVE, PAGE_READWRITE);
    check(reserved && !reads.read(reinterpret_cast<void*>(reserved), output, 1) &&
          !writes.write(reinterpret_cast<void*>(reserved), &mark, 1),
          "reserved uncommitted pages have no guest access");
    const uint64_t committed = cap13_win64_call4(
        alloc_slot, reserved, 2 * page, MEM_COMMIT, PAGE_READONLY);
    check(committed == reserved &&
          reads.read(reinterpret_cast<void*>(reserved), output, 32) &&
          !std::memcmp(output, zeros, 32) &&
          !writes.write(reinterpret_cast<void*>(reserved), &mark, 1),
          "whole reserved region commits as guarded read-only pages");
    check(cap13_win64_call4(alloc_slot, reserved, page,
                            MEM_COMMIT, PAGE_READWRITE) == 0 &&
          cap13_win64_call3(free_slot, reserved, 0, MEM_RELEASE) == 1,
          "partial recommit refuses and whole region releases");
    const uint64_t direct = cap13_win64_call4(
        alloc_slot, 0, page, MEM_COMMIT, PAGE_READWRITE);
    check(direct && cap13_win64_call3(free_slot, direct, 0, MEM_RELEASE) == 1,
          "null-address direct commit allocates and releases a region");

    const uint64_t protected_block = cap13_win64_call4(
        alloc_slot, 0, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint32_t old_protection = 0;
    check(protected_block &&
          writes.add(&old_protection, &old_protection,
                     sizeof(old_protection)),
          "old-protection output is a separately owned writable guest range");
    check(cap13_win64_call4(protect_slot, protected_block, 2 * page,
                            PAGE_READONLY,
                            reinterpret_cast<uint64_t>(&old_protection)) == 1 &&
          old_protection == PAGE_READWRITE &&
          reads.read(reinterpret_cast<void*>(protected_block), output, 1) &&
          !writes.write(reinterpret_cast<void*>(protected_block), &mark, 1),
          "read/write to read-only transition returns old protection and revokes writes");
    check(cap13_win64_call4(protect_slot, protected_block, 2 * page,
                            PAGE_READWRITE,
                            reinterpret_cast<uint64_t>(&old_protection)) == 1 &&
          old_protection == PAGE_READONLY &&
          writes.write(reinterpret_cast<void*>(protected_block), &mark, 1),
          "read-only to read/write transition restores guarded writes");
    check(cap13_win64_call4(protect_slot, protected_block, page,
                            PAGE_READONLY,
                            reinterpret_cast<uint64_t>(&old_protection)) == 0 &&
          cap13_win64_call4(protect_slot, protected_block, 2 * page,
                            PAGE_EXECUTE_READWRITE,
                            reinterpret_cast<uint64_t>(&old_protection)) == 0 &&
          writes.write(reinterpret_cast<void*>(protected_block), &mark, 1),
          "partial and executable protection refuse without changing access");
    check(cap13_win64_call4(protect_slot, protected_block, 2 * page,
                            PAGE_READONLY, 0) == 0 &&
          cap13_win64_call4(protect_slot,
                            reinterpret_cast<uint64_t>(&old_protection),
                            2 * page, PAGE_READONLY,
                            reinterpret_cast<uint64_t>(&old_protection)) == 0 &&
          writes.write(reinterpret_cast<void*>(protected_block), &mark, 1),
          "invalid output and unowned address refuse without changing access");
    check(writes.remove(&old_protection) &&
          cap13_win64_call3(free_slot, protected_block, 0, MEM_RELEASE) == 1,
          "protected region releases after output registration is revoked");

    cap13::WinApiShim lazy;
    lazy.register_builtin_exports();
    BackendState state{};
    state.page = page;
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = backend_allocate;
    backend.seal_executable = seal;
    backend.release = backend_release;
    backend.user = &state;
    check(lazy.set_executable_backend(backend) &&
          lazy.resolve("kernel32.dll", "VirtualAlloc", 0) == nullptr &&
          state.allocations == 0,
          "lazy PE import refuses before maps without allocating a thunk");
    cap13::GuestReadMap lazy_reads;
    cap13::GuestWriteMap lazy_writes;
    bool lazy_bound = lazy.set_guest_read_map(&lazy_reads) &&
                      lazy.set_guest_write_map(&lazy_writes);
    void* lazy_alloc = lazy_bound ?
        lazy.resolve("kernel32.dll", "VirtualAlloc", 0) : nullptr;
    void* lazy_free = lazy_bound ?
        lazy.resolve("kernel32.dll", "VirtualFree", 0) : nullptr;
    const uint64_t lazy_block = lazy_alloc ?
        cap13_win64_call4(lazy_alloc, 0, page, MEM_RESERVE | MEM_COMMIT,
                          PAGE_READWRITE) : 0;
    check(lazy_alloc && lazy_free && lazy_block &&
          lazy.resolve("kernel32.dll", "VirtualAlloc", 0) == lazy_alloc &&
          cap13_win64_call3(lazy_free, lazy_block, 0, MEM_RELEASE) == 1 &&
          state.allocations == 2,
          "PE-style resolution reuses sealed imports and owned virtual memory");
    for (void* p : state.slots)
        if (p) munmap(p, page);
    munmap(alloc_slot, page);
    munmap(free_slot, page);
    munmap(protect_slot, page);
    std::printf("TOTAL: checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
