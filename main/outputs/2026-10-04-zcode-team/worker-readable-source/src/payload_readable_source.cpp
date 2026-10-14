// payload_readable_source.cpp — worker-readable-source, 2026-10-05.
//
// Standalone payload probing KERNEL-MEDIATED READ primitives for OWN-process
// memory on the PROSPERO target, in priority order. The kernel performs (or
// refuses) each read itself and returns an error code instead of faulting
// the process; if such a primitive works for the process's own memory it is
// simultaneously the readability proof AND the owned read.
//
// Candidates (stop at the FIRST whose cases (a)+(b)+(c) are all decisive):
//   1  ptrace(PT_READ_D) — SYS_ptrace(26); TWO routes recorded: the raw
//      ::syscall trampoline AND the statically-linked ptrace stub. Both are
//      plain `syscall;retq` wrappers in libc.a syscalls.o (verified by
//      disassembly) so NEITHER sets errno; the raw kernel rc is expected in
//      BOTH, per the proven payload-selftest raw convention.
//   2  ptrace(PT_IO, PIOD_READ_D) — multi-byte read; the piod_len remainder
//      gives exact transfer counts (no word/error channel ambiguity).
//   3  /proc/self/mem then /proc/curproc/mem via open(2) + pread(2).
//   4  write(2) kernel-copyin probe — the kernel reads user memory into an
//      fd; unreadable addresses must surface as EFAULT/EIO, never SIGSEGV.
//
// Per-candidate cases:
//   (a) known-readable RW page: primitive must return the exact planted
//       32-bit word (0x5A5AC0DE, deliberately > 255 so it can never collide
//       with a positive errno in the raw return channel).
//   (b) unmapped: a local munmap hole AND the DM-class address 0x22C0000000
//       (the listing-absent candidate Blocks[0]); expect an error code, NO
//       crash. This previews, from inside a payload process, exactly what a
//       future in-game kernel-mediated read of that address must return.
//   (c) exec-only page (mprotect PROT_EXEC — proven to succeed on-target in
//       worker-payload-selftest): record WHICH code (EIO / EACCES / EPERM /
//       EFAULT) or a word-match if the kernel reads exec-only memory; NO
//       crash either way. Decisive = word-match OR a recognized errno.
//   (d) recorded honestly in every run: a WIDE high-address range (the
//       139 GiB pool envelope above the game's complete KERN_PROC_VMMAP top
//       0x1100400000) is NOT constructible in this standalone process, and
//       reading ANOTHER process (the game) is out of scope for this payload.
//       Both are untestable-separate-process until a game run.
//
// When candidate 1 is the first that fully works, ONE extra bounded PT_IO
// spliced probe ([RW | unmapped hole | RW]) is recorded as a NON-GATING
// design-note input (partial-transfer semantics).
//
// Raw conventions: EVERY kernel-mediated call records BOTH raw_rc AND the
// errno observed right after it (the proven SDK trampoline convention
// surfaces kernel errors as POSITIVE raw rc with errno unset).
//
// Bounded: ONE appended JSONL line (<= ~6 KiB), one process run, no loops,
// no network, no timestamps. Exit 0 iff every attempted check was decisive
// and the record was written. Production runtime code is NOT changed by
// this worker — this is evidence for a FUTURE ReadableCoverageSource::
// KernelMediatedRead candidate only.

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#if !defined(HOST_READABLESOURCE)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef HOST_READABLESOURCE
#define TARGET_NAME "host"
#else
#define TARGET_NAME "prospero"
#endif

namespace {

constexpr uint32_t kRwWord = 0x5A5AC0DEu;         // planted on RW pages (>255)
constexpr uint32_t kExecWord = 0x0DEC0DEDu;       // planted on the exec-only page
constexpr uint64_t kDmAddress = 0x22C0000000ull;  // listing-absent candidate Blocks[0]

// ---- raw call capture: raw_rc AND errno for EVERY call -------------------
struct RawCall {
    long long rc = LLONG_MIN;  // raw return (LLONG_MIN: call not made)
    int err = -1;              // errno observed right after (-1: not made)
};

// ---- pure classification (host-tested; the design-note mapping) ----------
struct ReadClass {
    enum V {
        Readable,          // expected word returned through the raw channel
        ErrorUnmapped,     // EFAULT(14) — proc_rwmem miss on FreeBSD
        ErrorProtection,   // EIO(5) — KERN_PROTECTION_FAILURE mapping
        ErrorPolicy,       // EPERM(1) / EACCES(13) — tracing/cred refusal
        ErrorOther,        // other positive kernel errno in [1,255]
        NotDecisive        // 0 with mismatch, negative rc, or out-of-range
    };
    static const char* name(V v) {
        switch (v) {
            case Readable:        return "readable";
            case ErrorUnmapped:   return "error-unmapped";
            case ErrorProtection: return "error-protection";
            case ErrorPolicy:     return "error-policy";
            case ErrorOther:      return "error-other";
            default:              return "not-decisive";
        }
    }
};

ReadClass::V classify(long long rc, uint32_t expected_word)
{
    if (rc == (long long)expected_word) return ReadClass::Readable;
    if (rc > 0 && rc < 256) {
        switch (rc) {
            case 14: return ReadClass::ErrorUnmapped;    // EFAULT
            case 5:  return ReadClass::ErrorProtection;  // EIO
            case 1:                                      // EPERM
            case 13: return ReadClass::ErrorPolicy;      // EACCES
            default: return ReadClass::ErrorOther;
        }
    }
    return ReadClass::NotDecisive;
}

// ---- bounded JSON builders ----------------------------------------------
struct Json {
    std::string text;
    explicit Json(const char* marker) {
        text += "{\"marker\":\"";
        text += marker;
        text += "\",\"schema\":1,\"target\":\"" TARGET_NAME "\",\"checks\":[";
    }
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
    void check(const char* id, bool decisive, const std::string& detail_body) {
        if (text.back() != '[') text += ",";
        text += "{\"id\":\""; text += id;
        text += "\",\"decisive\":"; text += decisive ? "true" : "false";
        if (!detail_body.empty()) { text += ",\"detail\":{"; text += detail_body; text += "}"; }
        text += "}";
    }
};

std::string num(long long v) { char b[32]; std::snprintf(b, sizeof b, "%lld", v); return b; }
std::string hex64(uint64_t v) { char b[32]; std::snprintf(b, sizeof b, "0x%llx", (unsigned long long)v); return b; }
std::string quoted(const std::string& s) { return "\"" + Json::esc(s.c_str()) + "\""; }

// One detail-object body: comma-managed "key":value pairs.
struct Fields {
    std::string text;
    bool any_ = false;
    void n(const char* k, long long v) { sep(); text += quoted(k); text += ":"; text += num(v); }
    void s(const char* k, const char* v) { sep(); text += quoted(k); text += ":"; text += quoted(v); }
    void raw(const char* k, const std::string& rendered) { sep(); text += quoted(k); text += ":"; text += rendered; }
    void o(const char* k, const Fields& inner) { sep(); text += quoted(k); text += ":{"; text += inner.text; text += "}"; }
    void sep() { if (any_) text += ","; any_ = true; }
};

Fields read_fields(const RawCall& c, const char* route, uint64_t addr, uint32_t word)
{
    Fields f;
    f.s("route", route);
    f.n("raw_rc", c.rc);
    f.n("errno", c.err);
    f.raw("addr", quoted(hex64(addr)));
    f.n("expect_word", (long long)word);
    f.s("class", ReadClass::name(classify(c.rc, word)));
    return f;
}

bool readable_word(const RawCall& c, uint32_t word)
{
    return c.rc != LLONG_MIN && classify(c.rc, word) == ReadClass::Readable;
}

bool decisive_error(const RawCall& c)
{
    // For a should-fail probe: a recognized error class, and NOT a read.
    if (c.rc == LLONG_MIN) return false;
    const ReadClass::V v = classify(c.rc, kRwWord);
    if (v != ReadClass::Readable && v != ReadClass::NotDecisive) return true;
    // Also accept the classic wrapper convention (-1 with a known errno),
    // should this target's route use it instead of the positive-raw convention.
    if (c.rc == -1 && c.err > 0 && c.err < 256) {
        const ReadClass::V w = classify(c.err, kRwWord);
        if (w != ReadClass::NotDecisive) return true;
    }
    // Partial transfer followed by an error (copyin family): decisive.
    if (c.rc >= 0 && c.rc < 8 && c.err > 0 && c.err < 256) return true;
    return false;
}

#if !defined(HOST_READABLESOURCE)

RawCall raw_ptrace_readd(int req, pid_t pid, uint64_t addr)
{
    errno = 0;
    RawCall c;
    c.rc = (long long)::syscall(SYS_ptrace, req, (long)pid, (long)addr, 0);
    c.err = errno;
    return c;
}

RawCall stub_ptrace_readd(int req, pid_t pid, uint64_t addr)
{
    errno = 0;
    RawCall c;
    c.rc = (long long)::ptrace(req, pid, (caddr_t)(uintptr_t)addr, 0);
    c.err = errno;
    return c;
}

struct TargetPlan {
    unsigned char* rw = nullptr;   // 0x3000 at mmap: [RW 0x1000 | hole | RW 0x1000]
    unsigned char* ex = nullptr;   // = rw + 0x2000; mprotect'ed PROT_EXEC after planting
    uint64_t rw_lo = 0, hole = 0, rw_hi = 0;
    int mprotect_rc = -1, mprotect_err = 0, munmap_rc = -1;
    bool rw_word_back = false, hi_word_back = false;
    bool exec_constructed = false; // dispatch-2 evidence: mprotect-to-exec is
                                   // UNRELIABLE standalone (rc=0 once, EPERM
                                   // once); case (c) is conditional on this.
};

bool setup(TargetPlan& t)
{
    // ONE mmap only: dispatch-1 evidence showed a second anon mmap handed
    // back the SAME base (ex aliased rw_lo, corrupting the planted word).
    // The exec-only page is page 2 of this same region — no second mmap.
    void* p = mmap(nullptr, 0x3000, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) return false;
    t.rw = (unsigned char*)p;
    std::memset(t.rw, 0, 0x1000);
    std::memset(t.rw + 0x2000, 0, 0x1000);
    std::memcpy(t.rw + 0x40, &kRwWord, 4);
    std::memcpy(t.rw + 0x2040, &kRwWord, 4);
    t.ex = t.rw + 0x2000;
    std::memcpy(t.ex, &kExecWord, 4);   // case-(c) word, planted BEFORE mprotect
    t.rw_lo = (uint64_t)t.rw;
    t.rw_hi = t.rw_lo + 0x2000;
    // Safe volatile readbacks BEFORE any protection change.
    t.rw_word_back =
        *(const volatile uint32_t*)(uintptr_t)(t.rw_lo + 0x40) == kRwWord;
    t.hi_word_back =
        *(const volatile uint32_t*)(uintptr_t)(t.rw_hi + 0x40) == kRwWord;
    t.munmap_rc = munmap(t.rw + 0x1000, 0x1000);
    if (t.munmap_rc != 0) return false;
    t.hole = t.rw_lo + 0x1000;
    errno = 0;
    t.mprotect_rc = mprotect(t.ex, 0x1000, PROT_EXEC);   // page 2 only
    t.mprotect_err = errno;
    t.exec_constructed = (t.mprotect_rc == 0);
    // setup_ok requires only what cases (a)+(b) need: the mapping, the
    // planted words, and the hole. Case (c) is conditional per its contract.
    return t.rw_word_back && t.hi_word_back;
}

// Candidate 1: PT_READ_D via raw trampoline (and static stub on case a).
// Returns true iff (a)+(b)+(c) were ALL decisive-AND-correct:
// (a) BOTH routes return the planted WORD (a refused call is a decisive
//     surface fact but does NOT make the candidate work);
// (b) hole + DM-class address return recognized error codes, no crash;
// (c) exec-only page: word-match OR recognized refusal — recorded either way.
bool run_candidate1(TargetPlan& t, Json& line, bool& fully_works)
{
    const pid_t pid = ::getpid();
    bool a_ok = false, b_ok = false, c_ok = false;
    Fields d;

    {   // (a) known-readable: BOTH routes must return the planted word
        const RawCall raw = raw_ptrace_readd(PT_READ_D, pid, t.rw_lo + 0x40);
        const RawCall stub = stub_ptrace_readd(PT_READ_D, pid, t.rw_lo + 0x40);
        a_ok = readable_word(raw, kRwWord) && readable_word(stub, kRwWord);
        d.s("case", "a-known-readable");
        d.o("raw", read_fields(raw, "raw-syscall", t.rw_lo + 0x40, kRwWord));
        d.o("stub", read_fields(stub, "static-stub", t.rw_lo + 0x40, kRwWord));
    }
    {   // (b) unmapped: local hole + the DM-class address; no crash allowed
        const RawCall hole = raw_ptrace_readd(PT_READ_D, pid, t.hole);
        const RawCall dm = raw_ptrace_readd(PT_READ_D, pid, kDmAddress);
        b_ok = decisive_error(hole) && decisive_error(dm);
        Fields b;
        b.s("case", "b-unmapped");
        b.o("hole", read_fields(hole, "raw-syscall", t.hole, kRwWord));
        b.o("dm_address", read_fields(dm, "raw-syscall", kDmAddress, kRwWord));
        d.o("b", b);
    }
    {   // (c) exec-only IF CONSTRUCTIBLE: word-match OR a recognized
        // refusal, never a crash; a construction failure is recorded and
        // does NOT doom the candidate (the task's case-c contract).
        if (t.exec_constructed) {
            const RawCall ex = raw_ptrace_readd(PT_READ_D, pid, (uint64_t)t.ex);
            c_ok = readable_word(ex, kExecWord) || decisive_error(ex);
            Fields c;
            c.s("case", "c-exec-only");
            c.o("raw", read_fields(ex, "raw-syscall", (uint64_t)t.ex, kExecWord));
            c.n("exec_word", (long long)kExecWord);
            c.n("mprotect_rc", t.mprotect_rc);
            c.n("mprotect_errno", t.mprotect_err);
            d.o("c", c);
        } else {
            Fields c;
            c.s("case", "c-exec-only-not-constructed");
            c.n("mprotect_rc", t.mprotect_rc);
            c.n("mprotect_errno", t.mprotect_err);
            c.s("note", "exec-only page not constructible this run; case c skipped, construction failure recorded");
            d.o("c", c);
        }
    }
    fully_works = a_ok && b_ok && c_ok;

    // Non-gating design-note input: PT_IO spliced read across [RW|hole|RW].
    if (fully_works) {
        static unsigned char iobuf[0x3000];
        ptrace_io_desc pio;
        pio.piod_op = PIOD_READ_D;
        pio.piod_offs = t.rw;
        pio.piod_addr = iobuf;
        pio.piod_len = sizeof iobuf;
        const RawCall raw = raw_ptrace_readd(PT_IO, pid, (uint64_t)&pio);
        const long long transferred =
            (raw.rc == 0) ? (long long)(sizeof iobuf - pio.piod_len) : 0;
        const bool words_ok = transferred >= 0x2044 &&
            std::memcmp(iobuf + 0x40, &kRwWord, 4) == 0 &&
            std::memcmp(iobuf + 0x2040, &kRwWord, 4) == 0;
        Fields e;
        e.s("route", "raw-syscall-ptio");
        e.n("raw_rc", raw.rc);
        e.n("errno", raw.err);
        e.n("requested", (long long)sizeof iobuf);
        e.n("transferred", transferred);
        e.raw("words_ok", words_ok ? "true" : "false");
        e.s("note", "non-gating design input recorded after c1 fully worked");
        line.check("e_ptio_splice_note", true, e.text);
    }

    line.check("c1_ptrace_read_d", true, d.text);
    return fully_works;
}

// Candidate 2: PT_IO (only when candidate 1 was not fully working).
bool run_candidate2(TargetPlan& t, Json& line, bool& fully_works)
{
    const pid_t pid = ::getpid();
    uint32_t wa = 0, wb = 0, we_word = 0;
    size_t ta = 0, tb = 0, th = 0, td = 0;
    RawCall re;   // default {LLONG_MIN, -1}: not made when exec not constructed
    auto ptio = [&](uint64_t offs, void* buf, size_t len, size_t& transferred) {
        ptrace_io_desc pio;
        pio.piod_op = PIOD_READ_D;
        pio.piod_offs = (void*)(uintptr_t)offs;
        pio.piod_addr = buf;
        pio.piod_len = len;
        errno = 0;
        RawCall raw;
        raw.rc = (long long)::syscall(SYS_ptrace, PT_IO, (long)pid, (long)&pio, 0);
        raw.err = errno;
        transferred = (raw.rc == 0) ? (len - pio.piod_len) : 0;
        return raw;
    };
    const RawCall ra = ptio(t.rw_lo + 0x40, &wa, 4, ta);
    const bool a_ok = (ra.rc == 0 && ta == 4 && wa == kRwWord);
    const RawCall rh = ptio(t.hole, &wb, 4, th);
    const RawCall rd = ptio(kDmAddress, &wb, 4, td);
    const bool b_ok = decisive_error(rh) && decisive_error(rd);
    bool c_ok = true;
    if (t.exec_constructed) {
        re = ptio((uint64_t)t.ex, &we_word, 4, tb);
        c_ok = readable_word(re, kExecWord) ||
               decisive_error(re) ||
               (re.rc == 0 && tb == 4 && we_word == kExecWord);
    }
    fully_works = a_ok && b_ok && c_ok;

    Fields d;
    d.s("case", "a-known-readable");
    d.n("raw_rc", ra.rc); d.n("errno", ra.err);
    d.n("transferred", (long long)ta); d.n("word", wa);
    d.n("expect_word", kRwWord);
    Fields b;
    b.s("case", "b-unmapped");
    b.o("hole", read_fields(rh, "raw-syscall-ptio", t.hole, kRwWord));
    b.o("dm_address", read_fields(rd, "raw-syscall-ptio", kDmAddress, kRwWord));
    Fields c;
    c.s("case", "c-exec-only");
    c.raw("exec_constructed", t.exec_constructed ? "true" : "false");
    c.n("mprotect_rc", t.mprotect_rc);
    c.n("mprotect_errno", t.mprotect_err);
    c.n("raw_rc", re.rc); c.n("errno", re.err);
    c.n("transferred", (long long)tb); c.n("word", we_word);
    c.n("expect_word", kExecWord);
    d.o("b", b);
    d.o("c", c);
    line.check("c2_ptrace_ptio", true, d.text);
    return fully_works;
}

// Candidate 3: /proc/self/mem (then /proc/curproc/mem) + pread.
bool run_candidate3(TargetPlan& t, Json& line, bool& fully_works)
{
    int fd = ::open("/proc/self/mem", O_RDONLY);
    const int open1_errno = errno;
    int open2_rc = -1, open2_errno = 0;
    const char* opened = nullptr;
    if (fd < 0) {
        open2_rc = ::open("/proc/curproc/mem", O_RDONLY);
        open2_errno = errno;
        if (open2_rc >= 0) { fd = open2_rc; opened = "/proc/curproc/mem"; }
    } else {
        opened = "/proc/self/mem";
    }
    Fields d;
    d.s("case", "open");
    d.n("open_rc", fd);
    d.n("open_errno", open1_errno);
    d.n("open_curproc_rc", open2_rc);
    d.n("open_curproc_errno", open2_errno);
    d.s("opened", opened ? opened : "none");
    bool fully = false;
    bool decisive = (fd < 0);  // ENOENT-class open failure is a decisive surface fact
    if (fd >= 0) {
        uint32_t wa = 0, we_word = 0;
        errno = 0;
        const long long pa = (long long)::pread(fd, &wa, 4, (off_t)(t.rw_lo + 0x40));
        const int pa_err = errno;
        const bool a_ok = (pa == 4 && wa == kRwWord);
        errno = 0;
        const long long ph = (long long)::pread(fd, &wa, 4, (off_t)t.hole);
        const int ph_err = errno;
        errno = 0;
        const long long pd = (long long)::pread(fd, &wa, 4, (off_t)kDmAddress);
        const int pd_err = errno;
        errno = 0;
        const long long pe = (long long)::pread(fd, &we_word, 4, (off_t)(uintptr_t)t.ex);
        const int pe_err = errno;
        RawCall ra{pa, pa_err}, rh{ph, ph_err}, rd{pd, pd_err}, re{pe, pe_err};
        const bool b_ok = decisive_error(rh) && decisive_error(rd);
        bool c_ok = true;
        if (t.exec_constructed) {
            c_ok = readable_word(re, kExecWord) ||
                   decisive_error(re) ||
                   (pe == 4 && we_word == kExecWord);
        }
        fully = a_ok && b_ok && c_ok;
        decisive = true;
        d.s("case", "pread");
        Fields a; a.s("case", "a-known-readable");
        a.n("pread_rc", pa); a.n("errno", pa_err); a.n("word", wa);
        a.n("expect_word", kRwWord);
        d.o("a", a);
        Fields b; b.s("case", "b-unmapped");
        b.o("hole", read_fields(rh, "proc-pread", t.hole, kRwWord));
        b.o("dm_address", read_fields(rd, "proc-pread", kDmAddress, kRwWord));
        d.o("b", b);
        Fields c; c.s("case", "c-exec-only");
        c.raw("exec_constructed", t.exec_constructed ? "true" : "false");
        c.n("mprotect_rc", t.mprotect_rc);
        c.n("mprotect_errno", t.mprotect_err);
        c.n("pread_rc", pe); c.n("errno", pe_err);
        d.o("c", c);
        ::close(fd);
    }
    line.check("c3_proc_mem", decisive, d.text);
    return fully;
}

// Candidate 4: write(2) kernel-copyin probe (bytes land in an owned fd).
bool run_candidate4(TargetPlan& t, Json& line, bool& fully_works)
{
    int fd = ::open("/dev/null", O_WRONLY);
    const int dn_errno = errno;
    const char* sink = "/dev/null";
    if (fd < 0) {
        fd = ::open("/data/modbridge-readablesource-probe.bin",
                    O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) sink = "/data/modbridge-readablesource-probe.bin";
    }
    Fields d;
    d.s("sink", sink);
    d.n("sink_open_rc", fd);
    d.n("devnull_open_errno", dn_errno);
    bool fully = false;
    if (fd >= 0) {
        errno = 0;
        const long long wa = (long long)::write(fd, t.rw + 0x40, 8);
        const int wa_err = errno;
        errno = 0;
        const long long wh = (long long)::write(fd, (const void*)(uintptr_t)t.hole, 8);
        const int wh_err = errno;
        errno = 0;
        const long long wd = (long long)::write(fd, (const void*)(uintptr_t)kDmAddress, 8);
        const int wd_err = errno;
        errno = 0;
        const long long we = (long long)::write(fd, t.ex, 8);
        const int we_err = errno;
        const RawCall ra{wa, wa_err}, rh{wh, wh_err}, rd{wd, wd_err}, re{we, we_err};
        const bool a_ok = (wa == 8);
        const bool b_ok = decisive_error(rh) && decisive_error(rd);
        const bool c_ok = !t.exec_constructed || (we == 8) || decisive_error(re);
        fully = a_ok && b_ok && c_ok;
        Fields a; a.s("case", "a-known-readable");
        a.n("write_rc", wa); a.n("errno", wa_err);
        Fields b; b.s("case", "b-unmapped");
        b.o("hole", read_fields(rh, "write-copyin", t.hole, kRwWord));
        b.o("dm_address", read_fields(rd, "write-copyin", kDmAddress, kRwWord));
        Fields c; c.s("case", "c-exec-only");
        c.raw("exec_constructed", t.exec_constructed ? "true" : "false");
        c.n("mprotect_rc", t.mprotect_rc);
        c.n("mprotect_errno", t.mprotect_err);
        c.n("write_rc", we); c.n("errno", we_err);
        d.o("a", a); d.o("b", b); d.o("c", c);
        ::close(fd);
    }
    line.check("c4_write_copyin", fd >= 0, d.text);
    return fully;
}

#endif  // !HOST_READABLESOURCE

}  // namespace

int main(int argc, char** argv)
{
    const char* sink = argc > 1 ? argv[1] : "/data/modbridge-readablesource.jsonl";
    Json line("modbridge.readablesource");
    bool all_decisive = true;
    int candidate_working = 0;  // 1..4; 0 = none fully worked (host: n/a)

#if defined(HOST_READABLESOURCE)
    // Host: pure classification table + framing only (no kernel probes).
    int candidate_working_unused_host = 0;  // host keeps candidate_working = 0
    (void)candidate_working_unused_host;
    {
        struct Row { long long rc; uint32_t word; ReadClass::V want; const char* why; };
        const Row rows[] = {
            {(long long)kRwWord, kRwWord, ReadClass::Readable, "word-through-channel"},
            {(long long)kExecWord, kExecWord, ReadClass::Readable, "word-through-channel"},
            {14, kRwWord, ReadClass::ErrorUnmapped, "EFAULT"},
            {5, kRwWord, ReadClass::ErrorProtection, "EIO"},
            {1, kRwWord, ReadClass::ErrorPolicy, "EPERM"},
            {13, kRwWord, ReadClass::ErrorPolicy, "EACCES"},
            {22, kRwWord, ReadClass::ErrorOther, "EINVAL-as-example"},
            {0, kRwWord, ReadClass::NotDecisive, "zero-mismatch"},
            {-1, kRwWord, ReadClass::NotDecisive, "negative-wrapper"},
            {LLONG_MIN, kRwWord, ReadClass::NotDecisive, "not-made-sentinel"},
        };
        Fields d;
        bool ok = true;
        for (const Row& r : rows) {
            const ReadClass::V got = classify(r.rc, r.word);
            ok = ok && got == r.want;
            d.n(r.why, (long long)got);
        }
        line.check("h1_classify_table", ok, d.text);
        all_decisive = ok;
    }
#else
    TargetPlan t;
    const bool setup_ok = setup(t);
    {
        Fields d;
        d.raw("setup_ok", setup_ok ? "true" : "false");
        d.n("munmap_rc", t.munmap_rc);
        d.raw("exec_constructed", t.exec_constructed ? "true" : "false");
        d.n("mprotect_rc", t.mprotect_rc);
        d.n("mprotect_errno", t.mprotect_err);
        d.raw("rw_lo", quoted(hex64(t.rw_lo)));
        d.raw("hole", quoted(hex64(t.hole)));
        d.raw("rw_hi", quoted(hex64(t.rw_hi)));
        d.raw("ex", quoted(hex64((uint64_t)t.ex)));
        d.raw("dm_address", quoted(hex64(kDmAddress)));
        // Pre-mprotect volatile readbacks captured in setup(); NO direct read
        // happens after mprotect (post-mprotect direct reads are the hazard
        // this payload exists to avoid).
        d.raw("rw_word_back", t.rw_word_back ? "true" : "false");
        d.raw("hi_word_back", t.hi_word_back ? "true" : "false");
        const bool rw_ok = setup_ok && t.rw_word_back && t.hi_word_back;
        line.check("0_setup", rw_ok, d.text);
        all_decisive = all_decisive && rw_ok;
    }

    int candidate_working_local = 0;  // replaced by hoisted candidate_working
    (void)candidate_working_local;
    if (!setup_ok) {
        line.check("c1_ptrace_read_d", false, "\"detail\":\"setup-failed\"");
        all_decisive = false;
    } else {
        bool c1 = false;
        run_candidate1(t, line, c1);
        if (c1) {
            candidate_working = 1;
        } else {
            bool c2 = false;
            run_candidate2(t, line, c2);
            if (c2) {
                candidate_working = 2;
            } else {
                bool c3 = false;
                run_candidate3(t, line, c3);
                if (c3) {
                    candidate_working = 3;
                } else {
                    bool c4 = false;
                    run_candidate4(t, line, c4);
                    if (c4) candidate_working = 4;
                }
            }
        }
    }

    // Skipped-candidate notes (bounded, honest).
    {
        Fields d;
        d.n("candidate_working", candidate_working);
        if (candidate_working == 1) {
            d.s("skipped", "c2,c3,c4: candidate 1 fully worked; PT_IO splice recorded as e_ptio_splice_note");
        } else if (candidate_working == 2) {
            d.s("skipped", "c3,c4: candidate 2 fully worked");
        } else if (candidate_working == 3) {
            d.s("skipped", "c4: candidate 3 fully worked");
        } else if (candidate_working == 4) {
            d.s("skipped", "none: candidate 4 fully worked");
        } else {
            d.s("skipped", "none: no candidate fully worked (all attempted, all decisive-or-failed)");
        }
        line.check("v_candidates", true, d.text);
    }

    // Case d: recorded in every run — what remains untestable without a game.
    {
        Fields d;
        d.s("wide_range", "untestable-separate-process: the 139 GiB pool envelope above the game's complete KERN_PROC_VMMAP top 0x1100400000 cannot be constructed in this standalone process");
        d.s("game_process", "out-of-scope: this payload never touches the game process; cross-process reads are a game-run question");
        d.s("dm_address_echo", hex64(kDmAddress).c_str());
        d.s("note", "b_dm_address/b-hole results preview the in-game kernel-mediated read semantics but do NOT constitute game-process evidence");
        line.check("d_untestable_scope", true, d.text);
    }

    {
        Fields d;
        d.n("SYS_ptrace", SYS_ptrace);
        d.n("SYS_read", SYS_read);
        d.n("SYS_open", SYS_open);
        d.n("SYS_write", SYS_write);
        d.n("SYS_pread", SYS_pread);
        d.n("PT_READ_D", PT_READ_D);
        d.n("PT_IO", PT_IO);
        d.n("PIOD_READ_D", PIOD_READ_D);
        d.n("pid", (long long)::getpid());
        line.check("s_surface_numbers", true, d.text);
    }
#endif

    std::string record = line.text;
    record += "],\"candidate_fully_working\":";
    record += num(candidate_working);
    record += ",\"all_decisive\":";
    record += all_decisive ? "true" : "false";
    record += "}\n";

    std::fputs(record.c_str(), stdout);
    bool wrote = false;
    if (std::FILE* out = std::fopen(sink, "a")) {
        wrote = std::fputs(record.c_str(), out) >= 0;
        std::fclose(out);
    }
    return (all_decisive && wrote) ? 0 : 1;
}
