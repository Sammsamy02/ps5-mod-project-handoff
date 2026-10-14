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
