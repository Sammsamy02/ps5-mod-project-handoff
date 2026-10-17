// carrier_driver.c — worker-carrier-probe-build (cc1, team 2026-10-04).
// NEW FILE. See carrier_driver.h for the mission, the name table provenance
// and the constants. Implementation classes, all lineage-proven:
//
//   - Read mechanism (the ONLY foreign-memory access class, inherited from
//     the igf6/qdrv1/fc1 proven pattern): a CURRENT per-invocation sysctl
//     KERN_PROC_VMMAP scan (mapscan.c) backs every dereference via
//     ig_map_cover(addr, len) FIRST, then a plain byte-loop dereference
//     into a stack copy. NO mprotect, NO raw syscall, NO write to game
//     memory, ever.
//   - Base derivation + admission (fc1pool_driver.c, predicate-identical
//     to qdrv1's admit()): anchor admission on the GUA header window
//     (rcodes 1-7) proves base := GUA_runtime - GUA_label; the pool is
//     only ever read at base + pool_label (0x9365ec0).
//   - Gate (qdrv1): the census arms only on an admitted header whose
//     NumElements rides strictly above the 10000 layout-gate threshold;
//     polls before that record refused_bindings_absent (pre-admission
//     refusals, the wiring-lag analog) / refused_layout_gate_closed.
//   - Control chain (fc1's validated chain, byte-identical predicates):
//     CurrentBlock >= 1 and sane, Blocks[0] nonzero/canonical/covered,
//     bootstrap ids distinct, "None" at pool+0x14040 decodes ASCII
//     (None_id_zero), "ByteProperty" at pool+0x14044 decodes ASCII.
//   - Census walk (NEW, the cc1 mission): for every pool block
//     [0, CurrentBlock) walk the mapped extent in 2-byte entry steps,
//     budget-bounded, matching ONLY the fixed 53-name table
//     (carrier_driver.h); wide entries are skipped and counted; matches
//     emit one census_match line each; progress marker every 4096 entries;
//     hard stop at budget with census_truncated:1.
//
// Fail-open: any check failure becomes a recorded terminal census refusal
// (CENSUS:REFUSED + reason_code, never re-rated); only the pre-census gate
// stages (map/anchor/count/pool-uninitialized) ride the bounded 30x4s
// window; module_start always returns 0.
#include "carrier_driver.h"

#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "ingame_abi.h"
#include "ingame_libc.h"

#include <stdint.h>
/* Lineage include order (v3 experiments.c / qh_driver.cpp): sys/types.h MUST
   precede sys/user.h-adjacent BSD headers on the pinned SDK. */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <time.h>

/* Host tests cannot map below 4 GiB (macOS arm64); the runtime anchor is
   overridable at compile time while every LABEL stays pinned. */
#ifndef CC1_RT_GUOBJECT_ARRAY
#define CC1_RT_GUOBJECT_ARRAY IG_RT_GUOBJECT_ARRAY
#endif

#ifndef CC1_HOST_TEST
#include <pthread.h>
#endif

/* ---- the census name table (provenance comments: carrier_driver.h) ---- */
#define K 0  /* candidate carrier */
#define F 1  /* ctor-family decoder control */
#define C 2  /* walk class-string */
#define P 3  /* positive control */
#define N 4  /* negative control */
const Cc1Name cc1_name_table[] = {
    {"ReceiveTick", K},
    {"BlueprintUpdateAnimation", K},
    {"Tick", K},
    {"K2_OnBecomeViewTarget", F},
    {"K2_OnEndViewTarget", F},
    {"K2_OnReset", F},
    {"ReceiveActorBeginCursorOver", F},
    {"ReceiveActorBeginOverlap", F},
    {"ReceiveActorEndCursorOver", F},
    {"ReceiveActorEndOverlap", F},
    {"ReceiveActorOnClicked", F},
    {"ReceiveActorOnInputTouchBegin", F},
    {"ReceiveActorOnInputTouchEnd", F},
    {"ReceiveActorOnInputTouchEnter", F},
    {"ReceiveActorOnInputTouchLeave", F},
    {"ReceiveActorOnReleased", F},
    {"ReceiveAnyDamage", F},
    {"ReceiveAsyncPhysicsTick", F},
    {"ReceiveBeginPlay", F},
    {"ReceiveDestroyed", F},
    {"ReceiveEndPlay", F},
    {"ReceiveHit", F},
    {"ReceivePointDamage", F},
    {"ReceiveRadialDamage", F},
    {"UserConstructionScript", F},
    {"BlueprintBeginPlay", F},
    {"BlueprintInitializeAnimation", F},
    {"BlueprintLinkedAnimationLayersInitialized", F},
    {"BlueprintPostEvaluateAnimation", F},
    {"BlueprintThreadSafeUpdateAnimation", F},
    {"Actor", C},
    {"Pawn", C},
    {"Character", C},
    {"PlayerController", C},
    {"GameModeBase", C},
    {"GameStateBase", C},
    {"GameViewportClient", C},
    {"HUD", C},
    {"PlayerCameraManager", C},
    {"WorldSettings", C},
    {"LevelScriptActor", C},
    {"AnimInstance", C},
    {"UserWidget", C},
    {"ItemDatabaseSubsystem", C},
    {"Info", C},
    {"Engine", C},
    {"PlayerState", C},
    {"Controller", C},
    {"CameraComponent", C},
    {"None", P},
    {"ByteProperty", P},
    {"PostRender", N},
    {"ZZZNotARealFunctionQQ", N},
};
const unsigned cc1_name_table_len =
    (unsigned)(sizeof(cc1_name_table) / sizeof(cc1_name_table[0]));
#undef K
#undef F
#undef C
#undef P
#undef N

/* ---- injectable ops seam. Target build: carrier_driver_start installs
   k_target_ops. Host tests (CC1_HOST_TEST): the test main assigns its own
   ops before calling carrier_driver_start(). ---- */
const Cc1Ops *g_ops;

#ifndef CC1_HOST_TEST

static int op_mapscan_run(void) { return ig_mapscan_run(); }
static int op_cover(uint64_t addr, uint64_t len, IgSpan *out) {
    return ig_map_cover(addr, len, out);
}
/* The igf6-proven read class: plain byte dereference of a span-checked
   known-mapped address. NO mprotect, NO raw syscall, NO write. */
static void op_deref(void *dst, uint64_t src, unsigned n) {
    const uint8_t *p = (const uint8_t *)(uintptr_t)src;
    uint8_t *q = (uint8_t *)dst;
    for (unsigned i = 0; i < n; ++i) q[i] = p[i];
}
static int op_sleep_sec(long sec) {
    struct timespec req, rem;
    req.tv_sec = sec;
    req.tv_nsec = 0;
    rem.tv_sec = 0;
    rem.tv_nsec = 0;
    return nanosleep((const struct timespec *)&req, &rem);
}
/* Exactly ONE UE4SS.log line per launch, at the terminal census verdict,
   through the lineage sceKernelOpen/Write/Close trio (O_APPEND|O_CREAT|
   O_NOFOLLOW, 0644 — the path_test/fc1 precedent). Returns 0 ok. */
static int op_log_line(const char *line, unsigned len) {
    int fd = sceKernelOpen(IG_UE4SS_LOG_PATH,
                           O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW,
                           (uint16_t)0644);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < len) {
        int64_t w = sceKernelWrite(fd, line + off, len - off);
        if (w <= 0 || (uint64_t)w > len - off) { sceKernelClose(fd); return -1; }
        off += (size_t)w;
    }
    return sceKernelClose(fd) == 0 ? 0 : -1;
}

static const Cc1Ops k_target_ops = {
    op_mapscan_run, op_cover, op_deref, op_sleep_sec, op_log_line};

#endif  /* !CC1_HOST_TEST */

/* ---- byte helpers (freestanding; no struct casts) ---- */
static uint32_t ld32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static uint64_t ld64(const uint8_t *p) {
    return (uint64_t)ld32(p) | ((uint64_t)ld32(p + 4) << 32);
}
/* User-half canonicality (cited: live_gua_reads.h igu_ptr_canonical). */
static int ptr_canonical(uint64_t p) { return (p >> 47) == 0; }
static int ptr_canonical_or_null(uint64_t p) {
    return p == 0 || ptr_canonical(p);
}

/* ---- per-poll state (one detached thread = single writer) ---- */
typedef struct Cc1State {
    IgSink *sink;
    unsigned seq;
    int map_trusted;
    int poll_admitted;
    int poll_rcode;
    uint32_t poll_num_elements;
    uint32_t poll_max_elements;
    uint32_t poll_max_chunks;
    uint32_t poll_num_chunks;
    int32_t poll_first_gc;
    int32_t poll_last_non_gc;
    int ever_admitted;
    unsigned refused_bindings_absent;    /* polls refused pre-admission */
    unsigned refused_layout_gate_closed; /* admitted, count <= threshold */
    int census_done;
    int census_validated;
    uint64_t census_entries;
    uint64_t census_markers;
    uint64_t matched_count;
    uint64_t wide_skipped;
    uint64_t negative_hits[2];           /* [PostRender, ZZZNotAReal..] */
    int census_truncated;
    int matches_overflowed;
    const char *pending_reason;          /* last blocking stage for the
                                            never-armed terminal verdict */
    unsigned polls_run;
    unsigned polls_admitted;
    int log_line_written;
    int log_line_rc;
    IgBuf buf;
} Cc1State;

static Cc1State g_st;

/* ---- GUA anchor admission (predicates identical to qdrv1 admit()) ---- */
enum {
    kAdmOk = 0,
    kAdmNotCovered = 1,
    kAdmNotReadable = 2,
    kAdmAmbiguousLayout = 3,
    kAdmPtrNonCanonical = 4,
    kAdmPtrsBothNull = 5,
    kAdmCountsInsane = 6,
    kAdmGcOrder = 7,
};

static int anchor_admit(uint64_t gua_rt, int *rcode, Cc1State *st) {
    IgSpan span;
    span.start = span.end = 0;
    span.prot = 0;
    *rcode = kAdmNotCovered;
    if (g_ops->cover(gua_rt, CC1_GUA_HEADER_BYTES, &span) != 1) return 0;
    if (!(span.prot & 1u)) { *rcode = kAdmNotReadable; return 0; }
    uint8_t hdr[CC1_GUA_HEADER_BYTES];
    for (unsigned i = 0; i < CC1_GUA_HEADER_BYTES; ++i) hdr[i] = 0;
    g_ops->deref(hdr, gua_rt, CC1_GUA_HEADER_BYTES);
    st->poll_first_gc = (int32_t)ld32(hdr + 0x00);
    st->poll_last_non_gc = (int32_t)ld32(hdr + 0x04);
    const uint64_t objects = ld64(hdr + 0x10);
    const uint64_t prealloc = ld64(hdr + 0x18);
    st->poll_max_elements = ld32(hdr + 0x20);
    st->poll_num_elements = ld32(hdr + 0x24);
    st->poll_max_chunks = ld32(hdr + 0x28);
    st->poll_num_chunks = ld32(hdr + 0x2C);
    /* Layout-B disambiguation (qdrv1: under layout A the first 8 bytes
       concatenate two small int32 fields and MUST fail canonicality). */
    if (ptr_canonical(ld64(hdr + 0x00))) {
        *rcode = kAdmAmbiguousLayout; return 0;
    }
    if (!ptr_canonical_or_null(objects) || !ptr_canonical_or_null(prealloc)) {
        *rcode = kAdmPtrNonCanonical; return 0;
    }
    if (objects == 0 && prealloc == 0) {
        *rcode = kAdmPtrsBothNull; return 0;
    }
    const int64_t num = (int64_t)st->poll_num_elements;
    const int64_t max_el = (int64_t)st->poll_max_elements;
    const int64_t max_ch = (int64_t)st->poll_max_chunks;
    const int64_t num_ch = (int64_t)st->poll_num_chunks;
    const int counts_ok =
        (num > 0) & (num <= max_el) & (max_ch >= 1) & (max_ch <= 512) &
        (num_ch >= 1) & (num_ch <= max_ch) & (max_el <= max_ch * 65536);
    if (!counts_ok) { *rcode = kAdmCountsInsane; return 0; }
    const int32_t fg = st->poll_first_gc;
    const int32_t lg = st->poll_last_non_gc;
    const int gc_ok = (fg > lg) & (fg >= 0) & (lg >= -1) &
                      ((int64_t)lg <= max_el);
    if (!gc_ok) { *rcode = kAdmGcOrder; return 0; }
    *rcode = kAdmOk;
    return 1;
}

/* ---- ASCII compare against a table name at entry+2 (fc1's decoder:
       header u16 bit0 wide, chars at +2, NUL required) ---- */
static int ascii_at(const uint8_t *win, unsigned win_len, const char *s) {
    unsigned n = 0;
    while (s[n] != 0) ++n;
    if (2u + n + 1u > win_len) return 0;
    for (unsigned i = 0; i < n; ++i)
        if (win[2 + i] != (uint8_t)s[i]) return 0;
    return win[2 + n] == 0;
}

static int flush_line(Cc1State *st) {
    if (ig_sink_write_record(st->sink, &st->buf) != 0) {
        st->sink->failed = 1;
        return -1;
    }
    return 0;
}

static void emit_counter(Cc1State *st, unsigned poll_idx, const char *phase) {
    IgBuf *b = &st->buf;
    ig_record_begin(b, (int)st->seq++, "qh_counter", 0, 0, IG_BUILD_ID);
    ig_kv_int(b, "poll", (int64_t)poll_idx);
    ig_kv_str(b, "phase", phase, 24);
    ig_kv_int(b, "map_trusted", (int64_t)st->map_trusted);
    ig_kv_int(b, "admitted", (int64_t)st->poll_admitted);
    ig_kv_int(b, "admission_rcode", (int64_t)st->poll_rcode);
    ig_kv_int(b, "gua_num_elements", (int64_t)st->poll_num_elements);
    ig_kv_int(b, "gua_max_elements", (int64_t)st->poll_max_elements);
    ig_kv_int(b, "gua_max_chunks", (int64_t)st->poll_max_chunks);
    ig_kv_int(b, "gua_num_chunks", (int64_t)st->poll_num_chunks);
    ig_kv_int(b, "gua_first_gc", (int64_t)st->poll_first_gc);
    ig_kv_int(b, "gua_last_non_gc", (int64_t)st->poll_last_non_gc);
    ig_kv_int(b, "refused_bindings_absent",
              (int64_t)st->refused_bindings_absent);
    ig_kv_int(b, "refused_layout_gate_closed",
              (int64_t)st->refused_layout_gate_closed);
    ig_kv_int(b, "census_entries_scanned", (int64_t)st->census_entries);
    ig_kv_int(b, "census_progress_markers", (int64_t)st->census_markers);
    ig_kv_int(b, "census_matched_so_far", (int64_t)st->matched_count);
    ig_record_flush(b);
    (void)flush_line(st);
}

/* ---- fc1's per-entry resolution (control chain) ---- */
struct EntryView {
    uint64_t block_ptr;
    uint64_t entry_addr;
    uint32_t header;
    uint32_t wide_flag;
    uint8_t bytes16[16];
};

static int resolve_entry(uint64_t pool_rt, uint32_t handle, uint32_t cur_block,
                         struct EntryView *ev, const char **reason) {
    const uint32_t blk = handle >> 16;
    const uint32_t off = handle & 0xFFFFu;
    if (blk >= cur_block || blk >= CC1_MAX_BLOCKS) {
        *reason = CC1_RC_HANDLE_OOB;
        return 0;
    }
    const uint64_t bp_addr = pool_rt + CC1_OFF_BLOCKS0 + 8ull * blk;
    IgSpan span;
    span.start = span.end = 0;
    span.prot = 0;
    if (g_ops->cover(bp_addr, 8, &span) != 1 || !(span.prot & 1u)) {
        *reason = CC1_RC_BLOCKPTR_UNCOVERED;
        return 0;
    }
    uint8_t raw[8];
    g_ops->deref(raw, bp_addr, 8);
    ev->block_ptr = ld64(raw);
    if (ev->block_ptr == 0 || !ptr_canonical(ev->block_ptr)) {
        *reason = CC1_RC_BLOCKPTR_INVALID;
        return 0;
    }
    ev->entry_addr = ev->block_ptr + (((uint64_t)off) << 1);
    span.start = span.end = 0;
    span.prot = 0;
    if (g_ops->cover(ev->entry_addr, CC1_ENTRY_WINDOW, &span) != 1 ||
        !(span.prot & 1u)) {
        *reason = CC1_RC_ENTRY_UNCOVERED;
        return 0;
    }
    uint8_t win[CC1_ENTRY_WINDOW];
    for (unsigned i = 0; i < CC1_ENTRY_WINDOW; ++i) win[i] = 0;
    g_ops->deref(win, ev->entry_addr, CC1_ENTRY_WINDOW);
    for (unsigned i = 0; i < 16; ++i) ev->bytes16[i] = win[i];
    ev->header = (uint32_t)win[0] | ((uint32_t)win[1] << 8);
    ev->wide_flag = ev->header & 1u;    /* bIsWide, bit0 */
    if (ev->wide_flag != 0) { *reason = CC1_RC_ENTRY_WIDE; return 0; }
    return 1;
}

/* ---- the census walk (the cc1 mission) ----
   Walks every pool block's mapped extent in 2-byte entry steps under the
   budget, matching only the fixed table. Returns 1 validated / 0 refused
   with *reason set. census-carrying counters land in st. */
static int census_run(Cc1State *st, uint64_t pool_rt, uint32_t cur_block,
                      uint64_t *census_none_hits, uint64_t *census_bp_hits,
                      const char **reason) {
    *census_none_hits = 0;
    *census_bp_hits = 0;
    static const char hd[] = "0123456789abcdef";
    for (uint32_t blk = 0; blk < cur_block; ++blk) {
        if (st->census_entries >= CC1_CENSUS_BUDGET_ENTRIES) {
            st->census_truncated = 1;
            break;
        }
        const uint64_t bp_addr = pool_rt + CC1_OFF_BLOCKS0 + 8ull * blk;
        IgSpan span;
        span.start = span.end = 0;
        span.prot = 0;
        if (g_ops->cover(bp_addr, 8, &span) != 1 || !(span.prot & 1u)) {
            *reason = CC1_RC_BLOCKPTR_UNCOVERED;
            return 0;
        }
        uint8_t raw[8];
        g_ops->deref(raw, bp_addr, 8);
        const uint64_t block_ptr = ld64(raw);
        if (block_ptr == 0 || !ptr_canonical(block_ptr)) {
            *reason = CC1_RC_CENSUS_BLOCKPTR;
            return 0;
        }
        for (uint32_t off = 0; off < 0x10000u;) {
            if (st->census_entries >= CC1_CENSUS_BUDGET_ENTRIES) {
                st->census_truncated = 1;
                break;
            }
            const uint64_t entry_addr = block_ptr + (((uint64_t)off) << 1);
            span.start = span.end = 0;
            span.prot = 0;
            if (g_ops->cover(entry_addr, CC1_ENTRY_WINDOW, &span) != 1 ||
                !(span.prot & 1u)) {
                if (off == 0) {
                    *reason = CC1_RC_CENSUS_ENTRY;
                    return 0;
                }
                break;  /* mapped extent of this block exhausted */
            }
            ++st->census_entries;
            if (st->census_entries % CC1_PROGRESS_EVERY == 0)
                ++st->census_markers;
            uint8_t win[CC1_ENTRY_WINDOW];
            for (unsigned i = 0; i < CC1_ENTRY_WINDOW; ++i) win[i] = 0;
            g_ops->deref(win, entry_addr, CC1_ENTRY_WINDOW);
            /* fc1-validated header decode (CANDIDATES sec 6.2): u16 header,
               bIsWide = bit0, Len = bits15:6 (chars at +2, ASCII entries). */
            const uint32_t header = (uint32_t)win[0] | ((uint32_t)win[1] << 8);
            const uint32_t len = (header >> 6) & 0x3FFu;
            uint32_t stride_units;  /* 2-byte offset units to the next entry */
            if (header & 1u) {  /* bIsWide: cannot carry an ASCII candidate */
                ++st->wide_skipped;
                stride_units = ((2u * len) + 3u) >> 1;  /* 2 + 2*Len bytes */
            } else {
                stride_units = (len + 3u) >> 1;         /* 2 + Len bytes */
                const uint32_t id = (blk << 16) | off;
                for (unsigned t = 0; t < cc1_name_table_len; ++t) {
                    if (!ascii_at(win, CC1_ENTRY_WINDOW, cc1_name_table[t].name))
                        continue;
                    if (cc1_name_table[t].kind == 4) {  /* negative control */
                        ++st->negative_hits[t == CC1_IDX_POSTRENDER ? 0 : 1];
                        break;
                    }
                    if (t == CC1_IDX_NONE) ++*census_none_hits;
                    if (t == CC1_IDX_BYTEPROPERTY) ++*census_bp_hits;
                    ++st->matched_count;
                    if (st->matched_count <= CC1_MAX_MATCHES) {
                        /* one census_match line per matched name */
                        IgBuf *b = &st->buf;
                        ig_record_begin(b, (int)st->seq++, "census_match", 0,
                                        0, IG_BUILD_ID);
                        ig_kv_str(b, "name", cc1_name_table[t].name,
                                  CC1_NAME_MAX);
                        ig_kv_int(b, "table_index", (int64_t)t);
                        ig_kv_int(b, "kind", (int64_t)cc1_name_table[t].kind);
                        ig_kv_int(b, "id", (int64_t)id);
                        ig_kv_int(b, "block", (int64_t)blk);
                        ig_kv_int(b, "offset", (int64_t)off);
                        ig_kv_hex(b, "block_ptr", block_ptr);
                        ig_kv_hex(b, "entry_addr", entry_addr);
                        {
                            char hex[33];
                            for (unsigned i = 0; i < 16; ++i) {
                                hex[i * 2] = hd[(win[i] >> 4) & 15];
                                hex[i * 2 + 1] = hd[win[i] & 15];
                            }
                            hex[32] = 0;
                            ig_kv_str(b, "bytes16", hex, 32);
                        }
                        ig_record_flush(b);
                        (void)flush_line(st);
                    }
                    break;  /* one table row per entry (exact-match) */
                }
            }
            /* stride entry-to-entry (never scan string bodies); a zero or
               insane header advances the minimum 2 bytes and the walk
               remains bounded by the per-block entry cap */
            if (stride_units == 0) stride_units = 1;
            off += stride_units;
        }
        if (st->census_truncated) break;
    }
    if (st->matched_count > CC1_MAX_MATCHES) st->matches_overflowed = 1;
    /* consistency gate: the census walk itself must have seen BOTH
       bootstrap strings. Redundant with the pre-census fc1 chain check
       (which gates first — see run_driver_loop), so a budget-truncated
       walk skips it: census_truncated=1 stays an honesty flag on a
       VALIDATED bounded census, never a silent coverage claim (the
       budget_entries/entries_scanned fields carry the real coverage). */
    if (st->census_truncated) return 1;
    if (*census_none_hits == 0) { *reason = CC1_RC_NONE_MISMATCH; return 0; }
    if (*census_bp_hits == 0) { *reason = CC1_RC_BP_MISMATCH; return 0; }
    return 1;
}

/* ---- the paced loop (body runs on the detached thread; host: inline) -- */
static void run_driver_loop(IgSink *sink) {
    Cc1State *st = &g_st;
    st->sink = sink;
    st->seq = 3;  /* 1 entry + 2 startup already written by module_start */
    st->map_trusted = 0;
    st->poll_admitted = 0;
    st->poll_rcode = kAdmNotCovered;
    st->poll_num_elements = 0;
    st->poll_max_elements = 0;
    st->poll_max_chunks = 0;
    st->poll_num_chunks = 0;
    st->poll_first_gc = 0;
    st->poll_last_non_gc = 0;
    st->ever_admitted = 0;
    st->refused_bindings_absent = 0;
    st->refused_layout_gate_closed = 0;
    st->census_done = 0;
    st->census_validated = 0;
    st->census_entries = 0;
    st->census_markers = 0;
    st->matched_count = 0;
    st->wide_skipped = 0;
    st->negative_hits[0] = 0;
    st->negative_hits[1] = 0;
    st->census_truncated = 0;
    st->matches_overflowed = 0;
    st->pending_reason = CC1_RC_MAP_UNTRUSTED;
    st->polls_run = 0;
    st->polls_admitted = 0;
    st->log_line_written = 0;
    st->log_line_rc = -1;

    const uint64_t gua_rt = (uint64_t)CC1_RT_GUOBJECT_ARRAY;
    int base_ok_bias = 0;
    uint64_t base = 0;
    uint64_t pool_rt = 0;
    int none_ok = 0;
    int bp_ok = 0;
    const char *census_reason = "";
    struct EntryView ev_none;
    struct EntryView ev_bp;
    ig_memset(&ev_none, 0, sizeof ev_none);
    ig_memset(&ev_bp, 0, sizeof ev_bp);
    uint32_t h_none = 0;
    uint32_t h_bp = 0;
    uint32_t cur_block = 0;
    uint32_t cur_bo = 0;
    uint64_t blocks0 = 0;

    /* seq 3: map/context record (CANDIDATES sec 6.3 shape; current_block
       is pre-admission 0 here — the live value rides census_result). */
    st->map_trusted = g_ops->mapscan_run();
    {
        IgBuf *b = &st->buf;
        ig_record_begin(b, (int)st->seq++, "mapscan", 0, 0, IG_BUILD_ID);
        ig_kv_int(b, "trusted", (int64_t)st->map_trusted);
        ig_kv_str(b, "pool_probe", "0x9365ec0", 16);
        ig_kv_int(b, "current_block", 0);
        ig_kv_hex(b, "gua_rt", gua_rt);
        ig_kv_str(b, "note", "pre-admission context; current_block=0 until "
                             "the gate opens", 56);
        ig_record_flush(b);
        (void)flush_line(st);
    }

    for (unsigned poll = 0; poll < CC1_POLL_COUNT; ++poll) {
        if (poll != 0) (void)g_ops->sleep_sec(CC1_POLL_SECONDS);
        ++st->polls_run;
        st->map_trusted = g_ops->mapscan_run();
        if (!st->map_trusted) {
            st->poll_admitted = 0;
            st->poll_rcode = kAdmNotCovered;
            st->pending_reason = CC1_RC_MAP_UNTRUSTED;
            emit_counter(st, poll, "bindings_absent");
            continue;
        }
        const int admitted = anchor_admit(gua_rt, &st->poll_rcode, st);
        st->poll_admitted = admitted;
        if (!admitted) {
            ++st->refused_bindings_absent;
            st->pending_reason = CC1_RC_ANCHOR_NOT_ADMITTED;
            emit_counter(st, poll, "bindings_absent");
            continue;
        }
        st->ever_admitted = 1;
        ++st->polls_admitted;
        if ((int64_t)st->poll_num_elements <= CC1_GATE_THRESHOLD) {
            ++st->refused_layout_gate_closed;
            st->pending_reason = CC1_RC_GATE_NEVER_OPENED;
            emit_counter(st, poll, "gate_closed");
            continue;
        }
        /* ---- gate OPEN: derive base, validate the pool controls, and
           run the census ONCE (never re-rated after a hard refusal) ---- */
        base = gua_rt - (uint64_t)IG_VADDR_GUOBJECT_ARRAY;
        base_ok_bias = (base == (uint64_t)IG_EXPECTED_BASE) ? 1 : 0;
        pool_rt = base + (uint64_t)IG_VADDR_FNAME_POOL;
        if (st->census_done) {
            emit_counter(st, poll, "terminal");
            continue;
        }
        /* pool header window (fc1 chain; hard refusals terminal) */
        IgSpan pspan;
        pspan.start = pspan.end = 0;
        pspan.prot = 0;
        if (g_ops->cover(pool_rt, CC1_POOL_WINDOW, &pspan) != 1) {
            st->census_done = 1;
            census_reason = CC1_RC_POOL_UNCOVERED;
            emit_counter(st, poll, "terminal");
            continue;
        }
        if (!(pspan.prot & 1u)) {
            st->census_done = 1;
            census_reason = CC1_RC_POOL_UNREADABLE;
            emit_counter(st, poll, "terminal");
            continue;
        }
        uint8_t hdr[CC1_POOL_WINDOW];
        for (unsigned i = 0; i < CC1_POOL_WINDOW; ++i) hdr[i] = 0;
        g_ops->deref(hdr, pool_rt, CC1_POOL_WINDOW);
        cur_block = ld32(hdr + CC1_OFF_CURRENT_BLOCK);
        cur_bo = ld32(hdr + CC1_OFF_CURRENT_BO);
        blocks0 = ld64(hdr + CC1_OFF_BLOCKS0);
        if (cur_block == 0) {
            /* transient fc1-class: pool init lags the GUA count */
            st->pending_reason = CC1_RC_UNINITIALIZED;
            emit_counter(st, poll, "gate_closed");
            continue;
        }
        if (cur_block > CC1_MAX_BLOCKS) {
            st->census_done = 1;
            census_reason = CC1_RC_CB_INSANE;
            emit_counter(st, poll, "terminal");
            continue;
        }
        if (blocks0 == 0 || !ptr_canonical(blocks0)) {
            st->census_done = 1;
            census_reason = CC1_RC_BLOCKS0_INVALID;
            emit_counter(st, poll, "terminal");
            continue;
        }
        {
            IgSpan b0span;
            b0span.start = b0span.end = 0;
            b0span.prot = 0;
            if (g_ops->cover(blocks0, 8, &b0span) != 1 ||
                !(b0span.prot & 1u)) {
                st->census_done = 1;
                census_reason = CC1_RC_BLOCKS0_UNCOVERED;
                emit_counter(st, poll, "terminal");
                continue;
            }
        }
        /* bootstrap ids window + distinctness (fc1 chain) */
        {
            IgSpan hspan;
            hspan.start = hspan.end = 0;
            hspan.prot = 0;
            if (g_ops->cover(pool_rt + CC1_OFF_ID_NONE, CC1_HANDLES_WINDOW,
                             &hspan) != 1 || !(hspan.prot & 1u)) {
                st->census_done = 1;
                census_reason = CC1_RC_HANDLES_UNCOVERED;
                emit_counter(st, poll, "terminal");
                continue;
            }
            uint8_t hw[CC1_HANDLES_WINDOW];
            g_ops->deref(hw, pool_rt + CC1_OFF_ID_NONE, CC1_HANDLES_WINDOW);
            h_none = ld32(hw);
            h_bp = ld32(hw + 4);
            if (h_bp == h_none) {
                st->census_done = 1;
                census_reason = CC1_RC_HANDLES_DEGENERATE;
                emit_counter(st, poll, "terminal");
                continue;
            }
        }
        /* resolve both control entries through Blocks[] (fc1 chain) and
           ASCII-verify them BEFORE the census (fc1 checks C7-C10) */
        {
            const char *r = "";
            if (!resolve_entry(pool_rt, h_none, cur_block, &ev_none, &r)) {
                st->census_done = 1;
                census_reason = r;
                emit_counter(st, poll, "terminal");
                continue;
            }
            if (!resolve_entry(pool_rt, h_bp, cur_block, &ev_bp, &r)) {
                st->census_done = 1;
                census_reason = r;
                emit_counter(st, poll, "terminal");
                continue;
            }
            uint8_t wnone[CC1_ENTRY_WINDOW];
            uint8_t wbp[CC1_ENTRY_WINDOW];
            for (unsigned i = 0; i < CC1_ENTRY_WINDOW; ++i) {
                wnone[i] = 0;
                wbp[i] = 0;
            }
            g_ops->deref(wnone, ev_none.entry_addr, CC1_ENTRY_WINDOW);
            g_ops->deref(wbp, ev_bp.entry_addr, CC1_ENTRY_WINDOW);
            none_ok = ascii_at(wnone, CC1_ENTRY_WINDOW, "None");
            bp_ok = ascii_at(wbp, CC1_ENTRY_WINDOW, "ByteProperty");
            if (!none_ok) {
                st->census_done = 1;
                census_reason = CC1_RC_NONE_MISMATCH;
                emit_counter(st, poll, "terminal");
                continue;
            }
            if (!bp_ok) {
                st->census_done = 1;
                census_reason = CC1_RC_BP_MISMATCH;
                emit_counter(st, poll, "terminal");
                continue;
            }
        }
        /* THE CENSUS (bounded; matches emit census_match lines) */
        {
            const char *r = "";
            uint64_t census_none = 0;
            uint64_t census_bp = 0;
            const int ok = census_run(st, pool_rt, cur_block, &census_none,
                                      &census_bp, &r);
            st->census_done = 1;
            st->census_validated = ok;
            census_reason = ok ? "none" : r;
        }
        emit_counter(st, poll, st->census_validated ? "census" : "terminal");
    }

    /* ---- terminal verdict: census_result ---- */
    const char *verdict;
    const char *reason;
    if (st->census_done && st->census_validated) {
        verdict = "CENSUS:VALIDATED";
        reason = "none";
    } else if (st->census_done) {
        verdict = "CENSUS:REFUSED";
        reason = census_reason[0] ? census_reason : "none";
    } else {
        /* the gate never opened inside the bounded window: the pending
           reason is the FINAL poll's blocking stage (grace-class) */
        verdict = "CENSUS:REFUSED";
        reason = st->pending_reason[0] ? st->pending_reason : "none";
    }
    {
        IgBuf *b = &st->buf;
        ig_record_begin(b, (int)st->seq++, "census_result", 0, 0, IG_BUILD_ID);
        ig_kv_str(b, "verdict", verdict, 24);
        ig_kv_str(b, "reason_code", reason, 40);
        ig_kv_int(b, "base_matches_observed_bias", (int64_t)base_ok_bias);
        ig_kv_hex(b, "base_derived", st->ever_admitted ? base : 0);
        ig_kv_hex(b, "pool_runtime", st->ever_admitted ? pool_rt : 0);
        ig_kv_int(b, "current_block", (int64_t)cur_block);
        ig_kv_int(b, "current_byte_offset", (int64_t)cur_bo);
        ig_kv_hex(b, "blocks0", blocks0);
        ig_kv_int(b, "id_none", (int64_t)h_none);
        ig_kv_int(b, "id_byteproperty", (int64_t)h_bp);
        /* controls object (CANDIDATES sec 6.3 shape) */
        ig_kv_key(b, "controls");
        ig_obj_open(b);
        ig_kv_int(b, "None_id_zero",
                  (int64_t)((h_none == 0 && none_ok) ? 1 : 0));
        ig_kv_int(b, "ByteProperty_ascii_ok", (int64_t)(bp_ok ? 1 : 0));
        ig_obj_close(b);
        /* negative_hits object */
        ig_kv_key(b, "negative_hits");
        ig_obj_open(b);
        ig_kv_int(b, "PostRender", (int64_t)st->negative_hits[0]);
        ig_kv_int(b, "ZZZNotARealFunctionQQ", (int64_t)st->negative_hits[1]);
        ig_obj_close(b);
        ig_kv_int(b, "matched_count", (int64_t)st->matched_count);
        ig_kv_int(b, "matches_overflowed", (int64_t)st->matches_overflowed);
        ig_kv_int(b, "wide_entries_skipped", (int64_t)st->wide_skipped);
        ig_kv_int(b, "census_entries_scanned", (int64_t)st->census_entries);
        ig_kv_int(b, "census_progress_markers", (int64_t)st->census_markers);
        ig_kv_int(b, "census_truncated", (int64_t)st->census_truncated);
        ig_kv_int(b, "budget_entries", (int64_t)CC1_CENSUS_BUDGET_ENTRIES);
        ig_record_flush(b);
        (void)flush_line(st);
    }

    /* ---- the ONE UE4SS.log line (terminal verdict) ---- */
    {
        char line[224];
        unsigned pos = 0;
        const char *part = "CC1: build=" IG_BUILD_ID " verdict=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        for (unsigned i = 0; verdict[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = verdict[i];
        part = " reason=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        for (unsigned i = 0; reason[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = reason[i];
        part = " entries=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        {
            char tmp[24];
            unsigned n = 0;
            uint64_t v = st->census_entries;
            if (v == 0) tmp[n++] = '0';
            while (v > 0 && n < sizeof tmp) {
                tmp[n++] = (char)('0' + (v % 10));
                v /= 10;
            }
            while (n > 0 && pos < sizeof(line) - 1) line[pos++] = tmp[--n];
        }
        part = " matches=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        {
            char tmp[24];
            unsigned n = 0;
            uint64_t v = st->matched_count;
            if (v == 0) tmp[n++] = '0';
            while (v > 0 && n < sizeof tmp) {
                tmp[n++] = (char)('0' + (v % 10));
                v /= 10;
            }
            while (n > 0 && pos < sizeof(line) - 1) line[pos++] = tmp[--n];
        }
        if (pos < sizeof(line) - 1) line[pos++] = '\n';
        line[pos] = 0;
        if (g_ops->log_line(line, pos) == 0) {
            st->log_line_written = 1;
        } else {
            st->log_line_rc = -1;
        }
    }

    /* ---- summary record ---- */
    {
        IgBuf *b = &st->buf;
        ig_record_begin(b, (int)st->seq++, "summary", 0, 0, IG_BUILD_ID);
        ig_kv_int(b, "polls_run", (int64_t)st->polls_run);
        ig_kv_int(b, "polls_admitted", (int64_t)st->polls_admitted);
        ig_kv_int(b, "census_done", (int64_t)st->census_done);
        ig_kv_int(b, "census_validated", (int64_t)st->census_validated);
        ig_kv_int(b, "matched_count", (int64_t)st->matched_count);
        ig_kv_int(b, "write_failures", (int64_t)st->sink->failed);
        ig_kv_int(b, "ue4ss_log_line_written", (int64_t)st->log_line_written);
        ig_kv_int(b, "ue4ss_log_line_rc", (int64_t)st->log_line_rc);
        ig_record_flush(b);
        (void)flush_line(st);
    }
}

/* ---- target thread body (plain C function; NO lambda, NO mutex) ---- */
#ifndef CC1_HOST_TEST
static void *cc1_thread_main(void *arg) {
    IgSink *s = (IgSink *)arg;
    run_driver_loop(s);
    ig_sink_close(s);
    return (void *)0;
}
#endif

int carrier_driver_start(IgSink *sink) {
#ifndef CC1_HOST_TEST
    g_ops = &k_target_ops;
    pthread_t thread = 0;
    const int rc = pthread_create(&thread, 0, cc1_thread_main, (void *)sink);
    if (rc != 0) return rc;
    if (thread == 0) return -1;
    (void)pthread_detach(thread);
    return 0;
#else
    /* host tests run the loop synchronously on the caller's thread */
    run_driver_loop(sink);
    return 0;
#endif
}
