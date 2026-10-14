// worker-io-controls HOST TEST (2026-10-05, macOS freestanding).
//
// Compiles the staged bridge's HOST-TEST-EXTRACT region VERBATIM
// (io_controls_extract.inc, extracted by extract_region.sh from
// ../staging/runtime_start_bridge_r14_diag_spanpub.cpp — the exact shipped
// bytes) against mocked sceKernel*/stdio, and asserts:
//   [1] Control A states: missing sink (0 + ENOENT) / readable (1, rc 32,
//       head words) / short (2) / empty (3); fresh errno per operation (a
//       stale errno from a previous call is NEVER reported).
//   [2] Control B paths: verified (1) / open failed (0) / write short (3) /
//       read-back mismatch (4) / read-back open failed (5) / read-back
//       short (6); per-operation fresh errno on every path.
//   [3] Control C: created (1, sibling actually removed by the best-effort
//       POSIX unlink) / open failed (0 + EACCES, unlink still attempted).
//   [4] Control D: runs ONLY when B verified (io_ctrl_d == 1 readable,
//       sink 64, R-4 capture slot holds the candidate bytes); SKIPPED
//       entirely when B fails (no io_ctrl_d* tag, no probe call);
//       refused (0) / unavailable (2) classifications including the direct
//       negative-errno error shape; invalid-request class (3) on a direct
//       probe call.
//   [5] Canary nonce: two boots differ; stale previous-boot content can
//       never pass the byte-exact read-back (code 4).
//   [6] Fixed order A < B < C < D in the emitted event stream.
//
// No composition runs, no console work, no game claims.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <utility>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

// ---- diagnostic channel recorder (the real bridge calls ::diagnostic_event)
static std::vector<std::pair<std::string, uint32_t>> g_events;
extern "C" void diagnostic_event(const char *tag, uint32_t value)
{
    g_events.emplace_back(std::string(tag), value);
}

// ---- stdio interposition: real calls bound BEFORE the macro renaming ------
static FILE *real_fopen(const char *p, const char *m) { return fopen(p, m); }
static size_t real_fread(void *b, size_t sz, size_t n, FILE *f) { return fread(b, sz, n, f); }
static int real_fclose(FILE *f) { return fclose(f); }
static int real_unlink(const char *p) { return unlink(p); }

// ---- fixture: a temp dir stands in for /data/plugins ----------------------
static char g_tmpdir[256];
static std::string g_sink;        // temp .../modbridge-copyin-sink.bin
static std::string g_ctest;       // temp .../modbridge-io-create-test.bin
static const char *kCTestLiteral = "/data/plugins/modbridge-io-create-test.bin";

static std::string translate_path(const char *p)
{
    const std::string prefix = "/data/plugins/";
    std::string s(p);
    if (s.rfind(prefix, 0) == 0)
    {
        return std::string(g_tmpdir) + "/" + s.substr(prefix.size());
    }
    return s;
}

// ---- mock stdio (definitions carry the names the macros rename to) --------
static int g_fopen_seq = 0;        // 1-based sequence of fopen calls per run
static int g_fread_cap_from = 0;   // apply the cap from this fopen seq on (0 = never)
static size_t g_fread_cap = 0;     // 0 = no cap
enum class RbFail { None, OpenFails, Tamper };
static RbFail g_rb_fail = RbFail::None;
static unsigned char g_tamper_bytes[32];
static int g_unlink_calls = 0;

static FILE *io_mock_fopen(const char *path, const char *mode)
{
    ++g_fopen_seq;
    if (g_rb_fail == RbFail::OpenFails)
    {
        errno = ENOENT;
        return nullptr;
    }
    return real_fopen(translate_path(path).c_str(), mode);
}
static size_t io_mock_fread(void *buf, size_t sz, size_t n, FILE *f)
{
    size_t want = sz * n;
    if (g_fread_cap && g_fopen_seq >= g_fread_cap_from && want > g_fread_cap)
    {
        want = g_fread_cap; // simulated short content: fewer bytes than asked
    }
    const size_t got = real_fread(buf, sz, want, f);
    if (g_rb_fail == RbFail::Tamper && got == 32)
    {
        memcpy(buf, g_tamper_bytes, 32); // simulated stale previous-boot sink
    }
    return got;
}
static int io_mock_fclose(FILE *f) { return real_fclose(f); }
static int io_mock_unlink(const char *path)
{
    ++g_unlink_calls;
    return real_unlink(translate_path(path).c_str());
}

// Macro renaming applied ONLY while the verbatim bridge region is included:
// every fopen/fread/fclose/unlink inside the extraction compiles into the
// mocks above; the shipped bridge bytes themselves are untouched.
#define fopen io_mock_fopen
#define fread io_mock_fread
#define fclose io_mock_fclose
#define unlink io_mock_unlink
#include "io_controls_extract.inc"
#undef fopen
#undef fread
#undef fclose
#undef unlink

// ---- mock sceKernel* (satisfy the region's extern declarations) -----------
enum class WShape { Ok, Ret1Fault, Neg14, Neg5, Short };
static WShape g_host_write_shape = WShape::Ok;  // canary / create-test writes
static WShape g_guest_write_shape = WShape::Ok; // candidate copyin writes
static bool g_ctest_open_fail = false;          // create-test open injects EACCES
static int g_cand_write_calls = 0;
static unsigned char g_last_canary[32];
static bool g_canary_seen = false;

// the mock "guest physical" backing for the window-E candidate address
static unsigned char g_src_backing[0x1000];
static const uint64_t kGuestAddr = 0x400000ull + 0x7dd2a40ull; // == kIoCtrlDCandidateAddr

extern "C" int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    const bool is_ctest = (std::strcmp(path, kCTestLiteral) == 0);
    if (is_ctest && g_ctest_open_fail)
    {
        errno = EACCES;
        return -1;
    }
    errno = 0;
    return ::open(translate_path(path).c_str(), flags, mode);
}
extern "C" int64_t sceKernelWrite(int fd, const void *buf, size_t n)
{
    const uint64_t a = reinterpret_cast<uint64_t>(buf);
    const bool in_guest = (a >= kGuestAddr && a - kGuestAddr < sizeof g_src_backing);
    const void *eff = buf;
    if (in_guest)
    {
        ++g_cand_write_calls;
        eff = g_src_backing + (a - kGuestAddr); // the kernel-mediated copyin read
    }
    else if (n == 32)
    {
        memcpy(g_last_canary, buf, 32); // record this boot's canary
        g_canary_seen = true;
    }
    const WShape shape = in_guest ? g_guest_write_shape : g_host_write_shape;
    switch (shape)
    {
    case WShape::Ret1Fault:
        errno = EFAULT;
        return -1; // shape (a): -1 + errno
    case WShape::Neg14:
        return -14; // shape (b): direct negative-errno return (EFAULT)
    case WShape::Neg5:
        return -5; // shape (b): direct negative-errno return (EIO)
    case WShape::Short:
        return (n > 10) ? 10 : (int64_t)n; // partial transfer
    case WShape::Ok:
        break;
    }
    errno = 0;
    return (int64_t)::write(fd, eff, n);
}
extern "C" int sceKernelClose(int fd)
{
    errno = 0;
    return ::close(fd);
}

// ---- tiny test framework ---------------------------------------------------
static int g_failures = 0;
static void expect(bool ok, const char *what)
{
    if (!ok)
    {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
    else
    {
        std::printf("ok:   %s\n", what);
    }
}
static void expect_eq(uint32_t got, uint32_t want, const char *what)
{
    if (got != want)
    {
        std::printf("FAIL: %s (got %u want %u)\n", what, got, want);
        ++g_failures;
    }
    else
    {
        std::printf("ok:   %s (%u)\n", what, got);
    }
}
static int64_t tag_idx(const char *tag)
{
    for (size_t i = 0; i < g_events.size(); ++i)
    {
        if (g_events[i].first == tag)
        {
            return (int64_t)i;
        }
    }
    return -1;
}
static uint32_t tag_val(const char *tag)
{
    const int64_t i = tag_idx(tag);
    return i < 0 ? 0xDEADBEEFu : g_events[(size_t)i].second;
}
static size_t tag_count(const char *tag)
{
    size_t c = 0;
    for (const auto &e : g_events)
    {
        if (e.first == tag)
        {
            ++c;
        }
    }
    return c;
}

// ---- fixture helpers --------------------------------------------------------
static void reset_fixture()
{
    g_events.clear();
    g_fopen_seq = 0;
    g_fread_cap_from = 0;
    g_fread_cap = 0;
    g_rb_fail = RbFail::None;
    g_unlink_calls = 0;
    g_host_write_shape = WShape::Ok;
    g_guest_write_shape = WShape::Ok;
    g_cand_write_calls = 0;
    g_ctest_open_fail = false;
    errno = 0;
    ::unlink(g_sink.c_str());
    ::unlink(g_ctest.c_str());
}
static void write_sink(const unsigned char *bytes, size_t n)
{
    FILE *f = fopen(g_sink.c_str(), "wb");
    if (f != nullptr)
    {
        if (n != 0)
        {
            fwrite(bytes, 1, n, f);
        }
        fclose(f);
    }
}

int main()
{
    // temp fixture dir
    snprintf(g_tmpdir, sizeof g_tmpdir, "/tmp/ioctrl_hosttest.XXXXXX");
    if (mkdtemp(g_tmpdir) == nullptr)
    {
        std::printf("FAIL: mkdtemp\n");
        return 2;
    }
    g_sink = std::string(g_tmpdir) + "/modbridge-copyin-sink.bin";
    g_ctest = std::string(g_tmpdir) + "/modbridge-io-create-test.bin";
    for (size_t i = 0; i < sizeof g_src_backing; ++i)
    {
        g_src_backing[i] = (unsigned char)((i * 7 + 0x5b) & 0xff);
    }

    // ------------------------------------------------------------------
    // [1a] Control A: missing sink (+ B open-failed, D skipped, stale errno)
    // ------------------------------------------------------------------
    reset_fixture();
    errno = 999; // stale errno poisoned before the whole run
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_a"), 0, "A1: io_ctrl_a == 0 (fopen failed)");
    expect_eq(tag_val("io_ctrl_a_rc"), 0xFFFFFFFFu, "A1: io_ctrl_a_rc == sentinel");
    expect_eq(tag_val("io_ctrl_a_errno"), (uint32_t)ENOENT, "A1: io_ctrl_a_errno == ENOENT (fresh, not the stale 999)");
    expect(tag_val("io_ctrl_a_errno") != 999u, "A1: stale errno never reported (A)");
    expect_eq(tag_count("io_ctrl_a_head"), 4u, "A1: four io_ctrl_a_head words emitted");
    expect_eq(tag_val("io_ctrl_a_head"), 0u, "A1: head words zero when nothing read");
    expect_eq(tag_val("io_ctrl_b"), 0, "A1: io_ctrl_b == 0 (sink open failed, no O_CREAT)");
    expect_eq(tag_val("io_ctrl_b_errno"), (uint32_t)ENOENT, "A1: io_ctrl_b_errno == ENOENT (fresh)");
    expect(tag_val("io_ctrl_b_errno") != 999u, "A1: stale errno never reported (B)");
    expect_eq(tag_val("io_ctrl_c"), 1, "A1: io_ctrl_c == 1 (creation independent of the sink)");
    expect(tag_idx("io_ctrl_d") < 0, "A1: D skipped — no io_ctrl_d tag");
    expect(tag_idx("pool_copyin_probe_fresh") < 0, "A1: D skipped — no probe call at all");
    expect(g_cand_write_calls == 0, "A1: D skipped — zero candidate writes");

    // ------------------------------------------------------------------
    // [1b/2a/3a/4a/6] All-good run: A readable, B verified, C created,
    // D readable + capture slot, fixed order.
    // ------------------------------------------------------------------
    reset_fixture();
    unsigned char marker[32];
    for (int i = 0; i < 32; ++i)
    {
        marker[i] = (unsigned char)(0xE0 + i);
    }
    write_sink(marker, 32);
    errno = 999; // stale errno poisoned before the whole run
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_a"), 1, "A2: io_ctrl_a == 1 (marker readable)");
    expect_eq(tag_val("io_ctrl_a_rc"), 32, "A2: io_ctrl_a_rc == 32");
    expect_eq(tag_val("io_ctrl_a_errno"), 0, "A2: io_ctrl_a_errno == 0 on success (fresh, not stale 999)");
    const uint32_t w0 = (uint32_t)marker[0] | ((uint32_t)marker[1] << 8) | ((uint32_t)marker[2] << 16) | ((uint32_t)marker[3] << 24);
    // the four head words cover the FIRST 16 bytes: word 3 = marker[12..15]
    const uint32_t w3 = (uint32_t)marker[12] | ((uint32_t)marker[13] << 8) | ((uint32_t)marker[14] << 16) | ((uint32_t)marker[15] << 24);
    expect_eq(tag_val("io_ctrl_a_head"), w0, "A2: head word 0 matches marker bytes");
    {
        size_t heads_seen = 0;
        uint32_t last_head = 0;
        for (const auto &e : g_events)
        {
            if (e.first == "io_ctrl_a_head")
            {
                last_head = e.second;
                ++heads_seen;
            }
        }
        expect(heads_seen == 4 && last_head == w3, "A2: head word 3 (4th emission) matches marker bytes");
    }
    expect_eq(tag_val("io_ctrl_b"), 1, "B-ok: io_ctrl_b == 1 (write + close-before read-back verified)");
    expect_eq(tag_val("io_ctrl_b_rc"), 32, "B-ok: io_ctrl_b_rc == 32 (read-back return)");
    expect_eq(tag_val("io_ctrl_b_errno"), 0, "B-ok: io_ctrl_b_errno == 0 (fresh, not stale 999)");
    expect(g_canary_seen, "B-ok: mock captured the 32-byte canary write");
    expect(memcmp(g_last_canary, "MBIO", 4) == 0 && g_last_canary[4] == 0x01u,
           "B-ok: canary carries the MBIO magic + format v1");
    {
        // read-back words must equal the recorded canary words
        std::vector<uint32_t> rb;
        for (const auto &e : g_events)
        {
            if (e.first == "io_ctrl_b_readback")
            {
                rb.push_back(e.second);
            }
        }
        bool words_ok = (rb.size() == 4);
        for (int w = 0; words_ok && w < 4; ++w)
        {
            uint32_t cw = (uint32_t)g_last_canary[4 * w] | ((uint32_t)g_last_canary[4 * w + 1] << 8) |
                          ((uint32_t)g_last_canary[4 * w + 2] << 16) | ((uint32_t)g_last_canary[4 * w + 3] << 24);
            words_ok = (rb[(size_t)w] == cw);
        }
        expect(words_ok, "B-ok: io_ctrl_b_readback words equal the canary bytes");
    }
    expect_eq(tag_val("io_ctrl_c"), 1, "C-ok: io_ctrl_c == 1 (created)");
    expect_eq(tag_val("io_ctrl_c_rc"), 8, "C-ok: io_ctrl_c_rc == 8");
    expect_eq(tag_val("io_ctrl_c_errno"), 0, "C-ok: io_ctrl_c_errno == 0 (fresh)");
    expect(g_unlink_calls == 1, "C-ok: best-effort POSIX unlink attempted exactly once");
    {
        FILE *f = fopen(g_ctest.c_str(), "rb");
        expect(f == nullptr, "C-ok: sibling file actually removed by the unlink");
        if (f != nullptr)
        {
            fclose(f);
        }
    }
    expect_eq(tag_val("io_ctrl_d"), 1, "D-ok: io_ctrl_d == 1 (candidate copyin readable)");
    expect_eq(tag_val("io_ctrl_d_rc"), 64, "D-ok: io_ctrl_d_rc == 64 (last chunk rc)");
    expect_eq(tag_val("io_ctrl_d_errno"), 0, "D-ok: io_ctrl_d_errno == 0 (fresh)");
    expect_eq(tag_val("io_ctrl_d_sink"), 64, "D-ok: io_ctrl_d_sink == 64 (accumulated write count)");
    expect_eq(tag_val("pool_copyin_probe_fresh"), 1, "D-ok: probe tag readable");
    {
        const uint8_t *cap = nullptr;
        uint32_t cap_len = 0;
        const int hit = bridge_write_copyin_capture(kGuestAddr, &cap, &cap_len);
        expect(hit == 1 && cap_len == 64, "D-ok: R-4 capture slot holds the candidate first read");
        bool cap_ok = (hit == 1 && cap != nullptr && cap_len == 64);
        for (uint32_t i = 0; cap_ok && i < cap_len; ++i)
        {
            cap_ok = (cap[i] == g_src_backing[i]);
        }
        expect(cap_ok, "D-ok: capture bytes equal the candidate backing bytes");
        const uint8_t *miss = nullptr;
        expect(bridge_write_copyin_capture(kGuestAddr + 0x100000, &miss, &cap_len) == 0,
               "D-ok: capture lookup by a different address misses");
    }
    expect(g_cand_write_calls == 1, "D-ok: exactly one candidate write chunk (64 bytes, single chunk)");
    expect(tag_idx("io_ctrl_a") >= 0 && tag_idx("io_ctrl_a") < tag_idx("io_ctrl_b") &&
               tag_idx("io_ctrl_b") < tag_idx("io_ctrl_c") && tag_idx("io_ctrl_c") < tag_idx("io_ctrl_d"),
           "order: A < B < C < D in the emitted event stream");

    // ------------------------------------------------------------------
    // [1c] Control A: short read (10-byte sink)
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 10);
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_a"), 2, "A3: io_ctrl_a == 2 (short read)");
    expect_eq(tag_val("io_ctrl_a_rc"), 10, "A3: io_ctrl_a_rc == 10");

    // ------------------------------------------------------------------
    // [1d] Control A: empty sink
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 0);
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_a"), 3, "A4: io_ctrl_a == 3 (empty)");
    expect_eq(tag_val("io_ctrl_a_rc"), 0, "A4: io_ctrl_a_rc == 0");

    // ------------------------------------------------------------------
    // [2b] Control B: write short -> 3, D skipped
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    g_host_write_shape = WShape::Short;
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_b"), 3, "B-short: io_ctrl_b == 3 (write short)");
    expect_eq(tag_val("io_ctrl_b_rc"), 10, "B-short: io_ctrl_b_rc == 10 (raw write return)");
    expect(tag_idx("io_ctrl_d") < 0 && g_cand_write_calls == 0,
           "B-short: D skipped (no tag, no candidate write)");

    // ------------------------------------------------------------------
    // [2c] Control B: read-back open failed -> 5 (fresh errno on the readback op)
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    g_rb_fail = RbFail::OpenFails;
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_b"), 5, "B-rbopen: io_ctrl_b == 5 (read-back open failed)");
    expect_eq(tag_val("io_ctrl_b_errno"), (uint32_t)ENOENT, "B-rbopen: io_ctrl_b_errno == ENOENT (fresh from the failing fopen)");
    expect_eq(tag_val("io_ctrl_b_rc"), 0xFFFFFFFFu, "B-rbopen: io_ctrl_b_rc == sentinel");
    expect(tag_idx("io_ctrl_d") < 0, "B-rbopen: D skipped");

    // ------------------------------------------------------------------
    // [2d] Control B: read-back short -> 6
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    g_fread_cap = 20;
    g_fread_cap_from = 2; // cap only the read-back fopen (A's marker fopen is seq 1)
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_a"), 1, "B-rbshort: Control A unaffected (seq-1 fopen uncapped)");
    expect_eq(tag_val("io_ctrl_b"), 6, "B-rbshort: io_ctrl_b == 6 (read-back short)");
    expect_eq(tag_val("io_ctrl_b_rc"), 20, "B-rbshort: io_ctrl_b_rc == 20");
    expect(tag_idx("io_ctrl_d") < 0, "B-rbshort: D skipped");

    // ------------------------------------------------------------------
    // [3b] Control C: open failed -> 0 + EACCES, unlink still attempted
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    g_ctest_open_fail = true;
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_c"), 0, "C-fail: io_ctrl_c == 0 (open failed)");
    expect_eq(tag_val("io_ctrl_c_errno"), (uint32_t)EACCES, "C-fail: io_ctrl_c_errno == EACCES");
    expect_eq(tag_val("io_ctrl_c_rc"), 0xFFFFFFFFu, "C-fail: io_ctrl_c_rc == sentinel");
    expect(g_unlink_calls == 1, "C-fail: unlink still attempted (best-effort, non-fatal)");
    expect_eq(tag_val("io_ctrl_d"), 1, "C-fail: D independent of C (runs when B passed)");

    // ------------------------------------------------------------------
    // [4b] Control D: kernel refuses the candidate (shape (a): -1 + EFAULT)
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    g_guest_write_shape = WShape::Ret1Fault;
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_d"), 0, "D-refused(a): io_ctrl_d == 0 (EFAULT -> refused)");
    expect_eq(tag_val("io_ctrl_d_errno"), (uint32_t)EFAULT, "D-refused(a): io_ctrl_d_errno == EFAULT (captured errno)");
    expect_eq(tag_val("io_ctrl_d_sink"), 0, "D-refused(a): io_ctrl_d_sink == 0");
    {
        // The capture store is process-persistent and earlier successful D
        // runs already filled the candidate slot, so the no-fill-on-failure
        // property is proven at an address NO successful probe ever used.
        g_guest_write_shape = WShape::Ret1Fault;
        const int rc2 = bridge_kernel_write_copyin_probe(kGuestAddr + 0x400000ull, 64);
        g_guest_write_shape = WShape::Ok;
        expect(rc2 == 0, "D-refused(a): direct refused probe at a fresh address -> 0");
        const uint8_t *cap = nullptr;
        uint32_t cap_len = 0;
        expect(bridge_write_copyin_capture(kGuestAddr + 0x400000ull, &cap, &cap_len) == 0,
               "D-refused(a): no capture slot filled on a non-admitted probe");
    }

    // ------------------------------------------------------------------
    // [4c] Control D: kernel refuses via the DIRECT negative-errno shape
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    g_guest_write_shape = WShape::Neg14;
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_d"), 0, "D-refused(b): io_ctrl_d == 0 (direct -EFAULT -> refused)");
    expect_eq(tag_val("io_ctrl_d_errno"), 14, "D-refused(b): io_ctrl_d_errno == 14 (effective code from -rc)");

    // ------------------------------------------------------------------
    // [4d] Control D: other kernel error (EIO) -> unavailable, both shapes
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    g_guest_write_shape = WShape::Neg5;
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_d"), 2, "D-unavail: io_ctrl_d == 2 (EIO -> unavailable, never a source claim)");
    expect_eq(tag_val("io_ctrl_d_errno"), 5, "D-unavail: io_ctrl_d_errno == 5");

    reset_fixture();
    write_sink(marker, 32);
    g_guest_write_shape = WShape::Short;
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_d"), 2, "D-partial: io_ctrl_d == 2 (partial transfer -> unavailable)");
    expect_eq(tag_val("io_ctrl_d_rc"), 10, "D-partial: io_ctrl_d_rc == 10 (raw rc)");
    expect_eq(tag_val("io_ctrl_d_sink"), 0, "D-partial: io_ctrl_d_sink == 0 (accounting)");

    // ------------------------------------------------------------------
    // [4e] Direct probe calls: invalid request class
    // ------------------------------------------------------------------
    reset_fixture();
    errno = 999;
    expect(bridge_kernel_write_copyin_probe(kGuestAddr, 0) == -1, "probe: len 0 -> -1");
    expect_eq(tag_val("pool_copyin_probe_fresh"), 3, "probe: len 0 -> fresh tag 3 (invalid request class)");
    expect(tag_idx("pool_copyin_probe_rc") < 0, "probe: len 0 emits only the fresh tag (compat shape)");
    expect(bridge_kernel_write_copyin_probe(kGuestAddr, 0x1000001ull) == -1, "probe: len > 16 MiB budget -> -1");
    expect_eq(tag_val("pool_copyin_probe_fresh"), 3, "probe: over-budget -> fresh tag 3");
    // (the invalid-request path performs no IO at all, so it must not touch
    // errno in either direction — nothing to assert there beyond the tags)

    // ------------------------------------------------------------------
    // [5] Canary nonce: boots differ; stale content can never pass
    // ------------------------------------------------------------------
    reset_fixture();
    write_sink(marker, 32);
    bridge_io_controls(); // boot 1
    unsigned char boot1[32];
    memcpy(boot1, g_last_canary, 32);
    expect(g_canary_seen, "nonce: boot 1 canary captured");
    ::usleep(2000); // cross the millisecond granularity of t_ms
    reset_fixture();
    write_sink(marker, 32);
    errno = 0;
    bridge_io_controls(); // boot 2
    expect(memcmp(boot1, g_last_canary, 32) != 0, "nonce: two boots produce DIFFERENT canaries");
    expect(memcmp(g_last_canary + 8, boot1 + 8, 8) != 0, "nonce: the t_ms nonce field itself differs");
    // stale-content validation: replay boot 1's canary as the read-back
    reset_fixture();
    write_sink(marker, 32);
    memcpy(g_tamper_bytes, boot1, 32);
    g_rb_fail = RbFail::Tamper; // simulates a sink holding the PREVIOUS boot's canary
    bridge_io_controls();
    expect_eq(tag_val("io_ctrl_b"), 4, "nonce: stale previous-boot content REJECTED (io_ctrl_b == 4)");
    expect(tag_idx("io_ctrl_d") < 0, "nonce: stale rejection keeps D skipped");

    // ------------------------------------------------------------------
    printf(g_failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
