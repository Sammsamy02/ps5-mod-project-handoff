#include "guest_pe_loader.h"
#include "pe_types.h"
#include "win_api_shim.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
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
void release_mapping(void* address, size_t bytes, void*) { munmap(address, bytes); }
int ue4ss_calls = 0;
extern "C" __attribute__((ms_abi)) uint64_t ue4ss_initialized()
{
    ++ue4ss_calls;
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
    cap13::WinApiShim* shim = nullptr;
    bool first_enter_saw_export = false;
    bool enter(void* teb) override
    {
        ++enters;
        if (enters == 1 && shim)
            first_enter_saw_export = shim->resolve("fixture.dll", "start_mod", 0) != nullptr;
        return allow && teb;
    }
    void leave() noexcept override { ++leaves; }
};
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    auto bytes = read_file(argv[1]);
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
    cap13::GuestTebView teb(bank);

    RecordingSwitch first_segment;
    first_segment.shim = &shim;
    auto first = cap13::pe_prepare_guest_with_shim(
        bytes.data(), bytes.size(), shim, options, bank, first_segment,
        "C:\\mods\\Fixture.DLL");
    check(first.ok && first.module &&
          shim.resolve("fixture.dll", "start_mod", 0) == nullptr,
          "mapped module is prepared but has no public export before attachment");
    if (!first.ok) return 1;

    // Replace the copied fixture's first import with fixture.dll!start_mod.
    // The copied entry calls that IAT slot and returns TRUE; no source bytes
    // are changed and no host function pointer is placed into the IAT.
    auto consumer_bytes = bytes;
    auto* first_import = reinterpret_cast<cap13::PeImportDescriptor*>(
        consumer_bytes.data() + 0x600);
    first_import->Name = 0x3100;
    const uint64_t name_thunk = 0x3120;
    std::memcpy(consumer_bytes.data() + 0x800, &name_thunk, sizeof(name_thunk));
    std::memcpy(consumer_bytes.data() + 0x900, "fixture.dll", sizeof("fixture.dll"));
    std::memcpy(consumer_bytes.data() + 0x922, "start_mod", sizeof("start_mod"));
    const uint8_t call_import_entry[] = {
        0x48, 0x8b, 0x05, 0x19, 0x20, 0x00, 0x00, // mov rax,[rip+0x2019] -> IAT RVA 0x3020
        0xff, 0xd0,                         // call rax
        0xb8, 0x01, 0x00, 0x00, 0x00,       // mov eax,1
        0xc3                                // ret
    };
    std::memcpy(consumer_bytes.data() + 0x400, call_import_entry,
                sizeof(call_import_entry));
    RecordingSwitch consumer_segment;
    auto early_consumer = cap13::pe_prepare_guest_with_shim(
        consumer_bytes.data(), consumer_bytes.size(), shim, options, bank,
        consumer_segment, "/mods/consumer.dll");
    bool missing_provider = false;
    for (const auto& name : early_consumer.missing)
        if (name == "fixture.dll!start_mod") missing_provider = true;
    check(!early_consumer.ok && missing_provider,
          "dependent DLL refuses while provider has not completed attachment");

    check(first.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          !first_segment.first_enter_saw_export,
          "provisional module registration stays hidden during DllMain entry");
    void* first_export = shim.resolve("FIXTURE.DLL", "start_mod", 0);
    check(first_export != nullptr &&
          shim.resolve("fixture.dll", "missing_export", 0) == nullptr &&
          shim.resolve("fixture.dll", nullptr, 1) == nullptr,
          "attached module publishes validated named export only");
    shim.register_module("FIXTURE.DLL", reinterpret_cast<void*>(0x300), lookup_ue4ss);
    check(shim.resolve("fixture.dll", "start_mod", 0) == first_export &&
          !shim.unregister_mapped_module("fixture.dll",
                                         reinterpret_cast<void*>(0x1234),
                                         first.module.get()) &&
          shim.resolve("fixture.dll", "start_mod", 0) == first_export,
          "manual duplicate and wrong-handle removal cannot replace a live mapped export");
    uint64_t returned = UINT64_MAX;
    check(first.module->invoke_integer_export("start_mod", teb, nullptr, 0, &returned) ==
              cap13::GuestCallStatus::Ok && returned == 0,
          "named export executes only through the guarded call path");
    const int before_consumer_calls = ue4ss_calls;
    auto consumer = cap13::pe_prepare_guest_with_shim(
        consumer_bytes.data(), consumer_bytes.size(), shim, options, bank,
        consumer_segment, "/mods/consumer.dll");
    check(consumer.ok && consumer.module,
          "dependent DLL resolves its import from the attached mapped provider");
    if (consumer.ok)
    {
        check(consumer.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
              ue4ss_calls == before_consumer_calls + 1,
              "dependent DllMain calls provider export through its resolved IAT");
        check(consumer.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
              "dependent module detaches before its provider");
    }

    RecordingSwitch second_segment;
    auto second = cap13::pe_prepare_guest_with_shim(
        bytes.data(), bytes.size(), shim, options, bank, second_segment,
        "/other/FIXTURE.dll");
    check(second.ok && second.module &&
          second.module->process_attach(teb) ==
              cap13::GuestNotifyStatus::ModuleRegistrationFailed &&
          shim.resolve("fixture.dll", "start_mod", 0) == first_export,
          "duplicate normalized basename refuses without replacing first module");
    check(first.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok &&
          shim.resolve("fixture.dll", "start_mod", 0) == nullptr,
          "clean detach removes only its mapped export registration");
    check(second.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          shim.resolve("fixture.dll", "start_mod", 0) != nullptr &&
          shim.resolve("fixture.dll", "start_mod", 0) != first_export,
          "second image can publish after first is detached");
    check(second.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
          "second image detaches cleanly");

    RecordingSwitch refused_segment;
    refused_segment.allow = false;
    auto refused = cap13::pe_prepare_guest_with_shim(
        bytes.data(), bytes.size(), shim, options, bank, refused_segment,
        "/mods/refused.dll");
    check(refused.ok && refused.module &&
          refused.module->process_attach(teb) == cap13::GuestNotifyStatus::BackendRefused &&
          shim.resolve("refused.dll", "start_mod", 0) == nullptr,
          "backend refusal removes provisional mapped-module registration");
    RecordingSwitch retry_segment;
    auto retry = cap13::pe_prepare_guest_with_shim(
        bytes.data(), bytes.size(), shim, options, bank, retry_segment,
        "/elsewhere/REFUSED.DLL");
    check(retry.ok && retry.module &&
          retry.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          shim.resolve("refused.dll", "start_mod", 0) != nullptr,
          "another image can attach with the same basename after refusal");
    check(retry.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
          "retry image detaches cleanly");

    shim.register_module("manual.dll", reinterpret_cast<void*>(0x200), lookup_ue4ss);
    RecordingSwitch collision_segment;
    auto collision = cap13::pe_prepare_guest_with_shim(
        bytes.data(), bytes.size(), shim, options, bank, collision_segment,
        "/mods/manual.dll");
    check(collision.ok && collision.module &&
          collision.module->process_attach(teb) ==
              cap13::GuestNotifyStatus::ModuleRegistrationFailed &&
          shim.resolve("manual.dll", "is_ue4ss_initialized", 0) ==
              reinterpret_cast<void*>(&ue4ss_initialized),
          "mapped module cannot replace a manual registry entry");

    RecordingSwitch reserved_segment;
    auto reserved = cap13::pe_prepare_guest_with_shim(
        bytes.data(), bytes.size(), shim, options, bank, reserved_segment,
        "/mods/KERNEL32.DLL");
    check(reserved.ok && reserved.module &&
          reserved.module->process_attach(teb) ==
              cap13::GuestNotifyStatus::ModuleRegistrationFailed &&
          shim.resolve("kernel32.dll", "Sleep", 0) != nullptr,
          "mapped module cannot replace a reviewed built-in DLL namespace");

    std::printf("cases=%d failed=%d\n", checks, failures);
    return failures ? 1 : 0;
}
