/* test_axes_detector.c — host tests for the in-probe axes detector.
 *
 * T1  table self-test (port of the upstream fingerprint_table_is_well_formed
 *     property): EVERY candidate build must resolve its own configuration,
 *     stats and version from the presence vector its own covers() implies.
 * T2  chunk-straddle test: a marker split exactly across the 64 KiB chunk
 *     boundary must still be found (carry logic).
 * T3  degenerate inputs: empty image, all-absent presence -> axes stay
 *     0/0 (not detected), never guessed.
 * T4  REAL retained Townfall image (.superpowers/native-observations/
 *     townfall-static-executable-2026-10-02/decrypted-eboot.bin): map the
 *     PT_LOAD segments into a vaddr image exactly as the loader would
 *     (runtime = 0x400000 + p_vaddr, proven by worker-vaddr-semantics),
 *     run the detector, print the resolved axes + full evidence.
 *
 * Exit code 0 iff T1-T3 all pass. T4 is a measurement: it prints evidence
 * and asserts only internal sanity (bytes scanned, marker-count gate).
 */
#include "../src/axes_detector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int g_failures;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ++g_failures;                                                   \
            printf("FAIL: "); printf(__VA_ARGS__); printf("\n");            \
        }                                                                   \
    } while (0)

/* ------------------------------------------------------------------ */
static const char *cfg_name(int c) {
    switch (c) {
    case IG_AXES_CONFIG_SHIPPING: return "shipping";
    case IG_AXES_CONFIG_DEVELOPMENT: return "development";
    case IG_AXES_CONFIG_TEST: return "test";
    default: return "not-detected";
    }
}

static void utf16le(const char *s, uint8_t *out, uint32_t *len) {
    uint32_t n = 0;
    for (const char *p = s; *p; ++p) {
        out[n++] = (uint8_t)*p;
        out[n++] = 0;
    }
    *len = n;
}

static int find_marker(const char *s) {
    uint8_t pat[512];
    uint32_t plen;
    utf16le(s, pat, &plen);
    for (int mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi) {
        const IgAxesMarker *m = &ig_axes_markers[mi];
        if ((uint32_t)m->len * 2u != plen) continue;
        if (memcmp(&ig_axes_pool[m->off], pat, plen) == 0) return mi;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* T1: well-formedness over all candidate builds.                        */
static void t1_self_test(void) {
    uint8_t present[IG_AXES_MARKER_COUNT];
    uint32_t costs[IG_AXES_CANDIDATES];
    int checked = 0;
    for (int c = 0; c < IG_AXES_CANDIDATES; ++c) {
        int vi, cfg, on;
        ig_axes_candidate(c, &vi, &cfg, &on);
        for (int mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi)
            present[mi] =
                (uint8_t)ig_axes_marker_covers(mi, vi, cfg, on);
        /* Upstream fingerprint_table_is_well_formed path: costs() straight
         * into resolve_axis(), no marker-count gate. */
        ig_axes_costs_of(present, costs);
        IgAxesResult r;
        memset(&r, 0, sizeof(r));
        ig_axes_resolve_costs(costs, &r);

        /* The scan-time gate must NOT silently become part of the model:
         * a scan of this build would find >= MIN_MARKERS markers (its own
         * covers set) unless the build has fewer than 10 table markers
         * covered at all, which the generator asserts cannot help. */
        uint32_t covered = 0;
        for (int mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi)
            covered += present[mi];
        if (covered >= IG_AXES_MIN_MARKERS) {
            IgAxesResult g;
            ig_axes_from_presence(present, &g);
            CHECK(g.build_configuration == r.build_configuration &&
                      g.stats == r.stats,
                  "T1 candidate %d: scan-path gate changed the verdict", c);
        }

        int want_cfg = (cfg == 2) ? IG_AXES_CONFIG_SHIPPING
                     : (cfg == 0) ? IG_AXES_CONFIG_DEVELOPMENT
                                  : IG_AXES_CONFIG_TEST;
        CHECK(r.build_configuration == want_cfg,
              "T1 candidate %d: config %d != %d (present=%u best=%u runner=%u)",
              c, r.build_configuration, want_cfg, covered,
              r.config_best_cost, r.config_runner_cost);
        CHECK(r.stats == (on ? IG_AXES_STATS_ON : IG_AXES_STATS_OFF),
              "T1 candidate %d: stats %d != %d", c, r.stats, on ? 2 : 1);
        if (vi < 0) {
            CHECK(r.version_resolved == 0,
                  "T1 candidate %d: version unexpectedly resolved (%d.%d)",
                  c, r.version_major, r.version_minor);
        } else {
            CHECK(r.version_resolved == 1 &&
                      r.version_major == ig_axes_versions[vi][0] &&
                      r.version_minor == ig_axes_versions[vi][1],
                  "T1 candidate %d: version %d.%d != %d.%d", c,
                  r.version_major, r.version_minor,
                  ig_axes_versions[vi][0], ig_axes_versions[vi][1]);
        }
        ++checked;
    }
    printf("T1 self-test: %d candidate builds, each resolves own "
           "config+stats+version\n", checked);
}

/* ------------------------------------------------------------------ */
/* T2: chunk-boundary straddle.                                          */
typedef struct {
    uint8_t *buf;
    uint64_t len;
    uint32_t min_read, max_read;   /* serving window jitter */
    unsigned seed;
} SynthRead;

static int synth_read(void *ud, uint64_t addr, void *dst, uint32_t len) {
    SynthRead *s = (SynthRead *)ud;
    if (addr < 0x400000 || addr - 0x400000 + len > s->len) return -1;
    /* deterministic pseudo-jittered window to shake out boundary handling */
    s->seed = s->seed * 1664525u + 1013904223u;
    uint32_t cut = s->min_read + (s->seed % (s->max_read - s->min_read + 1));
    if (len > cut) return -1;
    memcpy(dst, s->buf + (addr - 0x400000), len);
    return 0;
}

static void t2_straddle(void) {
    const char *m0 = "gc.StressTestGC";     /* DevOnly gate marker */
    int mi = find_marker(m0);
    CHECK(mi >= 0, "T2: marker %s not found in table", m0);
    if (mi < 0) return;

    uint8_t *buf = calloc(1, 4u * IG_AXES_CHUNK);
    uint8_t pat[64];
    uint32_t plen;
    utf16le(m0, pat, &plen);
    /* start exactly so the match spans the first chunk boundary */
    uint64_t off = IG_AXES_CHUNK - 6;
    memcpy(buf + off, pat, plen);

    for (int trial = 0; trial < 8; ++trial) {
        SynthRead s = {buf, 4u * IG_AXES_CHUNK, 4096, IG_AXES_CHUNK,
                       0xC0FFEEu + (unsigned)trial};
        uint8_t present[IG_AXES_MARKER_COUNT];
        IgAxesResult r;
        ig_axes_scan(synth_read, &s, 0x400000, 4u * IG_AXES_CHUNK, present, &r);
        CHECK(present[mi] == 1,
              "T2 trial %d: straddled marker missed (bytes=%llu refused=%u)",
              trial, (unsigned long long)r.bytes_scanned, r.read_refused);
        /* only one marker present -> below the 10-marker gate -> unknown */
        CHECK(r.build_configuration == IG_AXES_CONFIG_UNKNOWN &&
                  r.stats == IG_AXES_STATS_UNKNOWN,
              "T2 trial %d: resolved from a single marker (must stay unknown)",
              trial);
    }
    free(buf);
    printf("T2 straddle: marker split across chunk boundary found in all "
           "8 jittered-read trials; single marker stays not-detected\n");
}

/* ------------------------------------------------------------------ */
/* T3: degenerate inputs.                                                */
static int refusing_read(void *ud, uint64_t addr, void *dst, uint32_t len) {
    (void)ud; (void)addr; (void)dst; (void)len;
    return -1;
}

static void t3_degenerate(void) {
    uint8_t present[IG_AXES_MARKER_COUNT];
    IgAxesResult r;

    memset(present, 0, sizeof(present));
    ig_axes_from_presence(present, &r);
    CHECK(r.present_count == 0 &&
              r.build_configuration == IG_AXES_CONFIG_UNKNOWN &&
              r.stats == IG_AXES_STATS_UNKNOWN && !r.version_resolved,
          "T3: all-absent presence must stay not-detected");

    ig_axes_scan(refusing_read, NULL, 0x400000, 0x10000000, NULL, &r);
    CHECK(r.bytes_scanned == 0 && r.present_count == 0 &&
              r.build_configuration == IG_AXES_CONFIG_UNKNOWN &&
              r.stats == IG_AXES_STATS_UNKNOWN && r.read_refused > 0,
          "T3: fully-refused scan must stay not-detected with refusal evidence");

    printf("T3 degenerate: empty/refused inputs stay 0/0 with evidence\n");
}

/* ------------------------------------------------------------------ */
/* T4: real retained Townfall eboot.                                     */
typedef struct {
    uint8_t *img;      /* vaddr-mapped image, index = p_vaddr */
    uint64_t extent;
} GameImage;

static int game_read(void *ud, uint64_t addr, void *dst, uint32_t len) {
    GameImage *g = (GameImage *)ud;
    if (addr < 0x400000) return -1;
    uint64_t off = addr - 0x400000;
    if (off > g->extent || (uint64_t)len > g->extent - off) return -1;
    memcpy(dst, g->img + off, len);
    return 0;
}

static void t4_real_image(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("T4: SKIPPED (retained image not readable: %s)\n", path);
        return;
    }
    uint8_t hdr[64];
    if (fread(hdr, 1, 64, f) != 64 || memcmp(hdr, "\x7f""ELF", 4) != 0 ||
        hdr[4] != 2) {
        printf("T4: SKIPPED (not an ELF64 image)\n");
        fclose(f);
        return;
    }
    uint64_t phoff = *(uint64_t *)(hdr + 32);
    uint16_t phentsize = *(uint16_t *)(hdr + 54);
    uint16_t phnum = *(uint16_t *)(hdr + 56);

    GameImage g;
    g.extent = 0;
    uint64_t loads[16][5];   /* off, vaddr, filesz, memsz */
    int nload = 0;
    for (int i = 0; i < phnum; ++i) {
        uint8_t ph[56];
        if (fseek(f, (long)(phoff + (uint64_t)i * phentsize), SEEK_SET) != 0 ||
            fread(ph, 1, 56, f) != 56) break;
        uint32_t type = *(uint32_t *)ph;
        if (type != 1) continue;   /* PT_LOAD only */
        uint64_t off = *(uint64_t *)(ph + 8);
        uint64_t va = *(uint64_t *)(ph + 16);
        uint64_t filesz = *(uint64_t *)(ph + 32);
        uint64_t memsz = *(uint64_t *)(ph + 40);
        if (nload < 16) {
            loads[nload][0] = off; loads[nload][1] = va;
            loads[nload][2] = filesz; loads[nload][3] = memsz;
            ++nload;
        }
        if (va + memsz > g.extent) g.extent = va + memsz;
    }
    CHECK(nload > 0 && g.extent > 0, "T4: no PT_LOAD segments parsed");
    if (!nload || !g.extent) { fclose(f); return; }

    g.img = calloc(1, g.extent);
    CHECK(g.img != NULL, "T4: %llu MiB image alloc failed",
          (unsigned long long)(g.extent >> 20));
    if (!g.img) { fclose(f); return; }

    for (int i = 0; i < nload; ++i) {
        if (fseek(f, (long)loads[i][0], SEEK_SET) != 0) continue;
        (void)fread(g.img + loads[i][1], 1, loads[i][2], f);
    }
    fclose(f);

    printf("T4 image: %s | PT_LOADs=%d extent=0x%llx (%.1f MiB mapped)\n",
           path, nload, (unsigned long long)g.extent,
           (double)g.extent / 1048576.0);

    uint8_t present[IG_AXES_MARKER_COUNT];
    IgAxesResult r;
    ig_axes_scan(game_read, &g, 0x400000, g.extent, present, &r);

    /* present-marker address evidence (first hit per marker) is not kept by
     * the scan; counts and costs are the evidence contract here. */
    printf("T4 RESULT {\"build_configuration\":%d,\"stats\":%d,"
           "\"version_known\":%d,\"version\":\"%d.%d\","
           "\"present_count\":%u,\"bytes_scanned\":%llu,"
           "\"read_refused\":%u,\"chunks_read\":%u,"
           "\"config\":{\"resolved\":%d,\"best\":%u,\"runner\":%u,\"margin\":%u},"
           "\"stats_axis\":{\"resolved\":%d,\"best\":%u,\"runner\":%u,\"margin\":%u},"
           "\"version_axis\":{\"resolved\":%d,\"best\":%u,\"runner\":%u,\"margin\":%u}}\n",
           r.build_configuration, r.stats, r.version_known,
           r.version_major, r.version_minor,
           r.present_count, (unsigned long long)r.bytes_scanned,
           r.read_refused, r.chunks_read,
           r.config_resolved, r.config_best_cost, r.config_runner_cost,
           r.config_resolved ? (r.config_runner_cost - r.config_best_cost) : 0,
           r.stats_resolved, r.stats_best_cost, r.stats_runner_cost,
           r.stats_resolved ? (r.stats_runner_cost - r.stats_best_cost) : 0,
           r.version_resolved, r.version_best_cost, r.version_runner_cost,
           r.version_resolved ? (r.version_runner_cost - r.version_best_cost) : 0);
    printf("T4 verdict: build_configuration=%s stats=%s version=%s\n",
           cfg_name(r.build_configuration),
           r.stats == IG_AXES_STATS_ON ? "on"
           : r.stats == IG_AXES_STATS_OFF ? "off" : "not-detected",
           r.version_known ? "resolved (evidence only)" : "not resolved");

    /* internal sanity only — the axes VALUES are the measurement */
    CHECK(r.bytes_scanned > 0, "T4: nothing scanned");
    CHECK(r.present_count >= IG_AXES_MIN_MARKERS ||
              (r.build_configuration == 0 && r.stats == 0),
          "T4: resolved despite too few markers");
    CHECK(r.read_refused == 0, "T4: refused reads inside mapped image");

    free(g.img);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv) {
    printf("axes detector host tests: markers=%d versions=%d candidates=%d\n",
           IG_AXES_MARKER_COUNT, IG_AXES_VERSION_COUNT, IG_AXES_CANDIDATES);
    t1_self_test();
    t2_straddle();
    t3_degenerate();
    for (int i = 1; i < argc; ++i) t4_real_image(argv[i]);
    if (g_failures) {
        printf("RESULT: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("RESULT: ALL CHECKS PASSED\n");
    return 0;
}
