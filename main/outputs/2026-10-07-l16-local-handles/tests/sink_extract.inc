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

// L16: fully open bounded single-use sinks during module_start. No target
// memory is read until a fresh delayed probe. Restored namespaces cannot
// require a late create/reopen; positional readback uses the same descriptor.
#include "CopyinSink.hpp"
#include "CopyinStartup.hpp"
extern "C" int64_t sceKernelPread(int, void *, size_t, int64_t);
extern "C" int sceKernelIsInSandbox(void);
extern "C" int sysctlbyname(const char *, void *, size_t *, const void *, size_t);
static const char *kProbeParent = "/data/plugins";
static int g_copyin_parent_fd = -1;
static uint32_t g_copyin_probe_nonce = 0;
static uint64_t g_copyin_generation = 0;
static uint32_t g_copyin_pid = 0;
static l13_sink::FaultLedger g_copyin_faults;
static constexpr uint32_t kCopyinHandleLimit = 64u; // existing attempt cap
static constexpr uint64_t kCopyinStartupBudgetMs = 5000u;
static constexpr int kCopyinSinkFlags = O_RDWR | O_CREAT | O_TRUNC | O_NOFOLLOW | O_EXCL;
struct CopyinHandle { int fd = -1; char path[96]{}; };
static CopyinHandle g_copyin_handles[kCopyinHandleLimit]{};
static void l13_format_probe_sink(char *, uint64_t, uint32_t);
static void bridge_copyin_directory_release();

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
// Read-only public API observations. Jail/sandbox unavailability remains
// UNKNOWN; uid0 is not evidence of a restored root, prison or credential set.
static void bridge_copyin_credentials(uint32_t phase)
{
    const int saved_errno = errno;
    ::diagnostic_event("copyin_cred_phase", phase); // 1 startup, 2 delayed
    ::diagnostic_event("copyin_uid", static_cast<uint32_t>(::getuid()));
    ::diagnostic_event("copyin_euid", static_cast<uint32_t>(::geteuid()));
    ::diagnostic_event("copyin_gid", static_cast<uint32_t>(::getgid()));
    ::diagnostic_event("copyin_egid", static_cast<uint32_t>(::getegid()));
    errno = 0;
    const int sandbox = sceKernelIsInSandbox();
    const int sandbox_error = l13_sink::error(sandbox, errno);
    ::diagnostic_event("copyin_sandbox_raw", static_cast<uint32_t>(sandbox));
    ::diagnostic_event("copyin_sandbox_available", (sandbox == 0 || sandbox == 1) && !sandbox_error ? 1u : 0u);
    ::diagnostic_event("copyin_sandbox_errno", static_cast<uint32_t>(sandbox_error));
    int jailed = -1;
    size_t size = sizeof jailed;
    errno = 0;
    const int jail_rc = sysctlbyname("security.jail.jailed", &jailed, &size, nullptr, 0);
    const int jail_err = l13_sink::error(jail_rc, errno);
    const bool known = jail_rc == 0 && !jail_err && size == sizeof jailed && (jailed == 0 || jailed == 1);
    ::diagnostic_event("copyin_jail_available", known ? 1u : 0u);
    ::diagnostic_event("copyin_jail_value", static_cast<uint32_t>(known ? jailed : -1));
    ::diagnostic_event("copyin_jail_raw", static_cast<uint32_t>(jail_rc));
    ::diagnostic_event("copyin_jail_errno", static_cast<uint32_t>(jail_err));
    errno = saved_errno;
}
static void bridge_copyin_file_failure(uint32_t op, const char *path, int flags,
                                       int64_t raw, int err, uint32_t phase)
{
    // op1=open, 2=copyin write, 3=positional read, 4=close. Short/zero/malformed
    // results use EIO, retaining raw bits and the actual source errno separately.
    bridge_copyin_path_diagnostic(path);
    ::diagnostic_event("copyin_file_op", op);
    ::diagnostic_event("copyin_file_flags", static_cast<uint32_t>(flags));
    ::diagnostic_event("copyin_file_raw_lo", static_cast<uint32_t>(raw));
    ::diagnostic_event("copyin_file_raw_hi", static_cast<uint32_t>(uint64_t(raw) >> 32));
    ::diagnostic_event("copyin_file_errno", static_cast<uint32_t>(err));
    bridge_copyin_credentials(phase);
}
static void bridge_copyin_directory_prepare()
{
    if (g_copyin_parent_fd >= 0) return;
    bridge_copyin_credentials(1u);
    const auto start = TestClock::now();
    const int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW;
    errno = 0;
    const int fd = sceKernelOpen(kProbeParent, flags, 0);
    const int err = l13_sink::error(fd, errno);
    ::diagnostic_event("pool_copyin_parent_flags", static_cast<uint32_t>(flags));
    ::diagnostic_event("pool_copyin_parent_raw", static_cast<uint32_t>(fd));
    ::diagnostic_event("pool_copyin_parent_errno", static_cast<uint32_t>(err));
    if (fd < 0 || err != 0) {
        bridge_copyin_file_failure(1, kProbeParent, flags, fd, err, 1);
        return;
    }
    g_copyin_parent_fd = fd;
    g_copyin_generation = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        start.time_since_epoch()).count());
    g_copyin_pid = static_cast<uint32_t>(::getpid());
    g_copyin_probe_nonce = 0;
    for (uint32_t i = 0; i < kCopyinHandleLimit; ++i) {
        auto& handle = g_copyin_handles[i];
        // Target address is unknown at startup; the delayed diagnostic records
        // it separately. PID/generation/ordinal make each process-owned sink unique.
        l13_format_probe_sink(handle.path, 0, i + 1);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            TestClock::now() - start).count();
        if (elapsed >= static_cast<int64_t>(kCopyinStartupBudgetMs)) {
            ::diagnostic_event("copyin_startup_timeout", static_cast<uint32_t>(elapsed));
            bridge_copyin_directory_release();
            return; // scheduling bound; cannot preempt a kernel call already entered
        }
        errno = 0;
        handle.fd = sceKernelOpen(handle.path, kCopyinSinkFlags, 0600);
        const int sink_err = l13_sink::error(handle.fd, errno);
        ::diagnostic_event("pool_copyin_open_flags", static_cast<uint32_t>(kCopyinSinkFlags));
        if (handle.fd < 0 || sink_err != 0) {
            bridge_copyin_file_failure(1, handle.path, kCopyinSinkFlags, handle.fd, sink_err, 1);
            handle.fd = -1;
            bridge_copyin_directory_release(); // no partial bank, no leaked owners
            return;
        }
    }
    ::diagnostic_event("copyin_startup_handles", kCopyinHandleLimit);
    ::diagnostic_event("copyin_startup_elapsed_ms", static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        TestClock::now() - start).count()));
}
static void bridge_copyin_directory_release()
{
    for (auto& handle : g_copyin_handles) {
        if (handle.fd < 0) continue;
        errno = 0;
        const int raw = sceKernelClose(handle.fd);
        const int err = l13_sink::error(raw, errno);
        handle.fd = -1;
        if (raw != 0 || err) bridge_copyin_file_failure(4, handle.path, kCopyinSinkFlags, raw, err, 2);
    }
    if (g_copyin_parent_fd >= 0) {
        errno = 0;
        const int raw = sceKernelClose(g_copyin_parent_fd);
        const int err = l13_sink::error(raw, errno);
        g_copyin_parent_fd = -1;
        if (raw != 0 || err) bridge_copyin_file_failure(4, kProbeParent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW, raw, err, 2);
    }
}
static uint64_t bridge_copyin_now_ms()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        TestClock::now().time_since_epoch()).count());
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
    // No path lookup after startup: each fresh attempt consumes a preopened
    // exclusive sink exactly once. No cached prologue or recycled file offset.
    if (g_copyin_probe_nonce >= kCopyinHandleLimit) return bridge_copyin_result(2, 0, ENOSPC, 0);
    auto& handle = g_copyin_handles[g_copyin_probe_nonce++];
    if (handle.fd < 0) return bridge_copyin_result(2, -1, ENOENT, 0);
    const int fd = handle.fd;
    const char *probe_sink = handle.path;
    bridge_copyin_path_diagnostic(probe_sink);
    ::diagnostic_event("pool_copyin_probe_addr_lo", static_cast<uint32_t>(addr));
    ::diagnostic_event("pool_copyin_probe_addr_hi", static_cast<uint32_t>(addr >> 32));
    ::diagnostic_event("pool_copyin_probe_len", static_cast<uint32_t>(len));
    ::diagnostic_event("pool_copyin_open_flags", static_cast<uint32_t>(kCopyinSinkFlags));
    ::diagnostic_event("pool_copyin_open_mode", 0600u);
    ::diagnostic_event("copyin_retained_fd", static_cast<uint32_t>(fd));
    bridge_copyin_credentials(2u);
    int err = 0;
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
    const uint32_t slot = g_write_copyin_capture_next;
    g_write_copyin_capture_valid[slot] = false;
    const uint64_t expected = len < 0x1000u ? len : 0x1000u;
    int64_t got = -1;
    if (faulted || landed != len || err != 0) {
        if (err != ETIMEDOUT) bridge_copyin_file_failure(2, probe_sink, kCopyinSinkFlags, raw, err, 2);
    } else if (bridge_copyin_now_ms() >= deadline_ms) {
        err = ETIMEDOUT;
    } else {
        errno = 0;
        // Pread does not depend on or change the write cursor. The handle was
        // opened O_RDWR at startup and is never replaced by a delayed reopen.
        got = sceKernelPread(fd, g_write_copyin_capture[slot], static_cast<size_t>(expected), 0);
        err = l13_sink::error(got, errno);
        if (err != 0 || got != static_cast<int64_t>(expected)) {
            if (!err) err = EIO;
            bridge_copyin_file_failure(3, probe_sink, kCopyinSinkFlags, got, err, 2);
        }
    }
    errno = 0;
    const int close_rc = sceKernelClose(fd);
    const int close_err = l13_sink::error(close_rc, errno);
    handle.fd = -1;
    if (close_rc != 0 || close_err) bridge_copyin_file_failure(4, probe_sink, kCopyinSinkFlags, close_rc, close_err, 2);
    if (faulted) return bridge_copyin_result(0, raw, err, landed);
    if (landed != len || err != 0) return bridge_copyin_result(2, raw, err, landed);
    if (close_rc != 0 || close_err) return bridge_copyin_result(2, close_rc, close_err, landed);
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

