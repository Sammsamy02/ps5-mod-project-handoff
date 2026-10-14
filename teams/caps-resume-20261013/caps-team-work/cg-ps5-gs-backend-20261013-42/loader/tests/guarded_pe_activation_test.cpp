#include "guest_pe_loader.h"
#include "pe_loader.h"
#include "pe_types.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
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
std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
class StubResolver final : public cap13::ImportResolver
{
  public:
    void* resolve(const char*, const char*, uint64_t) override
    {
        return reinterpret_cast<void*>(&std::puts);
    }
};
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
uint32_t file_offset(const std::vector<uint8_t>& bytes, uint32_t rva)
{
    const auto* dos = reinterpret_cast<const cap13::PeDosHeader*>(bytes.data());
    const auto* coff = reinterpret_cast<const cap13::PeFileHeader*>(
        bytes.data() + dos->e_lfanew + 4);
    const auto* sections = reinterpret_cast<const cap13::PeSectionHeader*>(
        reinterpret_cast<const uint8_t*>(coff + 1) + coff->SizeOfOptionalHeader);
    for (uint16_t i = 0; i < coff->NumberOfSections; ++i)
        if (rva >= sections[i].VirtualAddress &&
            rva - sections[i].VirtualAddress < sections[i].SizeOfRawData)
            return sections[i].PointerToRawData + rva - sections[i].VirtualAddress;
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    auto bytes = read_file(argv[1]);
    check(!bytes.empty(), "unchanged real-TLS sample is available");
    if (bytes.empty()) return 2;

    StubResolver resolver;
    cap13::LoadOptions options;
    options.page_size = 0x1000;
    options.allow_rwx_partition = true;
    const auto strict = cap13::pe_load(bytes.data(), bytes.size(), resolver, options);
    check(!strict.ok && strict.reason.find("static TLS") != std::string::npos,
          "strict PE loader continues to refuse real static TLS");

    cap13::StaticTlsBank bank;
    cap13::UnavailableSegmentSwitch unavailable;
    auto guarded = cap13::pe_prepare_guest(bytes.data(), bytes.size(), resolver,
                                           options, bank, unavailable);
    check(guarded.ok && guarded.module != nullptr,
          "guarded loader maps real TLS and prepares a module");
    if (!guarded.ok)
    {
        std::printf("reason=%s\n", guarded.reason.c_str());
        return 1;
    }
    cap13::GuestTebView teb(bank);
    const auto& slots = teb.refresh().slots;
    check(slots.size() == 1 && slots[0] != nullptr,
          "guarded mapping registers one static TLS slot");
    check(guarded.module->process_attach(teb) == cap13::GuestNotifyStatus::BackendRefused,
          "unavailable segment backend refuses entry before guest calls");

    auto corrupt_index = bytes;
    const auto* dos = reinterpret_cast<const cap13::PeDosHeader*>(corrupt_index.data());
    const auto* coff = reinterpret_cast<const cap13::PeFileHeader*>(
        corrupt_index.data() + dos->e_lfanew + 4);
    const auto* opt = reinterpret_cast<const cap13::PeOptionalHeader64*>(coff + 1);
    const auto* sections = reinterpret_cast<const cap13::PeSectionHeader*>(
        reinterpret_cast<const uint8_t*>(opt) + coff->SizeOfOptionalHeader);
    const uint32_t tls_off = file_offset(corrupt_index,
        opt->DataDirectory[cap13::PE_DIR_TLS].VirtualAddress);
    check(tls_off && tls_off + sizeof(cap13::PeTlsDirectory64) <= corrupt_index.size() &&
          (sections[0].Characteristics & cap13::PE_SCN_MEM_EXECUTE),
          "real TLS fixture exposes an executable section for index corruption test");
    if (!tls_off || tls_off + sizeof(cap13::PeTlsDirectory64) > corrupt_index.size()) return 2;
    auto* corrupt_tls = reinterpret_cast<cap13::PeTlsDirectory64*>(corrupt_index.data() + tls_off);
    corrupt_tls->AddressOfIndex = opt->ImageBase + sections[0].VirtualAddress;
    cap13::StaticTlsBank reject_bank;
    auto rejected = cap13::pe_prepare_guest(corrupt_index.data(), corrupt_index.size(),
                                             resolver, options, reject_bank, unavailable);
    check(!rejected.ok && rejected.reason.find("writable") != std::string::npos &&
          reject_bank.attach_current_thread().slots.empty(),
          "TLS index targeting executable code is refused before bank registration");

    auto long_imports = read_file(argv[3]);
    cap13::StaticTlsBank long_bank;
    auto long_guest = cap13::pe_prepare_guest(long_imports.data(), long_imports.size(),
                                              resolver, options, long_bank, unavailable);
    check(long_guest.ok && long_guest.module,
          "guarded mapping accepts an unchanged real TLS DLL and its imports");
    if (!long_guest.ok)
        std::printf("long-import reason=%s\n", long_guest.reason.c_str());
    if (long_guest.ok)
    {
        cap13::GuestTebView long_teb(long_bank);
        check(long_guest.module->process_attach(long_teb) ==
                  cap13::GuestNotifyStatus::BackendRefused,
              "unchanged real DLL cannot execute without a segment backend");
    }

    // The pinned synthetic fixture has an x64 entry at file 0x400 and
    // start_mod at file 0x420. Replace only the in-memory copy with tiny
    // self-contained Win64 code; the source fixture remains unchanged.
    auto synthetic = read_file(argv[2]);
    check(synthetic.size() > 0x440 && synthetic[0] == 'M' && synthetic[1] == 'Z',
          "synthetic PE fixture has the pinned layout");
    if (synthetic.size() <= 0x440) return 2;
    const uint8_t entry_returns_true[] = {0xb8, 1, 0, 0, 0, 0xc3};
    const uint8_t export_returns_value[] = {0xb8, 0x34, 0x12, 0, 0, 0xc3};
    const uint8_t export_adds_two[] = {0x48, 0x89, 0xc8, 0x48, 0x01, 0xd0, 0xc3};
    std::memcpy(synthetic.data() + 0x400, entry_returns_true, sizeof(entry_returns_true));
    std::memcpy(synthetic.data() + 0x420, export_returns_value, sizeof(export_returns_value));
    std::memcpy(synthetic.data() + 0x430, export_adds_two, sizeof(export_adds_two));
    synthetic[0x410] = 0x65; // possible GS prefix, never executed here

    cap13::LoadOptions fixture_options = options;
    fixture_options.allow_no_relocs = true;
    const auto strict_gs = cap13::pe_load(
        synthetic.data(), synthetic.size(), resolver, fixture_options);
    check(!strict_gs.ok && strict_gs.reason.find("0x65") != std::string::npos,
          "strict path continues to refuse a possible GS executable byte");

    cap13::StaticTlsBank fixture_bank;
    RecordingSwitch sw;
    auto fixture = cap13::pe_prepare_guest(synthetic.data(), synthetic.size(),
                                           resolver, fixture_options, fixture_bank, sw);
    check(fixture.ok && fixture.module,
          "guarded path maps possible-GS image without exposing raw code");
    if (!fixture.ok)
    {
        std::printf("fixture reason=%s\n", fixture.reason.c_str());
        return 1;
    }
    cap13::GuestTebView fixture_teb(fixture_bank);
    uint64_t value = 0;
    check(fixture.module->invoke_integer_export("start_mod", fixture_teb,
                                                nullptr, 0, &value) ==
              cap13::GuestCallStatus::WrongState && sw.enters == 0,
          "export call before attachment is refused without segment entry");
    cap13::StaticTlsBank other_bank;
    cap13::GuestTebView wrong_teb(other_bank);
    check(fixture.module->process_attach(wrong_teb) ==
              cap13::GuestNotifyStatus::WrongThreadOrBank && sw.enters == 0,
          "foreign TEB is refused before segment entry");
    sw.allow = false;
    check(fixture.module->process_attach(fixture_teb) ==
              cap13::GuestNotifyStatus::BackendRefused && sw.enters == 1 && sw.leaves == 0,
          "segment refusal prevents DllMain and does not call leave");
    sw.allow = true;
    check(fixture.module->process_attach(fixture_teb) ==
              cap13::GuestNotifyStatus::Ok && sw.enters == 2 && sw.leaves == 1,
          "successful segment entry invokes Win64 DllMain and leaves");
    check(fixture.module->invoke_integer_export("start_mod", fixture_teb,
                                                nullptr, 0, &value) ==
              cap13::GuestCallStatus::Ok && value == 0x1234 &&
              sw.enters == 3 && sw.leaves == 2,
          "export call enters guest segment and returns Win64 value");
    const uint64_t add_args[] = {5, 7};
    check(fixture.module->invoke_integer_export("stop_mod", fixture_teb,
                                                add_args, 2, &value) ==
              cap13::GuestCallStatus::Ok && value == 12 &&
              sw.enters == 4 && sw.leaves == 3,
          "guarded export uses Win64 integer argument registers");
    check(fixture.module->invoke_integer_export("stop_mod", wrong_teb,
                                                add_args, 2, &value) ==
              cap13::GuestCallStatus::WrongThreadOrBank && sw.enters == 4,
          "foreign TEB export call is refused before segment entry");
    check(fixture.module->invoke_integer_export("stop_mod", fixture_teb,
                                                nullptr, 2, &value) ==
              cap13::GuestCallStatus::InvalidArguments && sw.enters == 4,
          "missing argument array is refused before segment entry");
    check(fixture.module->invoke_integer_export("stop_mod", fixture_teb,
                                                add_args, 7, &value) ==
              cap13::GuestCallStatus::UnsupportedArity && sw.enters == 4,
          "unsupported integer arity is refused before segment entry");
    sw.allow = false;
    check(fixture.module->invoke_integer_export("start_mod", fixture_teb,
                                                nullptr, 0, &value) ==
              cap13::GuestCallStatus::BackendRefused && sw.enters == 5 && sw.leaves == 3,
          "backend refusal blocks an attached export call");
    sw.allow = true;
    check(fixture.module->invoke_integer_export("missing_export", fixture_teb,
                                                nullptr, 0, &value) ==
              cap13::GuestCallStatus::MissingExport && sw.enters == 5,
          "missing export is refused before segment entry");
    check(fixture.module->process_detach(fixture_teb) ==
              cap13::GuestNotifyStatus::Ok && sw.enters == 6 && sw.leaves == 4,
          "process detach is gated and restores the segment");
    check(fixture.module->invoke_integer_export("start_mod", fixture_teb,
                                                nullptr, 0, &value) ==
              cap13::GuestCallStatus::WrongState && sw.enters == 6,
          "export after detach is refused without segment entry");

    std::printf("cases=%d failed=%d\n", checks, failures);
    return failures ? 1 : 0;
}
