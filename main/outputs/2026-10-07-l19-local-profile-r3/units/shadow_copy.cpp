// shadow_copy.cpp — implementation of the shadow-copy AOB scanner.
// See shadow_copy.hpp for the design contract. Pure C++17 + <cstring>; no OS
// calls in the scan path; the only runtime dependency is the injected (or
// default calloc/free) allocator during shadow_init/shadow_shutdown.

#include "shadow_copy.hpp"

#include <cstdlib>
#include <cstring>

namespace rc::shadowcopy
{

    namespace
    {
        // ---- ELF64 constants (Prospero ET_SCE_EXEC image, ei_osabi=9) -------
        constexpr uint8_t kElfMagic[4] = {0x7F, 'E', 'L', 'F'};
        constexpr uint16_t kElfClass64 = 2;
        constexpr uint16_t kElfDataLe = 1;
        constexpr uint16_t kPtLoad = 1;
        constexpr uint32_t kPfX = 1;
        constexpr uint32_t kPfW = 2;
        constexpr uint32_t kPfR = 4;
        constexpr size_t kEhdrPhoff = 0x20; // e_phoff (Q)
        constexpr size_t kEhdrPhentsize = 0x36; // e_phentsize (H)
        constexpr size_t kEhdrPhnum = 0x38; // e_phnum (H)
        constexpr size_t kExpectedPhentsize = 56;
        constexpr size_t kMaxPhnum = 64;

        // phdr record field offsets (Elf64_Phdr)
        struct RawPhdr
        {
            uint32_t p_type;
            uint32_t p_flags;
            uint64_t p_offset;
            uint64_t p_vaddr;
            uint64_t p_paddr;
            uint64_t p_filesz;
            uint64_t p_memsz;
            uint64_t p_align;
        };

        uint16_t ld16(const uint8_t *p) noexcept
        {
            return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) |
                                         (static_cast<uint16_t>(p[1]) << 8));
        }
        uint32_t ld32(const uint8_t *p) noexcept
        {
            return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                   (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        }
        uint64_t ld64(const uint8_t *p) noexcept
        {
            return static_cast<uint64_t>(ld32(p)) | (static_cast<uint64_t>(ld32(p + 4)) << 32);
        }

        ShadowImage *g_active = nullptr; // installed by shadow_init

        void *default_alloc(void *, size_t bytes) noexcept
        {
            return std::calloc(1, bytes);
        }
        void default_free(void *, void *ptr, size_t) noexcept
        {
            std::free(ptr);
        }
    } // namespace

    const char *shadow_error_name(ShadowError e) noexcept
    {
        switch (e)
        {
        case ShadowError::None: return "None";
        case ShadowError::NullInput: return "NullInput";
        case ShadowError::NotElf: return "NotElf";
        case ShadowError::BadPhdrs: return "BadPhdrs";
        case ShadowError::NoSegments: return "NoSegments";
        case ShadowError::FileTruncated: return "FileTruncated";
        case ShadowError::AllocFailed: return "AllocFailed";
        case ShadowError::InternalLimit: return "InternalLimit";
        }
        return "?";
    }

    ShadowAllocator shadow_default_allocator() noexcept
    {
        return ShadowAllocator{&default_alloc, &default_free, nullptr};
    }

    ShadowError shadow_init(const ShadowImage **out,
                            const uint8_t *file_bytes, size_t file_size,
                            uint64_t slide,
                            const ShadowLoadPolicy &policy,
                            const ShadowAllocator &alloc)
    {
        if (out == nullptr)
        {
            return ShadowError::NullInput;
        }
        *out = nullptr;
        if (file_bytes == nullptr || file_size < 64)
        {
            return ShadowError::NullInput;
        }
        if (alloc.alloc == nullptr || alloc.free == nullptr)
        {
            return ShadowError::NullInput;
        }

        // ---- ELF header validation (same fields the bridge parses) ---------
        if (std::memcmp(file_bytes, kElfMagic, sizeof(kElfMagic)) != 0 ||
            file_bytes[4] != kElfClass64 || file_bytes[5] != kElfDataLe)
        {
            return ShadowError::NotElf;
        }
        const uint64_t phoff = ld64(file_bytes + kEhdrPhoff);
        const uint16_t phentsize = ld16(file_bytes + kEhdrPhentsize);
        const uint16_t phnum = ld16(file_bytes + kEhdrPhnum);
        if (phentsize != kExpectedPhentsize || phnum == 0 || phnum > kMaxPhnum ||
            phoff > file_size || phoff + static_cast<uint64_t>(phnum) * phentsize > file_size)
        {
            return ShadowError::BadPhdrs;
        }

        // ---- pass 1: select PT_LOADs per policy ----------------------------
        struct Selected
        {
            RawPhdr ph;
            uint32_t index;
        };
        Selected selected[kMaxShadowSegments];
        size_t selected_count = 0;
        uint64_t extent = 0;

        for (uint32_t i = 0; i < phnum; ++i)
        {
            const uint8_t *rec = file_bytes + phoff + static_cast<uint64_t>(i) * phentsize;
            RawPhdr ph{};
            ph.p_type = ld32(rec);
            ph.p_flags = ld32(rec + 4);
            ph.p_offset = ld64(rec + 8);
            ph.p_vaddr = ld64(rec + 16);
            ph.p_paddr = ld64(rec + 24);
            ph.p_filesz = ld64(rec + 32);
            ph.p_memsz = ld64(rec + 40);
            ph.p_align = ld64(rec + 48);

            if (ph.p_type != kPtLoad || ph.p_memsz == 0)
            {
                continue;
            }
            // The extent describes the WHOLE image (all PT_LOADs), including
            // segments the policy declines to load — consumers use it for
            // range classification (e.g. the seed_address_classes table).
            {
                const uint64_t end = ph.p_vaddr + ph.p_memsz;
                if (end > extent)
                {
                    extent = end;
                }
            }
            const bool exec = ph.p_flags == kPfX;
            const bool ro = ph.p_flags == kPfR;
            const bool rw = (ph.p_flags & kPfR) != 0 && (ph.p_flags & kPfW) != 0;
            const bool nox = ph.p_flags == 0;
            const bool want = (exec && policy.load_exec) || (ro && policy.load_rodata) ||
                              (rw && policy.load_rw) || (nox && policy.load_nox);
            if (!want)
            {
                continue;
            }
            if (ph.p_filesz > ph.p_memsz)
            {
                return ShadowError::BadPhdrs;
            }
            if (ph.p_offset > file_size || ph.p_offset + ph.p_filesz > file_size)
            {
                return ShadowError::FileTruncated;
            }
            if (selected_count >= kMaxShadowSegments)
            {
                return ShadowError::InternalLimit;
            }
            selected[selected_count].ph = ph;
            selected[selected_count].index = i;
            ++selected_count;
        }
        if (selected_count == 0)
        {
            return ShadowError::NoSegments;
        }

        // ---- pass 2: ONE contiguous allocation + fill -------------------------
        // Layout: base[0 .. backing_size) mirrors the linked vaddr space from
        // the first selected segment's vaddr through the max selected end
        // (inter-segment gaps included as never-scanned filler). This single
        // extent is what lets the shadow satisfy UE4SS's ModuleSnapshot
        // module-extent contract and makes the base->live arithmetic trivial.
        uint64_t min_vaddr = UINT64_MAX;
        uint64_t selected_extent = 0;
        for (size_t s = 0; s < selected_count; ++s)
        {
            const RawPhdr &ph = selected[s].ph;
            if (ph.p_vaddr < min_vaddr)
            {
                min_vaddr = ph.p_vaddr;
            }
            const uint64_t end = ph.p_vaddr + ph.p_memsz;
            if (end > selected_extent)
            {
                selected_extent = end;
            }
        }
        const uint64_t backing_size = selected_extent - min_vaddr;

        ShadowImage *img = static_cast<ShadowImage *>(alloc.alloc(alloc.ctx, sizeof(ShadowImage)));
        if (img == nullptr)
        {
            return ShadowError::AllocFailed;
        }
        std::memset(img, 0, sizeof(*img));
        img->slide = slide;
        img->image_extent_vaddr = extent; // whole image, including policy-excluded PT_LOADs
        img->selected_extent_vaddr = selected_extent;
        img->backing_size = backing_size;
        img->total_backing_bytes = backing_size;
        img->segment_count = selected_count;

        uint8_t *base = static_cast<uint8_t *>(alloc.alloc(alloc.ctx, static_cast<size_t>(backing_size)));
        if (base == nullptr)
        {
            alloc.free(alloc.ctx, img, sizeof(ShadowImage));
            return ShadowError::AllocFailed;
        }
        // Zero first: allocator independence + honest zeroed BSS tails. Then
        // copy each selected PT_LOAD's file-backed prefix at its vaddr slot.
        std::memset(base, 0, static_cast<size_t>(backing_size));
        img->base = base;
        for (size_t s = 0; s < selected_count; ++s)
        {
            const RawPhdr &ph = selected[s].ph;
            uint8_t *slot = base + (ph.p_vaddr - min_vaddr);
            if (ph.p_filesz != 0)
            {
                std::memcpy(slot, file_bytes + ph.p_offset, static_cast<size_t>(ph.p_filesz));
            }
            img->segments[s].vaddr = ph.p_vaddr;
            img->segments[s].memsz = ph.p_memsz;
            img->segments[s].filesz = ph.p_filesz;
            img->segments[s].fileoff = ph.p_offset;
            img->segments[s].flags = ph.p_flags;
            img->segments[s].backing = slot;
        }

        g_active = img;
        *out = img;
        return ShadowError::None;
    }

    ShadowError shadow_init(const ShadowImage **out,
                            const uint8_t *file_bytes, size_t file_size,
                            uint64_t slide)
    {
        ShadowLoadPolicy policy{};
        policy.load_exec = true;
        policy.load_rodata = true;
        policy.load_rw = true;
        policy.load_nox = false; // runtime-proven set; LOAD8 opt-in only
        return shadow_init(out, file_bytes, file_size, slide, policy, shadow_default_allocator());
    }

    void shadow_shutdown(const ShadowImage *img)
    {
        if (img == nullptr)
        {
            return;
        }
        ShadowAllocator alloc = shadow_default_allocator();
        // Single-allocation layout: one backing + the image struct itself.
        if (img->base != nullptr)
        {
            alloc.free(alloc.ctx, img->base, static_cast<size_t>(img->backing_size));
        }
        if (g_active == img)
        {
            g_active = nullptr;
        }
        alloc.free(alloc.ctx, const_cast<ShadowImage *>(img), sizeof(ShadowImage));
    }

    const ShadowImage *shadow_active() noexcept
    {
        return g_active;
    }

    // ---- translation --------------------------------------------------------

    uint8_t *shadow_vaddr_to_ptr(const ShadowImage *img, uint64_t vaddr) noexcept
    {
        if (img == nullptr)
        {
            return nullptr;
        }
        for (size_t s = 0; s < img->segment_count; ++s)
        {
            const ShadowSegment &seg = img->segments[s];
            if (vaddr >= seg.vaddr && vaddr - seg.vaddr < seg.memsz)
            {
                return seg.backing + (vaddr - seg.vaddr);
            }
        }
        return nullptr;
    }

    bool shadow_ptr_to_vaddr(const ShadowImage *img, const uint8_t *ptr, uint64_t *vaddr_out) noexcept
    {
        if (img == nullptr)
        {
            return false;
        }
        for (size_t s = 0; s < img->segment_count; ++s)
        {
            const ShadowSegment &seg = img->segments[s];
            const uint8_t *base = seg.backing;
            if (ptr >= base && ptr < base + seg.memsz)
            {
                if (vaddr_out != nullptr)
                {
                    *vaddr_out = seg.vaddr + static_cast<uint64_t>(ptr - base);
                }
                return true;
            }
        }
        return false;
    }

    uint64_t shadow_linked_to_live(const ShadowImage *img, uint64_t linked) noexcept
    {
        return img == nullptr ? 0 : linked + img->slide;
    }

    uint64_t shadow_live_to_linked(const ShadowImage *img, uint64_t live) noexcept
    {
        return img == nullptr || live < img->slide ? 0 : live - img->slide;
    }

    // ---- AOB scanner ---------------------------------------------------------

    namespace
    {
        // Scan one contiguous byte region for the masked pattern. Appends
        // matches (region_vaddr + offset) into results while capacity lasts;
        // adds every match to *total. Uses the first literal byte as the
        // memchr probe byte. Returns false on invalid arguments.
        bool scan_region(const uint8_t *region, size_t region_size, uint64_t region_vaddr,
                         const uint8_t *pattern, const char *mask, size_t len,
                         uint64_t *results, size_t max_results,
                         size_t *stored, size_t *total) noexcept
        {
            if (len == 0 || mask == nullptr || pattern == nullptr || len > region_size)
            {
                return len != 0 && mask != nullptr && pattern != nullptr; // empty region: no matches, but args can still be valid
            }

            // First literal byte (probe). All-wildcard masks are refused.
            size_t probe = SIZE_MAX;
            for (size_t i = 0; i < len; ++i)
            {
                if (mask[i] != '?')
                {
                    probe = i;
                    break;
                }
            }
            if (probe == SIZE_MAX)
            {
                return false; // refuse all-wildcard: would match everywhere
            }
            const uint8_t probe_byte = pattern[probe];

            const size_t last_start = region_size - len;
            size_t pos = 0;
            while (pos <= last_start)
            {
                const void *hit = std::memchr(region + pos, probe_byte, last_start - pos + 1);
                if (hit == nullptr)
                {
                    break;
                }
                size_t start = static_cast<size_t>(static_cast<const uint8_t *>(hit) - region);
                if (start < probe)
                { // probe byte cannot align; skip forward
                    pos = start + 1;
                    continue;
                }
                const size_t base = start - probe;
                bool match = true;
                for (size_t j = 0; j < len; ++j)
                {
                    if (mask[j] != '?' && region[base + j] != pattern[j])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    ++*total;
                    if (results != nullptr && *stored < max_results)
                    {
                        results[(*stored)++] = region_vaddr + base;
                    }
                    pos = base + 1;
                }
                else
                {
                    pos = start + 1;
                }
            }
            return true;
        }
    } // namespace

    size_t scan_pattern_in(const ShadowImage *img,
                           uint64_t vaddr_lo, uint64_t vaddr_hi,
                           const uint8_t *pattern, const char *mask, size_t len,
                           uint64_t *results, size_t max_results)
    {
        if (img == nullptr || pattern == nullptr || mask == nullptr || len == 0)
        {
            return SIZE_MAX;
        }
        if (std::memchr(mask, '\0', len) != nullptr)
        {
            return SIZE_MAX; // mask shorter than len
        }

        size_t stored = 0;
        size_t total = 0;
        for (size_t s = 0; s < img->segment_count; ++s)
        {
            const ShadowSegment &seg = img->segments[s];
            // Scan the FILE-BACKED portion only: BSS is zero-fill in the file;
            // scanning it would fabricate matches for zero-literal patterns.
            const uint64_t file_start = seg.vaddr;
            const uint64_t file_end = seg.vaddr + seg.filesz;
            const uint64_t lo = file_start > vaddr_lo ? file_start : vaddr_lo;
            const uint64_t hi = file_end < vaddr_hi ? file_end : vaddr_hi;
            if (lo >= hi)
            {
                continue;
            }
            if (!scan_region(seg.backing + (lo - seg.vaddr), static_cast<size_t>(hi - lo), lo,
                             pattern, mask, len, results, max_results, &stored, &total))
            {
                return SIZE_MAX; // invalid pattern/mask (e.g. all-wildcard)
            }
        }
        return total;
    }

    size_t scan_pattern(uint8_t *pattern, const char *mask, size_t len,
                        uint64_t *results, size_t max_results)
    {
        return scan_pattern_in(shadow_active(), 0, UINT64_MAX,
                               pattern, mask, len, results, max_results);
    }

    size_t scan_pattern_live(const ShadowImage *img,
                             const uint8_t *pattern, const char *mask, size_t len,
                             uint64_t *results, size_t max_results)
    {
        const size_t n = scan_pattern_in(img, 0, UINT64_MAX, pattern, mask, len, results, max_results);
        if (n == SIZE_MAX || img == nullptr)
        {
            return n;
        }
        const size_t capped = n < max_results ? n : max_results;
        for (size_t i = 0; i < capped; ++i)
        {
            results[i] += img->slide;
        }
        return n;
    }

    // ---- rip-relative resolution ----------------------------------------------

    bool resolve_rip_ref(const ShadowImage *img, uint64_t site_vaddr, RipRefResult *out) noexcept
    {
        if (img == nullptr || out == nullptr)
        {
            return false;
        }
        const uint8_t *p = shadow_vaddr_to_ptr(img, site_vaddr);
        if (p == nullptr)
        {
            return false;
        }

        size_t i = 0;
        uint8_t rex = 0;
        if (p[i] == 0x48 || p[i] == 0x4C) // REX.W / REX.WR (both seen in this image)
        {
            rex = p[i];
            ++i;
        }
        const uint8_t op = p[i];
        const bool lea = (op == 0x8D);
        const bool mov = (op == 0x8B || op == 0x89);
        if (!lea && !mov)
        {
            return false;
        }
        ++i;
        const uint8_t modrm = p[i];
        // rip-relative: mod == 00, rm == 101 (no SIB, disp32 next)
        if ((modrm & 0xC7) != 0x05)
        {
            return false;
        }
        ++i;
        if (i + 4 > 16) // sanity: read stays inside the fetched window
        {
            return false;
        }
        const int32_t disp = static_cast<int32_t>(ld32(p + i));
        const size_t instr_len = i + 4; // optional REX + opcode + modrm + disp32

        out->site_vaddr = site_vaddr;
        out->instr_length = instr_len;
        out->target_vaddr = site_vaddr + instr_len + static_cast<int64_t>(disp);
        (void)rex;
        return true;
    }

} // namespace rc::shadowcopy
