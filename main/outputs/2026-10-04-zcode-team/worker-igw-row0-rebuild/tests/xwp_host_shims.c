// xwp_host_shims.c — host-side OS mock for the exec-write probe suite
// (worker-writability-probe-build, team 2026-10-04).
//
// The PRX compiles the lineage mapscan.c (real sysctl KERN_PROC_VMMAP);
// the HOST build compiles THIS file instead: it mocks the record-channel
// trio (forwarded to POSIX open/write/close), __error, getpid, and — the
// point of the suite — a SCRIPTED sceKernelMprotect MOCK whose call log
// the driver asserts (order, prot arguments, budget, restore pairing).
// ig_mapscan_run/ig_map_cover are mocked from a fixed span table (the real
// vmmap walk is v3-lineage host-tested; this suite targets the probe's
// record/DECISION logic).
#include "anchor_store.h"
#include "exec_write_probe.h"
#include "ingame_libc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* ---------------- record channel trio -> POSIX ---------------- */
/* __error: NOT redefined on the host — macOS libc already provides
   int *__error(void) returning &errno, which is exactly the PRX semantic. */
int sceKernelOpen(const char *path, int flags, uint16_t mode) {
    return open(path, flags, (mode_t)mode);
}
int64_t sceKernelWrite(int fd, const void *buf, size_t n) {
    return (int64_t)write(fd, buf, n);
}
int sceKernelClose(int fd) { return close(fd); }
int sysctl(int *mib, unsigned namelen, void *oldp, size_t *oldlenp,
           void *newp, size_t newlen) {
    (void)mib; (void)namelen; (void)oldp; (void)oldlenp;
    (void)newp; (void)newlen;
    errno = ENOMEM; /* never actually used by the mock map */
    return -1;
}

/* ---------------- mock map (fixed span table) ---------------- */
IgMapState g_map;
IgSpan g_spans[IG_SPAN_MAX];

typedef struct MockSpan { uint64_t start, end; uint32_t prot; } MockSpan;

static const MockSpan k_mock_map[] = {
    /* filler sane records (>=8 retained for trustworthiness) */
    {0x100000000ull, 0x100400000ull, XWP_PROT_R | XWP_PROT_X},
    {0x200000000ull, 0x200800000ull, XWP_PROT_R | XWP_PROT_W},
    {0x200800000ull, 0x200c00000ull, XWP_PROT_R},
    {0x210000000ull, 0x210400000ull, XWP_PROT_R | XWP_PROT_W | XWP_PROT_X},
    {0x220000000ull, 0x220200000ull, XWP_PROT_R | XWP_PROT_W},
    /* probe-owned rows */
    {0x180000000ull, 0x180100000ull, XWP_PROT_X},                  /* row0 RX ok   */
    {0x181000000ull, 0x181100000ull, XWP_PROT_X},                  /* row2 RWX fail*/
    {0x182000000ull, 0x182100000ull, XWP_PROT_X},                  /* row5 RWX ok  */
    {0x183000000ull, 0x183100000ull, XWP_PROT_R | XWP_PROT_X},     /* row3 mismatch*/
    {0x185000000ull, 0x185100000ull, XWP_PROT_X},                  /* row1 unconf  */
    {0x190000000ull, 0x197000000ull, XWP_PROT_X},                  /* rows6..12    */
};
#define MOCK_MAP_N (sizeof k_mock_map / sizeof k_mock_map[0])

static void mock_touch_pages(void) {
    /* The rw_test dereferences real addresses; map them so the host can
       execute the full sequence. NOT_COVERED 0x184000000 stays unmapped. */
    static const uint64_t bases[] = {
        0x180000000ull, 0x181000000ull, 0x182000000ull, 0x183000000ull,
        0x185000000ull, 0x190000000ull,
    };
    for (size_t i = 0; i < sizeof bases / sizeof bases[0]; ++i) {
        void *p = mmap((void *)(uintptr_t)bases[i], 0x10000,
                       PROT_READ | PROT_WRITE,
                       MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) {
            fprintf(stderr, "host mmap failed for 0x%llx: %s\n",
                    (unsigned long long)bases[i], strerror(errno));
            exit(2);
        }
        memset(p, (int)(0xA0 + i), 0x10000);
    }
}

int ig_mapscan_run(void) {
    static int inited;
    if (!inited) { mock_touch_pages(); inited = 1; }
    ig_memset(&g_map, 0, sizeof g_map);
    uint32_t kept = 0;
    for (size_t i = 0; i < MOCK_MAP_N && kept < IG_SPAN_MAX; ++i) {
        g_spans[kept].start = k_mock_map[i].start;
        g_spans[kept].end = k_mock_map[i].end;
        g_spans[kept].prot = k_mock_map[i].prot;
        ++kept;
    }
    g_map.walk.retained = kept;
    g_map.walk.insane = 0;
    g_map.rc = 0;
    g_map.trusted = ig_vmmap_trustworthy(&g_map.walk);
    return g_map.trusted;
}

int ig_map_cover(uint64_t addr, uint64_t len, IgSpan *out) {
    if (g_map.walk.truncated != 0) return 0;
    for (uint32_t i = 0; i < g_map.walk.retained; ++i) {
        const IgSpan *s = &g_spans[i];
        if (addr >= s->start && len <= s->end - s->start &&
            addr - s->start <= s->end - s->start - len) {
            if (out) *out = *s;
            return 1;
        }
    }
    return 0;
}

/* ---------------- scripted mprotect MOCK ---------------- */
#define MOCK_LOG_MAX 64
typedef struct MockCall { uint64_t addr, len, prot; int rc, eno; } MockCall;
MockCall g_mock_log[MOCK_LOG_MAX];
unsigned g_mock_calls;
uint64_t g_mock_fail_addr;   /* the scripted FAILURE address (0 = none) */
uint64_t g_mock_last_touched_prot[8]; /* prot the mock "applied" per addr slot */

int sceKernelMprotect(void *a, size_t n, int p) {
    if (g_mock_calls >= MOCK_LOG_MAX) {
        fprintf(stderr, "mock log overflow\n");
        exit(2);
    }
    MockCall *c = &g_mock_log[g_mock_calls++];
    c->addr = (uint64_t)(uintptr_t)a;
    c->len = n;
    c->prot = (uint64_t)p;
    if (g_mock_fail_addr != 0 && c->addr == g_mock_fail_addr && (p & 2)) {
        /* Scripted kernel refusal: WRITE-BEARING requests (W bit) to the
           scripted address fail with ENOMEM; the restore (prot=X) must
           still succeed — modelling "kernel refuses RWX on this page". */
        c->rc = -1;
        c->eno = ENOMEM;
        errno = ENOMEM;
        return -1;
    }
    c->rc = 0;
    c->eno = 0;
    return 0;
}

void mock_reset(int fail_addr_set, uint64_t fail_addr) {
    ig_memset(g_mock_log, 0, sizeof g_mock_log);
    g_mock_calls = 0;
    g_mock_fail_addr = fail_addr_set ? fail_addr : 0;
}
