/* fc1_host_env.c — worker-fname-runtime-probe host test environment.
 *
 * SYNTHETIC arenas laid out at fixed addresses (matches
 * tests/generated-host/fc1_config.h) and populated with the REAL captured
 * window bytes (tests/generated-host/fc1_fixtures.h, re-extracted from the
 * frozen capture at build time) relocated to the synthetic addresses:
 *   - lea rsi disp32 and E8 rel32 are RECOMPUTED for the synthetic layout;
 *     all other bytes stay byte-identical to the capture.
 *   - target 64 bytes placed on its own 16 KiB page (page 1 of the exec
 *     arena) so page-range math across distinct pages is exercised.
 *
 * Fake g_map/g_spans/ig_map_cover back the anchor_store.h interface the
 * probe consumes; fc1_host_mprotect is the software protection model.
 */
#include "anchor_store.h"
#include "fc1_config.h"
#include "fc1_host_env.h"
#include "fc1_fixtures.h"
#include "fc1_probe.h"
#include "fc1_windows.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- arenas (deterministic; mirrors generated host config) ---------- */
#define FC1_HOST_EXEC_BASE 0x110000000ull /* maps OK on macOS arm64 */
#define FC1_HOST_DATA_BASE 0x110010000ull /* 1 page of 0x4000 */
#define FC1_HOST_PAGE      0x4000ull
#define FC1_HOST_EXEC_PAGES 2

/* The probe dereferences the synthetic runtime addresses directly, so the
   arenas must REALLY be mapped at the fixed synthetic bases. */
#include <sys/mman.h>
#include <stdlib.h>
static uint8_t *g_exec;
static uint8_t *g_data;

static void *map_fixed(uint64_t addr, uint64_t len) {
    void *p = mmap((void *)(uintptr_t)addr, (size_t)len, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED || (uint64_t)(uintptr_t)p != addr) {
        fprintf(stderr, "LOUD FAILURE: cannot map synthetic arena at 0x%llx\n",
                (unsigned long long)addr);
        exit(3);
    }
    return p;
}

/* ---- software mprotect model ---------------------------------------- */
static int g_exec_prot;   /* starts 0x4 (execute-only, checkpoint reality) */
static int g_data_prot;   /* starts 0x1 (readable data) */
static int g_fail_mprotect_left;
static int g_fail_restore_left;

/* Layout MUST match fc1_host_env.h (asserted by the shared header; the
   .c includes the header, so keep only the definition here). */
#define FC1_HOST_CALLS_MAX 128
Fc1HostCall fc1_host_calls[FC1_HOST_CALLS_MAX];
int fc1_host_ncalls;

static long host_mprotect_apply(uint64_t addr, uint64_t len, int prot) {
    int in_exec = addr >= FC1_HOST_EXEC_BASE &&
                  addr + len <= FC1_HOST_EXEC_BASE +
                                    FC1_HOST_EXEC_PAGES * FC1_HOST_PAGE;
    int in_data = 0;  /* the probe never mprotects the data arena */
    if (!in_exec && !in_data) return -22;         /* EINVAL-ish */
    if ((addr & (FC1_HOST_PAGE - 1)) != 0) return -22;
    if ((len & (FC1_HOST_PAGE - 1)) != 0 || len == 0) return -22;
    if (in_exec) g_exec_prot = prot;
    else g_data_prot = prot;
    return 0;
}

long fc1_host_mprotect(const void *addr, size_t len, int prot) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    long rc;
    int is_request = prot == (IGFC1_PROT_READ | IGFC1_PROT_EXEC);
    if (!is_request && g_fail_restore_left > 0) {
        --g_fail_restore_left;            /* restore-failure injection */
        rc = -22;
    } else if (g_fail_mprotect_left > 0) {
        --g_fail_mprotect_left;
        rc = -22;
    } else {
        rc = host_mprotect_apply(a, (uint64_t)len, prot);
    }
    if (fc1_host_ncalls < FC1_HOST_CALLS_MAX) {
        fc1_host_calls[fc1_host_ncalls].addr = a;
        fc1_host_calls[fc1_host_ncalls].len = (uint64_t)len;
        fc1_host_calls[fc1_host_ncalls].prot = prot;
        fc1_host_calls[fc1_host_ncalls].rc = (int)rc;
        ++fc1_host_ncalls;
    }
    return rc;
}

/* The restore call is distinguishable by prot != (READ|EXEC). */
int fc1_host_fail_next_restore(int count) {
    g_fail_restore_left = count;
    return 0;
}

int fc1_host_fail_next_mprotect(int count) {
    g_fail_mprotect_left = count;
    return 0;
}

/* ---- fake map (anchor_store.h interface) ---------------------------- */
IgMapState g_map;
IgSpan g_spans[IG_SPAN_MAX];

int ig_map_cover(uint64_t addr, uint64_t len, IgSpan *out) {
    for (uint32_t i = 0; i < g_map.walk.retained; ++i) {
        if (g_spans[i].start <= addr &&
            addr + len <= g_spans[i].end) {
            *out = g_spans[i];
            return 1;
        }
    }
    return 0;
}

int ig_mapscan_run(void) { return g_map.trusted; }

/* ---- synthetic environment construction ----------------------------- */

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static const char *const kNames[FC1_NANCHORS] = {
    "Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"};

#define FC1_TARGET_OFF (FC1_HOST_PAGE)              /* page 1 start */
#define FC1_WINDOW_STRIDE 128

/* Compile-time sync with the GENERATED host config (loud mismatch). */
#if FC1_HOST_EXEC_BASE != IGFC1_EXEC_SPAN_START
#error "host env exec arena does not match generated fc1_config.h"
#endif
#if FC1_HOST_EXEC_BASE + FC1_TARGET_OFF != IGFC1_RT_FNAME_CTOR
#error "host env target address does not match generated fc1_config.h"
#endif
#if FC1_HOST_DATA_BASE != IGFC1_ANCHOR0_CONTENT_RT
#error "host env data arena does not match generated fc1_config.h"
#endif
#if FC1_HOST_PAGE != IGFC1_PAGE_SIZE
#error "host env page size does not match generated fc1_config.h"
#endif

static Fc1SiteSpec g_sites[FC1_NSITES];
static Fc1AnchorSpec g_anchors[FC1_NANCHORS];
static uint8_t g_anchor_u16[FC1_NANCHORS][48];
static Fc1Env g_env;

const Fc1Env *fc1_host_env(void) { return &g_env; }

void fc1_host_reset(int map_trusted) {
    if (!g_exec) {
        g_exec = map_fixed(FC1_HOST_EXEC_BASE,
                           FC1_HOST_EXEC_PAGES * FC1_HOST_PAGE);
        g_data = map_fixed(FC1_HOST_DATA_BASE, FC1_HOST_PAGE);
    }
    memset(g_exec, 0, FC1_HOST_EXEC_PAGES * FC1_HOST_PAGE);
    memset(g_data, 0, FC1_HOST_PAGE);
    g_exec_prot = 0x4;                    /* execute-only (checkpoint) */
    g_data_prot = 0x1;
    g_fail_mprotect_left = 0;
    g_fail_restore_left = 0;
    fc1_host_ncalls = 0;

    memset(&g_map, 0, sizeof g_map);
    g_map.trusted = map_trusted;
    g_map.walk.retained = 2;
    g_spans[0].start = FC1_HOST_EXEC_BASE;
    g_spans[0].end = FC1_HOST_EXEC_BASE + FC1_HOST_EXEC_PAGES * FC1_HOST_PAGE;
    g_spans[0].prot = 0x4;                /* execute-only */
    g_spans[1].start = FC1_HOST_DATA_BASE;
    g_spans[1].end = FC1_HOST_DATA_BASE + FC1_HOST_PAGE;
    g_spans[1].prot = 0x1;                /* readable data */

    /* anchor strings: UTF-16LE name + NUL at 0x40-strided offsets */
    uint64_t str_off[FC1_NANCHORS] = {0x00, 0x40, 0x80, 0xC0, 0x100};
    for (int i = 0; i < FC1_NANCHORS; ++i) {
        uint16_t n = 0;
        for (; kNames[i][n]; ++n) {
            uint16_t ch = (uint16_t)kNames[i][n];
            g_data[str_off[i] + 2 * n] = (uint8_t)(ch & 0xff);
            g_data[str_off[i] + 2 * n + 1] = (uint8_t)(ch >> 8);
            g_anchor_u16[i][2 * n] = g_data[str_off[i] + 2 * n];
            g_anchor_u16[i][2 * n + 1] = g_data[str_off[i] + 2 * n + 1];
        }
        g_data[str_off[i] + 2 * n] = 0;
        g_data[str_off[i] + 2 * n + 1] = 0;
        g_anchor_u16[i][2 * n] = 0;
        g_anchor_u16[i][2 * n + 1] = 0;
        g_anchors[i].name = kNames[i];
        g_anchors[i].content_rt = FC1_HOST_DATA_BASE + str_off[i];
        g_anchors[i].u16 = g_anchor_u16[i];
        g_anchors[i].u16_len = (uint16_t)(2 * n + 2);
    }

    /* target 64 bytes (captured) at page-1 start */
    for (int i = 0; i < 64; ++i)
        g_exec[FC1_TARGET_OFF + i] = fc1_fix_target64[i];

    /* windows: captured bytes, disp32/rel32 recomputed for synthetic rt */
    for (int i = 0; i < FC1_NSITES; ++i) {
        uint32_t len = fc1_fix_window_len[i];
        uint64_t w_rt = FC1_HOST_EXEC_BASE + (uint64_t)i * FC1_WINDOW_STRIDE;
        uint64_t c_rt = w_rt + len - 5;
        uint64_t s_rt = g_anchors[fc1_fix_window_anchor[i]].content_rt;
        uint8_t *dst = g_exec + (uint64_t)i * FC1_WINDOW_STRIDE;
        for (uint32_t k = 0; k < len; ++k) dst[k] = fc1_fix_window[i][k];
        /* lea rsi,[rip+disp32]: bytes 3..6 */
        put_u32(dst + 3, (uint32_t)(uint64_t)((int64_t)s_rt -
                                              (int64_t)(w_rt + 7)));
        /* e8 rel32: last 4 bytes */
        put_u32(dst + len - 4,
                (uint32_t)(uint64_t)((int64_t)FC1_HOST_EXEC_BASE +
                                         FC1_TARGET_OFF - (int64_t)(c_rt + 5)));
        g_sites[i].anchor = (uint8_t)fc1_fix_window_anchor[i];
        g_sites[i].window_vaddr = fc1_fix_window_vaddr[i];
        g_sites[i].call_vaddr = fc1_fix_call_vaddr[i];
        g_sites[i].window_rt = w_rt;
        g_sites[i].call_rt = c_rt;
        g_sites[i].anchor_str_rt = s_rt;
        g_sites[i].call_bytes[0] = 0xe8;
        put_u32(g_sites[i].call_bytes + 1,
                (uint32_t)(uint64_t)((int64_t)FC1_HOST_EXEC_BASE +
                                         FC1_TARGET_OFF - (int64_t)(c_rt + 5)));
    }

    g_env.target_vaddr = 0x23d590ull;
    g_env.target_rt = FC1_HOST_EXEC_BASE + FC1_TARGET_OFF;
    g_env.image_base = 0x400000ull;
    g_env.load0_delta = 0x4000ull;
    g_env.sites = g_sites;
    g_env.nsites = FC1_NSITES;
    g_env.anchors = g_anchors;
    g_env.nanchors = FC1_NANCHORS;
}

/* ---- corruption helpers (runtime-drift scenarios) ------------------- */

static uint8_t *window_ptr(int site) {
    return g_exec + (uint64_t)site * FC1_WINDOW_STRIDE;
}

void fc1_host_corrupt_target(int byte_off, uint8_t xor_mask) {
    g_exec[FC1_TARGET_OFF + byte_off] ^= xor_mask;
}

/* corrupt the imm32 of `mov edx,1` (byte 12 of the window) */
void fc1_host_corrupt_site_edx_imm(int site, uint8_t new_imm) {
    window_ptr(site)[12] = new_imm;
}

/* retarget the call rel32 of a window (runtime drift) */
void fc1_host_corrupt_site_call_target(int site, uint64_t new_target) {
    uint32_t len = fc1_fix_window_len[site];
    uint64_t c_rt = FC1_HOST_EXEC_BASE + (uint64_t)site * FC1_WINDOW_STRIDE +
                    len - 5;
    put_u32(window_ptr(site) + len - 4,
            (uint32_t)(uint64_t)((int64_t)new_target - (int64_t)(c_rt + 5)));
}

/* replace the first two window bytes with an unsupported opcode */
void fc1_host_corrupt_site_unsupported(int site) {
    window_ptr(site)[0] = 0x0f;
    window_ptr(site)[1] = 0xa2;           /* cpuid: outside allowed subset */
}

/* ---- inspection helpers --------------------------------------------- */

int fc1_host_exec_prot(void) { return g_exec_prot; }
uint64_t fc1_host_exec_base(void) { return FC1_HOST_EXEC_BASE; }
uint32_t fc1_host_window_len(int site) { return fc1_fix_window_len[site]; }
uint32_t fc1_host_window_disp32(int site) {
    return get_u32(window_ptr(site) + 3);
}
