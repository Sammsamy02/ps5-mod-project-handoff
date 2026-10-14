// shadow_copy.hpp — "shadow copy" code scanner for the PS5 UE4SS runtime port.
//
// Worker: worker-code-scan (research + prototype; no console contact).
//
// Problem being solved
// -------------------
// PC UE4SS finds engine functions/objects with an AOB (array-of-bytes) scan of
// the game module's mapped memory. On this PS5 target the game's executable
// segment is mapped EXEC-ONLY (kernel VMMAP prot=4, no READ bit): data reads
// of the live text fault (igf4 rc=3 covered_not_readable at 0x33ac8c0 /
// 0x890aa0; igf6 died twice at the first exec-span read). A live-page AOB
// scan of the game image is therefore impossible on this target.
//
// The shadow-copy approach: load the DECRYPTED eboot (the same sha-pinned
// image already used offline, sha256 3b6cfad4...) into OUR OWN PRX address
// space as ordinary readable memory, AOB-scan that private copy, and map a
// hit at linked vaddr X to the live game address X + slide, where slide is
// the per-boot load bias the bridge's geometry code already computes
// (validated uniform +0x400000 across all PT_LOADs on observed boots).
//
// File-offset trap (documented in the game profile, addresses.translation_rule):
//   file byte offset == linked vaddr + 0x4000  for LOAD0/LOAD1/LOAD3/LOAD7
//   file byte offset == linked vaddr - 0x324000 for LOAD8
// This module never assumes a uniform rule: every PT_LOAD's own
// p_offset/p_vaddr pair is used, exactly like the bridge's phdr parsing.
//
// Verified against proven seeds (launch-10 / anchor-validation):
//   g_uobject_array  linked 0x93ba110  runtime 0x97ba110  (BSS data; found via rip-relative code xrefs)
//   fname_ctor_wchar linked 0x23d590   runtime 0x63d590   (unique 16B prologue)
//   gmalloc          linked 0x930add8  runtime 0x970add8  (BSS data; found via the unique bootstrap store at 0xb660)
//   static_construct_object_internal linked 0x490aa0 runtime 0x890aa0 (25-byte sig for uniqueness)
//
// Dual-target: compiles on the macOS host (host_test) and inside the PS5 PRX
// (no OS calls in the scan path; memory comes from an injectable allocator).

#ifndef RC_SHADOW_COPY_HPP
#define RC_SHADOW_COPY_HPP

#include <cstddef>
#include <cstdint>

namespace rc::shadowcopy
{

    constexpr size_t kMaxShadowSegments = 8; // this image has 5 PT_LOADs

    // One PT_LOAD mirrored into PRX-owned memory. backing points INTO the
    // single contiguous ShadowImage::base allocation: backing[i] holds the
    // image byte for linked vaddr (vaddr + i). The BSS tail (filesz..memsz)
    // is present as zero bytes so translation stays total, but is never
    // scanned by default: BSS content at runtime differs from the file (it is
    // uninitialized in the file by definition). The single-allocation layout
    // exists so the whole shadow satisfies UE4SS's ModuleSnapshot contract
    // (one module extent [base, base+size) containing sorted, non-overlapping
    // readable ranges — PlatformModuleSnapshot.hpp rejects anything else).
    struct ShadowSegment
    {
        uint64_t vaddr;   // p_vaddr (linked address space)
        uint64_t memsz;   // p_memsz
        uint64_t filesz;  // p_filesz
        uint64_t fileoff; // p_offset (source offset inside the decrypted eboot file)
        uint32_t flags;   // p_flags: PF_X=1 PF_W=2 PF_R=4 (0x4000 16KiB alignment implied)
        uint8_t *backing; // == ShadowImage::base + vaddr - first selected segment's vaddr
    };

    struct ShadowImage
    {
        ShadowSegment segments[kMaxShadowSegments];
        size_t segment_count;
        uint64_t slide;               // per-boot ASLR offset: live == linked + slide
        uint64_t image_extent_vaddr;  // max(p_vaddr + p_memsz) over ALL PT_LOADs (incl. unloaded)
        uint64_t selected_extent_vaddr; // max(p_vaddr + p_memsz) over the LOADED segments
        uint8_t *base;                // single contiguous PRX-owned allocation ("the known base"):
                                      //   linked vaddr V (loaded)  -> base + V - segments[0].vaddr
                                      //   live game address        -> that pointer + (slide + segments[0].vaddr)
                                      //   i.e. live == shadow_ptr + slide - base - segments[0].vaddr
        uint64_t backing_size;        // selected_extent_vaddr - segments[0].vaddr (allocation size)
        uint64_t total_backing_bytes; // == backing_size (diagnostics alias)
    };

    // Allocator indirection so the PRX can route the ~150 MiB working copy to
    // its own heap; the default is calloc/free (host + PRX libc both fine).
    // alloc must return ZEROED memory (or shadow_init zeroes it itself).
    struct ShadowAllocator
    {
        void *(*alloc)(void *ctx, size_t bytes);
        void (*free)(void *ctx, void *ptr, size_t bytes);
        void *ctx;
    };

    // Which PT_LOAD classes become scannable shadow copies. Default = the
    // runtime-proven set: LOAD0 --X, LOAD1 R--, LOAD3/LOAD7 RW-. LOAD8 (---)
    // is OFF by default: the game profile records NO observed vmmap span for
    // LOAD8's runtime range at the igf3 vintage ("unmapped-until-proven"), so
    // addresses found there cannot be honestly mapped to live addresses yet.
    struct ShadowLoadPolicy
    {
        bool load_exec;   // PF_X only (--X, LOAD0: the game text)
        bool load_rodata; // PF_R only (R--, LOAD1: rodata/string anchors)
        bool load_rw;     // PF_R|PF_W (RW-, LOAD3/LOAD7: data + RELRO)
        bool load_nox;    // flags 0 (---, LOAD8): opt-in only, see above
    };

    enum class ShadowError
    {
        None = 0,
        NullInput,
        NotElf,          // bad magic / class / endian
        BadPhdrs,        // insane e_phoff/e_phentsize/e_phnum
        NoSegments,      // policy selected nothing
        FileTruncated,   // p_offset+p_filesz beyond the supplied file bytes
        AllocFailed,
        InternalLimit,   // more PT_LOADs than kMaxShadowSegments
    };

    const char *shadow_error_name(ShadowError e) noexcept;

    // Default allocator (calloc/free). Not thread-safe to swap; set once.
    ShadowAllocator shadow_default_allocator() noexcept;

    // Parse the decrypted eboot's ELF64 program headers (same field layout
    // the bridge's VMMAP/phdr code parses: e_phoff@0x20, e_phentsize@0x36,
    // e_phnum@0x38, phdr records of 56 bytes) and copy the selected PT_LOADs'
    // file-backed bytes into PRX-owned zero-filled memory. On success installs
    // the image as the module-active shadow used by scan_pattern() below and
    // returns it via *out. On failure *out is nullptr and no memory is kept.
    ShadowError shadow_init(const ShadowImage **out,
                            const uint8_t *file_bytes, size_t file_size,
                            uint64_t slide,
                            const ShadowLoadPolicy &policy,
                            const ShadowAllocator &alloc);

    // Convenience overload with the default (runtime-proven) load policy and
    // allocator.
    ShadowError shadow_init(const ShadowImage **out,
                            const uint8_t *file_bytes, size_t file_size,
                            uint64_t slide);

    void shadow_shutdown(const ShadowImage *img);

    // Active image installed by shadow_init (nullptr before init / after shutdown).
    const ShadowImage *shadow_active() noexcept;

    // ---- translation -------------------------------------------------------

    // Linked vaddr -> pointer into the shadow copy (nullptr if outside every
    // segment's [vaddr, vaddr+memsz)).
    uint8_t *shadow_vaddr_to_ptr(const ShadowImage *img, uint64_t vaddr) noexcept;

    // Pointer into the shadow copy -> linked vaddr (false if not inside).
    bool shadow_ptr_to_vaddr(const ShadowImage *img, const uint8_t *ptr, uint64_t *vaddr_out) noexcept;

    // live game address == linked vaddr + slide (per-boot; the bridge computes
    // the live base via its VMMAP/anchor geometry — validated 0x400000).
    uint64_t shadow_linked_to_live(const ShadowImage *img, uint64_t linked) noexcept;
    uint64_t shadow_live_to_linked(const ShadowImage *img, uint64_t live) noexcept;

    // ---- the AOB scanner ----------------------------------------------------

    // UE4SS-style masked scan over the ACTIVE shadow copy (installed by
    // shadow_init). Searches every loaded segment's FILE-BACKED region in
    // ascending linked-vaddr order; a match never crosses a segment boundary.
    //
    //   pattern  len bytes; mask[i] == '?' -> wildcard, anything else -> literal
    //   results  caller array filled with LINKED vaddrs (ascending)
    //   max_results capacity of results
    //   return   total number of matches found (a full count even when it
    //            exceeds max_results; only max_results are stored). SIZE_MAX
    //            on invalid arguments (no active shadow, len==0, mask
    //            shorter than len, all-wildcard mask, len > 4 GiB).
    //
    // No allocations, no exceptions, no OS calls: PRX/realtime-safe.
    size_t scan_pattern(uint8_t *pattern, const char *mask, size_t len,
                        uint64_t *results, size_t max_results);

    // Explicit-image and region-bounded variants. vaddr_hi is exclusive.
    size_t scan_pattern_in(const ShadowImage *img,
                           uint64_t vaddr_lo, uint64_t vaddr_hi,
                           const uint8_t *pattern, const char *mask, size_t len,
                           uint64_t *results, size_t max_results);

    // Same as scan_pattern but stores LIVE addresses (linked + slide).
    size_t scan_pattern_live(const ShadowImage *img,
                             const uint8_t *pattern, const char *mask, size_t len,
                             uint64_t *results, size_t max_results);

    // ---- rip-relative resolution (data seeds via code xrefs) ----------------

    // Decode the rip-relative instruction at linked vaddr site_vaddr and
    // return its target. Covers the encodings seen in this image's code-xref
    // routes: lea/mov with or without REX.W/REX.WR (48/4C), opcodes 8D (lea),
    // 8B/89 (mov both directions), modrm mod==00 rm==101, disp32 follows.
    //   48 8D 1D dddddddd  lea rbx,[rip+d]   (GUA ref sites 0x2b212a et al.)
    //   4C 8D 2D dddddddd  lea r13,[rip+d]   (GUA ref sites 0x2b1e57 et al.)
    //   48 89 1D dddddddd  mov [rip+d],rbx   (GMalloc bootstrap store 0xb660)
    //   48 8B 05 dddddddd  mov rax,[rip+d]   (generic data load)
    // target == site_vaddr + instr_length + disp32.
    struct RipRefResult
    {
        uint64_t site_vaddr;
        size_t instr_length; // 6 (no REX) or 7 (REX)
        uint64_t target_vaddr;
    };
    bool resolve_rip_ref(const ShadowImage *img, uint64_t site_vaddr, RipRefResult *out) noexcept;

} // namespace rc::shadowcopy

#endif // RC_SHADOW_COPY_HPP
