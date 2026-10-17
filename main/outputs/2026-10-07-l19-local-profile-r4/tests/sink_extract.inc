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

// L17: fully open bounded single-use sinks during module_start. No target
// memory is read until a fresh delayed probe. Restored namespaces cannot
// require a late create/reopen; positional readback uses the same descriptor.
#include "CopyinSink.hpp"
#include "CopyinStartup.hpp"
#include "CodeSignatureProfile.hpp"
#include "ImageCoverage.hpp"
extern "C" int64_t sceKernelPread(int, void *, size_t, int64_t);
extern "C" int sceKernelUnlink(const char *);
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
static l13_sink::InitializerOwner g_copyin_initializer_owner;
static void l13_format_probe_sink(char *, uint64_t, uint32_t);
static void bridge_copyin_directory_release();

static void bridge_ascii_diagnostic(const char *path,const char* length_tag,const char* word_tag)
{
    // Lossless ASCII, four little-endian bytes per repeated word. Length and
    // word order reconstruct the exact path without a new text logger/import.
    size_t n = 0;
    while (path[n] != '\0') ++n;
    ::diagnostic_event(length_tag, static_cast<uint32_t>(n));
    for (size_t i = 0; i < n; i += 4) {
        uint32_t word = 0;
        for (size_t j = 0; j < 4 && i + j < n; ++j)
            word |= uint32_t(static_cast<unsigned char>(path[i + j])) << (8 * j);
        ::diagnostic_event(word_tag, word);
    }
}
static void bridge_copyin_path_diagnostic(const char* path)
{
    bridge_ascii_diagnostic(path,"pool_copyin_path_len","pool_copyin_path_word");
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
    // op1=open, 2=copyin write, 3=positional read, 4=close, 5=unlink.
    // Short/zero/malformed
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
    g_copyin_initializer_owner.prepare();
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
        ::diagnostic_event("copyin_startup_ordinal", i + 1u);
        ::diagnostic_event("copyin_startup_open_raw", static_cast<uint32_t>(handle.fd));
        ::diagnostic_event("copyin_startup_open_errno", static_cast<uint32_t>(sink_err));
        ::diagnostic_event("pool_copyin_open_flags", static_cast<uint32_t>(kCopyinSinkFlags));
        if (handle.fd < 0 || sink_err != 0) {
            bridge_copyin_file_failure(1, handle.path, kCopyinSinkFlags, handle.fd, sink_err, 1);
            handle.fd = -1;
            bridge_copyin_directory_release(); // no partial bank, no leaked owners
            return;
        }
        ::diagnostic_event("copyin_startup_fd", static_cast<uint32_t>(handle.fd));
        // This exact pathname was created exclusively by this startup. Unlink
        // now while lookup is available; the open descriptor remains usable.
        // A denied unlink is diagnostic only: retain the file, never retry it
        // later and never delete paths from a previous session.
        errno = 0;
        const int unlink_raw = sceKernelUnlink(handle.path);
        const int unlink_err = l13_sink::error(unlink_raw, errno);
        ::diagnostic_event("copyin_unlink_raw", static_cast<uint32_t>(unlink_raw));
        ::diagnostic_event("copyin_unlink_errno", static_cast<uint32_t>(unlink_err));
        if (unlink_raw != 0 || unlink_err != 0)
            bridge_copyin_file_failure(5, handle.path, kCopyinSinkFlags, unlink_raw, unlink_err, 1);
    }
    ::diagnostic_event("copyin_startup_handles", kCopyinHandleLimit);
    ::diagnostic_event("copyin_startup_elapsed_ms", static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        TestClock::now() - start).count()));
}
static void bridge_copyin_directory_release()
{
    for (auto& handle : g_copyin_handles) {
        if (handle.fd < 0) continue;
        const int fd = std::exchange(handle.fd, -1);
        errno = 0;
        const int raw = sceKernelClose(fd);
        const int err = l13_sink::error(raw, errno);
        if (raw != 0 || err) bridge_copyin_file_failure(4, handle.path, kCopyinSinkFlags, raw, err, 2);
    }
    if (g_copyin_parent_fd >= 0) {
        const int fd = std::exchange(g_copyin_parent_fd, -1);
        errno = 0;
        const int raw = sceKernelClose(fd);
        const int err = l13_sink::error(raw, errno);
        if (raw != 0 || err) bridge_copyin_file_failure(4, kProbeParent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW, raw, err, 2);
    }
}
extern "C" int bridge_copyin_initializer_enter(void)
{
    if (!g_copyin_initializer_owner.enter()) return 0;
    // The gate stores the actual std::thread::id. This token is only a log
    // identity, not an OS numeric TID or evidence of descriptor headroom.
    const uint64_t token = std::hash<std::thread::id>{}(std::this_thread::get_id());
    ::diagnostic_event("copyin_initializer_id_hash_lo", static_cast<uint32_t>(token));
    ::diagnostic_event("copyin_initializer_id_hash_hi", static_cast<uint32_t>(token >> 32));
    return 1;
}
extern "C" void bridge_copyin_initializer_complete(void)
{
    if (!g_copyin_initializer_owner.allows()) return;
    bridge_copyin_directory_release();
    // Keep both owned captures and fault evidence for the rest of initialization.
}
extern "C" void bridge_copyin_initializer_leave(void)
{
    if (!g_copyin_initializer_owner.allows()) return;
    // Also covers exceptions before seed-check entry. Completion and close
    // sentinels make this an idempotent fallback on the initializer itself.
    bridge_copyin_directory_release();
    g_copyin_initializer_owner.leave();
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
    if (!g_copyin_initializer_owner.allows()) return 1; // fail closed, no ledger read
    return g_copyin_faults.overlaps(addr, len) ? 1 : 0;
}

// L18: the loader prepares one authenticated sparse cache before constructing
// the worker. It stays immutable until failed-start cleanup or joined stop.
static rc::codecache::Cache g_code_cache;
static void bridge_code_cache_clear_after_join() noexcept { g_code_cache.clear(); }
static void bridge_startup_resources_release()
{
    bridge_copyin_directory_release();
    bridge_code_cache_clear_after_join();
}
static rc::codecache::Result bridge_code_cache_prepare(uint64_t observed_base,
    uint64_t resolved_base,uint64_t resolved_end,uint32_t base_source,
    bool module_info=false,const rc::imagecoverage::Mapping* modules=nullptr,size_t module_count=0)
{
    struct Ops {
        int open(const char* path) { return sceKernelOpen(path,O_RDONLY|O_NOFOLLOW,0); }
        int64_t pread(int fd,void* bytes,size_t length,uint64_t offset) {
            return sceKernelPread(fd,bytes,length,static_cast<int64_t>(offset));
        }
        int close(int fd) { return sceKernelClose(fd); }
        uint64_t now_ms() { return bridge_copyin_now_ms(); }
    } ops;
    auto result=g_code_cache.prepare(rc::codeprofile::kProfile,observed_base,
        resolved_base,resolved_end,g_copyin_generation,ops);
    if(result.ok) {
        auto& m=g_code_cache.metadata;m.base_source=base_source;m.module_info=module_info;
        if(module_info && modules && module_count>0 && module_count<=4) {
            for(size_t i=0;i<module_count;++i) {
                if(!modules[i].start||modules[i].end<=modules[i].start) {
                    result.ok=false;result.reason="loaded module-info segment bounds invalid";break;
                }
                m.reported[m.reported_count++]={modules[i].start,modules[i].end,modules[i].protection};
            }
        } else if(module_info) {
            result.ok=false;result.reason="loaded module-info segment geometry unavailable";
        }
        if(!result.ok)g_code_cache.clear();
    }
    ::diagnostic_event("code_image_cache_ok",result.ok?1u:0u);
    ::diagnostic_event("code_image_source_configured",1u);
    ::diagnostic_event("code_image_open_fd",static_cast<uint32_t>(result.open_raw));
    ::diagnostic_event("code_image_open_errno",static_cast<uint32_t>(result.open_error));
    ::diagnostic_event("code_image_read_errno",static_cast<uint32_t>(result.read_error));
    ::diagnostic_event("code_image_read_raw_low",static_cast<uint32_t>(result.read_raw));
    ::diagnostic_event("code_image_read_raw_high",static_cast<uint32_t>(uint64_t(result.read_raw)>>32));
    ::diagnostic_event("code_image_bytes_read_low",static_cast<uint32_t>(result.bytes_read));
    ::diagnostic_event("code_image_bytes_read_high",static_cast<uint32_t>(result.bytes_read>>32));
    ::diagnostic_event("code_image_close_raw",static_cast<uint32_t>(result.close_raw));
    ::diagnostic_event("code_image_close_errno",static_cast<uint32_t>(result.close_error));
    ::diagnostic_event("code_image_verified_pins",static_cast<uint32_t>(result.verified_pins));
    ::diagnostic_event("code_image_elapsed_ms",static_cast<uint32_t>(ops.now_ms()-result.start_ms));
    ::diagnostic_event("code_image_shared_budget_ms",60000u);
    bridge_ascii_diagnostic(rc::codeprofile::kProfile.path,"code_image_path_len","code_image_path_word");
    bridge_ascii_diagnostic(result.reason,"code_image_reason_len","code_image_reason_word");
    if(result.failed_io_op) {
        const int64_t raw=result.failed_io_op==1?result.open_raw:result.failed_io_op==4?result.close_raw:result.read_raw;
        const int error=result.failed_io_op==1?result.open_error:result.failed_io_op==4?result.close_error:result.read_error;
        ::diagnostic_event("code_image_file_context",1u);
        bridge_copyin_file_failure(result.failed_io_op,rc::codeprofile::kProfile.path,O_RDONLY|O_NOFOLLOW,raw,error,1u);
    }
    return result;
}
extern "C" int bridge_code_cache_context(uint64_t module_base,const rc::codecache::Metadata** out)
{
    if(out)*out=nullptr;
    if(!out||!g_copyin_initializer_owner.allows())return 0;
    if(!g_code_cache.identity(module_base))return 0;
    *out=&g_code_cache.metadata;return 1;
}
extern "C" int bridge_code_cache_signature(uint64_t module_base,uint64_t address,
    const uint8_t* expected,size_t length)
{
    if(!g_copyin_initializer_owner.allows())return 0;
    if(!g_code_cache.identity(module_base)||bridge_copyin_range_faulted(address,length))return 0;
    return g_code_cache.prefix(address,expected,length)?1:0;
}
extern "C" int bridge_write_copyin_capture(uint64_t addr, const uint8_t **out_bytes, uint32_t *out_len)
{
    if (out_bytes == nullptr || out_len == nullptr) return 0;
    *out_bytes = nullptr;
    *out_len = 0;
    if (!g_copyin_initializer_owner.allows()) return 0;
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
    // A foreign refusal must not write telemetry or touch any bank/ring/ledger
    // state. Only the synchronized owner gate is consulted on this path.
    if (!g_copyin_initializer_owner.allows()) return -1;
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
    const int fd = std::exchange(handle.fd, -1);
    const char *probe_sink = handle.path;
    int close_rc = 0;
    int close_err = 0;
    l13_sink::CompletionCleanup close_handle{[&]() {
        errno = 0;
        close_rc = sceKernelClose(fd);
        close_err = l13_sink::error(close_rc, errno);
        if (close_rc != 0 || close_err)
            bridge_copyin_file_failure(4, probe_sink, kCopyinSinkFlags, close_rc, close_err, 2);
    }};
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
    close_handle.complete();
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

