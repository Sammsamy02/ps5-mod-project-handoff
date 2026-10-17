// fc1pool_driver.c — worker-fc1-validator-probe (fc1pool1, team 2026-10-04).
// NEW FILE. See fc1pool_driver.h for the chain and constants.
//
// Read mechanism (the ONLY foreign-memory access class, inherited from the
// igf6/qdrv1 proven pattern): a CURRENT per-invocation sysctl
// KERN_PROC_VMMAP scan (mapscan.c) backs every dereference via
// ig_map_cover(addr, len) FIRST, then a plain byte-loop dereference into a
// stack copy. NO mprotect, NO raw syscall, NO write to game memory, ever.
//
// Base derivation: the anchor admission (GUA header window, qdrv1 predicates
// rcodes 1-7 byte-identical semantics) proves the pinned runtime GUA is the
// live FUObjectArray; base := GUA_runtime - GUA_label. The probe then reads
// the pool ONLY at base + pool_label. base == 0x400000 is the expected
// single-load bias; a divergent base is still validated (the reads stay
// map-gated) but reported with base_matches_observed_bias=0 so the
// controller can reconcile before the initializer arms.
//
// Fail-open: any check failure becomes a recorded terminal (or grace-limited)
// refusal; nothing is retried except the transient stages, nothing is
// guessed through, module_start always returns 0.
#include "fc1pool_driver.h"

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
#ifndef FC1_RT_GUOBJECT_ARRAY
#define FC1_RT_GUOBJECT_ARRAY IG_RT_GUOBJECT_ARRAY
#endif

/* Bootstrap strings (exact expected ASCII at entry+2, NUL included). */
static const char kStrNone[] = "None";
static const char kStrByteProp[] = "ByteProperty";

/* Injectable ops seam. Target build: fc1pool_driver_run() installs
   k_target_ops. Host tests (FC1POOL_HOST_TEST): the test main assigns its
   own ops here before calling fc1pool_driver_run(). */
const Fc1Ops *g_ops;

#ifndef FC1POOL_HOST_TEST

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
static int op_sleep_ms(long ms) {
    struct timespec req, rem;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    rem.tv_sec = 0;
    rem.tv_nsec = 0;
    return nanosleep((const struct timespec *)&req, &rem);
}
/* Exactly ONE UE4SS.log line per launch, at the terminal verdict, through
   the lineage sceKernelOpen/Write/Close trio (O_APPEND|O_CREAT|O_NOFOLLOW,
   0644 — the path_test precedent). Returns 0 ok. */
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

static const Fc1Ops k_target_ops = {
    op_mapscan_run, op_cover, op_deref, op_sleep_ms, op_log_line};

#endif  /* !FC1POOL_HOST_TEST */

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

static int anchor_admit(uint64_t gua_rt, int *rcode) {
    IgSpan span;
    span.start = span.end = 0;
    span.prot = 0;
    *rcode = kAdmNotCovered;
    if (g_ops->cover(gua_rt, 96, &span) != 1) return 0;              /* 1 */
    if (!(span.prot & 1u)) { *rcode = kAdmNotReadable; return 0; }   /* 2 */
    uint8_t hdr[96];
    for (unsigned i = 0; i < 96; ++i) hdr[i] = 0;
    g_ops->deref(hdr, gua_rt, 96);
    int32_t first_gc = (int32_t)ld32(hdr + 0x00);
    int32_t last_non_gc = (int32_t)ld32(hdr + 0x04);
    uint64_t objects = ld64(hdr + 0x10);
    uint64_t prealloc = ld64(hdr + 0x18);
    uint32_t max_elements = ld32(hdr + 0x20);
    uint32_t num_elements = ld32(hdr + 0x24);
    uint32_t max_chunks = ld32(hdr + 0x28);
    uint32_t num_chunks = ld32(hdr + 0x2C);
    /* Layout-B disambiguation (qdrv1: under layout A the first 8 bytes
       concatenate two small int32 fields and MUST fail canonicality). */
    if (ptr_canonical(ld64(hdr + 0x00))) {                           /* 3 */
        *rcode = kAdmAmbiguousLayout; return 0;
    }
    if (!ptr_canonical_or_null(objects) || !ptr_canonical_or_null(prealloc)) {
        *rcode = kAdmPtrNonCanonical; return 0;                      /* 4 */
    }
    if (objects == 0 && prealloc == 0) {
        *rcode = kAdmPtrsBothNull; return 0;                         /* 5 */
    }
    const int64_t num = (int64_t)num_elements;
    const int64_t max_el = (int64_t)max_elements;
    const int64_t max_ch = (int64_t)max_chunks;
    const int64_t num_ch = (int64_t)num_chunks;
    const int counts_ok =
        (num > 0) & (num <= max_el) & (max_ch >= 1) & (max_ch <= 512) &
        (num_ch >= 1) & (num_ch <= max_ch) & (max_el <= max_ch * 65536);
    if (!counts_ok) { *rcode = kAdmCountsInsane; return 0; }         /* 6 */
    const int32_t fg = first_gc;
    const int32_t lg = last_non_gc;
    const int gc_ok = (fg > lg) & (fg >= 0) & (lg >= -1) & ((int64_t)lg <= max_el);
    if (!gc_ok) { *rcode = kAdmGcOrder; return 0; }                  /* 7 */
    *rcode = kAdmOk;
    return 1;
}

/* ---- per-entry resolution (chain step 3) ----
   On failure returns 0 with *reason set; on success fills the out fields. */
struct EntryView {
    uint64_t block_ptr;
    uint64_t entry_addr;
    uint32_t header;
    uint32_t wide_flag;
    uint32_t ascii_ok;
    uint8_t bytes16[16];   /* first 16 raw bytes at the entry address */
    IgSpan block_span;
    IgSpan entry_span;
};

static int resolve_entry(uint64_t pool_rt, uint32_t handle, uint32_t cur_block,
                         struct EntryView *ev, const char **reason) {
    const uint32_t blk = handle >> 16;
    const uint32_t off = handle & 0xFFFFu;
    if (blk >= cur_block || blk >= FC1_MAX_BLOCKS) {
        *reason = FC1_RC_HANDLE_OOB;
        return 0;
    }
    const uint64_t bp_addr = pool_rt + FC1_OFF_BLOCKS0 + 8ull * blk;
    IgSpan span;
    span.start = span.end = 0;
    span.prot = 0;
    if (g_ops->cover(bp_addr, 8, &span) != 1) {
        *reason = FC1_RC_BLOCKPTR_UNCOVERED;
        return 0;
    }
    if (!(span.prot & 1u)) { *reason = FC1_RC_BLOCKPTR_UNCOVERED; return 0; }
    uint8_t raw[8];
    g_ops->deref(raw, bp_addr, 8);
    ev->block_ptr = ld64(raw);
    ev->block_span = span;
    if (ev->block_ptr == 0 || !ptr_canonical(ev->block_ptr)) {
        *reason = FC1_RC_BLOCKPTR_INVALID;
        return 0;
    }
    ev->entry_addr = ev->block_ptr + (((uint64_t)off) << 1);
    span.start = span.end = 0;
    span.prot = 0;
    if (g_ops->cover(ev->entry_addr, FC1_ENTRY_WINDOW, &span) != 1) {
        *reason = FC1_RC_ENTRY_UNCOVERED;
        return 0;
    }
    if (!(span.prot & 1u)) { *reason = FC1_RC_ENTRY_UNCOVERED; return 0; }
    ev->entry_span = span;
    uint8_t win[FC1_ENTRY_WINDOW];
    for (unsigned i = 0; i < FC1_ENTRY_WINDOW; ++i) win[i] = 0;
    g_ops->deref(win, ev->entry_addr, FC1_ENTRY_WINDOW);
    for (unsigned i = 0; i < 16; ++i) ev->bytes16[i] = win[i];
    ev->header = ld32(win);          /* low u16 is the header word */
    ev->wide_flag = ev->header & 1u; /* bIsWide, bit0 of the header u16 */
    if (ev->wide_flag != 0) { *reason = FC1_RC_ENTRY_WIDE; return 0; }
    return 1;
}

static int ascii_at(const uint8_t *win, unsigned win_len, const char *s) {
    unsigned n = 0;
    while (s[n] != 0) ++n;
    if (2u + n + 1u > win_len) return 0;
    for (unsigned i = 0; i < n; ++i)
        if (win[2 + i] != (uint8_t)s[i]) return 0;
    return win[2 + n] == 0;  /* NUL terminator required */
}

/* record the 16 raw entry bytes as a hex string into the kv buffer */
static void kv_bytes16(IgBuf *b, const char *key, const uint8_t *bytes) {
    char hex[33];
    static const char hd[] = "0123456789abcdef";
    for (unsigned i = 0; i < 16; ++i) {
        hex[i * 2] = hd[(bytes[i] >> 4) & 15];
        hex[i * 2 + 1] = hd[bytes[i] & 15];
    }
    hex[32] = 0;
    ig_kv_str(b, key, hex, 32);
}

int fc1pool_driver_run(IgSink *sink) {
#ifndef FC1POOL_HOST_TEST
    g_ops = &k_target_ops;
#endif
    unsigned seq = 3;
    unsigned polls_run = 0;
    int terminal = 0;
    const char *reason = "none";
    const char *verdict = "";
    int validated = 0;
    int anchor_rcode = -1;
    int map_trusted = 0;
    uint64_t base = 0;
    int base_ok_bias = -1;     /* -1 untested, 0 divergent, 1 == 0x400000 */
    int admitted = 0;
    unsigned checks_passed = 0;
    const unsigned checks_total = 11;
    int log_line_rc = -1;
    int log_line_written = 0;

    /* Captured evidence (from the terminal poll). */
    uint64_t pool_rt = 0;
    uint32_t guard_byte = 0;
    int guard_read = 0;
    uint32_t cur_block = 0;
    uint32_t cur_bo = 0;
    uint64_t blocks0 = 0;
    int blocks0_canon = 0;
    uint32_t h_none = 0;
    uint32_t h_bp = 0;
    struct EntryView ev_none;
    struct EntryView ev_bp;
    int none_ok = 0;
    int bp_ok = 0;
    IgSpan pool_span;
    pool_span.start = pool_span.end = 0;
    pool_span.prot = 0;
    IgSpan pool_hdr_span;   /* covering span of [pool, pool+0x20), reported */
    pool_hdr_span.start = pool_hdr_span.end = 0;
    pool_hdr_span.prot = 0;

    ig_memset(&ev_none, 0, sizeof ev_none);
    ig_memset(&ev_bp, 0, sizeof ev_bp);

    const uint64_t gua_rt = (uint64_t)FC1_RT_GUOBJECT_ARRAY;

    while (!terminal && polls_run < FC1_GRACE_POLLS) {
        ++polls_run;
        map_trusted = g_ops->mapscan_run();
        if (!map_trusted) {
            reason = FC1_RC_MAP_UNTRUSTED;
            /* transient: map collection can fail early; wait and rescan */
            if (polls_run < FC1_GRACE_POLLS) (void)g_ops->sleep_ms(FC1_GRACE_SLEEP_MS);
            continue;
        }
        admitted = anchor_admit(gua_rt, &anchor_rcode);
        if (!admitted) {
            reason = FC1_RC_ANCHOR_NOT_ADMITTED;
            if (polls_run < FC1_GRACE_POLLS) (void)g_ops->sleep_ms(FC1_GRACE_SLEEP_MS);
            continue;
        }
        /* ---- anchor admitted: derive base and pool address ---- */
        base = gua_rt - (uint64_t)IG_VADDR_GUOBJECT_ARRAY;
        base_ok_bias = (base == (uint64_t)IG_EXPECTED_BASE) ? 1 : 0;
        pool_rt = base + (uint64_t)IG_VADDR_FNAME_POOL;

        /* Per-poll envelope (light): one line per attempted poll. */
        IgBuf pb;
        ig_record_begin(&pb, (int)seq, "poll", 0, 0, IG_BUILD_ID);
        ig_kv_int(&pb, "poll", (int64_t)polls_run);
        ig_kv_str(&pb, "phase", "anchor_admitted", 32);
        ig_kv_int(&pb, "map_trusted", (int64_t)map_trusted);
        ig_kv_int(&pb, "anchor_rcode", (int64_t)anchor_rcode);
        ig_kv_hex(&pb, "base_derived", base);
        ig_kv_hex(&pb, "pool_runtime", pool_rt);
        ig_kv_int(&pb, "base_matches_observed_bias", (int64_t)base_ok_bias);
        ig_record_flush(&pb);
        if (ig_sink_write_record(sink, &pb) != 0) sink->failed = 1;
        ++seq;
        break;  /* anchor stage done; fall through to the pool stage */
    }

    if (admitted) {
        /* ---- pool stage (reached at most ONCE; hard refusals terminal,
           only CurrentBlock==0 uses the remaining grace) ---- */
        for (;;) {
            /* Guard byte at pool-0x20 (evidence only; never a gate). */
            const uint64_t guard_addr = base + (uint64_t)IG_VADDR_INIT_FLAG;
            IgSpan gspan;
            gspan.start = gspan.end = 0;
            gspan.prot = 0;
            if (g_ops->cover(guard_addr, 1, &gspan) == 1 && (gspan.prot & 1u)) {
                uint8_t gb = 0;
                g_ops->deref(&gb, guard_addr, 1);
                guard_byte = gb;
                guard_read = 1;
            }

            if (g_ops->cover(pool_rt, FC1_POOL_WINDOW, &pool_span) != 1) {
                reason = FC1_RC_POOL_UNCOVERED;
                terminal = 1;
                break;
            }
            if (!(pool_span.prot & 1u)) {
                reason = FC1_RC_POOL_UNREADABLE;
                terminal = 1;
                break;
            }
            pool_hdr_span = pool_span;  /* evidence: the header covering span */
            uint8_t hdr[FC1_POOL_WINDOW];
            for (unsigned i = 0; i < FC1_POOL_WINDOW; ++i) hdr[i] = 0;
            g_ops->deref(hdr, pool_rt, FC1_POOL_WINDOW);
            cur_block = ld32(hdr + FC1_OFF_CURRENT_BLOCK);
            cur_bo = ld32(hdr + FC1_OFF_CURRENT_BO);
            blocks0 = ld64(hdr + FC1_OFF_BLOCKS0);
            blocks0_canon = ptr_canonical(blocks0);

            if (cur_block == 0) {
                /* engine FName pool not initialized yet: transient */
                if (polls_run < FC1_GRACE_POLLS) {
                    (void)g_ops->sleep_ms(FC1_GRACE_SLEEP_MS);
                    ++polls_run;
                    map_trusted = g_ops->mapscan_run();
                    if (!map_trusted) { reason = FC1_RC_MAP_UNTRUSTED; terminal = 1; break; }
                    admitted = anchor_admit(gua_rt, &anchor_rcode);
                    if (!admitted) { reason = FC1_RC_ANCHOR_NOT_ADMITTED; terminal = 1; break; }
                    continue;  /* re-read the pool header on the fresh map */
                }
                reason = FC1_RC_UNINITIALIZED;
                terminal = 1;
                break;
            }
            ++checks_passed;  /* C1: CurrentBlock >= 1 */

            if (cur_block > FC1_MAX_BLOCKS) {
                reason = FC1_RC_CB_INSANE;
                terminal = 1;
                break;
            }
            ++checks_passed;  /* C2: CurrentBlock sane */

            if (blocks0 == 0 || !blocks0_canon) {
                reason = FC1_RC_BLOCKS0_INVALID;
                terminal = 1;
                break;
            }
            ++checks_passed;  /* C3: Blocks[0] nonzero + canonical */
            if (g_ops->cover(blocks0, 8, &pool_span) != 1 ||
                !(pool_span.prot & 1u)) {
                reason = FC1_RC_BLOCKS0_UNCOVERED;
                terminal = 1;
                break;
            }
            pool_span.start = pool_span.end = 0;
            pool_span.prot = 0;
            ++checks_passed;  /* C4: Blocks[0] heap pointer in a readable span */

            /* bootstrap ids window */
            IgSpan hspan;
            hspan.start = hspan.end = 0;
            hspan.prot = 0;
            if (g_ops->cover(pool_rt + FC1_OFF_ID_NONE, FC1_HANDLES_WINDOW,
                             &hspan) != 1) {
                reason = FC1_RC_HANDLES_UNCOVERED;
                terminal = 1;
                break;
            }
            if (!(hspan.prot & 1u)) {
                reason = FC1_RC_HANDLES_UNCOVERED;
                terminal = 1;
                break;
            }
            uint8_t hw[FC1_HANDLES_WINDOW];
            g_ops->deref(hw, pool_rt + FC1_OFF_ID_NONE, FC1_HANDLES_WINDOW);
            h_none = ld32(hw);
            h_bp = ld32(hw + 4);
            ++checks_passed;  /* C5: bootstrap-id window read */
            if (h_bp == h_none) {
                reason = FC1_RC_HANDLES_DEGENERATE;
                terminal = 1;
                break;
            }
            ++checks_passed;  /* C6: ids distinct */

            const char *r = "";
            if (!resolve_entry(pool_rt, h_none, cur_block, &ev_none, &r)) {
                reason = r;
                terminal = 1;
                break;
            }
            ++checks_passed;  /* C7: "None" id resolves (block+entry windows) */
            if (!resolve_entry(pool_rt, h_bp, cur_block, &ev_bp, &r)) {
                reason = r;
                terminal = 1;
                break;
            }
            ++checks_passed;  /* C8: "ByteProperty" id resolves */

            uint8_t wnone[FC1_ENTRY_WINDOW];
            uint8_t wbp[FC1_ENTRY_WINDOW];
            for (unsigned i = 0; i < FC1_ENTRY_WINDOW; ++i) {
                wnone[i] = 0;
                wbp[i] = 0;
            }
            g_ops->deref(wnone, ev_none.entry_addr, FC1_ENTRY_WINDOW);
            g_ops->deref(wbp, ev_bp.entry_addr, FC1_ENTRY_WINDOW);
            none_ok = ascii_at(wnone, FC1_ENTRY_WINDOW, kStrNone);
            bp_ok = ascii_at(wbp, FC1_ENTRY_WINDOW, kStrByteProp);
            if (!none_ok) { reason = FC1_RC_NONE_MISMATCH; terminal = 1; break; }
            ++checks_passed;  /* C9: entry 0 string == "None" */
            if (!bp_ok) { reason = FC1_RC_BP_MISMATCH; terminal = 1; break; }
            ++checks_passed;  /* C10: +0x14044 id string == "ByteProperty" */

            validated = 1;
            ++checks_passed;  /* C11: whole chain holds */
            terminal = 1;
            break;
        }
    }

    /* verdict classification: a TRANSIENT refusal (map/anchor/uninitialized)
       consumed the bounded grace -> refused_after_grace; any hard pool-stage
       refusal -> refused; all 11 checks -> validated. */
    if (validated) {
        verdict = "validated";
    } else if (ig_strcmp(reason, FC1_RC_MAP_UNTRUSTED) == 0 ||
               ig_strcmp(reason, FC1_RC_ANCHOR_NOT_ADMITTED) == 0 ||
               ig_strcmp(reason, FC1_RC_UNINITIALIZED) == 0) {
        verdict = "refused_after_grace";
    } else {
        verdict = "refused";
    }

    /* ---- the ONE UE4SS.log line (terminal verdict) ---- */
    {
        char line[256];
        unsigned pos = 0;
        const char *part;
        /* small hand-rolled appends; bounded by the buffer */
        part = "FC1POOL: build=" IG_BUILD_ID " verdict=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        for (unsigned i = 0; verdict[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = verdict[i];
        part = " reason=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        for (unsigned i = 0; reason[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = reason[i];
        /* " base=0x" + 16 hex + " pool=0x" + 16 hex + " cb=" + u10 +
           " none=" + u10 + " byteprop=" + u10 + " checks=" + 3+1+3 +
           " (one line per launch)" + NUL: fits in 256. */
        part = " base=0x";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        static const char hd[] = "0123456789abcdef";
        for (int sh = 60; sh >= 0 && pos < sizeof(line) - 1; sh -= 4)
            line[pos++] = hd[((admitted ? base : 0) >> (unsigned)sh) & 15];
        part = " pool=0x";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        for (int sh = 60; sh >= 0 && pos < sizeof(line) - 1; sh -= 4)
            line[pos++] = hd[((admitted ? pool_rt : 0) >> (unsigned)sh) & 15];
        part = " cb=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        {
            char tmp[12];
            unsigned n = 0;
            uint32_t v = cur_block;
            if (v == 0) tmp[n++] = '0';
            while (v > 0 && n < sizeof tmp) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
            while (n > 0 && pos < sizeof(line) - 1) line[pos++] = tmp[--n];
        }
        part = " none=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        {
            char tmp[12];
            unsigned n = 0;
            uint32_t v = h_none;
            if (v == 0) tmp[n++] = '0';
            while (v > 0 && n < sizeof tmp) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
            while (n > 0 && pos < sizeof(line) - 1) line[pos++] = tmp[--n];
        }
        part = " byteprop=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        {
            char tmp[12];
            unsigned n = 0;
            uint32_t v = h_bp;
            if (v == 0) tmp[n++] = '0';
            while (v > 0 && n < sizeof tmp) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
            while (n > 0 && pos < sizeof(line) - 1) line[pos++] = tmp[--n];
        }
        part = " checks=";
        for (unsigned i = 0; part[i] && pos < sizeof(line) - 1; ++i)
            line[pos++] = part[i];
        {
            char tmp[8];
            unsigned n = 0;
            uint32_t v = checks_passed;
            if (v == 0) tmp[n++] = '0';
            while (v > 0 && n < sizeof tmp) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
            while (n > 0 && pos < sizeof(line) - 1) line[pos++] = tmp[--n];
        }
        if (pos < sizeof(line) - 1) line[pos++] = '/';
        {
            char tmp[8];
            unsigned n = 0;
            uint32_t v = checks_total;
            while (v > 0 && n < sizeof tmp) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
            while (n > 0 && pos < sizeof(line) - 1) line[pos++] = tmp[--n];
        }
        if (pos < sizeof(line) - 1) line[pos++] = '\n';
        line[pos] = 0;
        if (g_ops->log_line(line, pos) == 0) log_line_written = 1;
        else log_line_rc = -1;
    }

    /* ---- fc1pool_result detail record (verdict + chain summary;
            kept under the lineage 1 KiB line bound) ---- */
    {
        IgBuf rb;
        ig_record_begin(&rb, (int)seq, "fc1pool_result", 0, 0, IG_BUILD_ID);
        ig_kv_int(&rb, "poll", (int64_t)polls_run);
        ig_kv_str(&rb, "verdict", verdict, 32);
        ig_kv_str(&rb, "reason_code", reason[0] ? reason : "none", 48);
        ig_kv_int(&rb, "fc1pool_validated", (int64_t)validated);
        ig_kv_int(&rb, "map_trusted", (int64_t)map_trusted);
        ig_kv_int(&rb, "anchor_admitted", (int64_t)admitted);
        ig_kv_int(&rb, "anchor_rcode", (int64_t)anchor_rcode);
        ig_kv_hex(&rb, "base_derived", admitted ? base : 0);
        ig_kv_int(&rb, "base_matches_observed_bias",
                  (int64_t)(base_ok_bias < 0 ? 0 : base_ok_bias));
        ig_kv_hex(&rb, "pool_runtime", admitted ? pool_rt : 0);
        ig_kv_int(&rb, "guard_byte", (int64_t)guard_byte);
        ig_kv_int(&rb, "guard_byte_read", (int64_t)guard_read);
        ig_kv_int(&rb, "current_block", (int64_t)cur_block);
        ig_kv_int(&rb, "current_byte_offset", (int64_t)cur_bo);
        ig_kv_hex(&rb, "blocks0", blocks0);
        ig_kv_int(&rb, "blocks0_canonical", (int64_t)blocks0_canon);
        ig_kv_int(&rb, "id_none", (int64_t)h_none);
        ig_kv_int(&rb, "id_none_is_zero_expected", (int64_t)(h_none == 0 ? 1 : 0));
        ig_kv_int(&rb, "id_byteproperty", (int64_t)h_bp);
        /* the data-side reference for the initializer's later FNAME_Find
           behavioral cross-check (FNAME-VERDICT.md sec 5 option 1) */
        ig_kv_int(&rb, "byteprop_id_for_find_crosscheck", (int64_t)h_bp);
        ig_kv_int(&rb, "checks_passed", (int64_t)checks_passed);
        ig_kv_int(&rb, "checks_total", (int64_t)checks_total);
        ig_kv_int(&rb, "ue4ss_log_line_written", (int64_t)log_line_written);
        ig_record_flush(&rb);
        if (ig_sink_write_record(sink, &rb) != 0) sink->failed = 1;
        ++seq;
    }

    /* ---- fc1pool_entries evidence record (per-entry + window raw facts;
            split from the result record to keep every line < 1 KiB) ---- */
    {
        IgBuf zb;
        ig_record_begin(&zb, (int)seq, "fc1pool_entries", 0, 0, IG_BUILD_ID);
        ig_kv_hex(&zb, "pool_window_start", pool_hdr_span.start);
        ig_kv_hex(&zb, "pool_window_end", pool_hdr_span.end);
        ig_kv_int(&zb, "pool_window_prot", (int64_t)pool_hdr_span.prot);
        ig_kv_hex(&zb, "blockptr_none", ev_none.block_ptr);
        ig_kv_hex(&zb, "entry_addr_none", ev_none.entry_addr);
        ig_kv_int(&zb, "header_none", (int64_t)ev_none.header);
        kv_bytes16(&zb, "bytes16_none", ev_none.bytes16);
        ig_kv_int(&zb, "none_ascii_ok", (int64_t)none_ok);
        ig_kv_hex(&zb, "blockptr_byteproperty", ev_bp.block_ptr);
        ig_kv_hex(&zb, "entry_addr_byteproperty", ev_bp.entry_addr);
        ig_kv_int(&zb, "header_byteproperty", (int64_t)ev_bp.header);
        kv_bytes16(&zb, "bytes16_byteproperty", ev_bp.bytes16);
        ig_kv_int(&zb, "byteproperty_ascii_ok", (int64_t)bp_ok);
        ig_record_flush(&zb);
        if (ig_sink_write_record(sink, &zb) != 0) sink->failed = 1;
        ++seq;
    }

    /* ---- summary record ---- */
    {
        IgBuf sb;
        ig_record_begin(&sb, (int)seq, "summary", 0, 0, IG_BUILD_ID);
        ig_kv_int(&sb, "polls_run", (int64_t)polls_run);
        ig_kv_int(&sb, "validated", (int64_t)validated);
        ig_kv_int(&sb, "write_failures", (int64_t)sink->failed);
        ig_kv_int(&sb, "ue4ss_log_line_written", (int64_t)log_line_written);
        ig_kv_int(&sb, "ue4ss_log_line_rc", (int64_t)log_line_rc);
        ig_record_flush(&sb);
        if (ig_sink_write_record(sink, &sb) != 0) sink->failed = 1;
        ++seq;
    }
    (void)seq;
    return 0;
}
