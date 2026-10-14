#include "pe_import_scan.h"
#include "pe_types.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace cap13 {
namespace {

bool fits(size_t offset, size_t length, size_t size)
{
    return offset <= size && length <= size - offset;
}

struct RawImage
{
    const uint8_t* data;
    size_t size;
    const PeOptionalHeader64* optional;
    const PeSectionHeader* sections;
    uint16_t section_count;

    // Only file-backed bytes are readable. Virtual zero-fill is not accepted
    // as import metadata, even when its RVA lies inside SizeOfImage.
    bool translate(uint32_t rva, size_t length, size_t* offset,
                   size_t* available = nullptr) const
    {
        if (rva < optional->SizeOfHeaders)
        {
            if (!fits(rva, length, optional->SizeOfHeaders) ||
                !fits(rva, length, size)) return false;
            *offset = rva;
            if (available) *available = std::min(size, size_t(optional->SizeOfHeaders)) - rva;
            return true;
        }
        for (uint16_t i = 0; i < section_count; ++i)
        {
            const auto& section = sections[i];
            if (rva < section.VirtualAddress) continue;
            const uint64_t delta = uint64_t(rva) - section.VirtualAddress;
            if (delta > section.SizeOfRawData ||
                length > uint64_t(section.SizeOfRawData) - delta) continue;
            const uint64_t raw = uint64_t(section.PointerToRawData) + delta;
            if (raw > size || length > size - size_t(raw)) return false;
            *offset = size_t(raw);
            if (available) *available = section.SizeOfRawData - size_t(delta);
            return true;
        }
        return false;
    }
};

PeImportScanResult refuse(const char* why)
{
    PeImportScanResult result;
    result.reason = why;
    return result;
}

} // namespace

PeImportScanResult pe_scan_imported_dlls(const void* file_bytes, size_t file_size)
{
    if (!file_bytes || file_size < sizeof(PeDosHeader))
        return refuse("PE import scan: missing or short DOS header");
    const auto* data = static_cast<const uint8_t*>(file_bytes);
    PeDosHeader dos{};
    std::memcpy(&dos, data, sizeof(dos));
    if (dos.e_magic != 0x5a4d || dos.e_lfanew < 0)
        return refuse("PE import scan: invalid DOS header");
    const size_t pe = size_t(dos.e_lfanew);
    if (!fits(pe, 4 + sizeof(PeFileHeader), file_size) ||
        std::memcmp(data + pe, "PE\0\0", 4) != 0)
        return refuse("PE import scan: invalid NT header");
    PeFileHeader coff{};
    std::memcpy(&coff, data + pe + 4, sizeof(coff));
    const size_t optional_offset = pe + 4 + sizeof(coff);
    if (coff.Machine != 0x8664 ||
        coff.SizeOfOptionalHeader < sizeof(PeOptionalHeader64) ||
        !fits(optional_offset, coff.SizeOfOptionalHeader, file_size))
        return refuse("PE import scan: unsupported or truncated PE32+ header");
    const auto* optional = reinterpret_cast<const PeOptionalHeader64*>(data + optional_offset);
    if (optional->Magic != 0x020b || optional->NumberOfRvaAndSizes <= PE_DIR_IMPORT ||
        optional->SizeOfImage == 0 || optional->SizeOfImage > 512u * 1024u * 1024u ||
        optional->SizeOfHeaders > optional->SizeOfImage ||
        optional->SizeOfHeaders > file_size)
        return refuse("PE import scan: invalid PE32+ image bounds");
    if (coff.NumberOfSections == 0 || coff.NumberOfSections > 96)
        return refuse("PE import scan: invalid section count");
    const size_t section_offset = optional_offset + coff.SizeOfOptionalHeader;
    const size_t section_bytes = size_t(coff.NumberOfSections) * sizeof(PeSectionHeader);
    if (!fits(section_offset, section_bytes, file_size) ||
        !fits(section_offset, section_bytes, optional->SizeOfHeaders))
        return refuse("PE import scan: section table outside headers");
    RawImage image{data, file_size, optional,
        reinterpret_cast<const PeSectionHeader*>(data + section_offset),
        coff.NumberOfSections};
    for (uint16_t i = 0; i < image.section_count; ++i)
    {
        const auto& section = image.sections[i];
        const uint64_t mapped = std::max(section.VirtualSize, section.SizeOfRawData);
        if (uint64_t(section.VirtualAddress) + mapped > optional->SizeOfImage ||
            uint64_t(section.PointerToRawData) + section.SizeOfRawData > file_size ||
            (mapped && section.VirtualAddress < optional->SizeOfHeaders))
            return refuse("PE import scan: section range outside image or file");
        for (uint16_t j = 0; j < i; ++j)
        {
            const auto& previous = image.sections[j];
            const uint64_t previous_size = std::max(previous.VirtualSize, previous.SizeOfRawData);
            if (mapped && previous_size &&
                uint64_t(section.VirtualAddress) < uint64_t(previous.VirtualAddress) + previous_size &&
                uint64_t(previous.VirtualAddress) < uint64_t(section.VirtualAddress) + mapped)
                return refuse("PE import scan: overlapping image sections");
        }
    }
    if (optional->NumberOfRvaAndSizes > PE_DIR_DELAYIMPORT)
    {
        const auto& delayed = optional->DataDirectory[PE_DIR_DELAYIMPORT];
        if (delayed.VirtualAddress || delayed.Size)
            return refuse("PE import scan: delay imports are unsupported");
    }
    const auto& imports = optional->DataDirectory[PE_DIR_IMPORT];
    if (!imports.VirtualAddress && !imports.Size)
    {
        PeImportScanResult result;
        result.ok = true;
        return result;
    }
    if (!imports.VirtualAddress || imports.Size < sizeof(PeImportDescriptor) ||
        imports.Size > 1024u * 1024u)
        return refuse("PE import scan: malformed import directory size");
    size_t directory_offset = 0;
    if (!image.translate(imports.VirtualAddress, imports.Size, &directory_offset))
        return refuse("PE import scan: import directory outside file-backed image");
    PeImportScanResult result;
    const size_t count = imports.Size / sizeof(PeImportDescriptor);
    bool terminated = false;
    for (size_t i = 0; i < count; ++i)
    {
        PeImportDescriptor descriptor{};
        std::memcpy(&descriptor, data + directory_offset + i * sizeof(descriptor),
                    sizeof(descriptor));
        if (!descriptor.OriginalFirstThunk && !descriptor.TimeDateStamp &&
            !descriptor.ForwarderChain && !descriptor.Name && !descriptor.FirstThunk)
        {
            terminated = true;
            break;
        }
        size_t name_offset = 0;
        size_t available = 0;
        if (!descriptor.Name ||
            !image.translate(descriptor.Name, 1, &name_offset, &available))
            return refuse("PE import scan: DLL name RVA outside file-backed image");
        const size_t cap = std::min<size_t>(available, 4096);
        const void* end = std::memchr(data + name_offset, 0, cap);
        if (!end || end == data + name_offset)
            return refuse("PE import scan: unterminated or empty DLL name");
        const size_t length = static_cast<const uint8_t*>(end) - (data + name_offset);
        for (size_t n = 0; n < length; ++n)
            if (data[name_offset + n] < 0x20 || data[name_offset + n] >= 0x7f)
                return refuse("PE import scan: non-ASCII DLL name");
        result.dll_names.emplace_back(reinterpret_cast<const char*>(data + name_offset), length);
    }
    if (!terminated) return refuse("PE import scan: import descriptors lack terminator");
    result.ok = true;
    return result;
}

} // namespace cap13
