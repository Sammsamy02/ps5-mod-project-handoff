/*
 * live_gua_reads.h
 *
 * RUNG-3 LIVE-READ SEQUENCE for the batch2/batch3 reflection predicates.
 * Team: 2026-10-04-zcode-team / worker-batch2-live-prep.
 *
 * Purpose
 * -------
 * The worker-5 GUA predicate validated the FUObjectArray HEADER in place
 * (layout A at 0x97ba110, 71959 objects). The batch2/batch3 predicates
 * (probe_ustruct_chain / probe_uclass_property_links / probe_ffield_variant)
 * need to go one rung deeper: follow Objects** (chunk-table pointer,
 * live value 0x13800220a0), then chunk[0], then FUObjectItem[0], then the
 * UObjectBase it points at, checking vtable / flags / index / class / outer.
 * Nothing in the tree so far has dereferenced ANY of those pointers.
 *
 * This module is the exact, span-validated read sequence to run in-process
 * from a PRX. Every single dereference is preceded by a covering-readable-
 * span check against a caller-supplied span table (built from an in-process
 * KERN_PROC_VMMAP walk, e.g. the variable-kve_structsize walk of
 * worker-ingame-probe-v3/src/vmmap_walk.c). There is NO raw dereference
 * path: memory is only ever read through igu_span::data, and only after the
 * window [addr, addr+len) is fully covered by ONE span with READ prot.
 * An uncovered address can never fault the game; it produces a
 * machine-readable "evidence unavailable" status instead.
 *
 * Provenance of every constant (nothing guessed; NOT re-derived here):
 *   Layout-A FUObjectArray offsets (worker-predicate-gc-fix
 *   vendored-fixed/src/townfall_layout_probes.cpp:23-25,31):
 *     ObjFirstGCIndex=0x00, ObjLastNonGCIndex=0x04,
 *     MaxObjectsNotConsideredByGC=0x08, OpenForDisregardForGC=0x0C,
 *     ObjObjects (embedded TUObjectArray)=0x10, so:
 *     Objects**  = GUA+0x10   (TUObjectArray.Objects, pointer to chunk ptrs)
 *     MaxElements= GUA+0x20   (TUObjectArray.ObjMaxElements, u32)
 *     NumElements= GUA+0x24   (u32; live 71959)
 *     MaxChunks  = GUA+0x28   (u32; live 33)
 *     NumChunks  = GUA+0x2C   (u32; live 2)
 *   Live-validated observed set (worker-staged-pair-live/CLAIM.json):
 *     Objects**=0x13800220a0, MaxElements=2162688, NumElements=71959,
 *     MaxChunks=33, NumChunks=2, first_gc=35477, last_non_gc=35476,
 *     OpenForDisregardForGC=0, PreAllocated=NULL; identity engine 5.6,
 *     prospero-x86_64-sysv, pointer_size 8, single-load-bias base 0x400000.
 *   Chunk math (batch2 hpp provenance, UOA-HPP UObjectArray.hpp:88):
 *     NumElementsPerChunk = 64*1024 ELEMENTS per chunk (NOT bytes);
 *     FUObjectItem stride = 0x18 (SET-5_06 FUObjectItem: Object=0x0,
 *     Flags=0x8, ClusterRootIndex=0xC, SerialNumber=0x10).
 *     Live arithmetic cross-check: 2162688 == 33 * 65536 exactly.
 *   UObjectBase (SET-5_06 setter, batch2 hpp provenance block):
 *     vtable=0x0 (C++ vptr, identity-only), ObjectFlags u32=0x8,
 *     InternalIndex_Private i32=0xC, ClassPrivate=0x10, NamePrivate=0x18,
 *     OuterPrivate=0x20, total size 0x28.
 *     NOTE: the commissioning prompt listed OuterPrivate@0x18 and
 *     NamePrivate@0x20. The retained SET-5_06 setter and batch2's validated
 *     predicates both put NamePrivate BEFORE OuterPrivate (0x18 < 0x20).
 *     This module follows the cited setter and records both raw windows so
 *     the discrepancy is visible, never silently resolved.
 *   Masks (batch2 hpp provenance): RF_AllFlags = 0x0FFFFFFF
 *     (UnrealFlags.hpp:95); ObjectFlags sane iff no bits outside that mask.
 *   Canonical user-space pointer: (p >> 47) == 0 (48-bit user half),
 *     matching tlp::pointer_canonical_or_null's user-half discipline.
 *
 * Verdict mapping (caller side), mirroring batch2/batch3 discipline:
 *   Validated    = all steps OK and all checks PASS.
 *   Inconclusive = any coverage failure (no span / not readable / straddle)
 *                  or an exhausted null-scan. Evidence unavailable; NEVER
 *                  success or failure; never a raw fault.
 *   Refuted      = a cited layout forbids the bytes (non-canonical pointer,
 *                  flag bits outside RF_AllFlags, index out of bounds or
 *                  failing round-trip, null where the layout requires a
 *                  non-null object/class).
 *
 * Conventions: C99, no OS calls, no allocation, no console/network, no
 * VLAs. Host-testable: spans carry a host `data` pointer so the same code
 * runs on synthetic images (data = &fixture_bytes) and in-process
 * (data = (const uint8_t*)start, identity).
 */

#ifndef LIVE_GUA_READS_H
#define LIVE_GUA_READS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* KVME_PROT_READ/WRITE/EXEC = 1/2/4 (sys/user.h:425-427, FreeBSD 11.4). */
#define IGU_PROT_R 0x1u
#define IGU_PROT_W 0x2u
#define IGU_PROT_X 0x4u

/* ------------------------------------------------------------------ */
/* Span table                                                          */
/* ------------------------------------------------------------------ */

typedef struct igu_span {
    uint64_t start;        /* inclusive vaddr                            */
    uint64_t end;          /* exclusive vaddr (start + bytes)            */
    uint32_t prot;         /* KVME_PROT_* bits                           */
    const uint8_t* data;   /* host address of the byte at `start`.       */
                           /* Live supplier: (const uint8_t*)start.      */
                           /* Host fixture: pointer into fixture memory. */
                           /* NEVER dereferenced beyond [start,end).     */
} igu_span;

typedef struct igu_span_table {
    const igu_span* spans;
    size_t count;
} igu_span_table;

/* Readability policy: prot & IGU_PROT_R. NOTE the documented live caveat:
 * the recovered vmmap shows the exec image 0x400000-0x787c000 as prot 4
 * (X-only). Under this policy a vtable inside it classifies as
 * covered_not_readable (Inconclusive), never as a refutation, until the
 * span supplier re-arbitrates image prot semantics. Conservative by
 * design: the module would rather report "evidence unavailable" than read
 * through a wrong assumption. */
int igu_span_readable(const igu_span* s);

/* Live-supplier helper: identity-map the span data pointer. */
void igu_span_set_data_identity(igu_span* s);

/* PROTECTION-ENCODING HAZARD (observed in this tree, must not be guessed
 * away): kernel KERN_PROC_VMMAP records carry KVME_PROT bits (READ=1,
 * WRITE=2, EXEC=4; sys/user.h:425-427) — e.g. heap anon = 3, rodata = 1.
 * But probe-side span logs use POSIX mode bits (R=4, W=2, X=1) — the
 * validated GUA span was logged "prot 6" meaning POSIX RW, which under
 * KVME bits would decode as W|X (unreadable!). Every span fed to this
 * module MUST be KVME-encoded; suppliers reading older probe-side records
 * translate with this helper (4->1, 2->2, 1->4). */
uint32_t igu_posix_mode_to_kvme(uint32_t posix_mode);

/* ------------------------------------------------------------------ */
/* Span-validated reads                                                */
/* ------------------------------------------------------------------ */

/* Read result codes. Coverage failures are FIRST-CLASS evidence classes:
 *   IGU_E_NO_SPAN      -> not_covered_current_map
 *   IGU_E_NOT_READABLE -> covered_not_readable
 *   IGU_E_STRADDLE     -> window crosses a span boundary (partial cover) */
enum igu_read_rc {
    IGU_OK             =  0,
    IGU_E_NO_SPAN      = -1,
    IGU_E_NOT_READABLE = -2,
    IGU_E_STRADDLE     = -3,
    IGU_E_ARG          = -4,
    /* Semantic outcomes of the rung-3 sequence (step-level): */
    IGU_R_NULL         = -5, /* required pointer is null (context-dep.)  */
    IGU_R_NONCANONICAL = -6, /* pointer fails the user-canonical rule    */
    IGU_R_CHECK_FAIL   = -7, /* a step-e structural check CONTRADICTED   */
    IGU_R_SKIPPED      = -8  /* upstream step failed; value meaningless  */
};

/* Find the ONE span fully covering [addr, addr+len). Returns span index,
 * or negative IGU_E_* code. prot_out (may be NULL) receives the prot. */
int igu_find_covering(const igu_span_table* t, uint64_t addr, uint64_t len,
                      uint32_t* prot_out);

/* Copy len bytes from vaddr `addr` into `out`. Whole window must be
 * covered by one READABLE span. This is the ONLY memory access in the
 * module. Explicit little-endian accessors below; on the target the bytes
 * are LE, so these are plain copies with explicit LE reassembly (host
 * fixtures also store LE). */
int igu_read(const igu_span_table* t, uint64_t addr, void* out, size_t len);
int igu_read_u64(const igu_span_table* t, uint64_t addr, uint64_t* out);
int igu_read_u32(const igu_span_table* t, uint64_t addr, uint32_t* out);
int igu_read_i32(const igu_span_table* t, uint64_t addr, int32_t* out);

/* User-space canonicality: p == 0 is handled by callers; non-null p is
 * canonical iff (p >> 47) == 0. */
int igu_ptr_canonical(uint64_t p);

/* ------------------------------------------------------------------ */
/* Rung-3 record                                                       */
/* ------------------------------------------------------------------ */

/* Per-check state (batch3 parity: coverage failure is UNAVAILABLE, never
 * CONTRADICTED; only read-and-contradicted facts are CONTRADICTED). */
enum igu_check_state {
    IGU_CSK_NOT_RUN     = 0,
    IGU_CSK_PASS        = 1,
    IGU_CSK_CONTRADICTED = 2,
    IGU_CSK_UNAVAILABLE = 3
};

typedef struct igu_rung3_record {
    uint64_t gua_addr;

    /* Header observations (layout A; valid when st_hdr == IGU_OK). */
    int      st_hdr;
    uint32_t first_gc;          /* +0x00 */
    uint32_t last_non_gc;       /* +0x04 */
    uint32_t max_not_gc;        /* +0x08 */
    uint32_t open_dfgc;         /* +0x0C */
    uint32_t max_elements;      /* +0x20 */
    uint32_t num_elements;      /* +0x24 */
    uint32_t max_chunks;        /* +0x28 */
    uint32_t num_chunks;        /* +0x2C */
    /* Non-gating header info checks (IGU_CSK_*): */
    int      info_max_eq_chunks;/* max_elements == max_chunks*65536 (live: 1) */
    int      info_num_le_max;   /* num_elements <= max_elements               */
    int      info_nchunks_ok;   /* 1 <= num_chunks <= max_chunks              */

    /* a) Objects** = u64 @ GUA+0x10 */
    int      st_a;
    uint64_t objects_ptr;

    /* b) chunk[i] pointer (i = scanned chunk index, normally 0) */
    int      st_b;
    uint64_t chunk_ptr;
    uint32_t chunk_index;

    /* c) FUObjectItem (0x18) at chunk_ptr + slot*0x18 */
    int      st_c;
    uint64_t item_object;       /* +0x00 */
    uint32_t item_flags;        /* +0x08 raw (no cited mask: never judged) */
    uint32_t item_cluster;      /* +0x0C raw */
    uint32_t item_serial;       /* +0x10 raw */
    uint32_t items_scanned;     /* items from slot 0 to the accepted slot  */

    /* d) UObjectBase (0x28) at item_object */
    int      st_d;
    uint64_t vptr;              /* +0x00 (identity-only, never called)     */
    uint32_t obj_flags;         /* +0x08 */
    int32_t  internal_index;    /* +0x0C */
    uint64_t class_ptr;         /* +0x10 */
    uint8_t  name_raw[8];       /* +0x18 RAW, never interpreted (P2 rule;
                                    prompt/setter discrepancy documented) */
    uint64_t outer_ptr;         /* +0x20 */

    /* e) checks */
    int chk_vptr;        /* non-null, canonical, 8B window readable         */
    int chk_flags;       /* (obj_flags & ~0x0FFFFFFF) == 0                  */
    int chk_index_bounds;/* 0 <= internal_index < num_elements              */
    int chk_roundtrip;   /* item[internal_index].Object == item_object,
                            chunk math idx/65536, slot idx%65536            */
    int chk_class;       /* class_ptr non-null, canonical, 8B readable      */
    int chk_outer;       /* outer_ptr == 0 or canonical + 8B readable       */

    int rc;              /* IGU_OK or the first failing step/check code     */
} igu_rung3_record;

/* Max slots scanned for the first non-null item (batch2 walk cap parity). */
#define IGU_SCAN_MAX_ITEMS 64u

/* The rung-3 live-read sequence. scan_max bounds the null-item scan
 * (use IGU_SCAN_MAX_ITEMS or a smaller bound). rec->rc and every status
 * field are always populated. Returns rec->rc. */
int igu_probe_rung3(const igu_span_table* t, uint64_t gua_addr,
                    uint32_t scan_max, igu_rung3_record* rec);

/* Overall evidence class of a completed record:
 *   0 = all pass (caller may call the batch2/batch3 predicates),
 *   1 = Inconclusive (evidence unavailable: any coverage failure,
 *       exhausted scan, or skipped check),
 *   2 = Refuted (any check CONTRADICTED or semantic non-canonical/null
 *       where the cited layout requires a live object). */
int igu_rung3_class(const igu_rung3_record* rec);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIVE_GUA_READS_H */
