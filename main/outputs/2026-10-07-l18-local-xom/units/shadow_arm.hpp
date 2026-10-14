// shadow_arm.hpp — the THIRD ScanGame arm (worker-shadow-integrate).
//
// Staged, strictly additive integration of the shadow-copy scanner into the
// ScanGame path (INTEGRATION.md section 1.3), host-verified only:
//
//   ScanGame()                          (UnrealInitializer.cpp, __PROSPERO__)
//     1. SHAPE A gate                   (unchanged — refuses a partial table)
//     2. consume_required_slot_seeds    (unchanged — slots 0/2/3/4 seed-bound)
//     3. shadow_arm()                   (NEW — this unit, after seed arming)
//
// shadow_arm policy (INTEGRATION.md section 4, "seeds stay authoritative"):
//   - slide: bridge same-capture geometry ONLY (observed_base); zero geometry
//     or a disagreement with SeedTable::kBaseObserved = loud refusal.
//   - identity: sha256(decrypted eboot) must equal SeedTable::kImageSha256.
//   - REQUIRED slots (0/2/3/4) stay seed-authoritative: the shadow scan is a
//     CROSS-CHECK. Agreement logs confidence; disagreement is a LOUD refusal
//     listing the seed and shadow values — never a silent override. Uniqueness
//     discipline: a cross-check sig must hit EXACTLY its audited hit count.
//   - OPTIONAL slots (fname_tostring, gnatives, fuobject_hash_tables_get,
//     console_manager_singleton) become RESOLVABLE: each entry supplies a sig
//     (or none); no pinned optional sigs exist today, so those slots keep
//     their existing degradation paths (Conv_NameToString, FFrame null walk,
//     graceful skips). A future optional sig must pass the same uniqueness
//     audit as the required four (worker-code-scan REPORT.md section 5).
//   - Address classes travel with the hit (unchanged class rules): a LOAD0
//     hit maps to a live EXEC-ONLY address (call-only); a LOAD7 hit maps to
//     data-readable-under-lease. The shadow supplies addresses, never
//     permission to read live exec pages.
//
// The stage is dependency-injected for host testing: eboot bytes, bridge
// observed_base, seed pins, and the log sink arrive as plain inputs. The
// PRX-side wrapper (patch-unrealinitializer-shadow-arm.diff) wires them to
// the file core, LiveModuleFactsSnapshot, SeedTable.h, and Output::send, and
// turns every refusal into the same seed_refused throw the seed path uses.

#ifndef RC_SHADOW_ARM_HPP
#define RC_SHADOW_ARM_HPP

#include "shadow_module_source.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>

namespace rc::shadowarm
{

    using rc::shadowcopy::ShadowImage;

    // ---- pinned cross-check signatures --------------------------------------
    // From worker-code-scan host_test (35/35) + REPORT.md section 2: every sig
    // below is uniqueness-audited against the sha-pinned image. Do NOT add a
    // sig here without the same audit (the scanner reports full counts, making
    // the audit mechanical).
    struct SlotSig
    {
        const char *name;        // seed-table slot name
        const char *sig;         // hex AOB, "??" per wildcard byte
        const char *mask;        // UE4SS mask: '?' wildcard, else literal
        bool rip_decode;         // true: data-class (rip-relative operand decode)
        size_t expected_hits;    // exact audited hit count (refuse otherwise)
        const char *corroborate_sig;  // optional second sig (same resolution rule)
        const char *corroborate_mask;
        size_t corroborate_expected_hits;
    };

    // slot 0 g_uobject_array: unique lea-rip xref (sig B) + 2-site xref (sig A)
    inline constexpr SlotSig kGuaXref{
        "g_uobject_array",
        "48 8d 1d 00 00 00 00 4c 89 e7 4c 89 fe ff 50 18", "xxx????xxxxxxxxx",
        true, 1,
        "48 8d 1d 00 00 00 00 bf 00 00 00 00 39 43 24 7e", "xxx????xxxxxxxxx", 2};
    // slot 2 fname_ctor_wchar: unique 16-byte prologue (direct function class)
    inline constexpr SlotSig kFNameCtor{
        "fname_ctor_wchar",
        "55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74", "xxxxxxxxxxxxxxxx",
        false, 1, nullptr, nullptr, 0};
    // slot 3 gmalloc: unique bootstrap store, rip decode
    inline constexpr SlotSig kGMallocStore{
        "gmalloc",
        "48 89 1d 00 00 00 00 4c 8b 35 00 00 00 00 0f b6", "xxx????xxx????xx",
        true, 1, nullptr, nullptr, 0};
    // slot 4 static_construct_object_internal: unique 25-byte sig (the 16-byte
    // prologue alone is documented NON-unique: 22,879 file-wide hits)
    inline constexpr SlotSig kScoPrologue{
        "static_construct_object_internal",
        "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec 58 02 00 00 48 8b 05 4d 14",
        "xxxxxxxxxxxxxxxxxxxxxxxxx",
        false, 1, nullptr, nullptr, 0};

    // ---- inputs ----------------------------------------------------------------

    struct RequiredSlotPin
    {
        const SlotSig *sig;   // pinned sig (above)
        uint64_t seed_label;  // SeedTable slot .label (linked vaddr)
    };

    // The four REQUIRED slots in table order {0,2,3,4}.
    struct SeedPins
    {
        uint64_t base_observed;      // SeedTable::kBaseObserved
        const char *image_sha256;    // SeedTable::kImageSha256 (64 hex)
        uint64_t image_extent;       // pinned whole-image extent (0 = skip check)
        RequiredSlotPin required[4];
    };

    // Optional slots: resolvable when a sig is supplied; without one the
    // existing degradation path is kept (reported, not failed).
    struct OptionalSlotDef
    {
        const char *name;
        const SlotSig *sig; // nullptr -> fallback retained
    };

    inline constexpr size_t kMaxOptionalSlots = 8;

    struct ShadowArmInput
    {
        const uint8_t *eboot;             // decrypted eboot, already read
        size_t eboot_size;
        uint64_t bridge_observed_base;    // same-capture bridge geometry (slide source)
        SeedPins pins;                    // from SeedTable.h at the call site
        OptionalSlotDef optional[kMaxOptionalSlots]{}; // zero-initialized = none
        void (*log)(void *ctx, const char *msg);       // Output::send adapter
        void *log_ctx;
    };

    // ---- output -----------------------------------------------------------------

    enum class ShadowArmStatus
    {
        Armed = 0,
        RefusedNoGeometry,   // bridge_observed_base == 0 (never a cached slide)
        RefusedSlideMismatch,// bridge base != SeedTable base_observed
        RefusedIdentity,     // sha256 mismatch (game update)
        RefusedSource,       // scanner/snapshot failure (detail carries why)
        RefusedCrossCheck,   // seed vs shadow disagreement (never overridden)
        RefusedImageShape,   // image geometry drifted from the pins
        BadInput,
    };

    inline const char *shadow_arm_status_name(ShadowArmStatus s) noexcept
    {
        switch (s)
        {
        case ShadowArmStatus::Armed: return "Armed";
        case ShadowArmStatus::RefusedNoGeometry: return "RefusedNoGeometry";
        case ShadowArmStatus::RefusedSlideMismatch: return "RefusedSlideMismatch";
        case ShadowArmStatus::RefusedIdentity: return "RefusedIdentity";
        case ShadowArmStatus::RefusedSource: return "RefusedSource";
        case ShadowArmStatus::RefusedCrossCheck: return "RefusedCrossCheck";
        case ShadowArmStatus::RefusedImageShape: return "RefusedImageShape";
        case ShadowArmStatus::BadInput: return "BadInput";
        }
        return "?";
    }

    struct SlotResult
    {
        const char *name{};
        bool ran{};            // cross-check executed
        bool agree{};          // shadow label == seed label
        uint64_t seed_label{};
        uint64_t shadow_label{}; // rip-resolved target for data-class sigs
        uint64_t live{};         // shadow_label + slide
        size_t hits{};
    };

    struct OptionalResult
    {
        const char *name{};
        bool resolved{};   // true: live address bound from the shadow
        uint64_t live{};
        const char *note{}; // "fallback retained" when unresolved by design
    };

    struct ShadowArmReport
    {
        uint64_t slide{};
        char actual_sha256[65]{};
        SlotResult required[4]{};
        OptionalResult optional[kMaxOptionalSlots]{};
        size_t optional_count{};
        char detail[512]{}; // refusal detail (both values named on mismatch)
        // The armed shadow ModuleSource: keep it alive for as long as the
        // shadow ranges/snapshot are used. image is borrowed (owned by
        // snapshot's lifetime owner).
        std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> snapshot{};
        const ShadowImage *image{};
    };

    namespace detail
    {
        inline void arm_log(const ShadowArmInput &in, const char *msg)
        {
            if (in.log != nullptr)
            {
                in.log(in.log_ctx, msg);
            }
        }

        inline bool unhex_byte(char h, char l, uint8_t *out)
        {
            auto nib = [](char c) -> int
            {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = nib(h), lo = nib(l);
            if (hi < 0 || lo < 0)
            {
                return false;
            }
            *out = uint8_t((hi << 4) | lo);
            return true;
        }

        // "48 8d 1d 00 ..." -> bytes; "??" -> wildcard placeholder byte 00.
        inline bool parse_aob(const char *sig, uint8_t *buf, size_t cap, size_t *len)
        {
            size_t n = 0;
            for (const char *p = sig; *p;)
            {
                if (*p == ' ')
                {
                    ++p;
                    continue;
                }
                char h = p[0], l = p[1];
                if (h == '\0' || l == '\0' || n >= cap)
                {
                    return false;
                }
                if (h == '?' && l == '?')
                {
                    buf[n] = 0; // wildcard (mask drives comparison)
                }
                else if (!unhex_byte(h, l, &buf[n]))
                {
                    return false;
                }
                ++n;
                p += 2;
            }
            *len = n;
            return n > 0;
        }

        // Run one pinned sig with the uniqueness discipline; resolve the label
        // (rip-decoded for data-class). Returns false on discipline violation
        // or resolution failure (detail names the slot).
        bool resolve_slot(const ShadowImage *img, const SlotSig &slot,
                          uint64_t *label_out, size_t *hits_out, char *why, size_t why_cap)
        {
            uint8_t pat[64];
            size_t len = 0;
            if (!parse_aob(slot.sig, pat, sizeof(pat), &len) || std::strlen(slot.mask) != len)
            {
                std::snprintf(why, why_cap, "slot '%s': malformed pinned sig", slot.name);
                return false;
            }
            uint64_t hits[64];
            const size_t n = rc::shadowcopy::scan_pattern_in(img, 0, UINT64_MAX,
                                                             pat, slot.mask, len, hits, 64);
            if (n == SIZE_MAX)
            {
                std::snprintf(why, why_cap, "slot '%s': scanner refused the sig", slot.name);
                return false;
            }
            *hits_out = n;
            if (n != slot.expected_hits)
            {
                std::snprintf(why, why_cap,
                              "slot '%s': cross-check sig hit count %zu != audited %zu (image drifted or sig no longer unique; REFUSED, never guess)",
                              slot.name, n, slot.expected_hits);
                return false;
            }

            auto resolve_label = [&](uint64_t site) -> uint64_t
            {
                if (!slot.rip_decode)
                {
                    return site; // direct class: the hit IS the label
                }
                rc::shadowcopy::RipRefResult rr{};
                if (!rc::shadowcopy::resolve_rip_ref(img, site, &rr))
                {
                    return UINT64_MAX;
                }
                return rr.target_vaddr;
            };

            // All hits must resolve to the SAME label (multi-site sigs group
            // by resolved target — worker-code-scan REPORT.md section 2).
            uint64_t label = resolve_label(hits[0]);
            if (label == UINT64_MAX)
            {
                std::snprintf(why, why_cap, "slot '%s': rip decode failed at 0x%llx", slot.name,
                              (unsigned long long)hits[0]);
                return false;
            }
            for (size_t i = 1; i < n && i < 64; ++i)
            {
                const uint64_t other = resolve_label(hits[i]);
                if (other != label)
                {
                    std::snprintf(why, why_cap,
                                  "slot '%s': sites resolve to different targets (0x%llx vs 0x%llx); REFUSED",
                                  slot.name, (unsigned long long)label, (unsigned long long)other);
                    return false;
                }
            }
            *label_out = label;
            return true;
        }
    } // namespace detail

    // ---- the stage ----------------------------------------------------------------
    inline ShadowArmStatus shadow_arm(const ShadowArmInput &in, ShadowArmReport *out)
    {
        if (out == nullptr || in.eboot == nullptr || in.eboot_size == 0)
        {
            if (out != nullptr)
            {
                out->detail[0] = '\0';
                std::snprintf(out->detail, sizeof(out->detail), "shadow_arm: bad input");
            }
            return ShadowArmStatus::BadInput;
        }

        // ---- 1. slide provenance: same-capture bridge geometry ONLY ----------
        if (in.bridge_observed_base == 0)
        {
            std::snprintf(out->detail, sizeof(out->detail),
                          "[Shadow] REFUSED: no bridge geometry (observed_base == 0). The slide is per-boot "
                          "and must come from the same capture as the scan — never a cached value.");
            return ShadowArmStatus::RefusedNoGeometry;
        }
        if (in.pins.base_observed != 0 && in.bridge_observed_base != in.pins.base_observed)
        {
            std::snprintf(out->detail, sizeof(out->detail),
                          "[Shadow] REFUSED: bridge observed_base 0x%llx != SeedTable base_observed 0x%llx "
                          "(image moved or stale seed table; both must be re-derived together)",
                          (unsigned long long)in.bridge_observed_base,
                          (unsigned long long)in.pins.base_observed);
            return ShadowArmStatus::RefusedSlideMismatch;
        }
        const uint64_t slide = in.bridge_observed_base;

        // ---- 2. identity gate + shadow_init + ModuleSnapshot -----------------
        rc::shadowsource::ShadowSourceResult src =
            rc::shadowsource::Create(in.eboot, in.eboot_size, slide, in.pins.image_sha256);
        std::snprintf(out->actual_sha256, sizeof(out->actual_sha256), "%s", src.actual_sha256);
        if (src.err == rc::shadowsource::SourceError::HashMismatch)
        {
            std::snprintf(out->detail, sizeof(out->detail), "[Shadow] %s", src.detail);
            return ShadowArmStatus::RefusedIdentity;
        }
        if (src.err != rc::shadowsource::SourceError::None)
        {
            std::snprintf(out->detail, sizeof(out->detail), "[Shadow] %s", src.detail);
            return ShadowArmStatus::RefusedSource;
        }

        const ShadowImage *img = src.image;
        out->snapshot = src.snapshot;
        out->image = img;
        out->slide = slide;

        // ---- 3. image shape pins (cheap second identity layer) ----------------
        // This image: LOAD0 linked vaddr == 0 (slide == observed_base exactly)
        // and whole-image extent 0xbc602c8.
        if (img->segment_count == 0 || img->segments[0].vaddr != 0)
        {
            std::snprintf(out->detail, sizeof(out->detail),
                          "[Shadow] REFUSED: first PT_LOAD linked vaddr 0x%llx != 0 (slide arithmetic "
                          "assumes linked 0 == observed_base); re-derive against this image",
                          img->segment_count ? (unsigned long long)img->segments[0].vaddr : 0ull);
            return ShadowArmStatus::RefusedImageShape;
        }
        if (in.pins.image_extent != 0 && img->image_extent_vaddr != in.pins.image_extent)
        {
            std::snprintf(out->detail, sizeof(out->detail),
                          "[Shadow] REFUSED: image extent 0x%llx != pinned 0x%llx",
                          (unsigned long long)img->image_extent_vaddr,
                          (unsigned long long)in.pins.image_extent);
            return ShadowArmStatus::RefusedImageShape;
        }

        {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "[Shadow] shadow source armed: slide 0x%llx, %zu ranges, backing 0x%llx bytes; "
                          "required-slot cross-check follows (seeds stay authoritative)",
                          (unsigned long long)slide, (size_t)img->segment_count,
                          (unsigned long long)img->backing_size);
            detail::arm_log(in, msg);
        }

        // ---- 4. REQUIRED-slot cross-check (seed-authoritative, never override) --
        for (size_t i = 0; i < 4; ++i)
        {
            const RequiredSlotPin &pin = in.pins.required[i];
            SlotResult &res = out->required[i];
            res.name = pin.sig->name;
            res.seed_label = pin.seed_label;

            uint64_t label = 0;
            size_t hits = 0;
            if (!detail::resolve_slot(img, *pin.sig, &label, &hits, out->detail, sizeof(out->detail)))
            {
                std::snprintf(out->detail + std::strlen(out->detail),
                              sizeof(out->detail) - std::strlen(out->detail),
                              " [shadow_arm REFUSED for slot index %zu]", i);
                return ShadowArmStatus::RefusedCrossCheck;
            }
            res.ran = true;
            res.hits = hits;
            res.shadow_label = label;
            res.agree = (label == pin.seed_label);
            if (!res.agree)
            {
                std::snprintf(out->detail, sizeof(out->detail),
                              "[Shadow] REFUSED: required slot '%s' cross-check mismatch: seed label 0x%llx != "
                              "shadow-resolved 0x%llx (live would be 0x%llx vs 0x%llx). Seeds stay authoritative; "
                              "never a silent shadow override. Re-derive against the current image.",
                              res.name, (unsigned long long)pin.seed_label,
                              (unsigned long long)label,
                              (unsigned long long)(pin.seed_label + slide),
                              (unsigned long long)(label + slide));
                return ShadowArmStatus::RefusedCrossCheck;
            }
            res.live = label + slide;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "[Shadow] slot '%s' cross-check GREEN: seed 0x%llx == shadow 0x%llx (live 0x%llx, %zu sig hit(s))",
                              res.name, (unsigned long long)pin.seed_label,
                              (unsigned long long)label, (unsigned long long)res.live, hits);
                detail::arm_log(in, msg);
            }

            // Optional corroborating sig (e.g. GUA sig A): same resolution
            // rule, must land on the same label. Corroboration failure is a
            // refusal too — two sigs disagreeing means the image drifted.
            if (pin.sig->corroborate_sig != nullptr)
            {
                const SlotSig corrob{pin.sig->name, pin.sig->corroborate_sig,
                                     pin.sig->corroborate_mask, pin.sig->rip_decode,
                                     pin.sig->corroborate_expected_hits, nullptr, nullptr, 0};
                uint64_t corrob_label = 0;
                size_t corrob_hits = 0;
                if (!detail::resolve_slot(img, corrob, &corrob_label, &corrob_hits,
                                          out->detail, sizeof(out->detail)) || corrob_label != label)
                {
                    std::snprintf(out->detail, sizeof(out->detail),
                                  "[Shadow] REFUSED: corroborating sig for '%s' disagrees (resolved 0x%llx vs primary 0x%llx)",
                                  res.name, (unsigned long long)corrob_label, (unsigned long long)label);
                    return ShadowArmStatus::RefusedCrossCheck;
                }
            }
        }

        // ---- 5. OPTIONAL slots: resolvable via the scanner, fallbacks kept ----
        size_t n_opt = 0;
        for (size_t i = 0; i < kMaxOptionalSlots; ++i)
        {
            const OptionalSlotDef &def = in.optional[i];
            if (def.name == nullptr)
            {
                continue;
            }
            OptionalResult &res = out->optional[n_opt++];
            out->optional_count = n_opt;
            res.name = def.name;
            if (def.sig == nullptr)
            {
                res.resolved = false;
                res.note = "fallback retained (no pinned sig; existing degradation path owns it)";
                char msg[256];
                std::snprintf(msg, sizeof(msg), "[Shadow] optional slot '%s': %s", def.name, res.note);
                detail::arm_log(in, msg);
                continue;
            }
            uint64_t label = 0;
            size_t hits = 0;
            if (!detail::resolve_slot(img, *def.sig, &label, &hits, out->detail, sizeof(out->detail)))
            {
                // Optional slots degrade, never block: the consumer's fallback
                // stays in force (INTEGRATION.md section 4.3).
                res.resolved = false;
                res.note = "sig missed/unresolved; fallback retained";
                char msg[320];
                std::snprintf(msg, sizeof(msg), "[Shadow] optional slot '%s': %s (%s)",
                              def.name, res.note, out->detail);
                detail::arm_log(in, msg);
                continue;
            }
            res.resolved = true;
            res.live = label + slide;
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "[Shadow] optional slot '%s' resolved from shadow: linked 0x%llx live 0x%llx (%zu hit(s))",
                          def.name, (unsigned long long)label, (unsigned long long)res.live, hits);
            detail::arm_log(in, msg);
        }

        detail::arm_log(in, "[Shadow] shadow_arm complete: all four required cross-checks GREEN; optional slots reported");
        return ShadowArmStatus::Armed;
    }

} // namespace rc::shadowarm

#endif // RC_SHADOW_ARM_HPP
