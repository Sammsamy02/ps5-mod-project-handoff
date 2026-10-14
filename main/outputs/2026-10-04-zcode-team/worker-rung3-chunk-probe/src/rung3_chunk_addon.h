/*
 * rung3_chunk_addon.h
 *
 * RUNG-3 CHUNK/OBJECT READ ADD-ON for the next probe iteration (igf7-lineage
 * prep; NOT the current igf6, no console run in this assignment).
 * Team: 2026-10-04-zcode-team / worker-rung3-chunk-probe.
 *
 * Purpose
 * -------
 * worker-batch2-live-prep validated the rung-3 read core (igu_probe_rung3,
 * 111/111 host assertions): GUA header -> Objects** -> chunk[0] -> first
 * FUObjectItem -> UObjectBase. Two things were still open for the next probe
 * and are delivered HERE as an ADD-ON (no duplication, same probe lineage):
 *
 *   a) A MINIMAL in-process VMMAP re-walk. The live Objects** value
 *      0x13800220a0 is NOT covered by the recovered span list (gap
 *      0x1340c50000..0x13803b0000; worker-batch2-live-prep REPORT sec.2) —
 *      the live probe must re-walk KERN_PROC_VMMAP first. This module walks
 *      the sysctl buffer with the v3 variable-kve_structsize stride
 *      (worker-ingame-probe-v3/src/vmmap_walk.c pattern; KVE field offsets
 *      structsize=0, start=8, end=16, prot=56 per the pinned SDK header),
 *      keeps only sane records, and stops EARLY once every caller-named
 *      target address (GUA, chunk table, chunk windows, object windows) is
 *      covered by a retained READABLE span. NOT a full map dump: stats
 *      distinguish stop-at-end / insane-structsize / partial-tail /
 *      early-stop, and the v3 trust gate (retained>=8 && insane*4<=retained)
 *      decides whether ANY read may run on the result.
 *
 *   b) The PER-CHUNK deep read the core does not do: chunk[0] AND chunk[1]
 *      pointers, the first FUObjectItem (0x18) of EACH chunk, and the
 *      UObjectBase (0x28) fields of the object behind the first non-null
 *      item, with the batch2 check family (vptr / RF_AllFlags / index
 *      bounds / round-trip / ClassPrivate / OuterPrivate) plus the
 *      worker-5 needed-chunk rule (GOA_R_CHUNK_NULL_WHILE_NEEDED). Partial
 *      evidence is never lost: a chunk[1] failure does not discard recorded
 *      chunk[0] evidence, it only degrades the overall class honestly.
 *
 * Provenance (nothing guessed; all cited, none re-derived here):
 *   - Read core, span table, status codes, KVME prot bits, POSIX-mode
 *     translation: worker-batch2-live-prep/src/live_gua_reads.h (included;
 *     its provenance block carries the layout-A GUA offsets, the SET-5_06
 *     FUObjectItem/UObjectBase offsets, RF_AllFlags, 65536-element chunk
 *     identity, 48-bit user-canonical rule, and the NamePrivate/OuterPrivate
 *     prompt-vs-setter discrepancy note).
 *   - VMMAP walk: worker-ingame-probe-v3/src/vmmap_walk.c (cited): per-record
 *     kve_structsize stride, sanity bounds (start<=end, extent<=2^44,
 *     prot<=0xff, 136<=ksz<=kve_sizeof, 4-aligned), stop-never-guess.
 *   - Needed-chunk rule: worker-5 GOA_R_CHUNK_NULL_WHILE_NEEDED — chunk[i]
 *     null for a needed i (i < ceil(NumElements/65536)) refutes; the live
 *     header (71959 elements, 2 chunks) needs BOTH chunks 0 and 1.
 *   - Batch2 predicate handoff: rfl::probe_ustruct_chain (UStruct chain +
 *     UClass CDO extension) consumes a ValidatedGuaView assembled ONLY from
 *     a record whose chain steps a/b validated — see
 *     src/rung3_batch2_bridge.hpp; tlp::MemSpan has NO prot field and trusts
 *     spans unconditionally, so ONLY readable spans may be published.
 *
 * Import contract (igf7-lineage): this module adds ZERO new imports. The
 * lineage identity set is the 7 imports of worker-ingame-probe-v3/
 * src/ingame_abi.h (__error, sceKernelOpen, sceKernelWrite, sceKernelClose,
 * sysctl, getpid, clock_gettime); the sysctl VMMAP buffer arrives through
 * the existing path and everything below is pure C99 over that buffer.
 *
 * Safety invariant (unchanged from the core): the ONLY memory reads happen
 * inside igu_read()/igu_read_*() through span->data after a covering
 * READABLE-span proof. An address not covered by the fresh map is recorded
 * as evidence-unavailable (Inconclusive), never guessed, never dereferenced.
 * If the walk itself fails its trust gate the probe refuses EVERY read
 * (RC3_ST_NO_MAP) — the honest needs-map failure mode.
 *
 * Conventions: C99, no OS calls, no allocation, no console/network, no VLAs.
 * Host-testable: walk spans carry identity data pointers (live discipline);
 * host tests re-point span->data into fixture bytes after the walk.
 */

#ifndef RUNG3_CHUNK_ADDON_H
#define RUNG3_CHUNK_ADDON_H

#include "../../worker-batch2-live-prep/src/live_gua_reads.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Minimal VMMAP walk                                                  */
/* ------------------------------------------------------------------ */

/* Max caller-named coverage targets for the early-stop rule (mask fits
 * u32; the probe itself needs well under 8). Not a map-size bound: the
 * retained-span cap is the caller's span array. */
#define RC3_MAX_TARGETS 16u

/* Stop reasons (v3 parity + early-stop; 0..2 are v3's codes). */
#define RC3_WALK_STOP_END        0  /* buffer consumed cleanly            */
#define RC3_WALK_STOP_INSANE     1  /* insane/short kve_structsize: stop  */
#define RC3_WALK_STOP_PARTIAL    2  /* record header runs past buffer end */
#define RC3_WALK_STOP_TARGETS    3  /* early stop: all targets covered    */

typedef struct rc3_vmstats {
    uint32_t total;         /* records consumed (any sanity)               */
    uint32_t retained;      /* sane records kept into out[]                */
    uint32_t truncated;     /* sane records dropped by the cap             */
    uint32_t insane;        /* records failing per-record sanity           */
    int      walk_mode;     /* always 1 (variable kve_structsize stride)   */
    int      stop_reason;   /* RC3_WALK_STOP_*                             */
    uint64_t stop_off;      /* byte offset where the walk stopped          */
    uint32_t ksz_full;      /* ksz == kve_sizeof records (SDK-vendored)    */
    uint32_t ksz_var;       /* 137..kve_sizeof-1 records (path-stripped)   */
    uint32_t ksz_144;       /* ksz == 144 (span-arbitration prediction P1) */
    uint32_t aligned16k;    /* sane records 16 KiB-aligned start AND end   */
    uint32_t targets_total; /* targets considered (clamped to 16)          */
    uint32_t targets_covered; /* targets covered by a retained READ span   */
    uint32_t targets_mask;  /* bit i: target i covered                     */
    int      trustworthy;   /* v3 gate: retained>=8 && insane*4<=retained  */
} rc3_vmstats;

/* Walk the KERN_PROC_VMMAP buffer with the per-record kve_structsize stride.
 * Retained spans are KVME-encoded (prot bits R=1 W=2 X=4) and get IDENTITY
 * data pointers (live discipline: (const uint8_t*)start); a host test must
 * re-point span->data into fixture bytes before probing.
 *
 * targets/ntargets name the addresses that MUST be covered before the walk
 * may stop early (stop_reason RC3_WALK_STOP_TARGETS); a target counts as
 * covered when some retained span with READ prot contains [t, t+8). With
 * ntargets==0 the early stop is disabled (pure bounded walk).
 *
 * Never guesses: an insane structsize stops the walk (v3 discipline) and
 * leaves the surviving spans for the caller to judge via rc3_vmstats.
 * Returns the retained count. */
uint32_t rc3_vmmap_walk_min(const uint8_t* buf, uint64_t len,
                            uint32_t kve_sizeof,
                            igu_span* out, uint32_t cap,
                            const uint64_t* targets, uint32_t ntargets,
                            rc3_vmstats* st);

/* v3 trust gate (same formula, cited): 1 iff the walk retained enough sane
 * records with a sane residue. A walk that fails this gate must back NO
 * foreign read; rc3_probe_chunks enforces this via its `vm` argument. */
int rc3_vmmap_trustworthy(const rc3_vmstats* st);

/* ------------------------------------------------------------------ */
/* Per-chunk deep read                                                 */
/* ------------------------------------------------------------------ */

/* Positive status for "step not applicable" (e.g. chunk[1] when
 * num_chunks < 2). Distinct from every igu code (0 and negatives). */
#define RC3_ST_NOT_RUN   1
/* Probe-refused status: the walk backing this probe failed the trust gate;
 * nothing was read (needs-map). */
#define RC3_ST_NO_MAP    2

/* One chunk's commissioned evidence. */
typedef struct rc3_chunk_view {
    int      st_ptr;        /* chunk pointer read status (igu codes or
                               RC3_ST_NOT_RUN)                            */
    uint64_t ptr;           /* chunk[i] pointer (valid when st_ptr==OK)    */
    int      st_item;       /* first-item scan status (igu codes or
                               RC3_ST_NOT_RUN)                            */
    uint64_t item_object;   /* FUObjectItem.Object @+0x00 (raw)            */
    uint32_t item_flags;    /* +0x08 raw — NO cited mask: never judged     */
    uint32_t item_cluster;  /* +0x0C ClusterRootIndex raw                  */
    uint32_t item_serial;   /* +0x10 SerialNumber raw                      */
    uint32_t items_scanned; /* slots scanned up to the accepted item (>=1),
                               or the scan bound when none was non-null    */
    int      item_nonnull;  /* 1 iff a non-null canonical object was found */
    int      chk_needed;    /* IGU_CSK_*: null chunk while needed
                               (i < ceil(num_elements/65536)) refutes
                               (worker-5 GOA_R_CHUNK_NULL_WHILE_NEEDED)    */
} rc3_chunk_view;

/* Full add-on record. Every status field is populated on every path. */
typedef struct rc3_record {
    /* Map evidence (fills even when the probe refuses). */
    int      map_trusted;       /* vm gate result at probe entry           */
    rc3_vmstats vm;             /* walk stats (caller-supplied, recorded)  */

    /* Header (layout A; valid when st_hdr == IGU_OK). */
    int      st_hdr;
    uint32_t first_gc;          /* +0x00 */
    uint32_t last_non_gc;       /* +0x04 */
    uint32_t max_not_gc;        /* +0x08 */
    uint32_t open_dfgc;         /* +0x0C */
    uint32_t max_elements;      /* +0x20 */
    uint32_t num_elements;      /* +0x24 */
    uint32_t max_chunks;        /* +0x28 */
    uint32_t num_chunks;        /* +0x2C */
    int      info_max_eq_chunks;/* non-gating evidence, IGU_CSK_*          */
    int      info_num_le_max;
    int      info_nchunks_ok;

    /* a) Objects** = u64 @ GUA+0x10 */
    int      st_a;
    uint64_t objects_ptr;

    /* b+c) per-chunk views, chunk[0] and chunk[1] */
    rc3_chunk_view chunk[2];

    /* d) UObjectBase (0x28) behind the first non-null item across the two
     * chunks (chunk 0 preferred); obj_source_chunk = 0/1, -1 when none. */
    int      st_obj;
    int      obj_source_chunk;
    uint64_t obj_addr;
    uint64_t vptr;              /* +0x00 (identity-only, never called)     */
    uint32_t obj_flags;         /* +0x08 */
    int32_t  internal_index;    /* +0x0C */
    uint64_t class_ptr;         /* +0x10 */
    uint8_t  name_raw[8];       /* +0x18 RAW, never interpreted (P2 rule)  */
    uint64_t outer_ptr;         /* +0x20 */

    /* e) checks on the selected object (IGU_CSK_*). */
    int chk_vptr;          /* non-null, canonical, 8B window readable       */
    int chk_flags;         /* (obj_flags & ~0x0FFFFFFF) == 0                */
    int chk_index_bounds;  /* 0 <= internal_index < num_elements            */
    int chk_roundtrip;     /* item[internal_index].Object == obj_addr       */
    int chk_class;         /* class_ptr non-null, canonical, 8B readable    */
    int chk_outer;         /* outer_ptr == 0 or canonical + 8B readable     */

    int rc;                /* overall: refute > coverage-class > OK          */
} rc3_record;

/* The add-on read sequence over a WALKED span table.
 *
 * `vm` must be the rc3_vmstats of the walk that produced `t`; when
 * rc3_vmmap_trustworthy(vm) fails the probe reads NOTHING and returns
 * RC3_ST_NO_MAP with map_trusted=0 (needs-map; the next console iteration
 * must fix the map, not guess through it).
 *
 * scan_max bounds the per-chunk null-item scan (0 or >64 clamps to the
 * core's IGU_SCAN_MAX_ITEMS=64 parity). Both chunk views are always
 * attempted for i < num_chunks (recorded individually — a chunk[1]
 * coverage failure never discards chunk[0] evidence); chunk views with
 * i >= num_chunks get st_ptr = RC3_ST_NOT_RUN.
 *
 * Returns rec->rc. Overall rc: any refute-class fact (non-canonical
 * pointer, needed-chunk null, contradicted check) wins over any coverage
 * failure (IGU_E_NO_SPAN / NOT_READABLE / STRADDLE / exhausted scan),
 * which wins over IGU_OK. Info checks stay non-gating (core discipline). */
int rc3_probe_chunks(const igu_span_table* t, const rc3_vmstats* vm,
                     uint64_t gua_addr, uint32_t scan_max,
                     rc3_record* rec);

/* Overall evidence class:
 *   0 = Validated-class: map trusted, all commissioned steps and checks
 *       pass (caller may hand the published spans to the batch2 bridge).
 *   1 = Inconclusive: any coverage failure / exhausted scan / refused map
 *       / not-run commissioned step. Evidence unavailable, never failure.
 *   2 = Refuted: any non-canonical pointer or CONTRADICTED check.
 * Info-check contradictions are recorded but non-gating here; the batch2
 * bridge gate independently refuses the handoff when they contradict. */
int rc3_class(const rc3_record* rec);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RUNG3_CHUNK_ADDON_H */
