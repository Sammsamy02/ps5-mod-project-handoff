// mapscan.c — worker-anchor-validation (team 2026-10-04).
//
// CONTROLLER CORRECTION #1: minimal CURRENT-invocation readability
// verification. Before reading any address (Tick / SCO / GUA) the probe
// requires a live sysctl KERN_PROC_VMMAP span to cover it. The v3 map is
// NEVER assumed current; if the collection fails, the walk is untrusted, or
// no span covers the address, the probe REFUSES SAFELY (refusal record, no
// dereference, no crash) and the remaining checks still run.
//
// CONTROLLER CORRECTION #6: NO FULL MAP DUMP. The buffer is walked only to
// build the internal g_spans table; records carry counts/trust flags and,
// per probed address, the single covering span — never the span list.
//
// sysctl usage is the v3-lineage recipe (experiments.c ig_collect_vmmap):
// mib {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid()}, 65536-byte first
// try, doubling on ENOMEM up to IG_VMMAP_MAX (4 MiB). The walk is the
// v3 mode-1 per-record-kve_structsize walk (vmmap_walk.c, host-tested).
#include "anchor_store.h"
#include "ingame_libc.h"

#include <errno.h>
#include <stdint.h>
/* Lineage include order (v3 experiments.c): sys/types.h MUST precede
   sys/user.h on the pinned SDK (it provides register_t/u_int for the BSD
   headers ahead of them). */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>

IgMapState g_map;
IgSpan g_spans[IG_SPAN_MAX];

/* The v3 lineage allocated a 4 MiB static buffer + 16384-span table; this
   probe keeps exactly that shape (BSS, no allocation, single-threaded
   module_start by contract). */
static uint8_t g_vmmap_buf[IG_VMMAP_MAX];

/* sizeof(struct kinfo_vmentry) on the pinned SDK (1160): the sanity upper
   bound for each record's own kve_structsize. */
#define IG_KVE_SIZEOF ((uint32_t)sizeof(struct kinfo_vmentry))

static int ig_errno(void) {
    int *e = __error();
    return e ? *e : 0;
}

int ig_mapscan_run(void) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid()};
    size_t len = 65536;
    int rc = -1;
    unsigned attempts = 0;
    ig_memset(&g_map, 0, sizeof g_map);
    for (;;) {
        ++attempts;
        rc = sysctl(mib, 4, g_vmmap_buf, &len, 0, 0);
        if (rc == 0) break;
        int eno = ig_errno();
        if (eno == ENOMEM && len > 0 && (uint64_t)len * 2 <= IG_VMMAP_MAX) {
            len *= 2;
            continue;
        }
        break;  /* non-ENOMEM failure or the 4 MiB growth cap */
    }
    g_map.rc = rc;
    g_map.eno = ig_errno();
    g_map.attempts = (int)attempts;
    g_map.buf_len = (uint64_t)len;
    if (rc != 0 || len < IG_VM_HDR) {
        return 0;  /* untrusted: every foreign deref must be refused */
    }
    /* span-arbitration assertion evidence: first record's own structsize
       (expect 144, path stripped) must divide the buffer exactly. */
    uint32_t ks0 = (uint32_t)g_vmmap_buf[0] | ((uint32_t)g_vmmap_buf[1] << 8) |
                   ((uint32_t)g_vmmap_buf[2] << 16) |
                   ((uint32_t)g_vmmap_buf[3] << 24);
    g_map.first_ksz = ks0;
    if (ks0 >= IG_VM_HDR && ks0 <= IG_KVE_SIZEOF && (ks0 & 3u) == 0 &&
        (len % ks0) == 0) {
        g_map.mod_ok = 1;
    }
    uint32_t kept = ig_vmmap_walk(g_vmmap_buf, len, IG_KVE_SIZEOF,
                                  /*mode=*/1, g_spans, IG_SPAN_MAX,
                                  &g_map.walk, 0, 0);
    g_map.trusted = ig_vmmap_trustworthy(&g_map.walk);
    uint32_t readable = 0;
    for (uint32_t i = 0; i < kept; ++i)
        if (g_spans[i].prot & 1u) ++readable;
    g_map.readable_spans = readable;
    return g_map.trusted;
}

int ig_map_cover(uint64_t addr, uint64_t len, IgSpan *out) {
    /* Exact bound: the walk retains sane records packed from index 0.
       (retained == kept unless the 16384 cap truncated; truncation flips
       g_map.trusted off anyway via nothing — so also refuse when truncated
       to keep coverage complete.) */
    if (g_map.walk.truncated != 0) return 0;
    uint32_t n = g_map.walk.retained;
    if (n > IG_SPAN_MAX) n = IG_SPAN_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        const IgSpan *s = &g_spans[i];
        if (addr >= s->start && len <= s->end - s->start &&
            addr - s->start <= s->end - s->start - len) {
            if (out) *out = *s;
            return 1;
        }
    }
    return 0;
}
