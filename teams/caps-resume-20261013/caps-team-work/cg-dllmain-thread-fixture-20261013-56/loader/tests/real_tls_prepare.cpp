// Read-only host check: map and relocate an unchanged PE, then prepare TLS.
// Never resolves imports, invokes callbacks, or executes DLL code.
#include "pe_tls_registration.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using namespace cap13;

static bool fits(size_t off, size_t size, size_t limit)
{
    return off <= limit && size <= limit - off;
}

template<class T>
static bool read_struct(const std::vector<uint8_t>& data, size_t off, T& out)
{
    if (!fits(off, sizeof(T), data.size())) return false;
    std::memcpy(&out, data.data() + off, sizeof(T));
    return true;
}

static bool fail(const std::string& reason)
{
    std::printf("FAIL %s\n", reason.c_str());
    return false;
}

static int fail_exit(const std::string& reason)
{
    fail(reason);
    return 1;
}

struct PreparedImage
{
    std::vector<uint8_t> bytes;
    std::vector<PeSectionHeader> sections;
    PeOptionalHeader64 optional{};
    size_t relocations = 0;
    PeTlsDirectory64 tls{};
};

static bool map_for_tls(const char* path, PreparedImage& mapped)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return fail("source DLL unavailable");
    std::vector<uint8_t> file{std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>()};
    PeDosHeader dos{};
    if (!read_struct(file, 0, dos) || dos.e_magic != 0x5A4D || dos.e_lfanew < 0)
        return fail("invalid DOS header");
    const size_t pe_off = static_cast<size_t>(dos.e_lfanew);
    if (!fits(pe_off, 4, file.size()) ||
        std::memcmp(file.data() + pe_off, "PE\0\0", 4) != 0)
        return fail("invalid PE signature");
    PeFileHeader coff{};
    if (!read_struct(file, pe_off + 4, coff) || coff.Machine != 0x8664 ||
        coff.SizeOfOptionalHeader < sizeof(PeOptionalHeader64))
        return fail("not an x64 PE32+ image with expected headers");
    const size_t opt_off = pe_off + 4 + sizeof(PeFileHeader);
    if (!read_struct(file, opt_off, mapped.optional) ||
        mapped.optional.Magic != 0x20B ||
        mapped.optional.NumberOfRvaAndSizes <= PE_DIR_TLS ||
        !mapped.optional.SizeOfImage || mapped.optional.SizeOfImage > 512u * 1024u * 1024u)
        return fail("invalid PE optional header or image size");
    const size_t sec_off = opt_off + coff.SizeOfOptionalHeader;
    if (!fits(sec_off, size_t(coff.NumberOfSections) * sizeof(PeSectionHeader), file.size()))
        return fail("section table outside file");
    if (mapped.optional.SizeOfHeaders > file.size() ||
        mapped.optional.SizeOfHeaders > mapped.optional.SizeOfImage)
        return fail("headers outside file or image");
    mapped.bytes.resize(mapped.optional.SizeOfImage, 0);
    std::memcpy(mapped.bytes.data(), file.data(), mapped.optional.SizeOfHeaders);
    for (uint16_t i = 0; i < coff.NumberOfSections; ++i)
    {
        PeSectionHeader section{};
        if (!read_struct(file, sec_off + size_t(i) * sizeof(section), section) ||
            !fits(section.PointerToRawData, section.SizeOfRawData, file.size()) ||
            !fits(section.VirtualAddress, section.SizeOfRawData, mapped.bytes.size()))
            return fail("section raw range outside file or mapped image");
        mapped.sections.push_back(section);
        if (section.SizeOfRawData)
            std::memcpy(mapped.bytes.data() + section.VirtualAddress,
                        file.data() + section.PointerToRawData, section.SizeOfRawData);
    }

    const auto reloc = mapped.optional.DataDirectory[PE_DIR_BASERELOC];
    if (!reloc.VirtualAddress || !reloc.Size ||
        !fits(reloc.VirtualAddress, reloc.Size, mapped.bytes.size()))
        return fail("usable base relocation directory required");
    const uint64_t slide = reinterpret_cast<uint64_t>(mapped.bytes.data()) -
                           mapped.optional.ImageBase;
    size_t walked = 0;
    while (walked < reloc.Size)
    {
        PeRelocBlock block{};
        if (!fits(walked, sizeof(block), reloc.Size) ||
            !read_struct(mapped.bytes, size_t(reloc.VirtualAddress) + walked, block) ||
            block.BlockSize < sizeof(block) ||
            !fits(walked, block.BlockSize, reloc.Size) ||
            (block.BlockSize - sizeof(block)) % sizeof(uint16_t))
            return fail("malformed base relocation block");
        const size_t count = (block.BlockSize - sizeof(block)) / sizeof(uint16_t);
        for (size_t i = 0; i < count; ++i)
        {
            uint16_t entry = 0;
            if (!read_struct(mapped.bytes, size_t(reloc.VirtualAddress) + walked +
                             sizeof(block) + i * sizeof(entry), entry))
                return fail("relocation entry outside mapped image");
            const uint16_t type = entry >> 12;
            const uint64_t at = uint64_t(block.PageRva) + (entry & 0x0FFF);
            if (type == PE_REL_ABSOLUTE) continue;
            if (type == PE_REL_DIR64)
            {
                uint64_t value = 0;
                if (!fits(at, sizeof(value), mapped.bytes.size()))
                    return fail("DIR64 relocation target outside image");
                std::memcpy(&value, mapped.bytes.data() + at, sizeof(value));
                value += slide;
                std::memcpy(mapped.bytes.data() + at, &value, sizeof(value));
            }
            else if (type == PE_REL_HIGHLOW)
            {
                uint32_t value = 0;
                if (!fits(at, sizeof(value), mapped.bytes.size()))
                    return fail("HIGHLOW relocation target outside image");
                std::memcpy(&value, mapped.bytes.data() + at, sizeof(value));
                value += static_cast<uint32_t>(slide);
                std::memcpy(mapped.bytes.data() + at, &value, sizeof(value));
            }
            else return fail("unsupported base relocation type");
            ++mapped.relocations;
        }
        walked += block.BlockSize;
    }
    const auto tls_dir = mapped.optional.DataDirectory[PE_DIR_TLS];
    if (!tls_dir.VirtualAddress || tls_dir.Size < sizeof(PeTlsDirectory64) ||
        !read_struct(mapped.bytes, tls_dir.VirtualAddress, mapped.tls))
        return fail("TLS directory missing or outside mapped image");
    return true;
}

static bool executable_callback(const PreparedImage& mapped, uint64_t callback)
{
    const uint64_t base = reinterpret_cast<uint64_t>(mapped.bytes.data());
    if (callback < base) return false;
    const uint64_t rva = callback - base;
    for (const auto& section : mapped.sections)
    {
        const uint64_t size = section.VirtualSize && section.VirtualSize < section.SizeOfRawData ?
                              section.VirtualSize : section.SizeOfRawData;
        if ((section.Characteristics & PE_SCN_MEM_EXECUTE) &&
            rva >= section.VirtualAddress && rva - section.VirtualAddress < size)
            return true;
    }
    return false;
}

static bool executable_callback_adapter(uint64_t callback, void* user)
{
    return executable_callback(*static_cast<PreparedImage*>(user), callback);
}

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const size_t expected_callbacks = static_cast<size_t>(std::stoul(argv[2]));
    PreparedImage mapped;
    if (!map_for_tls(argv[1], mapped)) return 1;
    StaticTlsBank bank;
    auto registration = register_mapped_static_tls(
        bank, mapped.bytes.data(), mapped.bytes.size(), mapped.tls,
        executable_callback_adapter, &mapped);
    if (!registration.ok) return fail_exit(registration.reason);
    uint32_t written_index = UINT32_MAX;
    const size_t index_off = mapped.tls.AddressOfIndex -
                             reinterpret_cast<uint64_t>(mapped.bytes.data());
    std::memcpy(&written_index, mapped.bytes.data() + index_off, sizeof(written_index));
    if (written_index != registration.index) return fail_exit("TLS index was not written");
    if (registration.callbacks.size() != expected_callbacks)
        return fail_exit("unexpected callback count");
    for (uint64_t callback : registration.callbacks)
        if (!executable_callback(mapped, callback))
            return fail_exit("callback target is not file-backed executable code");

    auto main_view = bank.attach_current_thread();
    auto* main_block = bank.current_block(registration.index);
    const uint64_t base = reinterpret_cast<uint64_t>(mapped.bytes.data());
    const size_t start = mapped.tls.StartAddressOfRawData - base;
    const size_t initialized = mapped.tls.EndAddressOfRawData - mapped.tls.StartAddressOfRawData;
    if (!main_block || main_view.slots.size() != 1 ||
        main_view.slots[registration.index] != main_block ||
        std::memcmp(main_block, mapped.bytes.data() + start, initialized) != 0)
        return fail_exit("main-thread TLS template mismatch");
    for (size_t i = 0; i < mapped.tls.SizeOfZeroFill; ++i)
        if (main_block[initialized + i]) return fail_exit("TLS zero fill mismatch");
    bool worker_ok = false;
    std::thread worker([&] {
        auto worker_view = bank.attach_current_thread();
        auto* worker_block = bank.current_block(registration.index);
        worker_ok = worker_view.slots.size() == 1 && worker_block &&
                    worker_block != main_block &&
                    std::memcmp(worker_block, main_block, initialized) == 0;
    });
    worker.join();
    if (!worker_ok) return fail_exit("worker-thread TLS block mismatch");
    std::printf("PASS mapped static TLS: initialized=%zu zero-fill=%u callbacks=%zu relocations=%zu index=%u\n",
                initialized, mapped.tls.SizeOfZeroFill, registration.callbacks.size(),
                mapped.relocations, registration.index);
    return 0;
}
