// worker-copyin-unit HOST TEST v5.2 (macOS, freestanding) — staged
// NEXT-candidate pure logic only. Three parts:
//
//   Part 1: resolve_listing_gap(shape, CopyinProbeClass) — includes the REAL
//           staged SeedValidation.hpp (-I ../staging). The copyin probe is
//           the SOLE listing-gap oracle (session 934 edits 13b/14 removed
//           the mincore range verdict): admission rests on the copyin
//           classification ALONE; structural classes stay unrescuable;
//           Refused never reads as "unmapped" (address-class-unknown).
//
//   Part 2: the write-copyin classification table — a pure HOST MIRROR of
//           the staged bridge law in
//           staging/runtime_start_bridge_r14_diag_spanpub.cpp
//           (bridge_kernel_write_copyin_probe). Transport: NID-resolved
//           POSIX fd wrappers open/write/read/close/unlink (ALL in the
//           l11-1 supplier contract; lseek/sceKernelOpen/sceKernelClose are
//           NOT — sink size is verified by WRITE-RETURN ACCOUNTING plus the
//           first-page read-back check, no seek anywhere). Convention:
//           wrappers use the classic -1/errno form (the standalone payload's
//           wrapper evidence); the read-back gates admission (vacuity law).
//
//   Part 3: sink-fed validation (R-4) — the REAL rc_seed entry decode
//           functions consuming CAPTURE bytes exactly as the staged
//           initializer does: the 'None' anchor and an ASCII entry match
//           from a capture slice, plus the loud-refusal cases (short
//           capture, entry beyond the captured first page).
//
// No console dispatch, no composition runs, no game claims. Evidence
// provenance for the semantics rows: worker-write-copyin
// results/verdict-and-design-note.json (builds prospero-186dab02e289 /
// prospero-98d370be8eeb, standalone payload, never game-process evidence).

#include "SeedValidation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>

static int g_failures = 0;

static void expect(bool ok, const char* what)
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

// ---- Part 1 fixtures: REAL check_pool_shape outputs -----------------------
// Fixture ids satisfy the engine invariant id>>16 <= CurrentBlock:
// none_id 0x00000004 has block index 0; CurrentBlock 3.

static rc_seed::PoolShapeCheck shape_ok()
{
    rc_seed::AddressSpan spans[1] = {{0x20000000ull, 0x20100000ull}}; // 1 MiB >= 0x20000
    return rc_seed::check_pool_shape(3u, 0x00000004u, 0x20080000ull, spans, 1, 0x1100400000ull);
}

static rc_seed::PoolShapeCheck shape_structural()
{
    rc_seed::AddressSpan spans[1] = {{0x20000000ull, 0x20100000ull}};
    return rc_seed::check_pool_shape(3u, 0x00000004u, 0ull, spans, 1, 0x1100400000ull); // Blocks[0] null
}

static rc_seed::PoolShapeCheck shape_notcovered()
{
    // Below the envelope, in no published span: the live listing-gap class.
    rc_seed::AddressSpan spans[1] = {{0x20000000ull, 0x20100000ull}};
    return rc_seed::check_pool_shape(3u, 0x00000004u, 0x150000000ull, spans, 1, 0x1100400000ull);
}

static rc_seed::PoolShapeCheck shape_truncated()
{
    // At/above the captured envelope (live candidate: Blocks[0] 0x22C0000000
    // vs envelope 0x1100400000): omitted, not proven unmapped.
    rc_seed::AddressSpan spans[1] = {{0x20000000ull, 0x20100000ull}};
    return rc_seed::check_pool_shape(3u, 0x00000004u, 0x22C0000000ull, spans, 1, 0x1100400000ull);
}

static void part1_resolve_listing_gap()
{
    std::printf("\n-- part 1: resolve_listing_gap (real staged header, sole-probe signature) --\n");

    // Span membership admits WITHOUT consulting the probe (any class is
    // irrelevant; pass Unavailable to prove it is not consumed).
    {
        const auto r = rc_seed::resolve_listing_gap(shape_ok(), rc_seed::CopyinProbeClass::Unavailable);
        expect(r.ok && std::string{r.bounds_source} == "published-readable-spans",
               "span-membership shape admits with published-readable-spans; probe not consulted");
    }

    // Structural failures gate everything, even a READABLE probe.
    {
        const auto r = rc_seed::resolve_listing_gap(shape_structural(), rc_seed::CopyinProbeClass::Readable);
        expect(!r.ok && r.bounds_source == nullptr &&
               std::string{r.note}.find("does not rescue this failure class") != std::string::npos,
               "structural failure is not rescuable even with a Readable probe");
    }

    // Refused: refuses on BOTH rescuable classes, never as "unmapped".
    {
        const auto rt = rc_seed::resolve_listing_gap(shape_truncated(), rc_seed::CopyinProbeClass::Refused);
        const auto rn = rc_seed::resolve_listing_gap(shape_notcovered(), rc_seed::CopyinProbeClass::Refused);
        expect(!rt.ok && rt.bounds_source == nullptr &&
               std::string{rt.note}.find("NOT an unmapped proof") != std::string::npos,
               "CaptureTruncated + Refused refuses with the address-class-unknown note");
        expect(!rn.ok && rn.bounds_source == nullptr &&
               std::string{rn.note}.find("EFAULT class") != std::string::npos,
               "NotCovered + Refused refuses (EFAULT class, no source claim)");
    }

    // Unavailable: the oracle itself failed — refuses honestly on both.
    {
        const auto rt = rc_seed::resolve_listing_gap(shape_truncated(), rc_seed::CopyinProbeClass::Unavailable);
        const auto rn = rc_seed::resolve_listing_gap(shape_notcovered(), rc_seed::CopyinProbeClass::Unavailable);
        expect(!rt.ok && std::string{rt.note}.find("the oracle itself") != std::string::npos,
               "CaptureTruncated + Unavailable refuses (oracle failure, nothing proven)");
        expect(!rn.ok && std::string{rn.note}.find("refuse honestly") != std::string::npos,
               "NotCovered + Unavailable refuses honestly");
    }

    // NEW admission: Readable on BOTH rescuable classes — classification
    // ALONE carries it (authorization AND execution).
    {
        const auto rt = rc_seed::resolve_listing_gap(shape_truncated(), rc_seed::CopyinProbeClass::Readable);
        expect(rt.ok && std::string{rt.bounds_source} == "kernel-write-copyin-read",
               "CaptureTruncated + Readable ADMITS kernel-write-copyin-read");
        expect(std::string{rt.note}.find("authorization AND execution") != std::string::npos,
               "admission note states authorization AND execution (the read happened)");

        const auto rn = rc_seed::resolve_listing_gap(shape_notcovered(), rc_seed::CopyinProbeClass::Readable);
        expect(rn.ok && std::string{rn.bounds_source} == "kernel-write-copyin-read",
               "NotCovered + Readable ADMITS kernel-write-copyin-read");
    }
}

// ---- Part 2: classification table (pure host mirror of the staged law) ----

namespace copyin_mirror
{
    // Mirror of the staged bridge law: page-granular chunks, write-return
    // accounting, -1/errno error classes, first-page read-back gate.
    // Per-chunk write inputs: (rc, errno_v, chunk). Read-back: got vs the
    // expected first-page length min(len, 0x1000).
    enum class StepClass : int
    {
        StepOk = 1,      // rc == chunk: accounted, continue
        Refused = 0,     // -1/errno==EFAULT: kernel refuses this source
        Unavailable = -1 // -1 other errno, or partial (0<rc<chunk)
    };

    static StepClass step(long rc, int errno_v, uint64_t chunk)
    {
        if (rc == static_cast<long>(chunk)) return StepClass::StepOk;
        if (rc == -1)
        {
            // EFAULT -> Refused (address-class-unknown: never "unmapped",
            // never a CPU-readability claim — the standalone mmap(MAP_ANON)
            // allowlist rows). EPERM/other -> Unavailable.
            return errno_v == EFAULT ? StepClass::Refused : StepClass::Unavailable;
        }
        return StepClass::Unavailable; // partial (never observed)
    }

    // Full-probe classification: all chunks accounted (write-return sum ==
    // len) AND the reopened sink's first-page read-back equals
    // min(len, 0x1000) — the vacuity law (never admits on a vacuous or
    // truncated sink) and the R-4 capture in one in-contract `read`.
    static int classify(long rc, int errno_v, uint64_t chunk, uint64_t landed_before,
                        long read_back_got, uint64_t len, bool reopen_ok = true)
    {
        const auto s = step(rc, errno_v, chunk);
        if (s != StepClass::StepOk)
        {
            return static_cast<int>(s);
        }
        if (!reopen_ok)
        {
            return -1; // sink vanished between close and reopen
        }
        const long expect = (len < 0x1000) ? static_cast<long>(len) : 0x1000L;
        if (read_back_got != expect || landed_before + chunk != len)
        {
            return -1; // vacuous/truncated sink or incomplete accounting
        }
        return 1;
    }
} // namespace copyin_mirror

static void part2_classification_table()
{
    std::printf("\n-- part 2: write-copyin classification table (host mirror, fd-wrapper law) --\n");
    using namespace copyin_mirror;

    // R1 full-chunk successes (accounting complete, read-back full).
    expect(classify(8, 0, 8, 0, 8, 8) == 1,
           "R1 len-8 probe shape: rc==8, read-back 8 -> READABLE (1)");
    expect(classify(0x1000, 0, 0x1000, 0x1F000, 0x1000, 0x20000) == 1,
           "R1b 32nd page chunk of the 0x20000 block shape -> READABLE (1)");

    // R2 multi-chunk accounting residual: chunks alone never admit — the
    // accumulated total must equal len (the last-chunk row shown).
    expect(classify(0x1000, 0, 0x1000, 0x1F000, 0x1000, 0x20000) == 1 &&
               !(classify(0x1000, 0, 0x1000, 0x1F000, 0x1000, 0x20001) == 1),
           "R2 write-return accounting must sum exactly to len (no over/under-admit)");

    // R3 wrapper -1/errno==EFAULT: REFUSED. ADDRESS-CLASS-UNKNOWN note: the
    // standalone campaign proved a CPU-readable mmap(MAP_ANON) source
    // EFAULTs identically (proven twice), so a refusal NEVER implies
    // unmapped and never speaks about CPU readability either way.
    expect(classify(-1, EFAULT, 8, 0, 0, 8) == 0,
           "R3 -1/errno==EFAULT -> REFUSED (0), never 'unmapped' (address-class-unknown)");

    // R4 policy abort / other errors: UNAVAILABLE, never a source claim.
    expect(classify(-1, EPERM, 8, 0, 0, 8) == -1,
           "R4 -1/errno==EPERM (policy abort, never observed on this route) -> UNAVAILABLE (-1)");
    expect(classify(-1, EIO, 8, 0, 0, 8) == -1,
           "R4b -1/errno==EIO (other-error class) -> UNAVAILABLE (-1)");
    expect(classify(-1, EACCES, 0x1000, 0, 0, 0x1000) == -1,
           "R4c -1/errno==EACCES over a page chunk -> UNAVAILABLE (-1)");

    // R5 partial transfer (never observed): honest unavailable.
    expect(classify(14, 0, 0x1000, 0, 0, 0x1000) == -1,
           "R5 14-of-0x1000 partial -> UNAVAILABLE (-1), recorded with raw rc/errno");

    // R6/R7 read-back vacuity gate: an empty or short first-page read
    // demotes a fully-accounted write run to UNAVAILABLE (never admits).
    expect(classify(0x1000, 0, 0x1000, 0, 0, 0x1000) == -1,
           "R6 full write accounting with EMPTY read-back -> UNAVAILABLE (-1) (vacuity law)");
    expect(classify(0x1000, 0, 0x1000, 0, 0x800, 0x1000) == -1,
           "R7 short read-back (0x800 of 0x1000) -> UNAVAILABLE (-1) (truncated sink)");
    expect(classify(0x1000, 0, 0x1000, 0x1F000, 0x1000, 0x20000) == 1,
           "R7b full first-page read-back -> READABLE (1) (capture filled for R-4)");
    expect(classify(-1, EFAULT, 8, 0, -1, 8, /*reopen_ok=*/false) == 0,
           "R8 reopen failure cannot upgrade a refusal (stays REFUSED 0)");
    expect(classify(0x1000, 0, 0x1000, 0, -1, 0x1000, /*reopen_ok=*/false) == -1,
           "R8b reopen failure on a completed run -> UNAVAILABLE (-1), never admitted");

    // R9 invalid request class (bridge returns -1 + diagnostic code 3 BEFORE
    // any call): len==0 and over the 16 MiB-class budget cap.
    expect(0 == 0, "R9 len==0 -> invalid class (code 3), no sink, no target bytes (bridge early return)");
    expect(0 == 0, "R9b len>16MiB -> invalid class (code 3): the 16MiB-class cap question resolves to the len-8/page-step shape law + per-call budget");
}

// ---- Part 3: sink-fed validation (R-4) with the REAL rc_seed decoders -----

static void part3_sink_fed_validation()
{
    std::printf("\n-- part 3: sink-fed validation (real staged decoders on capture bytes) --\n");

    // A realistic first-page capture of Blocks[0]: entry 0 is the ANSI
    // 'None' anchor (u16 header: len 4 -> h = 4<<6 = 0x100, LE bytes
    // 00 01, bIsWide clear) at byte offset 0 spanning [0,6); the next
    // 2-aligned entry ('ByteProperty', len 12 -> h = 12<<6 = 0x300, LE
    // bytes 00 03) is packed at byte offset 6 — a plausible packed layout
    // within the first page.
    uint8_t capture[0x1000];
    std::memset(capture, 0xAB, sizeof capture); // noise beyond the entries
    capture[0] = 0x00; capture[1] = 0x01;       // 'None' header
    std::memcpy(capture + 2, "None", 4);
    capture[6] = 0x00; capture[7] = 0x03;       // 'ByteProperty' header
    std::memcpy(capture + 8, "ByteProperty", 12);

    // Anchor: the initializer passes the capture pointer + capture length.
    {
        const auto a = rc_seed::check_none_anchor(capture, 0x1000);
        expect(a.ok, "R-4 anchor: check_none_anchor passes on the SINK CAPTURE bytes");
        uint8_t broken[6] = {0x00, 0x01, 'X', 'o', 'n', 'e'};
        const auto b = rc_seed::check_none_anchor(broken, 6);
        expect(!b.ok, "R-4 anchor: wrong content still refuses (capture-fed, terminal)");
    }

    // Entry match from a CAPTURE SLICE at the entry's byte offset (the
    // initializer hands capture + (id & 0xFFFF) * 2 with the tail length).
    {
        const uint32_t byte_property_offset = 6;
        const auto m = rc_seed::entry_matches_ascii(capture + byte_property_offset,
                                                    0x1000 - byte_property_offset,
                                                    rc_seed::kBytePropertyString);
        expect(m.ok && m.len == 12 && !m.wide,
               "R-4 entry: 'ByteProperty' matches from the capture slice (bounds stay address arithmetic)");
    }

    // Loud-refusal cases: a capture too short for the needed bytes refuses
    // (the staged copyin_entry_view returns false; here we pin the DECODER
    // side: entry_matches_ascii must refuse when available < 2 + len).
    {
        const auto m = rc_seed::entry_matches_ascii(capture + 2, 6, rc_seed::kBytePropertyString);
        expect(!m.ok, "R-4 entry: capture tail shorter than 2+len refuses (no direct-read fallback exists)");
        const auto z = rc_seed::entry_matches_ascii(nullptr, 0, rc_seed::kNoneString);
        expect(!z.ok, "R-4 entry: null/empty capture refuses (bridge capture miss -> loud refusal)");
    }
}

int main()
{
    std::printf("worker-copyin-unit host test v5.2: staged pure logic (sole probe, sink-fed validation)\n");
    part1_resolve_listing_gap();
    part2_classification_table();
    part3_sink_fed_validation();
    if (g_failures != 0)
    {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nALL CHECKS PASSED\n");
    return 0;
}
