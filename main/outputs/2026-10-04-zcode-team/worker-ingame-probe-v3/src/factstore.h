// factstore.h — v3 (worker-ingame-probe-v3, team 2026-10-04).
// Vendored from worker-ingame-probe-v2/src/factstore.h. Changes (labeled):
//   - IG_SPAN_MAX raised 1024 -> 2048 so the observed 1243+ VMMAP records
//     are RETAINED, never silently dropped (controller amendment 3); the
//     per-entry list is still STREAMED to the log in bounded 2 KiB records,
//     so memory stays bounded. The v1/v2 1024 cap + corrupted page records
//     must not be treated as a complete reliable map — v3 records
//     total/retained/truncated explicitly.
//   - IgVmStats + ig_vmmap_walk(): pure buffer walk re-derived from the
//     pinned SDK struct kinfo_vmentry (target/include/sys/user.h), which
//     begins with `int kve_structsize` ("Variable size of record"). v1/v2
//     walked a FIXED sizeof(struct)=1160 stride — the corruption root cause
//     (real kernel records are variable-sized). See vmmap_walk.c.
//   - Base-hunt state (V2 tiers a/b/c) replacing the v2 ELF-only scan:
//     magic probes, GUA predicate-oracle trials under RECORDED address
//     hypotheses, all matching candidates preserved, per-match cross-checks
//     (controller amendment 1), ambiguity never silently resolved.
#ifndef INGAME_FACTSTORE_H
#define INGAME_FACTSTORE_H

#include "ingame_abi.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IG_SPAN_MAX 16384       // worker-span-arbitration: kernel returned
                                // 10017 (v2) / 10088 (v1) records of 144 B;
                                // the cap must cover the FULL record count
                                // (was 1024 in v2, 2048 in the first v3 draft)
#define IG_VMMAP_MAX (4u << 20) // brief bound: 4 MiB buffer growth
#define IG_LINKMAP_MAX 64       // brief bound: link-map walk
#define IG_CAND_MAX 64          // brief bound: ELF image candidates
#define IG_MAGIC_MAX 64         // recorded tier-a magic-probe hits
#define IG_SWEEP_MAX 32         // recorded per-large-span sweep summaries
#define IG_TRIAL_MAX 96         // recorded GUA predicate-oracle trials
#define IG_MATCH_MAX 8          // candidates passing GUA (all preserved)
#define IG_RAW_HDRS 16          // raw 136-byte record snapshots for evidence
#define IG_VM_PER_PART 16       // streamed entries per v1_vmmap_page record

typedef struct IgSpan {
    uint64_t start;   // inclusive
    uint64_t end;     // exclusive
    uint32_t prot;    // raw kve_protection, uninterpreted (bit0 observed readable)
} IgSpan;

/* ------------------------------------------------------------------ */
/* Pure VMMAP buffer walk (vmmap_walk.c; host-unit-tested). */
#define IG_VM_HDR 136           // SDK kinfo_vmentry bytes before kve_path
typedef struct IgVmStats {
    uint32_t total;       // records consumed by this walk (any sanity)
    uint32_t retained;    // sane records kept into out[]
    uint32_t truncated;   // sane records dropped by the out cap
    uint32_t insane;      // records failing per-record sanity
    uint32_t raws_kept;   // raw header snapshots stored
    int walk_mode;        // 1 = per-record kve_structsize, 2 = fixed sizeof
    int stop_reason;      // 0 end-of-buffer (clean), 1 insane/short
                          // structsize stop, 2 partial record at buffer end
    uint64_t stop_off;    // byte offset where the walk stopped
    uint32_t ksz_full;    // structsize == sizeof(kinfo_vmentry) records
    uint32_t ksz_var;     // 137..sizeof-1 (variable/path-truncated) records
    uint32_t ksz_144;     // structsize == 144 records (prediction P1:
                          // worker-span-arbitration — kernel packs records
                          // at 144 B, path stripped; expect ALL of them)
    uint32_t aligned16k;  // sane records with 16 KiB-aligned start AND end
} IgVmStats;

/* Walk mode 1: stride by each record's own kve_structsize (sanity-bounded);
   mode 2: fixed stride (the v1/v2 behavior, kept ONLY as flagged fallback).
   Loads are byte-assembled; no struct casts, no alignment assumptions.
   Returns retained count. */
uint32_t ig_vmmap_walk(const uint8_t *buf, uint64_t len, uint32_t kve_sizeof,
                       int mode, IgSpan *out, uint32_t cap, IgVmStats *st,
                       uint8_t *raws, uint32_t raw_cap);

/* Trust gate: only a walk with enough sane records may back containment. */
int ig_vmmap_trustworthy(const IgVmStats *st);

/* ------------------------------------------------------------------ */
/* Tier-a magic-probe evidence. */
typedef struct IgMagicHit {
    uint64_t addr;        // where 0x7fELF was observed (span-validated read)
    uint32_t span_idx;
    uint32_t prot;        // prot of the span START (probe may read via another)
    int sweep;            // 0 = span-start probe, 1 = 2 MiB grid probe
} IgMagicHit;

typedef struct IgSweep {      // one >64 MiB EXEC/READ span's 2 MiB grid
    uint32_t span_idx;
    uint64_t start, end;
    uint32_t prot;
    uint32_t probes;      // grid addresses examined (bounded)
    uint32_t refused;     // not inside any readable span -> never read
    uint32_t hits;        // 0x7fELF observed
    uint32_t truncated;   // grid cut by the per-span probe bound
} IgSweep;

/* ------------------------------------------------------------------ */
/* Tier-b GUA predicate-oracle trials. hyp values (recorded hypotheses,
 * controller amendment 2 — 0x93ba110 semantics NOT assumed):
 *   1 = h1_span_base:   GUA probed at span_start + vaddr (load-relative with
 *                       the span start AS the image base; covers file-offset
 *                       semantics too, since both spell B + vaddr)
 *   2 = h2_seg_trans:   GUA probed at S.start + (vaddr - seg.p_vaddr) for a
 *                       parsed tier-a image's PT_LOAD segment (per-segment
 *                       correspondence; recorded base is S.start - seg.p_vaddr)
 *   3 = h3_absolute:    GUA probed at the bare vaddr (absolute-address case)
 */
typedef struct IgTrial {
    uint64_t base;        // image_base handed to the predicate
    uint64_t probed;      // base + goa_vaddr (the exact address examined)
    uint8_t hyp;
    uint8_t verdict;      // tlp::Verdict (1 Validated, 2 Inconclusive, 3 Refuted)
    uint16_t code0;       // first rationale code
    uint8_t layout_a_verdict, layout_b_verdict;
    uint8_t layout_matched;   // 0 none, 1 FUObjectArray_outer, 2 TUObjectArray_direct
    int32_t cand_idx;     // tier-a parsed-image index or -1
    uint32_t span_idx;    // origin span (h1) / data span (h2)
} IgTrial;

/* Tier-c cross-checks per GUA-passing candidate (controller amendment 1:
 * a GUA pass NEVER auto-promotes; each cross-check is recorded separately). */
typedef struct IgMatch {
    int trial_idx;
    uint64_t base, probed;
    uint8_t hyp;
    /* cross-check 1/2: SURVEY-RECORDED ANCHOR BYTES at the PROVEN runtime
     * addresses Tick=0x33ac8c0 / SCO=0x890aa0 (worker-vaddr-semantics;
     * mapped-read exception applies — see experiments.c) plus the
     * Inconclusive-only code-shape classifiers:
     *   *_anchor: 1 byte-match, 0 mismatch, 0xff read refused
     *   *_prologue_like: CODE_I_PROLOGUE_LIKE (0x2101) observed */
    uint8_t tick_anchor, sco_anchor;
    uint8_t tick_verdict, tick_prologue_like; uint16_t tick_code0;
    uint8_t sco_verdict, sco_prologue_like; uint16_t sco_code0;
    /* cross-check 3: L7 segment-layout consistency, recorded for BOTH
     * co-official translation hypotheses (controller correction #2 — the
     * single-load-bias hypothesis stays OPEN; a hit at the bias-predicted
     * 0x97ba110 is candidate evidence, NEVER arbitration):
     *   free-mapping:  implied L7 base S_free = probed - 0x27e110; pass iff
     *                  a span at S_free matches L7 extent/anchor.
     *   bias:          L7 expected at 0x400000 + L7.p_vaddr (= 0x953c000)
     *                  and probed == 0x400000 + GUA vaddr (= 0x97ba110);
     *                  pass iff both hold and the span at the expected base
     *                  matches L7 extent/anchor.
     * segc_verdict encodes: 0 fail, 1 pass via S_free, 2 pass via bias,
     * 3 both, 4 pass but no span check possible (recorded, never silent).
     * ARBITRATION between the two requires this L7 correspondence evidence
     * (extent/prot/shared-page behavior), not the GUA predicate alone. */
    uint8_t segc_verdict;
    uint64_t segc_min_start, segc_max_end, segc_extent;
    uint64_t segc_free_l7_start;    /* probed - 0x27e110 (free-mapping) */
    uint64_t segc_bias_l7_start;    /* 0x400000 + L7.p_vaddr (bias) */
    uint64_t segc_bias_probed_pred; /* 0x400000 + GUA vaddr (bias-predicted probe) */
    uint64_t segc_bias_span_start, segc_bias_span_end; /* bias-interpretation
        matching span bounds (0 when no bias span pass) */
    uint8_t segc_free_pass, segc_bias_pass;
    uint8_t segc_bias_probed_match; /* probed == bias-predicted probe addr */
    uint8_t promoted;         /* 1 iff anchors matched AND segc pass */
    /* GUA observed (feeds the rfl ValidatedGuaView) */
    uint64_t objects_ptr; int32_t num_elements, num_chunks;
    uint8_t layout_matched;
    /* V3 dtdebug at this match's base (dyn known via its tier-a image) */
    int v3_rc; uint8_t v3_dt_present; uint64_t v3_dt_value, v3_r_map;
    int32_t v3_lm_entries; uint8_t v3_lm_trunc, v3_lm_blocked;
    /* rfl batch-2 results (tier c full set) */
    int8_t rfl_struct_verdict;      // -1 not run
    uint32_t rfl_items, rfl_plausible, rfl_struct_like, rfl_uclass_like;
    uint64_t rfl_first_struct, rfl_first_uclass;
    int8_t rfl_fld_verdict; uint16_t rfl_fld_code0; uint8_t rfl_fld_variant;
    int8_t rfl_fn_verdict; uint16_t rfl_fn_code0; uint64_t rfl_fn_func_ptr;
} IgMatch;

/* ------------------------------------------------------------------ */
/* One candidate image from tier-a magic hits (v2 IgCand retained + the
   PT_LOAD segment table needed by hypothesis h2). */
typedef struct IgCand {
    uint64_t base;            // address where ELF magic was observed
    uint64_t span_start;      // owning span bounds (evidence)
    uint64_t span_end;
    uint32_t prot;
    int magic_ok;             // 0x7fELF at base, span-validated read
    int class_ok;             // ELFCLASS64
    uint16_t e_type;          // raw e_type (2=EXEC 3=DYN observed values)
    uint16_t phnum;
    int phdr_ok;              // phdr table fully span-readable, sane entsize
    uint64_t dyn_vaddr;       // PT_DYNAMIC p_vaddr (0 if none/unreadable)
    uint64_t dyn_filesz;
    int dyn_found;
    int dyn_mode;             // 0 none, 1 dyn@base+vaddr, 2 dyn@absolute vaddr
    int dyn_parses;           // dynamic walk saw entries (and DT_NULL or cap)
    int dyn_entries;          // entries actually scanned
    int dt_null_seen;
    int has_exec_segment;     // some PT_LOAD carries PF_X
    uint64_t extent;          // max(p_vaddr+p_memsz) over PT_LOADs (vaddr space)
    int chosen;               // 1 for the largest parsed executable image
    /* v3: PT_LOAD segments for the h2 per-segment correspondence */
    struct { uint64_t vaddr, memsz; } segs[6];
    int seg_count;
    int seg_gua;              // index of the segment containing the GUA vaddr, -1
} IgCand;

typedef struct IgFacts {
    /* Collected EARLY (before all experiments) for fault containment;
       EMITTED as V1 (complete streamed list + stats + raw evidence). */
    IgSpan spans[IG_SPAN_MAX];
    uint32_t span_count;
    int span_truncated;
    uint64_t vmmap_buf;
    uint64_t vmmap_entries;
    uint64_t vmmap_kve_size;
    uint64_t vmmap_first_ksz; // record 0's own kve_structsize (expect 144)
    int vmmap_mod_ok;         // 1 iff buffer_len % first_ksz == 0 (arbitration
                              // assertion; both observed v1/v2 buffers divide
                              // exactly: 1442448=10017x144, 1452672=10088x144)
    unsigned vmmap_attempts;
    int vmmap_rc;
    int vmmap_errno;
    IgVmStats vm_walk;        // primary (mode-1) walk stats
    IgVmStats vm_walk_fb;     // fixed-stride fallback stats (only if used)
    int vm_fallback_used;     // 1 => spans[] came from the mode-2 walk
    int vm_trust;             // 1 => spans[] trustworthy for containment
    uint8_t vm_raw[IG_RAW_HDRS * IG_VM_HDR];  // first records' raw headers

    /* tier a */
    IgMagicHit magic[IG_MAGIC_MAX];
    uint32_t magic_count, magic_trunc;
    uint64_t spanstart_probes, spanstart_refused;
    IgSweep sweeps[IG_SWEEP_MAX];
    uint32_t sweep_count, sweep_trunc;
    uint64_t grid_probes, grid_refused;

    /* tier b */
    IgTrial trials[IG_TRIAL_MAX];
    uint32_t trial_count, trial_trunc;
    uint64_t h1_skipped, h2_skipped;
    IgCand cands[IG_CAND_MAX];
    uint32_t cand_count;
    int cand_truncated;

    /* tier c */
    IgMatch matches[IG_MATCH_MAX];
    uint32_t match_count;
    int ambiguous;            // >1 GUA-passing candidate (amendment 1b)

    /* v2 lineage fields kept for the V3/V4 records */
    int chosen_index;         // largest parsed executable tier-a image, -1 none
    uint64_t chosen_base, chosen_extent, chosen_dyn_addr;
    int chosen_dyn_mode;
} IgFacts;

extern IgFacts g_facts;

/* errno read through the imported __error word; 0 when unavailable. */
static inline int ig_errno(void) {
    int *e = __error();
    return e ? *e : 0;
}

/* True iff [addr, addr+len) lies fully inside one READABLE (prot bit0) span.
   Used BEFORE every foreign-pointer dereference: a false value means the
   dereference is refused and recorded, never attempted. When the VMMAP walk
   failed its trust gate, span_count is 0 and EVERY foreign deref is refused
   (needs-map) — the honest safe failure mode after v1/v2's corrupt maps. */
int ig_range_readable(uint64_t addr, uint64_t len);

/* Copy at most len bytes; returns bytes copied; refuses unreadable ranges. */
size_t ig_copy_checked(uint64_t addr, void *dst, size_t len);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  // INGAME_FACTSTORE_H
