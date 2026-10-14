// pe_loader.cpp -- minimal PE32+ mapper implementation (see pe_loader.h).
//
// Order of operations (design section 3): parse headers -> refusal checks
// (CLR, RWX partition, and unguarded static TLS/GS) -> allocate -> copy
// sections -> apply base relocations -> resolve imports into the IAT ->
// guarded preparation if requested -> apply page permissions. The public
// pe_load result remains strict; guest_pe_loader owns the guarded route.
//
// Memory primitives live in cap13::Os. On PS5 (__PROSPERO__) these compile
// against the sdk's mmap/mprotect surface; on macOS they are plain POSIX,
// which is what makes the M0 unit tests real without a console.
//
// CAP13 proposal-only. Host tests do not prove PS5 segment or memory behavior.

#include "pe_loader.h"
#include "pe_loader_internal.h"

#include <cstdio>
#include <cstring>

#include "pe_types.h"

#if defined(__APPLE__) || defined(__unix__) || defined(__PROSPERO__)
#include <sys/mman.h>
#include <unistd.h>
#define CAP13_HAS_MMAP 1
#else
#define CAP13_HAS_MMAP 0
#endif

namespace cap13 {

namespace {
#if CAP13_HAS_MMAP
#if defined(MAP_ANONYMOUS)
constexpr int kAnonymousMapFlag = MAP_ANONYMOUS;
#elif defined(MAP_ANON)
constexpr int kAnonymousMapFlag = MAP_ANON;
#elif defined(__PROSPERO__)
// ps5-payload-sdk v0.43 sys/mman.h defines MAP_ANON as 0x1000 only
// when __BSD_VISIBLE; strict _XOPEN_SOURCE builds hide both spellings.
constexpr int kAnonymousMapFlag = 0x1000;
#else
#error "anonymous mmap flag is unavailable on this POSIX target"
#endif
#endif
} // namespace

namespace Os
{
    void* alloc_rw(size_t size)
    {
#if CAP13_HAS_MMAP
        void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | kAnonymousMapFlag, -1, 0);
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

    void release(void* addr, size_t size)
    {
#if CAP13_HAS_MMAP
        if (addr && size) munmap(addr, size);
#else
        (void)addr;
        (void)size;
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
        if (read_bytes > s.opt->SizeOfHeaders - rva ||
            rva > s.file_size || read_bytes > s.file_size - rva) return false;
        out_off = rva;
        return true;
    }
    for (uint16_t i = 0; i < s.coff->NumberOfSections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        if (rva >= sec.VirtualAddress && rva - sec.VirtualAddress < sec.SizeOfRawData)
        {
            const uint32_t into = rva - sec.VirtualAddress;
            const uint64_t off = uint64_t(sec.PointerToRawData) + into;
            if (read_bytes > sec.SizeOfRawData - into ||
                off > s.file_size || read_bytes > s.file_size - off || off > UINT32_MAX) return false;
            out_off = uint32_t(off);
            return true;
        }
    }
    return false;
}

size_t rva_file_span(const Impl& s, uint32_t rva)
{
    uint32_t off = 0;
    if (!rva_to_file_offset(s, rva, 1, off)) return 0;
    if (rva < s.opt->SizeOfHeaders) return s.opt->SizeOfHeaders - rva;
    for (uint16_t i = 0; i < s.coff->NumberOfSections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        if (rva >= sec.VirtualAddress && rva - sec.VirtualAddress < sec.SizeOfRawData)
            return sec.SizeOfRawData - (rva - sec.VirtualAddress);
    }
    return 0;
}

bool image_range(const Impl& s, uint64_t rva, uint64_t bytes)
{
    return rva <= s.opt->SizeOfImage && bytes <= uint64_t(s.opt->SizeOfImage) - rva;
}

bool executable_rva(const Impl& s, uint32_t rva)
{
    if (!image_range(s, rva, 1)) return false;
    for (uint16_t i = 0; i < s.coff->NumberOfSections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        // Callable code must have bytes in the file, not only zero-filled
        // executable virtual tail space.
        const uint32_t exec_bytes = sec.VirtualSize && sec.VirtualSize < sec.SizeOfRawData ?
                                    sec.VirtualSize : sec.SizeOfRawData;
        const uint64_t end = uint64_t(sec.VirtualAddress) + exec_bytes;
        if ((sec.Characteristics & PE_SCN_MEM_EXECUTE) &&
            rva >= sec.VirtualAddress && uint64_t(rva) < end) return true;
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
    // Successful modules stay mapped for the process lifetime. A failed
    // preflight or incomplete map does not retain untrusted allocation.
    Os::release(r.image, r.image_size);
    r.image = nullptr;
    r.image_size = 0;
    r.ok = false;
    r.reason = std::move(reason);
    return r;
}

} // namespace

LoadResult pe_load_internal(const void* file_bytes, size_t file_size,
                            ImportResolver& resolver, const LoadOptions& options,
                            bool guest_segment_guarded, PePrepareHook prepare,
                            void* prepare_user)
{
    LoadResult out;
    Impl s;
    s.file = static_cast<const uint8_t*>(file_bytes);
    s.file_size = file_size;
    s.opts = options;
    if (guest_segment_guarded && !prepare)
        return fail(out, "guest-segment mapping requires a preparation hook");

    // ---- header parsing ----
    if (!s.file) return fail(out, kReasonTooSmall);
    if (s.opts.page_size < 0x1000 || (s.opts.page_size & (s.opts.page_size - 1)) != 0)
        return fail(out, "invalid page size");
#if defined(__APPLE__)
    const long host_page = sysconf(_SC_PAGESIZE);
    if (host_page <= 0 || uint64_t(host_page) != s.opts.page_size)
        return fail(out, "configured page size differs from host page size");
#endif
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
    if (s.opt->SizeOfImage > 512u * 1024u * 1024u) return fail(out, "SizeOfImage exceeds loader cap");
    if (s.opt->SizeOfHeaders > s.opt->SizeOfImage) return fail(out, "SizeOfHeaders exceeds SizeOfImage");
    const uint16_t num_sections = s.coff->NumberOfSections;
    if (num_sections == 0 || num_sections > 96) return fail(out, "implausible section count");
    const size_t section_end = size_t(opt_ptr - s.file) + s.coff->SizeOfOptionalHeader +
                               size_t(num_sections) * sizeof(PeSectionHeader);
    if (section_end > file_size || section_end > s.opt->SizeOfHeaders)
        return fail(out, "section table exceeds file or SizeOfHeaders");
    if (s.opt->SizeOfHeaders > file_size) return fail(out, "SizeOfHeaders exceeds file");
    s.sections = reinterpret_cast<const PeSectionHeader*>(opt_ptr + s.coff->SizeOfOptionalHeader);
    for (uint16_t i = 0; i < num_sections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        const uint64_t mapped_size = sec.VirtualSize > sec.SizeOfRawData ?
                                     sec.VirtualSize : sec.SizeOfRawData;
        if (uint64_t(sec.VirtualAddress) + mapped_size > s.opt->SizeOfImage ||
            uint64_t(sec.PointerToRawData) + sec.SizeOfRawData > file_size)
            return fail(out, "section range exceeds image or file");
        if (mapped_size && sec.VirtualAddress < s.opt->SizeOfHeaders)
            return fail(out, "section overlap with headers");
        for (uint16_t j = 0; j < i; ++j)
        {
            const PeSectionHeader& prev = s.sections[j];
            const uint64_t prev_size = prev.VirtualSize > prev.SizeOfRawData ?
                                       prev.VirtualSize : prev.SizeOfRawData;
            if (mapped_size && prev_size &&
                uint64_t(sec.VirtualAddress) < uint64_t(prev.VirtualAddress) + prev_size &&
                uint64_t(prev.VirtualAddress) < uint64_t(sec.VirtualAddress) + mapped_size)
                return fail(out, "section overlap in image");
        }
    }
    if (s.opt->AddressOfEntryPoint != 0 && !executable_rva(s, s.opt->AddressOfEntryPoint))
        return fail(out, "entry point outside executable section");

    // ---- zero-cost refusals (before any allocation) ----
    if (s.opt->DataDirectory[14].Size != 0 ||
        s.opt->DataDirectory[14].VirtualAddress != 0) // PE_DIR_COM (CLR .NET runtime header)
        return fail(out, "PE carries a CLR/COM descriptor; managed images belong to capability #10");
    if (s.opt->NumberOfRvaAndSizes > PE_DIR_DELAYIMPORT &&
        (s.opt->DataDirectory[PE_DIR_DELAYIMPORT].Size != 0 ||
         s.opt->DataDirectory[PE_DIR_DELAYIMPORT].VirtualAddress != 0))
        return fail(out, "PE has delay imports; delay import resolution unsupported -- refused");
    const PeDataDirectory tls_dir = s.opt->DataDirectory[PE_DIR_TLS];
    if (tls_dir.Size != 0 || tls_dir.VirtualAddress != 0)
    {
        uint32_t tls_off = 0;
        if (tls_dir.VirtualAddress == 0 || tls_dir.Size < sizeof(PeTlsDirectory64) ||
            !image_range(s, tls_dir.VirtualAddress, tls_dir.Size) ||
            !rva_to_file_offset(s, tls_dir.VirtualAddress, tls_dir.Size, tls_off))
            return fail(out, "corrupt TLS directory");
        PeTlsDirectory64 tls;
        std::memcpy(&tls, s.file + tls_off, sizeof(tls));
        if (!guest_segment_guarded &&
            (tls.StartAddressOfRawData != tls.EndAddressOfRawData ||
             tls.AddressOfCallBacks != 0 || tls.SizeOfZeroFill != 0))
            return fail(out, "PE has a static TLS directory with data or callbacks; "
                             "TEB/gs thread-local access unsupported -- refused");
    }
    // A missing or empty TLS directory cannot establish absence of Windows
    // TEB access. 0x65 is the x86 GS segment-override prefix. Reject any
    // executable file byte with that value until the runtime supports GS/TEB.
    // This is intentionally conservative: an immediate or embedded data byte
    // may also be 0x65, so this can refuse a DLL that does not use GS.
    for (uint16_t i = 0; !guest_segment_guarded && i < s.coff->NumberOfSections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        if (!(sec.Characteristics & PE_SCN_MEM_EXECUTE) || !sec.SizeOfRawData) continue;
        const uint8_t* begin = s.file + sec.PointerToRawData;
        const auto* hit = static_cast<const uint8_t*>(std::memchr(begin, 0x65, sec.SizeOfRawData));
        if (hit)
        {
            const uint64_t rva = uint64_t(sec.VirtualAddress) + (hit - begin);
            char reason[160];
            std::snprintf(reason, sizeof(reason),
                          "PE executable byte at RVA 0x%llx is 0x65 (possible GS segment override); TEB/gs unsupported -- refused",
                          (unsigned long long)rva);
            return fail(out, reason);
        }
    }

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
    if (s.slide != 0 && !s.opts.allow_no_relocs &&
        (s.opt->DataDirectory[PE_DIR_BASERELOC].Size == 0 ||
         (s.coff->Characteristics & 0x0001u) != 0))
        return fail(out, "nonzero slide without usable base relocation directory");
    if (s.slide != 0 && s.opt->DataDirectory[PE_DIR_BASERELOC].Size != 0)
    {
        const uint32_t dir_rva = s.opt->DataDirectory[PE_DIR_BASERELOC].VirtualAddress;
        const uint32_t dir_size = s.opt->DataDirectory[PE_DIR_BASERELOC].Size;
        if (dir_rva == 0 || dir_size < sizeof(PeRelocBlock) ||
            !image_range(s, dir_rva, dir_size))
            return fail(out, "base relocation directory exceeds image");
        uint32_t walked = 0;
        size_t applied_relocations = 0;
        while (walked + sizeof(PeRelocBlock) <= dir_size)
        {
            const PeRelocBlock* block = reinterpret_cast<const PeRelocBlock*>(image + dir_rva + walked);
            if (block->BlockSize < sizeof(PeRelocBlock) ||
                (block->BlockSize - sizeof(PeRelocBlock)) % sizeof(PeRelocEntry) != 0 ||
                uint64_t(walked) + block->BlockSize > dir_size)
                return fail(out, "corrupt base relocation block");
            const size_t count = (block->BlockSize - sizeof(PeRelocBlock)) / sizeof(PeRelocEntry);
            const PeRelocEntry* entries = reinterpret_cast<const PeRelocEntry*>(block + 1);
            for (size_t e = 0; e < count; ++e)
            {
                const uint64_t at_rva = uint64_t(block->PageRva) + entries[e].Offset;
                switch (entries[e].Type)
                {
                    case PE_REL_ABSOLUTE:
                        break; // padding
                    case PE_REL_DIR64:
                        if (!image_range(s, at_rva, sizeof(uint64_t)))
                            return fail(out, "relocation target outside image");
                        *reinterpret_cast<uint64_t*>(image + at_rva) += s.slide;
                        ++applied_relocations;
                        break;
                    case PE_REL_HIGHLOW:
                        if (!image_range(s, at_rva, sizeof(uint32_t)))
                            return fail(out, "relocation target outside image");
                        *reinterpret_cast<uint32_t*>(image + at_rva) += uint32_t(s.slide);
                        ++applied_relocations;
                        break;
                    default:
                        return fail(out, "unsupported base relocation type (not DIR64/HIGHLOW)");
                }
            }
            walked += block->BlockSize;
        }
        if (walked != dir_size) return fail(out, "corrupt base relocation block trailing bytes");
        if (applied_relocations == 0 && !s.opts.allow_no_relocs)
            return fail(out, "nonzero slide without usable base relocation entries");
    }

    // ---- imports (design 3d): resolve all, then refuse listing every miss ----
    if (s.opt->DataDirectory[PE_DIR_IMPORT].Size != 0 ||
        s.opt->DataDirectory[PE_DIR_IMPORT].VirtualAddress != 0)
    {
        const uint32_t dir_rva = s.opt->DataDirectory[PE_DIR_IMPORT].VirtualAddress;
        const uint32_t dir_size = s.opt->DataDirectory[PE_DIR_IMPORT].Size;
        uint32_t dir_off = 0;
        if (dir_rva == 0 || dir_size < sizeof(PeImportDescriptor) ||
            !image_range(s, dir_rva, dir_size) ||
            !rva_to_file_offset(s, dir_rva, dir_size, dir_off))
            return fail(out, "corrupt import directory bounds");
        bool terminated = false;
        for (uint32_t idx = 0;
             uint64_t(idx + 1) * sizeof(PeImportDescriptor) <= dir_size; ++idx)
        {
            uint32_t off = 0;
            if (!rva_to_file_offset(s, dir_rva + idx * sizeof(PeImportDescriptor), sizeof(PeImportDescriptor), off))
                return fail(out, "corrupt import directory (descriptor out of range)");
            const PeImportDescriptor d = *reinterpret_cast<const PeImportDescriptor*>(s.file + off);
            if (d.FirstThunk == 0 && d.Name == 0) { terminated = true; break; }
            if (!image_range(s, d.FirstThunk, 8))
                return fail(out, "corrupt import descriptor (IAT out of range)");

            uint32_t name_off = 0;
            if (!rva_to_file_offset(s, d.Name, 1, name_off)) return fail(out, "corrupt import descriptor name");
            const char* dll = reinterpret_cast<const char*>(s.file + name_off);
            const size_t dll_cap = rva_file_span(s, d.Name);
            if (!std::memchr(dll, '\0', dll_cap)) return fail(out, "unterminated import DLL name");

            // Prefer the INT (OriginalFirstThunk) for names; write resolutions
            // into the IAT (FirstThunk) in the mapped image.
            const uint32_t thunk_rva = d.OriginalFirstThunk ? d.OriginalFirstThunk : d.FirstThunk;
            for (uint32_t t = 0;; ++t)
            {
                uint32_t toff = 0;
                const uint64_t thunk_at = uint64_t(thunk_rva) + uint64_t(t) * 8;
                const uint64_t iat_at = uint64_t(d.FirstThunk) + uint64_t(t) * 8;
                if (thunk_at > UINT32_MAX || !image_range(s, thunk_at, 8) ||
                    !image_range(s, iat_at, 8) ||
                    !rva_to_file_offset(s, uint32_t(thunk_at), 8, toff))
                    return fail(out, "corrupt import thunk table");
                const uint64_t thunk = *reinterpret_cast<const uint64_t*>(s.file + toff);
                if (thunk == 0) break; // end of this DLL's thunk array

                void* resolved = nullptr;
                const char* symbol = nullptr;
                uint64_t ordinal = 0;
                if (thunk & PE_IMPORT_BY_ORDINAL)
                {
                    if (thunk & 0x7FFFFFFFFFFF0000ull)
                        return fail(out, "corrupt import ordinal thunk");
                    ordinal = thunk & 0xFFFFull;
                }
                else
                {
                    uint32_t soff = 0;
                    if (thunk > UINT32_MAX || !rva_to_file_offset(s, uint32_t(thunk), 3, soff))
                        return fail(out, "corrupt import name entry");
                    symbol = reinterpret_cast<const char*>(s.file + soff + 2); // skip Hint
                    const size_t cap = rva_file_span(s, uint32_t(thunk)) - 2;
                    if (!std::memchr(symbol, '\0', cap))
                        return fail(out, "unterminated import symbol name");
                }
                resolved = resolver.resolve(dll, symbol, ordinal);
                if (!resolved)
                {
                    out.missing.push_back(std::string(dll) + "!" +
                        (symbol ? std::string(symbol) : "#" + std::to_string(ordinal)));
                }
                else
                {
                    *reinterpret_cast<uint64_t*>(image + iat_at) = uint64_t(resolved);
                }
            }
        }
        if (!terminated) return fail(out, "corrupt import directory (missing terminator)");
        if (!out.missing.empty())
            return fail(out, "unresolved imports (fail-closed; no null-filled IAT)");
    }

    // A guarded caller prepares relocated TLS before the image becomes
    // read-only/executable. It receives only this temporary mapped view;
    // failure tears down the incomplete map and publishes no LoadResult.
    if (prepare)
    {
        PeMappedPreparation view;
        view.image = image;
        view.image_size = s.opt->SizeOfImage;
        view.sections = s.sections;
        view.section_count = num_sections;
        if (tls_dir.VirtualAddress)
            view.tls = reinterpret_cast<const PeTlsDirectory64*>(image + tls_dir.VirtualAddress);
        if (!prepare(view, reason, prepare_user))
            return fail(out, reason.empty() ? "mapped PE preparation refused" : reason);
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
    kept->image_size = s.opt->SizeOfImage;
    kept->export_dir_rva = s.opt->DataDirectory[PE_DIR_EXPORT].VirtualAddress;
    kept->export_dir_size = s.opt->DataDirectory[PE_DIR_EXPORT].Size;
    for (uint16_t i = 0; i < num_sections; ++i)
    {
        const PeSectionHeader& sec = s.sections[i];
        if (sec.Characteristics & PE_SCN_MEM_EXECUTE)
        {
            const uint32_t exec_bytes = sec.VirtualSize && sec.VirtualSize < sec.SizeOfRawData ?
                                        sec.VirtualSize : sec.SizeOfRawData;
            kept->executable_ranges.push_back({sec.VirtualAddress, sec.VirtualAddress + exec_bytes});
        }
    }
    out.ok = true;
    out.impl = kept;
    return out;
#endif
}

LoadResult pe_load(const void* file_bytes, size_t file_size, ImportResolver& resolver,
                   const LoadOptions& options)
{
    return pe_load_internal(file_bytes, file_size, resolver, options,
                            false, nullptr, nullptr);
}

namespace {
struct ExportView
{
    PeExportDirectory directory{};
    const uint32_t* functions = nullptr;
};

bool export_view(const LoadResult& result, ExportView& view)
{
    if (!result.ok || !result.impl || !result.impl->export_dir_size) return false;
    const auto& mapped = *result.impl;
    const auto inside = [&](uint64_t rva, uint64_t bytes) {
        return rva <= mapped.image_size && bytes <= mapped.image_size - rva;
    };
    if (mapped.export_dir_size < sizeof(PeExportDirectory) ||
        !inside(mapped.export_dir_rva, mapped.export_dir_size)) return false;
    std::memcpy(&view.directory, mapped.image + mapped.export_dir_rva,
                sizeof(view.directory));
    const auto& ex = view.directory;
    if (!inside(ex.AddressOfFunctions,
                uint64_t(ex.NumberOfFunctions) * sizeof(uint32_t))) return false;
    view.functions = reinterpret_cast<const uint32_t*>(mapped.image + ex.AddressOfFunctions);
    return true;
}

bool export_index(const LoadResult& result, const ExportView& view,
                  const char* name, uint64_t ordinal, uint32_t& index)
{
    const auto& ex = view.directory;
    if (!name)
    {
        if (ordinal > UINT16_MAX || ordinal < ex.Base) return false;
        const uint64_t slot = ordinal - ex.Base;
        if (slot >= ex.NumberOfFunctions) return false;
        index = static_cast<uint32_t>(slot);
        return true;
    }
    const auto inside = [&](uint64_t rva, uint64_t bytes) {
        return rva <= result.impl->image_size &&
               bytes <= result.impl->image_size - rva;
    };
    if (!inside(ex.AddressOfNames, uint64_t(ex.NumberOfNames) * sizeof(uint32_t)) ||
        !inside(ex.AddressOfNameOrdinals,
                uint64_t(ex.NumberOfNames) * sizeof(uint16_t))) return false;
    const uint32_t* names = reinterpret_cast<const uint32_t*>(
        result.image + ex.AddressOfNames);
    const uint16_t* ords = reinterpret_cast<const uint16_t*>(
        result.image + ex.AddressOfNameOrdinals);
    for (uint32_t i = 0; i < ex.NumberOfNames; ++i)
    {
        const uint32_t name_rva = names[i];
        if (!inside(name_rva, 1)) return false;
        const char* found = reinterpret_cast<const char*>(result.image + name_rva);
        if (!std::memchr(found, '\0', result.impl->image_size - name_rva)) return false;
        if (std::strcmp(found, name) != 0) continue;
        if (ords[i] >= ex.NumberOfFunctions) return false;
        index = ords[i];
        return true;
    }
    return false;
}

void* export_target(const LoadResult& result, const ExportView& view,
                    uint32_t index)
{
    const auto& mapped = *result.impl;
    if (index >= view.directory.NumberOfFunctions) return nullptr;
    const uint32_t fn_rva = view.functions[index];
    if (!fn_rva || fn_rva >= mapped.image_size) return nullptr;
    // Forwarders are strings within the export directory, never callable code.
    if (fn_rva >= mapped.export_dir_rva &&
        uint64_t(fn_rva) < uint64_t(mapped.export_dir_rva) + mapped.export_dir_size)
        return nullptr;
    for (const auto& range : mapped.executable_ranges)
        if (fn_rva >= range.begin && fn_rva < range.end)
            return mapped.image + fn_rva;
    return nullptr;
}
} // namespace

void* LoadResult::find_export(const char* name) const
{
    ExportView view;
    if (!name || !export_view(*this, view)) return nullptr;
    uint32_t index = 0;
    if (!export_index(*this, view, name, 0, index)) return nullptr;
    return export_target(*this, view, index);
}

void* LoadResult::find_export_ordinal(uint64_t ordinal) const
{
    ExportView view;
    if (!export_view(*this, view)) return nullptr;
    uint32_t index = 0;
    if (!export_index(*this, view, nullptr, ordinal, index)) return nullptr;
    return export_target(*this, view, index);
}

bool LoadResult::find_export_forwarder(const char* name, uint64_t ordinal,
                                       std::string& target) const
{
    target.clear();
    ExportView view;
    if (!export_view(*this, view)) return false;
    uint32_t index = 0;
    if (!export_index(*this, view, name, ordinal, index)) return false;
    const uint32_t rva = view.functions[index];
    const uint64_t begin = impl->export_dir_rva;
    const uint64_t end = begin + impl->export_dir_size;
    if (rva < begin || rva >= end) return false;
    const size_t available = static_cast<size_t>(end - rva);
    const size_t cap = available < 4096 ? available : 4096;
    const char* text = reinterpret_cast<const char*>(image + rva);
    const void* nul = std::memchr(text, '\0', cap);
    if (!nul || nul == text) return false;
    target.assign(text, static_cast<const char*>(nul) - text);
    return true;
}

namespace {
bool opt_dir_nonzero(const void* file_bytes, size_t file_size, uint32_t index)
{
    if (!file_bytes || file_size < sizeof(PeDosHeader) + 4 + sizeof(PeFileHeader) + sizeof(PeOptionalHeader64))
        return false;
    const auto* data = static_cast<const uint8_t*>(file_bytes);
    const PeDosHeader* dos = reinterpret_cast<const PeDosHeader*>(data);
    if (dos->e_magic != 0x5A4D) return false;
    if (dos->e_lfanew < 0 || uint64_t(dos->e_lfanew) + 4 + sizeof(PeFileHeader) > file_size)
        return false;
    const auto* coff = reinterpret_cast<const PeFileHeader*>(data + dos->e_lfanew + 4);
    if (coff->Machine != 0x8664 || coff->SizeOfOptionalHeader < sizeof(PeOptionalHeader64))
        return false;
    const uint64_t opt_off = uint64_t(dos->e_lfanew) + 4 + sizeof(PeFileHeader);
    if (opt_off + coff->SizeOfOptionalHeader > file_size) return false;
    const auto* opt = reinterpret_cast<const PeOptionalHeader64*>(reinterpret_cast<const uint8_t*>(coff) + sizeof(PeFileHeader));
    if (index >= 16 || opt->Magic != 0x020B || opt->NumberOfRvaAndSizes <= index) return false;
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
