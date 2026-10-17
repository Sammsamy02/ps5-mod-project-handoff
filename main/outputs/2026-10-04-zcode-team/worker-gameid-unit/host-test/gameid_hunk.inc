// ---- gameid (worker-gameid-unit staged NEXT candidate, 2026-10-05) ---------
// Self-reporting game identity so identity drift can never masquerade as a
// runtime bug (worker-game-version evidence dir + analyzer_v2 GAME-IDENTITY
// law). Emitted once at module_start, AFTER the spanpub block above (the call
// site sits right after supplier->capture(), so spanpub publish has already
// run). Design for /app0 FIRST: the bridge's own r10 probe (stat_app0 /
// stat_app0_ec) is the established /app0 evidence — 19 captured in-game
// sessions recorded stat_app0=0 ec=2 (ENOENT) with stat_ownmod=1, so on this
// target the /app0 stat may honestly fail; this unit then emits the errno and
// NOTHING else — no sha words, no version tag, no gamedata line. Absence is
// the honest PENDING; a fabricated identity would be the one unforgivable
// failure mode (analyzer_v2 hard-stops on ANY mismatch — a wrong-but-plausible
// tag would kill a good boot).
//
// Mechanisms, all NID-routed, NO raw syscalls (process-fatal in-game, 934):
//   stat  — std::filesystem::file_size/last_write_time(path, error_code), the
//           EXISTING route behind stat_app0/std::filesystem::is_regular_file
//           (libc++ -> NID-resolved libc stat; proven live: stat_ownmod=1).
//   read  — fopen/fread/fclose from the frozen import contract, bounded
//           256 KiB chunks (the .bss budget here is 256 KiB against a bridge
//           that already carries a ~38 MiB vmmap array). Budget: 194,241,424
//           bytes (both pinned eboot manifests' exact size) hashed once per
//           process at module_start — expected 0.2-1 s wall on target; the
//           measured value is emitted as gameid_sha_ms so the budget is
//           evidence, not a promise.
//   line  — the runtime-status gamedata row goes through the module's own
//           status-file mechanism shape (mfa_status POSIX open/write, PROVEN
//           in-game by session 934's runtime-status.txt): a second O_APPEND
//           whole-line write to the SAME <mod_root>/runtime-status.txt, taken
//           only AFTER StartActivatedRuntimeWithAck returned success (state 1
//           'activated' is written by the RuntimeAckSink before start()), so
//           the row lands after state 1 and can never be truncated away by
//           the sink's fresh-session open. It deliberately does NOT use
//           mfa_status_open (that O_TRUNCs — it would destroy the session
//           file) and does NOT forge a 'state\t' row (that would mint a seq
//           number owned by the sink's counter).
//
// Emitted tags (consumer: analyzer_v2 GAME-IDENTITY, worker-runbook-v2):
//   game_eboot_sha256_w0..w3      first 16 digest bytes as four BIG-ENDIAN
//                                 u32 words (analyzer reassembles pack(">I")
//                                 and compares against BOTH pinned manifests
//                                 3b6cfad4/afe0a2a5 — first 128 bits).
//   game_content_version_packed   (major<<24)|(minor<<16)|(patch<<8)|0 parsed
//                                 STRICTLY from /app0/sce_sys/pfs-version.dat
//                                 ASCII "AA.BBB.CCC"; pinned 02.003.000 ->
//                                 0x02030000. Any open/format failure: the
//                                 errno/format tag only, NEVER a guess.
//   gameid_eboot_size_low/high    stat size, established _low/_high halves.
//   gameid_eboot_mtime_low/high   raw file_time_type epoch count, same split.
//   gameid_stat_fold              size_lo ^ mtime_lo ^ mtime_hi — one-glance
//                                 drift tag (the "folded sig"), never primary.
//   gameid_stat_ec/open_ec/ver_ec/ver_fmt/sha_err/toobig   honest refusals.
//   gameid_sha_ms/gameid_unavailable/gameid_done           budget + summary.
// Runtime-only pins (NEVER asserted on host): decrypted-eboot
// 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc ->
// w0..w3 = 3b6cfad4 7dd17fea 17d429bc e80ae374; toplevel-eboot
// afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e ->
// w0..w3 = afe0a2a5 fc9e1c17 b215bdbb 3de3c80e.
static const char kGameidEbootPath[] = "/app0/eboot.bin";
static const char kGameidVersionPath[] = "/app0/sce_sys/pfs-version.dat";
static constexpr uint64_t kGameidMaxBytes = 1073741824ull;  // 1 GiB runtime cap
static constexpr size_t kGameidChunk = 262144u;             // 256 KiB .bss

// Self-contained SHA-256 (public-domain shape, no external symbols). Host
// tests (worker-gameid-unit/host-test) extract EXACTLY these marker-bounded
// lines from THIS staged file and drive the NIST vectors against them, so the
// tested bytes are the shipped bytes.
// [gameid-sha256-BEGIN]
struct GameidSha256 {
    uint32_t h[8];
    uint64_t bits;        // total message length in bits
    unsigned char buf[64];
    unsigned buflen;
};

static const uint32_t kGameidSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t gameid_sha256_ror(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32u - n));
}

static void gameid_sha256_init(GameidSha256 &c)
{
    static const uint32_t iv[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    for (unsigned i = 0; i < 8u; ++i) c.h[i] = iv[i];
    c.bits = 0;
    c.buflen = 0;
}

static void gameid_sha256_block(GameidSha256 &c, const unsigned char *p)
{
    uint32_t w[64];
    for (unsigned i = 0; i < 16u; ++i) {
        w[i] = (uint32_t)p[4u * i + 0u] << 24 | (uint32_t)p[4u * i + 1u] << 16 |
               (uint32_t)p[4u * i + 2u] << 8 | (uint32_t)p[4u * i + 3u];
    }
    for (unsigned i = 16u; i < 64u; ++i) {
        const uint32_t s0 = gameid_sha256_ror(w[i - 15u], 7) ^
                            gameid_sha256_ror(w[i - 15u], 18) ^ (w[i - 15u] >> 3);
        const uint32_t s1 = gameid_sha256_ror(w[i - 2u], 17) ^
                            gameid_sha256_ror(w[i - 2u], 19) ^ (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3];
    uint32_t e = c.h[4], f = c.h[5], g = c.h[6], h = c.h[7];
    for (unsigned i = 0; i < 64u; ++i) {
        const uint32_t S1 = gameid_sha256_ror(e, 6) ^ gameid_sha256_ror(e, 11) ^
                            gameid_sha256_ror(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = h + S1 + ch + kGameidSha256K[i] + w[i];
        const uint32_t S0 = gameid_sha256_ror(a, 2) ^ gameid_sha256_ror(a, 13) ^
                            gameid_sha256_ror(a, 22);
        const uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        const uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c.h[0] += a; c.h[1] += b; c.h[2] += cc; c.h[3] += d;
    c.h[4] += e; c.h[5] += f; c.h[6] += g; c.h[7] += h;
}

static void gameid_sha256_update(GameidSha256 &c, const unsigned char *p, size_t n)
{
    c.bits += (uint64_t)n * 8u;
    while (n > 0) {
        if (c.buflen == 0 && n >= 64u) {
            gameid_sha256_block(c, p);
            p += 64u;
            n -= 64u;
        } else {
            unsigned take = (unsigned)(64u - c.buflen);
            if ((size_t)take > n) take = (unsigned)n;
            for (unsigned i = 0; i < take; ++i) c.buf[c.buflen + i] = p[i];
            c.buflen += take;
            p += take;
            n -= take;
            if (c.buflen == 64u) {
                gameid_sha256_block(c, c.buf);
                c.buflen = 0;
            }
        }
    }
}

static void gameid_sha256_final(GameidSha256 &c, unsigned char out[32])
{
    const uint64_t bits = c.bits;
    unsigned char pad[72];  // 0x80 + zeros + 8-byte big-endian bit count
    unsigned padlen = (c.buflen < 56u) ? (56u - c.buflen) : (120u - c.buflen);
    pad[0] = 0x80;
    for (unsigned i = 1u; i < padlen; ++i) pad[i] = 0x00;
    for (unsigned i = 0; i < 8u; ++i)
        pad[padlen + i] = (unsigned char)((bits >> (56u - 8u * i)) & 0xffu);
    gameid_sha256_update(c, pad, (size_t)padlen + 8u);
    for (unsigned i = 0; i < 8u; ++i) {
        out[4u * i + 0u] = (unsigned char)(c.h[i] >> 24);
        out[4u * i + 1u] = (unsigned char)(c.h[i] >> 16);
        out[4u * i + 2u] = (unsigned char)(c.h[i] >> 8);
        out[4u * i + 3u] = (unsigned char)(c.h[i]);
    }
}
// [gameid-sha256-END]

// Bounded appends (no libc printf family — hand-rolled like
// lease_gap_append_hex above; decimal per the mfa_status put_u64 shape).
static void gameid_append(char *out, unsigned cap, unsigned &used, const char *text)
{
    if (text == nullptr) return;
    for (unsigned i = 0; text[i] != '\0' && used + 1u < cap; ++i) out[used++] = text[i];
}
static void gameid_append_dec(char *out, unsigned cap, unsigned &used, uint64_t v)
{
    char tmp[24];
    unsigned n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < sizeof tmp) {
        tmp[n++] = (char)('0' + (v % 10ull));
        v /= 10ull;
    }
    while (n > 0 && used + 1u < cap) out[used++] = tmp[--n];
}

// Identity state for the gamedata row: empty string = honestly unavailable.
static bool g_gameid_emitted = false;
static char g_gameid_sha16[17];
static char g_gameid_ver[16];
static char g_gameid_size[24];

static unsigned long long gameid_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0ull;
    return (unsigned long long)ts.tv_sec * 1000ull +
           (unsigned long long)ts.tv_nsec / 1000000ull;
}

// Strict ASCII "AA.BBB.CCC" (exactly pfs-version.dat's 10 bytes) -> packed
// (major<<24)|(minor<<16)|(patch<<8)|0 — the analyzer_v2 decode law. Returns
// false on ANY deviation; the caller then emits the format tag and NO packed
// version (never fabricated).
static bool gameid_parse_pfs_version(const char *b, unsigned n, uint32_t &packed)
{
    unsigned i = 0;
    uint32_t fields[3] = {0, 0, 0};
    for (unsigned f = 0; f < 3; ++f) {
        unsigned digits = 0;
        while (i < n && b[i] >= '0' && b[i] <= '9') {
            fields[f] = fields[f] * 10u + (uint32_t)(b[i] - '0');
            ++digits;
            ++i;
            if (digits > 3) return false;  // law: AA.BBB.CCC — max 3 per field
        }
        if (digits == 0) return false;
        if (f < 2) {
            if (i >= n || b[i] != '.') return false;
            ++i;
        }
    }
    if (i != n) return false;  // no trailing junk/newline tolerated
    packed = (fields[0] << 24) | (fields[1] << 16) | (fields[2] << 8);
    return true;
}

// Zero-padded on-disk rendering ("02.003.000" shape: 2.3.3 digits — the same
// decode law analyzer_v2 uses, %02d.%03d.%03d).
static void gameid_render_version(char *out, unsigned cap, uint32_t packed)
{
    static const char digits[] = "0123456789";
    const uint32_t f0 = (packed >> 24) & 0xffu;
    const uint32_t f1 = (packed >> 16) & 0xffu;
    const uint32_t f2 = (packed >> 8) & 0xffu;
    unsigned used = 0;
    char tmp[16];
    unsigned n = 0;
    // Decode law shape %02d.%03d.%03d — pad to it, never truncate (a 3-digit
    // major prints all three digits, exactly like printf would).
    if (f0 >= 100u) tmp[n++] = digits[(f0 / 100u) % 10u];
    tmp[n++] = digits[(f0 / 10u) % 10u];
    tmp[n++] = digits[f0 % 10u];
    tmp[n++] = '.';
    tmp[n++] = digits[(f1 / 100u) % 10u];
    tmp[n++] = digits[(f1 / 10u) % 10u];
    tmp[n++] = digits[f1 % 10u];
    tmp[n++] = '.';
    tmp[n++] = digits[(f2 / 100u) % 10u];
    tmp[n++] = digits[(f2 / 10u) % 10u];
    tmp[n++] = digits[f2 % 10u];
    tmp[n] = '\0';
    gameid_append(out, cap, used, tmp);
    out[used] = '\0';
}

// Version: /app0/sce_sys/pfs-version.dat, strict-parse or nothing. Called on
// BOTH the stat-success and stat-failure paths — the errno (or the packed
// value) is exactly the /app0-path evidence the next decision needs.
static void gameid_emit_version(void)
{
    std::FILE *vf = std::fopen(kGameidVersionPath, "rb");
    if (vf == nullptr) {
        ::diagnostic_event("gameid_ver_ec", (uint32_t)errno);
        return;
    }
    char vb[16];
    const size_t vn = std::fread(vb, 1u, sizeof vb, vf);
    const bool vf_err = (std::ferror(vf) != 0);
    std::fclose(vf);
    if (vf_err) {
        ::diagnostic_event("gameid_ver_ec", (uint32_t)EIO);
        return;
    }
    uint32_t packed = 0;
    if (gameid_parse_pfs_version(vb, (unsigned)vn, packed)) {
        ::diagnostic_event("game_content_version_packed", packed);
        gameid_render_version(g_gameid_ver, (unsigned)sizeof g_gameid_ver, packed);
    } else {
        ::diagnostic_event("gameid_ver_fmt", 1u);
    }
}

// The identity emission itself. Runs once per process on the module_start
// thread (the entry guard serializes module_start/module_stop).
static void bridge_gameid_emit(void)
{
    if (g_gameid_emitted) return;
    g_gameid_emitted = true;
    g_gameid_sha16[0] = '\0';
    g_gameid_ver[0] = '\0';
    g_gameid_size[0] = '\0';

    // Stat through the EXISTING std::filesystem route (stat_app0 precedent).
    const std::filesystem::path eboot(kGameidEbootPath);
    std::error_code sec;
    const std::uintmax_t size = std::filesystem::file_size(eboot, sec);
    if (sec) {
        ::diagnostic_event("gameid_stat_ec", (uint32_t)sec.value());
        ::diagnostic_event("gameid_unavailable", 1u);
        gameid_emit_version();  // the errno pair is the /app0 path evidence
        return;
    }
    std::error_code mec;
    const auto mtime = std::filesystem::last_write_time(eboot, mec);
    const uint64_t size64 = (uint64_t)size;
    const uint64_t mtime64 = mec ? 0ull
        : (uint64_t)static_cast<int64_t>(mtime.time_since_epoch().count());
    if (mec) ::diagnostic_event("gameid_mtime_ec", (uint32_t)mec.value());
    ::diagnostic_event("gameid_eboot_size_low", (uint32_t)(size64 & 0xffffffffull));
    ::diagnostic_event("gameid_eboot_size_high", (uint32_t)(size64 >> 32));
    ::diagnostic_event("gameid_eboot_mtime_low", (uint32_t)(mtime64 & 0xffffffffull));
    ::diagnostic_event("gameid_eboot_mtime_high", (uint32_t)(mtime64 >> 32));
    ::diagnostic_event("gameid_stat_fold",
        (uint32_t)((uint32_t)size64 ^ (uint32_t)(mtime64 & 0xffffffffull) ^
                   (uint32_t)(mtime64 >> 32)));
    {
        unsigned used = 0;
        gameid_append_dec(g_gameid_size, (unsigned)sizeof g_gameid_size, used, size64);
        g_gameid_size[used] = '\0';
    }

    // Bounded runtime cap — refuse loudly instead of an unbounded module_start.
    if (size64 == 0ull || size64 > kGameidMaxBytes) {
        ::diagnostic_event("gameid_toobig", 1u);
        ::diagnostic_event("gameid_unavailable", 1u);
        return;
    }

    // Streaming sha256 via the frozen-contract libc FILE* route. A read that
    // comes up short of the stat size refuses the words entirely (a partial
    // digest would be worse than absence).
    const unsigned long long t0 = gameid_now_ms();
    GameidSha256 c;
    gameid_sha256_init(c);
    static unsigned char gameid_chunk_buf[kGameidChunk];
    std::FILE *f = std::fopen(kGameidEbootPath, "rb");
    if (f == nullptr) {
        ::diagnostic_event("gameid_open_ec", (uint32_t)errno);
        ::diagnostic_event("gameid_unavailable", 1u);
        return;
    }
    uint64_t hashed = 0;
    bool io_error = false;
    for (;;) {
        const size_t got = std::fread(gameid_chunk_buf, 1u, kGameidChunk, f);
        if (got > 0) {
            gameid_sha256_update(c, gameid_chunk_buf, got);
            hashed += (uint64_t)got;
        }
        if (got < kGameidChunk) {
            if (std::ferror(f) != 0) io_error = true;
            break;
        }
        if (hashed > size64) break;  // grew mid-read: refuse (never pretend)
    }
    std::fclose(f);
    const unsigned long long t1 = gameid_now_ms();
    ::diagnostic_event("gameid_sha_ms", (uint32_t)(t1 - t0));
    if (io_error || hashed != size64) {
        ::diagnostic_event("gameid_sha_err", 1u);
        ::diagnostic_event("gameid_unavailable", 1u);
        return;
    }
    unsigned char digest[32];
    gameid_sha256_final(c, digest);
    for (unsigned w = 0; w < 4u; ++w) {
        const uint32_t word = (uint32_t)digest[4u * w + 0u] << 24 |
                              (uint32_t)digest[4u * w + 1u] << 16 |
                              (uint32_t)digest[4u * w + 2u] << 8 |
                              (uint32_t)digest[4u * w + 3u];
        char tag[32];
        unsigned used = 0;
        gameid_append(tag, (unsigned)sizeof tag, used, "game_eboot_sha256_w");
        gameid_append_dec(tag, (unsigned)sizeof tag, used, w);
        tag[used] = '\0';
        ::diagnostic_event(tag, word);
    }
    {
        unsigned used = 0;
        static const char hexd[] = "0123456789abcdef";
        for (unsigned i = 0; i < 8u; ++i) {  // 16 hex chars = digest bytes 0..7
            g_gameid_sha16[used++] = hexd[digest[i] >> 4];
            g_gameid_sha16[used++] = hexd[digest[i] & 0xfu];
        }
        g_gameid_sha16[used] = '\0';
    }

    // Version: /app0/sce_sys/pfs-version.dat, strict-parse or nothing.
    gameid_emit_version();
    ::diagnostic_event("gameid_done", 1u);
}

// The human-durable runtime-status gamedata row: written straight after the
// StartActivatedRuntimeWithAck success (RuntimeAckSink state 1 'activated' is
// already in the file), through the module's proven POSIX open/write shape
// (mfa_status lineage) with O_APPEND — never mfa_status_open (O_TRUNC would
// destroy the session file), never a forged 'state\t' seq row.
// Format (task law): gamedata\t<sha16>\t<ver-or-'-'>\t<size>\n
static void bridge_gameid_status_line(const char *mod_root)
{
    if (!g_gameid_emitted || g_gameid_sha16[0] == '\0' || g_gameid_size[0] == '\0')
        return;  // no verified identity -> no row (honest absence)
    if (mod_root == nullptr || mod_root[0] == '\0') return;
    char path[512];
    unsigned used = 0;
    gameid_append(path, (unsigned)sizeof path, used, mod_root);
    if (used + 1u >= (unsigned)sizeof path) return;
    if (path[used - 1u] != '/') path[used++] = '/';
    gameid_append(path, (unsigned)sizeof path, used, "runtime-status.txt");
    path[used] = '\0';
    const int fd = open(path, O_WRONLY | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0) {
        ::diagnostic_event("gameid_status_ec", (uint32_t)errno);
        return;
    }
    char line[128];
    unsigned n = 0;
    gameid_append(line, (unsigned)sizeof line, n, "gamedata\t");
    gameid_append(line, (unsigned)sizeof line, n, g_gameid_sha16);
    gameid_append(line, (unsigned)sizeof line, n, "\t");
    if (g_gameid_ver[0] != '\0')
        gameid_append(line, (unsigned)sizeof line, n, g_gameid_ver);
    else
        gameid_append(line, (unsigned)sizeof line, n, "-");
    gameid_append(line, (unsigned)sizeof line, n, "\t");
    gameid_append(line, (unsigned)sizeof line, n, g_gameid_size);
    if (n + 1u < (unsigned)sizeof line) line[n++] = '\n';
    line[n] = '\0';
    size_t off = 0;
    while (off < n) {
        const ssize_t w = write(fd, line + off, n - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    close(fd);
    ::diagnostic_event("gameid_status_line", off == n ? 1u : 2u);
}

