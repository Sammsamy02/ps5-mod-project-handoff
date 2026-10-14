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

    auto ordinal_bytes = consumer_bytes;
    const uint64_t ordinal_thunk = 0x8000000000000001ull; // fixture!start_mod
    std::memcpy(ordinal_bytes.data() + 0x800, &ordinal_thunk, sizeof(ordinal_thunk));
    auto early_ordinal = cap13::pe_prepare_guest_with_shim(
        ordinal_bytes.data(), ordinal_bytes.size(), shim, options, bank,
        consumer_segment, "/mods/ordinal-consumer.dll");
    bool missing_ordinal = false;
    for (const auto& name : early_ordinal.missing)
        if (name == "fixture.dll!#1") missing_ordinal = true;
    check(!early_ordinal.ok && missing_ordinal,
          "ordinal import refuses while provider has not completed attachment");

    auto malformed_ordinal = ordinal_bytes;
    const uint64_t reserved_bits = 0x8000000100000001ull;
    std::memcpy(malformed_ordinal.data() + 0x800, &reserved_bits,
                sizeof(reserved_bits));
    auto malformed = cap13::pe_prepare_guest_with_shim(
        malformed_ordinal.data(), malformed_ordinal.size(), shim, options, bank,
        consumer_segment, "/mods/malformed-consumer.dll");
    check(!malformed.ok && malformed.reason == "corrupt import ordinal thunk",
          "reserved ordinal thunk bits are rejected as malformed PE");

    check(first.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          !first_segment.first_enter_saw_export,
          "provisional module registration stays hidden during DllMain entry");
    void* first_export = shim.resolve("FIXTURE.DLL", "start_mod", 0);
    check(first_export != nullptr &&
          shim.resolve("fixture.dll", "missing_export", 0) == nullptr &&
          shim.resolve("fixture.dll", nullptr, 1) == first_export &&
          shim.resolve("fixture.dll", nullptr, 0) == nullptr &&
          shim.resolve("fixture.dll", nullptr, 3) == nullptr &&
          shim.resolve("fixture.dll", nullptr, 0x10001) == nullptr,
          "attached module publishes validated name and bounded ordinal exports");
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
    auto ordinal_consumer = cap13::pe_prepare_guest_with_shim(
        ordinal_bytes.data(), ordinal_bytes.size(), shim, options, bank,
        consumer_segment, "/mods/ordinal-consumer.dll");
    check(ordinal_consumer.ok && ordinal_consumer.module,
          "dependent DLL resolves ordinal import from attached mapped provider");
    if (ordinal_consumer.ok)
    {
        const int before_ordinal_calls = ue4ss_calls;
        check(ordinal_consumer.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
              ue4ss_calls == before_ordinal_calls + 1,
              "dependent DllMain calls provider ordinal export through its IAT");
        check(ordinal_consumer.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
              "ordinal dependent module detaches before its provider");
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

    // The unchanged fixture has ExportDirectory at file offset 0x640 and
    // its first EAT RVA at 0x678. Mutate copies only; do not edit the source.
    const struct { uint32_t rva; const char* path; const char* label; } bad_exports[] = {
        {0, "/mods/sparse.dll", "zero EAT slot refuses ordinal export"},
        {0x2050, "/mods/forwarder.dll", "forwarder RVA refuses ordinal export"},
        {0x2000, "/mods/data-export.dll", "non-executable RVA refuses ordinal export"},
    };
    for (const auto& bad : bad_exports)
    {
        auto modified = bytes;
        std::memcpy(modified.data() + 0x678, &bad.rva, sizeof(bad.rva));
        RecordingSwitch segment;
        auto bad_module = cap13::pe_prepare_guest_with_shim(
            modified.data(), modified.size(), shim, options, bank, segment, bad.path);
        const bool prepared = bad_module.ok && bad_module.module;
        const bool attached = prepared &&
            bad_module.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok;
        check(attached && shim.resolve(bad.path, nullptr, 1) == nullptr,
              bad.label);
        if (attached)
            check(bad_module.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
                  "invalid export fixture detaches cleanly");
    }

    auto rebased_exports = bytes;
    const uint32_t export_base = 7;
    std::memcpy(rebased_exports.data() + 0x650, &export_base, sizeof(export_base));
    RecordingSwitch rebased_segment;
    auto rebased = cap13::pe_prepare_guest_with_shim(
        rebased_exports.data(), rebased_exports.size(), shim, options, bank,
        rebased_segment, "/mods/rebased.dll");
    const bool rebased_attached = rebased.ok && rebased.module &&
        rebased.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok;
    check(rebased_attached &&
          shim.resolve("rebased.dll", nullptr, 1) == nullptr &&
          shim.resolve("rebased.dll", nullptr, 7) ==
              shim.resolve("rebased.dll", "start_mod", 0),
          "ordinal lookup subtracts the declared export base");
    if (rebased_attached)
        check(rebased.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
              "rebased export fixture detaches cleanly");

    // Relocate copied EAT entries into an enlarged export directory and
    // place standard forwarder strings in otherwise-unused fixture bytes.
    auto forward_bytes = bytes;
    const auto* dos = reinterpret_cast<const cap13::PeDosHeader*>(forward_bytes.data());
    auto* opt = reinterpret_cast<cap13::PeOptionalHeader64*>(
        forward_bytes.data() + dos->e_lfanew + 4 + sizeof(cap13::PeFileHeader));
    opt->DataDirectory[cap13::PE_DIR_EXPORT].Size = 0x180;
    const uint32_t named_forward_rva = 0x2170;
    const uint32_t ordinal_forward_rva = 0x2190;
    std::memcpy(forward_bytes.data() + 0x678, &named_forward_rva, 4);
    std::memcpy(forward_bytes.data() + 0x67c, &ordinal_forward_rva, 4);
    std::memcpy(forward_bytes.data() + 0x770, "forward-target.start_mod",
                sizeof("forward-target.start_mod"));
    std::memcpy(forward_bytes.data() + 0x790, "forward-target.#1",
                sizeof("forward-target.#1"));

    RecordingSwitch target_segment;
    auto target = cap13::pe_prepare_guest_with_shim(
        bytes.data(), bytes.size(), shim, options, bank, target_segment,
        "/mods/forward-target.dll");
    const bool target_attached = target.ok && target.module &&
        target.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok;
    check(target_attached, "forwarder target attaches");
    RecordingSwitch proxy_segment;
    auto proxy = cap13::pe_prepare_guest_with_shim(
        forward_bytes.data(), forward_bytes.size(), shim, options, bank,
        proxy_segment, "/mods/forward-proxy.dll");
    const bool proxy_attached = proxy.ok && proxy.module &&
        proxy.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok;
    check(proxy_attached, "forwarder proxy attaches");
    if (target_attached && proxy_attached)
    {
        void* expected = shim.resolve("forward-target.dll", "start_mod", 0);
        check(expected &&
              shim.resolve("forward-proxy.dll", "start_mod", 0) == expected &&
              shim.resolve("forward-proxy.dll", nullptr, 1) == expected &&
              shim.resolve("forward-proxy.dll", "stop_mod", 0) == expected &&
              shim.resolve("forward-proxy.dll", nullptr, 2) == expected,
              "named and ordinal forwarders resolve through attached provider");

        auto forwarded_consumer_bytes = consumer_bytes;
        std::memcpy(forwarded_consumer_bytes.data() + 0x900,
                    "forward-proxy.dll", sizeof("forward-proxy.dll"));
        RecordingSwitch forwarded_consumer_segment;
        auto forwarded_consumer = cap13::pe_prepare_guest_with_shim(
            forwarded_consumer_bytes.data(), forwarded_consumer_bytes.size(),
            shim, options, bank, forwarded_consumer_segment,
            "/mods/forwarded-consumer.dll");
        check(forwarded_consumer.ok && forwarded_consumer.module,
              "dependent PE import resolves through mapped forwarder");
        if (forwarded_consumer.ok)
        {
            const int before_forwarded_call = ue4ss_calls;
            check(forwarded_consumer.module->process_attach(teb) ==
                      cap13::GuestNotifyStatus::Ok &&
                  ue4ss_calls == before_forwarded_call + 1,
                  "dependent DllMain calls forwarded provider through its IAT");
            check(forwarded_consumer.module->process_detach(teb) ==
                      cap13::GuestNotifyStatus::Ok,
                  "forwarded dependent detaches before proxy");
        }

        auto chain_bytes = forward_bytes;
        std::memcpy(chain_bytes.data() + 0x770, "forward-proxy.start_mod",
                    sizeof("forward-proxy.start_mod"));
        RecordingSwitch chain_segment;
        auto chain = cap13::pe_prepare_guest_with_shim(
            chain_bytes.data(), chain_bytes.size(), shim, options, bank,
            chain_segment, "/mods/forward-chain.dll");
        const bool chain_attached = chain.ok && chain.module &&
            chain.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok;
        check(chain_attached &&
              shim.resolve("forward-chain.dll", "start_mod", 0) == expected,
              "two-hop mapped forwarder chain reaches attached provider");
        if (chain_attached)
            check(chain.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
                  "forwarder chain detaches before proxy");

        const struct { const char* text; const char* path; const char* label; }
            refused_forwarders[] = {
                {"forward-target.#65536", "/mods/large-ordinal.dll",
                 "out-of-range forwarded ordinal refuses"},
                {"missing.start_mod", "/mods/missing-forward.dll",
                 "missing forwarded module refuses"},
                {"malformed", "/mods/malformed-forward.dll",
                 "forwarder without module separator refuses"},
            };
        for (const auto& bad : refused_forwarders)
        {
            auto bad_bytes = forward_bytes;
            std::memcpy(bad_bytes.data() + 0x770, bad.text,
                        std::strlen(bad.text) + 1);
            RecordingSwitch bad_segment;
            auto bad_module = cap13::pe_prepare_guest_with_shim(
                bad_bytes.data(), bad_bytes.size(), shim, options, bank,
                bad_segment, bad.path);
            const bool attached = bad_module.ok && bad_module.module &&
                bad_module.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok;
            check(attached && shim.resolve(bad.path, "start_mod", 0) == nullptr,
                  bad.label);
            if (attached)
                check(bad_module.module->process_detach(teb) ==
                          cap13::GuestNotifyStatus::Ok,
                      "refused-forwarder fixture detaches cleanly");
        }

        auto unterminated = forward_bytes;
        const uint32_t last_forward_rva = 0x21b0;
        std::memcpy(unterminated.data() + 0x678, &last_forward_rva, 4);
        std::memset(unterminated.data() + 0x7b0, 'x', 16);
        RecordingSwitch unterminated_segment;
        auto unterminated_module = cap13::pe_prepare_guest_with_shim(
            unterminated.data(), unterminated.size(), shim, options, bank,
            unterminated_segment, "/mods/unterminated.dll");
        const bool unterminated_attached = unterminated_module.ok &&
            unterminated_module.module &&
            unterminated_module.module->process_attach(teb) ==
                cap13::GuestNotifyStatus::Ok;
        check(unterminated_attached &&
              shim.resolve("unterminated.dll", "start_mod", 0) == nullptr,
              "unterminated forwarder refuses at export-directory boundary");
        if (unterminated_attached)
            check(unterminated_module.module->process_detach(teb) ==
                      cap13::GuestNotifyStatus::Ok,
                  "unterminated-forwarder fixture detaches cleanly");
    }
    if (proxy_attached)
        check(proxy.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
              "forwarder proxy detaches cleanly");
    if (target_attached)
        check(target.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
              "forwarder target detaches cleanly");

    auto cycle_a_bytes = forward_bytes;
    auto cycle_b_bytes = forward_bytes;
    std::memcpy(cycle_a_bytes.data() + 0x770, "cycle-b.start_mod",
                sizeof("cycle-b.start_mod"));
    std::memcpy(cycle_b_bytes.data() + 0x770, "cycle-a.start_mod",
                sizeof("cycle-a.start_mod"));
    RecordingSwitch cycle_a_segment;
    RecordingSwitch cycle_b_segment;
    auto cycle_a = cap13::pe_prepare_guest_with_shim(
        cycle_a_bytes.data(), cycle_a_bytes.size(), shim, options, bank,
        cycle_a_segment, "/mods/cycle-a.dll");
    auto cycle_b = cap13::pe_prepare_guest_with_shim(
        cycle_b_bytes.data(), cycle_b_bytes.size(), shim, options, bank,
        cycle_b_segment, "/mods/cycle-b.dll");
    const bool cycle_attached = cycle_a.ok && cycle_b.ok &&
        cycle_a.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
        cycle_b.module->process_attach(teb) == cap13::GuestNotifyStatus::Ok;
    check(cycle_attached &&
          shim.resolve("cycle-a.dll", "start_mod", 0) == nullptr,
          "forwarder cycle refuses without recursion or a raw pointer");
    if (cycle_attached)
    {
        check(cycle_b.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
              "cycle second module detaches cleanly");
        check(cycle_a.module->process_detach(teb) == cap13::GuestNotifyStatus::Ok,
              "cycle first module detaches cleanly");
    }

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
