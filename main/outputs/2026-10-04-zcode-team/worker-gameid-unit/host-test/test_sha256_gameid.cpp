// worker-gameid-unit host test: NIST FIPS 180-4 vectors against the EXACT
// sha256 impl extracted from the STAGED bridge TU (gameid_sha256.inc).
// The pinned eboot prefix expectations are RUNTIME-ONLY (task law): the file
// itself is never hashed or asserted here — this tests the IMPL, not the file.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "gameid_sha256.inc"

static int g_failures = 0;

static void hexdigest(const unsigned char d[32], char out[65])
{
    static const char hexd[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[2 * i] = hexd[d[i] >> 4];
        out[2 * i + 1] = hexd[d[i] & 0xf];
    }
    out[64] = '\0';
}

// Stream data in variable chunk sizes (also exercises the buffered-update
// path with chunks that never align to 64 bytes).
static void digest_chunks(const unsigned char *data, size_t n, size_t chunk,
                          unsigned char out[32])
{
    GameidSha256 c;
    gameid_sha256_init(c);
    size_t off = 0;
    while (off < n) {
        size_t take = n - off;
        if (take > chunk) take = chunk;
        gameid_sha256_update(c, data + off, take);
        off += take;
    }
    gameid_sha256_final(c, out);
}

static void one(const char *name, const std::vector<unsigned char> &data,
                const char *expect)
{
    unsigned char d[32];
    char hex[65];
    digest_chunks(data.data(), data.size(), 1 << 20, d);  // 1 MiB like target
    hexdigest(d, hex);
    const bool ok = std::strcmp(hex, expect) == 0;
    std::printf("%-40s %s  %s\n", name, ok ? "PASS" : "FAIL", hex);
    if (!ok) {
        std::printf("    expected %s\n", expect);
        ++g_failures;
    }
    // Cross-check with an awkward 13-byte chunk size (buffer boundary law).
    unsigned char d13[32];
    char hex13[65];
    digest_chunks(data.data(), data.size(), 13, d13);
    hexdigest(d13, hex13);
    const bool ok13 = std::strcmp(hex13, expect) == 0;
    std::printf("%-40s %s  (13-byte chunks)\n", name, ok13 ? "PASS" : "FAIL");
    if (!ok13) {
        std::printf("    got      %s\n", hex13);
        ++g_failures;
    }
}

int main()
{
    // NIST FIPS 180-4 / CAVP SHA2 test vectors (absolute correctness).
    one("NIST: empty message", {}, "e3b0c44298fc1c149afbf4c8996fb924"
                                   "27ae41e4649b934ca495991b7852b855");
    {
        const char *s = "abc";
        one("NIST: \"abc\"", std::vector<unsigned char>(s, s + 3),
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad");
    }
    {
        const char *s = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        // Exactly 56 bytes — this ALSO pins the pad-boundary law absolutely.
        one("NIST: 448-bit message (pad boundary)",
            std::vector<unsigned char>(s, s + 56),
            "248d6a61d20638b8e5c026930c3e6039a"
            "33ce45964ff2167f6ecedd419db06c1");
    }
    {
        // 1,000,000 x 'a' — the multi-block streaming vector.
        std::vector<unsigned char> big(1000000u, (unsigned char)'a');
        one("NIST: 1,000,000 x 'a'", big,
            "cdc76e5c9914fb9281a1c7e284d73e67"
            "f1809a48a497200e046d39ccc7112cd0");
    }
    // Pad-law self-consistency across the 55..65 boundary class: the impl
    // must agree with itself across chunk sizes AND single-update one-shot
    // (absolute correctness is already pinned by the vectors above).
    {
        unsigned char pattern[65];
        for (int i = 0; i < 65; ++i)
            pattern[i] = (unsigned char)("0123456789abcdef"[i % 16]);
        bool all = true;
        for (size_t n = 55; n <= 65; ++n) {
            unsigned char da[32], db[32];
            digest_chunks(pattern, n, 7, da);        // tiny chunks (pad > 1 block)
            GameidSha256 c;
            gameid_sha256_init(c);
            gameid_sha256_update(c, pattern, n);     // single update
            gameid_sha256_final(c, db);
            if (std::memcmp(da, db, 32) != 0) all = false;
        }
        std::printf("%-40s %s\n", "pad boundary 55..65 chunked==one-shot",
                    all ? "PASS" : "FAIL");
        if (!all) ++g_failures;
    }
    // Word law: reassemble w0..w3 big-endian from the "abc" digest and check
    // it equals the first 16 hex chars — the analyzer_v2 reassembly law.
    {
        const char *s = "abc";
        unsigned char d[32];
        digest_chunks((const unsigned char *)s, 3, 64, d);
        uint32_t w[4];
        for (int i = 0; i < 4; ++i) {
            w[i] = (uint32_t)d[4 * i + 0] << 24 | (uint32_t)d[4 * i + 1] << 16 |
                   (uint32_t)d[4 * i + 2] << 8 | (uint32_t)d[4 * i + 3];
        }
        uint32_t want[4] = {0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223};
        const bool ok = std::memcmp(w, want, sizeof want) == 0;
        std::printf("%-40s %s\n", "word law w0..w3 big-endian (\"abc\")",
                    ok ? "PASS" : "FAIL");
        if (!ok) ++g_failures;
    }

    if (g_failures != 0) {
        std::printf("RESULT: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("RESULT: ALL PASS\n");
    return 0;
}
