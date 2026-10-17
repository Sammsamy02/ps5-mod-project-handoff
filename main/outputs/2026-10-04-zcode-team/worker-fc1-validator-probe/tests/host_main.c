// host_main.c — worker-fc1-validator-probe (fc1pool1, team 2026-10-04).
// NEW FILE. Host test driver: compiles the REAL fc1pool_driver.c +
// record_sink.c + ingame_libc.c + vmmap_walk.c + sentinels.c against host
// shims and a synthetic arena really mapped at the synthetic addresses
// (MAP_FIXED, above 4 GiB — macOS arm64 constraint, fname-runtime-probe
// lineage precedent), then runs one scenario per invocation:
//
//   host_main <scenario> <out.jsonl> <ue4ss.log>
//
// The fake map (g_map/g_spans, host-defined since mapscan.c is target-only)
// covers the arena with ONE readable+writable span, so every driver
// ig_map_cover-equivalent gate behaves exactly like a real current-map gate
// for arena addresses. All label arithmetic in the driver is unchanged: the
// pool address is DERIVED (GUA_RT - 0x93ba110 + 0x9365ec0), never hardcoded.
//
// Scenarios: main | uninit | uncovered | none_mismatch | blocks0_zero |
//            map_untrusted | wide | bp_oob
// Exit 0 iff the scenario's own in-process expectations hold; the JSONL and
// UE4SS.log outputs are asserted by tests/run_host_checks.py (the oracle).
#include "anchor_store.h"
#include "fc1pool_driver.h"
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
#define HOST_GUA_RT 0x110200000ull   /* the FC1_RT_GUOBJECT_ARRAY override */
#define GUA_LABEL 0x93ba110ull
#define POOL_LABEL 0x9365ec0ull
#define INIT_FLAG_LABEL 0x9365ea0ull
/* derived exactly as the driver derives it */
#define HOST_BASE (HOST_GUA_RT - GUA_LABEL)
#define HOST_POOL (HOST_BASE + POOL_LABEL)
#define HOST_GUARD (HOST_BASE + INIT_FLAG_LABEL)
#define BLOCK0 (ARENA_BASE + 0x800000ull)
#define BLOCK1 (ARENA_BASE + 0xa00000ull)

static uint8_t *arena;
static int sleep_calls;
static int log_calls;
static int host_cover_refuse_pool;
static const char *g_scenario;
static const char *g_ue4ss_path;

/* host-defined map state (mapscan.c is target-only) */
IgMapState g_map;
IgSpan g_spans[IG_SPAN_MAX];

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

/* Same full-containment rule as the real ig_map_cover. */
static int host_cover(uint64_t addr, uint64_t len, IgSpan *out) {
    if (host_cover_refuse_pool && addr >= HOST_POOL &&
        addr < HOST_POOL + 0x20000ull) return 0;
    if (g_map.walk.truncated != 0) return 0;
    for (uint32_t i = 0; i < IG_SPAN_MAX; ++i) {
        const IgSpan *s = &g_spans[i];
        if (s->end == 0) break;
        if (addr >= s->start && len <= s->end - s->start &&
            addr - s->start <= s->end - s->start - len) {
            if (out) *out = *s;
            return 1;
        }
    }
    return 0;
}

static void host_deref(void *dst, uint64_t src, unsigned n) {
    memcpy(dst, (const void *)(uintptr_t)src, n);
}
static int host_sleep_ms(long ms) {
    (void)ms;
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

static struct Fc1Ops host_ops = {host_mapscan_run, host_cover, host_deref,
                                 host_sleep_ms, host_log_line};

extern const Fc1Ops *g_ops;  /* fc1pool_driver.c seam */

/* ---- scenario arena builders ---- */
static void build_gua(void) {
    store32(HOST_GUA_RT + 0x00, 35477);         /* first_gc */
    store32(HOST_GUA_RT + 0x04, 35476);         /* last_non_gc */
    store32(HOST_GUA_RT + 0x08, 0);
    store32(HOST_GUA_RT + 0x0C, 0);
    store64(HOST_GUA_RT + 0x10, 0x13800220a0ull); /* Objects** (canonical) */
    store64(HOST_GUA_RT + 0x18, 0);               /* PreAllocated */
    store32(HOST_GUA_RT + 0x20, 262144);          /* MaxElements */
    store32(HOST_GUA_RT + 0x24, 71959);           /* NumElements (live fact) */
    store32(HOST_GUA_RT + 0x28, 4);               /* MaxChunks */
    store32(HOST_GUA_RT + 0x2C, 2);               /* NumChunks */
}

static void put_entry(uint64_t entry_addr, const char *s) {
    size_t n = strlen(s);
    uint16_t header = (uint16_t)((uint32_t)n << 1); /* bIsWide=0, Len=n */
    memcpy((void *)(uintptr_t)entry_addr, &header, 2);
    memcpy((void *)(uintptr_t)(entry_addr + 2), s, n + 1); /* incl NUL */
}

static void build_pool(int uninit, int blocks0_zero, int none_mismatch,
                       int wide, int bp_oob) {
    memset((void *)(uintptr_t)HOST_POOL, 0, 0x40);
    *(uint8_t *)(uintptr_t)HOST_GUARD = 1;      /* init flag byte */
    store32(HOST_POOL + FC1_OFF_CURRENT_BLOCK, uninit ? 0 : 2);
    store32(HOST_POOL + FC1_OFF_CURRENT_BO, 0x1234);
    store64(HOST_POOL + FC1_OFF_BLOCKS0, blocks0_zero ? 0 : BLOCK0);
    store64(HOST_POOL + FC1_OFF_BLOCKS0 + 8, BLOCK1);
    if (uninit || blocks0_zero) return;
    memset((void *)(uintptr_t)BLOCK0, 0, 0x100);
    if (none_mismatch) put_entry(BLOCK0, "Nona"); else put_entry(BLOCK0, "None");
    put_entry(BLOCK0 + 0x80, "ByteProperty");
    store32(HOST_POOL + FC1_OFF_ID_NONE, 0);            /* block 0, off 0 */
    if (bp_oob) store32(HOST_POOL + FC1_OFF_ID_BYTEPROP, 5u << 16);
    else store32(HOST_POOL + FC1_OFF_ID_BYTEPROP, 0x80 >> 1);
    if (wide) {
        uint16_t w = 1; /* bIsWide set on the "None" entry */
        memcpy((void *)(uintptr_t)BLOCK0, &w, 2);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: host_main <scenario> <out.jsonl> <ue4ss.log>\n");
        return 2;
    }
    const char *scenario = argv[1];
    g_scenario = scenario;
    g_ue4ss_path = argv[3];
    int uninit = !strcmp(scenario, "uninit");
    int blocks0_zero = !strcmp(scenario, "blocks0_zero");
    int none_mismatch = !strcmp(scenario, "none_mismatch");
    int wide = !strcmp(scenario, "wide");
    int bp_oob = !strcmp(scenario, "bp_oob");
    int is_main = !strcmp(scenario, "main");
    host_cover_refuse_pool = !strcmp(scenario, "uncovered");

    mkdir("/tmp/fc1pool-host", 0755);  /* host config paths may be touched */
    arena = mmap((void *)(uintptr_t)ARENA_BASE, ARENA_SIZE, PROT_READ | PROT_WRITE,
                 MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena == MAP_FAILED || (uintptr_t)arena != ARENA_BASE) {
        fprintf(stderr, "arena mmap failed\n");
        return 2;
    }
    memset(arena, 0, ARENA_SIZE);
    unlink(argv[2]);
    unlink(argv[3]);

    build_gua();
    build_pool(uninit, blocks0_zero, none_mismatch, wide, bp_oob);
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
    ig_record_flush(&sb);
    if (ig_sink_write_record(&sink, &sb) != 0) return 3;

    int rc = fc1pool_driver_run(&sink);
    if (ig_sink_close(&sink) != 0) return 3;

    /* in-process expectations (structure only; content asserted in python) */
    int ok = rc == 0;
    if (!strcmp(scenario, "map_untrusted") || uninit) {
        if (sleep_calls != FC1_GRACE_POLLS - 1) ok = 0;
    } else if (is_main) {
        if (sleep_calls != 0) ok = 0;
    }
    if (log_calls != 1) ok = 0;  /* exactly ONE UE4SS.log line, every scenario */
    printf("%s: rc=%d polls=%d sleeps=%d logs=%d sink_failed=%d\n", scenario, rc,
           0, sleep_calls, log_calls, sink.failed);
    return ok ? 0 : 1;
}
