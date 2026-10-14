#include "guest_pe_batch.h"
#include "win_api_shim.h"
#include "pe_types.h"

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
int ue4ss_calls = 0;
void check(bool yes, const char* label)
{
    ++checks;
    std::printf("%s %s\n", yes ? "PASS" : "FAIL", label);
    if (!yes) ++failures;
}
std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
bool allocate_writable(size_t minimum, void** address, size_t* bytes, void*)
{
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* memory = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (memory == MAP_FAILED) return false;
    *address = memory;
    *bytes = page;
    return true;
}
bool seal_executable(void* memory, size_t bytes, void*)
{
    return mprotect(memory, bytes, PROT_READ | PROT_EXEC) == 0;
}
void release_mapping(void* memory, size_t bytes, void*) { munmap(memory, bytes); }
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
    int enters = 0;
    int allow_until = 1000;
    bool enter(void* teb) override { return ++enters <= allow_until && teb; }
    void leave() noexcept override {}
};
std::vector<uint8_t> consumer_for(const std::vector<uint8_t>& fixture,
                                  const char* provider)
{
    auto copy = fixture;
    auto* descriptor = reinterpret_cast<cap13::PeImportDescriptor*>(copy.data() + 0x600);
    descriptor->Name = 0x3100;
    const uint64_t name_thunk = 0x3120;
    std::memcpy(copy.data() + 0x800, &name_thunk, sizeof(name_thunk));
    std::memset(copy.data() + 0x900, 0, 0x20);
    std::memcpy(copy.data() + 0x900, provider, std::strlen(provider) + 1);
    std::memcpy(copy.data() + 0x922, "start_mod", sizeof("start_mod"));
    const uint8_t call_import_entry[] = {
        0x48, 0x8b, 0x05, 0x19, 0x20, 0x00, 0x00,
        0xff, 0xd0, 0xb8, 0x01, 0x00, 0x00, 0x00, 0xc3
    };
    std::memcpy(copy.data() + 0x400, call_import_entry, sizeof(call_import_entry));
    return copy;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    auto fixture = read_file(argv[1]);
    if (fixture.empty()) return 2;
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

    auto consumer = consumer_for(fixture, "provider.dll");
    RecordingSwitch consumer_switch, provider_switch;
    const int before = ue4ss_calls;
    std::vector<cap13::GuestPeBatchInput> inputs = {
        {"/mods/consumer.dll", consumer.data(), consumer.size(), &consumer_switch},
        {"C:\\mods\\Provider.DLL", fixture.data(), fixture.size(), &provider_switch},
    };
    auto ordered = cap13::pe_attach_guest_batch(inputs, shim, options, bank, teb);
    check(ordered.ok && ordered.input_indices == std::vector<size_t>({1, 0}) &&
          ordered.modules.size() == 2 && ue4ss_calls > before,
          "consumer-first batch attaches provider then calls it through the IAT");
    check(cap13::pe_detach_guest_batch(ordered, teb) && ordered.modules.empty() &&
          shim.resolve("provider.dll", "start_mod", 0) == nullptr,
          "successful batch detaches in reverse order and removes exports");

    auto first = consumer_for(fixture, "middle.dll");
    auto middle = consumer_for(fixture, "last.dll");
    RecordingSwitch first_switch, middle_switch, last_switch;
    auto chain = cap13::pe_attach_guest_batch(
        {{"/mods/first.dll", first.data(), first.size(), &first_switch},
         {"/mods/middle.dll", middle.data(), middle.size(), &middle_switch},
         {"/mods/last.dll", fixture.data(), fixture.size(), &last_switch}},
        shim, options, bank, teb);
    check(chain.ok && chain.input_indices == std::vector<size_t>({2, 1, 0}),
          "three-DLL chain attaches in dependency order across retries");
    check(cap13::pe_detach_guest_batch(chain, teb) && chain.modules.empty(),
          "three-DLL chain detaches in reverse attachment order");

    auto unknown = consumer_for(fixture, "external.dll");
    RecordingSwitch unknown_switch;
    auto external = cap13::pe_attach_guest_batch(
        {{"/mods/consumer.dll", unknown.data(), unknown.size(), &unknown_switch}},
        shim, options, bank, teb);
    check(!external.ok && external.modules.empty() &&
          !external.missing.empty() &&
          external.missing[0] == "external.dll!start_mod",
          "unsupported external import refuses with its exact name");

    auto absent_export = consumer;
    std::memset(absent_export.data() + 0x922, 0, 0x20);
    std::memcpy(absent_export.data() + 0x922, "missing_export",
                sizeof("missing_export"));
    RecordingSwitch absent_switch, absent_provider_switch;
    auto absent = cap13::pe_attach_guest_batch(
        {{"/mods/consumer.dll", absent_export.data(), absent_export.size(), &absent_switch},
         {"/mods/provider.dll", fixture.data(), fixture.size(), &absent_provider_switch}},
        shim, options, bank, teb);
    check(!absent.ok && !absent.rollback_failed && absent.modules.empty() &&
          !absent.missing.empty() &&
          absent.missing[0] == "provider.dll!missing_export" &&
          shim.resolve("provider.dll", "start_mod", 0) == nullptr,
          "absent export in attached provider refuses and rolls it back");

    RecordingSwitch duplicate_a, duplicate_b;
    auto duplicate = cap13::pe_attach_guest_batch(
        {{"/mods/FIXTURE.dll", fixture.data(), fixture.size(), &duplicate_a},
         {"C:\\other\\fixture.DLL", fixture.data(), fixture.size(), &duplicate_b}},
        shim, options, bank, teb);
    check(!duplicate.ok && duplicate.modules.empty() &&
          duplicate_a.enters == 0 && duplicate_b.enters == 0 &&
          duplicate.reason.find("duplicate") != std::string::npos,
          "duplicate normalized DLL basename refuses before attachment");

    auto a = consumer_for(fixture, "b.dll");
    auto b = consumer_for(fixture, "a.dll");
    RecordingSwitch a_switch, b_switch;
    auto cycle = cap13::pe_attach_guest_batch(
        {{"/mods/a.dll", a.data(), a.size(), &a_switch},
         {"/mods/b.dll", b.data(), b.size(), &b_switch}},
        shim, options, bank, teb);
    check(!cycle.ok && cycle.modules.empty() &&
          cycle.reason.find("intra-batch") != std::string::npos,
          "intra-batch import cycle refuses without guest entry");

    auto failed_consumer = consumer;
    const uint8_t false_entry[] = {0x31, 0xc0, 0xc3};
    std::memcpy(failed_consumer.data() + 0x400, false_entry, sizeof(false_entry));
    RecordingSwitch fail_switch, rollback_switch;
    auto failed = cap13::pe_attach_guest_batch(
        {{"/mods/consumer.dll", failed_consumer.data(), failed_consumer.size(), &fail_switch},
         {"/mods/provider.dll", fixture.data(), fixture.size(), &rollback_switch}},
        shim, options, bank, teb);
    check(!failed.ok && !failed.rollback_failed && failed.modules.empty() &&
          shim.resolve("provider.dll", "start_mod", 0) == nullptr,
          "later DllMain refusal rolls back attached provider");

    RecordingSwitch dirty_fail_switch, dirty_rollback_switch;
    dirty_rollback_switch.allow_until = 1;
    auto dirty = cap13::pe_attach_guest_batch(
        {{"/mods/consumer.dll", failed_consumer.data(), failed_consumer.size(), &dirty_fail_switch},
         {"/mods/provider.dll", fixture.data(), fixture.size(), &dirty_rollback_switch}},
        shim, options, bank, teb);
    check(!dirty.ok && dirty.rollback_failed && dirty.modules.size() == 1 &&
          shim.resolve("provider.dll", "start_mod", 0) != nullptr,
          "failed rollback retains live provider for explicit cleanup");
    dirty_rollback_switch.allow_until = 1000;
    check(cap13::pe_detach_guest_batch(dirty, teb) && dirty.modules.empty() &&
          shim.resolve("provider.dll", "start_mod", 0) == nullptr,
          "caller can retry cleanup after segment backend recovers");

    std::printf("cases=%d failed=%d\n", checks, failures);
    return failures ? 1 : 0;
}
