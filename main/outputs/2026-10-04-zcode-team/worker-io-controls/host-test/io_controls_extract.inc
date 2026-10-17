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

// Sink paths (fixed bridge-owned /data names). The main sink is NEVER
// created here and NEVER unlinked (truncate-per-probe law): it pre-exists
// (4 KiB pre-created file of the 1047 boot) and every write opens it
// O_TRUNC. The creation-test sibling is the ONLY file this region creates,
// and it gets a best-effort POSIX unlink (may fail in-game; non-fatal).
static const char *kSinkPath = "/data/plugins/modbridge-copyin-sink.bin";
static const char *kIoCreateTestPath = "/data/plugins/modbridge-io-create-test.bin";

// Control D candidate-memory address: the validated data-window E anchor
// (the AES S-box head — igf7-verified UNIQUE in the retained image, a
// REQUIRED anchor) at the validated base bias. This is the one game-memory
// address this TU can name at module_start WITHOUT fabricating a pool
// constant; the initializer probes the live Blocks[0] candidate with the
// SAME function during seed validation (the R-4 flow). D's boot-time run
// proves the kernel-copyin route end-to-end on real game memory every
// boot, independent of the seeds path.
static constexpr uint64_t kIoCtrlDCandidateAddr = 0x400000ull + 0x7dd2a40ull; // window E runtime vaddr (bias 0x400000)
static constexpr uint64_t kIoCtrlDCandidateLen = 64u;                         // window E length (kDataWindowAnchors 'E')

// Little-endian u32 loader for the *_head/*_readback word emissions (file
// scope here; the anonymous-namespace ld32b above serves the walk code).
static uint32_t bridge_io_ld32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// The 32-byte bridge-owned canary: "MBIO" magic, format version 1 (LE),
// the boot t_ms nonce (u64 LE), then a nonce-derived filler. Every byte
// after the magic varies with the boot's t_ms, so content from a previous
// boot can NEVER pass the byte-exact read-back comparison — a stale sink
// cannot masquerade as this boot's write.
static void bridge_io_canary_build(unsigned char out[32], uint64_t t_ms) noexcept
{
    out[0] = 'M'; out[1] = 'B'; out[2] = 'I'; out[3] = 'O';
    out[4] = 0x01u; out[5] = 0x00u; out[6] = 0x00u; out[7] = 0x00u; // format v1
    for (int i = 0; i < 8; ++i)
        out[8 + i] = (unsigned char)((t_ms >> (8 * i)) & 0xffu);
    for (int i = 0; i < 16; ++i)
        out[16 + i] = (unsigned char)(((t_ms >> (8 * (i % 8))) & 0xffu) ^ (unsigned char)(0x5au + i));
}

// Telemetry of the MOST RECENT probe run (mirrored under io_ctrl_d_* by
// Control D; never consumed for admission — the probe's own return and
// tags remain the oracle). code uses the pool_copyin_probe_fresh classes.
struct BridgeProbeTelemetry { uint32_t code; uint32_t rc; uint32_t err; uint32_t sink; };
static BridgeProbeTelemetry g_last_probe_telemetry{0u, 0u, 0u, 0u};

// SINK-FED VALIDATION SUPPORT (R-4, unchanged from v5.2): bounded 3-slot
// capture store keyed by probed address — the first page of every fully
// verified probe lands here; the initializer looks captures up BY ADDRESS
// and refuses loudly on a miss (never a direct read fallback).
static uint8_t g_write_copyin_capture[3][0x1000]; // first page per probed block
static uint64_t g_write_copyin_capture_addr[3];   // probed range base per slot
static uint32_t g_write_copyin_capture_len[3];    // valid bytes per slot
static bool g_write_copyin_capture_valid[3];
static uint32_t g_write_copyin_capture_next = 0;  // round-robin within the 3 slots

extern "C" int bridge_write_copyin_capture(uint64_t addr, const uint8_t **out_bytes, uint32_t *out_len)
{
    if (out_bytes == nullptr || out_len == nullptr) {
        return 0;
    }
    for (uint32_t i = 0; i < 3; ++i) {
        if (g_write_copyin_capture_valid[i] && g_write_copyin_capture_addr[i] == addr) {
            *out_bytes = g_write_copyin_capture[i];
            *out_len = g_write_copyin_capture_len[i];
            return 1;
        }
    }
    return 0;
}

// The write-copyin probe — v5.3: same name, same signature, same
// classification, RECONCILED transport (Sony-native + close-before-
// readback). Returns (readable: 1 / refused: 0 / unavailable: -1).
// Diagnostic tags (pool_copyin_probe_*):
//   pool_copyin_probe_fresh  1 readable / 0 refused / 2 unavailable /
//                            3 invalid request class
//   pool_copyin_probe_rc     raw write rc of the last chunk (u32;
//                            0xFFFFFFFF encodes -1)
//   pool_copyin_probe_errno  effective code of the failing op (see the
//                            error-shape law above)
//   pool_copyin_probe_sink   final sink byte count (u32; <= 16 MiB)
extern "C" int bridge_kernel_write_copyin_probe(uint64_t addr, uint64_t len)
{
    static constexpr uint64_t kPage = 0x1000;
    static constexpr uint64_t kMaxProbeBytes = 0x1000000; // 16 MiB class budget cap
    if (len == 0 || len > kMaxProbeBytes) {
        g_last_probe_telemetry = BridgeProbeTelemetry{3u, 0u, 0u, 0u}; // invalid request class
        ::diagnostic_event("pool_copyin_probe_fresh", 3u);
        return -1; // no calls, no sink, no target bytes touched
    }
    // Real-existing-file sink open, Sony-native (the PROVEN 1047 pattern).
    // No O_CREAT: the sink pre-exists; a missing sink is the honest
    // unavailable class (and exactly what Control A/C observe per boot).
    errno = 0; // fresh per-operation capture: never a stale errno
    const int fd = sceKernelOpen(kSinkPath, O_WRONLY | O_TRUNC | O_NOFOLLOW, 0600);
    const int errno_at_open = errno;
    if (fd < 0) {
        const uint32_t rc_tag = (fd == -1) ? 0xFFFFFFFFu : static_cast<uint32_t>(-fd);
        const int err_code = (fd == -1) ? errno_at_open : -fd;
        g_last_probe_telemetry = BridgeProbeTelemetry{2u, rc_tag, static_cast<uint32_t>(err_code), 0u};
        ::diagnostic_event("pool_copyin_probe_fresh", 2u); // unavailable (sink open class)
        ::diagnostic_event("pool_copyin_probe_rc", rc_tag);
        ::diagnostic_event("pool_copyin_probe_errno", static_cast<uint32_t>(err_code));
        ::diagnostic_event("pool_copyin_probe_sink", 0u);
        return -1; // target bytes untouched
    }
    uint64_t landed = 0;    // write-return accounting (rc==chunk accumulation)
    uint64_t cursor = addr; // NOT dereferenced CPU-side: the kernel carries it
    int verdict = -1;       // unavailable unless proven otherwise
    uint32_t code = 2u;     // diagnostic class (unavailable default)
    bool completed = false; // every chunk fully accounted by write returns
    int64_t last_rc = 0;
    int err_code = 0;
    while (cursor < addr + len) {
        // Page-granular step: a chunk never crosses a page boundary, so a
        // refusal is unambiguous for that page. len==8 yields the single
        // 8-byte probe shape.
        const uint64_t page_remaining = kPage - (cursor & (kPage - 1));
        const uint64_t range_left = addr + len - cursor;
        const uint64_t chunk = page_remaining < range_left ? page_remaining : range_left;
        errno = 0; // fresh per-operation capture
        last_rc = sceKernelWrite(fd, reinterpret_cast<const void *>(cursor), static_cast<size_t>(chunk));
        err_code = (last_rc < -1) ? static_cast<int>(-last_rc) : errno; // error-shape law
        if (last_rc == static_cast<int64_t>(chunk)) {
            landed += static_cast<uint64_t>(chunk); // write-return accounting
            cursor += chunk;
            continue;
        }
        if (last_rc < 0) {
            // Error under either convention:
            //   EFAULT (14) -> REFUSED (0): the kernel refuses THIS source.
            //     NEVER read as "unmapped": PROT_NONE and non-allowlisted
            //     source classes refuse identically (address-class-unknown).
            //   anything else -> UNAVAILABLE (2): honest, never a source
            //     claim in either direction.
            if (err_code == EFAULT) {
                code = 0u;
                verdict = 0;
            } else {
                code = 2u;
                verdict = -1;
            }
            break;
        }
        // 0 < rc < chunk: partial transfer (never observed in the campaign)
        // — recorded honestly as unavailable with the raw rc/code, never
        // admitted, never claimed as a clean refusal.
        code = 2u;
        verdict = -1;
        break;
    }
    if (verdict == -1 && landed == len) {
        completed = true;
    }
    // THE session-1047 FIX (always): close the WRITE fd before the fopen
    // read-back — the sandbox FS does not publish unflushed writes from an
    // open second handle to a new fopen reader. 1047 transferred 0x20000
    // real pool bytes and lost only this race; 1059/1072's ENOENT was the
    // sink file-state, not the route.
    errno = 0; // fresh capture discipline (the read-back arbitrates below)
    (void)sceKernelClose(fd);
    // Read-back capture on the closed sink (the vacuity law using ONLY
    // stdio — fopen/fread, the proven live log-device route): reopen "rb"
    // and read the sink's FIRST page. The read return must equal
    // min(len, page): an empty/short read demotes to UNAVAILABLE (never
    // admits — this rules out vacuous sinks and a truncated file), a full
    // read both proves the bytes truly landed AND fills the R-4 capture
    // for the sink-fed validation.
    FILE *rs = nullptr;
    if (completed) {
        errno = 0; // fresh per-operation capture
        rs = fopen(kSinkPath, "rb");
        if (rs == nullptr) {
            completed = false; // sink vanished/unpublishable between close and reopen
        }
    }
    if (completed) {
        const long expect = (len < kPage) ? static_cast<long>(len) : static_cast<long>(kPage);
        errno = 0; // fresh per-operation capture
        const long got = (long)fread(g_write_copyin_capture[g_write_copyin_capture_next], 1,
                                     static_cast<size_t>(0x1000), rs);
        if (got != expect) {
            completed = false; // vacuous/truncated sink: never admitted
        } else {
            const uint32_t slot = g_write_copyin_capture_next;
            g_write_copyin_capture_addr[slot] = addr;
            g_write_copyin_capture_len[slot] = static_cast<uint32_t>(got);
            g_write_copyin_capture_valid[slot] = true;
            g_write_copyin_capture_next = (g_write_copyin_capture_next + 1u) % 3u;
        }
    }
    if (completed) {
        code = 1u;
        verdict = 1;
    }
    if (rs != nullptr) {
        fclose(rs);
    }
    g_last_probe_telemetry = BridgeProbeTelemetry{
        code, static_cast<uint32_t>(last_rc), static_cast<uint32_t>(err_code),
        static_cast<uint32_t>(landed)};
    ::diagnostic_event("pool_copyin_probe_fresh", code);
    ::diagnostic_event("pool_copyin_probe_rc", static_cast<uint32_t>(last_rc));
    ::diagnostic_event("pool_copyin_probe_errno", static_cast<uint32_t>(err_code));
    ::diagnostic_event("pool_copyin_probe_sink", static_cast<uint32_t>(landed));
    return verdict;
}

// ---- launch10-io: the batched IO controls (ALWAYS ON) ---------------------
// Called by module_start immediately after the spanpub block; independent
// of the seeds path — this evidence set is produced on EVERY boot that
// reaches the frozen-snapshot publication. One function, four controls, in
// this fixed order:
//   A  marker read of the EXISTING main sink (stdio, BEFORE any
//      truncation): the direct observability the 1047-vs-1059/1072
//      file-state question needs. io_ctrl_a: 1 marker readable /
//      0 fopen failed / 2 short read / 3 empty. io_ctrl_a_rc: the fread
//      return (0xFFFFFFFF when fopen itself failed). io_ctrl_a_errno:
//      fresh capture after the failing call. io_ctrl_a_head: the first
//      16 read bytes as 4 u32 words (LE, in order) for host comparison
//      against the known marker or prior-boot data.
//   B  canary write + read-back on the main sink through the PROVEN
//      route: sceKernelOpen(O_WRONLY|O_TRUNC|O_NOFOLLOW, 0600) ->
//      sceKernelWrite(32-byte t_ms-nonce canary) -> sceKernelClose ->
//      fopen/fread(32) -> byte-exact compare. io_ctrl_b: 1 verified /
//      0 open failed / 3 write short / 4 read-back mismatch /
//      5 read-back open failed / 6 read-back short. io_ctrl_b_rc /
//      io_ctrl_b_errno: the rc and effective code of the op that decided
//      the outcome (fresh per operation). io_ctrl_b_readback: the read-
//      back bytes as 4 u32 words.
//   C  creation test on an INDEPENDENT sibling (O_CREAT proven here, not
//      on the main sink): open/write(8)/close, io_ctrl_c: 1 created /
//      0 open failed + io_ctrl_c_rc/io_ctrl_c_errno, then a best-effort
//      POSIX ::unlink of THE SIBLING only (may fail in-game; non-fatal).
//      Independent of D.
//   D  candidate-memory copy — ONLY when B verified the write route:
//      the copyin probe above against the validated window-E candidate
//      (address-class evidence for the boot, R-4 capture slot filled).
//      io_ctrl_d mirrors the probe's pool_copyin_probe_fresh class;
//      io_ctrl_d_rc/io_ctrl_d_errno/io_ctrl_d_sink mirror its telemetry.
//      When B fails, D is SKIPPED entirely: no io_ctrl_d* tag, no probe
//      call — an unproven write route must not spend the candidate copy.
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
            b_err = static_cast<uint32_t>(wfd < -1 ? -wfd : errno); // error-shape law
            b_rc = (wfd == -1) ? 0xFFFFFFFFu : static_cast<uint32_t>(-wfd);
        } else {
            errno = 0; // fresh per-operation capture
            const int64_t w = sceKernelWrite(wfd, canary, sizeof canary);
            b_rc = static_cast<uint32_t>(w);
            b_err = static_cast<uint32_t>(w < -1 ? -w : errno); // error-shape law
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
            c_err = static_cast<uint32_t>(fd < -1 ? -fd : errno); // error-shape law
            c_rc = (fd == -1) ? 0xFFFFFFFFu : static_cast<uint32_t>(-fd);
        } else {
            errno = 0; // fresh per-operation capture
            static const unsigned char c_bytes[8] = {'M', 'B', 'C', 'R', 'E', 'A', 'T', '1'};
            const int64_t w = sceKernelWrite(fd, c_bytes, sizeof c_bytes);
            c_rc = static_cast<uint32_t>(w);
            c_err = static_cast<uint32_t>(w < -1 ? -w : errno); // error-shape law
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

// ---- HOST-TEST-EXTRACT-END -----------------------------------------------
