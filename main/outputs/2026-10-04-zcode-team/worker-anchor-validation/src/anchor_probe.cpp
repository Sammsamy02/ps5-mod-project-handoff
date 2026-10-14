// anchor_probe.cpp — worker-anchor-validation (team 2026-10-04).
// THE CRITICAL PATH TASK: one focused ploader PRX run that (a) verifies the
// mapping correspondence via code byte anchors, (b) confirms the live code
// anchors against the RETAINED IMAGE bytes, and (c) runs the vendored batch1
// GUObjectArray STRUCTURAL predicate at 0x97ba110 — all in a single game
// launch, one build_id, every check recorded before the next dereference.
//
// CONTROLLER CORRECTIONS APPLIED (this file implements them):
//  #1 SAFETY BEFORE DEREFERENCE: every address is gated by a CURRENT
//     per-invocation sysctl KERN_PROC_VMMAP coverage check (mapscan.c, mode-1
//     lineage walk, trust-gated). No v3 map is assumed current. An
//     unverifiable address produces a refusal record (gate=1..4) — NEVER a
//     dereference — and later checks still run (correction #4).
//  #2 CODE ANCHORS: Tick/SCO live bytes are compared for BYTE EQUALITY with
//     the expected bytes extracted from the retained image at file offset
//     vaddr+0x4000 (injected via generated/ingame_config.h from
//     expected_bytes_tick_sco.txt).
//  #3 GUA PREDICATE IS STRUCTURAL: the vendored worker-5 predicate
//     (townfall_layout_probes) runs under BOTH layout hypotheses (A
//     FUObjectArray-outer / B TUObjectArray-direct) on the LIVE bytes. NO
//     byte comparison with the executable file for GUA (BSS is
//     runtime-populated). Two bounded feed configurations, recorded
//     separately, never collapsed:
//       cfgH "gua_pred_header": the task-literal feed — exactly the 96
//            verified bytes at 0x97ba110 as the only span (header-local;
//            pointer targets outside the span are Refuted-class by
//            construction and recorded as such).
//       cfgL "gua_pred_live": the full CURRENT readable span set — pointer
//            chasing is range-gated against real mappings, so Validated is
//            possible and meaningful.
//  #5 WRITES AUTHORIZED: one controlled write test — SAME-VALUE writeback of
//     the 96 already-read GUA bytes via 8-byte stores (value-preserving: no
//     semantic change even under concurrent readers), then re-read+compare.
//     Only attempted when the covering span is prot-writable.
//  #6 NO FULL MAP DUMP: the map is internal only; records carry counts/trust
//     flags and the single covering span per probed address.
//
// Fault containment: records are flushed in order; the log up to the last
// line localizes any unexpected fault to the next gated dereference.
#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "townfall_layout_probes.hpp"

/* record_sink.h is a C header (vendored v3 lineage, byte-unchanged); this
   TU is C++, so the sink/record functions must be referenced with C linkage
   to merge with the C TUs' definitions (v3 got this via its C experiments.c
   + extern "C"-guarded experiments.h). */
extern "C" {
#include "record_sink.h"
}

#include <stdint.h>

extern "C" void ig_run_anchor_probe(IgSink *sink);

extern "C" unsigned g_ctor_events;
extern "C" unsigned g_fini_events;

/* Cross-check state surfaced by the seq-10 summary (set during seq 4..7). */
static int s_tick_attempted;
static int s_tick_match;    /* 1 match, 0 mismatch, -1 not attempted */
static int s_sco_attempted;
static int s_sco_match;
static int s_writeback_attempted;
static int s_writeback_restored;
static IgSpan s_gua_cover;

namespace {

/* Live readable span set, converted once after the mapscan (correction #1). */
tlp::MemSpan s_mem[IG_SPAN_MAX];
uint32_t s_mem_count;

void build_live_spans(void) {
    s_mem_count = 0;
    uint32_t n = g_map.walk.retained;
    if (n > IG_SPAN_MAX) n = IG_SPAN_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        if ((g_spans[i].prot & 1u) == 0) continue;  // readable only
        s_mem[s_mem_count].start = g_spans[i].start;
        s_mem[s_mem_count].size = g_spans[i].end - g_spans[i].start;
        s_mem[s_mem_count].data =
            reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(
                g_spans[i].start));
        ++s_mem_count;
    }
}

/* Access gate — correction #1. 0 = proceed; else refusal reason code. */
enum {
    ACC_OK = 0,
    ACC_REFUSED_MAP_UNTRUSTED = 1,
    ACC_REFUSED_NOT_COVERED = 2,
    ACC_REFUSED_NOT_READABLE = 3,
    ACC_REFUSED_NOT_WRITABLE = 4,
};

int gate_access(uint64_t addr, uint64_t len, int need_write, IgSpan *cover) {
    if (!g_map.trusted) return ACC_REFUSED_MAP_UNTRUSTED;
    if (!ig_map_cover(addr, len, cover)) return ACC_REFUSED_NOT_COVERED;
    if ((cover->prot & 1u) == 0) return ACC_REFUSED_NOT_READABLE;
    if (need_write && (cover->prot & 2u) == 0) return ACC_REFUSED_NOT_WRITABLE;
    return ACC_OK;
}

/* Bounded hex encode (lowercase); out needs 2*n+1 bytes. */
void hex_encode(const uint8_t *b, size_t n, char *out) {
    static const char hexd[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = hexd[(b[i] >> 4) & 15];
        out[2 * i + 1] = hexd[b[i] & 15];
    }
    out[2 * n] = '\0';
}

void hex_to_bytes(const char *hex, uint8_t *out, size_t nbytes) {
    for (size_t i = 0; i < nbytes; ++i) {
        char hi = hex[2 * i], lo = hex[2 * i + 1];
        uint8_t v = (uint8_t)((hi >= '0' && hi <= '9') ? (hi - '0')
                                                       : (hi - 'a' + 10));
        v = (uint8_t)(v << 4);
        v |= (uint8_t)((lo >= '0' && lo <= '9') ? (lo - '0') : (lo - 'a' + 10));
        out[i] = v;
    }
}

uint64_t le64(const uint8_t *p) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

int flush_record(IgSink *s, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(s, b) != 0) s->failed = 1;
    return s->failed;
}

/* bounded key assembly: "<pre><suf>" (both literals, total < 60 bytes) */
void mk_key(char *dst, const char *pre, const char *suf) {
    size_t i = 0;
    while (pre && *pre && i < 28) dst[i++] = *pre++;
    while (suf && *suf && i < 60) dst[i++] = *suf++;
    dst[i] = '\0';
}

/* Emit predicate result fields with key prefix (e.g. "p"). */
void emit_pred(IgBuf *b, const char *pre, const tlp::GuaProbeResult &r) {
    char k[64];
    mk_key(k, pre, "_verdict");
    ig_kv_str(b, k, tlp::verdict_string(r.verdict), 24);
    mk_key(k, pre, "_rc0");
    ig_kv_str(b, k, r.code_count > 0 ? tlp::rcode_string(r.codes[0]) : "", 40);
    mk_key(k, pre, "_la_verdict");
    ig_kv_str(b, k, tlp::verdict_string(r.layout_a.verdict), 24);
    mk_key(k, pre, "_la_rc");
    ig_kv_str(b, k, r.layout_a.code_count > 0
                        ? tlp::rcode_string(r.layout_a.codes[0]) : "", 40);
    mk_key(k, pre, "_lb_verdict");
    ig_kv_str(b, k, tlp::verdict_string(r.layout_b.verdict), 24);
    mk_key(k, pre, "_lb_rc");
    ig_kv_str(b, k, r.layout_b.code_count > 0
                        ? tlp::rcode_string(r.layout_b.codes[0]) : "", 40);
    mk_key(k, pre, "_layout_matched");
    ig_kv_int(b, k, (int64_t)r.observed.layout_matched);
    mk_key(k, pre, "_first_gc");
    ig_kv_int(b, k, (int64_t)r.observed.obj_first_gc_index);
    mk_key(k, pre, "_last_non_gc");
    ig_kv_int(b, k, (int64_t)r.observed.obj_last_non_gc_index);
    mk_key(k, pre, "_max_elements");
    ig_kv_int(b, k, (int64_t)r.observed.max_elements);
    mk_key(k, pre, "_num_elements");
    ig_kv_int(b, k, (int64_t)r.observed.num_elements);
    mk_key(k, pre, "_max_chunks");
    ig_kv_int(b, k, (int64_t)r.observed.max_chunks);
    mk_key(k, pre, "_num_chunks");
    ig_kv_int(b, k, (int64_t)r.observed.num_chunks);
    mk_key(k, pre, "_objects_ptr");
    ig_kv_hex(b, k, r.observed.objects_ptr);
    mk_key(k, pre, "_prealloc_ptr");
    ig_kv_hex(b, k, r.observed.preallocated_objects_ptr);
    mk_key(k, pre, "_chunk0");
    ig_kv_hex(b, k, r.observed.chunk0_ptr);
    mk_key(k, pre, "_chunk1");
    ig_kv_hex(b, k, r.observed.chunk1_ptr);
    mk_key(k, pre, "_chunks_scanned");
    ig_kv_int(b, k, (int64_t)r.observed.chunks_scanned);
}

/* One gated 16-byte code anchor check (correction #2). seq 4 / 5. */
void code_anchor(IgSink *sink, int seq, const char *exp, uint64_t rt_addr,
                 uint64_t vaddr, uint64_t file_off, const char *expected_hex,
                 int *attempted_out, int *match_out) {
    IgBuf b;
    IgSpan cover;
    char live_hex[40];
    uint8_t live_bytes[16];
    int gate = gate_access(rt_addr, 16, 0, &cover);
    int match = -1;
    unsigned diff_mask = 0;
    tlp::CodeProbeResult shape;
    shape.verdict = tlp::Verdict::NotProbed;
    shape.code_count = 0;
    live_hex[0] = '\0';

    if (gate == ACC_OK) {
        *attempted_out = 1;
        const uint8_t *p =
            reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(rt_addr));
        for (int i = 0; i < 16; ++i) live_bytes[i] = p[i];  /* gated deref */
        hex_encode(live_bytes, 16, live_hex);
        uint8_t exp_bytes[16];
        hex_to_bytes(expected_hex, exp_bytes, 16);
        match = 1;
        for (int i = 0; i < 16; ++i)
            if (live_bytes[i] != exp_bytes[i]) {
                match = 0;
                diff_mask |= 1u << i;
            }
        shape = tlp::probe_code_shape(s_mem, s_mem_count, IG_IMAGE_BASE,
                                      vaddr, nullptr);
    }
    *match_out = match;

    ig_record_begin(&b, seq, exp, gate, 0, IG_BUILD_ID);
    ig_kv_hex(&b, "rt_addr", rt_addr);
    ig_kv_hex(&b, "profile_vaddr", vaddr);
    ig_kv_hex(&b, "image_file_off", file_off);
    ig_kv_str(&b, "expected_src",
              "decrypted-eboot.bin sha16=" IG_IMAGE_SHA16
              " off=vaddr+0x4000", 64);
    ig_kv_str(&b, "expected_hex", expected_hex, 32);
    if (gate == ACC_OK) {
        ig_kv_hex(&b, "span_start", cover.start);
        ig_kv_hex(&b, "span_end", cover.end);
        ig_kv_hex(&b, "span_prot", cover.prot);
        ig_kv_str(&b, "live_hex", live_hex, 32);
        ig_kv_int(&b, "bytes_read", 16);
        ig_kv_int(&b, "match", match);
        ig_kv_int(&b, "diff_mask", (int64_t)diff_mask);
        ig_kv_str(&b, "shape_verdict", tlp::verdict_string(shape.verdict), 24);
        ig_kv_str(&b, "shape_rc", shape.code_count > 0
                                      ? tlp::rcode_string(shape.codes[0])
                                      : "", 40);
    } else {
        ig_kv_str(&b, "refusal",
                  gate == ACC_REFUSED_MAP_UNTRUSTED ? "map_untrusted"
                  : gate == ACC_REFUSED_NOT_COVERED
                      ? "not_covered_current_map"
                      : "covered_not_readable", 40);
    }
    flush_record(sink, &b);
}

}  // namespace

extern "C" void ig_run_anchor_probe(IgSink *sink) {
    IgBuf b;

    /* ---- seq 3: mapscan (bounded evidence; NO dump — correction #6). ---- */
    int trusted = ig_mapscan_run();
    build_live_spans();
    ig_record_begin(&b, 3, "mapscan", g_map.rc, g_map.eno, IG_BUILD_ID);
    ig_kv_int(&b, "attempts", g_map.attempts);
    ig_kv_hex(&b, "buf_len", g_map.buf_len);
    ig_kv_hex(&b, "first_ksz", g_map.first_ksz);
    ig_kv_int(&b, "mod_ok", g_map.mod_ok);
    ig_kv_int(&b, "total", g_map.walk.total);
    ig_kv_int(&b, "retained", g_map.walk.retained);
    ig_kv_int(&b, "truncated", g_map.walk.truncated);
    ig_kv_int(&b, "insane", g_map.walk.insane);
    ig_kv_int(&b, "ksz_144", g_map.walk.ksz_144);
    ig_kv_int(&b, "stop_reason", g_map.walk.stop_reason);
    ig_kv_int(&b, "trusted", trusted);
    ig_kv_int(&b, "readable_spans", g_map.readable_spans);
    ig_kv_str(&b, "note", "internal coverage source only; no map dump", 64);
    flush_record(sink, &b);

    /* ---- seq 4/5: Tick / SCO code anchors (correction #2). ---- */
    code_anchor(sink, 4, "tick_anchor", IG_RT_UGAMEENGINE_TICK,
                IG_VADDR_UGAMEENGINE_TICK, IG_TICK_FILE_OFF,
                IG_TICK_EXPECTED_HEX, &s_tick_attempted, &s_tick_match);
    code_anchor(sink, 5, "sco_anchor", IG_RT_STATIC_CONSTRUCT_OBJECT,
                IG_VADDR_STATIC_CONSTRUCT_OBJECT, IG_SCO_FILE_OFF,
                IG_SCO_EXPECTED_HEX, &s_sco_attempted, &s_sco_match);

    /* ---- seq 6: GUA live read (96 bytes at 0x97ba110, structural input). */
    uint8_t gua[96];
    int gua_gate = gate_access(IG_RT_GUOBJECT_ARRAY, 96, 0, &s_gua_cover);
    int gua_read_ok = 0;
    if (gua_gate == ACC_OK) {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(IG_RT_GUOBJECT_ARRAY));
        for (int i = 0; i < 96; ++i) gua[i] = p[i];  /* gated deref */
        gua_read_ok = 1;
    }
    {
        ig_record_begin(&b, 6, "gua_read", gua_gate, 0, IG_BUILD_ID);
        ig_kv_hex(&b, "rt_addr", IG_RT_GUOBJECT_ARRAY);
        ig_kv_hex(&b, "profile_vaddr", IG_VADDR_GUOBJECT_ARRAY);
        ig_kv_int(&b, "bytes", 96);
        if (gua_gate == ACC_OK) {
            char hex[200];
            hex_encode(gua, 96, hex);
            ig_kv_hex(&b, "span_start", s_gua_cover.start);
            ig_kv_hex(&b, "span_end", s_gua_cover.end);
            ig_kv_hex(&b, "span_prot", s_gua_cover.prot);
            ig_kv_str(&b, "hex", hex, 192);
            ig_kv_hex(&b, "w0", le64(gua + 0x00));
            ig_kv_hex(&b, "w1", le64(gua + 0x08));
            ig_kv_hex(&b, "w2", le64(gua + 0x10));  /* A: Objects */
            ig_kv_hex(&b, "w3", le64(gua + 0x18));  /* A: PreAllocated */
        } else {
            ig_kv_str(&b, "refusal",
                      gua_gate == ACC_REFUSED_MAP_UNTRUSTED
                          ? "map_untrusted"
                      : gua_gate == ACC_REFUSED_NOT_COVERED
                            ? "not_covered_current_map"
                            : "covered_not_readable", 40);
        }
        flush_record(sink, &b);
    }

    /* ---- seq 7: same-value writeback write test (correction #5). ---- */
    {
        IgSpan cover;
        int wgate = gua_read_ok
                        ? gate_access(IG_RT_GUOBJECT_ARRAY, 96, 1, &cover)
                        : ACC_REFUSED_MAP_UNTRUSTED;
        int attempted = 0;
        int restored = -1;
        if (wgate == ACC_OK) {
            volatile uint64_t *q = reinterpret_cast<volatile uint64_t *>(
                static_cast<uintptr_t>(IG_RT_GUOBJECT_ARRAY));
            for (int i = 0; i < 12; ++i) q[i] = le64(gua + 8 * i);
            attempted = 1;
            restored = 1;
            const uint8_t *p = reinterpret_cast<const uint8_t *>(
                static_cast<uintptr_t>(IG_RT_GUOBJECT_ARRAY));
            for (int i = 0; i < 96; ++i)
                if (p[i] != gua[i]) { restored = 0; break; }
        }
        s_writeback_attempted = attempted;
        s_writeback_restored = restored;
        ig_record_begin(&b, 7, "gua_writeback", wgate, 0, IG_BUILD_ID);
        ig_kv_int(&b, "attempted", attempted);
        if (wgate == ACC_OK) {
            ig_kv_hex(&b, "span_prot", cover.prot);
            ig_kv_int(&b, "restored", restored);
            ig_kv_str(&b, "method", "same_value_writeback_96B_8byte_stores",
                      40);
        } else {
            ig_kv_str(&b, "refusal",
                      wgate == ACC_REFUSED_MAP_UNTRUSTED
                          ? "gua_read_not_performed"
                      : wgate == ACC_REFUSED_NOT_COVERED
                            ? "not_covered_current_map"
                      : wgate == ACC_REFUSED_NOT_READABLE
                            ? "covered_not_readable"
                            : "covered_not_writable", 40);
        }
        flush_record(sink, &b);
    }

    /* ---- seq 8: predicate, cfgH header-span-only feed (task-literal). ---- */
    int pred_h_verdict = -1;
    {
        int gate = gua_read_ok ? ACC_OK : ACC_REFUSED_MAP_UNTRUSTED;
        ig_record_begin(&b, 8, "gua_pred_header", gate, 0, IG_BUILD_ID);
        if (gate == ACC_OK) {
            tlp::MemSpan hs[1];
            hs[0].start = IG_RT_GUOBJECT_ARRAY;
            hs[0].size = 96;
            hs[0].data = reinterpret_cast<const uint8_t *>(
                static_cast<uintptr_t>(IG_RT_GUOBJECT_ARRAY));
            tlp::GuaProbeResult r = tlp::probe_guobject_array(
                hs, 1, IG_IMAGE_BASE, IG_VADDR_GUOBJECT_ARRAY);
            pred_h_verdict = (int)r.verdict;
            ig_kv_str(&b, "cfg", "header_span_only_96B", 32);
            emit_pred(&b, "p", r);
        } else {
            ig_kv_str(&b, "refusal", "gua_read_not_performed", 32);
        }
        flush_record(sink, &b);
    }

    /* ---- seq 9: predicate, cfgL live readable spans (structural full). --- */
    int pred_l_verdict = -1;
    {
        int gate = gua_read_ok ? ACC_OK : ACC_REFUSED_MAP_UNTRUSTED;
        ig_record_begin(&b, 9, "gua_pred_live", gate, 0, IG_BUILD_ID);
        if (gate == ACC_OK) {
            tlp::GuaProbeResult r = tlp::probe_guobject_array(
                s_mem, s_mem_count, IG_IMAGE_BASE, IG_VADDR_GUOBJECT_ARRAY);
            pred_l_verdict = (int)r.verdict;
            ig_kv_str(&b, "cfg", "live_readable_spans", 32);
            ig_kv_int(&b, "spans_used", (int64_t)s_mem_count);
            emit_pred(&b, "p", r);
        } else {
            ig_kv_str(&b, "refusal", "gua_read_not_performed", 32);
        }
        flush_record(sink, &b);
    }

    /* ---- seq 10: summary (per-check pass/fail; indicator, NOT a claim). -- */
    {
        int anchor_confirmed =
            (s_tick_match == 1) && (s_sco_match == 1);
        int gua_validated =
            (pred_h_verdict == (int)tlp::Verdict::Validated) ||
            (pred_l_verdict == (int)tlp::Verdict::Validated);
        ig_record_begin(&b, 10, "summary", 0, 0, IG_BUILD_ID);
        ig_kv_int(&b, "map_trusted", g_map.trusted);
        ig_kv_int(&b, "tick_attempted", s_tick_attempted);
        ig_kv_int(&b, "tick_match", s_tick_match);
        ig_kv_int(&b, "sco_attempted", s_sco_attempted);
        ig_kv_int(&b, "sco_match", s_sco_match);
        ig_kv_int(&b, "gua_read_ok", gua_read_ok);
        ig_kv_int(&b, "writeback_attempted", s_writeback_attempted);
        ig_kv_int(&b, "writeback_restored", s_writeback_restored);
        ig_kv_int(&b, "pred_header_verdict", pred_h_verdict);
        ig_kv_int(&b, "pred_live_verdict", pred_l_verdict);
        ig_kv_int(&b, "anchor_confirmed", anchor_confirmed);
        ig_kv_int(&b, "gua_validated_any_cfg", gua_validated);
        ig_kv_int(&b, "promotion_gate_indicator",
                  anchor_confirmed && gua_validated);
        ig_kv_int(&b, "ctor_events", (int64_t)g_ctor_events);
        ig_kv_int(&b, "fini_events", (int64_t)g_fini_events);
        ig_kv_int(&b, "write_failures", (int64_t)sink->failed);
        ig_kv_str(&b, "note",
                  "indicator only; promotion decided by controller per "
                  "SEGMENT-CORRESPONDENCE rules", 64);
        flush_record(sink, &b);
    }
    /* Quiesce: caller closes the channel; module stays loaded and inert. */
}
