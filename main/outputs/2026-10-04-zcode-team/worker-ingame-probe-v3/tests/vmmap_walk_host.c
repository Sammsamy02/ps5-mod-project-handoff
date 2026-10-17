// tests/vmmap_walk_host.c — host unit test for the v3 VMMAP buffer walk
// (src/vmmap_walk.c). Synthesizes records with VARIABLE kve_structsize
// (the root cause of the v1/v2 corruption) including 36-bit addresses,
// an insane record and a structsize=0 terminator; asserts the mode-1 walk
// parses every record exactly, the raw snapshots are kept, and the
// mode-2 (fixed-stride, v1/v2 behavior) walk over the same buffer is
// flagged as the fallback interpretation.
//
// RESUME DELTAS (worker-ingame-probe-v3-resume, 2026-10-04):
//   1. The ksz=0 terminator is now padded to a FULL 136-byte header and
//      counted into len: the walk requires a whole header before inspecting
//      structsize, so the prior 4-byte terminator was never read and
//      stop_reason stayed 0 (the failure this test now actually covers).
//   2. Three extra full-stride sane records added so retained >= 8: the
//      ig_vmmap_trustworthy production gate (>= 8 sane, <= 20% insane) is
//      now exercised on the PASS side too.
//   3. stop_reason semantics: 2 = partial record at buffer end. The fixed
//      stride lands mid-record by construction, so mode 2 must report 2,
//      not 0 (recorded as a resume delta, not silent).
#include "../src/factstore.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (0)

static void st32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void st64(uint8_t *p, uint64_t v) {
    st32(p, (uint32_t)v); st32(p + 4, (uint32_t)(v >> 32));
}

/* Build one record: kve_structsize=ksz, type, start, end, prot; the rest
   of the header zeroed and (for ksz>136) the tail filled with path-ish
   bytes so a wrong-stride walk would see garbage there. */
static uint64_t put_rec(uint8_t *buf, uint64_t off, uint32_t ksz,
                        uint32_t type, uint64_t start, uint64_t end,
                        uint32_t prot) {
    memset(buf + off, 0, ksz);
    st32(buf + off + 0, ksz);
    st32(buf + off + 4, type);
    st64(buf + off + 8, start);
    st64(buf + off + 16, end);
    st32(buf + off + 56, prot);
    for (uint32_t i = 136; i + 16 <= ksz; i += 16) {
        memset(buf + off + i, 'A' + (int)((i / 16) % 26), 16);
    }
    return off + ksz;
}

int main(void) {
    static uint8_t buf[12 * 1160];
    memset(buf, 0xEE, sizeof buf);   // poison: any read past records shows
    uint64_t len = 0;
    len = put_rec(buf, len, 1160, 9, 0x400000ull, 0x787c000ull, 4);      // L0-like, full stride
    len = put_rec(buf, len, 160, 1, 0x8018c000ull, 0x100118000ull, 1);   // variable 160
    len = put_rec(buf, len, 1160, 2, 0x100000090ull, 0x7ed7f8000ull, 5); // 36-bit addresses
    len = put_rec(buf, len, 144, 3, 0x81000000ull, 0x814f8000ull, 3);    // variable 144
    len = put_rec(buf, len, 1160, 4, 0x90000000ull, 0x90500000ull, 3);
    len = put_rec(buf, len, 1160, 6, 0x91000000ull, 0x91500000ull, 3);   // extra full-stride sane
    len = put_rec(buf, len, 1160, 7, 0x92000000ull, 0x92500000ull, 3);   // extra full-stride sane
    len = put_rec(buf, len, 1160, 8, 0x93000000ull, 0x93500000ull, 3);   // extra full-stride sane
    len = put_rec(buf, len, 1160, 5, 0x1000ull, 0x0ull, 1);              // INSANE start>end
    const uint64_t term_off = len;
    /* Terminator: structsize=0 zeroed to a FULL header so the walk's
       header-presence check admits it; the walk must stop_reason=1 here. */
    memset(buf + len, 0, IG_VM_HDR);
    len = put_rec(buf, len, 0, 0, 0, 0, 0);
    len += IG_VM_HDR;   // terminator occupies a full header slot
    // trailing bytes past len stay poisoned on purpose

    static IgSpan spans[64];
    static uint8_t raws[2 * IG_VM_HDR];
    IgVmStats st;
    uint32_t kept = ig_vmmap_walk(buf, len, 1160, 1, spans, 64, &st, raws, 2);

    CHECK(st.walk_mode == 1);
    CHECK(st.total == 9);            // rec0..rec8; ksz=0 terminator stops the walk
    CHECK(st.retained == 8);
    CHECK(st.insane == 1);
    CHECK(st.truncated == 0);
    CHECK(kept == 8);
    CHECK(st.stop_reason == 1);      // stopped at the ksz=0 terminator
    CHECK(st.stop_off == term_off);  // the offset where the walk refused
    CHECK(st.ksz_full == 7);         // full-stride records (incl. the insane one)
    CHECK(st.ksz_var == 2);          // 160 and 144
    CHECK(st.ksz_144 == 1);          // exactly the 144-byte record (P1 counter)
    CHECK(spans[0].start == 0x400000ull && spans[0].end == 0x787c000ull);
    CHECK(spans[0].prot == 4);
    CHECK(spans[1].start == 0x8018c000ull && spans[1].end == 0x100118000ull);
    CHECK(spans[2].start == 0x100000090ull && spans[2].end == 0x7ed7f8000ull);
    CHECK(spans[2].prot == 5);       // 36-bit round-trip exact
    CHECK(spans[3].start == 0x81000000ull && spans[3].end == 0x814f8000ull);
    CHECK(spans[4].start == 0x90000000ull && spans[4].end == 0x90500000ull);
    CHECK(spans[5].start == 0x91000000ull && spans[5].end == 0x91500000ull);
    CHECK(spans[6].start == 0x92000000ull && spans[6].end == 0x92500000ull);
    CHECK(spans[7].start == 0x93000000ull && spans[7].end == 0x93500000ull);
    CHECK(st.raws_kept == 2);
    uint32_t ksz0 = (uint32_t)raws[0] | ((uint32_t)raws[1] << 8) |
                    ((uint32_t)raws[2] << 16) | ((uint32_t)raws[3] << 24);
    CHECK(ksz0 == 1160);
    uint64_t s0 = 0;
    for (int i = 0; i < 8; ++i) s0 |= (uint64_t)raws[8 + i] << (8 * i);
    CHECK(s0 == 0x400000ull);

    /* mode-2 (fixed 1160) over the same buffer: the v1/v2 interpretation.
       It must run without reading past len, be flagged walk_mode=2, and end
       with stop_reason=2 (partial record): its fixed stride lands mid-buffer
       by construction. Its parse of this synthetic buffer is (by design)
       wrong — it is the flagged fallback interpretation only. */
    IgVmStats fb;
    uint32_t kept2 = ig_vmmap_walk(buf, len, 1160, 2, spans, 64, &fb, 0, 0);
    CHECK(fb.walk_mode == 2);
    CHECK(fb.stop_reason == 2);      // fixed stride ends mid-record (never past len)
    if (kept2 > 64) CHECK(0);
    printf("mode2 kept=%u total=%u insane=%u stop=%d\n", kept2, fb.total,
           fb.insane, fb.stop_reason);

    /* trust gate behavior: 8 sane / 1 insane must pass (production gate is
       >= 8 sane and <= 20% insane). */
    CHECK(ig_vmmap_trustworthy(&st) == 1);
    IgVmStats bogus;
    memset(&bogus, 0, sizeof bogus);
    bogus.retained = 10; bogus.insane = 40;   // 400% insane
    CHECK(ig_vmmap_trustworthy(&bogus) == 0);
    IgVmStats thin;
    memset(&thin, 0, sizeof thin);
    thin.retained = 7; thin.insane = 0;       // below the sane floor
    CHECK(ig_vmmap_trustworthy(&thin) == 0);

    printf("%s (kept=%u mode2kept=%u)\n",
           g_failures ? "VMWALK-HOST-FAILURES" : "VMWALK-HOST-OK", kept, kept2);
    return g_failures ? 1 : 0;
}
