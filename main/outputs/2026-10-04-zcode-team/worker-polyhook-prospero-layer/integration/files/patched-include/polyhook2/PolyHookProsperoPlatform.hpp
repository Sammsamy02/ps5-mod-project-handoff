#ifndef POLYHOOK_2_PROSPERO_PLATFORM_HPP
#define POLYHOOK_2_PROSPERO_PLATFORM_HPP

// PolyHookProsperoPlatform.hpp — worker-polyhook-prospero-layer (2026-10-04).
//
// The Prospero platform layer PolyHook's x64Detour path needs and does not
// have (ue4ss-polyhook-platform-port-1/report.md:20-22 — "missing verified
// contract"): protection translation, old-protection lookup, fail-closed
// memory access gates, and a bounded near-address executable allocator.
//
// Design rules, each with its in-tree provenance:
//  * ADDITIVE. This header + PolyHookProsperoPlatform.cpp define exactly the
//    PLH symbols whose platform chains select no branch under __PROSPERO__
//    (MemProtector.cpp, MemAccessor.cpp, Misc.cpp, PolyHookOs.cpp define none
//    of them), so no canonical or port-overlay source is edited.
//  * SEAM. All kernel contact goes through plh_prospero::Backend function
//    pointers (mirrors worker-1-live-supplier live_module_os_ops.hpp). The
//    target backend (raw sysctls + raw syscalls) is bound in the .cpp under
//    __PROSPERO__; host tests inject fakes. The PLH-visible logic is
//    identical on both sides — this header is platform-independent C++.
//  * CURRENT MAP. Every protection/access decision re-collects and re-walks
//    a CURRENT sysctl KERN_PROC_VMMAP buffer (controller correction #1 of
//    worker-anchor-validation: never trust a stale map), using a byte-close
//    port of the proven v3 mode-1 walk (worker-ingame-probe-v3/src/vmmap_walk.c,
//    host-unit-tested lineage; copied through worker-igf5-diagnosis-rebuild
//    and worker-anchor-validation unchanged).
//  * FAIL CLOSED. Anything not fully covered by one trusted readable/writable
//    span is refused BEFORE dereference (igf4 seq4/5 "covered_not_readable"
//    discipline: rc=3 refusal, nothing crashed). Refusal surfaces as
//    PLH::ProtFlag::UNSET / status=false / written=0, which PLH's
//    MemoryProtector destructor and hook() error paths already treat as
//    "do not touch memory, fail the hook".
//
// VERDICT DEPENDENCY (worker-exec-writability EXEC-WRITABILITY-EVIDENCE.md):
// every function here that would actually issue kernel call 74/73/477 has a
// success outcome that is exactly the open exec-writability question
// (W_WORKS / W_FAILS_R_WORKS / BOTH_FAIL from EXEC-WRITABILITY-PROBE-DESIGN
// §7). This layer is required on the BEST-CASE branch (W_WORKS) and its
// fail-closed gates are the safe behavior on the worst ones; it does NOT
// settle the verdict and must not be armed in-game before the probe's
// go/no-go (controller + user).

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace plh_prospero {

// Byte-assembled loads (the proven walk never struct-casts: record
// boundaries are 4-aligned, not 8-aligned — vmmap_walk.c header comment).
inline uint32_t ld32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline uint64_t ld64(const uint8_t* p) {
    return (uint64_t)ld32(p) | ((uint64_t)ld32(p + 4) << 32);
}

// ---------------------------------------------------------------------------
// Pinned constants (verified this session against the pinned SDK v0.43):
//   SDK target/include/sys/mman.h:52-55  PROT_NONE 0x00 / READ 0x01 /
//                                        WRITE 0x02 / EXEC 0x04
//   SDK target/include/sys/syscall.h:81-82,398
//                                        SYS_munmap 73 / SYS_mprotect 74 /
//                                        SYS_mmap 477
//   SDK target/include/machine/param.h:99 PAGE_SHIFT 14 -> 16 KiB pages
//   kve_protection raw bits (FreeBSD kinfo_vmentry) share the 1/2/4 numbering
//   (live records: prot=0x4 exec-only span, prot=0x3 RW data span —
//   worker-exec-writability EXEC-WRITABILITY-EVIDENCE.md §1).
// ---------------------------------------------------------------------------
enum SdkProt : int {
    kProtNone = 0x0,
    kProtRead = 0x1,
    kProtWrite = 0x2,
    kProtExec = 0x4,
};

// PLH::ProtFlag underlying values (polyhook2/Enums.hpp:22-31, uint8_t):
// UNSET 0, X 2, R 4, W 8, S 16, P 32, NONE 64, RWX = R|W|X = 14.
enum PlhFlagBits : unsigned {
    kPlhUnset = 0u,
    kPlhX = 1u << 1,
    kPlhR = 1u << 2,
    kPlhW = 1u << 3,
    kPlhNone = 1u << 6,
    kPlhRwx = kPlhR | kPlhW | kPlhX,
};

static const uint32_t kPage16K = 0x4000u;      // pinned PAGE_SHIFT 14 fallback
static const uint32_t kVmHdr = 136u;           // kinfo_vmentry bytes before kve_path
static const uint32_t kKveSizeofCap = 1160u;   // sizeof(kinfo_vmentry), pinned SDK user.h

// ---------------------------------------------------------------------------
// Pure protection translation. Semantics match the POLYHOOK2_OS_LINUX branch
// of polyhook2/sources/MemProtector.cpp:93-126 bit for bit, under the SDK's
// (identical FreeBSD) numbering. No verdict dependency: arithmetic only.
// ---------------------------------------------------------------------------
inline constexpr int translate_plh_to_prot(unsigned plh_flags) {
    int native = kProtNone;
    if (plh_flags & kPlhX) native |= kProtExec;
    if (plh_flags & kPlhR) native |= kProtRead;
    if (plh_flags & kPlhW) native |= kProtWrite;
    if (plh_flags & kPlhNone) native = kProtNone;
    return native;
}

inline constexpr unsigned translate_prot_to_plh(int prot) {
    unsigned flags = kPlhUnset;
    if (prot & kProtExec) flags |= kPlhX;
    if (prot & kProtRead) flags |= kPlhR;
    if (prot & kProtWrite) flags |= kPlhW;
    if (prot == kProtNone) flags |= kPlhNone;
    return flags;
}

// ---------------------------------------------------------------------------
// Proven VMMAP walk (mode 1). Byte-close port of ig_vmmap_walk from
// worker-ingame-probe-v3/src/vmmap_walk.c (see CLAIM-3 provenance in
// CLAIM.json): byte-assembled loads (record boundaries are 4-aligned, never
// struct-cast), stride by each record's OWN kve_structsize, per-record
// sanity (start<=end, span <= 2^44, prot <= 0xff), never guesses past an
// insane structsize. WalkStats mirrors the IgVmStats fields the trust gate
// and evidence use.
// ---------------------------------------------------------------------------
struct VmSpan {
    uint64_t start;   // inclusive
    uint64_t end;     // exclusive
    uint32_t prot;    // raw kve_protection, uninterpreted (1=R, 2=W, 4=X)
};

struct WalkStats {
    uint32_t total;       // records consumed (any sanity)
    uint32_t retained;    // sane records kept into out[]
    uint32_t truncated;   // sane records dropped by the out cap
    uint32_t insane;      // records failing per-record sanity
    int stop_reason;      // 0 clean, 1 insane/short structsize, 2 partial tail
    uint64_t stop_off;    // byte offset where the walk stopped
    uint32_t ksz_full;    // structsize == kve_sizeof_cap records
    uint32_t ksz_var;     // 137..cap-1 (path-truncated) records
    uint32_t ksz_144;     // structsize == 144 records (P1 prediction)
    uint32_t aligned16k;  // sane records with 16-KiB-aligned start AND end
};

inline uint32_t vmmap_walk(const uint8_t* buf, uint64_t len,
                           uint32_t kve_sizeof_cap,
                           VmSpan* out, uint32_t cap, WalkStats* st) {
    if (kve_sizeof_cap < kVmHdr) kve_sizeof_cap = kVmHdr;
    uint32_t kept = 0;
    st->total = st->retained = st->truncated = st->insane = 0;
    st->stop_reason = 0;
    st->stop_off = 0;
    st->ksz_full = st->ksz_var = st->ksz_144 = st->aligned16k = 0;
    uint64_t off = 0;
    while (off + kVmHdr <= len) {
        const uint32_t ksz = ld32(buf + off);
        if (off + (uint64_t)ksz > len) {
            st->stop_reason = 2;  // cut mid-record (partial sysctl tail)
            break;
        }
        if (ksz < kVmHdr || ksz > kve_sizeof_cap || (ksz & 3u) != 0) {
            st->stop_reason = 1;  // insane/short structsize: stop, never guess
            break;
        }
        if (ksz == kve_sizeof_cap) ++st->ksz_full; else ++st->ksz_var;
        if (ksz == 144) ++st->ksz_144;
        const uint64_t start = ld64(buf + off + 8);   // KVE_START
        const uint64_t end = ld64(buf + off + 16);    // KVE_END
        const uint32_t prot = ld32(buf + off + 56);   // KVE_PROT
        ++st->total;
        const int sane = start <= end && (end - start) <= (1ull << 44) &&
                         prot <= 0xffu;
        if (!sane) {
            ++st->insane;
        } else {
            ++st->retained;
            if (kept < cap) {
                out[kept].start = start;
                out[kept].end = end;
                out[kept].prot = prot;
                ++kept;
                if (((start | end) & 0x3fffu) == 0) ++st->aligned16k;
            } else {
                ++st->truncated;
            }
        }
        off += ksz;
    }
    st->stop_off = off;
    return kept;
}

// Exact proven trust gate (ig_vmmap_trustworthy): enough sane records and
// <= 20% insane. A map that fails this may back NOTHING.
inline int vmmap_trustworthy(const WalkStats& st) {
    return (st.retained >= 8 && st.insane * 4 <= st.retained) ? 1 : 0;
}

// Exact proven coverage formula (ig_map_cover): one span must cover
// [addr, addr+len) fully: addr >= start && len <= end-start &&
// addr-start <= end-start-len.
inline const VmSpan* find_covering_span(const VmSpan* spans, uint32_t count,
                                        uint64_t addr, uint64_t len) {
    for (uint32_t i = 0; i < count; ++i) {
        const VmSpan& s = spans[i];
        if (addr >= s.start && len <= s.end - s.start &&
            addr - s.start <= s.end - s.start - len) {
            return &s;
        }
    }
    return nullptr;
}

// True when [addr, addr+len) intersects ANY recorded span. The allocator's
// candidate gate: a fresh anonymous mapping must land in a HOLE of the
// current map (kernel picks; the hint is advisory) — a candidate inside an
// existing mapping is exactly what must be skipped.
inline int range_intersects_any(const VmSpan* spans, uint32_t count,
                                uint64_t addr, uint64_t len) {
    for (uint32_t i = 0; i < count; ++i) {
        const VmSpan& s = spans[i];
        if (s.start < addr + len && addr < s.end) return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Backend seam. Every kernel touch is one of these five calls. The target
// backend (bound in PolyHookProsperoPlatform.cpp under __PROSPERO__)
// implements them with the pinned-SDK recipes; host tests inject fakes.
//   collect_vmmap: CURRENT-invocation sysctl KERN_PROC_VMMAP collection into
//     caller scratch (target: mib {CTL_KERN 1, KERN_PROC 14, KERN_PROC_VMMAP
//     32, getpid()}, 65536-byte first probe, bounded doubling, 4 MiB cap —
//     the v3/igf lineage recipe). 0 on success.
//   raw_mprotect: raw kernel protection transition. Target: inline syscall
//     74 (SYS_mprotect). Return 0 only on a observed-zero result; any other
//     result is a refusal. igf5 honesty note: a raw syscall's failure VALUE
//     is not a decoded errno (no CF decode in the igf5-lineage shim); the
//     raw result word is surfaced through last_raw_result for post-mortem.
//   raw_mmap: anonymous private mapping near hint. Target: raw syscall 477
//     (SYS_mmap). 0 = failure. libc mmap() must NOT be used in the composed
//     PRX toolchain (weak kernel_mmap resolves statically to 0 — the exact
//     libc.a mprotect trap of worker-import-audit-weakify §2.4).
//   raw_munmap: target: raw syscall 73 (SYS_munmap). 0 on success.
//   page_size: target: sysconf(_SC_PAGESIZE); the core falls back to the
//     pinned 0x4000 only when the value is non-positive or not a power of
//     two. sysconf's in-game return is itself unproven; do not infer.
// ---------------------------------------------------------------------------
struct Backend {
    void* ctx = nullptr;
    int (*collect_vmmap)(void* ctx, uint8_t* buf, uint32_t cap, uint64_t* len);
    int (*raw_mprotect)(void* ctx, uint64_t addr, uint64_t len, int prot);
    uint64_t (*raw_mmap)(void* ctx, uint64_t hint, uint64_t len, int prot);
    int (*raw_munmap)(void* ctx, uint64_t addr, uint64_t len);
    uint64_t (*page_size)(void* ctx);
};

// Backend registry (single-threaded PLH arm path; PLH serializes hooking).
const Backend* active_backend();
void set_active_backend(const Backend* backend);

// Bounded scratch for one collection. The target TU owns a static buffer of
// this size in the .cpp (matches the igf lineage 4 MiB collection cap).
static const uint32_t kVmmapScratchCap = 4u << 20;

// Diagnostics surface (post-mortem records only; never a status source).
struct Diag {
    uint64_t last_mprotect_raw;  // raw result word of the last SYS_mprotect
    uint64_t last_mmap_raw;      // raw result word of the last SYS_mmap
    uint32_t mprotect_calls;     // transitions attempted through this core
    uint32_t refused_no_cover;   // refusals: no single trusted covering span
    uint32_t refused_untrusted;  // refusals: map failed the trust gate
    uint32_t alloc_rwx_ok;       // allocations that got RWX directly
    uint32_t alloc_rw_exec_transition;  // allocations that need a later X flip
};
Diag* diag();  // defined once per binary in PolyHookProsperoPlatform.cpp;
               // host tests link host/diag_stub.cpp (same single definition).
const Backend* active_backend();  // defined in PolyHookProsperoPlatform.cpp
void set_active_backend(const Backend* backend);

inline bool page_size_valid(uint64_t ps) {
    return ps > 0 && (ps & (ps - 1)) == 0 && ps <= (64u << 10);
}

inline uint64_t page_size_or_fallback(const Backend& be) {
    const uint64_t ps = be.page_size ? be.page_size(be.ctx) : 0;
    if (page_size_valid(ps)) return ps;
    return kPage16K;  // pinned PAGE_SHIFT 14; documented fallback only
}

// One current collection + walk + trust gate. Returns retained count, or 0
// when the backend failed, the walk is untrusted, or retained overflowed
// the caller's span capacity in a way that would break coverage answers
// (truncated maps refuse: fail closed).
inline uint32_t collect_and_trust(const Backend& be, uint8_t* scratch,
                                  uint32_t scratch_cap, VmSpan* spans,
                                  uint32_t span_cap, WalkStats* st) {
    uint64_t len = 0;
    if (!be.collect_vmmap) return 0;
    if (be.collect_vmmap(be.ctx, scratch, scratch_cap, &len) != 0) return 0;
    if (len == 0 || len > scratch_cap) return 0;
    const uint32_t kept = vmmap_walk(scratch, len, kKveSizeofCap, spans,
                                     span_cap, st);
    if (!vmmap_trustworthy(*st)) {
        ++diag()->refused_untrusted;
        return 0;
    }
    if (st->truncated != 0) return 0;  // incomplete map: answer nothing
    return kept;
}

// ---------------------------------------------------------------------------
// mem_protect core: old-prot lookup FIRST (from the current map), then the
// transition. Returns the old protection as PLH flag bits. On any refusal:
// *status = 0 and returns kPlhUnset — PLH's MemoryProtector destructor then
// skips the restore (MemProtector.hpp:43) and isGood() is false.
// ---------------------------------------------------------------------------
inline unsigned mem_protect_core(const Backend& be, uint8_t* scratch,
                                 uint32_t scratch_cap, VmSpan* spans,
                                 uint32_t span_cap, uint64_t dest,
                                 uint64_t size, unsigned plh_flags,
                                 int* status) {
    ++diag()->mprotect_calls;
    *status = 0;
    const uint64_t ps = page_size_or_fallback(be);
    const uint64_t aligned_dest = dest & ~(ps - 1);
    const uint64_t aligned_size = (size + (ps - 1)) & ~(ps - 1);
    WalkStats st;
    const uint32_t kept = collect_and_trust(be, scratch, scratch_cap, spans,
                                            span_cap, &st);
    if (kept == 0) return kPlhUnset;
    const VmSpan* cover = find_covering_span(spans, kept, aligned_dest,
                                             aligned_size);
    if (!cover) {
        ++diag()->refused_no_cover;
        return kPlhUnset;
    }
    // Old protection comes from the CURRENT map's raw kve_protection bits
    // (1=R, 2=W, 4=X) — the igf-lineage restore discipline's lookup source.
    const unsigned old_flags = translate_prot_to_plh((int)cover->prot);
    const int rc = be.raw_mprotect
        ? be.raw_mprotect(be.ctx, aligned_dest, aligned_size,
                          translate_plh_to_prot(plh_flags))
        : -1;
    if (rc != 0) {
        // Observed non-zero: treat as refusal. Status stays 0; old prot is
        // still reported (Windows/Linux branches also return the old value
        // unconditionally) but the destructor will skip restore on UNSET
        // only — a refused transition with a known old prot would attempt
        // a restore, so refuse as UNSET to guarantee no further kernel
        // traffic after a failed transition.
        (void)old_flags;
        return kPlhUnset;
    }
    *status = 1;
    return old_flags;
}

// Publish exec permission for PLH-owned memory after a deferred-transition
// allocation (see bound_alloc_core). Same kernel path, same verdict
// dependency: success here is the W-verdict, not a property of this code.
inline int exec_publish_core(const Backend& be, uint8_t* scratch,
                             uint32_t scratch_cap, VmSpan* spans,
                             uint32_t span_cap, uint64_t addr, uint64_t size,
                             unsigned plh_flags) {
    int status = 0;
    (void)mem_protect_core(be, scratch, scratch_cap, spans, span_cap, addr,
                           size, plh_flags, &status);
    return status;
}

// ---------------------------------------------------------------------------
// Fail-closed access gates. Stricter than the Linux branch (which gates only
// the memcpy source): every byte actually dereferenced must sit inside a
// trusted R (read) / W (write) span of the CURRENT map. Reads and writes
// clamp to the covering span's end like the Linux branch does (PLH callers
// loop in bounded chunks); copies require full single-span coverage both
// sides because memcpy has no partial concept.
// ---------------------------------------------------------------------------
inline bool safe_read_core(const Backend& be, uint8_t* scratch,
                           uint32_t scratch_cap, VmSpan* spans,
                           uint32_t span_cap, uint64_t src, uint64_t dest,
                           uint64_t size, uint64_t* read) {
    *read = 0;
    WalkStats st;
    const uint32_t kept = collect_and_trust(be, scratch, scratch_cap, spans,
                                            span_cap, &st);
    if (kept == 0) return false;
    const VmSpan* cover = find_covering_span(spans, kept, src, 1);
    if (!cover || (cover->prot & kProtRead) == 0) {
        ++diag()->refused_no_cover;
        return false;  // igf4 "covered_not_readable": refuse before deref
    }
    const uint64_t avail = cover->end - src;
    const uint64_t n = size < avail ? size : avail;
    if (n == 0) return false;
    memcpy((void*)(uintptr_t)dest, (const void*)(uintptr_t)src, (size_t)n);
    *read = n;
    return true;
}

inline bool safe_write_core(const Backend& be, uint8_t* scratch,
                            uint32_t scratch_cap, VmSpan* spans,
                            uint32_t span_cap, uint64_t dest, uint64_t src,
                            uint64_t size, uint64_t* written) {
    *written = 0;
    WalkStats st;
    const uint32_t kept = collect_and_trust(be, scratch, scratch_cap, spans,
                                            span_cap, &st);
    if (kept == 0) return false;
    const VmSpan* ssrc = find_covering_span(spans, kept, src, 1);
    const VmSpan* sdst = find_covering_span(spans, kept, dest, 1);
    if (!ssrc || (ssrc->prot & kProtRead) == 0 ||
        !sdst || (sdst->prot & kProtWrite) == 0) {
        ++diag()->refused_no_cover;
        return false;
    }
    uint64_t n = size;
    if (ssrc->end - src < n) n = ssrc->end - src;
    if (sdst->end - dest < n) n = sdst->end - dest;
    if (n == 0) return false;
    memcpy((void*)(uintptr_t)dest, (const void*)(uintptr_t)src, (size_t)n);
    *written = n;
    return true;
}

inline bool mem_copy_core(const Backend& be, uint8_t* scratch,
                          uint32_t scratch_cap, VmSpan* spans,
                          uint32_t span_cap, uint64_t dest, uint64_t src,
                          uint64_t size) {
    WalkStats st;
    const uint32_t kept = collect_and_trust(be, scratch, scratch_cap, spans,
                                            span_cap, &st);
    if (kept == 0) return false;
    const VmSpan* ssrc = find_covering_span(spans, kept, src, size);
    const VmSpan* sdst = find_covering_span(spans, kept, dest, size);
    if (!ssrc || (ssrc->prot & kProtRead) == 0 ||
        !sdst || (sdst->prot & kProtWrite) == 0) {
        ++diag()->refused_no_cover;
        return false;
    }
    memcpy((void*)(uintptr_t)dest, (const void*)(uintptr_t)src, (size_t)size);
    return true;
}

// ---------------------------------------------------------------------------
// Bounded near-address executable allocator (Misc.hpp boundAlloc contract).
// Policy, and its documented verdict dependency (CLAIM-5):
//   1. Try mmap RWX at the ascending page-aligned hint (one raw syscall
//      477 per try; bounded tries, never MAP_FIXED — no clobber of unknown
//      pages).
//   2. If RWX is refused, try PROT_READ|PROT_WRITE and record
//      exec_transition_required: PLH then builds the trampoline writable
//      and a later exec_publish must flip it — an outcome that IS the
//      W-verdict. If BOTH_FAIL holds on target, allocation for detour use
//      must be treated as unavailable (hooks stay disabled).
//   3. Anything landing outside [start, start+len) bounds is unmapped and
//      the search advances; a kernel that ignores hints degrades to scheme
//      fallback inside PLH (x64Detour falls through to code cave), never to
//      an unbounded loop (PLH_PROSPERO_ALLOC_MAX_TRIES cap).
// ---------------------------------------------------------------------------
struct AllocDetail {
    uint64_t address;
    int used_rwx;                   // 1 when the RWX attempt succeeded
    int exec_transition_required;   // 1 when only RW could be obtained
    uint32_t attempts;              // raw_mmap calls issued
};

static const uint32_t kAllocMaxTries = 4096u;

inline uint64_t bound_alloc_core(const Backend& be, uint8_t* scratch,
                                 uint32_t scratch_cap, VmSpan* spans,
                                 uint32_t span_cap, uint64_t start,
                                 uint64_t end, uint64_t size,
                                 AllocDetail* detail) {
    AllocDetail local;
    if (!detail) detail = &local;
    detail->address = 0;
    detail->used_rwx = 0;
    detail->exec_transition_required = 0;
    detail->attempts = 0;
    if (end <= start || size == 0) return 0;
    const uint64_t ps = page_size_or_fallback(be);
    const uint64_t asz = (size + (ps - 1)) & ~(ps - 1);
    if (asz < size) return 0;  // overflow
    if (asz > end - start) return 0;

    // Current-map gate: a fresh mapping must land in a HOLE of the current
    // map (range_intersects_any == 0). The allocator never hints into an
    // existing mapping and never uses MAP_FIXED.
    WalkStats st;
    const uint32_t kept = collect_and_trust(be, scratch, scratch_cap, spans,
                                            span_cap, &st);
    if (kept == 0) return 0;

    uint64_t addr = start;
    if (addr % ps != 0) addr += ps - (addr % ps);
    for (uint32_t tries = 0; tries < kAllocMaxTries && addr <= end - asz;
         ++tries, addr += ps) {
        // The candidate page range must sit in free address space.
        if (range_intersects_any(spans, kept, addr, asz)) continue;

        if (be.raw_mmap) {
            const uint64_t rwx = be.raw_mmap(be.ctx, addr, asz,
                                             kProtRead | kProtWrite |
                                             kProtExec);
            ++detail->attempts;
            if (rwx != 0) {
                // Kernel may ignore the hint (Linux branch precedent:
                // Misc.cpp:202-208 checks the result against the window and
                // frees when outside). Never hand out an out-of-range page.
                if (rwx >= start && rwx <= end - asz) {
                    detail->address = rwx;
                    detail->used_rwx = 1;
                    ++diag()->alloc_rwx_ok;
                    return rwx;
                }
                if (be.raw_munmap) (void)be.raw_munmap(be.ctx, rwx, asz);
            }
            const uint64_t rw = be.raw_mmap(be.ctx, addr, asz,
                                            kProtRead | kProtWrite);
            ++detail->attempts;
            if (rw != 0) {
                if (rw >= start && rw <= end - asz) {
                    detail->address = rw;
                    detail->exec_transition_required = 1;
                    ++diag()->alloc_rw_exec_transition;
                    return rw;
                }
                if (be.raw_munmap) (void)be.raw_munmap(be.ctx, rw, asz);
            }
        }
        // Both attempts refused at this hint: advance one page. No unbounded
        // loop; a fully refusing backend returns 0 after kAllocMaxTries.
    }
    return 0;
}

inline void bound_alloc_free_core(const Backend& be, uint64_t address,
                                  uint64_t size) {
    if (!address || !be.raw_munmap) return;
    const uint64_t ps = page_size_or_fallback(be);
    (void)be.raw_munmap(be.ctx, address, (size + (ps - 1)) & ~(ps - 1));
}

}  // namespace plh_prospero

#endif  // POLYHOOK_2_PROSPERO_PLATFORM_HPP
