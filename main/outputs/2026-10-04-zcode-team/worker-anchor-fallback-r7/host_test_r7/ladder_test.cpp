// Host test for the r7 anchor ladder decision logic (worker-anchor-fallback-r7).
// Standalone extraction of runtime_start_bridge_r7.cpp's ladder: same structs,
// same tables, same per-site verify structure and ordered-ladder gate. Two
// host shims replace target mechanics exactly like host_test_r6/walk_test.cpp
// replaced the OS walk inputs:
//   site_byte()  — on target a direct load from rt; here a registry lookup
//                  that COUNTS unregistered reads (proves uncovered spans are
//                  never dereferenced).
//   diagnostic_event() — captured into a table for assertions.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstddef>

// ---- extracted from the r7 bridge (identical values) -----------------------
static constexpr uint64_t kCandidateBase = 0x400000;
static constexpr uint32_t kVmProtRead = 1u;

struct AnchorSite {
    uint64_t vaddr;
    const char *tag_covered;
    const char *tag_match;
    const char *tag_mm_off;
    const char *tag_live;
};
struct DataWindowAnchor {
    uint32_t length;
    const unsigned char *bytes;
    bool gates;
    uint32_t site_count;
    const char *tag_all;
    AnchorSite sites[2];
};

static const unsigned char kWindowE[] = { 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75 };
static const unsigned char kWindowD[] = { 0x43, 0x75, 0x72, 0x72, 0x65, 0x6e, 0x74, 0x20, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x6c, 0x69, 0x73, 0x74, 0x00 };
static const unsigned char kWindowB[] = { 0x55, 0x00, 0x45, 0x00, 0x35, 0x00, 0x00, 0x00 };
static const unsigned char kWindowC[] = { 0x42, 0x00, 0x75, 0x00, 0x69, 0x00, 0x6c, 0x00, 0x64, 0x00, 0x49, 0x00, 0x64, 0x00, 0x4f, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00, 0x72, 0x00, 0x69, 0x00, 0x64, 0x00, 0x65, 0x00, 0x00, 0x00 };
static const unsigned char kWindowA[] = { 0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x47, 0x4e, 0x55, 0x00, 0x04, 0xbb, 0x43, 0x30, 0xa7, 0xd7, 0x1d, 0xce, 0xfe, 0x1c, 0xc2, 0xc4, 0xd1, 0x97, 0x69, 0xd5, 0x00, 0x00, 0x00, 0x00 };

static const DataWindowAnchor kDataWindowAnchors[5] = {
    { 64, kWindowE, true, 1, nullptr,
      { { 0x7dd2a40, "anchor_E_covered", "anchor_E_match", "anchor_E_mm_off", "anchor_E_live_w0" } } },
    { 19, kWindowD, true, 1, nullptr,
      { { 0x7b8845d, "anchor_D_covered", "anchor_D_match", "anchor_D_mm_off", "anchor_D_live_w0" } } },
    { 8, kWindowB, true, 1, nullptr,
      { { 0x7826782, "anchor_B_covered", "anchor_B_match", "anchor_B_mm_off", "anchor_B_live_w0" } } },
    { 32, kWindowC, true, 2, "anchor_C_match",
      { { 0x75cffda, "anchor_C1_covered", "anchor_C1_match", "anchor_C1_mm_off", "anchor_C1_live_w0" },
        { 0x76c8e58, "anchor_C2_covered", "anchor_C2_match", "anchor_C2_mm_off", "anchor_C2_live_w0" } } },
    { 36, kWindowA, false, 1, nullptr,
      { { 0xbc5f250, "anchor_A_covered", "anchor_A_match", "anchor_A_mm_off", "anchor_A_live_w0" } } },
};

struct OsVmMapping { uint64_t start, end; uint32_t protection; };

// ---- host shims -------------------------------------------------------------
struct HostPage { uint64_t rt; const unsigned char *bytes; uint32_t len; };
static HostPage g_pages[8];
static size_t g_page_count;
static size_t g_unregistered_reads;   // must stay 0: uncovered == never read
static unsigned char g_sentinel_byte; // poison returned by (never-called) unregistered reads

static unsigned char site_byte(uint64_t rt, uint32_t i)
{
    for (size_t p = 0; p < g_page_count; ++p)
        if (g_pages[p].rt == rt && i < g_pages[p].len)
            return g_pages[p].bytes[i];
    ++g_unregistered_reads;
    return g_sentinel_byte++;
}

struct DiagEvent { const char *tag; uint32_t value; };
static DiagEvent g_diag[128];
static size_t g_diag_count;
extern "C" void diagnostic_event(const char *tag, uint32_t value)
{
    if (g_diag_count < sizeof(g_diag) / sizeof(g_diag[0])) {
        g_diag[g_diag_count].tag = tag;
        g_diag[g_diag_count].value = value;
        ++g_diag_count;
    }
}

// ---- extracted ladder logic (structure identical to the r7 bridge) ----------
static bool anchor_site_verify(uint32_t length, const unsigned char *bytes,
                               const AnchorSite &site,
                               const OsVmMapping *mappings, size_t count)
{
    const uint64_t rt = kCandidateBase + site.vaddr;
    bool covered = false;
    for (size_t i = 0; i < count; ++i) {
        if ((mappings[i].protection & kVmProtRead) != 0 &&
            mappings[i].start <= rt && rt + length <= mappings[i].end) {
            covered = true;
            break;
        }
    }
    diagnostic_event(site.tag_covered, covered ? 1u : 0u);
    if (!covered) return false;
    uint32_t mismatch_at = 0xffffffffu;
    for (uint32_t i = 0; i < length; ++i)
        if (site_byte(rt, i) != bytes[i]) { mismatch_at = i; break; }
    const bool matched = mismatch_at == 0xffffffffu;
    diagnostic_event(site.tag_match, matched ? 1u : 0u);
    if (!matched) {
        diagnostic_event(site.tag_mm_off, mismatch_at);
        diagnostic_event(site.tag_live,
                         ((uint32_t)site_byte(rt, mismatch_at < 4 ? mismatch_at : 0)) |
                             (((uint32_t)site_byte(rt, mismatch_at < 4 ? mismatch_at + 1 : 1)) << 8));
    }
    return matched;
}

static bool window_anchor_ladder_verify(const OsVmMapping *mappings, size_t count)
{
    bool ladder_verified = false;
    for (size_t w = 0; w < sizeof(kDataWindowAnchors) / sizeof(kDataWindowAnchors[0]); ++w) {
        const DataWindowAnchor &a = kDataWindowAnchors[w];
        bool all_sites_matched = true;
        for (uint32_t s = 0; s < a.site_count; ++s)
            if (!anchor_site_verify(a.length, a.bytes, a.sites[s], mappings, count))
                all_sites_matched = false;
        if (a.site_count > 1 && a.tag_all != nullptr)
            diagnostic_event(a.tag_all, all_sites_matched ? 1u : 0u);
        if (a.gates && all_sites_matched) ladder_verified = true;
    }
    return ladder_verified;
}

// ---- harness ----------------------------------------------------------------
static int fails = 0;
static void chk(const char *name, bool ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

// Registration order: A FIRST, E LAST — so g_page_count=5 drops E's page and
// g_page_count=6 keeps everything; a test that drops a page PROVES the ladder
// never reads it (any read would hit the poison sentinel and be counted).
static unsigned char bufE[64], bufD[19], bufB[8], bufC1[32], bufC2[32], bufA[36];
static void reset_world(int corrupt)
{
    memcpy(bufE, kWindowE, 64); memcpy(bufD, kWindowD, 19); memcpy(bufB, kWindowB, 8);
    memcpy(bufC1, kWindowC, 32); memcpy(bufC2, kWindowC, 32); memcpy(bufA, kWindowA, 36);
    if (corrupt & 0x01) bufE[7] ^= 0xff;          // E fails at offset 7
    if (corrupt & 0x02) bufD[3] ^= 0xff;          // D fails at offset 3
    if (corrupt & 0x04) bufB[5] ^= 0xff;          // B fails at offset 5
    if (corrupt & 0x08) bufC1[9] ^= 0xff;         // C canonical site fails
    if (corrupt & 0x10) bufC2[9] ^= 0xff;         // C second site fails
    if (corrupt & 0x20) bufA[11] ^= 0xff;         // A fails (must never matter)
    g_page_count = 0; g_unregistered_reads = 0; g_diag_count = 0; g_sentinel_byte = 0xAA;
    const HostPage pages[6] = {
        { kCandidateBase + 0xbc5f250, bufA, 36 },
        { kCandidateBase + 0x7b8845d, bufD, 19 },
        { kCandidateBase + 0x7826782, bufB, 8 },
        { kCandidateBase + 0x75cffda, bufC1, 32 },
        { kCandidateBase + 0x76c8e58, bufC2, 32 },
        { kCandidateBase + 0x7dd2a40, bufE, 64 },
    };
    for (size_t i = 0; i < 6; ++i) g_pages[g_page_count++] = pages[i];
}

static uint32_t diag_value(const char *tag)
{
    for (size_t i = g_diag_count; i-- > 0;)
        if (strcmp(g_diag[i].tag, tag) == 0) return g_diag[i].value;
    return 0xffffffffu; // absent
}
static bool diag_present(const char *tag)
{
    for (size_t i = 0; i < g_diag_count; ++i)
        if (strcmp(g_diag[i].tag, tag) == 0) return true;
    return false;
}

// One big readable span covering every window (igf7-like data-span shape,
// widened past A so A's covered path is exercised too).
static const OsVmMapping kAllReadable[] = { { 0x400000, 0x10000000, 1 } };

int main()
{
    // 1. E-ok: every window covered and matching; ladder verifies; A corrupt
    //    in the SAME run proves A never gates.
    reset_world(0x20);
    chk("E-ok: ladder verified", window_anchor_ladder_verify(kAllReadable, 1) == true);
    chk("E-ok: anchor_E_match=1", diag_value("anchor_E_match") == 1);
    chk("E-ok: no E mm_off emitted", !diag_present("anchor_E_mm_off"));
    chk("E-ok: anchor_A_match=0 yet decision unaffected", diag_value("anchor_A_match") == 0);
    chk("E-ok: anchor_C_match=1 (both sites)", diag_value("anchor_C_match") == 1);
    chk("E-ok: no unregistered reads", g_unregistered_reads == 0);

    // 2. E-fail-D-ok: E corrupted in-game, D (and B) hold the ladder.
    reset_world(0x01);
    chk("E-fail-D-ok: ladder verified via D", window_anchor_ladder_verify(kAllReadable, 1) == true);
    chk("E-fail-D-ok: anchor_E_match=0", diag_value("anchor_E_match") == 0);
    chk("E-fail-D-ok: anchor_E_mm_off=7", diag_value("anchor_E_mm_off") == 7);
    chk("E-fail-D-ok: anchor_E_live_w0 = window head (r6 semantics: mismatch >= 4)",
        diag_value("anchor_E_live_w0") == (((uint32_t)bufE[0]) | (((uint32_t)bufE[1]) << 8)));
    chk("E-fail-D-ok: anchor_D_match=1", diag_value("anchor_D_match") == 1);
    chk("E-fail-D-ok: anchor_B_match=1", diag_value("anchor_B_match") == 1);

    // 3. all-fail: every gate rung corrupted; A matching must NOT rescue.
    reset_world(0x01 | 0x02 | 0x04 | 0x08 | 0x10);
    chk("all-fail: ladder refuses", window_anchor_ladder_verify(kAllReadable, 1) == false);
    chk("all-fail: C aggregate=0", diag_value("anchor_C_match") == 0);
    chk("all-fail: A logged even on refusal", diag_present("anchor_A_covered"));
    // 3b. same, but A itself intact: A-match alone must still not verify.
    reset_world(0x01 | 0x02 | 0x04 | 0x08 | 0x10);
    memcpy(bufA, kWindowA, 36); // A back to expected
    chk("all-fail: A-match-only still refuses",
        (g_diag_count = 0, window_anchor_ladder_verify(kAllReadable, 1)) == false);

    // 4. C-ambiguous: E/D/B corrupted, C1 matches, C2 mismatches -> refuse.
    reset_world(0x01 | 0x02 | 0x04 | 0x10);
    chk("C-ambiguous: refuse on C2 mismatch", window_anchor_ladder_verify(kAllReadable, 1) == false);
    chk("C-ambiguous: anchor_C1_match=1", diag_value("anchor_C1_match") == 1);
    chk("C-ambiguous: anchor_C2_match=0", diag_value("anchor_C2_match") == 0);
    chk("C-ambiguous: anchor_C_match=0", diag_value("anchor_C_match") == 0);
    // 4b. C-both-ok: only E/D/B corrupted, both C sites match -> C gates.
    reset_world(0x01 | 0x02 | 0x04);
    chk("C-both-ok: C both sites verifies ladder", window_anchor_ladder_verify(kAllReadable, 1) == true);
    chk("C-both-ok: anchor_C_match=1", diag_value("anchor_C_match") == 1);

    // 5. coverage-refused-then-next: E sits in an EXEC-ONLY span (prot=4, the
    //    real first-span shape), D/B in a readable data span; E must be
    //    refused unread and the ladder must fall through to D.
    //    E rt [0x81d2a40, 0x81d2a80); D rt 0x7f8845d; B rt 0x7c26782.
    static const OsVmMapping kExecOverE[] = {
        { 0x400000, 0x7800000, 4 },                    // exec-only, no windows
        { 0x7800000, 0x81d0000, 1 },                   // data: B, D (not E)
        { 0x81d0000, 0x81d3000, 4 },                   // exec-only OVER E
        { 0x81d3000, 0x10000000, 1 },                  // data beyond E
    };
    reset_world(0);
    g_page_count = 5; // drop E's page (index 5): any E read would be counted
    chk("coverage-refused: ladder verifies via D", window_anchor_ladder_verify(kExecOverE, 4) == true);
    chk("coverage-refused: anchor_E_covered=0", diag_value("anchor_E_covered") == 0);
    chk("coverage-refused: NO anchor_E_match emitted", !diag_present("anchor_E_match"));
    chk("coverage-refused: E never dereferenced", g_unregistered_reads == 0);
    chk("coverage-refused: anchor_D_match=1", diag_value("anchor_D_match") == 1);
    chk("coverage-refused: anchor_B_match=1", diag_value("anchor_B_match") == 1);

    // 6. menu-like igf7 shape: exec-only first span, data span [0x787c000,
    //    0x82bc000), A uncovered (real igf7 refusal). E gates; A is logged
    //    unread; C2 (0x7ac8e58) is inside the data span and covered.
    static const OsVmMapping kMenuLike[] = {
        { 0x400000, 0x787c000, 4 },
        { 0x787c000, 0x82bc000, 1 },
    };
    reset_world(0);
    for (size_t i = 1; i < g_page_count; ++i) g_pages[i - 1] = g_pages[i]; // drop A's page (index 0)
    --g_page_count; // any A read would now hit the poison sentinel and count
    chk("menu-like: ladder verifies with E", window_anchor_ladder_verify(kMenuLike, 2) == true);
    chk("menu-like: anchor_A_covered=0", diag_value("anchor_A_covered") == 0);
    chk("menu-like: NO anchor_A_match emitted", !diag_present("anchor_A_match"));
    chk("menu-like: A never dereferenced", g_unregistered_reads == 0);
    chk("menu-like: anchor_C2_covered=1 (C2 inside data span)", diag_value("anchor_C2_covered") == 1);
    chk("menu-like: anchor_E_match=1", diag_value("anchor_E_match") == 1);

    printf("RESULT: %s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
