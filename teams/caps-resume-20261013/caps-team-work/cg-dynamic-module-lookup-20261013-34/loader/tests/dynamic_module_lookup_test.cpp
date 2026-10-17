#include "win_api_shim.h"
#include "win64_bridge.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
int failures = 0;
int checks = 0;
void check(bool ok, const char* label)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}
bool allocate_writable(size_t minimum, void** address, size_t* bytes, void*)
{
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* region = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) return false;
    *address = region;
    *bytes = page;
    return true;
}
bool seal_executable(void* address, size_t bytes, void*)
{
    return mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}
void release_mapping(void* address, size_t bytes, void*) { munmap(address, bytes); }

struct ReadState
{
    std::vector<std::pair<const char*, size_t>> ranges;
    int reads = 0;
};
bool read_guest_bytes(void* user, const void* guest, void* dst, size_t bytes)
{
    auto& state = *static_cast<ReadState*>(user);
    ++state.reads;
    const uintptr_t address = reinterpret_cast<uintptr_t>(guest);
    for (const auto& range : state.ranges)
    {
        const uintptr_t begin = reinterpret_cast<uintptr_t>(range.first);
        if (address >= begin && address - begin <= range.second &&
            bytes <= range.second - (address - begin))
        {
            std::memcpy(dst, guest, bytes);
            return true;
        }
    }
    return false;
}
extern "C" __attribute__((ms_abi)) uint64_t fake_export() { return 7; }
bool named_lookup(cap13::WinModule, const char* symbol, void** out)
{
    if (symbol && std::strcmp(symbol, "is_ue4ss_initialized") == 0)
    {
        *out = reinterpret_cast<void*>(&fake_export);
        return true;
    }
    return false;
}
bool mapped_lookup(void*, const char* symbol, uint64_t ordinal,
                   void** out, std::string*)
{
    if ((symbol && std::strcmp(symbol, "start_mod") == 0) ||
        (!symbol && ordinal == 7))
    {
        *out = reinterpret_cast<void*>(&fake_export);
        return true;
    }
    return false;
}
} // namespace

int main()
{
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = allocate_writable;
    backend.seal_executable = seal_executable;
    backend.release = release_mapping;
    check(shim.set_executable_backend(backend), "executable wrapper backend configured");
    check(shim.resolve("kernel32.dll", "GetModuleHandleA", 0) == nullptr &&
          shim.resolve("kernel32.dll", "GetProcAddress", 0) == nullptr,
          "dynamic lookup imports stay hidden without validated guest reads");

    char ue4ss_name[] = "UE4SS.DLL";
    char provider_name[] = "provider.dll";
    char provider_stem[] = "provider";
    char kernel_name[] = "KERNEL32.dll";
    char export_name[] = "is_ue4ss_initialized";
    char provider_export[] = "start_mod";
    char wrong_case[] = "Start_mod";
    char builtin_export[] = "GetCurrentProcessId";
    char missing_module[] = "absent.dll";
    char missing_export[] = "absent_export";
    char unterminated[] = {'U', 'E', '4', 'S', 'S'};
    ReadState reads;
    for (auto range : {std::pair<const char*, size_t>{ue4ss_name, sizeof(ue4ss_name)},
                       {provider_name, sizeof(provider_name)},
                       {provider_stem, sizeof(provider_stem)},
                       {kernel_name, sizeof(kernel_name)},
                       {export_name, sizeof(export_name)},
                       {provider_export, sizeof(provider_export)},
                       {wrong_case, sizeof(wrong_case)},
                       {builtin_export, sizeof(builtin_export)},
                       {missing_module, sizeof(missing_module)},
                       {missing_export, sizeof(missing_export)},
                       {unterminated, sizeof(unterminated)}})
        reads.ranges.push_back(range);
    cap13::WinApiShim::Hooks hooks{};
    hooks.read_guest_bytes = read_guest_bytes;
    hooks.read_guest_user = &reads;
    shim.set_hooks(hooks);
    shim.register_module("UE4SS.dll", reinterpret_cast<void*>(0x100), named_lookup);
    int owner = 0;
    check(shim.register_mapped_module("/mods/provider.dll",
                                      reinterpret_cast<void*>(0x500),
                                      mapped_lookup, &owner),
          "attached-module test registry accepts provider");
    check(!shim.set_main_module(reinterpret_cast<void*>(0x700)),
          "unregistered main-module handle is rejected");
    shim.register_module("game.exe", reinterpret_cast<void*>(0x700), named_lookup);
    check(shim.set_main_module(reinterpret_cast<void*>(0x700)),
          "registered process module is selected for null-name lookup");

    void* get_module = shim.resolve("kernel32.dll", "GetModuleHandleA", 0);
    void* get_proc = shim.resolve("kernel32.dll", "GetProcAddress", 0);
    check(get_module && get_proc, "validated lookup rows publish sealed Win64 wrappers");
    if (!get_module || !get_proc) return 1;
    const uint64_t ue4ss = cap13_win64_call1(get_module,
                                              reinterpret_cast<uint64_t>(ue4ss_name));
    const uint64_t provider = cap13_win64_call1(get_module,
                                                 reinterpret_cast<uint64_t>(provider_name));
    const uint64_t kernel = cap13_win64_call1(get_module,
                                               reinterpret_cast<uint64_t>(kernel_name));
    check(ue4ss == 0x100 && provider == 0x500 && kernel != 0 &&
          kernel != ue4ss && kernel != provider,
          "named attached and built-in modules return stable distinct handles");
    check(cap13_win64_call1(get_module,
              reinterpret_cast<uint64_t>(provider_stem)) == provider,
          "extensionless module name appends the default DLL suffix");
    check(cap13_win64_call1(get_module, 0) == 0x700,
          "null module name returns configured main executable handle");
    check(cap13_win64_call2(get_proc, ue4ss,
                            reinterpret_cast<uint64_t>(export_name)) ==
              reinterpret_cast<uint64_t>(&fake_export) &&
          cap13_win64_call2(get_proc, provider,
                            reinterpret_cast<uint64_t>(provider_export)) ==
              reinterpret_cast<uint64_t>(&fake_export),
          "GetProcAddress resolves exact named exports by module handle");
    const int before_ordinal = reads.reads;
    check(cap13_win64_call2(get_proc, provider, 7) ==
              reinterpret_cast<uint64_t>(&fake_export) &&
          reads.reads == before_ordinal,
          "low-word ordinal resolves without dereferencing it as a pointer");
    check(cap13_win64_call2(get_proc, provider,
                            reinterpret_cast<uint64_t>(wrong_case)) == 0 &&
          cap13_win64_call2(get_proc, provider, 0x10007) == 0,
          "export names stay case-sensitive and invalid ordinal form refuses");
    const uint64_t builtin = cap13_win64_call2(
        get_proc, kernel, reinterpret_cast<uint64_t>(builtin_export));
    check(builtin && cap13_win64_call0(reinterpret_cast<void*>(builtin)) ==
          static_cast<uint64_t>(getpid()),
          "built-in module handle returns a callable reviewed export");
    void* get_error = shim.resolve("kernel32.dll", "GetLastError", 0);
    check(get_error != nullptr, "dynamic failures can be inspected through GetLastError");
    if (get_error)
    {
        check(cap13_win64_call1(get_module,
                  reinterpret_cast<uint64_t>(missing_module)) == 0 &&
              cap13_win64_call0(get_error) == 126,
              "missing module reports ERROR_MOD_NOT_FOUND");
        check(cap13_win64_call2(get_proc, provider,
                  reinterpret_cast<uint64_t>(missing_export)) == 0 &&
              cap13_win64_call0(get_error) == 127,
              "missing procedure reports ERROR_PROC_NOT_FOUND");
        check(cap13_win64_call2(get_proc, 0xbad, 7) == 0 &&
              cap13_win64_call0(get_error) == 6,
              "unknown HMODULE reports ERROR_INVALID_HANDLE");
        check(cap13_win64_call1(get_module, 0xdead0000) == 0 &&
              cap13_win64_call0(get_error) == 87,
              "invalid guest pointer reports ERROR_INVALID_PARAMETER");
    }
    check(cap13_win64_call1(get_module, 0xdead0000) == 0 &&
          cap13_win64_call1(get_module,
                            reinterpret_cast<uint64_t>(unterminated)) == 0 &&
          cap13_win64_call2(get_proc, provider, 0xdead0000) == 0 &&
          cap13_win64_call2(get_proc, 0xbad, 7) == 0,
          "invalid guest strings and unknown handles refuse without host dereference");
    std::printf("cases=%d failed=%d\n", checks, failures);
    return failures ? 1 : 0;
}
