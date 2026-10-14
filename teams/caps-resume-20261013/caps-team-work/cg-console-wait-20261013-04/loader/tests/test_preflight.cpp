#include "pe_loader.h"
#include "pe_types.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace cap13;

struct StubResolver final : ImportResolver {
    void* resolve(const char*, const char*, uint64_t) override {
        return reinterpret_cast<void*>(&std::puts);
    }
};

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static PeFileHeader* coff(std::vector<uint8_t>& b) {
    const auto* dos = reinterpret_cast<const PeDosHeader*>(b.data());
    return reinterpret_cast<PeFileHeader*>(b.data() + dos->e_lfanew + 4);
}
static PeOptionalHeader64* opt(std::vector<uint8_t>& b) {
    return reinterpret_cast<PeOptionalHeader64*>(coff(b) + 1);
}
static uint32_t file_offset(std::vector<uint8_t>& b, uint32_t rva) {
    auto* sections = reinterpret_cast<PeSectionHeader*>(
        reinterpret_cast<uint8_t*>(opt(b)) + coff(b)->SizeOfOptionalHeader);
    for (uint16_t i = 0; i < coff(b)->NumberOfSections; ++i) {
        const auto& sec = sections[i];
        if (rva >= sec.VirtualAddress && rva - sec.VirtualAddress < sec.SizeOfRawData)
            return sec.PointerToRawData + (rva - sec.VirtualAddress);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 5) return 2;
    const std::string which = argv[1];
    const char* input = (which == "empty_tls" || which == "bad_tls" ||
                         which == "bad_tls_oversize" || which == "tls_overlay" ||
                         which == "bad_reloc_target" || which == "bad_reloc_odd" ||
                         which == "empty_reloc" || which == "absolute_reloc")
        ? argv[3] : ((which == "real_tls" || which == "tls_zero_size") ? argv[4] : argv[2]);
    auto bytes = read_file(input);
    if (bytes.empty()) return 3;
    LoadOptions options;
    options.page_size = 0x1000; // Rosetta's actual page granularity.
    options.allow_rwx_partition = true; // independent of this preflight test.
    if (which == "baseline" || which == "bad_optional" ||
        which == "bad_import_dir" || which == "bad_export" ||
        which == "nonexec_export" || which == "export_virtual_tail" ||
        which == "bad_entry" || which == "bad_delay" ||
        which == "import_overlay" || which == "overlap_sections" ||
        which == "entry_virtual_tail" || which == "wrong_page_size" ||
        which == "import_zero_size")
        options.allow_no_relocs = true; // known position-independent fixture.
    if (which == "wrong_page_size") options.page_size = 0x4000;

    if (which == "bad_optional") coff(bytes)->SizeOfOptionalHeader = 0xffff;
    if (which == "bad_tls") opt(bytes)->DataDirectory[PE_DIR_TLS].VirtualAddress = 0xffffff00u;
    if (which == "tls_zero_size") opt(bytes)->DataDirectory[PE_DIR_TLS].Size = 0;
    if (which == "bad_tls_oversize") opt(bytes)->DataDirectory[PE_DIR_TLS].Size = opt(bytes)->SizeOfImage;
    if (which == "bad_entry") opt(bytes)->AddressOfEntryPoint = opt(bytes)->SizeOfImage + 0x1000;
    if (which == "bad_delay") {
        opt(bytes)->DataDirectory[PE_DIR_DELAYIMPORT].VirtualAddress = 0x1000;
        opt(bytes)->DataDirectory[PE_DIR_DELAYIMPORT].Size = 32;
    }
    if (which == "bad_import_dir") opt(bytes)->DataDirectory[PE_DIR_IMPORT].Size = sizeof(PeImportDescriptor);
    if (which == "import_zero_size") opt(bytes)->DataDirectory[PE_DIR_IMPORT].Size = 0;
    if (which == "overlap_sections") {
        auto* sections = reinterpret_cast<PeSectionHeader*>(
            reinterpret_cast<uint8_t*>(opt(bytes)) + coff(bytes)->SizeOfOptionalHeader);
        sections[1].VirtualAddress = sections[0].VirtualAddress;
    }
    if (which == "entry_virtual_tail") {
        auto* sections = reinterpret_cast<PeSectionHeader*>(
            reinterpret_cast<uint8_t*>(opt(bytes)) + coff(bytes)->SizeOfOptionalHeader);
        sections[0].VirtualSize = 1;
        opt(bytes)->AddressOfEntryPoint = sections[0].VirtualAddress + 0x100;
    }
    if (which == "tls_overlay" || which == "import_overlay") {
        auto* sections = reinterpret_cast<PeSectionHeader*>(
            reinterpret_cast<uint8_t*>(opt(bytes)) + coff(bytes)->SizeOfOptionalHeader);
        const auto& sec = sections[coff(bytes)->NumberOfSections - 1];
        const uint32_t rva = sec.VirtualAddress + sec.SizeOfRawData - 4;
        if (which == "tls_overlay") {
            opt(bytes)->DataDirectory[PE_DIR_TLS].VirtualAddress = rva;
            opt(bytes)->DataDirectory[PE_DIR_TLS].Size = sizeof(PeTlsDirectory64);
        } else {
            opt(bytes)->DataDirectory[PE_DIR_IMPORT].VirtualAddress = rva;
            opt(bytes)->DataDirectory[PE_DIR_IMPORT].Size = sizeof(PeImportDescriptor);
        }
        bytes.resize(bytes.size() + 64, 0);
    }
    if (which == "helper_bad_pe") {
        reinterpret_cast<PeDosHeader*>(bytes.data())->e_lfanew = INT32_MAX;
        const bool ok = !pe_has_tls_directory(bytes.data(), bytes.size()) &&
                        !pe_has_com_descriptor(bytes.data(), bytes.size());
        std::printf("%s %s\n", which.c_str(), ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }
    if (which == "bad_reloc_target" || which == "bad_reloc_odd" ||
        which == "empty_reloc" || which == "absolute_reloc") {
        auto* block = reinterpret_cast<PeRelocBlock*>(
            bytes.data() + file_offset(bytes, opt(bytes)->DataDirectory[PE_DIR_BASERELOC].VirtualAddress));
        if (which == "bad_reloc_target") block->PageRva = opt(bytes)->SizeOfImage + 0x1000;
        else if (which == "bad_reloc_odd") block->BlockSize = 9;
        else if (which == "empty_reloc") {
            block->BlockSize = sizeof(PeRelocBlock);
            opt(bytes)->DataDirectory[PE_DIR_BASERELOC].Size = sizeof(PeRelocBlock);
        } else {
            auto* entry = reinterpret_cast<PeRelocEntry*>(block + 1);
            const size_t count = (block->BlockSize - sizeof(PeRelocBlock)) / sizeof(PeRelocEntry);
            for (size_t i = 0; i < count; ++i) entry[i].Type = PE_REL_ABSOLUTE;
        }
    }
    if (which == "bad_export" || which == "nonexec_export" ||
        which == "export_virtual_tail") {
        auto* sections = reinterpret_cast<PeSectionHeader*>(
            reinterpret_cast<uint8_t*>(opt(bytes)) + coff(bytes)->SizeOfOptionalHeader);
        const uint32_t rva = opt(bytes)->DataDirectory[PE_DIR_EXPORT].VirtualAddress;
        for (uint16_t i = 0; i < coff(bytes)->NumberOfSections; ++i) {
            const auto& sec = sections[i];
            if (rva >= sec.VirtualAddress && rva - sec.VirtualAddress < sec.SizeOfRawData) {
                auto* ex = reinterpret_cast<PeExportDirectory*>(
                    bytes.data() + sec.PointerToRawData + (rva - sec.VirtualAddress));
                if (which == "bad_export") ex->AddressOfNames = 0xfffffff0u;
                else {
                    const uint32_t funcs_off = file_offset(bytes, ex->AddressOfFunctions);
                    auto* funcs = reinterpret_cast<uint32_t*>(bytes.data() + funcs_off);
                    uint32_t target = 0x100;
                    if (which == "export_virtual_tail") {
                        auto* first = reinterpret_cast<PeSectionHeader*>(
                            reinterpret_cast<uint8_t*>(opt(bytes)) + coff(bytes)->SizeOfOptionalHeader);
                        first[0].VirtualSize = 1;
                        target = first[0].VirtualAddress + 0x100;
                    }
                    for (uint32_t n = 0; n < ex->NumberOfFunctions; ++n) funcs[n] = target;
                }
                break;
            }
        }
    }

    StubResolver resolver;
    const auto r = pe_load(bytes.data(), bytes.size(), resolver, options);
    bool ok = false;
    if (which == "baseline" || which == "empty_tls") ok = r.ok;
    else if (which == "bad_optional") ok = !r.ok && r.reason.find("section table") != std::string::npos;
    else if (which == "bad_tls") ok = !r.ok && r.reason.find("corrupt TLS") != std::string::npos;
    else if (which == "tls_zero_size") ok = !r.ok && r.reason.find("corrupt TLS") != std::string::npos;
    else if (which == "bad_tls_oversize" || which == "tls_overlay") ok = !r.ok && r.reason.find("corrupt TLS") != std::string::npos;
    else if (which == "bad_entry") ok = !r.ok && r.reason.find("entry point") != std::string::npos;
    else if (which == "bad_delay") ok = !r.ok && r.reason.find("delay import") != std::string::npos;
    else if (which == "import_overlay") ok = !r.ok && r.reason.find("import directory") != std::string::npos;
    else if (which == "overlap_sections") ok = !r.ok && r.reason.find("section overlap") != std::string::npos;
    else if (which == "entry_virtual_tail") ok = !r.ok && r.reason.find("entry point") != std::string::npos;
    else if (which == "bad_import_dir") ok = !r.ok && r.reason.find("import directory") != std::string::npos;
    else if (which == "import_zero_size") ok = !r.ok && r.reason.find("import directory") != std::string::npos;
    else if (which == "bad_export") ok = r.ok && r.find_export("start_mod") == nullptr;
    else if (which == "nonexec_export") ok = r.ok && r.find_export("start_mod") == nullptr;
    else if (which == "export_virtual_tail") ok = r.ok && r.find_export("start_mod") == nullptr;
    else if (which == "wrong_page_size") ok = !r.ok && r.reason.find("host page size") != std::string::npos;
    else if (which == "bad_reloc_target") ok = !r.ok && r.reason.find("relocation target") != std::string::npos;
    else if (which == "bad_reloc_odd") ok = !r.ok && r.reason.find("relocation block") != std::string::npos;
    else if (which == "empty_reloc" || which == "absolute_reloc") ok = !r.ok && r.reason.find("usable base relocation") != std::string::npos;
    else if (which == "real_tls") ok = !r.ok && r.reason.find("static TLS") != std::string::npos;
    else if (which == "missing_reloc") ok = !r.ok && r.reason.find("base relocation") != std::string::npos;
    std::printf("%s %s: ok=%d reason=%s\n", which.c_str(), ok ? "PASS" : "FAIL", r.ok, r.reason.c_str());
    return ok ? 0 : 1;
}
