// anchor_store.h — worker-anchor-validation (team 2026-10-04).
// MINIMAL subset of worker-ingame-probe-v3/src/factstore.h: only the types
// and declarations this focused probe needs (IgSpan / IgVmStats / walk API /
// map state). Values are byte-identical to the v3 lineage definitions;
// everything not used by this probe (base-hunt, magic, sweep, trial, match,
// rfl and link-map state) is intentionally absent.
//
// CONTROLLER CORRECTION #1 (this worker): before ANY foreign dereference the
// probe verifies readability against a CURRENT per-invocation sysctl
// KERN_PROC_VMMAP scan (bounded collection, mode-1 lineage walk). The v3
// map is never assumed current. The scan is used ONLY internally to answer
// coverage questions; NO map dump records are emitted (controller
// correction #6) — records carry only counts, trust flags and, per probed
// address, the single covering span's [start,end,prot].
#ifndef INGAME_ANCHOR_STORE_H
#define INGAME_ANCHOR_STORE_H

#include "ingame_abi.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IG_SPAN_MAX 16384       /* v3 lineage cap: covers 10088-record maps */
#define IG_VMMAP_MAX (4u << 20) /* bounded collection: 4 MiB growth cap */
#define IG_VM_HDR 136           /* SDK kinfo_vmentry bytes before kve_path */

typedef struct IgSpan {
    uint64_t start;   /* inclusive */
    uint64_t end;     /* exclusive */
    uint32_t prot;    /* raw kve_protection, uninterpreted (bit0 readable) */
} IgSpan;

typedef struct IgVmStats {
    uint32_t total;       /* records consumed by this walk (any sanity) */
    uint32_t retained;    /* sane records kept into out[] */
    uint32_t truncated;   /* sane records dropped by the out cap */
    uint32_t insane;      /* records failing per-record sanity */
    uint32_t raws_kept;   /* raw header snapshots stored (0 here) */
    int walk_mode;        /* 1 = per-record kve_structsize */
    int stop_reason;      /* 0 clean, 1 insane/short structsize, 2 partial */
    uint64_t stop_off;    /* byte offset where the walk stopped */
    uint32_t ksz_full;    /* structsize == sizeof(kinfo_vmentry) records */
    uint32_t ksz_var;     /* 137..sizeof-1 (variable/path-truncated) */
    uint32_t ksz_144;     /* structsize == 144 records (P1 prediction) */
    uint32_t aligned16k;  /* sane records with 16 KiB-aligned start AND end */
} IgVmStats;

/* Pure VMMAP buffer walk (vmmap_walk.c; v3 host-unit-tested, include-line
   change only). Mode 1: stride by each record's own kve_structsize. */
uint32_t ig_vmmap_walk(const uint8_t *buf, uint64_t len, uint32_t kve_sizeof,
                       int mode, IgSpan *out, uint32_t cap, IgVmStats *st,
                       uint8_t *raws, uint32_t raw_cap);
/* Trust gate: only a walk with enough sane records may back containment. */
int ig_vmmap_trustworthy(const IgVmStats *st);

/* mapscan.c — ONE bounded current-invocation collection + walk. */
typedef struct IgMapState {
    int rc;               /* final sysctl rc (0 ok) */
    int eno;              /* errno at final attempt */
    int attempts;         /* bounded doubling attempts used */
    int trusted;          /* ig_vmmap_trustworthy(walk) after the run */
    int mod_ok;           /* buffer_len % first_ksz == 0 assertion (P1) */
    uint64_t buf_len;     /* final sysctl buffer length */
    uint32_t first_ksz;   /* first record's own kve_structsize (expect 144) */
    uint32_t readable_spans; /* retained spans with prot bit0 set */
    IgVmStats walk;       /* mode-1 walk stats */
} IgMapState;

extern IgMapState g_map;
extern IgSpan g_spans[IG_SPAN_MAX];

/* Collect + walk once. Returns g_map.trusted. Never dereferences foreign
   memory; emits nothing. */
int ig_mapscan_run(void);

/* 1 iff some retained span covers [addr, addr+len) fully; the covering span
   is copied to *out. Coverage is by ADDRESS; the caller checks prot bits
   (bit0 readable, bit1 writable) for the specific access kind. */
int ig_map_cover(uint64_t addr, uint64_t len, IgSpan *out);

#ifdef __cplusplus
}
#endif

#endif  // INGAME_ANCHOR_STORE_H
