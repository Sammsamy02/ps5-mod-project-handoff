// worker-io-controls HOST TEST, part 2 (2026-10-05, macOS freestanding).
//
// Compiles the STAGED SeedValidation.hpp (-I ../staging) and proves the pure
// semantics the staged UnrealInitializer rescue path relies on:
//   [1] NONE-ID SEMANTICS: check_pool_shape structurally accepts NoneId == 0
//       (UE5 stores "None" first, entry 0 of Blocks[0] -> id 0) — the audit's
//       finding that the initializer's old `words.none_id != 0` break gate
//       was a false refusal; ByteProperty's id is the readiness signal.
//   [2] RESCUE GUARD: only CaptureTruncated / NotCovered resolve through the
//       copyin classification; Structural (null Blocks[0], misaligned) and
//       SpanTableUnavailable NEVER consume a probe attempt (resolve_listing_gap
//       refuses them regardless of the probe class — the initializer only
//       calls the probe when the class is rescuable, and the pure resolver
//       independently refuses any other class).
//   [3] R-4 CAPTURE DECODE: a simulated first-page sink capture with the real
//       pool head layout (None at byte 0 -> id 0; ByteProperty at byte 6 ->
//       id 3) decodes through check_none_anchor + entry_matches_ascii +
//       check_entry_bounds EXACTLY as the staged initializer consumes it; a
//       slice beyond the captured page refuses.
//   [4] ENTRY MATCH FAILURE IS TERMINAL EVIDENCE (never retried).
//
// No composition runs, no console work, no game claims.

#include "SeedValidation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0;
static void expect(bool ok, const char *what)
{
    if (!ok)
    {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
    else
    {
        std::printf("ok:   %s\n", what);
    }
}

int main()
{
    // The LIVE-shape fixture: Blocks[0] = the observed DM-class candidate
    // 0x22C0000000, above the complete-listing envelope 0x1100400000, in no
    // published span -> check_pool_shape must classify CaptureTruncated.
    rc_seed::AddressSpan spans[1] = {{0x20000000ull, 0x20100000ull}}; // 1 MiB >= 0x20000
    constexpr uint64_t kEnvelope = 0x1100400000ull;
    constexpr uint64_t kLiveBlocks0 = 0x22C0000000ull;

    // ------------------------------------------------------------------
    // [1] NoneId == 0 is structurally legitimate
    // ------------------------------------------------------------------
    {
        // span-membership shape with none_id == 0: 0>>16 = 0 <= current_block
        const auto shape = rc_seed::check_pool_shape(0u, 0u, 0x20080000ull, spans, 1, kEnvelope);
        expect(shape.ok, "none-id: NoneId 0 + CurrentBlock 0 + covered Blocks[0] is a PASSING shape");
        expect(shape.failure_class == rc_seed::PoolShapeCheck::FailureClass::None,
               "none-id: failure_class None");
        // and the engine invariant still refuses a genuinely wrong id
        const auto bad = rc_seed::check_pool_shape(0u, 0x00010000u, 0x20080000ull, spans, 1, kEnvelope);
        expect(!bad.ok && bad.failure_class == rc_seed::PoolShapeCheck::FailureClass::Structural,
               "none-id: none_id block index above CurrentBlock still refuses (structural)");
    }

    // ------------------------------------------------------------------
    // [2] Rescue guard: class gating of the copyin attempt
    // ------------------------------------------------------------------
    {
        const auto truncated = rc_seed::check_pool_shape(0u, 0u, kLiveBlocks0, spans, 1, kEnvelope);
        expect(!truncated.ok && truncated.failure_class == rc_seed::PoolShapeCheck::FailureClass::CaptureTruncated,
               "guard: live DM-class Blocks[0] classifies CaptureTruncated");

        const auto gap_ok = rc_seed::resolve_listing_gap(truncated, rc_seed::CopyinProbeClass::Readable);
        expect(gap_ok.ok && std::string(gap_ok.bounds_source) == "kernel-write-copyin-read",
               "guard: CaptureTruncated + Readable -> ADMITTED via kernel-write-copyin-read");

        const auto refused = rc_seed::resolve_listing_gap(truncated, rc_seed::CopyinProbeClass::Refused);
        expect(!refused.ok && std::string(refused.note).find("NOT an unmapped proof") != std::string::npos,
               "guard: CaptureTruncated + Refused -> refuse (never an unmapped proof)");

        const auto unavail = rc_seed::resolve_listing_gap(truncated, rc_seed::CopyinProbeClass::Unavailable);
        expect(!unavail.ok, "guard: CaptureTruncated + Unavailable -> honest refuse");

        // STRUCTURAL failures must NOT be rescuable even by a hypothetical
        // Readable probe — the pure resolver enforces what the initializer's
        // class check guards before spending the attempt.
        const auto null_shape = rc_seed::check_pool_shape(0u, 0u, 0ull, spans, 1, kEnvelope);
        expect(!null_shape.ok && null_shape.failure_class == rc_seed::PoolShapeCheck::FailureClass::Structural,
               "guard: null Blocks[0] classifies Structural");
        expect(!rc_seed::resolve_listing_gap(null_shape, rc_seed::CopyinProbeClass::Readable).ok,
               "guard: Structural is NEVER rescued (no probe attempt may be spent)");
        const auto misaligned = rc_seed::check_pool_shape(0u, 0u, 0x20080003ull, spans, 1, kEnvelope);
        expect(!misaligned.ok && misaligned.failure_class == rc_seed::PoolShapeCheck::FailureClass::Structural,
               "guard: misaligned Blocks[0] classifies Structural");
        expect(!rc_seed::resolve_listing_gap(misaligned, rc_seed::CopyinProbeClass::Readable).ok,
               "guard: misaligned is NEVER rescued");
        const auto notcovered = rc_seed::check_pool_shape(0u, 0u, 0x150000000ull, spans, 1, kEnvelope);
        expect(!notcovered.ok && notcovered.failure_class == rc_seed::PoolShapeCheck::FailureClass::NotCovered,
               "guard: below-envelope miss classifies NotCovered");
        expect(rc_seed::resolve_listing_gap(notcovered, rc_seed::CopyinProbeClass::Readable).ok,
               "guard: NotCovered + Readable -> admitted (the rescuable pair)");
    }

    // ------------------------------------------------------------------
    // [3] R-4 capture decode of the real pool head (1047 content shape)
    // ------------------------------------------------------------------
    {
        // Simulated first-page sink capture: entry 0 = "None" (header
        // u16 LE: len 4 -> 4<<6 = 0x100, wide bit clear), payload at +2;
        // entry 1 at byte offset 6 -> id 3 = "ByteProperty" (len 12 ->
        // 12<<6 = 0x300). Session 1047 observed exactly this head order.
        std::vector<uint8_t> cap(0x1000, 0);
        auto put_entry = [&](size_t off, const char *s)
        {
            const uint16_t hdr = (uint16_t)(strlen(s) << 6);
            cap[off] = (uint8_t)(hdr & 0xff);
            cap[off + 1] = (uint8_t)(hdr >> 8);
            memcpy(cap.data() + off + 2, s, strlen(s));
        };
        put_entry(0, "None");
        put_entry(6, "ByteProperty");

        constexpr uint32_t kNoneId = 0u;        // entry 0 of Blocks[0]
        constexpr uint32_t kBytePropertyId = 3u; // byte offset 6 / stride 2

        const auto anchor = rc_seed::check_none_anchor(cap.data(), cap.size());
        expect(anchor.ok, "capture: entry 0 decodes as the ANSI 'None' anchor (id 0)");

        const auto nb = rc_seed::check_entry_bounds(kNoneId, 0u, 64u, 2 + rc_seed::kNoneString.size());
        expect(nb.ok, "capture: None entry bounds pass inside the open block 0");

        const auto bb = rc_seed::check_entry_bounds(kBytePropertyId, 0u, 64u, 2 + rc_seed::kBytePropertyString.size());
        expect(bb.ok, "capture: ByteProperty (id 3) bounds pass");

        const auto m = rc_seed::entry_matches_ascii(cap.data() + 6, cap.size() - 6, rc_seed::kBytePropertyString);
        expect(m.ok && !m.wide && m.len == 12, "capture: ByteProperty entry matches from the capture slice");

        // A slice beyond the captured page must refuse — the staged
        // initializer's rescue_capture_bytes refuses loudly, never falls
        // back to a direct read (mirrored here at the pure layer).
        const auto beyond = rc_seed::entry_matches_ascii(cap.data() + 0x1000, 0, rc_seed::kBytePropertyString);
        expect(!beyond.ok, "capture: an out-of-capture slice refuses (no fallback)");
    }

    // ------------------------------------------------------------------
    // [4] Terminal evidence: a content mismatch is a refusal, never retried
    // ------------------------------------------------------------------
    {
        std::vector<uint8_t> bad_page(0x1000, 0);
        const uint16_t hdr = (uint16_t)(4 << 6) | 0x1u; // wide bit SET
        bad_page[0] = (uint8_t)(hdr & 0xff);
        bad_page[1] = (uint8_t)(hdr >> 8);
        memcpy(bad_page.data() + 2, "None", 4);
        const auto anchor = rc_seed::check_none_anchor(bad_page.data(), bad_page.size());
        expect(!anchor.ok, "terminal: a wide entry-0 fails the None anchor (terminal refutation class)");
    }

    std::printf(g_failures == 0 ? "\nALL RESCUE-PATH TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
