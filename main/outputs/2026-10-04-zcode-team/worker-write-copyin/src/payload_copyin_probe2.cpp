// payload_copyin_probe2.cpp — worker-write-copyin, 2026-10-06. DISPATCH 2
// (fixed-build retry 1 of 2; build 1 = dispatch 1 raw evidence preserved).
//
// DISPATCH-1 FINDING THIS BUILD RESOLVES: write(2) to real files returned
// -1/errno=EFAULT(14) ADDRESS-INDEPENDENTLY — even from known-readable
// planted mmap(MAP_ANON) pages (volatile-verified) — while the SAME
// process's stdio record appends (fputs -> write -> kernel copyin from
// HEAP buffers into files in the SAME /data directory) landed 12 JSONL
// lines. Together with prior ptrace EPERM at mmap/hole/DM addresses (the
// heap was never tested), the unified hypothesis is a KERNEL SOURCE-REGION
// CLASS ALLOWLIST: kernel-mediated access to user memory is allowed for
// loaded-image/stack/heap regions and refused (EFAULT) for runtime
// mmap(MAP_ANON) regions. /dev/null's vacuous no-copyin success is
// source-independent and does not contradict this.
//
// THIS BUILD: plant identical 256-byte positional patterns in FOUR source
// classes, write each to its own real file under /data (expect rc==256 AND
// byte-true contents — THE positive proof per class), then run the
// refusal ladder (munmap hole, PROT_NONE page, DM-class, high unmapped)
// from the FIRST class that proved byte-true (priority: bss, stack, heap).
//
//   c1  static g_buf[256]  (.bss of the loaded payload image)
//   c2  stack char[256]    (main frame)
//   c3  malloc(256)        (libc heap)
//   m1  mmap(MAP_ANON) region (control: expect EFAULT per dispatch 1)
//   d1  write(/dev/null, mmap src, 8)   (control: expect rc=8 vacuous,
//        replicating worker-readable-source candidate 4 in this same run)
//   d2  write(/dev/null, static src, 8) (vacuousness is source-independent)
//   then, from the first positive class:
//   bh2 hole 0x1000-wide (munmap)      expect -1/errno=EFAULT (or raw 14),
//   bn2 PROT_NONE page (mprotect on the never-written fresh mmap page 3;
//        dispatch 1 CONSTRUCTED it rc=0 — keep that design), file empty
//   bd2 DM-class 0x22C0000000          raw recorded
//   bhi2 high unmapped 0x4000000000    expect EFAULT, file empty
//
// Conventions (dispatch-1 PROOF): this target's write wrapper is the
// -1/errno convention (fresh errno captured; errno was 0 before every call
// and 14 after a refused write). Both conventions still classified.
//
// Verdict law: WORKS iff some class proves byte-true success AND hole AND
// high-unmapped refusals from that class with empty files;
// WORKS_ERROR_MAP_NOTE if refusals are non-success non-EFAULT with empty
// files; DEAD if no class proves the positive; INCONCLUSIVE if setup fails.
// Decisive-vs-works gating separate, every field defaulted+assigned, fresh
// errno per call, ONE mmap only, per-check JSONL lines appended immediately.
//
// Bounded: one process run, <= ~12 lines. NO game contact, no ploader.ini,
// no game files; only this payload's own /data/modbridge-copyin2* files.

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifdef HOST_COPYIN2
#define TARGET_NAME "hostmac-logic-only"
#else
#define TARGET_NAME "prospero"
#endif

namespace {

constexpr uint64_t kHighUnmapped = 0x4000000000ull;
constexpr uint64_t kDmAddress    = 0x22C0000000ull;
constexpr size_t   kRegionBytes  = 0x4000;
constexpr long long kNotMade = LLONG_MIN;

inline uint8_t pat_a(size_t i) { return (uint8_t)('A' + (i % 26)); }

struct Call { long long rc = kNotMade; int en = -1; };

unsigned char g_buf[256];            // c1 source: payload-image .bss

const char* classify_write(long long rc, long long len, int en)
{
    if (rc == len && len > 0) return "success-full";
    if (rc >= 0 && rc < len) return "ambiguous-partial-or-raw-err";
    if (rc == -1) return "wrapper-error-errno";
    if (rc > len && rc < 256) return "raw-error-code";
    (void)en;
    return "other";
}

const char* code_name(long long code)
{
    switch (code) {
        case 1:  return "EPERM";
        case 5:  return "EIO";
        case 9:  return "EBADF";
        case 13: return "EACCES";
        case 14: return "EFAULT";
        case 22: return "EINVAL";
        default: return "code-other";
    }
}

struct Fields {
    std::string text;
    bool any_ = false;
    void sep() { if (any_) text += ","; any_ = true; }
    static std::string esc(const char* s) {
        std::string out;
        for (const char* p = s; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
            else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
            else out += (char)c;
        }
        return out;
    }
    static std::string num(long long v) { char b[32]; std::snprintf(b, sizeof b, "%lld", v); return b; }
    void n(const char* k, long long v) { sep(); text += "\""; text += k; text += "\":"; text += num(v); }
    void s(const char* k, const char* v) { sep(); text += "\""; text += k; text += "\":\""; text += esc(v); text += "\""; }
    void b(const char* k, bool v) { sep(); text += "\""; text += k; text += "\":"; text += v ? "true" : "false"; }
    void addr(const char* k, uint64_t v) { sep(); text += "\""; text += k; text += "\":\"0x"; 
        char b[32]; std::snprintf(b, sizeof b, "%llx", (unsigned long long)v); text += b; text += "\""; }
    void raw(const char* k, const std::string& rendered) { sep(); text += "\""; text += k; text += "\":"; text += rendered; }
    void call(const char* k, const Call& c) {
        sep(); text += "\""; text += k; text += "\":{\"rc\":";
        text += (c.rc == kNotMade ? std::string("null") : num(c.rc));
        text += ",\"errno\":"; text += num(c.en); text += "}";
    }
};

std::string make_line(const char* check_id, bool decisive, const Fields& d)
{
    std::string line = "{\"marker\":\"modbridge.copyin2\",\"schema\":2,\"target\":\"" TARGET_NAME;
    line += "\",\"check\":\""; line += check_id;
    line += "\",\"decisive\":"; line += decisive ? "true" : "false";
    line += ",\"detail\":{"; line += d.text; line += "}}\n";
    return line;
}

void append_line(const std::string& line, const char* sink)
{
    if (std::FILE* f = std::fopen(sink, "a")) {
        std::fputs(line.c_str(), f);   // stdio append: the per-run CONTROL that
        std::fclose(f);                // copyin-from-heap works (dispatch-1 fact)
    }
}

std::string hex_bytes(const unsigned char* p, long long n)
{
    static const char* d = "0123456789abcdef";
    std::string out;
    for (long long i = 0; i < n; ++i) {
        out += d[(p[i] >> 4) & 0xF];
        out += d[p[i] & 0xF];
    }
    return out;
}

struct Readback {
    Call open_call;
    long long total = -1;
    int read_err = -1;
    bool cmp_equal = false;
    bool empty = false;
    bool authoritative = false;
    std::string content_hex;   // full content, cap 64 B
};

Readback readback_file(const char* path, const unsigned char* expect, long long expect_n)
{
    Readback rb;
    errno = 0;
    rb.open_call.rc = ::open(path, O_RDONLY);
    rb.open_call.en = errno;
    if (rb.open_call.rc < 0) return rb;
    static unsigned char buf[4097];
    long long total = 0;
    int last_err = 0;
    for (;;) {
        errno = 0;
        const long long r = (long long)::read(rb.open_call.rc, buf + total,
                                              (size_t)(4097 - total));
        last_err = errno;
        if (r < 0) { total = -1; break; }
        if (r == 0) break;
        total += r;
        if (total > 4096) break;
    }
    rb.read_err = last_err;
    rb.total = total;
    rb.authoritative = (total >= 0);
    if (total > 0) {
        const long long full = total < 64 ? total : 64;
        rb.content_hex = hex_bytes(buf, full);
    }
    rb.empty = (total == 0);
    if (expect && expect_n >= 0 && total == expect_n) {
        rb.cmp_equal = (std::memcmp(buf, expect, (size_t)expect_n) == 0);
    }
    ::close(rb.open_call.rc);
    return rb;
}

struct CaseResult {
    const char* id = "unset";
    const char* sink = "unset";
    const char* src_class = "unset";
    Call open_call, w, close_call;
    long long len = -1;
    Readback rb;
    const char* wclass = "not-made";
    const char* wcode = "not-made";
    bool open_ok = false;
};

CaseResult run_case(const char* id, const char* sink_path, const char* src_class,
                    const void* src, long long len,
                    const unsigned char* expect, long long expect_n)
{
    CaseResult r;
    r.id = id;
    r.sink = sink_path;
    r.src_class = src_class;
    r.len = len;
    errno = 0;
    r.open_call.rc = ::open(sink_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    r.open_call.en = errno;
    r.open_ok = (r.open_call.rc >= 0);
    if (r.open_ok) {
        errno = 0;
        r.w.rc = (long long)::write(r.open_call.rc, src, (size_t)len);
        r.w.en = errno;
        errno = 0;
        r.close_call.rc = ::close(r.open_call.rc);
        r.close_call.en = errno;
        r.wclass = classify_write(r.w.rc, r.len, r.w.en);
        r.wcode = (r.wclass == std::string("success-full"))
                      ? "none" : code_name(r.w.rc == -1 ? r.w.en : r.w.rc);
    } else {
        r.wclass = "open-failed";
        r.wcode = code_name(r.open_call.en);
    }
    r.rb = readback_file(sink_path, expect, expect_n);
    return r;
}

void case_fields(Fields& d, const CaseResult& r, uint64_t src_addr)
{
    d.s("case", r.id);
    d.s("sink", r.sink);
    d.s("src_class", r.src_class);
    d.addr("src", src_addr);
    d.n("len", r.len);
    d.call("open", r.open_call);
    d.call("write", r.w);
    d.call("close", r.close_call);
    d.s("class", r.wclass);
    d.s("code", r.wcode);
    d.call("rb_open", r.rb.open_call);
    d.n("rb_total", r.rb.total);
    d.n("rb_read_errno", r.rb.read_err);
    d.b("rb_authoritative", r.rb.authoritative);
    d.b("rb_empty", r.rb.empty);
    d.b("rb_cmp_equal", r.rb.cmp_equal);
    if (!r.rb.content_hex.empty()) {
        d.raw("rb_content_head64", "\"" + r.rb.content_hex + "\"");
    }
}

bool case_positive(const CaseResult& r)
{
    return r.open_ok && r.wclass == std::string("success-full")
        && r.rb.authoritative && r.rb.cmp_equal;
}

bool case_refused_empty(const CaseResult& r)
{
    return r.open_ok && r.wclass != std::string("success-full")
        && r.rb.authoritative && r.rb.empty;
}

bool faulted_eight(const CaseResult& r)  // len-8 case: unambiguous EFAULT?
{
    const bool wrapper = (r.w.rc == -1 && r.w.en == 14);
    const bool raw     = (r.w.rc == 14);
    return (wrapper || raw) && case_refused_empty(r);
}

struct Setup {
    Call mmap_call, munmap_call, mprotect_call;
    uint64_t base = 0, page_none = 0, hole = 0;
    bool mapped = false, hole_ok = false, none_constructed = false;
    bool m_back = false;   // mmap page0 volatile-verified readable (control)
};

Setup do_setup()
{
    Setup t;
    errno = 0;
    t.mmap_call.rc = (long long)::mmap(nullptr, kRegionBytes,
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANON, -1, 0);
    t.mmap_call.en = errno;
    if (t.mmap_call.rc < 0) return t;
    t.mapped = true;
    unsigned char* base = (unsigned char*)(uintptr_t)t.mmap_call.rc;
    t.base = (uint64_t)base;
    t.hole = t.base + 0x2000;
    t.page_none = t.base + 0x3000;
    for (size_t i = 0; i < 64; ++i) base[i] = pat_a(i);   // page0 planted (control)
    t.m_back = true;
    for (size_t j = 0; j < 64; ++j) {
        if (*(const volatile uint8_t*)(base + j) != pat_a(j)) t.m_back = false;
    }
    errno = 0;
    t.munmap_call.rc = ::munmap(base + 0x2000, 0x1000);
    t.munmap_call.en = errno;
    t.hole_ok = (t.munmap_call.rc == 0);
    errno = 0;
    t.mprotect_call.rc = ::mprotect(base + 0x3000, 0x1000, PROT_NONE);  // page3 never written
    t.mprotect_call.en = errno;
    t.none_constructed = (t.mprotect_call.rc == 0);
    return t;
}

}  // namespace

int main(int argc, char** argv)
{
    const char* sink = argc > 1 ? argv[1] : "/data/modbridge-copyin2.jsonl";
    bool all_decisive = true;

    const Setup t = do_setup();
    {
        Fields d;
        d.call("mmap", t.mmap_call);
        d.addr("base", t.base);
        d.addr("hole", t.hole);
        d.addr("page_none", t.page_none);
        d.call("munmap", t.munmap_call);
        d.call("mprotect_protnone", t.mprotect_call);
        d.b("mapped", t.mapped);
        d.b("mmap_page0_readable", t.m_back);
        d.b("hole_ok", t.hole_ok);
        d.b("none_constructed", t.none_constructed);
        d.s("note", "ONE mmap only; page3 never written before mprotect (dispatch-1 construction design kept)");
        const bool decisive = t.mapped && t.m_back && t.hole_ok;
        all_decisive = all_decisive && decisive;
        append_line(make_line("0_setup", decisive, d), sink);
    }

    // Plant the same positional pattern in three allowed-region classes.
    unsigned char stack_buf[256];
    unsigned char* heap_buf = (unsigned char*)std::malloc(256);
    unsigned char exp[256];
    for (int j = 0; j < 256; ++j) {
        g_buf[j] = pat_a((size_t)j);
        stack_buf[j] = pat_a((size_t)j);
        if (heap_buf) heap_buf[j] = pat_a((size_t)j);
        exp[j] = pat_a((size_t)j);
    }
    const bool heap_ok = (heap_buf != nullptr);

    // Positive ladder: bss, stack, heap, then the mmap control (expect EFAULT).
    const CaseResult c1 = run_case("c1_bss_256", "/data/modbridge-copyin2-c1.bin",
                                   "image-bss", g_buf, 256, exp, 256);
    const CaseResult c2 = run_case("c2_stack_256", "/data/modbridge-copyin2-c2.bin",
                                   "stack", stack_buf, 256, exp, 256);
    const CaseResult c3 = run_case("c3_heap_256", "/data/modbridge-copyin2-c3.bin",
                                   heap_buf ? "malloc-heap" : "malloc-heap-UNAVAILABLE-bss-src", heap_buf ? (const void*)heap_buf : (const void*)g_buf,
                                   256, exp, 256);
    const CaseResult m1 = run_case("m1_mmap_anon_256", "/data/modbridge-copyin2-m1.bin",
                                   "mmap-anon", (const void*)(uintptr_t)t.base, 256,
                                   exp, 256);
    for (const CaseResult* r : {&c1, &c2, &c3, &m1}) {
        const bool decisive = r->open_ok && r->rb.authoritative;
        all_decisive = all_decisive && decisive;
        Fields d;
        const uint64_t src_addr = (r == &c1) ? (uint64_t)(uintptr_t)g_buf
                                : (r == &c2) ? (uint64_t)(uintptr_t)stack_buf
                                : (r == &c3) ? (uint64_t)(uintptr_t)(heap_buf ? heap_buf : g_buf)
                                : t.base;
        case_fields(d, *r, src_addr);
        append_line(make_line(r->id, decisive, d), sink);
    }

    // /dev/null controls (vacuousness; no readback file).
    {
        Fields d;
        errno = 0;
        const int dn = ::open("/dev/null", O_WRONLY);
        const int dn_err = errno;
        d.n("devnull_open_rc", dn);
        d.n("devnull_open_errno", dn_err);
        if (dn >= 0) {
            errno = 0;
            const long long r1 = (long long)::write(dn, (const void*)(uintptr_t)t.base, 8);
            const int e1 = errno;
            errno = 0;
            const long long r2 = (long long)::write(dn, g_buf, 8);
            const int e2 = errno;
            d.call("devnull_mmap_src", Call{r1, e1});
            d.call("devnull_bss_src", Call{r2, e2});
            ::close(dn);
        }
        d.s("note", "prior evidence: /dev/null write path performs NO copyin (vacuous)");
        append_line(make_line("d_devnull_controls", true, d), sink);
    }

    // Refusal ladder from the FIRST positive allowed class.
    const bool c1_ok = case_positive(c1);
    const bool c2_ok = case_positive(c2);
    const bool c3_ok = case_positive(c3) && heap_ok;
    const char* ladder_class = "none";
    const void* ladder_src = nullptr;
    uint64_t ladder_addr = 0;
    if (c1_ok) { ladder_class = "image-bss"; ladder_src = g_buf; ladder_addr = (uint64_t)(uintptr_t)g_buf; }
    else if (c2_ok) { ladder_class = "stack"; ladder_src = stack_buf; ladder_addr = (uint64_t)(uintptr_t)stack_buf; }
    else if (c3_ok) { ladder_class = "malloc-heap"; ladder_src = heap_buf; ladder_addr = (uint64_t)(uintptr_t)heap_buf; }

    const CaseResult bh2 = run_case("bh2_hole_8", "/data/modbridge-copyin2-bh2.bin",
                                    ladder_class, ladder_src, 8, nullptr, 0);
    const CaseResult bn2 = run_case("bn2_protnone_8", "/data/modbridge-copyin2-bn2.bin",
                                    ladder_class, ladder_src, 8, nullptr, 0);
    const CaseResult bd2 = run_case("bd2_dmclass_8", "/data/modbridge-copyin2-bd2.bin",
                                    ladder_class, (const void*)kDmAddress, 8, nullptr, 0);
    const CaseResult bhi2 = run_case("bhi2_highunmapped_8", "/data/modbridge-copyin2-bhi2.bin",
                                     ladder_class, (const void*)kHighUnmapped, 8, nullptr, 0);
    for (const CaseResult* r : {&bh2, &bn2, &bd2, &bhi2}) {
        const bool decisive = r->open_ok && r->rb.authoritative;
        all_decisive = all_decisive && decisive;
        Fields d;
        const uint64_t rsrc = (r == &bd2) ? kDmAddress
                            : (r == &bhi2) ? kHighUnmapped : ladder_addr;
        case_fields(d, *r, rsrc);
        if (r == &bh2 || r == &bn2) {
            // hole/PROT_NONE refusals are only meaningful when ladder_src is a
            // REAL readable source; when no class proved positive we write
            // FROM NULL — record the gate honestly.
            d.s("gate", ladder_src ? "refusal from first positive class" : "NO positive class: src null, refusal cases vacuous");
        }
        if (r == &bn2 && !t.none_constructed) {
            d.s("gate2", "PROT_NONE not constructed this run; conclusions gated off");
        }
        append_line(make_line(r->id, decisive, d), sink);
    }

    // Verdict.
    {
        const bool positives_ok = (c1_ok || c2_ok || c3_ok);
        const bool ladder_ran = (ladder_src != nullptr);
        const bool hole_efault = ladder_ran && faulted_eight(bh2);
        const bool high_efault = ladder_ran && faulted_eight(bhi2);
        const bool b_refused = ladder_ran && case_refused_empty(bh2) &&
                               case_refused_empty(bhi2) && case_refused_empty(bd2) &&
                               (!t.none_constructed || case_refused_empty(bn2));

        const char* verdict = "INCONCLUSIVE";
        std::string note = "unreached";
        if (!t.mapped || !t.hole_ok) {
            verdict = "INCONCLUSIVE";
            note = "setup failed; no verdict either way";
        } else if (positives_ok && hole_efault && high_efault) {
            verdict = "WORKS";
            note = "byte-true positive from an allowed region class + EFAULT refusals (hole, high) with empty files";
        } else if (positives_ok && b_refused) {
            verdict = "WORKS_ERROR_MAP_NOTE";
            note = "byte-true positive + non-success refusals with empty files; error map correction needed from recorded codes";
        } else if (!positives_ok) {
            verdict = "DEAD";
            note = "no region class proved the positive write; write-copyin policy-dead in this context (stdio contradiction noted from dispatch 1)";
        } else {
            verdict = "INCONCLUSIVE";
            note = "positive ok but refusals neither EFAULT nor refused-empty; inspect raw records";
        }

        Fields d;
        d.b("c1_bss_positive", c1_ok);
        d.b("c2_stack_positive", c2_ok);
        d.b("c3_heap_positive", c3_ok);
        d.b("m1_mmap_positive", case_positive(m1));
        d.b("positives_ok", positives_ok);
        d.s("ladder_class", ladder_class);
        d.b("ladder_ran", ladder_ran);
        d.b("hole_efault", hole_efault);
        d.b("high_efault", high_efault);
        d.b("b_refused_empty", b_refused);
        d.b("protnone_constructed", t.none_constructed);
        d.s("verdict", verdict);
        d.s("note", note.c_str());
        append_line(make_line("v_verdict", true, d), sink);
    }

    std::free(heap_buf);
    return all_decisive ? 0 : 1;
}
