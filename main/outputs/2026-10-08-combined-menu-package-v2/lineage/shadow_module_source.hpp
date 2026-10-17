// shadow_module_source.hpp — wraps the host-verified shadow-copy scanner
// (worker-code-scan shadow_copy.{hpp,cpp}) into the port's ModuleSource
// contract (Unreal/PlatformModuleSnapshot.hpp, lineage-overlay-2-l10r3-b).
//
// Worker: worker-shadow-integrate (staged unit integration).
// Research + host verification only; no console contact, no game claims, no
// shared-lineage edits. Dual-target: compiles on the macOS host (host_test)
// and inside the PS5 PRX. No OS calls in this header's scan path; the eboot
// bytes arrive already read (PRX file core / host fixture — injectable).
//
// Three responsibilities (INTEGRATION.md sections 2-4):
//
//  1. Identity gate — sha256(decrypted eboot) must equal the pinned
//     SeedTable::kImageSha256 BEFORE anything is scanned. A game update
//     invalidates every signature at once; the refusal names both hashes.
//     (Compact FIPS 180-4 SHA-256 below: no OpenSSL dependency on the PRX;
//     verified against the FIPS test vectors + the pinned image digest in
//     the host test.)
//
//  2. ModuleSnapshot assembly — exactly the INTEGRATION.md section 2 sketch:
//       monolithic  = true                       (single-image game)
//       MainExe     = { base = img->base, size = img->backing_size,
//                       readable_ranges = per-PT_LOAD {backing, filesz} }
//       find_export = nullptr                    (no dynsym on this image)
//     The REAL PlatformModuleSnapshot constructor enforces the extent
//     contract (one module extent containing sorted, non-overlapping
//     readable ranges); a shadow that fails it is refused loudly, never
//     published. The lifetime owner retains the ShadowImage, so every
//     described byte stays alive exactly as long as the snapshot.
//
//  3. ShadowSigAdapter — the match-address bifurcation (INTEGRATION.md
//     section 3). On PC one pointer is both the live address to store and
//     readable bytes to decode. On this target they split:
//       live(c)  = linked vaddr of c.get_match_address() + slide  (store/call)
//       bytes(c) = c.get_match_address() itself                  (shadow, readable)
//     Data-class handlers decode rip-relative operands on bytes(c) via
//     rc::shadowcopy::resolve_rip_ref, then add slide.
//
// Slide provenance (INTEGRATION.md section 4.5): `slide` is supplied by the
// CALLER from the bridge's same-capture geometry (LiveModuleFactsSnapshot
// observed_base route; validated uniform 0x400000 == SeedTable
// kBaseObserved on observed boots). This unit never caches or derives a
// slide on its own.

#ifndef RC_SHADOW_MODULE_SOURCE_HPP
#define RC_SHADOW_MODULE_SOURCE_HPP

#include "shadow_copy.hpp"

#include <Unreal/PlatformModuleSnapshot.hpp>
#include <SigScanner/SinglePassSigScanner.hpp>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>

namespace rc::shadowsource
{

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

    // ---- the ModuleSource wrapper ------------------------------------------

    enum class SourceError
    {
        None = 0,
        NullInput,
        HashMismatch,      // identity gate: actual != pinned sha256
        ScannerInitFailed, // shadow_copy mapped ShadowError
        BadPin,            // pinned sha256 string malformed
        SnapshotRejected,  // PlatformModuleSnapshot ctor threw (contract)
    };

    inline const char *source_error_name(SourceError e) noexcept
    {
        switch (e)
        {
        case SourceError::None: return "None";
        case SourceError::NullInput: return "NullInput";
        case SourceError::HashMismatch: return "HashMismatch";
        case SourceError::ScannerInitFailed: return "ScannerInitFailed";
        case SourceError::BadPin: return "BadPin";
        case SourceError::SnapshotRejected: return "SnapshotRejected";
        }
        return "?";
    }

    struct ShadowSourceResult
    {
        SourceError err{SourceError::NullInput};
        char detail[512]{};      // human-readable refusal detail (both hashes / what())
        char actual_sha256[65]{}; // filled whenever the hash was computed
        // On success (err == None): the ModuleSnapshot over the shadow AND the
        // borrowed ShadowImage (valid exactly as long as `snapshot` — its
        // lifetime owner performs shadow_shutdown).
        std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> snapshot{};
        const rc::shadowcopy::ShadowImage *image{};
    };

    namespace detail
    {
        // Lifetime owner for ModuleInput::lifetime: the ShadowImage owns the
        // single contiguous backing allocation; its deleter runs
        // shadow_shutdown. Holds a copy of the scanner error name for logging.
        struct ShadowOwner
        {
            rc::shadowcopy::ShadowImage *img;
            ~ShadowOwner()
            {
                if (img != nullptr)
                {
                    rc::shadowcopy::shadow_shutdown(img);
                }
            }
        };
    } // namespace detail

    // Create the shadow ModuleSource. `slide` must come from the bridge's
    // same-capture geometry (this unit never invents one). `pinned_sha256_hex`
    // is SeedTable::kImageSha256 (64 hex chars).
    inline ShadowSourceResult Create(const uint8_t *eboot_bytes, size_t eboot_size,
                                     uint64_t slide,
                                     const char *pinned_sha256_hex,
                                     const rc::shadowcopy::ShadowLoadPolicy &policy,
                                     const rc::shadowcopy::ShadowAllocator &alloc)
    {
        ShadowSourceResult r;
        if (eboot_bytes == nullptr || eboot_size == 0 || pinned_sha256_hex == nullptr)
        {
            r.err = SourceError::NullInput;
            std::snprintf(r.detail, sizeof(r.detail), "shadow source: null input (bytes=%p size=%zu pin=%p)",
                          (const void *)eboot_bytes, eboot_size, (const void *)pinned_sha256_hex);
            return r;
        }
        if (std::strlen(pinned_sha256_hex) != 64)
        {
            r.err = SourceError::BadPin;
            std::snprintf(r.detail, sizeof(r.detail), "shadow source: pinned sha256 must be 64 hex chars, got %zu", std::strlen(pinned_sha256_hex));
            return r;
        }

        // ---- identity gate (INTEGRATION.md section 4.1) --------------------
        sha256_hex(eboot_bytes, eboot_size, r.actual_sha256);
        if (!sha256_hex_equal(r.actual_sha256, pinned_sha256_hex))
        {
            r.err = SourceError::HashMismatch;
            std::snprintf(r.detail, sizeof(r.detail),
                          "shadow identity REFUSED: decrypted eboot sha256 %s != pinned SeedTable::kImageSha256 %s "
                          "(game update? every signature and seed is invalid at once; re-derive against the new image, never scan a mismatched one)",
                          r.actual_sha256, pinned_sha256_hex);
            return r;
        }

        // ---- shadow_init (scanner install) ----------------------------------
        const rc::shadowcopy::ShadowImage *img = nullptr;
        const rc::shadowcopy::ShadowError se =
            rc::shadowcopy::shadow_init(&img, eboot_bytes, eboot_size, slide, policy, alloc);
        if (se != rc::shadowcopy::ShadowError::None || img == nullptr)
        {
            r.err = SourceError::ScannerInitFailed;
            std::snprintf(r.detail, sizeof(r.detail), "shadow_init failed: %s", rc::shadowcopy::shadow_error_name(se));
            return r;
        }

        // ---- ModuleSnapshot assembly (INTEGRATION.md section 2) -------------
        try
        {
            RC::Unreal::Platform::ModuleInput in{};
            auto owner = std::make_shared<detail::ShadowOwner>();
            owner->img = const_cast<rc::shadowcopy::ShadowImage *>(img);
            in.lifetime = owner; // retains every described byte until release
            in.monolithic = true;
            RC::Unreal::Platform::ModuleImage &image = in.images[RC::ScanTarget::MainExe];
            image.base = img->base;          // the "known base"
            image.size = static_cast<size_t>(img->backing_size);
            image.readable_ranges.reserve(img->segment_count);
            for (size_t s = 0; s < img->segment_count; ++s)
            {
                const rc::shadowcopy::ShadowSegment &seg = img->segments[s];
                image.readable_ranges.push_back(RC::ReadableRange{seg.backing, static_cast<size_t>(seg.filesz)}); // file-backed only
            }
            in.find_export = [](std::string_view) -> void * { return nullptr; }; // no dynsym on this image
            r.snapshot = RC::Unreal::Platform::ModuleSnapshot::Create(std::move(in));
            r.image = img;
            r.err = SourceError::None;
            std::snprintf(r.detail, sizeof(r.detail), "shadow source armed: %u ranges, backing 0x%llx bytes, slide 0x%llx",
                          static_cast<unsigned>(img->segment_count),
                          (unsigned long long)img->backing_size, (unsigned long long)slide);
            return r;
        }
        catch (const std::exception &e)
        {
            // The REAL PlatformModuleSnapshot contract rejected the shadow:
            // extent/overlap/ordering violation. Never publish; free the image.
            rc::shadowcopy::shadow_shutdown(img);
            r.image = nullptr;
            r.snapshot.reset();
            r.err = SourceError::SnapshotRejected;
            std::snprintf(r.detail, sizeof(r.detail), "PlatformModuleSnapshot rejected the shadow ranges: %s", e.what());
            return r;
        }
    }

    // Default policy/allocator overload (runtime-proven load set: LOAD0/1/3/7;
    // LOAD8 stays opt-out — its runtime mapping is unproven, so addresses found
    // there cannot be honestly mapped live).
    inline ShadowSourceResult Create(const uint8_t *eboot_bytes, size_t eboot_size,
                                     uint64_t slide, const char *pinned_sha256_hex)
    {
        rc::shadowcopy::ShadowLoadPolicy policy{};
        policy.load_exec = true;
        policy.load_rodata = true;
        policy.load_rw = true;
        policy.load_nox = false;
        return Create(eboot_bytes, eboot_size, slide, pinned_sha256_hex,
                      policy, rc::shadowcopy::shadow_default_allocator());
    }

    // ---- ShadowSigAdapter: the match-address bifurcation --------------------
    // (INTEGRATION.md section 3 — call live()/bytes() first inside every
    // on_match_found when the scan ran over the shadow ranges.)
    namespace ShadowSigAdapter
    {
        // (a) the LIVE address to store/call: shadow ptr -> linked vaddr + slide.
        // Returns nullptr if the match pointer is not inside the shadow (caller
        // then refuses loudly — a handler must never guess).
        inline uint8_t *live(const rc::shadowcopy::ShadowImage *img, RC::SignatureContainer &c)
        {
            uint64_t v = 0;
            if (!rc::shadowcopy::shadow_ptr_to_vaddr(img, c.get_match_address(), &v))
            {
                return nullptr;
            }
            return reinterpret_cast<uint8_t *>(v + img->slide);
        }

        // (b) the READABLE bytes for operand decode: the shadow pointer itself.
        // (The live exec page may never be read — that is the whole point.)
        inline const uint8_t *bytes(RC::SignatureContainer &c)
        {
            return c.get_match_address();
        }

        // Linked vaddr of the match site (for resolve_rip_ref on shadow bytes).
        inline bool site_vaddr(const rc::shadowcopy::ShadowImage *img, RC::SignatureContainer &c, uint64_t *out)
        {
            return rc::shadowcopy::shadow_ptr_to_vaddr(img, c.get_match_address(), out);
        }

        // Data-class recipe (GUA/GMalloc family): decode the rip-relative
        // operand on the shadow bytes, then add slide.
        inline bool live_via_rip(const rc::shadowcopy::ShadowImage *img, RC::SignatureContainer &c,
                                 uint64_t *live_out, uint64_t *linked_out = nullptr)
        {
            uint64_t site = 0;
            if (!site_vaddr(img, c, &site))
            {
                return false;
            }
            rc::shadowcopy::RipRefResult rr{};
            if (!rc::shadowcopy::resolve_rip_ref(img, site, &rr))
            {
                return false;
            }
            if (linked_out != nullptr)
            {
                *linked_out = rr.target_vaddr;
            }
            if (live_out != nullptr)
            {
                *live_out = rr.target_vaddr + img->slide;
            }
            return true;
        }
    } // namespace ShadowSigAdapter

} // namespace rc::shadowsource

#endif // RC_SHADOW_MODULE_SOURCE_HPP
