// ---- HOST-TEST-EXTRACT-BEGIN (io-controls + write-copyin probe) ----------

// The in-contract Sony-native route (session 968: the POSIX-layer fd
// wrappers fail in the sandboxed game process; session 1047: THIS route
// transferred real pool bytes in-game).
extern "C" int sceKernelOpen(const char *, int, uint16_t);
extern "C" int64_t sceKernelWrite(int, const void *, size_t);
extern "C" int sceKernelClose(int);

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x0100 // BSD/Prospero fcntl.h value; guarded for hosts
#endif

// Legacy diagnostic-only paths. Lazy admission probes use their own basename
// under the retained /data/plugins parent; Control A/B/C are not run at startup.
static const char *kSinkPath = "/data/plugins/modbridge-copyin-sink.bin";
static const char *kIoCreateTestPath = "/data/plugins/modbridge-io-create-test.bin";

// L13: retain only the parent directory during module startup. ploader restores
// root/jail directories after injection; delayed absolute /data lookups may then
// fail. No sink is created and no target memory is read by this lifecycle setup.
#include "CopyinSink.hpp"
#include "CopyinStartup.hpp"
extern "C" int _openat(int, const char *, int, ...);
extern "C" int64_t sceKernelRead(int, void *, size_t);
static const char *kProbeParent = "/data/plugins";
static int g_copyin_parent_fd = -1;
static uint32_t g_copyin_probe_nonce = 0;
static uint64_t g_copyin_generation = 0;
static uint32_t g_copyin_pid = 0;
static l13_sink::FaultLedger g_copyin_faults;

static void bridge_copyin_path_diagnostic(const char *path)
{
    // Lossless ASCII, four little-endian bytes per repeated word. Length and
    // word order reconstruct the exact path without a new text logger/import.
    size_t n = 0;
    while (path[n] != '\0') ++n;
    ::diagnostic_event("pool_copyin_path_len", static_cast<uint32_t>(n));
    for (size_t i = 0; i < n; i += 4) {
        uint32_t word = 0;
        for (size_t j = 0; j < 4 && i + j < n; ++j)
            word |= uint32_t(static_cast<unsigned char>(path[i + j])) << (8 * j);
        ::diagnostic_event("pool_copyin_path_word", word);
    }
}
static void bridge_copyin_directory_prepare()
{
    if (g_copyin_parent_fd >= 0) return;
    const int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW;
    errno = 0;
    const int fd = sceKernelOpen(kProbeParent, flags, 0);
    const int err = l13_sink::error(fd, errno);
    ::diagnostic_event("pool_copyin_parent_flags", static_cast<uint32_t>(flags));
    ::diagnostic_event("pool_copyin_parent_raw", static_cast<uint32_t>(fd));
    ::diagnostic_event("pool_copyin_parent_errno", static_cast<uint32_t>(err));
    if (fd >= 0 && err == 0) {
        g_copyin_parent_fd = fd;
        g_copyin_generation = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        g_copyin_pid = static_cast<uint32_t>(::getpid());
    }
}
static void bridge_copyin_directory_release()
{
    if (g_copyin_parent_fd >= 0) {
        (void)sceKernelClose(g_copyin_parent_fd);
        g_copyin_parent_fd = -1;
    }
}
static uint64_t bridge_copyin_now_ms()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
static void l13_format_probe_sink(char *out, uint64_t addr, uint32_t nonce)
{
    static const char prefix[] = "/data/plugins/modbridge-copyin-p";
    static const char hex[] = "0123456789abcdef";
    size_t o = 0;
    for (; prefix[o]; ++o) out[o] = prefix[o];
    for (int shift = 60; shift >= 0; shift -= 4) out[o++] = hex[(addr >> shift) & 15u];
    out[o++] = '-';
    for (int shift = 60; shift >= 0; shift -= 4) out[o++] = hex[(g_copyin_generation >> shift) & 15u];
    out[o++] = '-';
    for (int shift = 28; shift >= 0; shift -= 4) out[o++] = hex[(g_copyin_pid >> shift) & 15u];
    out[o++] = '-';
    for (int shift = 28; shift >= 0; shift -= 4) out[o++] = hex[(nonce >> shift) & 15u];
    out[o] = '\0';
}

// Control D is an explicit diagnostic entry only; module_start never runs it.
// Its address and 64-byte length are distinct from a pool-block probe.
static constexpr uint64_t kIoCtrlDCandidateAddr = 0x400000ull + 0x7dd2a40ull;
static constexpr uint64_t kIoCtrlDCandidateLen = 64u;
static uint32_t bridge_io_ld32(const unsigned char *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static void bridge_io_canary_build(unsigned char out[32], uint64_t t_ms) noexcept
{
    out[0] = 'M'; out[1] = 'B'; out[2] = 'I'; out[3] = 'O';
    out[4] = 1; out[5] = 0; out[6] = 0; out[7] = 0;
    for (int i = 0; i < 8; ++i) out[8 + i] = static_cast<unsigned char>(t_ms >> (8 * i));
    for (int i = 0; i < 16; ++i)
        out[16 + i] = static_cast<unsigned char>((t_ms >> (8 * (i % 8))) ^ (0x5au + i));
}
struct BridgeProbeTelemetry { uint32_t code; uint32_t rc; uint32_t err; uint32_t sink; };
static BridgeProbeTelemetry g_last_probe_telemetry{0u, 0u, 0u, 0u};
static uint8_t g_write_copyin_capture[3][0x1000];
static uint64_t g_write_copyin_capture_addr[3];
static uint32_t g_write_copyin_capture_len[3];
static bool g_write_copyin_capture_valid[3];
static uint32_t g_write_copyin_capture_next = 0;
extern "C" int bridge_copyin_range_faulted(uint64_t addr, uint64_t len)
{
    return g_copyin_faults.overlaps(addr, len) ? 1 : 0;
}
extern "C" int bridge_write_copyin_capture(uint64_t addr, const uint8_t **out_bytes, uint32_t *out_len)
{
    if (out_bytes == nullptr || out_len == nullptr) return 0;
    *out_bytes = nullptr;
    *out_len = 0;
    for (uint32_t i = 0; i < 3; ++i) {
        if (!g_write_copyin_capture_valid[i] || addr < g_write_copyin_capture_addr[i]) continue;
        const uint64_t offset = addr - g_write_copyin_capture_addr[i];
        if (offset >= g_write_copyin_capture_len[i]) continue;
        const uint32_t remaining = g_write_copyin_capture_len[i] - static_cast<uint32_t>(offset);
        if (bridge_copyin_range_faulted(addr, remaining)) return 0;
        *out_bytes = g_write_copyin_capture[i] + offset;
        *out_len = remaining;
        return 1;
    }
    return 0;
}
static int bridge_copyin_result(uint32_t code, int64_t raw, int err, uint64_t landed)
{
    g_last_probe_telemetry = {code, static_cast<uint32_t>(raw), static_cast<uint32_t>(err), static_cast<uint32_t>(landed)};
    ::diagnostic_event("pool_copyin_probe_fresh", code);
    ::diagnostic_event("pool_copyin_probe_rc", static_cast<uint32_t>(raw));
    ::diagnostic_event("pool_copyin_probe_errno", static_cast<uint32_t>(err));
    ::diagnostic_event("pool_copyin_probe_sink", static_cast<uint32_t>(landed));
    ::diagnostic_event("pool_copyin_probe_stop", static_cast<uint32_t>(landed));
    return code == 1 ? 1 : (code == 0 ? 0 : -1);
}

// Scheduling deadline only: no new operation begins after it. Kernel calls,
// diagnostics and mandatory descriptor cleanup cannot be preempted here.
extern "C" int bridge_kernel_write_copyin_probe_until(uint64_t addr, uint64_t len,
                                                       uint64_t deadline_ms)
{
    if (!l13_sink::valid_range(addr, len)) return bridge_copyin_result(3, 0, EINVAL, 0);
    if (bridge_copyin_range_faulted(addr, len)) return bridge_copyin_result(0, 0, EFAULT, 0);
    for (uint32_t i = 0; i < 3; ++i) {
        if (g_write_copyin_capture_valid[i] && addr < g_write_copyin_capture_addr[i] + g_write_copyin_capture_len[i] &&
            g_write_copyin_capture_addr[i] < addr + len) g_write_copyin_capture_valid[i] = false;
    }
    if (bridge_copyin_now_ms() >= deadline_ms) return bridge_copyin_result(2, 0, ETIMEDOUT, 0);
    if (g_copyin_parent_fd < 0) return bridge_copyin_result(2, -1, ENOENT, 0);
    // Retain evidence, with a hard 64-file process budget. No unsafe absolute
    // cleanup is attempted after the process namespace has been restored.
    if (g_copyin_probe_nonce >= 64u) return bridge_copyin_result(2, 0, ENOSPC, 0);
    char probe_sink[96];
    l13_format_probe_sink(probe_sink, addr, ++g_copyin_probe_nonce);
    const char *basename = probe_sink + sizeof("/data/plugins/") - 1;
    const int flags = O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_EXCL;
    bridge_copyin_path_diagnostic(probe_sink);
    ::diagnostic_event("pool_copyin_probe_addr_lo", static_cast<uint32_t>(addr));
    ::diagnostic_event("pool_copyin_probe_addr_hi", static_cast<uint32_t>(addr >> 32));
    ::diagnostic_event("pool_copyin_probe_len", static_cast<uint32_t>(len));
    ::diagnostic_event("pool_copyin_open_flags", static_cast<uint32_t>(flags));
    ::diagnostic_event("pool_copyin_open_mode", 0600u);
    ::diagnostic_event("pool_copyin_open_dirfd", static_cast<uint32_t>(g_copyin_parent_fd));
    if (bridge_copyin_now_ms() >= deadline_ms) return bridge_copyin_result(2, 0, ETIMEDOUT, 0);
    errno = 0;
    const int fd = _openat(g_copyin_parent_fd, basename, flags, 0600);
    int err = l13_sink::error(fd, errno);
    ::diagnostic_event("pool_copyin_open_raw", static_cast<uint32_t>(fd));
    ::diagnostic_event("pool_copyin_open_errno", static_cast<uint32_t>(err));
    if (fd < 0 || err != 0) return bridge_copyin_result(2, fd, err, 0);
    uint64_t landed = 0;
    int64_t raw = 0;
    bool faulted = false;
    while (landed < len) {
        if (bridge_copyin_now_ms() >= deadline_ms) { err = ETIMEDOUT; break; }
        const uint64_t cursor = addr + landed;
        const uint64_t page_left = 0x1000u - (cursor & 0xfffu);
        const uint64_t chunk = len - landed < page_left ? len - landed : page_left;
        errno = 0;
        raw = sceKernelWrite(fd, reinterpret_cast<const void *>(cursor), static_cast<size_t>(chunk));
        err = l13_sink::error(raw, errno);
        if (err != 0 || raw < 0) {
            if (err == EFAULT) {
                faulted = true;
                g_copyin_faults.record(addr, len);
                ::diagnostic_event("pool_copyin_probe_fault_off", static_cast<uint32_t>(landed));
                ::diagnostic_event("pool_copyin_fault_addr_lo", static_cast<uint32_t>(cursor));
                ::diagnostic_event("pool_copyin_fault_addr_hi", static_cast<uint32_t>(cursor >> 32));
            }
            break;
        }
        if (raw == 0 || static_cast<uint64_t>(raw) > chunk) { err = EIO; break; }
        landed += static_cast<uint64_t>(raw);
        if (static_cast<uint64_t>(raw) != chunk) {
            ::diagnostic_event("pool_probe_op_rc", static_cast<uint32_t>(raw));
            ::diagnostic_event("pool_probe_stop_addr_lo", static_cast<uint32_t>(addr + landed));
            ::diagnostic_event("pool_probe_stop_addr_hi", static_cast<uint32_t>((addr + landed) >> 32));
            ::diagnostic_event("pool_probe_op_pagealign", ((addr + landed) & 0x3fffu) == 0 ? 1u : 0u);
        }
    }
    errno = 0;
    const int close_rc = sceKernelClose(fd); // close BEFORE reopening through retained parent
    const int close_err = l13_sink::error(close_rc, errno);
    if (faulted) return bridge_copyin_result(0, raw, err, landed);
    if (landed != len || err != 0) return bridge_copyin_result(2, raw, err, landed);
    if (close_rc != 0) return bridge_copyin_result(2, close_rc, close_err, landed);
    if (bridge_copyin_now_ms() >= deadline_ms) return bridge_copyin_result(2, raw, ETIMEDOUT, landed);
    errno = 0;
    const int rfd = _openat(g_copyin_parent_fd, basename, O_RDONLY | O_NOFOLLOW, 0);
    err = l13_sink::error(rfd, errno);
    ::diagnostic_event("pool_copyin_readopen_raw", static_cast<uint32_t>(rfd));
    ::diagnostic_event("pool_copyin_readopen_errno", static_cast<uint32_t>(err));
    if (rfd < 0 || err != 0) return bridge_copyin_result(2, rfd, err, landed);
    const uint32_t slot = g_write_copyin_capture_next;
    g_write_copyin_capture_valid[slot] = false;
    const uint64_t expected = len < 0x1000u ? len : 0x1000u;
    int64_t got = -1;
    err = ETIMEDOUT;
    if (bridge_copyin_now_ms() < deadline_ms) {
        errno = 0;
        got = sceKernelRead(rfd, g_write_copyin_capture[slot], static_cast<size_t>(expected));
        err = l13_sink::error(got, errno);
    }
    (void)sceKernelClose(rfd);
    if (err != 0 || got != static_cast<int64_t>(expected)) return bridge_copyin_result(2, got, err, landed);
    if (bridge_copyin_now_ms() >= deadline_ms) return bridge_copyin_result(2, got, ETIMEDOUT, landed);
    g_write_copyin_capture_addr[slot] = addr;
    g_write_copyin_capture_len[slot] = static_cast<uint32_t>(got);
    g_write_copyin_capture_valid[slot] = true;
    g_write_copyin_capture_next = (slot + 1u) % 3u;
    return bridge_copyin_result(1, raw, 0, landed);
}
extern "C" int bridge_kernel_write_copyin_probe(uint64_t addr, uint64_t len)
{
    return bridge_kernel_write_copyin_probe_until(addr, len, UINT64_MAX);
}

// Legacy explicit diagnostics only. No module-start caller remains: running
// A/B/C/D eagerly would perform sink/oracle work before VirtualQuery admission.
extern "C" void bridge_io_controls(void)
{
    // Boot nonce: milliseconds of the process-steady clock — unique per
    // boot at the resolution that matters, and never derived from any
    // observable game state.
    const uint64_t t_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    // ---- Control A: marker read (before any truncation) ----
    {
        unsigned char head[32];
        memset(head, 0, sizeof head);
        uint32_t a_code = 0u;
        uint32_t a_rc = 0u;
        uint32_t a_err = 0u;
        errno = 0; // fresh per-operation capture
        FILE *f = fopen(kSinkPath, "rb");
        if (f == nullptr) {
            a_code = 0u; // fopen failed
            a_err = static_cast<uint32_t>(errno);
            a_rc = 0xFFFFFFFFu; // no fread happened
        } else {
            errno = 0; // fresh per-operation capture
            const size_t got = fread(head, 1, sizeof head, f);
            a_err = static_cast<uint32_t>(errno);
            a_rc = static_cast<uint32_t>(got);
            fclose(f);
            a_code = (got == sizeof head) ? 1u : ((got == 0) ? 3u : 2u);
        }
        ::diagnostic_event("io_ctrl_a", a_code);
        ::diagnostic_event("io_ctrl_a_rc", a_rc);
        ::diagnostic_event("io_ctrl_a_errno", a_err);
        for (int w = 0; w < 4; ++w) {
            ::diagnostic_event("io_ctrl_a_head", bridge_io_ld32(head + 4 * w));
        }
    }

    // ---- Control B: canary write + close-before read-back ----
    unsigned char canary[32];
    bridge_io_canary_build(canary, t_ms);
    unsigned char back[32];
    memset(back, 0, sizeof back);
    uint32_t b_code = 0u;
    uint32_t b_rc = 0u;
    uint32_t b_err = 0u;
    {
        errno = 0; // fresh per-operation capture
        const int wfd = sceKernelOpen(kSinkPath, O_WRONLY | O_TRUNC | O_NOFOLLOW, 0600);
        if (wfd < 0) {
            b_code = 0u; // open failed
            b_err = static_cast<uint32_t>(l13_sink::error(wfd, errno)); // error-shape law
            b_rc = static_cast<uint32_t>(wfd);
        } else {
            errno = 0; // fresh per-operation capture
            const int64_t w = sceKernelWrite(wfd, canary, sizeof canary);
            b_rc = static_cast<uint32_t>(w);
            b_err = static_cast<uint32_t>(l13_sink::error(w, errno)); // error-shape law
            errno = 0; // close is best-effort; the read-back arbitrates
            (void)sceKernelClose(wfd);
            if (w == static_cast<int64_t>(sizeof canary)) {
                errno = 0; // fresh per-operation capture
                FILE *rf = fopen(kSinkPath, "rb");
                if (rf == nullptr) {
                    b_code = 5u; // read-back open failed
                    b_err = static_cast<uint32_t>(errno);
                    b_rc = 0xFFFFFFFFu;
                } else {
                    errno = 0; // fresh per-operation capture
                    const size_t got = fread(back, 1, sizeof back, rf);
                    b_err = static_cast<uint32_t>(errno);
                    b_rc = static_cast<uint32_t>(got);
                    fclose(rf);
                    if (got != sizeof back) {
                        b_code = 6u; // read-back short
                    } else {
                        b_code = (memcmp(back, canary, sizeof canary) == 0) ? 1u : 4u;
                    }
                }
            } else {
                b_code = 3u; // write short (or failed): route unproven
            }
        }
    }
    ::diagnostic_event("io_ctrl_b", b_code);
    ::diagnostic_event("io_ctrl_b_rc", b_rc);
    ::diagnostic_event("io_ctrl_b_errno", b_err);
    for (int w = 0; w < 4; ++w) {
        ::diagnostic_event("io_ctrl_b_readback", bridge_io_ld32(back + 4 * w));
    }

    // ---- Control C: creation test (independent sibling) ----
    {
        errno = 0; // fresh per-operation capture
        const int fd = sceKernelOpen(kIoCreateTestPath,
                                     O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
        uint32_t c_code = 0u;
        uint32_t c_rc = 0u;
        uint32_t c_err = 0u;
        if (fd < 0) {
            c_code = 0u; // open failed
            c_err = static_cast<uint32_t>(l13_sink::error(fd, errno)); // error-shape law
            c_rc = static_cast<uint32_t>(fd);
        } else {
            errno = 0; // fresh per-operation capture
            static const unsigned char c_bytes[8] = {'M', 'B', 'C', 'R', 'E', 'A', 'T', '1'};
            const int64_t w = sceKernelWrite(fd, c_bytes, sizeof c_bytes);
            c_rc = static_cast<uint32_t>(w);
            c_err = static_cast<uint32_t>(l13_sink::error(w, errno)); // error-shape law
            errno = 0; // best-effort close: creation was decided by the open
            (void)sceKernelClose(fd);
            c_code = 1u; // created (O_CREAT open succeeded; a short write
                         // stays visible honestly in io_ctrl_c_rc)
        }
        ::diagnostic_event("io_ctrl_c", c_code);
        ::diagnostic_event("io_ctrl_c_rc", c_rc);
        ::diagnostic_event("io_ctrl_c_errno", c_err);
        // Best-effort cleanup of THE SIBLING only (POSIX unlink — may fail
        // in-game; non-fatal). The MAIN sink is never unlinked.
        (void)::unlink(kIoCreateTestPath);
    }

    // ---- Control D: candidate-memory copy (only when B verified) ----
    if (b_code == 1u) {
        // The probe emits its own pool_copyin_probe_* set; the io_ctrl_d_*
        // mirrors make the batched evidence set self-contained.
        (void)bridge_kernel_write_copyin_probe(kIoCtrlDCandidateAddr, kIoCtrlDCandidateLen);
        ::diagnostic_event("io_ctrl_d", g_last_probe_telemetry.code);
        ::diagnostic_event("io_ctrl_d_rc", g_last_probe_telemetry.rc);
        ::diagnostic_event("io_ctrl_d_errno", g_last_probe_telemetry.err);
        ::diagnostic_event("io_ctrl_d_sink", g_last_probe_telemetry.sink);
    }
    // B failed: D is deliberately SKIPPED — no io_ctrl_d* tag, no probe
    // call. An unproven write route never spends the candidate copy.
}

