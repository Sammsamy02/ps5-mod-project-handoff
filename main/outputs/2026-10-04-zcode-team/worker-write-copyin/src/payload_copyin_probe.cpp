// payload_copyin_probe.cpp — worker-write-copyin, 2026-10-06.
//
// Standalone payload probing WRITE(2) KERNEL COPYIN to REAL OWNED FILES on
// the PROSPERO target. The kernel's copyin of the user source buffer IS the
// read: on an unreadable/unmapped source page the kernel returns an error
// (EFAULT expected, raw rc convention: POSITIVE code, errno UNSET — proven
// by worker-readable-source); on a readable page the bytes land in the file,
// so one write(2) is simultaneously the readability PROOF and the OWNED READ.
// This is the one remaining candidate after worker-readable-source exhausted
// ptrace (EPERM policy-dead), /proc/self/mem (absent), /dev/null
// write-copyin (VACUOUS: no copyin performed), process_vm_readv/memfd
// (absent from the surface).
//
// Per-case files (each O_WRONLY|O_CREAT|O_TRUNC under the payload's proven
// writable /data area, same path class as prior payloads' record files):
//   a1  write 256 B from RW page A +0x100 (pattern A) -> rc==256 expected;
//       file must equal pattern A (THE positive byte-true proof)
//   a2  write 256 B from RW page B +0x100 (pattern B) -> rc==256; file equals
//       pattern B — DISCRIMINATION: distinct source pages, distinct contents
//   as  write   8 B from RW page A +0x40  -> rc==8 (channel-clean positive:
//       any raw error code is > 8, so rc==len is unambiguous)
//   x   write  16 B from page A +0xFF8 spanning [A-tail | B-head] -> rc==16,
//       file equals the spliced contents (multi-page copyin design input,
//       non-gating)
//   bh  write   8 B from the local munmap HOLE -> raw EFAULT(14) expected,
//       file stays EMPTY (len=8 makes any raw code in [1,255]\{8}
//       unambiguous — a partial transfer can never exceed len)
//   bn  write   8 B from the PROT_NONE page, ONLY if mprotect constructed it
//       (mprotect is FLAKY standalone: rc=0 once, -1/EPERM twice in prior
//       evidence — raw result recorded either way; case conclusions gated on
//       what actually constructed). Page 3 is NEVER WRITTEN before the
//       mprotect (prior W^X-on-dirtied-page hypothesis).
//   bd  write   8 B from DM-class 0x22C0000000 -> raw recorded (comparable
//       with prior evidence; not gating)
//   bhi write   8 B from high unmapped 0x4000000000 -> raw EFAULT(14)
//       expected, file stays EMPTY. THIS is the task's case (b).
//
// ORDER: all positive cases first, wild-address cases LAST. Evidence is
// appended ONE COMPLETE JSONL LINE PER CHECK, each line fopen("a")/fclose'd
// immediately, so any crash prefix of the run still yields valid evidence
// lines (also defeats the dispatch-1 giant-line serialization defect class).
//
// write(2)-specific classification (CRITICAL correctness): rc==len is
// SUCCESS, NOT an error. Classes:
//   success-full                  rc == len
//   ambiguous-partial-or-raw-err  0 <= rc < len (raw convention: rc would be
//                                 the error code; wrapper convention: a
//                                 partial count — the FILE READBACK is the
//                                 ground truth that disambiguates)
//   raw-error-code                rc > len && rc < 256 (a transfer count can
//                                 never exceed len, so this is a raw kernel
//                                 error code; with len=8, EFAULT=14 lands
//                                 here unambiguously)
//   wrapper-error-errno           rc == -1 (read errno)
//   other                         rc < -1 or rc >= 256
// errno is reset to 0 before EVERY call and captured immediately after (the
// stale-errno trap from prior evidence). Kernel refusals via raw routes keep
// errno==0 (proven); mprotect showed the -1/errno convention, so BOTH are
// recorded for every call and classification handles both.
//
// Verdict law (task):
//   WORKS   iff a1+a2 byte-true successes AND as positive AND bh raw EFAULT
//           AND bhi raw EFAULT AND both b files read back empty
//   WORKS_ERROR_MAP_NOTE  positives byte-true AND b-cases non-success with
//           files empty but a code other than EFAULT (error-map correction
//           recorded for the controller; still an owned-read mechanism)
//   DEAD    positives fail (EPERM/EIO/vacuous: hole write "succeeds" and
//           lands bytes = no copyin discrimination)
//   INCONCLUSIVE  setup failed (no evidence either way)
// Decisive-vs-works gating is kept SEPARATE: decisive means the check
// produced a conclusive classified surface fact (an EFAULT refusal is
// decisive evidence but never makes the candidate work).
//
// Bounded: ONE process run, no loops, no network, no timestamps, <= ~14
// small JSONL lines. Exit 0 iff every attempted check was decisive and the
// record was written. NO game contact, no game files, no ploader.ini; this
// payload touches only its own /data/modbridge-copyin* files. Production
// runtime code is NOT changed by this worker — evidence for a FUTURE
// bridge_kernel_range_verdict owned-read sibling path only.

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

#if !defined(HOST_COPYIN)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#ifdef HOST_COPYIN
#define TARGET_NAME "host"
#else
#define TARGET_NAME "prospero"
#endif

namespace {

constexpr uint64_t kHighUnmapped = 0x4000000000ull;  // task case (b): high, above any plausible mapping
constexpr uint64_t kDmAddress    = 0x22C0000000ull;  // prior evidence DM-class address (non-gating)
constexpr size_t   kRegionBytes  = 0x4000;           // ONE mmap: page0=A, page1=B, page2=hole, page3=PROT_NONE

// Pattern law: page A byte i = 'A' + (i % 26); page B byte i = 'a' + (i % 26).
inline uint8_t pat_a(size_t i) { return (uint8_t)('A' + (i % 26)); }
inline uint8_t pat_b(size_t i) { return (uint8_t)('a' + (i % 26)); }

constexpr long long kNotMade = LLONG_MIN;

// ---- raw call capture: raw_rc AND fresh errno for EVERY call --------------
struct Call {
    long long rc = kNotMade;  // kNotMade: call not made
    int en = -1;              // -1: call not made
};

// ---- pure write-specific classification (host-tested) ---------------------
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
        case 2:  return "ENOENT";
        case 5:  return "EIO";
        case 9:  return "EBADF";
        case 13: return "EACCES";
        case 14: return "EFAULT";
        case 22: return "EINVAL";
        default: return "code-other";
    }
}

// ---- bounded JSON field builders (proven-valid pattern, small lines) ------
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
    static std::string hex64(uint64_t v) { char b[32]; std::snprintf(b, sizeof b, "0x%llx", (unsigned long long)v); return b; }
    void n(const char* k, long long v) { sep(); text += "\""; text += k; text += "\":"; text += num(v); }
    void s(const char* k, const char* v) { sep(); text += "\""; text += k; text += "\":\""; text += esc(v); text += "\""; }
    void raw(const char* k, const std::string& rendered) { sep(); text += "\""; text += k; text += "\":"; text += rendered; }
    void b(const char* k, bool v) { sep(); text += "\""; text += k; text += "\":"; text += v ? "true" : "false"; }
    void addr(const char* k, uint64_t v) { sep(); text += "\""; text += k; text += "\":\""; text += hex64(v); text += "\""; }
    void call(const char* k, const Call& c) {
        sep(); text += "\""; text += k; text += "\":{\"rc\":";
        text += (c.rc == kNotMade ? std::string("null") : num(c.rc));
        text += ",\"errno\":"; text += num(c.en); text += "}";
    }
};

std::string make_line(const char* check_id, bool decisive, const Fields& d)
{
    std::string line = "{\"marker\":\"modbridge.copyin\",\"schema\":1,\"target\":\"" TARGET_NAME;
    line += "\",\"check\":\""; line += check_id;
    line += "\",\"decisive\":"; line += decisive ? "true" : "false";
    line += ",\"detail\":{"; line += d.text; line += "}}\n";
    return line;
}

void append_line(const std::string& line, const char* sink)
{
    if (std::FILE* f = std::fopen(sink, "a")) {   // fopen "a" CREATES: proven on-target
        std::fputs(line.c_str(), f);
        std::fclose(f);
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

// ---- readback: the file is the ground truth -------------------------------
struct Readback {
    Call open_call;            // ::open(path, O_RDONLY)
    long long total = -1;      // bytes read to EOF (cap 4097); -1: read error
    int read_err = -1;
    long long expect_n = -1;   // -1: no expectation (record-only case)
    bool cmp_equal = false;    // total == expect_n && memcmp == 0
    bool empty = false;        // total == 0
    bool authoritative = false;// open ok AND read loop completed (total >= 0)
    std::string first16_hex;   // up to first 16 bytes, hex
    std::string content_hex;   // (small cases) full content hex, cap 64 B
};

#if !defined(HOST_COPYIN)

Readback readback_file(const char* path, const unsigned char* expect, long long expect_n)
{
    Readback rb;
    rb.expect_n = expect_n;
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
        if (total > 4096) break;   // overlong: recorded honestly, loop bounded
    }
    rb.read_err = last_err;
    rb.total = total;
    rb.authoritative = (total >= 0);
    if (total > 0) {
        const long long shown = total < 16 ? total : 16;
        rb.first16_hex = hex_bytes(buf, shown);
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

Call do_close(int fd)
{
    Call c; errno = 0;
    c.rc = ::close(fd);
    c.en = errno;
    return c;
}

struct CaseResult {
    const char* id = "unset";      // ALWAYS assigned (out-param defect guard)
    const char* sink = "unset";    // ALWAYS assigned
    Call open_call, w, close_call;
    long long len = -1;
    Readback rb;
    const char* wclass = "not-made";
    const char* wcode = "not-made";
    bool open_ok = false;
};

CaseResult run_case(const char* id, const char* sink_path,
                    const void* src, long long len,
                    const unsigned char* expect, long long expect_n)
{
    CaseResult r;
    r.id = id;                       // every out-field assigned on EVERY path
    r.sink = sink_path;
    r.len = len;
    errno = 0;
    r.open_call.rc = ::open(sink_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    r.open_call.en = errno;
    r.open_ok = (r.open_call.rc >= 0);
    if (r.open_ok) {
        errno = 0;
        r.w.rc = (long long)::write(r.open_call.rc, src, (size_t)len);
        r.w.en = errno;
        r.close_call = do_close(r.open_call.rc);
        r.wclass = classify_write(r.w.rc, r.len, r.w.en);
        r.wcode = (r.wclass == std::string("success-full"))
                      ? "none" : code_name(r.w.rc == -1 ? r.w.en : r.w.rc);
    } else {
        r.wclass = "open-failed";    // recorded, never left unset
        r.wcode = code_name(r.open_call.en);
    }
    r.rb = readback_file(sink_path, expect, expect_n);
    return r;
}

Fields case_fields(const CaseResult& r, uint64_t src_addr)
{
    Fields d;
    d.s("case", r.id);
    d.s("sink", r.sink);
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
    d.raw("rb_first16", r.rb.first16_hex.empty() ? "\"\"" : "\"" + r.rb.first16_hex + "\"");
    if (!r.rb.content_hex.empty()) {
        d.raw("rb_content_head64", "\"" + r.rb.content_hex + "\"");
    }
    return d;
}

bool case_positive(const CaseResult& r)
{
    return r.open_ok && r.wclass == std::string("success-full")
        && r.rb.authoritative && r.rb.cmp_equal;
}

bool case_refused_empty(const CaseResult& r)
{
    // kernel refused the copyin (any non-success class) AND the file provably
    // stayed empty: genuine not-readable discrimination.
    return r.open_ok && r.wclass != std::string("success-full")
        && r.rb.authoritative && r.rb.empty;
}

struct Setup {
    Call mmap_call, munmap_call, mprotect_call;
    uint64_t base = 0, page_a = 0, page_b = 0, hole = 0, page_none = 0;
    bool mapped = false, planted_a = false, planted_b = false;
    bool hole_ok = false, none_constructed = false;
};

Setup do_setup()
{
    Setup t;
    errno = 0;
    t.mmap_call.rc = (long long)::mmap(nullptr, kRegionBytes,
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANON, -1, 0);
    t.mmap_call.en = errno;
    if (t.mmap_call.rc < 0) return t;   // mapped stays false; ALL fields set
    t.mapped = true;
    unsigned char* base = (unsigned char*)(uintptr_t)t.mmap_call.rc;
    t.base = (uint64_t)base;
    t.page_a = t.base;
    t.page_b = t.base + 0x1000;
    t.page_none = t.base + 0x3000;
    // Plant FULL-page positional patterns (pages A and B only).
    for (size_t i = 0; i < 0x1000; ++i) {
        base[i] = pat_a(i);
        base[0x1000 + i] = pat_b(i);
    }
    // Volatile verify (RW pages only, BEFORE any protection change): check
    // the offsets the cases actually read from, plus mid-page.
    t.planted_a = true;
    t.planted_b = true;
    static const size_t offs[] = {0x40, 0x100, 0x800, 0xFF8};
    for (const size_t o : offs) {
        for (size_t j = 0; j < 8; ++j) {
            if (*(const volatile uint8_t*)(base + o + j) != pat_a(o + j))
                t.planted_a = false;
            if (*(const volatile uint8_t*)(base + 0x1000 + o + j) != pat_b(o + j))
                t.planted_b = false;
        }
    }
    errno = 0;
    t.munmap_call.rc = ::munmap(base + 0x2000, 0x1000);
    t.munmap_call.en = errno;
    t.hole = t.base + 0x2000;
    t.hole_ok = (t.munmap_call.rc == 0);
    // PROT_NONE on the NEVER-WRITTEN page 3 (dirty-page W^X hypothesis from
    // prior evidence: fresh page gave mprotect rc=0, written page gave
    // -1/EPERM). The raw result is recorded either way; case bn is gated on
    // none_constructed.
    errno = 0;
    t.mprotect_call.rc = ::mprotect(base + 0x3000, 0x1000, PROT_NONE);
    t.mprotect_call.en = errno;
    t.none_constructed = (t.mprotect_call.rc == 0);
    return t;
}

#endif  // !HOST_COPYIN

}  // namespace

int main(int argc, char** argv)
{
    const char* sink = argc > 1 ? argv[1] : "/data/modbridge-copyin.jsonl";
    bool all_decisive = true;

#if defined(HOST_COPYIN)
    // Host: pure classification table + framing (no kernel probes).
    {
        struct Row { long long rc; long long len; int en; const char* want; };
        const Row rows[] = {
            {256, 256, 0,  "success-full"},
            {8,   8,   0,  "success-full"},
            {14,  8,   0,  "raw-error-code"},                  // unambiguous raw EFAULT
            {14,  256, 0,  "ambiguous-partial-or-raw-err"},    // readback disambiguates
            {8,   8,   0,  "success-full"},
            {5,   8,   0,  "ambiguous-partial-or-raw-err"},    // EIO=5 < len=8: rc-ambiguous; file readback disambiguates
            {1,   8,   0,  "ambiguous-partial-or-raw-err"},    // EPERM=1 < len=8: same
            {0,   8,   0,  "ambiguous-partial-or-raw-err"},
            {-1,  8,   14, "wrapper-error-errno"},
            {300, 8,   0,  "other"},
            {-9,  8,   0,  "other"},
        };
        Fields d;
        bool ok = true;
        for (const Row& r : rows) {
            const char* got = classify_write(r.rc, r.len, r.en);
            ok = ok && std::strcmp(got, r.want) == 0;
            d.s(r.want, got);
        }
        // code-name spot checks
        const bool names_ok = std::strcmp(code_name(14), "EFAULT") == 0 &&
                              std::strcmp(code_name(1), "EPERM") == 0 &&
                              std::strcmp(code_name(5), "EIO") == 0 &&
                              std::strcmp(code_name(13), "EACCES") == 0 &&
                              std::strcmp(code_name(999), "code-other") == 0;
        d.b("code_names_ok", names_ok);
        ok = ok && names_ok;
        const std::string line = make_line("h1_classify_table", ok, d);
        std::fputs(line.c_str(), stdout);
        append_line(line, sink);
        all_decisive = ok;
    }
#else
    // ---- setup ----------------------------------------------------------
    const Setup t = do_setup();
    {
        Fields d;
        d.call("mmap", t.mmap_call);
        d.addr("base", t.base);
        d.addr("page_a", t.page_a);
        d.addr("page_b", t.page_b);
        d.addr("hole", t.hole);
        d.addr("page_none", t.page_none);
        d.call("munmap", t.munmap_call);
        d.call("mprotect_protnone", t.mprotect_call);
        d.b("mapped", t.mapped);
        d.b("planted_a", t.planted_a);
        d.b("planted_b", t.planted_b);
        d.b("hole_ok", t.hole_ok);
        d.b("none_constructed", t.none_constructed);
        d.s("note", "single mmap of 0x4000: page0=A page1=B page2=hole page3=PROT_NONE;"
                    "page3 never written before mprotect (prior dirty-page W^X hypothesis);"
                    "no second mmap anywhere (prior aliasing trap)");
        const bool decisive = t.mapped && t.planted_a && t.planted_b && t.hole_ok;
        all_decisive = all_decisive && decisive;
        append_line(make_line("0_setup", decisive, d), sink);
    }

    // Case expectations (RAM copies for memcmp; on-stack, readable).
    unsigned char exp_a1[256], exp_a2[256], exp_as[8], exp_x[16];
    for (int j = 0; j < 256; ++j) {
        exp_a1[j] = pat_a((size_t)(0x100 + j));
        exp_a2[j] = pat_b((size_t)(0x100 + j));
    }
    for (int j = 0; j < 8; ++j) exp_as[j] = pat_a((size_t)(0x40 + j));
    for (int j = 0; j < 8; ++j) {
        exp_x[j] = pat_a((size_t)(0xFF8 + j));      // A tail
        exp_x[8 + j] = pat_b((size_t)j);            // B head
    }

    // ---- positive cases FIRST (all risky addresses LAST) ----------------
    const CaseResult a1 = run_case("a1_patternA_256", "/data/modbridge-copyin-a1.bin",
                                   (const void*)(uintptr_t)(t.page_a + 0x100), 256,
                                   exp_a1, 256);
    const CaseResult a2 = run_case("a2_patternB_256", "/data/modbridge-copyin-a2.bin",
                                   (const void*)(uintptr_t)(t.page_b + 0x100), 256,
                                   exp_a2, 256);
    const CaseResult as = run_case("as_patternA_8", "/data/modbridge-copyin-as.bin",
                                   (const void*)(uintptr_t)(t.page_a + 0x40), 8,
                                   exp_as, 8);
    const CaseResult cx = run_case("x_crosspage_16", "/data/modbridge-copyin-x.bin",
                                   (const void*)(uintptr_t)(t.page_a + 0xFF8), 16,
                                   exp_x, 16);
    for (const CaseResult* r : {&a1, &a2, &as, &cx}) {
        const bool decisive = r->open_ok && r->rb.authoritative;
        all_decisive = all_decisive && decisive;
        append_line(make_line(r->id, decisive, case_fields(*r, 0)), sink);
    }

    // ---- refusal cases ---------------------------------------------------
    const CaseResult bh = run_case("bh_hole_8", "/data/modbridge-copyin-bh.bin",
                                   (const void*)(uintptr_t)t.hole, 8, nullptr, 0);
    const CaseResult bn = run_case("bn_protnone_8", "/data/modbridge-copyin-bn.bin",
                                   (const void*)(uintptr_t)t.page_none, 8, nullptr, 0);
    const CaseResult bd = run_case("bd_dmclass_8", "/data/modbridge-copyin-bd.bin",
                                   (const void*)kDmAddress, 8, nullptr, 0);
    const CaseResult bhi = run_case("bhi_highunmapped_8", "/data/modbridge-copyin-bhi.bin",
                                    (const void*)kHighUnmapped, 8, nullptr, 0);
    for (const CaseResult* r : {&bh, &bn, &bd, &bhi}) {
        const bool decisive = r->open_ok && r->rb.authoritative;
        all_decisive = all_decisive && decisive;
        Fields d = case_fields(*r, 0);
        if (r == &bn && !t.none_constructed) {
            d.s("gate", "PROT_NONE not constructed this run (raw mprotect result in 0_setup); case recorded, conclusions gated off");
        }
        append_line(make_line(r->id, decisive, d), sink);
    }

    // ---- verdict (computed in ONE place; every input assigned above) -----
    {
        const bool a1_ok = case_positive(a1);
        const bool a2_ok = case_positive(a2);
        const bool as_ok = case_positive(as);
        const bool x_ok = case_positive(cx);
        const bool positives_ok = a1_ok && a2_ok && as_ok;

        const bool hole_efault = (bh.w.rc == 14) && case_refused_empty(bh);
        const bool high_efault = (bhi.w.rc == 14) && case_refused_empty(bhi);
        const bool b_all_refused_empty = case_refused_empty(bh) && case_refused_empty(bhi)
            && case_refused_empty(bd) && (!t.none_constructed || case_refused_empty(bn));
        const bool hole_vacuous = (bh.w.rc == bh.len) && bh.rb.authoritative && !bh.rb.empty;

        const char* verdict = "INCONCLUSIVE";
        std::string note = "unreached";
        if (!t.mapped || !t.planted_a || !t.planted_b || !t.hole_ok) {
            verdict = "INCONCLUSIVE";
            note = "setup failed; no verdict either way";
        } else if (hole_vacuous) {
            verdict = "DEAD";
            note = "vacuous: hole write 'succeeded' and landed bytes - no copyin discrimination";
        } else if (positives_ok && hole_efault && high_efault) {
            verdict = "WORKS";
            note = "positive byte-true file contents + raw EFAULT on hole and high unmapped, files empty";
        } else if (positives_ok && b_all_refused_empty) {
            verdict = "WORKS_ERROR_MAP_NOTE";
            note = "positive byte-true + kernel-refused non-success on all b-cases with files empty; error map needs correction from the recorded raw codes";
        } else if (!positives_ok) {
            verdict = "DEAD";
            note = "positive case failed (EPERM/EIO/open-failure class); see a1/a2/as raw records";
        } else {
            verdict = "INCONCLUSIVE";
            note = "positives ok but b-cases neither EFAULT nor refused-empty; inspect raw records";
        }

        Fields d;
        d.b("a1_positive", a1_ok);
        d.b("a2_positive", a2_ok);
        d.b("as_positive", as_ok);
        d.b("x_crosspage_ok", x_ok);
        d.b("positives_ok", positives_ok);
        d.b("hole_efault", hole_efault);
        d.b("high_efault", high_efault);
        d.b("b_all_refused_empty", b_all_refused_empty);
        d.b("hole_vacuous", hole_vacuous);
        d.b("protnone_constructed", t.none_constructed);
        d.s("verdict", verdict);
        d.s("note", note.c_str());
        append_line(make_line("v_verdict", true, d), sink);
    }

    // ---- surface numbers ---------------------------------------------------
    {
        Fields d;
        d.n("SYS_write", SYS_write);
        d.n("SYS_open", SYS_open);
        d.n("SYS_close", SYS_close);
        d.n("SYS_read", SYS_read);
        d.n("SYS_mmap", SYS_mmap);
        d.n("SYS_munmap", SYS_munmap);
        d.n("SYS_mprotect", SYS_mprotect);
        d.n("pid", (long long)::getpid());
        append_line(make_line("s_surface_numbers", true, d), sink);
    }

    // ---- scope honesty -----------------------------------------------------
    {
        Fields d;
        d.s("game_process", "out-of-scope: this payload never touches the game process; no ploader.ini, no game files");
        d.s("in_game_context", "untestable-separate-process: these results prove the payload (pldmgr-child) context only; in-game write-copyin semantics need a game run");
        d.s("sink_dir", "/data/modbridge-copyin-*: owned probe files, same writable-area path class as prior payloads' record files");
        append_line(make_line("d_scope", true, d), sink);
    }
#endif

    return all_decisive ? 0 : 1;
}
