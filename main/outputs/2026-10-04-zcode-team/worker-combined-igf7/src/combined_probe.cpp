// combined_probe.cpp — worker-combined-igf7 (team 2026-10-04).
//
// ONE combined in-game probe, igf7- build lineage (module modbridge_ingame_facts,
// log /data/modbridge-ingame-facts.jsonl, the 7 verbatim frozen-contract
// imports, write-through synchronous records, igf4/igf6 record_sink
// discipline; ordering is load-bearing for crash localization):
//
//   seq3    ONE mapscan: bounded sysctl KERN_PROC_VMMAP collection + ONE
//           rc3_vmmap_walk_min walk producing THE shared span table (v3
//           variable-kve_structsize stride, sane-record retention, v3 trust
//           gate). Derived base := lowest prot&4 span start (expected
//           0x400000; recorded, never assumed) + exec extent cross-check.
//   seq4..  paged mapspans records persisting the READABLE span list
//           (closes the igf6 gap: 8685 spans counted but never saved).
//   then    C2 ALTERNATIVE candidate reads, order B, C, D, E, A (A last):
//           each gated FIRST on the same-invocation table — refusal record
//           (map_untrusted | exec_span_intersection | uncovered | straddle |
//           covered_not_readable) or RAW live_hex + covering-span fields.
//           NO expected bytes exist anywhere in this module; NO comparison;
//           the correspondence decision is made OFFLINE by the controller
//           (C2-ALTERNATIVE-SPEC.md sec.6 — no self-fulfilling probe).
//   then    RUNG-3 chunk reads over the SAME table:
//           rc3_probe_chunks (header @GUA -> Objects** @GUA+0x10 ->
//           chunk[0]/chunk[1] -> first FUObjectItem each -> UObjectBase
//           fields + batch2 check family) + rc3_class.
//   then    RUNG-3 PREDICATE handoff: rc3b::run_ustruct_chain — readable-
//           only tlp::MemSpan publication + gated rfl::probe_ustruct_chain
//           (UStruct chain + UClass CDO extension) on the UObjectBase data.
//   last    summary record: base, exec extent, walk stats, per-candidate
//           gated/refused flags, exec_reads_attempted (structurally 0),
//           write failures, rung3 rc/class + per-check states, batch2
//           ran/gate/verdict.
//
// Safety invariants (all inherited, none new): the ONLY foreign memory
// accesses are igu_read()/igu_read_u64() calls through span->data after a
// covering READABLE-span proof on THIS invocation's walk. No exec-span read
// (explicit derived-exec-span intersection refusal — the igf6 crash class),
// no syscall instruction, no mprotect, no writes to game memory, no threads;
// everything runs synchronously inside module_start; the channel closes at
// the end and the module stays loaded but INERT.

#include <stdint.h>
#include <stddef.h>

extern "C" {
#include "ingame_libc.h"
#include "ingame_abi.h"
#include "record_sink.h"
}

#include "generated/ingame_config.h"

#include "../../worker-rung3-chunk-probe/src/rung3_batch2_bridge.hpp"

#ifndef IG_HOST_TEST
/* Lineage include order (v3 experiments.c / c2 mapscan.c): sys/types.h MUST
   precede sys/user.h on the pinned SDK. Host tests provide the map via
   ig_probe_load_map() and never compile this block (macOS lacks
   KERN_PROC_VMMAP). */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>
#include "anchor_store.h"   /* IG_VMMAP_MAX + lineage map constants */
#endif

namespace igf7 {

/* ------------------------------------------------------------------ */
/* Shared state (single module_start execution; no threads)             */
/* ------------------------------------------------------------------ */

static const size_t SPAN_CAP = 16384;      /* v3 lineage cap (igf6: 10047) */
static igu_span    g_spans[SPAN_CAP];
static rc3_vmstats g_walk;
static size_t      g_span_count;
static int         g_map_trusted;

static uint64_t g_base;          /* lowest prot&4 span start (expected 4e5) */
static uint64_t g_exec_lo;       /* min prot&4 start                        */
static uint64_t g_exec_hi;       /* max prot&4 end                          */
static uint32_t g_exec_count;    /* prot&4 spans retained                   */
static int      g_base_absent;   /* 1 iff no prot&4 span in the map         */

static int g_seq;                /* next record seq (entry/startup are 1/2) */
static unsigned g_write_failures;

/* Per-candidate outcome for the summary. */
enum CandOutcome {
    CAND_READ_OK = 0,
    CAND_MAP_UNTRUSTED = 1,
    CAND_EXEC_INTERSECT = 2,
    CAND_UNCOVERED = 3,
    CAND_STRADDLE = 4,
    CAND_NOT_READABLE = 5,
    CAND_READ_ERROR = 6   /* gate passed but igu_read failed: honest record */
};
struct CandState {
    int  outcome;
    int  gated;          /* 1 iff a readable span covered the window        */
    uint64_t rt;
    uint64_t span_start, span_end;
    uint32_t span_prot;
    int  bytes_read;
};
static CandState g_cand[5];
static const char* const g_cand_id[5] = {"B", "C", "D", "E", "A"};
static const uint64_t g_cand_vaddr[5] = {
    IG_VADDR_CAND_B, IG_VADDR_CAND_C, IG_VADDR_CAND_D,
    IG_VADDR_CAND_E, IG_VADDR_CAND_A };
static const uint32_t g_cand_len[5] = {
    IG_LEN_CAND_B, IG_LEN_CAND_C, IG_LEN_CAND_D, IG_LEN_CAND_E, IG_LEN_CAND_A };

static rc3_record   g_rung3;
static int          g_rung3_rc;
static int          g_rung3_cls;
static rc3b::Batch2Result g_batch2;

static igu_span_table table() {
    igu_span_table t;
    t.spans = g_spans;
    t.count = g_span_count;
    return t;
}

/* ------------------------------------------------------------------ */
/* Map: ONE collection + ONE walk (shared by C2 and rung-3)             */
/* ------------------------------------------------------------------ */

static void reset_state() {
    ig_memset(&g_walk, 0, sizeof(g_walk));
    g_span_count = 0;
    g_map_trusted = 0;
    g_base = 0;
    g_exec_lo = 0;
    g_exec_hi = 0;
    g_exec_count = 0;
    g_base_absent = 0;
    g_seq = 2;               /* entry(1) + startup(2) already emitted */
    g_write_failures = 0;
    for (int i = 0; i < 5; ++i) {
        g_cand[i].outcome = CAND_MAP_UNTRUSTED;
        g_cand[i].gated = 0;
        g_cand[i].rt = 0;
        g_cand[i].span_start = g_cand[i].span_end = 0;
        g_cand[i].span_prot = 0;
        g_cand[i].bytes_read = 0;
    }
    ig_memset(&g_rung3, 0, sizeof(g_rung3));
    g_rung3_rc = 0;
    g_rung3_cls = -1;
    ig_memset(&g_batch2, 0, sizeof(g_batch2));
}

/* Walk `buf` (a KERN_PROC_VMMAP buffer) into the shared table, derive the
   exec geometry, and evaluate the trust gate. ntargets==0 => full bounded
   walk (no early stop): the whole readable span list is persisted later. */
void ig_probe_load_map(const uint8_t* buf, uint64_t len, uint32_t kve_sizeof) {
    g_span_count = rc3_vmmap_walk_min(buf, len, kve_sizeof, g_spans,
                                      (uint32_t)SPAN_CAP, 0, 0, &g_walk);
    g_map_trusted = rc3_vmmap_trustworthy(&g_walk);
    g_exec_lo = ~0ull;
    g_exec_hi = 0;
    g_exec_count = 0;
    for (size_t i = 0; i < g_span_count; ++i) {
        if (g_spans[i].prot & IGU_PROT_X) {
            ++g_exec_count;
            if (g_spans[i].start < g_exec_lo) g_exec_lo = g_spans[i].start;
            if (g_spans[i].end > g_exec_hi) g_exec_hi = g_spans[i].end;
        }
    }
    g_base_absent = (g_exec_count == 0) ? 1 : 0;
    g_base = g_base_absent ? 0ull : g_exec_lo;
}

#ifndef IG_HOST_TEST
/* Bounded sysctl collection (c2 mapscan.c recipe): 65536 first try, doubling
   on ENOMEM up to the 4 MiB lineage cap. Returns 0 on success. */
static int ig_collect_vmmap(uint8_t* buf, uint64_t cap, uint64_t* out_len) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid()};
    size_t len = 65536;
    for (;;) {
        int rc = sysctl(mib, 4, buf, &len, 0, 0);
        if (rc == 0) {
            *out_len = (uint64_t)len;
            return 0;
        }
        int eno = 0;
        int* e = __error();
        if (e) eno = *e;
        if (eno == ENOMEM && len > 0 && ((uint64_t)len * 2u) <= cap) {
            len *= 2;
            continue;
        }
        return -1;
    }
}
#endif /* !IG_HOST_TEST */

/* ------------------------------------------------------------------ */
/* Record helpers                                                       */
/* ------------------------------------------------------------------ */

static void emit(IgSink* sink, IgBuf* b) {
    ig_record_flush(b);
    if (ig_sink_write_record(sink, b) != 0) ++g_write_failures;
}

static void kv_hexbytes(IgBuf* b, const char* key, const uint8_t* p,
                        size_t n) {
    static const char hexd[] = "0123456789abcdef";
    ig_kv_key(b, key);
    ig_buf_ch(b, '"');
    for (size_t i = 0; i < n; ++i) {
        ig_buf_ch(b, hexd[(p[i] >> 4) & 15]);
        ig_buf_ch(b, hexd[p[i] & 15]);
    }
    ig_buf_ch(b, '"');
    b->need_sep = 1;
}

/* ------------------------------------------------------------------ */
/* seq3 mapscan + seq4.. paged readable-span persistence                */
/* ------------------------------------------------------------------ */

static void emit_mapscan(IgSink* sink) {
    IgBuf b;
    ++g_seq;
    ig_record_begin(&b, g_seq, "mapscan", 0, 0, IG_BUILD_ID);
    ig_kv_int(&b, "walk_mode", g_walk.walk_mode);
    ig_kv_int(&b, "stop_reason", g_walk.stop_reason);
    ig_kv_hex(&b, "stop_off", g_walk.stop_off);
    ig_kv_int(&b, "records_total", (int64_t)g_walk.total);
    ig_kv_int(&b, "retained", (int64_t)g_walk.retained);
    ig_kv_int(&b, "truncated", (int64_t)g_walk.truncated);
    ig_kv_int(&b, "insane", (int64_t)g_walk.insane);
    ig_kv_int(&b, "ksz_full", (int64_t)g_walk.ksz_full);
    ig_kv_int(&b, "ksz_var", (int64_t)g_walk.ksz_var);
    ig_kv_int(&b, "ksz_144", (int64_t)g_walk.ksz_144);
    ig_kv_int(&b, "aligned16k", (int64_t)g_walk.aligned16k);
    ig_kv_int(&b, "map_trusted", g_map_trusted);
    ig_kv_hex(&b, "derived_base", g_base);
    ig_kv_int(&b, "base_absent", g_base_absent);
    ig_kv_hex(&b, "exec_span_lo", g_exec_lo);
    ig_kv_hex(&b, "exec_span_hi", g_exec_hi);
    ig_kv_int(&b, "exec_span_count", (int64_t)g_exec_count);
    /* Geometry cross-checks vs the checkpoint facts (informational; the
       offline comparison re-verifies base == 0x400000). */
    ig_kv_int(&b, "base_eq_expected", (g_base == IG_EXPECTED_BASE) ? 1 : 0);
    ig_kv_int(&b, "exec_extent_eq_expected",
              (!g_base_absent &&
               (g_exec_hi - g_exec_lo) == IG_EXPECTED_EXEC_EXTENT) ? 1 : 0);
    uint32_t readable = 0;
    for (size_t i = 0; i < g_span_count; ++i)
        if (g_spans[i].prot & IGU_PROT_R) ++readable;
    ig_kv_int(&b, "readable_spans", (int64_t)readable);
    emit(sink, &b);
}

#define SPANS_PER_PAGE 24

static void emit_span_pages(IgSink* sink) {
    uint32_t page = 0;
    size_t i = 0;
    uint32_t readable_total = 0;
    while (i < g_span_count) {
        /* count first for a stable "pages" field */
        ++page;
        IgBuf b;
        ++g_seq;
        ig_record_begin(&b, g_seq, "mapspans", 0, 0, IG_BUILD_ID);
        ig_kv_int(&b, "page", page);
        ig_arr_open(&b, "spans");
        uint32_t n = 0;
        for (; i < g_span_count && n < SPANS_PER_PAGE; ++i) {
            if (!(g_spans[i].prot & IGU_PROT_R)) continue; /* readable only */
            ++readable_total;
            ++n;
            if (b.need_sep) ig_buf_ch(&b, ',');
            /* start/end as QUOTED hex strings (bare 0x numbers are not
               valid JSON); prot stays a decimal number. */
            ig_buf_ch(&b, '[');
            ig_buf_ch(&b, '"'); ig_buf_u64_hex(&b, g_spans[i].start);
            ig_buf_ch(&b, '"');
            ig_buf_ch(&b, ',');
            ig_buf_ch(&b, '"'); ig_buf_u64_hex(&b, g_spans[i].end);
            ig_buf_ch(&b, '"');
            ig_buf_ch(&b, ',');
            ig_buf_u64_dec(&b, g_spans[i].prot);
            ig_buf_ch(&b, ']');
            b.need_sep = 1;   /* element separator for the NEXT span */
        }
        ig_arr_close(&b);
        ig_kv_int(&b, "spans_on_page", (int64_t)n);
        emit(sink, &b);
        if (i >= g_span_count) break;
    }
    (void)readable_total;
}

/* ------------------------------------------------------------------ */
/* C2 alternative candidate reads (B, C, D, E, A — A last)              */
/* ------------------------------------------------------------------ */

static int exec_span_intersects(uint64_t lo, uint64_t hi) {
    for (size_t i = 0; i < g_span_count; ++i) {
        if (!(g_spans[i].prot & IGU_PROT_X)) continue;
        if (lo < g_spans[i].end && g_spans[i].start < hi) return 1;
    }
    return 0;
}

static void emit_candidate(IgSink* sink, int idx) {
    IgBuf b;
    ++g_seq;
    ig_record_begin(&b, g_seq, "c2cand", 0, 0, IG_BUILD_ID);
    ig_kv_str(&b, "cand", g_cand_id[idx], 1);
    ig_kv_hex(&b, "vaddr", g_cand_vaddr[idx]);

    const igu_span_table t = table();
    CandState* cs = &g_cand[idx];
    const uint64_t rt = g_base_absent ? 0ull : g_base + g_cand_vaddr[idx];
    cs->rt = rt;
    ig_kv_hex(&b, "rt", rt);
    ig_kv_int(&b, "len", (int64_t)g_cand_len[idx]);

    if (!g_map_trusted) {
        cs->outcome = CAND_MAP_UNTRUSTED;
        ig_kv_str(&b, "refusal", "map_untrusted", 24);
        emit(sink, &b);
        return;
    }
    if (exec_span_intersects(rt, rt + g_cand_len[idx])) {
        /* igf6 crash class: NEVER deref anything the map shows exec. */
        cs->outcome = CAND_EXEC_INTERSECT;
        ig_kv_str(&b, "refusal", "exec_span_intersection", 24);
        emit(sink, &b);
        return;
    }
    uint32_t prot = 0;
    const int ci = igu_find_covering(&t, rt, g_cand_len[idx], &prot);
    if (ci == IGU_E_NO_SPAN) {
        cs->outcome = CAND_UNCOVERED;
        ig_kv_str(&b, "refusal", "uncovered", 16);
        emit(sink, &b);
        return;
    }
    if (ci == IGU_E_STRADDLE) {
        cs->outcome = CAND_STRADDLE;
        ig_kv_str(&b, "refusal", "straddle", 16);
        emit(sink, &b);
        return;
    }
    if (!(prot & IGU_PROT_R)) {
        cs->outcome = CAND_NOT_READABLE;
        ig_kv_str(&b, "refusal", "covered_not_readable", 24);
        emit(sink, &b);
        return;
    }
    cs->gated = 1;
    cs->span_start = t.spans[ci].start;
    cs->span_end = t.spans[ci].end;
    cs->span_prot = prot;
    uint8_t raw[64];
    const int rc = igu_read(&t, rt, raw, g_cand_len[idx]);
    if (rc != IGU_OK) {
        cs->outcome = CAND_READ_ERROR;
        ig_kv_int(&b, "read_rc", rc);
        emit(sink, &b);
        return;
    }
    cs->outcome = CAND_READ_OK;
    cs->bytes_read = (int)g_cand_len[idx];
    /* RAW live bytes ONLY. No expected bytes, no comparison, no byte_match:
       the controller compares offline against the pinned capture. */
    kv_hexbytes(&b, "live_hex", raw, g_cand_len[idx]);
    ig_kv_hex(&b, "span_start", cs->span_start);
    ig_kv_hex(&b, "span_end", cs->span_end);
    ig_kv_int(&b, "span_prot", (int64_t)cs->span_prot);
    emit(sink, &b);
}

/* ------------------------------------------------------------------ */
/* Rung-3 chunk reads + predicate handoff                               */
/* ------------------------------------------------------------------ */

static const char* csk_name(int v) {
    switch (v) {
        case IGU_CSK_PASS: return "pass";
        case IGU_CSK_CONTRADICTED: return "contradicted";
        case IGU_CSK_UNAVAILABLE: return "unavailable";
        default: return "not_run";
    }
}

/* Bounded per-chunk-view record key: "<pre><i><suf>" (max 23 chars). */
static const char* ckey(char* buf, size_t cap, const char* pre, int i,
                        const char* suf) {
    size_t n = 0;
    for (; *pre && n + 1 < cap; ++pre) buf[n++] = *pre;
    if (n + 1 < cap) buf[n++] = (char)('0' + i);
    for (; *suf && n + 1 < cap; ++suf) buf[n++] = *suf;
    buf[n] = '\0';
    return buf;
}

static void emit_rung3(IgSink* sink) {
    const igu_span_table t = table();
    ++g_seq;
    g_rung3_rc = rc3_probe_chunks(&t, &g_walk, IG_RT_GUOBJECT_ARRAY,
                                  IGU_SCAN_MAX_ITEMS, &g_rung3);
    g_rung3_cls = rc3_class(&g_rung3);

    IgBuf b;
    ig_record_begin(&b, g_seq, "rung3", g_rung3_rc, 0, IG_BUILD_ID);
    ig_kv_hex(&b, "gua_rt", IG_RT_GUOBJECT_ARRAY);
    ig_kv_int(&b, "cls", g_rung3_cls);
    ig_kv_int(&b, "map_trusted", g_rung3.map_trusted);
    ig_kv_int(&b, "st_hdr", g_rung3.st_hdr);
    ig_kv_int(&b, "first_gc", (int64_t)g_rung3.first_gc);
    ig_kv_int(&b, "last_non_gc", (int64_t)g_rung3.last_non_gc);
    ig_kv_int(&b, "max_not_gc", (int64_t)g_rung3.max_not_gc);
    ig_kv_int(&b, "open_dfgc", (int64_t)g_rung3.open_dfgc);
    ig_kv_int(&b, "max_elements", (int64_t)g_rung3.max_elements);
    ig_kv_int(&b, "num_elements", (int64_t)g_rung3.num_elements);
    ig_kv_int(&b, "max_chunks", (int64_t)g_rung3.max_chunks);
    ig_kv_int(&b, "num_chunks", (int64_t)g_rung3.num_chunks);
    ig_kv_str(&b, "info_max_eq_chunks", csk_name(g_rung3.info_max_eq_chunks), 16);
    ig_kv_str(&b, "info_num_le_max", csk_name(g_rung3.info_num_le_max), 16);
    ig_kv_str(&b, "info_nchunks_ok", csk_name(g_rung3.info_nchunks_ok), 16);
    ig_kv_int(&b, "st_a", g_rung3.st_a);
    ig_kv_hex(&b, "objects_ptr", g_rung3.objects_ptr);
    for (int i = 0; i < 2; ++i) {
        char key[24];
        const rc3_chunk_view* cv = &g_rung3.chunk[i];
        ig_kv_int(&b, ckey(key,sizeof(key),"c",i,"_st_ptr"), cv->st_ptr);
        ig_kv_hex(&b, ckey(key,sizeof(key),"c",i,"_ptr"), cv->ptr);
        ig_kv_int(&b, ckey(key,sizeof(key),"c",i,"_st_item"), cv->st_item);
        ig_kv_hex(&b, ckey(key,sizeof(key),"c",i,"_item_object"),
                  cv->item_object);
        ig_kv_int(&b, ckey(key,sizeof(key),"c",i,"_item_flags"),
                  (int64_t)cv->item_flags);
        ig_kv_int(&b, ckey(key,sizeof(key),"c",i,"_item_cluster"),
                  (int64_t)cv->item_cluster);
        ig_kv_int(&b, ckey(key,sizeof(key),"c",i,"_item_serial"),
                  (int64_t)cv->item_serial);
        ig_kv_int(&b, ckey(key,sizeof(key),"c",i,"_items_scanned"),
                  (int64_t)cv->items_scanned);
        ig_kv_int(&b, ckey(key,sizeof(key),"c",i,"_item_nonnull"),
                  cv->item_nonnull);
        ig_kv_str(&b, ckey(key,sizeof(key),"c",i,"_chk_needed"),
                  csk_name(cv->chk_needed), 16);
    }
    ig_kv_int(&b, "st_obj", g_rung3.st_obj);
    ig_kv_int(&b, "obj_source_chunk", g_rung3.obj_source_chunk);
    ig_kv_hex(&b, "obj_addr", g_rung3.obj_addr);
    ig_kv_hex(&b, "vptr", g_rung3.vptr);
    ig_kv_int(&b, "obj_flags", (int64_t)g_rung3.obj_flags);
    ig_kv_int(&b, "internal_index", (int64_t)g_rung3.internal_index);
    ig_kv_hex(&b, "class_ptr", g_rung3.class_ptr);
    kv_hexbytes(&b, "name_raw", g_rung3.name_raw, 8);
    ig_kv_hex(&b, "outer_ptr", g_rung3.outer_ptr);
    ig_kv_str(&b, "chk_vptr", csk_name(g_rung3.chk_vptr), 16);
    ig_kv_str(&b, "chk_flags", csk_name(g_rung3.chk_flags), 16);
    ig_kv_str(&b, "chk_index_bounds", csk_name(g_rung3.chk_index_bounds), 16);
    ig_kv_str(&b, "chk_roundtrip", csk_name(g_rung3.chk_roundtrip), 16);
    ig_kv_str(&b, "chk_class", csk_name(g_rung3.chk_class), 16);
    ig_kv_str(&b, "chk_outer", csk_name(g_rung3.chk_outer), 16);
    emit(sink, &b);
}

static void emit_batch2(IgSink* sink) {
    const igu_span_table t = table();
    ++g_seq;
    g_batch2 = rc3b::run_ustruct_chain(g_rung3, g_walk, t.spans, t.count);
    IgBuf b;
    ig_record_begin(&b, g_seq, "batch2", 0, 0, IG_BUILD_ID);
    ig_kv_int(&b, "ran", (int64_t)g_batch2.ran);
    ig_kv_int(&b, "gate", (int64_t)g_batch2.gate);
    ig_kv_int(&b, "verdict", (int64_t)g_batch2.verdict);
    ig_kv_int(&b, "code_count", (int64_t)g_batch2.code_count);
    ig_arr_open(&b, "codes");
    for (int i = 0; i < g_batch2.code_count && i < 8; ++i) {
        if (i) { ig_buf_ch(&b, ','); }
        ig_buf_u64_dec(&b, g_batch2.codes[i]);
    }
    ig_arr_close(&b);
    ig_kv_int(&b, "items_examined", (int64_t)g_batch2.items_examined);
    ig_kv_int(&b, "plausible_uobject", (int64_t)g_batch2.plausible_uobject);
    ig_kv_int(&b, "implausible_uobject", (int64_t)g_batch2.implausible_uobject);
    ig_kv_int(&b, "roundtrips_ok", (int64_t)g_batch2.roundtrips_ok);
    ig_kv_int(&b, "struct_like_count", (int64_t)g_batch2.struct_like_count);
    ig_kv_int(&b, "walk_truncated", (int64_t)g_batch2.walk_truncated);
    ig_kv_hex(&b, "first_plausible", g_batch2.first_plausible);
    ig_kv_hex(&b, "first_ustruct_like", g_batch2.first_ustruct_like);
    ig_kv_hex(&b, "first_uclass_like", g_batch2.first_uclass_like);
    emit(sink, &b);
}

/* ------------------------------------------------------------------ */
/* Summary                                                              */
/* ------------------------------------------------------------------ */

static void emit_summary(IgSink* sink) {
    IgBuf b;
    ++g_seq;
    ig_record_begin(&b, g_seq, "summary", 0, 0, IG_BUILD_ID);
    ig_kv_hex(&b, "derived_base", g_base);
    ig_kv_hex(&b, "exec_span_lo", g_exec_lo);
    ig_kv_hex(&b, "exec_span_hi", g_exec_hi);
    ig_kv_int(&b, "map_trusted", g_map_trusted);
    ig_kv_int(&b, "spans_retained", (int64_t)g_walk.retained);
    ig_kv_int(&b, "spans_truncated", (int64_t)g_walk.truncated);
    ig_kv_int(&b, "spans_emitted", (int64_t)g_span_count);
    ig_arr_open(&b, "candidates");
    for (int i = 0; i < 5; ++i) {
        if (i) ig_buf_ch(&b, ',');
        ig_buf_raw(&b, "{\"id\":\"");
        ig_buf_raw(&b, g_cand_id[i]);
        ig_buf_raw(&b, "\",\"gated\":");
        ig_buf_u64_dec(&b, (uint64_t)g_cand[i].gated);
        ig_buf_raw(&b, ",\"outcome\":");
        ig_buf_u64_dec(&b, (uint64_t)g_cand[i].outcome);
        ig_buf_raw(&b, ",\"bytes_read\":");
        ig_buf_u64_dec(&b, (uint64_t)g_cand[i].bytes_read);
        ig_buf_ch(&b, '}');
    }
    ig_arr_close(&b);
    ig_kv_int(&b, "exec_reads_attempted", 0); /* structural: gate refuses exec */
    ig_kv_int(&b, "game_memory_writes", 0);   /* probe contains no write path */
    ig_kv_int(&b, "write_failures", (int64_t)g_write_failures);
    ig_kv_int(&b, "rung3_rc", g_rung3_rc);
    ig_kv_int(&b, "rung3_cls", g_rung3_cls);
    ig_kv_int(&b, "rung3_map_trusted", g_rung3.map_trusted);
    ig_kv_int(&b, "rung3_st_hdr", g_rung3.st_hdr);
    ig_kv_int(&b, "rung3_st_a", g_rung3.st_a);
    ig_kv_int(&b, "batch2_ran", (int64_t)g_batch2.ran);
    ig_kv_int(&b, "batch2_gate", (int64_t)g_batch2.gate);
    ig_kv_int(&b, "batch2_verdict", (int64_t)g_batch2.verdict);
    emit(sink, &b);
}

/* ------------------------------------------------------------------ */
/* Entry point called by module_start (after entry + startup records)   */
/* ------------------------------------------------------------------ */

static void emit_all(IgSink* sink) {
    emit_mapscan(sink);
    emit_span_pages(sink);
    for (int i = 0; i < 5; ++i) emit_candidate(sink, i); /* B, C, D, E, A */
    emit_rung3(sink);
    emit_batch2(sink);
    emit_summary(sink);
}

}  /* namespace igf7 */

extern "C" void ig_run_combined_probe(IgSink* sink) {
    igf7::reset_state();

#ifndef IG_HOST_TEST
    static uint8_t vmmap_buf[IG_VMMAP_MAX];  /* 4 MiB BSS, no allocation */
    uint64_t len = 0;
    if (igf7::ig_collect_vmmap(vmmap_buf, IG_VMMAP_MAX, &len) != 0) {
        /* Collection failed: the walk never ran; the trust gate stays 0 and
           every downstream step must refuse (honest needs-map failure). */
        igf7::ig_probe_load_map(vmmap_buf, 0, 0);
    } else {
        igf7::ig_probe_load_map(vmmap_buf, len,
                                (uint32_t)sizeof(struct kinfo_vmentry));
    }
#endif

    igf7::emit_all(sink);
}

#ifdef IG_HOST_TEST
/* Host-test hooks (never built into the PRX): feed a synthetic VMMAP
   buffer, re-point span data pointers into fixture bytes after the walk
   (live discipline), and run the emission sequence on the prepared map. */
extern "C" void igf7_host_set_map(const uint8_t* buf, uint64_t len) {
    igf7::reset_state();
    igf7::ig_probe_load_map(buf, len, 144); /* path-stripped kve records */
}
extern "C" int igf7_host_repoint(uint64_t start, const uint8_t* data) {
    for (size_t i = 0; i < igf7::g_span_count; ++i) {
        if (igf7::g_spans[i].start == start) {
            igf7::g_spans[i].data = data;
            return 1;
        }
    }
    return 0;
}
extern "C" void ig_run_combined_probe_no_map_reset(IgSink* sink) {
    igf7::emit_all(sink);
}
#endif
