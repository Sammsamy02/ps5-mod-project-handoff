#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
// Exact retained streaming SHA primitive; no scanner or crypto dependency.
namespace rc::codehash {
    // ---- compact SHA-256 (FIPS 180-4) --------------------------------------
    // Streaming implementation so the PRX hashes the eboot in chunks if it
    // ever reads it incrementally; the host test hashes the whole 194 MB in
    // one update. No dynamic allocation.
    struct Sha256State
    {
        uint32_t h[8];
        uint64_t total_len;
        uint8_t block[64];
        size_t block_len;
    };

    inline void sha256_init(Sha256State *s) noexcept
    {
        static constexpr uint32_t kInit[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                              0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        for (int i = 0; i < 8; ++i)
            s->h[i] = kInit[i];
        s->total_len = 0;
        s->block_len = 0;
    }

    inline uint32_t sha256_rotr(uint32_t x, uint32_t n) noexcept
    {
        return (x >> n) | (x << (32 - n));
    }

    inline void sha256_compress(uint32_t h[8], const uint8_t p[64]) noexcept
    {
        static constexpr uint32_t kK[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
        {
            w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
                   (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i)
        {
            const uint32_t s0 = sha256_rotr(w[i - 15], 7) ^ sha256_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = sha256_rotr(w[i - 2], 17) ^ sha256_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i)
        {
            const uint32_t S1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + S1 + ch + kK[i] + w[i];
            const uint32_t S0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    inline void sha256_update(Sha256State *s, const uint8_t *data, size_t n) noexcept
    {
        s->total_len += n;
        while (n > 0)
        {
            const size_t take = 64 - s->block_len < n ? 64 - s->block_len : n;
            std::memcpy(s->block + s->block_len, data, take);
            s->block_len += take;
            data += take;
            n -= take;
            if (s->block_len == 64)
            {
                sha256_compress(s->h, s->block);
                s->block_len = 0;
            }
        }
    }

    inline void sha256_final(Sha256State *s, uint8_t out[32]) noexcept
    {
        const uint64_t bits = s->total_len * 8;
        // Manual padding (do not route through update: it mutates total_len).
        s->block[s->block_len++] = 0x80;
        if (s->block_len > 56)
        {
            std::memset(s->block + s->block_len, 0, 64 - s->block_len);
            sha256_compress(s->h, s->block);
            s->block_len = 0;
        }
        std::memset(s->block + s->block_len, 0, 56 - s->block_len);
        for (int i = 0; i < 8; ++i)
        {
            s->block[56 + i] = uint8_t(bits >> (56 - 8 * i));
        }
        sha256_compress(s->h, s->block);
        for (int i = 0; i < 8; ++i)
        {
            out[i * 4] = uint8_t(s->h[i] >> 24);
            out[i * 4 + 1] = uint8_t(s->h[i] >> 16);
            out[i * 4 + 2] = uint8_t(s->h[i] >> 8);
            out[i * 4 + 3] = uint8_t(s->h[i]);
        }
    }

    // One-shot: lowercase hex digest (65 chars with NUL).
    inline void sha256_hex(const uint8_t *data, size_t n, char out[65]) noexcept
    {
        Sha256State st;
        sha256_init(&st);
        sha256_update(&st, data, n);
        uint8_t digest[32];
        sha256_final(&st, digest);
        static constexpr char kHex[] = "0123456789abcdef";
        for (int i = 0; i < 32; ++i)
        {
            out[i * 2] = kHex[digest[i] >> 4];
            out[i * 2 + 1] = kHex[digest[i] & 0xF];
        }
        out[64] = '\0';
    }

    // Case-insensitive 64-hex-char comparison (accepts upper-case pins).
    inline bool sha256_hex_equal(const char *a, const char *b) noexcept
    {
        if (a == nullptr || b == nullptr)
            return false;
        for (int i = 0; i < 64; ++i)
        {
            char x = a[i], y = b[i];
            if (x >= 'A' && x <= 'F')
                x = char(x - 'A' + 'a');
            if (y >= 'A' && y <= 'F')
                y = char(y - 'A' + 'a');
            if (x != y || x == '\0')
                return false;
        }
        return true;
    }

}
