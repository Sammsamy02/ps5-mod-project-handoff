#include "guest_pe_batch.h"
#include "guest_pe_directory_source.h"
#include "guest_pe_dynamic_source.h"
#include "guest_read_map.h"
#include "pe_import_scan.h"
#include "win_api_shim.h"
#include "pe_types.h"
#include "win64_bridge.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <iterator>
#include <sys/mman.h>
#include <sys/stat.h>
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
struct SourceState
{
    struct Entry
    {
        const char* name;
        const char* path;
        const std::vector<uint8_t>* bytes;
        RecordingSwitch* segment;
    };
    std::vector<Entry> entries;
    std::vector<std::string> requested;
};
bool fetch_dependency(void* user, const char* name,
                      cap13::GuestPeFetchedDependency* out)
{
    auto& source = *static_cast<SourceState*>(user);
    source.requested.emplace_back(name);
    for (const auto& entry : source.entries)
    {
        if (std::strcmp(entry.name, name) != 0) continue;
        out->source_path = entry.path;
        out->bytes = *entry.bytes;
        out->segment = entry.segment;
        return true;
    }
    return false;
}
struct GuestReadRange
{
    const char* address;
    size_t bytes;
};
struct GuestReadState
{
    std::vector<GuestReadRange> ranges;
};
bool read_guest_name(void* user, const void* address, void* output, size_t bytes)
{
    auto& state = *static_cast<GuestReadState*>(user);
    const uintptr_t requested = reinterpret_cast<uintptr_t>(address);
    for (const auto& range : state.ranges)
    {
        const uintptr_t begin = reinterpret_cast<uintptr_t>(range.address);
        if (requested >= begin && requested - begin <= range.bytes &&
            bytes <= range.bytes - (requested - begin))
        {
            std::memcpy(output, address, bytes);
            return true;
        }
    }
    return false;
}
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
    if (argc != 7) return 2;
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

    auto fixture_scan = cap13::pe_scan_imported_dlls(fixture.data(), fixture.size());
    check(fixture_scan.ok && fixture_scan.dll_names.size() == 2 &&
          fixture_scan.dll_names[0] == "UE4SS.dll" &&
          fixture_scan.dll_names[1] == "KERNEL32.dll",
          "read-only scanner names the synthetic fixture's two static DLL imports");
    bool real_scans_ok = true;
    for (int i = 2; i < 7; ++i)
    {
        auto real = read_file(argv[i]);
        auto scan = cap13::pe_scan_imported_dlls(real.data(), real.size());
        real_scans_ok &= scan.ok && !scan.dll_names.empty();
    }
    check(real_scans_ok, "five unchanged upstream DLLs yield bounded import names");

    RecordingSwitch discovered_consumer, discovered_provider;
    SourceState source;
    source.entries.push_back({"provider.dll", "/mods/provider.dll",
                              &fixture, &discovered_provider});
    auto discovered = cap13::pe_attach_guest_closure(
        {"/mods/consumer.dll", consumer.data(), consumer.size(), &discovered_consumer},
        fetch_dependency, &source, shim, options, bank, teb);
    check(discovered.ok && discovered.input_indices == std::vector<size_t>({1, 0}) &&
          source.requested == std::vector<std::string>({"provider.dll"}),
          "root consumer discovers its local provider without a caller list");
    check(cap13::pe_detach_guest_batch(discovered, teb) && discovered.modules.empty(),
          "discovered dependency closure detaches cleanly");

    RecordingSwitch closure_first, closure_middle, closure_last;
    SourceState transitive;
    transitive.entries.push_back({"middle.dll", "/mods/middle.dll",
                                  &middle, &closure_middle});
    transitive.entries.push_back({"last.dll", "/mods/last.dll",
                                  &fixture, &closure_last});
    auto transitive_result = cap13::pe_attach_guest_closure(
        {"/mods/first.dll", first.data(), first.size(), &closure_first},
        fetch_dependency, &transitive, shim, options, bank, teb);
    check(transitive_result.ok &&
          transitive_result.input_indices == std::vector<size_t>({2, 1, 0}) &&
          transitive.requested == std::vector<std::string>({"middle.dll", "last.dll"}),
          "static import closure discovers two transitive local dependencies");
    check(cap13::pe_detach_guest_batch(transitive_result, teb),
          "transitive closure detaches cleanly");

    RecordingSwitch missing_root_switch;
    SourceState missing_source;
    auto missing_closure = cap13::pe_attach_guest_closure(
        {"/mods/consumer.dll", unknown.data(), unknown.size(), &missing_root_switch},
        fetch_dependency, &missing_source, shim, options, bank, teb);
    check(!missing_closure.ok && !missing_closure.missing.empty() &&
          missing_closure.missing[0] == "external.dll!start_mod",
          "unavailable dependency stays an exact named import refusal");

    RecordingSwitch mismatch_root, mismatch_provider;
    SourceState mismatch_source;
    mismatch_source.entries.push_back({"provider.dll", "/mods/wrong.dll",
                                       &fixture, &mismatch_provider});
    auto mismatch = cap13::pe_attach_guest_closure(
        {"/mods/consumer.dll", consumer.data(), consumer.size(), &mismatch_root},
        fetch_dependency, &mismatch_source, shim, options, bank, teb);
    check(!mismatch.ok && mismatch.modules.empty() && mismatch_root.enters == 0 &&
          mismatch.reason.find("mismatched") != std::string::npos,
          "source cannot substitute a DLL under a different basename");

    RecordingSwitch closure_a, closure_b;
    SourceState cycle_source;
    cycle_source.entries.push_back({"b.dll", "/mods/b.dll", &b, &closure_b});
    auto closure_cycle = cap13::pe_attach_guest_closure(
        {"/mods/a.dll", a.data(), a.size(), &closure_a},
        fetch_dependency, &cycle_source, shim, options, bank, teb);
    check(!closure_cycle.ok && closure_cycle.modules.empty() &&
          closure_cycle.reason.find("intra-batch") != std::string::npos,
          "discovered cyclic dependency refuses before guest entry");

    auto malformed_import = fixture;
    auto* bad_descriptor = reinterpret_cast<cap13::PeImportDescriptor*>(
        malformed_import.data() + 0x600);
    bad_descriptor->Name = 0x7fffffff;
    auto malformed_scan = cap13::pe_scan_imported_dlls(
        malformed_import.data(), malformed_import.size());
    RecordingSwitch malformed_switch;
    SourceState malformed_source;
    auto malformed_closure = cap13::pe_attach_guest_closure(
        {"/mods/malformed.dll", malformed_import.data(), malformed_import.size(),
         &malformed_switch}, fetch_dependency, &malformed_source,
        shim, options, bank, teb);
    check(!malformed_scan.ok && !malformed_closure.ok &&
          malformed_switch.enters == 0 && malformed_closure.modules.empty(),
          "malformed import RVA refuses before any guest entry");

    char temp_template[] = "/tmp/cap13-deps-XXXXXX";
    char* temp_folder = mkdtemp(temp_template);
    if (!temp_folder) return 2;
    const std::string provider_path = std::string(temp_folder) + "/provider.dll";
    {
        std::ofstream stream(provider_path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(fixture.data()), fixture.size());
    }
    RecordingSwitch file_consumer_switch, file_provider_switch;
    cap13::GuestPeDirectorySource directory(temp_folder, &file_provider_switch);
    auto file_closure = cap13::pe_attach_guest_closure(
        {"/mods/consumer.dll", consumer.data(), consumer.size(), &file_consumer_switch},
        cap13::GuestPeDirectorySource::fetch, &directory,
        shim, options, bank, teb);
    check(directory.ready() && file_closure.ok &&
          file_closure.input_indices == std::vector<size_t>({1, 0}),
          "neighboring provider DLL is discovered from a real directory");
    check(cap13::pe_detach_guest_batch(file_closure, teb),
          "directory-backed closure detaches cleanly");

    const std::string duplicate_path = std::string(temp_folder) + "/Provider.DLL";
    {
        std::ofstream stream(duplicate_path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(fixture.data()), fixture.size());
    }
    struct stat provider_stat{}, duplicate_stat{};
    const bool case_insensitive_volume =
        stat(provider_path.c_str(), &provider_stat) == 0 &&
        stat(duplicate_path.c_str(), &duplicate_stat) == 0 &&
        provider_stat.st_ino == duplicate_stat.st_ino &&
        provider_stat.st_dev == duplicate_stat.st_dev;
    RecordingSwitch ambiguous_root;
    auto ambiguous = cap13::pe_attach_guest_closure(
        {"/mods/consumer.dll", consumer.data(), consumer.size(), &ambiguous_root},
        cap13::GuestPeDirectorySource::fetch, &directory,
        shim, options, bank, teb);
    check(case_insensitive_volume ? ambiguous.ok :
          (!ambiguous.ok && ambiguous_root.enters == 0 &&
           directory.last_error().find("ambiguous") != std::string::npos),
          "case variants resolve or refuse according to filesystem identity");
    if (ambiguous.ok) cap13::pe_detach_guest_batch(ambiguous, teb);
    unlink(duplicate_path.c_str());
    unlink(provider_path.c_str());
    symlink(argv[1], provider_path.c_str());
    RecordingSwitch symlink_root;
    auto unsafe = cap13::pe_attach_guest_closure(
        {"/mods/consumer.dll", consumer.data(), consumer.size(), &symlink_root},
        cap13::GuestPeDirectorySource::fetch, &directory,
        shim, options, bank, teb);
    check(!unsafe.ok && symlink_root.enters == 0 &&
          directory.last_error().find("regular") != std::string::npos,
          "symlinked dependency refuses before guest entry");
    unlink(provider_path.c_str());
    rmdir(temp_folder);

    auto crt_consumer = consumer_for(fixture, "VCRUNTIME140.dll");
    RecordingSwitch crt_consumer_switch, crt_provider_switch;
    SourceState crt_source;
    crt_source.entries.push_back({"VCRUNTIME140.dll", "/mods/VCRUNTIME140.dll",
                                  &fixture, &crt_provider_switch});
    auto crt_closure = cap13::pe_attach_guest_closure(
        {"/mods/crt-consumer.dll", crt_consumer.data(), crt_consumer.size(),
         &crt_consumer_switch}, fetch_dependency, &crt_source,
        shim, options, bank, teb);
    check(crt_closure.ok && crt_closure.input_indices ==
          std::vector<size_t>({1, 0}) &&
          crt_source.requested == std::vector<std::string>({"VCRUNTIME140.dll"}),
          "an unavailable CRT shim catalog does not block a local PE dependency");
    check(shim.resolve("VCRUNTIME140.dll", "CxxThrowException", 0) == nullptr,
          "attached local CRT does not expose the shim's unreviewed EH body");
    char crt_name[] = "VCRUNTIME140.dll";
    char start_name[] = "start_mod";
    GuestReadState guest_names{{{crt_name, sizeof(crt_name)},
                                {start_name, sizeof(start_name)}}};
    cap13::WinApiShim::Hooks hooks{};
    hooks.read_guest_bytes = read_guest_name;
    hooks.read_guest_user = &guest_names;
    shim.set_hooks(hooks);
    void* get_module = shim.resolve("kernel32.dll", "GetModuleHandleA", 0);
    void* get_proc = shim.resolve("kernel32.dll", "GetProcAddress", 0);
    const uint64_t crt_handle = get_module ? cap13_win64_call1(
        get_module, reinterpret_cast<uint64_t>(crt_name)) : 0;
    const uint64_t crt_export = get_proc ? cap13_win64_call2(
        get_proc, crt_handle, reinterpret_cast<uint64_t>(start_name)) : 0;
    check(crt_closure.ok && crt_handle != 0 && crt_export ==
          reinterpret_cast<uint64_t>(shim.resolve("VCRUNTIME140.dll", "start_mod", 0)),
          "dynamic GetProcAddress finds an export in the attached copied PE");
    check(cap13::pe_detach_guest_batch(crt_closure, teb) &&
          cap13_win64_call1(get_module, reinterpret_cast<uint64_t>(crt_name)) == 0 &&
          cap13_win64_call2(get_proc, crt_handle,
                            reinterpret_cast<uint64_t>(start_name)) == 0,
          "detached local CRT provider is no longer visible by dynamic lookup");

    auto system_consumer = consumer_for(fixture, "KERNEL32.dll");
    RecordingSwitch system_consumer_switch, system_provider_switch;
    SourceState system_source;
    system_source.entries.push_back({"KERNEL32.dll", "/mods/KERNEL32.dll",
                                     &fixture, &system_provider_switch});
    auto system_result = cap13::pe_attach_guest_closure(
        {"/mods/system-consumer.dll", system_consumer.data(),
         system_consumer.size(), &system_consumer_switch},
        fetch_dependency, &system_source, shim, options, bank, teb);
    check(!system_result.ok && system_source.requested.empty() &&
          system_provider_switch.enters == 0 &&
          !system_result.missing.empty() &&
          system_result.missing[0] == "KERNEL32.dll!start_mod",
          "reviewed system namespace stays reserved against a local DLL file");

    cap13::GuestReadMap read_map;
    int first_owner = 0, second_owner = 0;
    char readable[] = "safe-guest-text";
    char copied[sizeof(readable)]{};
    check(read_map.add(&first_owner, readable, sizeof(readable)) &&
          read_map.read(readable, copied, sizeof(copied)) &&
          std::strcmp(copied, readable) == 0,
          "owned guest range permits an exact bounded read");
    check(!read_map.add(&second_owner, readable + 2, 3) &&
          !read_map.add(&second_owner,
                        reinterpret_cast<void*>(UINTPTR_MAX - 1), 4) &&
          !read_map.read(readable + sizeof(readable) - 1, copied, 2),
          "overlap, integer wrap, and out-of-range reads refuse");
    check(read_map.remove(&first_owner) &&
          !read_map.read(readable, copied, 1),
          "owner removal revokes its guest-readable range");

    check(shim.set_guest_read_map(&read_map),
          "shim installs the bounded image reader before PE preparation");
    int no_range_owner = 0, no_range_handle = 0;
    auto no_range_lookup = +[](void*, const char*, uint64_t, void**,
                               std::string*) -> bool { return false; };
    check(shim.register_mapped_module("/mods/no-range.dll", &no_range_handle,
                                      no_range_lookup, &no_range_owner) &&
          shim.unregister_mapped_module("/mods/no-range.dll", &no_range_handle,
                                        &no_range_owner),
          "mapped registry without a read range remains removable");
    auto dynamic_image = fixture;
    auto* dynamic_import = reinterpret_cast<cap13::PeImportDescriptor*>(
        dynamic_image.data() + 0x600);
    dynamic_import->Name = 0x3100;
    const uint64_t dynamic_thunk = 0x3120;
    const uint64_t dynamic_proc_thunk = 0x3170;
    std::memcpy(dynamic_image.data() + 0x800, &dynamic_thunk,
                sizeof(dynamic_thunk));
    std::memcpy(dynamic_image.data() + 0x808, &dynamic_proc_thunk,
                sizeof(dynamic_proc_thunk));
    std::memset(dynamic_image.data() + 0x900, 0, 0x80);
    std::memcpy(dynamic_image.data() + 0x900, "KERNEL32.dll",
                sizeof("KERNEL32.dll"));
    std::memcpy(dynamic_image.data() + 0x922, "GetModuleHandleA",
                sizeof("GetModuleHandleA"));
    std::memcpy(dynamic_image.data() + 0x940, "UE4SS.dll",
                sizeof("UE4SS.dll"));
    std::memcpy(dynamic_image.data() + 0x950, "is_ue4ss_initialized",
                sizeof("is_ue4ss_initialized"));
    std::memcpy(dynamic_image.data() + 0x972, "GetProcAddress",
                sizeof("GetProcAddress"));
    const uint8_t dynamic_entry[] = {
        0x48, 0x83, 0xec, 0x28,                   // align stack and shadow space
        0x48, 0x8d, 0x0d, 0x35, 0x21, 0x00, 0x00, // lea rcx,[rip+0x2135] -> module RVA 0x3140
        0x48, 0x8b, 0x05, 0x0e, 0x20, 0x00, 0x00, // mov rax,[rip+0x200e] -> first IAT RVA 0x3020
        0xff, 0xd0,                               // call GetModuleHandleA
        0x48, 0x89, 0xc1,                         // mov rcx,rax
        0x48, 0x8d, 0x15, 0x32, 0x21, 0x00, 0x00, // lea rdx,[rip+0x2132] -> symbol RVA 0x3150
        0x48, 0x8b, 0x05, 0x03, 0x20, 0x00, 0x00, // mov rax,[rip+0x2003] -> second IAT RVA 0x3028
        0xff, 0xd0,                               // call GetProcAddress
        0x48, 0x85, 0xc0, 0x0f, 0x95, 0xc0,
        0x0f, 0xb6, 0xc0,
        0x48, 0x83, 0xc4, 0x28, 0xc3              // return TRUE iff export found
    };
    std::memcpy(dynamic_image.data() + 0x400, dynamic_entry,
                sizeof(dynamic_entry));
    RecordingSwitch dynamic_switch;
    auto dynamic_pe = cap13::pe_prepare_guest_with_shim(
        dynamic_image.data(), dynamic_image.size(), shim, options, bank,
        dynamic_switch, "/mods/dynamic.dll");
    check(dynamic_pe.ok && dynamic_pe.module,
          "static PE imports of dynamic lookup APIs prepare with a registered read map");
    if (dynamic_pe.ok && dynamic_pe.module)
    {
        const auto attach = dynamic_pe.module->process_attach(teb);
        check(attach == cap13::GuestNotifyStatus::Ok &&
              dynamic_switch.enters == 1,
              "DllMain resolves a module and export from its image literals through the bounded map");
        void* entry = shim.resolve("dynamic.dll", "start_mod", 0);
        uint8_t byte = 0;
        check(entry && read_map.read(entry, &byte, 1),
              "attached PE image is readable through the map");
        check(dynamic_pe.module->process_detach(teb) ==
                  cap13::GuestNotifyStatus::Ok &&
              !read_map.read(entry, &byte, 1),
              "detached PE image is revoked from the read map");
    }

    char dynamic_template[] = "/tmp/cap13-dynamic-XXXXXX";
    char* dynamic_folder = mkdtemp(dynamic_template);
    if (!dynamic_folder) return 2;
    const std::string dynamic_path = std::string(dynamic_folder) + "/late.dll";
    {
        std::ofstream stream(dynamic_path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(fixture.data()), fixture.size());
    }
    RecordingSwitch dynamic_loader_switch;
    cap13::GuestPeDirectorySource dynamic_directory(dynamic_folder,
                                                     &dynamic_loader_switch);
    std::vector<cap13::GuestPeBatchResult> retained_dynamic;
    retained_dynamic.reserve(2);
    cap13::GuestPeDynamicLoadContext dynamic_context{};
    dynamic_context.fetch = cap13::GuestPeDirectorySource::fetch;
    dynamic_context.fetch_user = &dynamic_directory;
    dynamic_context.shim = &shim;
    dynamic_context.options = options;
    dynamic_context.bank = &bank;
    dynamic_context.teb = &teb;
    dynamic_context.retained = &retained_dynamic;
    cap13::WinApiShim::Hooks dynamic_hooks{};
    dynamic_hooks.load_pe = cap13::pe_load_guest_from_source;
    dynamic_hooks.load_user = &dynamic_context;
    shim.set_hooks(dynamic_hooks);
    char dynamic_request[] = "late.dll";
    check(read_map.add(&dynamic_request, dynamic_request,
                       sizeof(dynamic_request)),
          "dynamic DLL request string enters the bounded guest read map");
    void* load_library = shim.resolve("kernel32.dll", "LoadLibraryA", 0);
    const uint64_t loaded_handle = load_library ? cap13_win64_call1(
        load_library, reinterpret_cast<uint64_t>(dynamic_request)) : 0;
    check(dynamic_directory.ready() && loaded_handle &&
          retained_dynamic.size() == 1 && retained_dynamic[0].ok &&
          dynamic_loader_switch.enters == 1 &&
          dynamic_context.last_error.empty(),
          "LoadLibraryA attaches an unchanged PE from the bounded directory source");
    if (loaded_handle && retained_dynamic.size() == 1)
    {
        uint8_t first_byte = 0;
        check(read_map.read(reinterpret_cast<void*>(loaded_handle),
                            &first_byte, 1) &&
              shim.resolve("late.dll", "start_mod", 0) != nullptr,
              "dynamically attached PE publishes its image and named export");
        check(cap13::pe_detach_guest_batch(retained_dynamic[0], teb) &&
              !read_map.read(reinterpret_cast<void*>(loaded_handle),
                             &first_byte, 1) &&
              shim.resolve("late.dll", "start_mod", 0) == nullptr,
              "dynamic batch detach revokes its image and export");
    }
    SourceState wrong_dynamic_source;
    RecordingSwitch wrong_dynamic_switch;
    wrong_dynamic_source.entries.push_back({"wrong.dll", "/mods/late.dll",
                                            &fixture, &wrong_dynamic_switch});
    dynamic_context.fetch = fetch_dependency;
    dynamic_context.fetch_user = &wrong_dynamic_source;
    check(cap13::pe_load_guest_from_source(&dynamic_context, "wrong.dll") ==
              nullptr && wrong_dynamic_switch.enters == 0 &&
          dynamic_context.last_error.find("basename") != std::string::npos,
          "dynamic source cannot substitute a PE under a different basename");
    dynamic_context.fetch = cap13::GuestPeDirectorySource::fetch;
    dynamic_context.fetch_user = &dynamic_directory;
    retained_dynamic.emplace_back();
    const int before_full = dynamic_loader_switch.enters;
    check(cap13::pe_load_guest_from_source(&dynamic_context, "late.dll") ==
              nullptr && dynamic_loader_switch.enters == before_full &&
          dynamic_context.last_error.find("capacity") != std::string::npos,
          "full retention storage refuses before another PE attach");
    retained_dynamic.pop_back();
    RecordingSwitch dynamic_fail_switch, dynamic_rollback_switch;
    dynamic_rollback_switch.allow_until = 1;
    SourceState dynamic_failure_source;
    dynamic_failure_source.entries.push_back(
        {"badroot.dll", "/mods/badroot.dll", &failed_consumer,
         &dynamic_fail_switch});
    dynamic_failure_source.entries.push_back(
        {"provider.dll", "/mods/provider.dll", &fixture,
         &dynamic_rollback_switch});
    dynamic_context.fetch = fetch_dependency;
    dynamic_context.fetch_user = &dynamic_failure_source;
    check(cap13::pe_load_guest_from_source(&dynamic_context, "badroot.dll") ==
              nullptr && retained_dynamic.size() == 2 &&
          retained_dynamic[1].rollback_failed &&
          retained_dynamic[1].modules.size() == 1 &&
          shim.resolve("provider.dll", "start_mod", 0) != nullptr,
          "dynamic load retains a provider when rollback cannot detach it");
    dynamic_rollback_switch.allow_until = 1000;
    check(cap13::pe_detach_guest_batch(retained_dynamic[1], teb) &&
          retained_dynamic[1].modules.empty() &&
          shim.resolve("provider.dll", "start_mod", 0) == nullptr,
          "retained dynamic rollback can be cleaned after backend recovery");
    check(read_map.remove(&dynamic_request),
          "dynamic request range is revoked after use");
    unlink(dynamic_path.c_str());
    rmdir(dynamic_folder);

    std::printf("cases=%d failed=%d\n", checks, failures);
    return failures ? 1 : 0;
}
