// host_main.c — worker-carrier-probe-build (cc1, team 2026-10-04).
// NEW FILE. Host test driver: compiles the REAL carrier_driver.c +
// record_sink.c + ingame_libc.c + vmmap_walk.c + sentinels.c against host
// shims and a synthetic arena really mapped at the synthetic addresses
// (MAP_FIXED, above 4 GiB — macOS arm64 constraint, fname-runtime-probe /
// fc1pool1 lineage precedent), then runs one scenario per invocation:
//
//   host_main <scenario> <out.jsonl> <ue4ss.log>
//
// The fake map (g_map/g_spans, host-defined since mapscan.c is target-only)
// covers the arena with readable spans, so every driver cover-equivalent
// gate behaves exactly like a real current-map gate for arena addresses.
// All label arithmetic in the driver is unchanged: the pool address is
// DERIVED (GUA_RT - 0x93ba110 + 0x9365ec0), never hardcoded.
//
// Scenarios (verdict/reason asserted by tests/run_host_checks.py, the
// strict-JSONL oracle):
//   pass path:  main | negative_clean | wide_only | budget*
//   grace:      map_untrusted | anchor_refused | gate_threshold | uninit
//   hard pool:  uncovered | pool_unreadable | cb_insane | blocks0_zero |
//               blocks0_uncovered | handles_uncovered | ids_degenerate |
//               bp_oob | bp_blockptr_zero | bp_blockptr_uncovered |
//               bp_entry_uncovered | bp_wide | none_mismatch | bp_mismatch
//   census:     census_blockptr_bad | census_entry_uncovered
//   (* budget runs on a second binary compiled with a tiny census budget)
// Exit 0 iff the scenario's own in-process expectations hold.
#include "anchor_store.h"
#include "carrier_driver.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ---- shims for the record_sink import trio (verbatim-row names) ---- */
static int g_host_err;
int sceKernelOpen(const char *path, int flags, uint16_t mode) {
    return open(path, flags, (mode_t)mode);
}
int64_t sceKernelWrite(int fd, const void *buf, size_t n) {
    return (int64_t)write(fd, buf, n);
}
int sceKernelClose(int fd) { return close(fd); }
int *__error(void) { return &g_host_err; }

/* lineage counters (defined by module_entry.c on target; not compiled here) */
unsigned g_ctor_events;
unsigned g_fini_events;

/* ---- the synthetic arena ---- */
#define ARENA_BASE 0x110000000ull
#define ARENA_SIZE 0x1000000ull /* 16 MiB */
#define HOST_GUA_RT 0x110200000ull /* the CC1_RT_GUOBJECT_ARRAY override */
#define GUA_LABEL 0x93ba110ull
#define POOL_LABEL 0x9365ec0ull
#define INIT_FLAG_LABEL 0x9365ea0ull
/* derived exactly as the driver derives it */
#define HOST_BASE (HOST_GUA_RT - GUA_LABEL)
#define HOST_POOL (HOST_BASE + POOL_LABEL)
#define HOST_GUARD (HOST_BASE + INIT_FLAG_LABEL)
#define BLOCK0 (ARENA_BASE + 0x800000ull)
#define BLOCK1 (ARENA_BASE + 0xa00000ull)
#define BLOCK2 (ARENA_BASE + 0xc00000ull)

static uint8_t *arena;
static int sleep_calls;
static int log_calls;
static const char *g_scenario;
static const char *g_ue4ss_path;

/* host-defined map state (mapscan.c is target-only) */
IgMapState g_map;
IgSpan g_spans[IG_SPAN_MAX];

/* refusal ranges: cover() refuses any address inside one */
struct Range { uint64_t lo, hi; };
static struct Range g_refuse[4];
static int g_refuse_n;
static int g_pool_prot_mask;  /* drop span prot bits for pool-window addrs */

static void store32(uint64_t addr, uint32_t v) {
    memcpy((void *)(uintptr_t)addr, &v, 4);
}
static void store64(uint64_t addr, uint64_t v) {
    memcpy((void *)(uintptr_t)addr, &v, 8);
}

static void host_reset_map(void) {
    memset(&g_map, 0, sizeof g_map);
    memset(g_spans, 0, sizeof g_spans);
    g_spans[0].start = ARENA_BASE;
    g_spans[0].end = ARENA_BASE + ARENA_SIZE;
    g_spans[0].prot = 3;
    g_map.trusted = 1;
    g_map.walk.retained = 1;
    g_map.walk.truncated = 0;
    g_map.readable_spans = 1;
}

static int host_mapscan_run(void) {
    if (!strcmp(g_scenario, "map_untrusted")) return 0;
    host_reset_map();
    return g_map.trusted;
}

static int addr_in_refuse(uint64_t addr, uint64_t len) {
    for (int i = 0; i < g_refuse_n; ++i)
        if (addr >= g_refuse[i].lo && addr + len <= g_refuse[i].hi) return 1;
    return 0;
}

/* Same full-containment rule as the real ig_map_cover. */
static int host_cover(uint64_t addr, uint64_t len, IgSpan *out) {
    if (addr_in_refuse(addr, len)) return 0;
    if (g_map.walk.truncated != 0) return 0;
    for (uint32_t i = 0; i < IG_SPAN_MAX; ++i) {
        const IgSpan *s = &g_spans[i];
        if (s->end == 0) break;
        if (addr >= s->start && len <= s->end - s->start &&
            addr - s->start <= s->end - s->start - len) {
            if (out) {
                *out = *s;
                if (g_pool_prot_mask && addr >= HOST_POOL &&
                    addr < HOST_POOL + 0x20000ull)
                    out->prot &= ~(uint32_t)1u;
            }
            return 1;
        }
    }
    return 0;
}

static void host_deref(void *dst, uint64_t src, unsigned n) {
    memcpy(dst, (const void *)(uintptr_t)src, n);
}
static int host_sleep_sec(long sec) {
    (void)sec;
    ++sleep_calls;
    return 0;
}
static int host_log_line(const char *line, unsigned len) {
    ++log_calls;
    FILE *f = fopen(g_ue4ss_path, "a");
    if (!f) return -1;
    fwrite(line, 1, len, f);
    fclose(f);
    return 0;
}

static struct Cc1Ops host_ops = {host_mapscan_run, host_cover, host_deref,
                                 host_sleep_sec, host_log_line};

extern const Cc1Ops *g_ops;  /* carrier_driver.c seam */

/* ---- scenario arena builders ---- */
static void build_gua(uint32_t num_elements) {
    store32(HOST_GUA_RT + 0x00, 35477);         /* first_gc */
    store32(HOST_GUA_RT + 0x04, 35476);         /* last_non_gc */
    store32(HOST_GUA_RT + 0x08, 0);
    store32(HOST_GUA_RT + 0x0C, 0);
    store64(HOST_GUA_RT + 0x10, 0x13800220a0ull); /* Objects** (canonical) */
    store64(HOST_GUA_RT + 0x18, 0);               /* PreAllocated */
    store32(HOST_GUA_RT + 0x20, 262144);          /* MaxElements */
    store32(HOST_GUA_RT + 0x24, num_elements);    /* NumElements */
    store32(HOST_GUA_RT + 0x28, 4);               /* MaxChunks */
    store32(HOST_GUA_RT + 0x2C, 2);               /* NumChunks */
}

static void put_entry(uint64_t entry_addr, const char *s) {
    size_t n = strlen(s);
    /* CANDIDATES sec 6.2 header law: bIsWide = bit0, Len = bits15:6 */
    uint16_t header = (uint16_t)((uint32_t)n << 6);
    memcpy((void *)(uintptr_t)entry_addr, &header, 2);
    memcpy((void *)(uintptr_t)(entry_addr + 2), s, n + 1); /* incl NUL */
}

static void wide_entry(uint64_t entry_addr) {
    uint16_t header;
    memcpy(&header, (void *)(uintptr_t)entry_addr, 2);
    header |= 1; /* bIsWide */
    memcpy((void *)(uintptr_t)entry_addr, &header, 2);
}

/* Main pass pool: the positive controls, all three carrier candidates, one
   family control, three class strings, and the PostRender negative control
   PRESENT (the census counts it in negative_hits; in-game the honest
   expectation is 0). ZZZ stays absent. */
static void build_pool(uint32_t cur_block, int none_mismatch, int bp_mismatch,
                       int bp_wide, int wide_tick, int bp_oob,
                       int bp_blockptr_zero, int bp_blockptr_uncovered,
                       int bp_entry_uncovered, int ids_degenerate,
                       int blocks0_zero, int blocks0_uncovered,
                       int census_blockptr_bad, int census_entry_uncovered) {
    memset((void *)(uintptr_t)HOST_POOL, 0, 0x40);
    *(uint8_t *)(uintptr_t)HOST_GUARD = 1;      /* init flag byte */
    store32(HOST_POOL + CC1_OFF_CURRENT_BLOCK, cur_block);
    store32(HOST_POOL + CC1_OFF_CURRENT_BO, 0x1234);
    store64(HOST_POOL + CC1_OFF_BLOCKS0,
            blocks0_zero ? 0
            : blocks0_uncovered ? 0x5000000000ull : BLOCK0);
    store64(HOST_POOL + CC1_OFF_BLOCKS0 + 8,
            (bp_entry_uncovered || census_entry_uncovered)
                ? (ARENA_BASE + ARENA_SIZE - 0x10ull)
                : bp_blockptr_uncovered ? 0x5000000000ull : BLOCK1);
    store64(HOST_POOL + CC1_OFF_BLOCKS0 + 16,
            census_blockptr_bad ? 0 : BLOCK2);
    if (cur_block == 0) return;
    memset((void *)(uintptr_t)BLOCK0, 0, 0x100);
    memset((void *)(uintptr_t)BLOCK1, 0, 0x100);
    if (!none_mismatch) put_entry(BLOCK0, "None");
    else put_entry(BLOCK0, "Nona");
    if (!bp_mismatch) put_entry(BLOCK0 + 0x80, "ByteProperty");
    else put_entry(BLOCK0 + 0x80, "ByteProperti");
    if (bp_wide) wide_entry(BLOCK0 + 0x80);
    put_entry(BLOCK0 + 0x400, "ReceiveTick");
    if (wide_tick) wide_entry(BLOCK0 + 0x400);
    put_entry(BLOCK0 + 0x800, "Tick");
    put_entry(BLOCK0 + 0xc00, "BlueprintUpdateAnimation");
    put_entry(BLOCK0 + 0x1000, "PostRender");
    put_entry(BLOCK0 + 0x1400, "ReceiveBeginPlay");
    put_entry(BLOCK0 + 0x1800, "Actor");
    put_entry(BLOCK0 + 0x1c00, "Pawn");
    put_entry(BLOCK0 + 0x2000, "ItemDatabaseSubsystem");
    put_entry(BLOCK1, "GameViewportClient");
    store32(HOST_POOL + CC1_OFF_ID_NONE, 0);               /* blk 0, off 0 */
    if (ids_degenerate) store32(HOST_POOL + CC1_OFF_ID_BYTEPROP, 0);
    else if (bp_oob) store32(HOST_POOL + CC1_OFF_ID_BYTEPROP, 5u << 16);
    else if (bp_blockptr_zero || bp_blockptr_uncovered)
        store32(HOST_POOL + CC1_OFF_ID_BYTEPROP, (1u << 16) | 0x40u);
    else if (bp_entry_uncovered)
        store32(HOST_POOL + CC1_OFF_ID_BYTEPROP, (1u << 16) | 0x800u);
    else store32(HOST_POOL + CC1_OFF_ID_BYTEPROP, 0x80 >> 1); /* 0x40 */
    if (bp_blockptr_zero) store64(HOST_POOL + CC1_OFF_BLOCKS0 + 8, 0);
    if (bp_blockptr_uncovered) store64(HOST_POOL + CC1_OFF_BLOCKS0 + 8,
                                       0x5000000000ull);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: host_main <scenario> <out.jsonl> <ue4ss.log>\n");
        return 2;
    }
    const char *scenario = argv[1];
    g_scenario = scenario;
    g_ue4ss_path = argv[3];
    uint32_t cur_block = 2;
    uint32_t num_elements = 71959;  /* the live-validated GUA count */
    int none_mismatch = !strcmp(scenario, "none_mismatch");
    int bp_mismatch = !strcmp(scenario, "bp_mismatch");
    int bp_wide = !strcmp(scenario, "bp_wide");
    int wide_tick = !strcmp(scenario, "wide_only");
    int bp_oob = !strcmp(scenario, "bp_oob");
    int bp_blockptr_zero = !strcmp(scenario, "bp_blockptr_zero");
    int bp_blockptr_uncovered = !strcmp(scenario, "bp_blockptr_uncovered");
    int bp_entry_uncovered = !strcmp(scenario, "bp_entry_uncovered");
    int ids_degenerate = !strcmp(scenario, "ids_degenerate");
    int blocks0_uncovered = !strcmp(scenario, "blocks0_uncovered");
    int blocks0_zero = !strcmp(scenario, "blocks0_zero");
    int census_blockptr_bad = !strcmp(scenario, "census_blockptr_bad");
    int census_entry_uncovered = !strcmp(scenario, "census_entry_uncovered");
    if (!strcmp(scenario, "uninit")) cur_block = 0;
    if (!strcmp(scenario, "cb_insane")) cur_block = 20000;
    if (census_blockptr_bad) cur_block = 3;
    if (!strcmp(scenario, "negative_clean")) {
        /* pool carries ONLY the two positive controls */
        cur_block = 2;
    }
    if (!strcmp(scenario, "anchor_refused")) num_elements = 0;
    if (!strcmp(scenario, "gate_threshold")) num_elements = 5000;

    if (!strcmp(scenario, "uncovered")) {
        g_refuse[g_refuse_n++] =
            (struct Range){HOST_POOL, HOST_POOL + 0x20000ull};
    }
    if (!strcmp(scenario, "pool_unreadable")) g_pool_prot_mask = 1;
    if (!strcmp(scenario, "handles_uncovered")) {
        g_refuse[g_refuse_n++] = (struct Range){HOST_POOL + CC1_OFF_ID_NONE,
                                                HOST_POOL + CC1_OFF_ID_NONE +
                                                    CC1_HANDLES_WINDOW};
    }

    mkdir("/tmp/cc1-host", 0755);  /* host config paths may be touched */
    arena = mmap((void *)(uintptr_t)ARENA_BASE, ARENA_SIZE,
                 PROT_READ | PROT_WRITE,
                 MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena == MAP_FAILED || (uintptr_t)arena != ARENA_BASE) {
        fprintf(stderr, "arena mmap failed\n");
        return 2;
    }
    memset(arena, 0, ARENA_SIZE);
    unlink(argv[2]);
    unlink(argv[3]);

    build_gua(num_elements);
    if (strcmp(scenario, "map_untrusted") != 0 &&
        strcmp(scenario, "anchor_refused") != 0) {
        build_pool(cur_block, none_mismatch, bp_mismatch, bp_wide, wide_tick,
                   bp_oob, bp_blockptr_zero, bp_blockptr_uncovered,
                   bp_entry_uncovered, ids_degenerate, blocks0_zero,
                   blocks0_uncovered, census_blockptr_bad,
                   census_entry_uncovered);
    }
    if (strcmp(scenario, "negative_clean") == 0) {
        /* strip everything but the two positive controls */
        memset((void *)(uintptr_t)(BLOCK0 + 0x300), 0, 0x1e00);
        memset((void *)(uintptr_t)BLOCK1, 0, 0x100);
    }
    host_reset_map();

    g_ops = &host_ops;

    static IgSink sink;
    sink.fd = -1;
    sink.failed = 0;
    sink.open_errno = 0;
    if (ig_sink_open(&sink, argv[2]) != 0) return 3;
    IgBuf eb;
    ig_record_begin(&eb, 1, "entry", 0, 0, IG_BUILD_ID);
    ig_kv_str(&eb, "phase", "module_start", 32);
    ig_kv_str(&eb, "module", "cc1", 8);
    ig_kv_str(&eb, "note", "host test entry", 64);
    ig_record_flush(&eb);
    if (ig_sink_write_record(&sink, &eb) != 0) return 3;
    IgBuf sb;
    ig_record_begin(&sb, 2, "startup", 0, 0, IG_BUILD_ID);
    ig_kv_str(&sb, "phase", "post_init_array", 32);
    ig_kv_int(&sb, "ctors_run", 1);
    ig_kv_int(&sb, "channel_open_rc", 0);
    ig_kv_int(&sb, "channel_open_errno", 0);
    ig_kv_int(&sb, "entry_marker_written", 1);
    ig_kv_int(&sb, "write_failures_so_far", 0);
    ig_kv_int(&sb, "polls", (int64_t)CC1_POLL_COUNT);
    ig_kv_int(&sb, "poll_s", (int64_t)CC1_POLL_SECONDS);
    ig_record_flush(&sb);
    if (ig_sink_write_record(&sink, &sb) != 0) return 3;

    int rc = carrier_driver_start(&sink);
    if (ig_sink_close(&sink) != 0) return 3;

    /* in-process expectations (structure only; content asserted in python) */
    int ok = rc == 0;
    if (sleep_calls != (int)CC1_POLL_COUNT - 1) ok = 0; /* polls 1..29 */
    if (log_calls != 1) ok = 0;  /* exactly ONE UE4SS.log line, every scenario */
    printf("%s: rc=%d sleeps=%d logs=%d sink_failed=%d\n", scenario, rc,
           sleep_calls, log_calls, sink.failed);
    return ok ? 0 : 1;
}
