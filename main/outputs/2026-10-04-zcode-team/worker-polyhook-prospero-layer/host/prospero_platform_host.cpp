// prospero_platform_host.cpp — worker-polyhook-prospero-layer host tests.
//
// Exercises the platform header's pure core with MOCK backends and synthetic
// KERN_PROC_VMMAP buffers (the proven fixture pattern of
// worker-anchor-validation/tests/vmmap_host.c: byte-assembled records with
// variable kve_structsize, including the exact live spans from the igf
// lineage), plus the REAL polyhook2/MemProtector.hpp RAII consumer compiled
// on the host to prove the UNSET-refusal -> destructor-skips-restore chain.
//
// No kernel call, no game memory, no link of PLH platform branches (those
// are compile-gated separately under the composition argv).

#include "polyhook2/PolyHookProsperoPlatform.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

// Real PLH RAII consumer (platform-independent class; on host it resolves to
// the APPLE OS selection, which only affects PolyHookOsIncludes, not this
// header's logic).
#include "polyhook2/MemProtector.hpp"
#include "polyhook2/Enums.hpp"

// Host-link stubs for the MemAccessor base virtuals. On __PROSPERO__ these
// four are defined by PolyHookProsperoPlatform.cpp (the missing verified
// contract this worker supplies); on the host the Apple definitions are not
// linked, so provide never-called fail-closed stubs (the definitions must
// match the header signatures exactly — that is the compile-time proof).
bool PLH::MemAccessor::mem_copy(uint64_t, uint64_t, uint64_t) const {
    return false;
}
bool PLH::MemAccessor::safe_mem_write(uint64_t, uint64_t, uint64_t,
                                      size_t&) const noexcept {
    return false;
}
bool PLH::MemAccessor::safe_mem_read(uint64_t, uint64_t, uint64_t,
                                     size_t&) const noexcept {
    return false;
}
PLH::ProtFlag PLH::MemAccessor::mem_protect(uint64_t, uint64_t, PLH::ProtFlag,
                                            bool& status) const {
    status = false;
    return PLH::ProtFlag::UNSET;
}

using namespace plh_prospero;

static int failures = 0;
#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (cond) { printf("ok   %s\n", msg); }                            \
        else { printf("FAIL %s\n", msg); ++failures; }                     \
    } while (0)

// ---------------------------------------------------------------------------
// Synthetic kinfo_vmentry buffer builder (vmmap_host.c pattern).
// ---------------------------------------------------------------------------
static void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void wr64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)((v >> (8 * i)) & 0xff);
}
static size_t put_rec(uint8_t* buf, size_t off, uint64_t start, uint64_t end,
                      uint32_t prot, uint32_t ksz) {
    memset(buf + off, 0, ksz);
    wr32(buf + off, ksz);
    wr64(buf + off + 8, start);
    wr64(buf + off + 16, end);
    wr32(buf + off + 56, prot);
    return off + ksz;
}

// ---------------------------------------------------------------------------
// Mock backend: fixture map + recorded calls.
// ---------------------------------------------------------------------------
struct MockState {
    std::vector<uint8_t> map;   // raw kinfo_vmentry bytes collect_vmmap returns
    int collect_rc = 0;
    // raw_mprotect recording:
    int mprotect_rc = 0;
    int mprotect_calls = 0;
    uint64_t mprotect_addr = 0, mprotect_len = 0;
    int mprotect_prot = -1;
    // raw_mmap recording:
    uint64_t mmap_addr = 0, mmap_len = 0;
    int mmap_prot = -1;
    int mmap_mode = 0;  // 0=grant RWX at exact hint; 1=refuse RWX, grant RW;
                        // 2=always refuse; 3=grant but outside bounds
    int mmap_calls = 0;
    int munmap_calls = 0;
    uint64_t page_sz = 16384;
};

static MockState g_mock;

static int mock_collect(void*, uint8_t* buf, uint32_t cap, uint64_t* len) {
    (void)cap;
    if (g_mock.collect_rc != 0) return g_mock.collect_rc;
    memcpy(buf, g_mock.map.data(), g_mock.map.size());
    *len = g_mock.map.size();
    return 0;
}
static int mock_mprotect(void*, uint64_t addr, uint64_t len, int prot) {
    ++g_mock.mprotect_calls;
    g_mock.mprotect_addr = addr;
    g_mock.mprotect_len = len;
    g_mock.mprotect_prot = prot;
    diag()->last_mprotect_raw = (uint64_t)g_mock.mprotect_rc;  // backend contract
    return g_mock.mprotect_rc;
}
static uint64_t mock_mmap(void*, uint64_t hint, uint64_t len, int prot) {
    ++g_mock.mmap_calls;
    g_mock.mmap_addr = hint;
    g_mock.mmap_len = len;
    g_mock.mmap_prot = prot;
    switch (g_mock.mmap_mode) {
    case 0: return hint;                       // RWX granted at hint
    case 1:                                    // RWX refused, RW granted
        return (prot & kProtExec) ? 0 : hint;
    case 2: return 0;                          // everything refused
    default: return hint + (1ull << 32);       // kernel ignored the hint
    }
}
static int mock_munmap(void*, uint64_t, uint64_t) {
    ++g_mock.munmap_calls;
    return 0;
}
static uint64_t mock_page_size(void*) { return g_mock.page_sz; }

static Backend make_mock() {
    Backend b;
    b.ctx = nullptr;
    b.collect_vmmap = &mock_collect;
    b.raw_mprotect = &mock_mprotect;
    b.raw_mmap = &mock_mmap;
    b.raw_munmap = &mock_munmap;
    b.page_size = &mock_page_size;
    return b;
}

// The exact live spans of the igf lineage (EXEC-WRITABILITY-EVIDENCE §1):
// exec-only text [0x400000,0x787c000) prot=4, RW data [0x970c000,0x9a34000)
// prot=3, one 36-bit R-only span, x3 repetitions to pass the >=8 trust gate.
static void build_live_map(uint8_t* buf, size_t* len, uint32_t ksz = 144) {
    size_t off = 0;
    for (int i = 0; i < 3; ++i) {
        off = put_rec(buf, off, 0x400000ull, 0x787c000ull, 4, ksz);
        off = put_rec(buf, off, 0x970c000ull, 0x9a34000ull, 3, ksz);
        off = put_rec(buf, off, 0x500000090ull, 0x510000090ull, 1, ksz);
    }
    *len = off;
}

static uint8_t scratch[kVmmapScratchCap];
static VmSpan spans[16384];

static void reset_diag() {
    memset(diag(), 0, sizeof(Diag));
}

// ---------------------------------------------------------------------------
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);  // survive hang forensics
    // ---- 1. Protection translation (CLAIM-2) ------------------------------
    CHECK(translate_plh_to_prot(kPlhX) == 4, "X -> PROT_EXEC");
    CHECK(translate_plh_to_prot(kPlhR) == 1, "R -> PROT_READ");
    CHECK(translate_plh_to_prot(kPlhW) == 2, "W -> PROT_WRITE");
    CHECK(translate_plh_to_prot(kPlhR | kPlhW) == 3, "R|W -> 3");
    CHECK(translate_plh_to_prot(kPlhRwx) == 7, "RWX -> 7");
    CHECK(translate_plh_to_prot(kPlhNone) == 0, "NONE -> PROT_NONE");
    CHECK(translate_plh_to_prot(kPlhUnset) == 0, "UNSET -> PROT_NONE");
    CHECK(translate_prot_to_plh(4) == kPlhX, "PROT_EXEC -> X");
    CHECK(translate_prot_to_plh(3) == (kPlhR | kPlhW), "3 -> R|W");
    CHECK(translate_prot_to_plh(7) == kPlhRwx, "7 -> RWX");
    CHECK(translate_prot_to_plh(0) == kPlhNone, "PROT_NONE -> NONE");
    CHECK(translate_prot_to_plh(1) == kPlhR, "1 -> R");
    const unsigned combos[] = {kPlhX, kPlhR, kPlhW, kPlhR | kPlhW, kPlhRwx,
                               kPlhNone};
    int rt_ok = 1;
    for (unsigned c : combos)
        if (translate_prot_to_plh(translate_plh_to_prot(c)) != c) rt_ok = 0;
    CHECK(rt_ok, "round-trip translate(translate(f)) == f for PLH combos");

    // ---- 2. Walk + trust on the exact live map (CLAIM-3) ------------------
    uint8_t mapbuf[kVmmapScratchCap];
    size_t mlen = 0;
    build_live_map(mapbuf, &mlen);
    WalkStats st;
    const uint32_t kept = vmmap_walk(mapbuf, mlen, 1160, spans, 16384, &st);
    CHECK(kept == 9, "mode-1 walk keeps 9 sane records");
    CHECK(st.total == 9 && st.retained == 9 && st.insane == 0,
          "walk stats 9/9/0");
    CHECK(st.stop_reason == 0, "clean end-of-buffer stop");
    CHECK(st.ksz_144 == 9, "all structsizes 144 (P1 prediction)");
    CHECK(st.aligned16k == 6,
          "6 of 9 records 16-KiB aligned (36-bit span is unaligned)");
    CHECK(spans[0].start == 0x400000ull && spans[0].end == 0x787c000ull &&
              spans[0].prot == 4,
          "span 0 exact: live exec span, prot=4");
    CHECK(spans[1].start == 0x970c000ull && spans[1].end == 0x9a34000ull &&
              spans[1].prot == 3,
          "span 1 exact: live RW data span, prot=3");
    CHECK(vmmap_trustworthy(st) == 1, "trust gate passes on live map");

    // Variable structsize mix (path-truncated records): still walks.
    size_t vlen = 0;
    for (int i = 0; i < 4; ++i) {
        vlen = put_rec(mapbuf, vlen, 0x1000000ull + i * 0x100000ull,
                       0x1000000ull + i * 0x100000ull + 0x40000ull, 5, 144);
        vlen = put_rec(mapbuf, vlen, 0x2000000ull + i * 0x100000ull,
                       0x2000000ull + i * 0x100000ull + 0x40000ull, 6, 1160);
    }
    WalkStats stv;
    const uint32_t keptv = vmmap_walk(mapbuf, vlen, 1160, spans, 16384, &stv);
    CHECK(keptv == 8 && stv.ksz_full == 4 && stv.ksz_var == 4,
          "variable kve_structsize mix: 144 and 1160 records both kept");
    CHECK(vmmap_trustworthy(stv) == 1, "mixed map trusted");

    // Insane structsize stops the walk, never guesses (v1/v2 corruption fix).
    vlen = 0;
    for (int i = 0; i < 9; ++i)
        vlen = put_rec(mapbuf, vlen, 0x1000000ull, 0x1040000ull, 5, 144);
    wr32(mapbuf + vlen, 7);  // structsize 7 < header: insane
    WalkStats sti;
    (void)vmmap_walk(mapbuf, vlen + kVmHdr, 1160, spans, 16384, &sti);
    CHECK(sti.stop_reason == 1 && sti.retained == 9,
          "insane structsize: stop with retained prefix intact");
    CHECK(vmmap_trustworthy(sti) == 1,
          "9 sane records still pass trust (prefix map usable)");

    // ---- 3. mem_protect core: success + old-prot lookup (CLAIM-3/4) -------
    Backend be = make_mock();
    reset_diag();
    g_mock = MockState();
    build_live_map(mapbuf, &mlen);
    memcpy(scratch, mapbuf, mlen);
    g_mock.map.assign(mapbuf, mapbuf + mlen);
    set_active_backend(&be);

    int status = 0;
    const unsigned old = mem_protect_core(be, scratch, kVmmapScratchCap,
                                          spans, 16384, 0x33ac8c0ull, 20,
                                          kPlhRwx, &status);
    CHECK(status == 1, "covered exec-span request: status set");
    CHECK(old == kPlhX, "old-prot lookup returns X for the exec span");
    CHECK(g_mock.mprotect_calls == 1 && g_mock.mprotect_rc == 0,
          "exactly one raw_mprotect call");
    CHECK(g_mock.mprotect_addr == 0x33ac000ull,
          "dest page-rounded DOWN to 16-KiB page");
    CHECK(g_mock.mprotect_len == 0x4000ull, "size rounded UP to one page");
    CHECK(g_mock.mprotect_prot == 7, "RWX request translated to prot 7");

    // RW data span: old prot R|W; restore request goes back with 3.
    g_mock.mprotect_calls = 0;
    const unsigned oldrw = mem_protect_core(be, scratch, kVmmapScratchCap,
                                            spans, 16384, 0x9800000ull,
                                            0x10, kPlhR | kPlhW, &status);
    CHECK(status == 1 && oldrw == (kPlhR | kPlhW),
          "RW data span: old prot R|W");
    CHECK(g_mock.mprotect_prot == 3, "restore request translates to prot 3");

    // ---- 4. Refusals, all fail-closed (CLAIM-4) ---------------------------
    // (a) aligned request falls in a HOLE between spans -> refuse, no
    // syscall. (16-KiB alignment means an aligned request either sits inside
    // one 16-KiB-aligned span or crosses free space — both must refuse.)
    g_mock.mprotect_calls = 0;
    status = 1;
    (void)mem_protect_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x787cff0ull, 0x20, kPlhRwx, &status);
    CHECK(status == 0 && g_mock.mprotect_calls == 0,
          "hole-crossing request refused BEFORE any kernel call");
    // (b) uncovered address -> refuse.
    status = 1;
    (void)mem_protect_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0xdead000ull, 8, kPlhRwx, &status);
    CHECK(status == 0, "uncovered address refused");
    // (c) untrusted map -> refuse. A 2-sane-record map fails the proven
    // gate (retained >= 8 required).
    size_t blen = 0;
    blen = put_rec(mapbuf, blen, 0x1000000ull, 0x1040000ull, 5, 144);
    blen = put_rec(mapbuf, blen, 0x2000000ull, 0x2040000ull, 6, 144);
    WalkStats bad;
    (void)vmmap_walk(mapbuf, blen, 1160, spans, 16384, &bad);
    CHECK(vmmap_trustworthy(bad) == 0, "trust gate rejects 2-record map");
    g_mock.map.assign(mapbuf, mapbuf + blen);
    status = 1;
    (void)mem_protect_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x1000000ull, 8, kPlhRwx, &status);
    CHECK(status == 0, "untrusted map: refusal, no mprotect");
    // (d) backend collect failure -> refuse.
    g_mock.map.assign(mapbuf, mapbuf + mlen);
    g_mock.collect_rc = -1;
    status = 1;
    (void)mem_protect_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x400000ull, 8, kPlhRwx, &status);
    CHECK(status == 0, "collect failure: refusal, no mprotect");
    g_mock.collect_rc = 0;
    // (e) raw transition observed non-zero -> refused as UNSET.
    g_mock.mprotect_rc = 13;
    status = 1;
    const unsigned failed_old = mem_protect_core(
        be, scratch, kVmmapScratchCap, spans, 16384, 0x400000ull, 8, kPlhRwx,
        &status);
    CHECK(status == 0 && failed_old == kPlhUnset,
          "non-zero transition result: status 0 and UNSET (no restore)");
    CHECK(diag()->last_mprotect_raw == 13, "raw result word surfaced in Diag");
    g_mock.mprotect_rc = 0;
    // (f) truncated map -> refuse.
    status = 1;
    (void)mem_protect_core(be, scratch, kVmmapScratchCap, spans, 4 /*cap*/,
                           0x400000ull, 8, kPlhRwx, &status);
    CHECK(status == 0, "truncated map (span cap exceeded): refusal");

    // ---- 5. PLH MemoryProtector RAII over the core (CLAIM-4) --------------
    // A thin accessor binding the core, exactly the binding the target TU
    // performs (minus the PLH symbol layer, which is PROSPERO-only).
    struct CoreAccessor : public PLH::MemAccessor {
        mutable int protect_calls = 0;
        mutable uint64_t last_restore_flags = 0xffffffffull;
        PLH::ProtFlag mem_protect(uint64_t dest, uint64_t size,
                                  PLH::ProtFlag prot, bool& st) const override {
            ++protect_calls;
            if (protect_calls == 2)  // the destructor's restore call
                last_restore_flags = (uint64_t)prot;
            int s = 0;
            const unsigned oldf = mem_protect_core(
                *active_backend(), scratch, kVmmapScratchCap, spans, 16384,
                dest, size, (unsigned)prot, &s);
            st = s != 0;
            return static_cast<PLH::ProtFlag>(oldf);
        }
        bool mem_copy(uint64_t dest, uint64_t src, uint64_t size) const override {
            return mem_copy_core(*active_backend(), scratch, kVmmapScratchCap,
                                 spans, 16384, dest, src, size);
        }
        bool safe_mem_write(uint64_t dest, uint64_t src, uint64_t size,
                            size_t& written) const noexcept override {
            uint64_t n = 0;
            const bool ok = safe_write_core(*active_backend(), scratch,
                                            kVmmapScratchCap, spans, 16384,
                                            dest, src, size, &n);
            written = (size_t)n;
            return ok;
        }
        bool safe_mem_read(uint64_t src, uint64_t dest, uint64_t size,
                           size_t& read) const noexcept override {
            uint64_t n = 0;
            const bool ok = safe_read_core(*active_backend(), scratch,
                                           kVmmapScratchCap, spans, 16384,
                                           src, dest, size, &n);
            read = (size_t)n;
            return ok;
        }
    };
    {
        g_mock.map.assign(mapbuf, mapbuf + mlen);
        g_mock.mprotect_rc = 0;
        CoreAccessor acc;
        {
            PLH::MemoryProtector prot(0x33ac8c0ull, 20, PLH::ProtFlag::RWX,
                                      acc);
            CHECK(prot.isGood(), "RAII: covered transition isGood");
            CHECK(prot.originalProt() == PLH::ProtFlag::X,
                  "RAII: originalProt is X on the exec span");
        }
        CHECK(acc.protect_calls == 2,
              "RAII: destructor restored exactly once (2 accessor calls)");
        CHECK(acc.last_restore_flags == (unsigned)PLH::ProtFlag::X,
              "RAII: restore requested X (the looked-up old prot)");
    }
    {
        // Uncovered target: ctor refuses with UNSET -> destructor MUST skip
        // the restore (MemProtector.hpp:43) — the fail-closed chain.
        CoreAccessor acc;
        {
            PLH::MemoryProtector prot(0xdead0000ull, 8, PLH::ProtFlag::RWX,
                                      acc);
            CHECK(!prot.isGood(), "RAII: uncovered transition not good");
            CHECK(prot.originalProt() == PLH::ProtFlag::UNSET,
                  "RAII: originalProt UNSET on refusal");
        }
        CHECK(acc.protect_calls == 1,
              "RAII: destructor skipped restore after UNSET refusal");
    }

    // ---- 6. Accessor gates (igf4/igf6 discipline) -------------------------
    // FIXTURE REALISM: any span the gates will actually deref must be backed
    // by REAL host memory (on target the kernel map is true, so an R span
    // deref is safe; here a fake "readable" address would memcpy from
    // unmapped host space). The exec-only span stays at the fake live
    // address on purpose: the gate must refuse BEFORE any deref, and the
    // test asserts exactly that.
    static uint8_t rwbuf[0x20000];   // real RW memory
    static uint8_t robuf[0x10000];   // real memory used as an R-only span
    static uint8_t dstbuf[0x10000];  // real copy destination
    size_t glen = 0;
    for (int i = 0; i < 3; ++i) {
        glen = put_rec(mapbuf, glen, 0x400000ull, 0x787c000ull, 4, 144);
        glen = put_rec(mapbuf, glen, (uint64_t)(uintptr_t)rwbuf,
                       (uint64_t)(uintptr_t)rwbuf + sizeof(rwbuf), 3, 144);
        glen = put_rec(mapbuf, glen, (uint64_t)(uintptr_t)robuf,
                       (uint64_t)(uintptr_t)robuf + sizeof(robuf), 1, 144);
        glen = put_rec(mapbuf, glen, (uint64_t)(uintptr_t)dstbuf,
                       (uint64_t)(uintptr_t)dstbuf + sizeof(dstbuf), 3, 144);
    }
    g_mock.map.assign(mapbuf, mapbuf + glen);
    memset(rwbuf, 0xAB, sizeof(rwbuf));
    memset(robuf, 0xCD, sizeof(robuf));
    uint64_t n = 0;
    // exec-only span (prot=4): read AND write refused (igf6 read-death and
    // igf4 covered_not_readable, encoded as graceful refusals).
    CHECK(!safe_read_core(be, scratch, kVmmapScratchCap, spans, 16384,
                          0x33ac8c0ull, (uint64_t)(uintptr_t)dstbuf, 16, &n) &&
              n == 0,
          "exec-only span: read refused before deref");
    CHECK(!safe_write_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x33ac8c0ull, (uint64_t)(uintptr_t)rwbuf, 16, &n) &&
              n == 0,
          "exec-only span: write refused before deref");
    // RW span: read ok, clamped at span end like the Linux branch.
    const uint64_t rw_end = (uint64_t)(uintptr_t)rwbuf + sizeof(rwbuf);
    memset(dstbuf, 0, 64);
    CHECK(safe_read_core(be, scratch, kVmmapScratchCap, spans, 16384,
                         rw_end - 0x10, (uint64_t)(uintptr_t)dstbuf, 64, &n) &&
              n == 0x10,
          "RW span read clamps to covering span end");
    CHECK(dstbuf[0] == 0xAB && dstbuf[15] == 0xAB,
          "clamped read copied real bytes");
    CHECK(safe_write_core(be, scratch, kVmmapScratchCap, spans, 16384,
                          (uint64_t)(uintptr_t)rwbuf,
                          (uint64_t)(uintptr_t)robuf, 16, &n) &&
              n == 16,
          "RW span write granted (source in R span)");
    // copy: R source -> W dest ok; R -> R dest refused.
    CHECK(mem_copy_core(be, scratch, kVmmapScratchCap, spans, 16384,
                        (uint64_t)(uintptr_t)dstbuf,
                        (uint64_t)(uintptr_t)robuf, 16),
          "copy R-span -> W-span granted");
    CHECK(dstbuf[0] == 0xCD, "copy moved real bytes");
    CHECK(!mem_copy_core(be, scratch, kVmmapScratchCap, spans, 16384,
                         (uint64_t)(uintptr_t)robuf,
                         (uint64_t)(uintptr_t)rwbuf, 16),
          "copy into R-only dest refused");
    // (spans refreshed by each call; restore the live map for later blocks)
    g_mock.map.assign(mapbuf, mapbuf + mlen);

    // ---- 7. Allocator (CLAIM-5) -------------------------------------------
    // Hole map (x3 for the trust gate): exec span, RW gap, data span — the
    // allocator must hint into the gap [0x787c000, 0x970c000).
    size_t hlen = 0;
    for (int i = 0; i < 3; ++i) {
        hlen = put_rec(mapbuf, hlen, 0x400000ull, 0x787c000ull, 4, 144);
        hlen = put_rec(mapbuf, hlen, 0x970c000ull, 0x9a34000ull, 3, 144);
        hlen = put_rec(mapbuf, hlen, 0x500000090ull, 0x510000090ull, 1, 144);
    }
    g_mock.map.assign(mapbuf, mapbuf + hlen);
    g_mock = MockState();
    g_mock.map.assign(mapbuf, mapbuf + hlen);
    g_mock.mmap_mode = 0;
    AllocDetail det;
    const uint64_t got = bound_alloc_core(be, scratch, kVmmapScratchCap,
                                          spans, 16384, 0x8000000ull,
                                          0x9000000ull, 0x1000, &det);
    CHECK(got != 0 && det.used_rwx == 1 &&
              det.exec_transition_required == 0,
          "hole hint granted RWX directly");
    CHECK(g_mock.mmap_addr == 0x8000000ull && g_mock.mmap_prot == 7,
          "first candidate page-aligned at start, prot 7 requested");
    CHECK(g_mock.mmap_len == 0x4000ull, "alloc size rounded to page");

    // RWX refused -> RW + exec_transition_required (the verdict dependency).
    g_mock.mmap_mode = 1;
    g_mock.mmap_calls = 0;
    const uint64_t got2 = bound_alloc_core(be, scratch, kVmmapScratchCap,
                                           spans, 16384, 0x8000000ull,
                                           0x9000000ull, 0x1000, &det);
    CHECK(got2 != 0 && det.used_rwx == 0 &&
              det.exec_transition_required == 1,
          "RWX refusal degrades to RW with exec_transition_required");
    CHECK(g_mock.mmap_calls == 2, "exactly two mmap attempts (RWX then RW)");

    // Everything refused -> 0 after bounded tries. Candidates: 0x8000000 to
    // 0x8ffc000 page-stepped = 1024 candidates x 2 attempts each.
    g_mock.mmap_mode = 2;
    const uint64_t got3 = bound_alloc_core(be, scratch, kVmmapScratchCap,
                                           spans, 16384, 0x8000000ull,
                                           0x9000000ull, 0x1000, &det);
    CHECK(got3 == 0 && det.attempts == 2048,
          "refusing backend: 0 with bounded attempts (2 per candidate)");

    // Kernel ignores hint (returns far address): unmapped, search advances,
    // never returns the out-of-bounds address.
    g_mock.mmap_mode = 3;
    g_mock.munmap_calls = 0;
    const uint64_t got4 = bound_alloc_core(be, scratch, kVmmapScratchCap,
                                           spans, 16384, 0x8000000ull,
                                           0x9000000ull, 0x1000, &det);
    CHECK(got4 == 0 && g_mock.munmap_calls == det.attempts,
          "hint-ignoring kernel: out-of-range results unmapped, 0 returned");

    // Candidate intersecting existing spans never reaches the kernel.
    g_mock = MockState();
    g_mock.map.assign(mapbuf, mapbuf + hlen);
    g_mock.mmap_mode = 0;
    (void)bound_alloc_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x7870000ull, 0x9a00000ull, 0x4000, &det);
    CHECK(g_mock.mmap_addr >= 0x787c000ull,
          "no mmap hint ever intersects an existing span");

    // Degenerate requests.
    CHECK(bound_alloc_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x9000000ull, 0x8000000ull, 0x1000, &det) == 0,
          "end <= start refused");
    CHECK(bound_alloc_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x8000000ull, 0x9000000ull, 0, &det) == 0,
          "zero size refused");
    CHECK(bound_alloc_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x8000000ull, 0x9000000ull, (uint64_t)1 << 63,
                           &det) == 0,
          "overflowing size refused");

    // ---- 8. Page-size handling -------------------------------------------
    g_mock = MockState();
    g_mock.map.assign(mapbuf, mapbuf + hlen);
    g_mock.page_sz = 0;  // sysconf-style failure
    int st2 = 0;
    (void)mem_protect_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x33ac8c1ull, 1, kPlhRwx, &st2);
    CHECK(g_mock.mprotect_addr == 0x33ac000ull && g_mock.mprotect_len == 0x4000,
          "page_size failure falls back to pinned 0x4000");
    g_mock.page_sz = 4096;
    (void)mem_protect_core(be, scratch, kVmmapScratchCap, spans, 16384,
                           0x33ac8c1ull, 1, kPlhRwx, &st2);
    CHECK(g_mock.mprotect_addr == 0x33ac000ull && g_mock.mprotect_len == 0x1000,
          "valid backend page size honored (4-KiB host shape)");

    // ---- 9. exec_publish core routes through the same path ----------------
    // (address inside the exec span of the map left by section 8)
    g_mock.mprotect_rc = 0;
    CHECK(exec_publish_core(be, scratch, kVmmapScratchCap, spans, 16384,
                            0x33ac8c0ull, 0x4000, kPlhRwx) == 1,
          "exec_publish reports success through the verdict-gated path");
    g_mock.mprotect_rc = -1;
    CHECK(exec_publish_core(be, scratch, kVmmapScratchCap, spans, 16384,
                            0x33ac8c0ull, 0x4000, kPlhRwx) == 0,
          "exec_publish reports refusal when the kernel refuses");

    printf("\n%s (%d failures)\n", failures == 0 ? "ALL PASS" : "FAILURES",
           failures);
    return failures == 0 ? 0 : 1;
}
