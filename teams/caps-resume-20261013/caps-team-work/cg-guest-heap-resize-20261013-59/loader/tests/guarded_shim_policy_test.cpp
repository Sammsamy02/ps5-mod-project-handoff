#include "guest_pe_loader.h"
#include "pe_types.h"
#include "win_api_shim.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}
std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
bool allocate_writable(size_t minimum, void** address, size_t* bytes, void*)
{
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* result = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (result == MAP_FAILED) return false;
    *address = result;
    *bytes = page;
    return true;
}
bool seal_executable(void* address, size_t bytes, void*)
{
    return mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}
void release_mapping(void* address, size_t bytes, void*)
{
    munmap(address, bytes);
}
extern "C" __attribute__((ms_abi)) uint64_t ue4ss_initialized()
{
    return 1;
}
bool lookup_ue4ss(cap13::WinModule, const char* symbol, void** out)
{
    if (std::strcmp(symbol, "is_ue4ss_initialized") != 0) return false;
    *out = reinterpret_cast<void*>(&ue4ss_initialized);
    return true;
}
class RecordingSwitch final : public cap13::GuestSegmentSwitch
{
  public:
    bool allow = true;
    int enters = 0;
    int leaves = 0;
    bool enter(void* teb) override
    {
        ++enters;
        return allow && teb != nullptr;
    }
    void leave() noexcept override { ++leaves; }
};
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    auto bytes = read_file(argv[1]);
    check(!bytes.empty(), "pinned synthetic PE fixture loaded read-only");
    if (bytes.empty()) return 2;

    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = allocate_writable;
    backend.seal_executable = seal_executable;
    backend.release = release_mapping;
    check(shim.set_executable_backend(backend), "reviewed import backend configured");
    shim.register_module("UE4SS.dll", reinterpret_cast<void*>(0x100), lookup_ue4ss);

    cap13::LoadOptions options;
    options.page_size = 0x1000;
    options.allow_rwx_partition = true;
    options.allow_no_relocs = true;
    cap13::StaticTlsBank bank;
    RecordingSwitch segment;
    auto loaded = cap13::pe_prepare_guest_with_shim(bytes.data(), bytes.size(), shim,
                                                     options, bank, segment);
    check(loaded.ok && loaded.module,
          "guarded loader resolves reviewed shim imports and registers module policy");
    if (!loaded.ok)
    {
        std::printf("reason=%s\n", loaded.reason.c_str());
        return 1;
    }
    cap13::GuestTebView teb(bank);
    segment.allow = false;
    check(loaded.module->process_attach(teb) == cap13::GuestNotifyStatus::BackendRefused &&
          segment.enters == 1 && segment.leaves == 0,
          "backend refusal prevents DllMain and clears provisional policy");
    segment.allow = true;
    check(loaded.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          segment.enters == 2 && segment.leaves == 1,
          "retry registers policy before real fixture DllMain");
    check(loaded.module->thread_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          loaded.module->thread_detach(teb) == cap13::GuestNotifyStatus::Ok &&
          segment.enters == 2 && segment.leaves == 1,
          "DllMain DisableThreadLibraryCalls suppresses thread callbacks");
    check(loaded.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok &&
          segment.enters == 3 && segment.leaves == 2,
          "process detach runs through gate and releases policy");
    check(loaded.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          segment.enters == 4 && segment.leaves == 3,
          "module can register policy again after clean detach");
    check(loaded.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
          "second clean detach succeeds");

    // Build one real static-TLS directory in the in-memory fixture's unused
    // .idata tail. Its three VA fields have matching DIR64 relocations.
    auto with_tls = bytes;
    auto* dos = reinterpret_cast<cap13::PeDosHeader*>(with_tls.data());
    auto* coff = reinterpret_cast<cap13::PeFileHeader*>(with_tls.data() + dos->e_lfanew + 4);
    auto* opt = reinterpret_cast<cap13::PeOptionalHeader64*>(coff + 1);
    const uint64_t base = opt->ImageBase;
    cap13::PeTlsDirectory64 tls{};
    tls.StartAddressOfRawData = base + 0x3140;
    tls.EndAddressOfRawData = base + 0x3141;
    tls.AddressOfIndex = base + 0x3150;
    std::memcpy(with_tls.data() + 0x900, &tls, sizeof(tls));
    with_tls[0x940] = 0x5a;
    opt->DataDirectory[cap13::PE_DIR_TLS] = {0x3100, sizeof(tls)};
    cap13::PeRelocBlock reloc{0x3000, 14};
    const uint16_t targets[] = {0xa100, 0xa108, 0xa110};
    std::memcpy(with_tls.data() + 0x980, &reloc, sizeof(reloc));
    std::memcpy(with_tls.data() + 0x988, targets, sizeof(targets));
    opt->DataDirectory[cap13::PE_DIR_BASERELOC] = {0x3180, 14};
    cap13::LoadOptions tls_options = options;
    tls_options.allow_no_relocs = false;
    cap13::StaticTlsBank tls_bank;
    RecordingSwitch tls_segment;
    auto tls_loaded = cap13::pe_prepare_guest_with_shim(
        with_tls.data(), with_tls.size(), shim, tls_options, tls_bank, tls_segment);
    check(tls_loaded.ok && tls_loaded.module,
          "in-memory fixture with relocated static TLS maps through reviewed shim");
    if (!tls_loaded.ok)
        std::printf("TLS fixture reason=%s\n", tls_loaded.reason.c_str());
    if (tls_loaded.ok)
    {
        cap13::GuestTebView tls_teb(tls_bank);
        const auto& slots = tls_teb.refresh().slots;
        check(slots.size() == 1 && slots[0] &&
              *static_cast<uint8_t*>(slots[0]) == 0x5a,
              "relocated TLS template is published before DllMain");
        check(tls_loaded.module->process_attach(tls_teb) == cap13::GuestNotifyStatus::Ok &&
              tls_segment.enters == 1 && tls_segment.leaves == 1,
              "static-TLS DllMain runs with registered module policy");
        check(tls_loaded.module->thread_attach(tls_teb) == cap13::GuestNotifyStatus::Ok &&
              tls_loaded.module->thread_detach(tls_teb) == cap13::GuestNotifyStatus::Ok &&
              tls_segment.enters == 3 && tls_segment.leaves == 3,
              "static TLS keeps thread notifications after DisableThreadLibraryCalls");
        check(tls_loaded.module->process_detach(tls_teb) == cap13::GuestNotifyStatus::Ok,
              "static-TLS policy is removed after process detach");
    }

    auto truefov = read_file(argv[2]);
    cap13::StaticTlsBank truefov_bank;
    auto truefov_load = cap13::pe_prepare_guest_with_shim(
        truefov.data(), truefov.size(), shim, options, truefov_bank, segment);
    check(!truefov_load.ok && !truefov_load.module &&
          !truefov_load.missing.empty() &&
          truefov_load.reason.find("unresolved imports") != std::string::npos,
          "real gameplay DLL refuses when reviewed Windows imports are absent");

    auto decorated = read_file(argv[3]);
    cap13::StaticTlsBank decorated_bank;
    auto decorated_load = cap13::pe_prepare_guest_with_shim(
        decorated.data(), decorated.size(), shim, options, decorated_bank, segment);
    bool full_long_name = false;
    for (const auto& missing : decorated_load.missing)
        if (missing.size() > 255 &&
            missing.find("?get_default_devices_ref@DefaultTargets") != std::string::npos &&
            missing.find("@@std@@XZ") != std::string::npos)
            full_long_name = true;
    check(!decorated_load.ok && !decorated_load.module && full_long_name,
          "unresolved decorated import retains its full name in refusal evidence");
    std::printf("cases=%d failed=%d\n", checks, failures);
    return failures ? 1 : 0;
}
