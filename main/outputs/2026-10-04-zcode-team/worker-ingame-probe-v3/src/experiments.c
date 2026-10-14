// experiments.c
//
// V1..V5 loader-fact experiments for the v3 probe PRX
// (worker-ingame-probe-v3, team 2026-10-04). Vendored from
// worker-ingame-probe-v2/src/experiments.c with the two v2 defects fixed
// and the base hunt rebuilt around the PROVEN segment correspondence:
//
//   1. RECORD WRITER: strict comma-correct JSON via the v3 record_sink
//      (v1/v2 emitted `"k":v"k2":v2` — verified against the raw v2 log).
//   2. VMMAP WALK: kve_structsize-strided (vmmap_walk.c). The SDK struct
//      kinfo_vmentry begins with "Variable size of record"; v1/v2 walked a
//      fixed sizeof()=1160 stride, corrupting most page records.
//   3. BASE HUNT (V2) rebuilt per worker-vaddr-semantics
//      (SEGMENT-CORRESPONDENCE.md): survey values are ELF p_vaddrs; L0 is
//      PROVEN mapped at 0x400000 (EXEC span byte-exact); Tick/SCO runtime
//      anchors are 0x33ac8c0/0x890aa0; GUObjectArray lives in L7 BSS at
//      mapped_start(L7)+0x27e110 — mapped_start(L7) is the hunt target.
//      Recorded hypotheses (never assumed): h1 span-base (B+vaddr),
//      h2 segment-translated, h2s segment-table L7/chain/grid hunt,
//      h3 absolute vaddr.
//   4. CANDIDATE DISCIPLINE (controller amendment): the GUA predicate is
//      candidate DISCOVERY ONLY — run at every viable candidate, all
//      matches preserved, ambiguity recorded, promotion requires the two
//      survey anchor byte checks + L7 segment consistency, each recorded.
//
// Fault containment: the VMMAP span snapshot is collected FIRST and every
// foreign dereference is validated against readable (prot bit0) spans
// BEFORE the dereference. ONE LABELED EXCEPTION (bounded, hardware-backed):
// ig_range_mapped_readable() also accepts exec-only (prot bit2) spans for
// FIXED <=16-byte anchor reads (tier-a ELF magic at X-span starts, segment
// content anchors, Tick/SCO anchor bytes). x86-64 paging cannot express
// true execute-only, so a data read of a page we SEE mapped cannot fault;
// this exception is never used for pointer chases or variable data.
#include "experiments.h"
#include "factstore.h"
#include "ingame_abi.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>
#include <errno.h>
#include <sys/types.h>   /* u_int* for SDK elf headers, ahead of them */
#include <sys/elf64.h>
#include <sys/link_elf.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <time.h>
#include <unistd.h>

#ifndef PT_LOAD
#define PT_LOAD 1
#endif
#ifndef PT_DYNAMIC
#define PT_DYNAMIC 2
#endif
#ifndef PF_X
#define PF_X 0x1
#endif
#ifndef DT_NULL
#define DT_NULL 0
#endif
#ifndef DT_DEBUG
#define DT_DEBUG 21
#endif
#ifndef ELFCLASS64
#define ELFCLASS64 2
#endif

IgFacts g_facts;
unsigned g_ctor_events;
unsigned g_fini_events;

/* ------------------------------------------------------------------ */
/* PROVEN segment table (worker-vaddr-semantics SEGMENT-CORRESPONDENCE
   §2; capture decrypted-eboot.bin sha256 3b6cfad4... re-verified
   2026-10-04 by this worker before embedding). prot bits: 1 R, 2 W, 4 X.
   anchor = first 16 file-backed bytes at p_offset (this run's dd);
   anchor_strong=0 marks padding (weak, e.g. L0's 0xCC page). */
typedef struct IgSegSpec {
    int ph;
    uint64_t vaddr, memsz, filesz, rounded;
    uint32_t prot;
    uint8_t anchor[16];
    int anchor_strong;
} IgSegSpec;

static const IgSegSpec IG_SEGS[5] = {
    {0, 0x0ull, 0x7479b1cull, 0x7479b1cull, 0x747c000ull, 4u,
     {0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}, 0},
    {1, 0x747c000ull, 0xa3e918ull, 0xa3e918ull, 0xa40000ull, 1u,
     {0x01,0x1b,0x03,0x3b,0xd4,0xaa,0x01,0x00,0xf4,0x33,0x00,0x00,0x80,0x40,0xb8,0xf8}, 1},
    {3, 0x7ebc000ull, 0x1280000ull, 0x127e4e8ull, 0x1280000ull, 3u,
     {0x01,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x4f,0x6b,0x00,0x00}, 1},
    {7, 0x913c000ull, 0x4f7418ull, 0x1cc5f2ull, 0x4f8000ull, 3u,
     {0x31,0x32,0x33,0x34,0x18,0x00,0x00,0x00,0xd8,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 1},
    {8, 0x9633420ull, 0x262cea8ull, 0x262cea8ull, 0x2630000ull, 1u,
     {0x00,0x62,0x7a,0x51,0x45,0x78,0x79,0x31,0x38,0x39,0x5a,0x49,0x23,0x36,0x23,0x34}, 1},
};
#define IG_SEG_L0 0
#define IG_SEG_L1 1
#define IG_SEG_L3 2
#define IG_SEG_L7 3
#define IG_SEG_L8 4

#define IG_L0_PROVEN_BASE 0x400000ull
#define IG_GUA_L7_DELTA   0x27e110ull   /* 0x93ba110 - L7.p_vaddr (BSS) */
#define IG_GAP_LO 0x787c000ull
#define IG_GAP_HI 0x8018c000ull
#define IG_DYN_VADDR 0xbc5f278ull       /* PT_DYNAMIC inside L8 */
#define IG_GRID_STEP 0x100000ull        /* 1 MiB grid over gap spans */
#define IG_GRID_MAX 2048u

/* Survey-recorded prologue bytes (worker-vaddr-semantics REPORT §2,
   re-read from the capture this run): Tick@file 0x2fb08c0 and
   SCO@file 0x494aa0 both start with this 16-byte x86-64 frame setup. */
static const uint8_t IG_ANCHOR_PROLOGUE[16] =
    {0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec};
#define IG_RUNTIME_TICK (IG_L0_PROVEN_BASE + IG_VADDR_UGAMEENGINE_TICK)
#define IG_RUNTIME_SCO  (IG_L0_PROVEN_BASE + IG_VADDR_STATIC_CONSTRUCT_OBJECT)

/* Prebuilt readable-span list (built once after the walk). */
static IgRMemSpan s_rmem[IG_SPAN_MAX];
static uint32_t s_rmem_n;

/* Segment-candidate matches (V2 working state, emitted in records). */
typedef struct IgSegCand {
    uint32_t span_idx;
    uint8_t ph;
    uint8_t size_match, prot_compat, anchor;  /* anchor: 0 no 1 yes 2 refused */
} IgSegCand;
#define IG_SEGC_MAX 64
static IgSegCand s_segc[IG_SEGC_MAX];
static uint32_t s_segc_n, s_segc_trunc;
static uint64_t s_l8_rt;        /* runtime L8 mapped start if derived */
static uint64_t s_dyn_rt;      /* runtime _DYNAMIC if derived */

/* Global anchor evidence (depth extension): computed in tier_c even when
   ZERO GUA candidates exist, so the L0 correspondence evidence (Tick/SCO
   byte shapes at the proven runtime addresses) is recorded in EVERY run. */
static uint8_t s_tick_anchor, s_sco_anchor;   /* 1 match, 0 mismatch, 0xff refused */
static uint8_t s_tick_prologue_like, s_sco_prologue_like;
static int s_tick_verdict, s_sco_verdict;
static unsigned s_tick_code0, s_sco_code0;
static int s_anchors_computed;

/* Bias-predicted segment anchor checks (depth extension, correction #2):
   under the single-load-bias hypothesis every LOAD is expected at
   0x400000 + p_vaddr; anchor evidence per segment is recorded for ALL five
   specs so bias coherence is checkable per segment, not just for L7. */
typedef struct IgBiasAnchor {
    uint8_t ph;
    uint8_t result;      /* 1 match, 0 mismatch, 0xff read refused */
    uint8_t strong;      /* 1 = strong anchor (0 = padding; mismatch is NOT
                            disproof for weak anchors, recorded only) */
    uint64_t addr;       /* 0x400000 + p_vaddr (the bias-predicted base) */
} IgBiasAnchor;
#define IG_BIAS_ANCHOR_MAX 5
static IgBiasAnchor s_bias_anchor[IG_BIAS_ANCHOR_MAX];
static uint32_t s_bias_anchor_n;

/* ------------------------------------------------------------------ */
/* Readability validation (v2 discipline) + the labeled anchor exception. */
int ig_range_readable(uint64_t addr, uint64_t len) {
    if (len == 0) return 1;
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        if ((s->prot & 1u) && addr >= s->start &&
            addr - s->start < s->end - s->start &&
            s->end - addr >= len)
            return 1;
    }
    return 0;
}

size_t ig_copy_checked(uint64_t addr, void *dst, size_t len) {
    if (!ig_range_readable(addr, len)) return 0;
    ig_memcpy(dst, (const void *)(uintptr_t)addr, len);
    return len;
}

/* LABELED EXCEPTION (see header comment): mapped-but-exec-only pages are
   accepted for FIXED, <=64-byte header/anchor reads only (ELF ehdr is 64
   bytes; all other exception uses are <=16). */
int ig_range_mapped_readable(uint64_t addr, uint64_t len) {
    if (len == 0) return 1;
    if (len > 64) return 0;
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        if ((s->prot & 5u) && addr >= s->start &&
            addr - s->start < s->end - s->start &&
            s->end - addr >= len)
            return 1;
    }
    return 0;
}

static size_t copy_mapped(uint64_t addr, void *dst, size_t len) {
    if (!ig_range_mapped_readable(addr, len)) return 0;
    ig_memcpy(dst, (const void *)(uintptr_t)addr, len);
    return len;
}

/* 16-byte anchor compare: 1 match, 0 mismatch, 0xff read refused. */
static uint8_t anchor_check(uint64_t addr, const uint8_t *expect) {
    uint8_t got[16];
    if (copy_mapped(addr, got, 16) != 16) return 0xff;
    for (int i = 0; i < 16; ++i) if (got[i] != expect[i]) return 0;
    return 1;
}

static void record_out(IgBuf *b, int *seq, const char *exp,
                       int64_t rc, int64_t eno) {
    ig_record_begin(b, ++(*seq), exp, rc, eno, IG_BUILD_ID);
}

static void emit(IgSink *sink, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(sink, b) != 0) sink->failed = 1;
}

/* ------------------------------------------------------------------ */
/* Early collection: KERN_PROC_VMMAP bounded to 4 MiB, walked with the
   v3 kve_structsize stride (mode 1), fixed-stride fallback only if the
   primary walk fails the trust gate. No trustworthy walk => span_count 0
   => EVERY foreign deref refused for the rest of the run (needs-map). */
static uint8_t g_vmmap_buf[IG_VMMAP_MAX] __attribute__((aligned(16)));

static void build_rmem(void) {
    s_rmem_n = 0;
    for (uint32_t i = 0; i < g_facts.span_count && s_rmem_n < IG_SPAN_MAX; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        if (!(s->prot & 1u)) continue;
        if (s->end <= s->start) continue;
        s_rmem[s_rmem_n].start = s->start;
        s_rmem[s_rmem_n].size = s->end - s->start;
        ++s_rmem_n;
    }
}

void ig_collect_vmmap(void) {
    int mib_pid[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid()};
    size_t len = 65536;
    int rc = -1;
    unsigned attempts = 0;
    for (;;) {
        ++attempts;
        rc = sysctl(mib_pid, 4, g_vmmap_buf, &len, 0, 0);
        if (rc == 0) break;
        int eno = ig_errno();
        if (eno == ENOMEM && len > 0 && len * 2 <= IG_VMMAP_MAX) {
            len *= 2;
            continue;
        }
        break;  /* non-ENOMEM failure or the 4 MiB growth cap */
    }
    g_facts.vmmap_rc = rc;
    g_facts.vmmap_errno = ig_errno();
    g_facts.vmmap_buf = (uint64_t)len;
    g_facts.vmmap_attempts = attempts;
    g_facts.vmmap_kve_size = (uint64_t)sizeof(struct kinfo_vmentry);
    g_facts.vmmap_first_ksz = 0;
    g_facts.vmmap_mod_ok = 0;
    if (rc == 0 && len >= 8) {
        /* worker-span-arbitration assertion: the kernel packs records at
           each record's own kve_structsize (expected 144, path stripped);
           buffer_length must divide exactly by it. Both observed v1/v2
           buffers do: 1442448=10017x144, 1452672=10088x144. A modulo
           failure REFUSES trust (needs-map), never guessed strides. */
        uint32_t ks0 = (uint32_t)g_vmmap_buf[0] |
                       ((uint32_t)g_vmmap_buf[1] << 8) |
                       ((uint32_t)g_vmmap_buf[2] << 16) |
                       ((uint32_t)g_vmmap_buf[3] << 24);
        g_facts.vmmap_first_ksz = ks0;
        g_facts.vmmap_mod_ok =
            (ks0 >= IG_VM_HDR && ks0 <= 1160 && (ks0 & 3u) == 0 &&
             (len % ks0) == 0) ? 1 : 0;
    }
    g_facts.span_count = 0;
    g_facts.span_truncated = 0;
    g_facts.vmmap_entries = 0;
    ig_memset(&g_facts.vm_walk, 0, sizeof g_facts.vm_walk);
    ig_memset(&g_facts.vm_walk_fb, 0, sizeof g_facts.vm_walk_fb);
    g_facts.vm_fallback_used = 0;
    g_facts.vm_trust = 0;
    if (rc == 0) {
        uint32_t ks = (uint32_t)sizeof(struct kinfo_vmentry);
        g_facts.vmmap_entries = (uint64_t)(len / ks);
        uint32_t kept = ig_vmmap_walk(g_vmmap_buf, (uint64_t)len, ks, 1,
                                      g_facts.spans, IG_SPAN_MAX,
                                      &g_facts.vm_walk, g_facts.vm_raw,
                                      IG_RAW_HDRS);
        g_facts.span_count = kept;
        g_facts.span_truncated = g_facts.vm_walk.truncated;
        g_facts.vm_trust = (ig_vmmap_trustworthy(&g_facts.vm_walk) &&
                            g_facts.vmmap_mod_ok) ? 1 : 0;
        if (!g_facts.vm_trust) {
            g_facts.vm_fallback_used = 1;
            uint32_t k2 = ig_vmmap_walk(g_vmmap_buf, (uint64_t)len, ks, 2,
                                        g_facts.spans, IG_SPAN_MAX,
                                        &g_facts.vm_walk_fb, 0, 0);
            if (ig_vmmap_trustworthy(&g_facts.vm_walk_fb) &&
                g_facts.vmmap_mod_ok) {
                g_facts.span_count = k2;
                g_facts.span_truncated = g_facts.vm_walk_fb.truncated;
                g_facts.vm_trust = 1;
            } else {
                g_facts.span_count = 0;   /* no containment basis at all */
            }
        }
    }
    build_rmem();
}

/* ------------------------------------------------------------------ */
/* V1: complete mapping report. TOTAL/RETAINED/TRUNCATED explicit
   (controller amendment 3); streamed per-entry list + raw headers. */
static void v1_vmmap_page(IgSink *sink, IgBuf *b, int *seq,
                          uint32_t part, uint32_t parts) {
    record_out(b, seq, "v1_vmmap_page", 0, 0);
    ig_kv_int(b, "part", (int64_t)part);
    ig_kv_int(b, "parts", (int64_t)parts);
    ig_arr_open(b, "entries");
    uint32_t from = part * IG_VM_PER_PART;
    uint32_t to = from + IG_VM_PER_PART;
    if (to > g_facts.span_count) to = g_facts.span_count;
    for (uint32_t i = from; i < to; ++i) {
        if (i != from) ig_buf_ch(b, ',');
        ig_obj_open(b);
        ig_kv_hex(b, "start", g_facts.spans[i].start);
        ig_kv_hex(b, "end", g_facts.spans[i].end);
        ig_kv_int(b, "prot", (int64_t)g_facts.spans[i].prot);
        ig_obj_close(b);
        if (b->truncated) break;
    }
    ig_arr_close(b);
    emit(sink, b);
}

static void v1_vmmap_raw(IgSink *sink, IgBuf *b, int *seq,
                         uint32_t chunk, uint32_t chunks) {
    record_out(b, seq, "v1_vmmap_raw", 0, 0);
    ig_kv_int(b, "chunk", (int64_t)chunk);
    ig_kv_int(b, "chunks", (int64_t)chunks);
    ig_kv_str(b, "note",
              "raw first-136-byte kinfo_vmentry headers (u64 words) for "
              "offline layout re-derivation", 96);
    ig_arr_open(b, "hdrs");
    for (uint32_t r = chunk * 2; r < chunk * 2 + 2 &&
                                r < g_facts.vm_walk.raws_kept; ++r) {
        if (r != chunk * 2) ig_buf_ch(b, ',');
        ig_obj_open(b);
        ig_kv_int(b, "rec", (int64_t)r);
        ig_arr_open(b, "w");
        const uint8_t *h = g_facts.vm_raw + (size_t)r * IG_VM_HDR;
        for (uint32_t w = 0; w < 17; ++w) {
            if (w != 0) ig_buf_ch(b, ',');
            uint64_t v = (uint64_t)h[w * 8] |
                         ((uint64_t)h[w * 8 + 1] << 8) |
                         ((uint64_t)h[w * 8 + 2] << 16) |
                         ((uint64_t)h[w * 8 + 3] << 24) |
                         ((uint64_t)h[w * 8 + 4] << 32) |
                         ((uint64_t)h[w * 8 + 5] << 40) |
                         ((uint64_t)h[w * 8 + 6] << 48) |
                         ((uint64_t)h[w * 8 + 7] << 56);
            ig_buf_ch(b, '"');
            ig_buf_u64_hex(b, v);
            ig_buf_ch(b, '"');
            if (b->truncated) break;
        }
        ig_arr_close(b);
        ig_obj_close(b);
        if (b->truncated) break;
    }
    ig_arr_close(b);
    emit(sink, b);
}

static void put_prot_histogram(IgBuf *b) {
    uint32_t dv[8]; uint32_t dc[8]; int dn = 0;
    for (uint32_t i = 0; i < g_facts.span_count && dn < 8; ++i) {
        uint32_t p = g_facts.spans[i].prot;
        int f = -1;
        for (int k = 0; k < dn; ++k) if (dv[k] == p) { f = k; break; }
        if (f < 0 && dn < 8) { dv[dn] = p; dc[dn] = 1; ++dn; }
        else if (f >= 0) ++dc[f];
    }
    ig_arr_open(b, "prot_histogram");
    for (int k = 0; k < dn; ++k) {
        if (k != 0) ig_buf_ch(b, ',');
        ig_obj_open(b);
        ig_kv_hex(b, "prot", (uint64_t)dv[k]);
        ig_kv_int(b, "count", (int64_t)dc[k]);
        ig_obj_close(b);
    }
    ig_arr_close(b);
}

static void v1_vmmap_full(IgSink *sink, IgBuf *b, int *seq) {
    const IgVmStats *w = &g_facts.vm_walk;
    int64_t frc = (int64_t)g_facts.vmmap_rc;
    if (g_facts.vmmap_rc != 0 && g_facts.vmmap_errno == ENOMEM) frc = -3;
    uint32_t parts = (g_facts.span_count + IG_VM_PER_PART - 1) /
                     IG_VM_PER_PART;
    uint32_t rawchunks = (g_facts.vm_walk.raws_kept + 1) / 2;
    record_out(b, seq, "v1_vmmap_full", frc, (int64_t)g_facts.vmmap_errno);
    ig_kv_int(b, "final_buf", (int64_t)g_facts.vmmap_buf);
    ig_kv_int(b, "kve_size", (int64_t)g_facts.vmmap_kve_size);
    ig_kv_int(b, "entries", (int64_t)g_facts.vmmap_entries);
    ig_kv_int(b, "attempts", (int64_t)g_facts.vmmap_attempts);
    /* amendment 3: TOTAL / RETAINED / TRUNCATED, explicit. */
    ig_kv_int(b, "total_records", (int64_t)w->total);
    ig_kv_int(b, "retained", (int64_t)g_facts.span_count);
    ig_kv_int(b, "truncated", (int64_t)g_facts.span_truncated);
    ig_kv_int(b, "insane_records", (int64_t)w->insane);
    ig_kv_int(b, "walk_mode", (int64_t)w->walk_mode);
    ig_kv_int(b, "stop_reason", (int64_t)w->stop_reason);
    ig_kv_hex(b, "stop_off", w->stop_off);
    ig_kv_int(b, "ksz_full_stride_records", (int64_t)w->ksz_full);
    ig_kv_int(b, "ksz_variable_records", (int64_t)w->ksz_var);
    ig_kv_int(b, "ksz_144_records", (int64_t)w->ksz_144);
    ig_kv_int(b, "first_record_structsize", (int64_t)g_facts.vmmap_first_ksz);
    ig_kv_int(b, "buffer_mod_exact", g_facts.vmmap_mod_ok);
    ig_kv_str(b, "arbitration_note",
              "span-arbitration: kernel records expected at 144B (path "
              "stripped); v1/v2 buffers divide exactly (10017x144, "
              "10088x144); mod failure refuses trust", 160);
    ig_kv_int(b, "aligned16k_records", (int64_t)w->aligned16k);
    ig_kv_int(b, "raws_kept", (int64_t)w->raws_kept);
    ig_kv_int(b, "trust", g_facts.vm_trust);
    ig_kv_int(b, "fallback_used", g_facts.vm_fallback_used);
    if (g_facts.vm_fallback_used) {
        ig_kv_int(b, "fb_total", (int64_t)g_facts.vm_walk_fb.total);
        ig_kv_int(b, "fb_retained", (int64_t)g_facts.vm_walk_fb.retained);
        ig_kv_int(b, "fb_insane", (int64_t)g_facts.vm_walk_fb.insane);
        ig_kv_int(b, "fb_trust",
                  ig_vmmap_trustworthy(&g_facts.vm_walk_fb));
    }
    ig_kv_int(b, "parts", (int64_t)parts);
    ig_kv_int(b, "entries_per_part", IG_VM_PER_PART);
    ig_kv_int(b, "raw_chunks", (int64_t)rawchunks);
    ig_kv_str(b, "map_note",
              "v1/v2 logs are NOT a reliable map: fixed-stride walk + 1024 "
              "cap corrupted the page records; this list is complete for "
              "the retained set with totals above", 160);
    ig_kv_str(b, "walk_note",
              "stride=kve_structsize per record (SDK struct kinfo_vmentry "
              "header field 0); fallback=fixed sizeof stride (v2 behavior)",
              128);
    uint64_t r = 0, wr = 0, x = 0;
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        uint32_t p = g_facts.spans[i].prot;
        if (p & 1u) ++r;
        if (p & 2u) ++wr;
        if (p & 4u) ++x;
    }
    ig_kv_int(b, "spans_r", (int64_t)r);
    ig_kv_int(b, "spans_w", (int64_t)wr);
    ig_kv_int(b, "spans_x", (int64_t)x);
    put_prot_histogram(b);
    emit(sink, b);
    for (uint32_t p = 0; p < parts; ++p) v1_vmmap_page(sink, b, seq, p, parts);
    for (uint32_t c = 0; c < rawchunks; ++c) v1_vmmap_raw(sink, b, seq, c, rawchunks);
}

/* ------------------------------------------------------------------ */
/* ELF helpers (vendored v2 logic + PT_LOAD segment capture). */
static int dyn_walk(uint64_t dyn_addr, uint64_t filesz, int *entries_out,
                    int *dt_null_seen, int *dt_debug_present,
                    uint64_t *dt_debug_value) {
    unsigned steps = 256;
    if (filesz != 0) {
        uint64_t s = filesz / sizeof(Elf64_Dyn);
        if (s < steps) steps = (unsigned)s;
        if (steps == 0) steps = 1;
    }
    int scanned = 0, sane = 0, nul = 0, dbg = 0;
    uint64_t dbgval = 0;
    for (unsigned i = 0; i < steps; ++i) {
        Elf64_Dyn d;
        if (ig_copy_checked(dyn_addr + (uint64_t)i * sizeof d, &d,
                            sizeof d) != sizeof d)
            break;
        uint64_t tag = (uint64_t)d.d_tag;
        ++scanned;
        int tag_sane = (tag <= 0x40) ||
                       (tag >= 0x60000000ULL && tag < 0x80000000ULL);
        if (tag_sane) ++sane;
        if (tag == DT_NULL) { nul = 1; break; }
        if (tag == DT_DEBUG) { dbg = 1; dbgval = (uint64_t)d.d_un.d_ptr; }
    }
    if (entries_out) *entries_out = scanned;
    if (dt_null_seen) *dt_null_seen = nul;
    if (dt_debug_present) *dt_debug_present = dbg;
    if (dt_debug_value) *dt_debug_value = dbgval;
    return scanned > 0 && (nul || sane == scanned);
}

static int phdr_table_read(uint64_t base, const Elf64_Ehdr *eh,
                           IgCand *c) {
    c->dyn_found = 0; c->extent = 0; c->has_exec_segment = 0;
    c->dyn_vaddr = 0; c->dyn_filesz = 0; c->seg_count = 0; c->seg_gua = -1;
    for (unsigned p = 0; p < eh->e_phnum; ++p) {
        Elf64_Phdr ph;
        if (ig_copy_checked(base + eh->e_phoff +
                                (uint64_t)p * eh->e_phentsize,
                            &ph, sizeof ph) != sizeof ph)
            return 0;
        if (ph.p_type == PT_DYNAMIC) {
            c->dyn_vaddr = (uint64_t)ph.p_vaddr;
            c->dyn_filesz = (uint64_t)ph.p_filesz;
            c->dyn_found = 1;
        } else if (ph.p_type == PT_LOAD) {
            if (ph.p_flags & PF_X) c->has_exec_segment = 1;
            uint64_t e2 = (uint64_t)ph.p_vaddr + (uint64_t)ph.p_memsz;
            if (e2 > c->extent) c->extent = e2;
            if (c->seg_count < 6) {
                c->segs[c->seg_count].vaddr = (uint64_t)ph.p_vaddr;
                c->segs[c->seg_count].memsz = (uint64_t)ph.p_memsz;
                if ((uint64_t)ph.p_vaddr <= IG_VADDR_GUOBJECT_ARRAY &&
                    IG_VADDR_GUOBJECT_ARRAY < e2)
                    c->seg_gua = c->seg_count;
                ++c->seg_count;
            }
        }
    }
    return 1;
}

/* Parse an ELF image at base (tier-a magic hit). Candidate is recorded
   with full evidence even when parsing fails; returns 1 if magic present. */
static int parse_image(uint64_t base, const IgSpan *s, uint32_t span_idx) {
    if (g_facts.cand_count >= IG_CAND_MAX) {
        g_facts.cand_truncated = 1;
        return 0;
    }
    IgCand *c = &g_facts.cands[g_facts.cand_count];
    ig_memset(c, 0, sizeof *c);
    c->base = base;
    c->span_start = s->start;
    c->span_end = s->end;
    c->prot = s->prot;
    c->seg_gua = -1;
    Elf64_Ehdr eh;
    ig_memset(&eh, 0, sizeof eh);
    if (copy_mapped(base, &eh, sizeof eh) != sizeof eh ||
        eh.e_ident[0] != 0x7f || eh.e_ident[1] != 'E' ||
        eh.e_ident[2] != 'L' || eh.e_ident[3] != 'F') {
        /* no magic here: not an image candidate; do not keep the slot */
        return 0;
    }
    c->magic_ok = 1;
    c->class_ok = (eh.e_ident[4] == ELFCLASS64);
    c->e_type = eh.e_type;
    c->phnum = eh.e_phnum;
    if (c->class_ok && eh.e_phentsize >= sizeof(Elf64_Phdr) &&
        eh.e_phnum >= 1 && eh.e_phnum <= 128) {
        c->phdr_ok = phdr_table_read(base, &eh, c);
        if (c->phdr_ok && c->dyn_found) {
            int e1 = 0, n1 = 0, d1p = 0; uint64_t d1v = 0;
            int ok1 = dyn_walk(c->base + c->dyn_vaddr, c->dyn_filesz,
                               &e1, &n1, &d1p, &d1v);
            int e2n = 0, n2 = 0, d2p = 0; uint64_t d2v = 0;
            int ok2 = 0;
            if (c->dyn_vaddr != c->base + c->dyn_vaddr)
                ok2 = dyn_walk(c->dyn_vaddr, c->dyn_filesz,
                               &e2n, &n2, &d2p, &d2v);
            int score1 = (ok1 ? 2 : 0) + n1;
            int score2 = (ok2 ? 2 : 0) + n2;
            if (ok1 && score1 >= score2) {
                c->dyn_mode = 1; c->dyn_entries = e1;
                c->dt_null_seen = n1; c->dyn_parses = 1;
            } else if (ok2) {
                c->dyn_mode = 2; c->dyn_entries = e2n;
                c->dt_null_seen = n2; c->dyn_parses = 1;
            }
        }
    }
    ++g_facts.cand_count;
    (void)span_idx;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Tier a: magic probes. EVERY span start (any prot; mapped-read exception
   covers exec-only spans); >64 MiB EXEC/READ spans get a bounded
   page-aligned 2 MiB grid. */
static int probe_magic4(uint64_t addr) {
    uint8_t m[4];
    if (copy_mapped(addr, m, 4) != 4) return -1;
    return (m[0] == 0x7f && m[1] == 'E' && m[2] == 'L' && m[3] == 'F') ? 1 : 0;
}

static void add_magic(uint64_t addr, uint32_t span_idx, uint32_t prot,
                      int sweep) {
    if (g_facts.magic_count >= IG_MAGIC_MAX) {
        g_facts.magic_trunc = 1;
        return;
    }
    IgMagicHit *h = &g_facts.magic[g_facts.magic_count++];
    h->addr = addr;
    h->span_idx = span_idx;
    h->prot = prot;
    h->sweep = (uint32_t)sweep;
}

static void tier_a_magic(void) {
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        int r = probe_magic4(s->start);
        if (r < 0) ++g_facts.spanstart_refused;
        else if (r == 1) add_magic(s->start, i, s->prot, 0);
        ++g_facts.spanstart_probes;
    }
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        uint64_t size = s->end - s->start;
        if (size <= (64ull << 20)) continue;
        if (!((s->prot & 1u) || (s->prot & 4u))) continue;
        if (g_facts.sweep_count >= IG_SWEEP_MAX) { g_facts.sweep_trunc = 1; continue; }
        IgSweep *sw = &g_facts.sweeps[g_facts.sweep_count++];
        sw->span_idx = i; sw->start = s->start; sw->end = s->end;
        sw->prot = s->prot;
        uint64_t a = (s->start + 0x1fffffull) & ~0x1fffffull;
        for (; a + 4 <= s->end && sw->probes < 256; a += 0x200000ull) {
            ++sw->probes;
            int r = probe_magic4(a);
            if (r < 0) ++sw->refused;
            else if (r == 1) { ++sw->hits; add_magic(a, i, s->prot, 1); }
        }
        if (a + 4 <= s->end) sw->truncated = 1;
        g_facts.grid_probes += sw->probes;
        g_facts.grid_refused += sw->refused;
    }
    /* Parse an ELF image at every magic hit (bounded). */
    for (uint32_t m = 0; m < g_facts.magic_count; ++m) {
        uint32_t si = g_facts.magic[m].span_idx;
        if (si >= g_facts.span_count) continue;
        parse_image(g_facts.magic[m].addr, &g_facts.spans[si], si);
    }
    /* v2-lineage chosen image: largest parsed executable image. */
    int best = -1;
    for (uint32_t i = 0; i < g_facts.cand_count; ++i) {
        IgCand *c = &g_facts.cands[i];
        if (!(c->magic_ok && c->class_ok && c->phdr_ok && c->dyn_parses &&
              c->has_exec_segment && c->extent != 0))
            continue;
        if (best < 0 || c->extent > g_facts.cands[best].extent) best = (int)i;
    }
    g_facts.chosen_index = best;
    if (best >= 0) {
        IgCand *c = &g_facts.cands[best];
        c->chosen = 1;
        g_facts.chosen_base = c->base;
        g_facts.chosen_extent = c->extent;
        g_facts.chosen_dyn_mode = c->dyn_mode;
        g_facts.chosen_dyn_addr = (c->dyn_mode == 2)
            ? c->dyn_vaddr : c->base + c->dyn_vaddr;
    }
}

/* ------------------------------------------------------------------ */
/* Segment-table candidate matching (worker-vaddr-semantics §2/§5). */
static void tier_a_bias_anchors(void) {
    s_bias_anchor_n = 0;
    for (int k = 0; k < 5 && s_bias_anchor_n < IG_BIAS_ANCHOR_MAX; ++k) {
        const IgSegSpec *sp = &IG_SEGS[k];
        IgBiasAnchor *ba = &s_bias_anchor[s_bias_anchor_n++];
        ba->ph = (uint8_t)sp->ph;
        ba->addr = IG_L0_PROVEN_BASE + sp->vaddr;
        ba->strong = (uint8_t)(sp->anchor_strong ? 1 : 0);
        /* The L0 spec IS the proven base itself (weak 0xCC padding anchor):
           its "check" records what the first 16 runtime bytes actually are;
           mismatch is expected to be possible and is NOT disproof. */
        ba->result = anchor_check(ba->addr, sp->anchor);
    }
}

static void tier_a_segcands(void) {
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        uint64_t size = s->end - s->start;
        for (int k = 0; k < 5; ++k) {
            const IgSegSpec *sp = &IG_SEGS[k];
            uint8_t size_match = (size == sp->rounded) ? 1 : 0;
            uint8_t prot_compat =
                ((s->prot & sp->prot) == sp->prot) ? 1 : 0;
            uint8_t anc = 2;
            if (sp->anchor_strong)
                anc = anchor_check(s->start, sp->anchor);
            if (!size_match && anc != 1) continue;
            if (s_segc_n >= IG_SEGC_MAX) { ++s_segc_trunc; continue; }
            IgSegCand *sc = &s_segc[s_segc_n++];
            sc->span_idx = i;
            sc->ph = (uint8_t)sp->ph;
            sc->size_match = size_match;
            sc->prot_compat = prot_compat;
            sc->anchor = anc;
            if (sp->ph == 8 && anchor_check(s->start, sp->anchor) == 1 &&
                size == sp->rounded) {
                /* L8 matched: derive the runtime _DYNAMIC (PT_DYNAMIC
                   vaddr 0xbc5f278 lives in L8). Shared-page caveat:
                   L7/L8 share vaddr page 0x9633000; the loader maps L8's
                   page-rounded start (expected S + 0x4f8000 contiguous). */
                s_l8_rt = s->start;
                s_dyn_rt = s->start + (IG_DYN_VADDR - sp->vaddr);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Tier b: GUA predicate-oracle trials (candidate DISCOVERY ONLY). */
static void gua_trial(uint64_t base, uint64_t probed, uint8_t hyp,
                      uint32_t span_idx, int32_t cand_idx, int force) {
    IgGuaOutC gua;
    ig_memset(&gua, 0, sizeof gua);
    ig_layout_guobject_prebuilt(s_rmem, s_rmem_n, base,
                                IG_VADDR_GUOBJECT_ARRAY, &gua);
    int want_record = force || gua.verdict == 1;
    int32_t rec_idx = -1;
    if (g_facts.trial_count < IG_TRIAL_MAX) {
        if (want_record || g_facts.trial_count < 24) {
            IgTrial *t = &g_facts.trials[g_facts.trial_count++];
            rec_idx = (int32_t)(g_facts.trial_count - 1);
            t->base = base;
            t->probed = probed;
            t->hyp = hyp;
            t->verdict = (uint8_t)gua.verdict;
            t->code0 = gua.code_count > 0 ? (uint16_t)gua.codes[0] : (uint16_t)0;
            t->layout_a_verdict = (uint8_t)gua.layout_a.verdict;
            t->layout_b_verdict = (uint8_t)gua.layout_b.verdict;
            t->layout_matched = (uint8_t)gua.layout_matched;
            t->cand_idx = cand_idx;
            t->span_idx = span_idx;
        } else {
            ++g_facts.trial_trunc;   /* unremarkable trial, record slots kept */
        }
    } else {
        ++g_facts.trial_trunc;
    }
    if (gua.verdict == 1 && g_facts.match_count < IG_MATCH_MAX) {
        IgMatch *mm = &g_facts.matches[g_facts.match_count++];
        ig_memset(mm, 0, sizeof *mm);
        mm->trial_idx = rec_idx;
        mm->base = base;
        mm->probed = probed;
        mm->hyp = hyp;
        mm->objects_ptr = gua.objects_ptr;
        mm->num_elements = gua.num_elements;
        mm->num_chunks = gua.num_chunks;
        mm->layout_matched = (uint8_t)gua.layout_matched;
        mm->rfl_struct_verdict = -1;
        mm->rfl_fld_verdict = -1;
        mm->rfl_fn_verdict = -1;
        mm->segc_verdict = 0;
        mm->v3_rc = -1;
    } else if (gua.verdict == 1) {
        ++g_facts.match_count;  /* count overflow; list capped at 8 */
    }
}

/* L7-consistency check for a GUA probed address — BOTH co-official
   translation hypotheses are tested and recorded SEPARATELY (controller
   correction #2: the single-load-bias hypothesis stays OPEN; arbitration
   requires L7's own correspondence evidence, never the GUA predicate).
   Interpretation FREE: implied L7 base S_free = probed - 0x27e110 (L7
   mapped wherever it is); pass iff the span at S_free matches L7
   extent/anchor. Interpretation BIAS: L7 expected at the proven L0 base +
   L7.p_vaddr and the probe at base + GUA vaddr; pass iff probed matches
   that prediction AND the span at the expected L7 base matches L7
   extent/anchor. Returns bit1|bit2; win_start/win_end carry the
   free-interpretation matching span (the primary hunt target). */
static uint8_t seg_consistency(uint64_t probed, uint64_t *win_start,
                               uint64_t *win_end, uint64_t *bias_ws,
                               uint64_t *bias_we) {
    const IgSegSpec *l7 = &IG_SEGS[IG_SEG_L7];
    uint8_t pass = 0;
    uint64_t s_free = probed - IG_GUA_L7_DELTA;
    uint64_t s_bias = IG_L0_PROVEN_BASE + l7->vaddr;
    uint64_t probed_bias = IG_L0_PROVEN_BASE + IG_VADDR_GUOBJECT_ARRAY;
    /* FREE interpretation: span at the implied L7 base. */
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        if (s_free < s->start || s_free >= s->end) continue;
        uint8_t sz = ((s->end - s->start) == l7->rounded) ? 1 : 0;
        uint8_t an = anchor_check(s_free, l7->anchor);
        if (sz || an == 1) {
            pass |= 1u;
            *win_start = s->start;
            *win_end = s->end;
        }
        break;
    }
    /* BIAS interpretation: only applicable when the probe lands exactly at
       the bias-predicted address; then the span at the bias-predicted L7
       base must match L7's own extent/anchor (never assumed from the pass
       of the GUA predicate alone). */
    if (probed == probed_bias) {
        for (uint32_t i = 0; i < g_facts.span_count; ++i) {
            const IgSpan *s = &g_facts.spans[i];
            if (s_bias < s->start || s_bias >= s->end) continue;
            uint8_t sz = ((s->end - s->start) == l7->rounded) ? 1 : 0;
            uint8_t an = anchor_check(s_bias, l7->anchor);
            if (sz || an == 1) {
                pass |= 2u;
                *bias_ws = s->start;
                *bias_we = s->end;
            }
            break;
        }
    }
    return pass;
}

static void tier_b_trials(void) {
    /* h1: every span start as base (B + vaddr); covers the file-offset
       semantics too (same arithmetic). */
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        uint64_t B = g_facts.spans[i].start;
        uint64_t probed = B + IG_VADDR_GUOBJECT_ARRAY;
        if (probed < B) continue;
        if (ig_range_readable(probed, 8))
            gua_trial(B, probed, 1, i, -1, 0);
        else
            ++g_facts.h1_skipped;
    }
    /* h2: segment-translated probes for every parsed tier-a image whose
       PT_LOAD covers the GUA vaddr. */
    for (uint32_t c = 0; c < g_facts.cand_count; ++c) {
        const IgCand *cd = &g_facts.cands[c];
        if (cd->seg_gua < 0) continue;
        uint64_t delta = IG_VADDR_GUOBJECT_ARRAY -
                         cd->segs[cd->seg_gua].vaddr;
        for (uint32_t i = 0; i < g_facts.span_count; ++i) {
            const IgSpan *s = &g_facts.spans[i];
            if (!(s->prot & 1u)) continue;
            if (s->end - s->start <= delta) continue;
            uint64_t probed = s->start + delta;
            if (!ig_range_readable(probed, 8)) { ++g_facts.h2_skipped; continue; }
            gua_trial(s->start - cd->segs[cd->seg_gua].vaddr, probed, 2,
                      i, (int32_t)c, 0);
        }
    }
    /* h3: absolute vaddr. */
    if (ig_range_readable(IG_VADDR_GUOBJECT_ARRAY, 8))
        gua_trial(0, IG_VADDR_GUOBJECT_ARRAY, 3, 0, -1, 1);

    /* h2s (PRIMARY, coordinator): find mapped_start(L7).
       (a) segment-table matches for L7; (b) RW spans >= L7 memsz;
       (c) contiguous-chain hypothesis after a matched L1;
       (d) bounded 1 MiB grid over gap-region spans (page-aligned). */
    const IgSegSpec *l7 = &IG_SEGS[IG_SEG_L7];
    const IgSegSpec *l1 = &IG_SEGS[IG_SEG_L1];
    for (uint32_t m = 0; m < s_segc_n; ++m) {
        if (s_segc[m].ph != 7) continue;
        uint32_t i = s_segc[m].span_idx;
        if (i >= g_facts.span_count) continue;
        uint64_t S = g_facts.spans[i].start;
        gua_trial(S - l7->vaddr, S + IG_GUA_L7_DELTA, 4, i, -1, 1);
    }
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        if ((s->prot & 3u) != 3u) continue;
        if (s->end - s->start < l7->memsz) continue;
        uint64_t S = s->start;
        uint64_t probed = S + IG_GUA_L7_DELTA;
        if (ig_range_readable(probed, 8))
            gua_trial(S - l7->vaddr, probed, 4, i, -1, 0);
    }
    for (uint32_t m = 0; m < s_segc_n; ++m) {
        if (s_segc[m].ph != 1) continue;
        uint32_t i = s_segc[m].span_idx;
        if (i >= g_facts.span_count) continue;
        const IgSpan *s1 = &g_facts.spans[i];
        /* contiguous chain: L1.start + L1.rounded = L3, + L3.rounded = L7 */
        uint64_t S7 = s1->start + l1->rounded +
                      IG_SEGS[IG_SEG_L3].rounded;
        uint64_t probed7 = S7 + IG_GUA_L7_DELTA;
        if (ig_range_readable(probed7, 8))
            gua_trial(S7 - l7->vaddr, probed7, 4, i, -1, 1);
        /* record the chained L8 placement for the V3 dynamic derivation
           (shared-page caveat: L8 expected at S7 + L7 rounded size) */
        if (s_segc[m].size_match) {
            uint64_t S8 = S7 + l7->rounded;
            uint64_t dyn8 = S8 + (IG_DYN_VADDR - IG_SEGS[IG_SEG_L8].vaddr);
            if (s_dyn_rt == 0 && ig_range_readable(dyn8, 8)) {
                s_l8_rt = S8;
                s_dyn_rt = dyn8;
            }
        }
    }
    {
        uint32_t grid = 0;
        for (uint32_t i = 0; i < g_facts.span_count && grid < IG_GRID_MAX; ++i) {
            const IgSpan *s = &g_facts.spans[i];
            uint64_t lo = s->start > IG_GAP_LO ? s->start : IG_GAP_LO;
            uint64_t hi = s->end < IG_GAP_HI ? s->end : IG_GAP_HI;
            if (!(s->prot & 1u) || hi <= lo) continue;
            uint64_t S = (lo + 0xfffffull) & ~0xfffffull;
            if (S < lo) S = lo;
            for (; S + IG_GUA_L7_DELTA + 8 <= hi && grid < IG_GRID_MAX;
                 S += IG_GRID_STEP) {
                uint64_t probed = S + IG_GUA_L7_DELTA;
                ++grid;
                if (ig_range_readable(probed, 8))
                    gua_trial(S - l7->vaddr, probed, 4, i, -1, 0);
            }
            /* end-aligned placement (16 KiB-aligned end of the span) */
            uint64_t Se = (hi - l7->rounded) & ~0x3fffull;
            if (Se > lo && grid < IG_GRID_MAX) {
                uint64_t probed = Se + IG_GUA_L7_DELTA;
                ++grid;
                if (ig_range_readable(probed, 8))
                    gua_trial(Se - l7->vaddr, probed, 4, i, -1, 0);
            }
        }
    }
    g_facts.ambiguous = (g_facts.match_count > 1) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Tier c: cross-checks + full predicate set per GUA-passing candidate. */
static size_t copy_name_checked(uint64_t addr, char *dst, size_t cap) {
    static const size_t tries[] = {96, 48, 16, 8, 4, 2, 1};
    for (unsigned t = 0; t < sizeof tries / sizeof tries[0]; ++t) {
        size_t want = tries[t];
        if (want > cap - 1) continue;
        if (ig_range_readable(addr, want)) {
            ig_memcpy(dst, (const void *)(uintptr_t)addr, want);
            dst[want] = '\0';
            size_t n = ig_strlen(dst);
            if (n < want) return n;
            return want;
        }
    }
    dst[0] = '\0';
    return 0;
}

static void tier_c_crosschecks(void) {
    /* Global anchors at the PROVEN L0 runtime addresses — stored in statics
       so v2_base_hunt can emit them even with ZERO matches (depth). */
    s_tick_anchor = anchor_check(IG_RUNTIME_TICK, IG_ANCHOR_PROLOGUE);
    s_sco_anchor = anchor_check(IG_RUNTIME_SCO, IG_ANCHOR_PROLOGUE);
    IgCodeOutC tick, sco;
    ig_memset(&tick, 0, sizeof tick);
    ig_memset(&sco, 0, sizeof sco);
    ig_layout_code_shape_prebuilt(s_rmem, s_rmem_n, IG_L0_PROVEN_BASE,
                                  IG_VADDR_UGAMEENGINE_TICK, &tick);
    ig_layout_code_shape_prebuilt(s_rmem, s_rmem_n, IG_L0_PROVEN_BASE,
                                  IG_VADDR_STATIC_CONSTRUCT_OBJECT, &sco);
    s_tick_verdict = tick.verdict;
    s_sco_verdict = sco.verdict;
    s_tick_code0 = tick.code_count > 0 ? tick.codes[0] : 0;
    s_sco_code0 = sco.code_count > 0 ? sco.codes[0] : 0;
    s_tick_prologue_like = 0;
    for (int i = 0; i < tick.code_count; ++i)
        if (tick.codes[i] == 0x2101u) s_tick_prologue_like = 1;
    s_sco_prologue_like = 0;
    for (int i = 0; i < sco.code_count; ++i)
        if (sco.codes[i] == 0x2101u) s_sco_prologue_like = 1;
    s_anchors_computed = 1;
    uint32_t listed = g_facts.match_count < IG_MATCH_MAX
                          ? g_facts.match_count : IG_MATCH_MAX;
    for (uint32_t m = 0; m < listed; ++m) {
        IgMatch *mm = &g_facts.matches[m];
        mm->tick_anchor = s_tick_anchor;
        mm->sco_anchor = s_sco_anchor;
        mm->tick_verdict = (uint8_t)s_tick_verdict;
        mm->tick_code0 = (uint16_t)s_tick_code0;
        mm->tick_prologue_like = s_tick_prologue_like;
        mm->sco_verdict = (uint8_t)s_sco_verdict;
        mm->sco_code0 = (uint16_t)s_sco_code0;
        mm->sco_prologue_like = s_sco_prologue_like;
        uint64_t ws = 0, we = 0, bws = 0, bwe = 0;
        mm->segc_free_l7_start = mm->probed - IG_GUA_L7_DELTA;
        mm->segc_bias_l7_start =
            IG_L0_PROVEN_BASE + IG_SEGS[IG_SEG_L7].vaddr;
        mm->segc_bias_probed_pred =
            IG_L0_PROVEN_BASE + IG_VADDR_GUOBJECT_ARRAY;
        mm->segc_bias_probed_match =
            (mm->probed == mm->segc_bias_probed_pred) ? 1u : 0u;
        mm->segc_verdict = seg_consistency(mm->probed, &ws, &we,
                                           &bws, &bwe);
        mm->segc_free_pass = (mm->segc_verdict & 1u) ? 1u : 0u;
        mm->segc_bias_pass = (mm->segc_verdict & 2u) ? 1u : 0u;
        mm->segc_min_start = ws;
        mm->segc_max_end = we;
        mm->segc_bias_span_start = bws;
        mm->segc_bias_span_end = bwe;
        mm->segc_extent = IG_SEGS[IG_SEG_L7].rounded;
        mm->promoted = (mm->tick_anchor == 1 && mm->sco_anchor == 1 &&
                        mm->segc_verdict != 0) ? 1 : 0;
        /* Full predicate set: rfl batch-2 via the ValidatedGuaView. */
        IgGuaViewC view;
        view.objects_ptr = mm->objects_ptr;
        view.num_elements = mm->num_elements;
        view.num_chunks = mm->num_chunks;
        IgRflStructOutC st;
        ig_memset(&st, 0, sizeof st);
        if (ig_rfl_struct_chain(s_rmem, s_rmem_n, &view, &st) == 0) {
            mm->rfl_struct_verdict = (int8_t)st.verdict;
            mm->rfl_items = st.items_examined;
            mm->rfl_plausible = st.plausible_uobject;
            mm->rfl_struct_like = st.struct_like_count;
            mm->rfl_uclass_like =
                (st.first_uclass_like != 0) ? 1u : 0u;
            mm->rfl_first_struct = st.first_ustruct_like;
            mm->rfl_first_uclass = st.first_uclass_like;
            if (st.first_ustruct_like != 0) {
                IgRflFieldOutC fld;
                ig_memset(&fld, 0, sizeof fld);
                if (ig_rfl_child_variant(s_rmem, s_rmem_n,
                                         st.first_ustruct_like, &view,
                                         &fld) == 0) {
                    mm->rfl_fld_verdict = (int8_t)fld.verdict;
                    mm->rfl_fld_code0 = (uint16_t)(fld.code_count > 0
                        ? fld.codes[0] : 0);
                    mm->rfl_fld_variant = (uint8_t)fld.variant;
                } else {
                    mm->rfl_fld_verdict = -2;
                }
            }
            if (st.first_uclass_like != 0) {
                IgRflFuncOutC fn;
                ig_memset(&fn, 0, sizeof fn);
                if (ig_rfl_ufunction(s_rmem, s_rmem_n,
                                     st.first_uclass_like, &fn) == 0) {
                    mm->rfl_fn_verdict = (int8_t)fn.verdict;
                    mm->rfl_fn_code0 = (uint16_t)(fn.code_count > 0
                        ? fn.codes[0] : 0);
                    mm->rfl_fn_func_ptr = fn.func_ptr;
                } else {
                    mm->rfl_fn_verdict = -2;
                }
            }
        } else {
            mm->rfl_struct_verdict = -2;
        }
        /* V3 dtdebug at this match: prefer the L8-derived runtime dynamic,
           else a tier-a parsed image's dynamic. */
        uint64_t dyn_addr = s_dyn_rt;
        if (dyn_addr == 0 && mm->trial_idx >= 0) {
            /* fall back to any parsed image dynamic (recorded provenance) */
            if (g_facts.chosen_dyn_addr != 0) dyn_addr = g_facts.chosen_dyn_addr;
        }
        if (dyn_addr == 0) {
            mm->v3_rc = -9;  /* no dynamic section address derivable */
        } else {
            int entries = 0, nul = 0, dpresent = 0;
            uint64_t dval = 0;
            int ok = dyn_walk(dyn_addr, 0, &entries, &nul, &dpresent, &dval);
            if (!ok) mm->v3_rc = -5;
            else if (!dpresent) mm->v3_rc = -6;
            else if (dval == 0) mm->v3_rc = -7;
            else {
                mm->v3_dt_present = 1;
                mm->v3_dt_value = dval;
                struct r_debug rd;
                ig_memset(&rd, 0, sizeof rd);
                if (ig_copy_checked(dval, &rd, sizeof rd) != sizeof rd ||
                    rd.r_map == 0) {
                    mm->v3_rc = -8;
                } else {
                    mm->v3_r_map = (uint64_t)(uintptr_t)rd.r_map;
                    mm->v3_rc = 0;
                    struct link_map *it = rd.r_map;
                    int emitted = 0, blocked = 0, trunc = 0;
                    uint64_t visited[IG_LINKMAP_MAX];
                    while (it != 0) {
                        if (emitted >= IG_LINKMAP_MAX) { trunc = 1; break; }
                        int seen = 0;
                        for (int q = 0; q < emitted; ++q)
                            if (visited[q] == (uint64_t)(uintptr_t)it) { seen = 1; break; }
                        if (seen) break;
                        struct link_map lm;
                        ig_memset(&lm, 0, sizeof lm);
                        if (ig_copy_checked((uint64_t)(uintptr_t)it, &lm,
                                            sizeof lm) != sizeof lm) {
                            ++blocked;
                            break;
                        }
                        visited[emitted++] = (uint64_t)(uintptr_t)it;
                        char nm[96];
                        size_t got = 0;
                        if (lm.l_name != 0)
                            got = copy_name_checked(
                                (uint64_t)(uintptr_t)lm.l_name, nm, sizeof nm);
                        else nm[0] = '\0';
                        if (lm.l_name != 0 && got == 0) ++blocked;
                        it = lm.l_next;
                    }
                    mm->v3_lm_entries = emitted;
                    mm->v3_lm_trunc = (uint8_t)trunc;
                    mm->v3_lm_blocked = (uint8_t)blocked;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* V2 record emitters. */
static void v2_magic_page(IgSink *sink, IgBuf *b, int *seq) {
    uint32_t parts = (g_facts.magic_count + 11) / 12;
    if (g_facts.magic_count == 0) parts = 0;
    for (uint32_t p = 0; p < parts; ++p) {
        record_out(b, seq, "v2_magic_page", 0, 0);
        ig_kv_int(b, "part", (int64_t)p);
        ig_kv_int(b, "parts", (int64_t)parts);
        ig_arr_open(b, "hits");
        uint32_t from = p * 12, to = from + 12;
        if (to > g_facts.magic_count) to = g_facts.magic_count;
        for (uint32_t i = from; i < to; ++i) {
            if (i != from) ig_buf_ch(b, ',');
            ig_obj_open(b);
            ig_kv_hex(b, "addr", g_facts.magic[i].addr);
            ig_kv_int(b, "span", (int64_t)g_facts.magic[i].span_idx);
            ig_kv_int(b, "prot", (int64_t)g_facts.magic[i].prot);
            ig_kv_int(b, "sweep", (int64_t)g_facts.magic[i].sweep);
            ig_obj_close(b);
            if (b->truncated) break;
        }
        ig_arr_close(b);
        emit(sink, b);
    }
}

static void v2_sweep_page(IgSink *sink, IgBuf *b, int *seq) {
    uint32_t parts = (g_facts.sweep_count + 5) / 6;
    if (g_facts.sweep_count == 0) parts = 0;
    for (uint32_t p = 0; p < parts; ++p) {
        record_out(b, seq, "v2_sweep_page", 0, 0);
        ig_kv_int(b, "part", (int64_t)p);
        ig_kv_int(b, "parts", (int64_t)parts);
        ig_arr_open(b, "sweeps");
        uint32_t from = p * 6, to = from + 6;
        if (to > g_facts.sweep_count) to = g_facts.sweep_count;
        for (uint32_t i = from; i < to; ++i) {
            const IgSweep *w = &g_facts.sweeps[i];
            if (i != from) ig_buf_ch(b, ',');
            ig_obj_open(b);
            ig_kv_int(b, "span", (int64_t)w->span_idx);
            ig_kv_hex(b, "start", w->start);
            ig_kv_hex(b, "end", w->end);
            ig_kv_int(b, "prot", (int64_t)w->prot);
            ig_kv_int(b, "probes", (int64_t)w->probes);
            ig_kv_int(b, "refused", (int64_t)w->refused);
            ig_kv_int(b, "hits", (int64_t)w->hits);
            ig_kv_int(b, "truncated", (int64_t)w->truncated);
            ig_obj_close(b);
            if (b->truncated) break;
        }
        ig_arr_close(b);
        emit(sink, b);
    }
}

static void v2_segcand_page(IgSink *sink, IgBuf *b, int *seq) {
    uint32_t parts = (s_segc_n + 7) / 8;
    if (s_segc_n == 0) parts = 0;
    for (uint32_t p = 0; p < parts; ++p) {
        record_out(b, seq, "v2_segcand_page", 0, 0);
        ig_kv_int(b, "part", (int64_t)p);
        ig_kv_int(b, "parts", (int64_t)parts);
        ig_kv_str(b, "table",
                  "seg specs ph0/1/3/7/8 from worker-vaddr-semantics "
                  "(capture sha 3b6cfad4...)", 96);
        ig_arr_open(b, "cands");
        uint32_t from = p * 8, to = from + 8;
        if (to > s_segc_n) to = s_segc_n;
        for (uint32_t i = from; i < to; ++i) {
            const IgSegCand *sc = &s_segc[i];
            if (i != from) ig_buf_ch(b, ',');
            ig_obj_open(b);
            ig_kv_int(b, "span", (int64_t)sc->span_idx);
            ig_kv_hex(b, "start", g_facts.spans[sc->span_idx].start);
            ig_kv_hex(b, "end", g_facts.spans[sc->span_idx].end);
            ig_kv_int(b, "prot", (int64_t)g_facts.spans[sc->span_idx].prot);
            ig_kv_int(b, "ph", (int64_t)sc->ph);
            ig_kv_int(b, "sz", (int64_t)sc->size_match);
            ig_kv_int(b, "pc", (int64_t)sc->prot_compat);
            ig_kv_int(b, "anc", (int64_t)sc->anchor);
            ig_obj_close(b);
            if (b->truncated) break;
        }
        ig_arr_close(b);
        emit(sink, b);
    }
}

static void v2_trial_page(IgSink *sink, IgBuf *b, int *seq) {
    uint32_t parts = (g_facts.trial_count + 7) / 8;
    if (g_facts.trial_count == 0) parts = 0;
    for (uint32_t p = 0; p < parts; ++p) {
        record_out(b, seq, "v2_trial_page", 0, 0);
        ig_kv_int(b, "part", (int64_t)p);
        ig_kv_int(b, "parts", (int64_t)parts);
        ig_kv_str(b, "hyp_legend",
                  "1 spanbase B+v, 2 segtrans, 3 absolute, 4 segtable-L7",
                  64);
        ig_arr_open(b, "trials");
        uint32_t from = p * 8, to = from + 8;
        if (to > g_facts.trial_count) to = g_facts.trial_count;
        for (uint32_t i = from; i < to; ++i) {
            const IgTrial *t = &g_facts.trials[i];
            if (i != from) ig_buf_ch(b, ',');
            ig_obj_open(b);
            ig_kv_int(b, "hyp", (int64_t)t->hyp);
            ig_kv_hex(b, "base", t->base);
            ig_kv_hex(b, "probed", t->probed);
            ig_kv_int(b, "verdict", (int64_t)t->verdict);
            ig_kv_int(b, "code0", (int64_t)t->code0);
            ig_kv_int(b, "la", (int64_t)t->layout_a_verdict);
            ig_kv_int(b, "lb", (int64_t)t->layout_b_verdict);
            ig_kv_int(b, "matched", (int64_t)t->layout_matched);
            ig_kv_int(b, "span", (int64_t)t->span_idx);
            ig_kv_int(b, "cand", (int64_t)t->cand_idx);
            ig_obj_close(b);
            if (b->truncated) break;
        }
        ig_arr_close(b);
        emit(sink, b);
    }
}

static void v2_match_page(IgSink *sink, IgBuf *b, int *seq) {
    uint32_t listed = g_facts.match_count < IG_MATCH_MAX
                          ? g_facts.match_count : IG_MATCH_MAX;
    for (uint32_t m = 0; m < listed; ++m) {
        const IgMatch *mm = &g_facts.matches[m];
        record_out(b, seq, "v2_match_page", 0, 0);
        ig_kv_int(b, "idx", (int64_t)m);
        ig_kv_int(b, "hyp", (int64_t)mm->hyp);
        ig_kv_hex(b, "base", mm->base);
        ig_kv_hex(b, "probed", mm->probed);
        ig_kv_int(b, "gua_layout_matched", (int64_t)mm->layout_matched);
        ig_kv_int(b, "num_elements", (int64_t)mm->num_elements);
        ig_kv_int(b, "num_chunks", (int64_t)mm->num_chunks);
        ig_kv_hex(b, "objects_ptr", mm->objects_ptr);
        ig_kv_int(b, "tick_anchor", mm->tick_anchor == 0xff ? -1 : mm->tick_anchor);
        ig_kv_int(b, "tick_prologue_like", (int64_t)mm->tick_prologue_like);
        ig_kv_int(b, "sco_anchor", mm->sco_anchor == 0xff ? -1 : mm->sco_anchor);
        ig_kv_int(b, "sco_prologue_like", (int64_t)mm->sco_prologue_like);
        ig_kv_int(b, "segc_verdict", (int64_t)mm->segc_verdict);
        ig_kv_hex(b, "segc_span_start", mm->segc_min_start);
        ig_kv_hex(b, "segc_span_end", mm->segc_max_end);
        ig_kv_hex(b, "segc_extent", mm->segc_extent);
        /* BOTH translation hypotheses recorded per candidate (controller
           correction #2): free-mapping implied base vs bias-predicted base,
           each with its own pass bit; never collapsed on predicate evidence. */
        ig_kv_hex(b, "segc_free_l7_start", mm->segc_free_l7_start);
        ig_kv_int(b, "segc_free_pass", (int64_t)mm->segc_free_pass);
        ig_kv_hex(b, "segc_bias_l7_start", mm->segc_bias_l7_start);
        ig_kv_hex(b, "segc_bias_probed_pred", mm->segc_bias_probed_pred);
        ig_kv_int(b, "segc_bias_probed_match", (int64_t)mm->segc_bias_probed_match);
        ig_kv_int(b, "segc_bias_pass", (int64_t)mm->segc_bias_pass);
        ig_kv_int(b, "promoted", (int64_t)mm->promoted);
        ig_kv_str(b, "promotion_rule",
                  "GUA pass never promotes alone; requires both survey "
                  "anchor byte matches AND L7 correspondence; free/bias "
                  "recorded separately", 160);
        ig_kv_int(b, "rfl_struct_verdict", (int64_t)mm->rfl_struct_verdict);
        ig_kv_int(b, "rfl_items", (int64_t)mm->rfl_items);
        ig_kv_int(b, "rfl_plausible", (int64_t)mm->rfl_plausible);
        ig_kv_int(b, "rfl_struct_like", (int64_t)mm->rfl_struct_like);
        ig_kv_hex(b, "rfl_first_struct", mm->rfl_first_struct);
        ig_kv_int(b, "rfl_fld_verdict", (int64_t)mm->rfl_fld_verdict);
        ig_kv_int(b, "rfl_fld_variant", (int64_t)mm->rfl_fld_variant);
        ig_kv_int(b, "rfl_fn_verdict", (int64_t)mm->rfl_fn_verdict);
        ig_kv_hex(b, "rfl_fn_func_ptr", mm->rfl_fn_func_ptr);
        ig_kv_str(b, "rfl_note",
                  "ufunction probe runs at the first UClass-like anchor as "
                  "classifier-only shape evidence (no UFunction address is "
                  "derivable without name resolution, P2 rule)", 128);
        emit(sink, b);
    }
}

static void v2_base_hunt(IgSink *sink, IgBuf *b, int *seq) {
    if (!g_facts.vm_trust) {
        record_out(b, seq, "v2_base_hunt", -2, 0);
        ig_kv_str(b, "status", "needs-map", 16);
        ig_kv_str(b, "note",
                  "no trustworthy VMMAP walk: every foreign deref refused; "
                  "compare v1_vmmap_full walk stats", 96);
        emit(sink, b);
        return;
    }
    tier_a_magic();
    tier_a_segcands();
    tier_a_bias_anchors();
    tier_b_trials();
    tier_c_crosschecks();
    record_out(b, seq, "v2_base_hunt", 0, 0);
    ig_kv_str(b, "provenance",
              "segment table worker-vaddr-semantics (capture 3b6cfad4...); "
              "L0 PROVEN at 0x400000; survey values are p_vaddrs", 128);
    ig_kv_hex(b, "l0_proven_base", IG_L0_PROVEN_BASE);
    ig_kv_hex(b, "runtime_tick", IG_RUNTIME_TICK);
    ig_kv_hex(b, "runtime_sco", IG_RUNTIME_SCO);
    /* Global anchor evidence, emitted even with ZERO GUA candidates so the
       L0 correspondence is checked in every run (depth extension). */
    ig_kv_int(b, "tick_anchor", s_tick_anchor == 0xff ? -1 : s_tick_anchor);
    ig_kv_int(b, "tick_prologue_like", (int64_t)s_tick_prologue_like);
    ig_kv_int(b, "tick_shape_verdict", (int64_t)s_tick_verdict);
    ig_kv_int(b, "tick_shape_code0", (int64_t)s_tick_code0);
    ig_kv_int(b, "sco_anchor", s_sco_anchor == 0xff ? -1 : s_sco_anchor);
    ig_kv_int(b, "sco_prologue_like", (int64_t)s_sco_prologue_like);
    ig_kv_int(b, "sco_shape_verdict", (int64_t)s_sco_verdict);
    ig_kv_int(b, "sco_shape_code0", (int64_t)s_sco_code0);
    /* BOTH translation hypotheses co-official (controller correction #2):
       the falsifiable mapping-recovery prediction (bias-consistent L7 at
       0x400000+L7.p_vaddr, W span covering the bias-predicted GUA probe)
       is CANDIDATE EVIDENCE ONLY — a hit there does not arbitrate. */
    ig_kv_hex(b, "bias_l7_start_pred",
              IG_L0_PROVEN_BASE + IG_SEGS[IG_SEG_L7].vaddr);
    ig_kv_hex(b, "bias_gua_probe_pred",
              IG_L0_PROVEN_BASE + IG_VADDR_GUOBJECT_ARRAY);
    ig_kv_str(b, "hypotheses_open",
              "single-load-bias (L7 at 0x400000+L7.p_vaddr) AND free-mapping "
              "(L7 elsewhere) both kept open; arbitration needs L7 "
              "extent/prot evidence", 160);
    /* Bias-predicted base anchor evidence for ALL five LOAD specs. */
    ig_arr_open(b, "bias_anchor_checks");
    for (uint32_t k = 0; k < s_bias_anchor_n; ++k) {
        if (k != 0) ig_buf_ch(b, ',');
        ig_obj_open(b);
        ig_kv_int(b, "ph", (int64_t)s_bias_anchor[k].ph);
        ig_kv_hex(b, "expected_base", s_bias_anchor[k].addr);
        ig_kv_int(b, "anchor", s_bias_anchor[k].result == 0xff
                                   ? -1 : s_bias_anchor[k].result);
        ig_kv_int(b, "strong", (int64_t)s_bias_anchor[k].strong);
        ig_obj_close(b);
    }
    ig_arr_close(b);
    ig_kv_int(b, "spanstart_probes", (int64_t)g_facts.spanstart_probes);
    ig_kv_int(b, "spanstart_refused", (int64_t)g_facts.spanstart_refused);
    ig_kv_int(b, "magic_hits", (int64_t)g_facts.magic_count);
    ig_kv_int(b, "magic_truncated", (int64_t)g_facts.magic_trunc);
    ig_kv_int(b, "sweeps", (int64_t)g_facts.sweep_count);
    ig_kv_int(b, "grid_probes", (int64_t)g_facts.grid_probes);
    ig_kv_int(b, "grid_refused", (int64_t)g_facts.grid_refused);
    ig_kv_int(b, "images_parsed", (int64_t)g_facts.cand_count);
    ig_kv_int(b, "segcands", (int64_t)s_segc_n);
    ig_kv_int(b, "h1_skipped_unreadable", (int64_t)g_facts.h1_skipped);
    ig_kv_int(b, "h2_skipped_unreadable", (int64_t)g_facts.h2_skipped);
    ig_kv_int(b, "trials_recorded", (int64_t)g_facts.trial_count);
    ig_kv_int(b, "trials_overflow", (int64_t)g_facts.trial_trunc);
    ig_kv_int(b, "gua_matches", (int64_t)g_facts.match_count);
    ig_kv_int(b, "ambiguous", g_facts.ambiguous);
    if (g_facts.ambiguous) {
        ig_kv_str(b, "ambiguity_note",
                  "MORE THAN ONE candidate passed the GUA predicate; ALL "
                  "are preserved in v2_match_page; nothing auto-chosen", 128);
    }
    if (s_l8_rt) {
        ig_kv_hex(b, "l8_mapped_start", s_l8_rt);
        ig_kv_hex(b, "runtime_dynamic", s_dyn_rt);
    }
    if (g_facts.chosen_index >= 0) {
        ig_kv_int(b, "largest_image_idx", (int64_t)g_facts.chosen_index);
        ig_kv_hex(b, "largest_image_base", g_facts.chosen_base);
    }
    emit(sink, b);
    v2_magic_page(sink, b, seq);
    v2_sweep_page(sink, b, seq);
    v2_segcand_page(sink, b, seq);
    v2_trial_page(sink, b, seq);
    v2_match_page(sink, b, seq);
}

/* ------------------------------------------------------------------ */
/* V3: per-match DT_DEBUG summary (details already in v2_match_page). */
static void v3_dtdebug(IgSink *sink, IgBuf *b, int *seq) {
    uint32_t listed = g_facts.match_count < IG_MATCH_MAX
                          ? g_facts.match_count : IG_MATCH_MAX;
    for (uint32_t m = 0; m < listed; ++m) {
        const IgMatch *mm = &g_facts.matches[m];
        record_out(b, seq, "v3_dtdebug", (int64_t)mm->v3_rc, 0);
        ig_kv_int(b, "match_idx", (int64_t)m);
        ig_kv_hex(b, "base", mm->base);
        ig_kv_hex(b, "dyn_addr_used",
                  s_dyn_rt != 0 ? s_dyn_rt : g_facts.chosen_dyn_addr);
        ig_kv_int(b, "dt_debug_present", (int64_t)mm->v3_dt_present);
        ig_kv_hex(b, "dt_debug_value", mm->v3_dt_value);
        ig_kv_hex(b, "r_map", mm->v3_r_map);
        ig_kv_int(b, "entries", (int64_t)mm->v3_lm_entries);
        ig_kv_int(b, "entries_truncated", (int64_t)mm->v3_lm_trunc);
        ig_kv_int(b, "validation_blocked_derefs", (int64_t)mm->v3_lm_blocked);
        if (mm->v3_rc == -9)
            ig_kv_str(b, "note",
                      "no dynamic address derivable (no L8 segment match, "
                      "no parsed image dynamic)", 96);
        emit(sink, b);
    }
    if (listed == 0) {
        record_out(b, seq, "v3_dtdebug", -1, 0);
        ig_kv_str(b, "status", "no-match", 16);
        emit(sink, b);
    }
}

/* ------------------------------------------------------------------ */
/* V4: already-validated-base summary. */
static void v4_base_summary(IgSink *sink, IgBuf *b, int *seq) {
    record_out(b, seq, "v4_base_summary",
               g_facts.vm_trust ? 0 : -2, 0);
    ig_kv_int(b, "gua_matches", (int64_t)g_facts.match_count);
    uint32_t listed = g_facts.match_count < IG_MATCH_MAX
                          ? g_facts.match_count : IG_MATCH_MAX;
    int promoted = 0;
    for (uint32_t m = 0; m < listed; ++m)
        if (g_facts.matches[m].promoted) ++promoted;
    ig_kv_int(b, "promoted_count", (int64_t)promoted);
    ig_kv_str(b, "map_status",
              g_facts.vm_trust
                  ? "v3 walk trustworthy; v1/v2 page records were corrupt "
                    "(fixed-stride) and are superseded by this run"
                  : "needs-map: no trustworthy walk", 128);
    ig_kv_str(b, "hypotheses",
              "h1 B+vaddr; h2 segment-translated (parsed images); h2s "
              "segment-table L7 hunt (matches + RW spans + L1 chain + 1MiB "
              "gap grid, 16K-aligned); h3 absolute vaddr", 160);
    /* RESUME FIX: the prior draft redeclared `listed`/`promoted` here
       (compile-breaking under -Werror); the pre-count emitted above is the
       authoritative promoted_count and the array below is evidence. */
    ig_arr_open(b, "matches");
    for (uint32_t m = 0; m < listed; ++m) {
        const IgMatch *mm = &g_facts.matches[m];
        if (m != 0) ig_buf_ch(b, ',');
        ig_obj_open(b);
        ig_kv_int(b, "idx", (int64_t)m);
        ig_kv_int(b, "hyp", (int64_t)mm->hyp);
        ig_kv_hex(b, "probed", mm->probed);
        ig_kv_int(b, "promoted", (int64_t)mm->promoted);
        ig_kv_int(b, "tick_anchor", mm->tick_anchor == 0xff ? -1 : mm->tick_anchor);
        ig_kv_int(b, "sco_anchor", mm->sco_anchor == 0xff ? -1 : mm->sco_anchor);
        ig_kv_int(b, "segc", (int64_t)mm->segc_verdict);
        ig_kv_int(b, "segc_free", (int64_t)mm->segc_free_pass);
        ig_kv_int(b, "segc_bias", (int64_t)mm->segc_bias_pass);
        ig_obj_close(b);
        if (b->truncated) break;
    }
    ig_arr_close(b);
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* V5: firmware signal + summary. */
static void v5_fw_summary(IgSink *sink, IgBuf *b, int *seq) {
    char vers[96];
    ig_memset(vers, 0, sizeof vers);
    size_t vlen = sizeof vers - 1;
#ifdef KERN_VERSION
    int mib[2] = {CTL_KERN, KERN_VERSION};
    int rc = sysctl(mib, 2, vers, &vlen, 0, 0);
#else
    int rc = -100;
#endif
    int eno = ig_errno();
    record_out(b, seq, "v5_fw_summary", (int64_t)rc, (int64_t)eno);
    ig_kv_str(b, "kern_version", vers, 88);
    ig_kv_str(b, "fw_note",
#ifdef KERN_VERSION
              "sysctl KERN_VERSION recorded; crt1 kernel_get_fw_version is "
              "not linkable in a PRX (no crt1) and is NOT claimed",
#else
              "KERN_VERSION absent from pinned headers; crt1 route absent",
#endif
              120);
    ig_kv_int(b, "records_written", (int64_t)(*seq));
    ig_kv_int(b, "write_failures", (int64_t)sink->failed);
    ig_kv_int(b, "ctor_events", (int64_t)g_ctor_events);
    ig_kv_int(b, "span_count", (int64_t)g_facts.span_count);
    ig_kv_int(b, "gua_matches", (int64_t)g_facts.match_count);
    ig_kv_int(b, "ambiguous", g_facts.ambiguous);
    ig_kv_str(b, "writer_fix",
              "v3 record writer emits separator-correct JSON; every "
              "non-truncated line parses (host-verified json.loads)", 128);
    ig_kv_str(b, "import_profile",
              "7 verbatim console-proven identities only", 48);
    ig_kv_str(b, "quiesce", "inert_no_threads_module_stays_loaded", 40);
    ig_kv_str(b, "unload_attempted", "no", 4);
    ig_kv_str(b, "log_path", IG_LOG_PATH, 64);
    emit(sink, b);
}

void ig_run_all_experiments(IgSink *sink) {
    IgBuf b;
    int seq = 2;  /* seq 1 (entry) and 2 (startup) written by module_entry.c */
    v1_vmmap_full(sink, &b, &seq);
    v2_base_hunt(sink, &b, &seq);
    v3_dtdebug(sink, &b, &seq);
    v4_base_summary(sink, &b, &seq);
    v5_fw_summary(sink, &b, &seq);
}
