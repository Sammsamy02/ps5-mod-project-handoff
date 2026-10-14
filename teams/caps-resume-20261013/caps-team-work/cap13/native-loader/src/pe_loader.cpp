// pe_loader.cpp -- minimal PE32+ mapper implementation (see pe_loader.h).
//
// Order of operations (design section 3): parse headers -> refusal checks
// (CLR, static TLS, RWX partition) -> allocate -> copy sections -> apply
// base relocations -> resolve imports into the IAT -> apply page
// permissions. The caller then invokes the entry point through the win64
// bridge (dllmain_invoker.cpp).
//
// Memory primitives live in cap13::Os. On PS5 (__PROSPERO__) these compile
// against the sdk's mmap/mprotect surface; on macOS they are plain POSIX,
// which is what makes the M0 unit tests real without a console.
//
// CAP13 proposal-only. Design-grade: syntax-checked on macOS clang;
// behavior validated only by the planned M0 unit tests.

#include "pe_loader.h"

#include <cstdio>
#include <cstring>

#include "pe_types.h"

#if defined(__APPLE__) || defined(__unix__) || defined(__PROSPERO__)
#include <sys/mman.h>
#define CAP13_HAS_MMAP 1
#else
#define CAP13_HAS_MMAP 0
#endif

namespace cap13 {

namespace Os
{
    void* alloc_rw(size_t size)
    {
#if CAP13_HAS_MMAP
        void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return p == MAP_FAILED ? nullptr : p;
#else
        (void)size;
        return nullptr;
#endif
    }

    // Apply 'prot' to [addr, addr+size) rounded outward to page bounds.
    bool protect(void* addr, size_t size, int prot, uint32_t page_size)
    {
#if CAP13_HAS_MMAP
        const uintptr_t begin = reinterpret_cast<uintptr_t>(addr) & ~uintptr_t(page_size - 1);
        const uintptr_t end = (reinterpret_cast<uintptr_t>(addr) + size + page_size - 1) & ~uintptr_t(page_size - 1);
        return mprotect(reinterpret_cast<void*>(begin), end - begin, prot) == 0;
#else
        (void)addr;
        (void)size;
        (void)prot;
        (void)page_size;
        return true;
#endif
    }
} // namespace Os

namespace {

struct Impl
{
    const uint8_t* file = nullptr;
    size_t file_size = 0;
    const PeFileHeader* coff = nullptr;
    const PeOptionalHeader64* opt = nullptr;
    const PeSectionHeader* sections = nullptr;
    uint8_t* image = nullptr;   // mapped copy; retained for process lifetime
    uint64_t image_bytes = 0;
    uint64_t slide = 0;
    uint32_t export_dir_rva = 0;
    uint32_t export_dir_size = 0;
    LoadOptions opts;
};

const char kReasonNotPe[] = "not a PE image (no MZ/PE signature)";
const char kReasonNotX64[] = "PE is not x86_64 (Machine != 0x8664); ARM64 PEs unsupported";
const char kReasonTooSmall[] = "truncated PE (headers exceed file size)";

bool rva_to_file_offset(const Impl& s, uint32_t rva, size_t read_bytes, uint32_t& out_off)
{
    if (rva < s.opt->SizeOfHeaders)
    {
        if (size_t(rva) + read_bytes > s.file_size) return false;
        out_off = rva;
        return true;
    }
    for (uint16_t i = 0; i < s.coff->NumberOfSections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        if (rva >= sec.VirtualAddress && rva - sec.VirtualAddress < sec.SizeOfRawData)
        {
            const uint32_t off = sec.PointerToRawData + (rva - sec.VirtualAddress);
            if (size_t(off) + read_bytes > s.file_size) return false;
            out_off = off;
            return true;
        }
    }
    return false;
}

// The 16 KiB permission partition (design 3b): union section permissions per
// page; refuse any page whose union needs WRITE and EXEC simultaneously
// unless the J1 probe has authorized RWX partitions.
bool partition_ok(const Impl& s, std::string& reason)
{
    const uint32_t page = s.opts.page_size;
    for (uint64_t base = 0; base < s.opt->SizeOfImage; base += page)
    {
        bool exec = false, write = false;
        for (uint16_t i = 0; i < s.coff->NumberOfSections; ++i)
        {
            const PeSectionHeader& sec = s.sections[i];
            const uint64_t sec_end = sec.VirtualAddress + (sec.VirtualSize ? sec.VirtualSize : sec.SizeOfRawData);
            if (base + page <= sec.VirtualAddress || sec_end <= base) continue;
            if (sec.Characteristics & PE_SCN_MEM_EXECUTE) exec = true;
            if (sec.Characteristics & PE_SCN_MEM_WRITE) write = true;
        }
        if (exec && write && !s.opts.allow_rwx_partition)
        {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "16KiB page at RVA 0x%llx would need RWX; J1 exec-memory probe has not authorized RWX partitions",
                          (unsigned long long)base);
            reason = buf;
            return false;
        }
    }
    return true;
}

LoadResult fail(LoadResult& r, std::string reason)
{
    r.ok = false;
    r.reason = std::move(reason);
    return r;
}

} // namespace

LoadResult pe_load(const void* file_bytes, size_t file_size, ImportResolver& resolver,
                   const LoadOptions& options)
{
    LoadResult out;
    Impl s;
    s.file = static_cast<const uint8_t*>(file_bytes);
    s.file_size = file_size;
    s.opts = options;

    // ---- header parsing ----
    if (file_size < sizeof(PeDosHeader) + 4 + sizeof(PeFileHeader)) return fail(out, kReasonTooSmall);
    const PeDosHeader* dos = reinterpret_cast<const PeDosHeader*>(s.file);
    if (dos->e_magic != 0x5A4D) return fail(out, kReasonNotPe);
    if (dos->e_lfanew < 0 || size_t(dos->e_lfanew) + 4 + sizeof(PeFileHeader) > file_size)
        return fail(out, kReasonTooSmall);
    if (std::memcmp(s.file + dos->e_lfanew, "PE\0\0", 4) != 0) return fail(out, kReasonNotPe);
    s.coff = reinterpret_cast<const PeFileHeader*>(s.file + dos->e_lfanew + 4);
    if (s.coff->Machine != 0x8664) return fail(out, kReasonNotX64);
    if (s.coff->SizeOfOptionalHeader < sizeof(PeOptionalHeader64)) return fail(out, kReasonTooSmall);
    const uint8_t* opt_ptr = reinterpret_cast<const uint8_t*>(s.coff) + sizeof(PeFileHeader);
    if (size_t(opt_ptr - s.file) + sizeof(PeOptionalHeader64) > file_size) return fail(out, kReasonTooSmall);
    s.opt = reinterpret_cast<const PeOptionalHeader64*>(opt_ptr);
    if (s.opt->Magic != 0x020B) return fail(out, "not PE32+ (OptionalHeader Magic != 0x020B)");
    if (s.opt->NumberOfRvaAndSizes <= PE_DIR_TLS) return fail(out, "NumberOfRvaAndSizes < 10; PE too old for this loader");
    if (s.opt->SizeOfImage == 0) return fail(out, "implausible SizeOfImage");
    if (s.opt->SizeOfHeaders > s.opt->SizeOfImage) return fail(out, "SizeOfHeaders exceeds SizeOfImage");
    const uint16_t num_sections = s.coff->NumberOfSections;
    if (num_sections == 0 || num_sections > 96) return fail(out, "implausible section count");
    if (size_t(opt_ptr - s.file) + sizeof(PeOptionalHeader64) + size_t(num_sections) * sizeof(PeSectionHeader) > file_size)
        return fail(out, kReasonTooSmall);
    s.sections = reinterpret_cast<const PeSectionHeader*>(opt_ptr + s.coff->SizeOfOptionalHeader);

    // ---- zero-cost refusals (before any allocation) ----
    if (s.opt->DataDirectory[14].Size != 0) // PE_DIR_COM (CLR .NET runtime header)
        return fail(out, "PE carries a CLR/COM descriptor; managed images belong to capability #10");
    if (s.opt->DataDirectory[PE_DIR_TLS].Size != 0)
        return fail(out, "PE has a static TLS directory; TEB/gs thread-local access is not "
                         "emulatable in userspace on this target (design 3e) -- refused");

    std::string reason;
    if (!partition_ok(s, reason)) return fail(out, std::move(reason));

    // ---- allocate + copy (design 3b) ----
#if !CAP13_HAS_MMAP
    return fail(out, "host lacks mmap; this build targets PS5/macOS only");
#else
    const uint32_t page = s.opts.page_size;
    const uint64_t rounded = (s.opt->SizeOfImage + page - 1) / page * page;
    uint8_t* image = static_cast<uint8_t*>(Os::alloc_rw(rounded));
    if (!image) return fail(out, "anonymous allocation for PE image failed");
    std::memset(image, 0, rounded);

    const uint32_t headers = s.opt->SizeOfHeaders;
    if (headers > rounded) return fail(out, "SizeOfHeaders exceeds mapped region");
    std::memcpy(image, s.file, headers);
    for (uint16_t i = 0; i < num_sections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        if (uint64_t(sec.VirtualAddress) + sec.SizeOfRawData > rounded)
            return fail(out, "section exceeds SizeOfImage");
        if (uint64_t(sec.PointerToRawData) + sec.SizeOfRawData > file_size)
            return fail(out, "section raw data exceeds file size");
        std::memcpy(image + sec.VirtualAddress, s.file + sec.PointerToRawData, sec.SizeOfRawData);
        // VirtualSize beyond SizeOfRawData stays zero-filled (BSS-like).
    }

    s.image = image;
    s.image_bytes = rounded;
    s.slide = uint64_t(image) - s.opt->ImageBase;
    out.image = image;
    out.image_size = rounded;
    out.preferred_base = s.opt->ImageBase;
    out.slide = s.slide;
    out.entry_rva = s.opt->AddressOfEntryPoint;

    // ---- base relocations (design 3c) ----
    if (s.slide != 0 && s.opt->DataDirectory[PE_DIR_BASERELOC].Size != 0)
    {
        const uint32_t dir_rva = s.opt->DataDirectory[PE_DIR_BASERELOC].VirtualAddress;
        const uint32_t dir_size = s.opt->DataDirectory[PE_DIR_BASERELOC].Size;
        if (uint64_t(dir_rva) + dir_size > rounded) return fail(out, "base relocation directory exceeds image");
        uint32_t walked = 0;
        while (walked + sizeof(PeRelocBlock) <= dir_size)
        {
            const PeRelocBlock* block = reinterpret_cast<const PeRelocBlock*>(image + dir_rva + walked);
            if (block->BlockSize < sizeof(PeRelocBlock) || walked + block->BlockSize > dir_size)
                return fail(out, "corrupt base relocation block");
            const size_t count = (block->BlockSize - sizeof(PeRelocBlock)) / sizeof(PeRelocEntry);
            const PeRelocEntry* entries = reinterpret_cast<const PeRelocEntry*>(block + 1);
            for (size_t e = 0; e < count; ++e)
            {
                uint8_t* at = image + block->PageRva + entries[e].Offset;
                if (at + 8 > image + rounded) return fail(out, "relocation target outside image");
                switch (entries[e].Type)
                {
                    case PE_REL_ABSOLUTE:
                        break; // padding
                    case PE_REL_DIR64:
                        *reinterpret_cast<uint64_t*>(at) += s.slide;
                        break;
                    case PE_REL_HIGHLOW:
                        *reinterpret_cast<uint32_t*>(at) += uint32_t(s.slide);
                        break;
                    default:
                        return fail(out, "unsupported base relocation type (not DIR64/HIGHLOW)");
                }
            }
            walked += block->BlockSize;
        }
    }

    // ---- imports (design 3d): resolve all, then refuse listing every miss ----
    if (s.opt->DataDirectory[PE_DIR_IMPORT].Size != 0)
    {
        const uint32_t dir_rva = s.opt->DataDirectory[PE_DIR_IMPORT].VirtualAddress;
        for (uint32_t idx = 0;; ++idx)
        {
            uint32_t off = 0;
            if (!rva_to_file_offset(s, dir_rva + idx * sizeof(PeImportDescriptor), sizeof(PeImportDescriptor), off))
                return fail(out, "corrupt import directory (descriptor out of range)");
            const PeImportDescriptor d = *reinterpret_cast<const PeImportDescriptor*>(s.file + off);
            if (d.FirstThunk == 0 && d.Name == 0) break; // null terminator
            if (d.FirstThunk >= s.opt->SizeOfImage) return fail(out, "corrupt import descriptor (IAT out of range)");

            uint32_t name_off = 0;
            if (!rva_to_file_offset(s, d.Name, 1, name_off)) return fail(out, "corrupt import descriptor name");
            const char* dll = reinterpret_cast<const char*>(s.file + name_off);
            const size_t dll_cap = file_size - name_off;

            // Prefer the INT (OriginalFirstThunk) for names; write resolutions
            // into the IAT (FirstThunk) in the mapped image.
            const uint32_t thunk_rva = d.OriginalFirstThunk ? d.OriginalFirstThunk : d.FirstThunk;
            for (uint32_t t = 0;; ++t)
            {
                uint32_t toff = 0;
                if (!rva_to_file_offset(s, thunk_rva + t * 8, 8, toff))
                    return fail(out, "corrupt import thunk table");
                const uint64_t thunk = *reinterpret_cast<const uint64_t*>(s.file + toff);
                if (thunk == 0) break; // end of this DLL's thunk array

                void* resolved = nullptr;
                char symbol[256];
                uint64_t ordinal = 0;
                if (thunk & PE_IMPORT_BY_ORDINAL)
                {
                    ordinal = thunk & 0xFFFFFFFFFFFFull;
                }
                else
                {
                    uint32_t soff = 0;
                    if (!rva_to_file_offset(s, uint32_t(thunk), 3, soff))
                        return fail(out, "corrupt import name entry");
                    const char* nm = reinterpret_cast<const char*>(s.file + soff + 2); // skip Hint
                    const size_t cap = file_size - (soff + 2);
                    size_t len = 0;
                    while (len < cap && len < sizeof(symbol) - 1 && nm[len] != '\0') ++len;
                    if (len >= sizeof(symbol) - 1) return fail(out, "import symbol name too long");
                    std::memcpy(symbol, nm, len);
                    symbol[len] = '\0';
                }
                resolved = resolver.resolve(dll, ordinal ? nullptr : symbol, ordinal);
                if (!resolved)
                {
                    char row[300];
                    std::snprintf(row, sizeof(row), "%.*s!%s", int(dll_cap < 96 ? dll_cap : 96), dll,
                                  ordinal ? "<ordinal>" : symbol);
                    out.missing.push_back(row);
                }
                else
                {
                    *reinterpret_cast<uint64_t*>(image + d.FirstThunk + t * 8) = uint64_t(resolved);
                }
            }
        }
        if (!out.missing.empty())
            return fail(out, "unresolved imports (fail-closed; no null-filled IAT)");
    }

    // ---- page permissions (design 3b) ----
    if (!Os::protect(image, s.opt->SizeOfHeaders, PROT_READ, page))
        return fail(out, "mprotect on PE headers failed");
    for (uint64_t base = 0; base < s.opt->SizeOfImage; base += page)
    {
        bool exec = false, write = false;
        for (uint16_t i = 0; i < num_sections; ++i)
        {
            const PeSectionHeader& sec = s.sections[i];
            const uint64_t sec_end = sec.VirtualAddress + (sec.VirtualSize ? sec.VirtualSize : sec.SizeOfRawData);
            if (base + page <= sec.VirtualAddress || sec_end <= base) continue;
            if (sec.Characteristics & PE_SCN_MEM_EXECUTE) exec = true;
            if (sec.Characteristics & PE_SCN_MEM_WRITE) write = true;
        }
        int prot = PROT_READ;
        if (write) prot |= PROT_WRITE;
        if (exec) prot |= PROT_EXEC;
        if (!Os::protect(image + base, page, prot, page))
            return fail(out, "mprotect on PE section page failed");
    }

    // ---- publish state (retained: the Impl and image are never freed; the
    // image stays mapped for the process lifetime, matching the #9 host's
    // retained-module law) ----
    LoadResult::Impl* kept = new LoadResult::Impl();
    kept->image = image;
    kept->export_dir_rva = s.opt->DataDirectory[PE_DIR_EXPORT].VirtualAddress;
    kept->export_dir_size = s.opt->DataDirectory[PE_DIR_EXPORT].Size;
    out.ok = true;
    out.impl = kept;
    return out;
#endif
}

void* LoadResult::find_export(const char* name) const
{
    if (!ok || !impl || impl->export_dir_size == 0 || !name) return nullptr;
    uint8_t* img = impl->image;
    const PeExportDirectory& ex = *reinterpret_cast<const PeExportDirectory*>(img + impl->export_dir_rva);
    const uint32_t* names = reinterpret_cast<const uint32_t*>(img + ex.AddressOfNames);
    const uint16_t* ords = reinterpret_cast<const uint16_t*>(img + ex.AddressOfNameOrdinals);
    const uint32_t* funcs = reinterpret_cast<const uint32_t*>(img + ex.AddressOfFunctions);
    for (uint32_t i = 0; i < ex.NumberOfNames; ++i)
    {
        const char* n = reinterpret_cast<const char*>(img + names[i]);
        if (std::strcmp(n, name) != 0) continue;
        const uint32_t fn_rva = funcs[ords[i]];
        // Forwarded exports point inside the export directory itself; this
        // loader does not follow forwarders and reports them as unresolved.
        if (fn_rva >= impl->export_dir_rva && fn_rva < impl->export_dir_rva + impl->export_dir_size) return nullptr;
        return img + fn_rva;
    }
    return nullptr;
}

namespace {
bool opt_dir_nonzero(const void* file_bytes, size_t file_size, uint32_t index)
{
    if (!file_bytes || file_size < sizeof(PeDosHeader) + 4 + sizeof(PeFileHeader) + sizeof(PeOptionalHeader64))
        return false;
    const PeDosHeader* dos = reinterpret_cast<const PeDosHeader*>(file_bytes);
    if (dos->e_magic != 0x5A4D) return false;
    const auto* coff = reinterpret_cast<const PeFileHeader*>(static_cast<const uint8_t*>(file_bytes) + dos->e_lfanew + 4);
    if (coff->Machine != 0x8664 || coff->SizeOfOptionalHeader < sizeof(PeOptionalHeader64)) return false;
    const auto* opt = reinterpret_cast<const PeOptionalHeader64*>(reinterpret_cast<const uint8_t*>(coff) + sizeof(PeFileHeader));
    if (opt->Magic != 0x020B || opt->NumberOfRvaAndSizes <= index) return false;
    return opt->DataDirectory[index].Size != 0;
}
} // namespace

bool pe_has_tls_directory(const void* file_bytes, size_t file_size)
{
    return opt_dir_nonzero(file_bytes, file_size, PE_DIR_TLS);
}

bool pe_has_com_descriptor(const void* file_bytes, size_t file_size)
{
    return opt_dir_nonzero(file_bytes, file_size, 14); // CLR runtime header dir
}

} // namespace cap13
