// vmmap_host.c — host test for the vendored v3 mode-1 vmmap walk (the
// readability gate's parsing layer) plus the exact coverage-inclusion math
// the igf4 probe's ig_map_cover uses (addr >= start, len <= end-start,
// addr-start <= end-start-len).
#include "anchor_store.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (!(cond)) { printf("FAIL %s\n", msg); ++failures; }       \
        else { printf("ok   %s\n", msg); }                           \
    } while (0)

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void wr64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)((v >> (8 * i)) & 0xff);
}

static size_t put_rec(uint8_t *buf, size_t off, uint64_t start, uint64_t end,
                      uint32_t prot, uint32_t ksz) {
    memset(buf + off, 0, ksz);
    wr32(buf + off, ksz);
    wr64(buf + off + 8, start);
    wr64(buf + off + 16, end);
    wr32(buf + off + 56, prot);
    return off + ksz;
}

int main(void) {
    static uint8_t buf[64 * 1024];
    size_t len = 0;
    /* the checkpoint's two live spans + one 36-bit span, x3 for the >=8 gate */
    for (int i = 0; i < 3; ++i) {
        len = put_rec(buf, len, 0x400000ull, 0x787c000ull, 5, 144);
        len = put_rec(buf, len, 0x970c000ull, 0x9a34000ull, 6, 144);
        len = put_rec(buf, len, 0x500000090ull, 0x510000090ull, 1, 144);
    }
    IgSpan spans[IG_SPAN_MAX];
    IgVmStats st;
    uint32_t kept = ig_vmmap_walk(buf, len, 1160, 1, spans, IG_SPAN_MAX, &st,
                                  0, 0);
    CHECK(kept == 9, "mode-1 keeps 9 sane records");
    CHECK(st.total == 9 && st.retained == 9 && st.insane == 0,
          "walk stats 9/9/0");
    CHECK(st.stop_reason == 0, "clean end-of-buffer stop");
    CHECK(st.ksz_144 == 9, "all structsizes 144 (P1 prediction)");
    CHECK(spans[0].start == 0x400000ull && spans[0].end == 0x787c000ull &&
          spans[0].prot == 5, "span 0 exact (exec)");
    CHECK(spans[1].start == 0x970c000ull && spans[1].end == 0x9a34000ull,
          "span 1 exact (rw)");
    CHECK(spans[2].start == 0x500000090ull, "36-bit span round-trips");
    CHECK(ig_vmmap_trustworthy(&st) == 1, "trust gate passes at 9 sane");

    /* coverage math (same formula as ig_map_cover) over the parsed spans */
    int cov = 0;
    uint64_t a = 0x33ac8c0ull; /* Tick runtime */
    for (uint32_t i = 0; i < kept; ++i)
        if (a >= spans[i].start && 16 <= spans[i].end - spans[i].start &&
            a - spans[i].start <=
                spans[i].end - spans[i].start - 16) cov = 1;
    CHECK(cov == 1, "Tick 0x33ac8c0 covered by exec span");
    cov = 0;
    a = 0x97ba110ull; /* GUA runtime */
    for (uint32_t i = 0; i < kept; ++i)
        if (a >= spans[i].start && 96 <= spans[i].end - spans[i].start &&
            a - spans[i].start <=
                spans[i].end - spans[i].start - 96) cov = 1;
    CHECK(cov == 1, "GUA 0x97ba110 covered by rw span");
    cov = 0;
    a = 0x9a34000ull; /* one past rw end */
    for (uint32_t i = 0; i < kept; ++i)
        if (a >= spans[i].start && 1 <= spans[i].end - spans[i].start &&
            a - spans[i].start <=
                spans[i].end - spans[i].start - 1) cov = 1;
    CHECK(cov == 0, "address past rw end NOT covered");

    /* insane structsize stops the walk, never guesses */
    len = put_rec(buf, len, 0x1000, 0x2000, 1, 144);
    wr32(buf + len - 144, 7);   /* overwrite the record's OWN structsize */
    len += 64;
    kept = ig_vmmap_walk(buf, len, 1160, 1, spans, IG_SPAN_MAX, &st, 0, 0);
    CHECK(st.stop_reason == 1, "insane structsize stops with reason 1");
    CHECK(st.total == 9 && st.insane == 0, "insane record not counted");
    (void)spans;

    if (failures) { printf("VMMAP-HOST-FAILURES: %d\n", failures); return 1; }
    printf("VMMAP-HOST-CHECKS-PASSED\n");
    return 0;
}
