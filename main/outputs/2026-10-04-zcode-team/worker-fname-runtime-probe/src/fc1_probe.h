/* fc1_probe.h — worker-fname-runtime-probe (team 2026-10-04).
 *
 * FC-1 runtime verification add-on: validates the FName ctor call-site
 * candidate (vaddr 0x23d590, runtime 0x63d590) in the live exec span.
 *
 * DESIGN CONTRACT (matches the igf5 corrected mprotect-first design,
 * worker-code-anchor-reads/src/code_anchor_probe.cpp STEP1-4):
 *   - NEVER raw-read an exec-only page. Every exec-span read is preceded
 *     by mprotect(cover_pages, PROT_READ|PROT_EXEC) and followed by
 *     restore of the original protection; a read happens ONLY after
 *     mprotect success. mprotect failure => refusal record, no read.
 *   - Original protection comes from the current-invocation VMMAP
 *     covering span when the map is trusted; the checkpoint constant
 *     (IGFC1_EXEC_PROT_FALLBACK) is a LABELED fallback only.
 *   - FC1-C fail-closed: any recorded site disagreeing fails the whole
 *     FC-1 verdict (never partial credit).
 *   - FC1-D anchor strings are DATA-region reads: never mprotect'd, only
 *     read when the trusted current map shows the covering span readable;
 *     they never gate the code verdict.
 *
 * Two entry points:
 *   ig_run_fc1_probe(IgSink*)            — config-driven (igf6 build)
 *   ig_run_fc1_probe_env(IgSink*, env)   — injectable tables (host tests;
 *                                          the config entry forwards to it)
 */
#ifndef IGFC1_PROBE_H
#define IGFC1_PROBE_H

/* record_sink.h is the UNGUARDED lineage header (worker-code-anchor-reads);
   C++ consumers must see its declarations with C linkage, exactly as
   code_anchor_probe.cpp wraps it. */
#ifdef __cplusplus
extern "C" {
#endif
#include "record_sink.h"
#ifdef __cplusplus
}
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Fc1SiteSpec {
    uint8_t  anchor;          /* 0..4: Engine, Renderer, AnimGraphRuntime,
                                 Landscape, RenderCore (resolver NAME order) */
    uint64_t window_vaddr;    /* static window start */
    uint64_t call_vaddr;      /* static E8 address */
    uint64_t window_rt;       /* runtime = base + vaddr */
    uint64_t call_rt;
    uint64_t anchor_str_rt;   /* runtime addr of the UTF-16LE anchor string */
    uint8_t  call_bytes[5];   /* recorded E8 + rel32 (offline capture) */
} Fc1SiteSpec;

typedef struct Fc1AnchorSpec {
    const char *name;         /* anchor name (ASCII, for records) */
    uint64_t content_rt;      /* runtime addr of the UTF-16LE content */
    const uint8_t *u16;       /* expected bytes: UTF-16LE name + NUL */
    uint16_t u16_len;         /* expected byte length */
} Fc1AnchorSpec;

typedef struct Fc1Env {
    uint64_t target_vaddr;    /* 0x23d590 */
    uint64_t target_rt;       /* 0x63d590 */
    uint64_t image_base;      /* 0x400000 */
    uint64_t load0_delta;     /* file_off = vaddr + 0x4000 */
    const Fc1SiteSpec *sites;
    uint32_t nsites;          /* 9 */
    const Fc1AnchorSpec *anchors;
    uint32_t nanchors;        /* 5 */
} Fc1Env;

/* Add-on entry for the NEXT probe's module_start / probe runner.
   Emits records at seq IGFC1_SEQ_BASE .. IGFC1_SEQ_BASE+11:
   base+0 fc1_a_candidate, base+1..9 fc1_b_site_i, base+10 fc1_d_anchors,
   base+11 fc1_summary. Uses the generated fc1_config.h tables. */
void ig_run_fc1_probe(IgSink *sink);

/* Injectable variant (host tests / future retargets). */
void ig_run_fc1_probe_env(IgSink *sink, const Fc1Env *env);

#ifdef __cplusplus
}
#endif

#endif /* IGFC1_PROBE_H */
