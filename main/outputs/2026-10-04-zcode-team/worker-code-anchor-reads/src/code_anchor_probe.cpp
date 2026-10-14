// code_anchor_probe.cpp — worker-code-anchor-reads (team 2026-10-04).
//
// igf5 MISSION: resolve the ONLY remaining unresolved check from the igf4
// anchor-validation run — the 16 code-anchor bytes at Tick 0x33ac8c0 and
// SCO 0x890aa0. The igf4 safety gate REFUSED these reads because the
// current VMMAP reports the exec span [0x400000, 0x787c000) execute-only
// (prot=0x4, no READ bit). This probe performs the user-authorized reads
// under the CONTROLLER-ORDERED mprotect-FIRST sequence (revises the igf4
// task's "direct read, fall through on fault" design, which was UNSAFE:
// no handler exists — a raw read fault would kill the process before any
// code could react):
//
//   STEP 1 query current protection (current-invocation sysctl
//          KERN_PROC_VMMAP covering span; checkpoint constant 0x4 only as
//          a LABELED fallback), then sceKernelMprotect-equivalent on the
//          SPECIFIC 16 KiB page(s) containing the anchor: add PROT_READ,
//          PRESERVE PROT_EXEC (new prot = 0x5). Nonzero return => the
//          errno is recorded and NO read is attempted (refusal record).
//   STEP 2 read 16 bytes at the anchor (kernel just validated the range).
//   STEP 3 restore the ORIGINAL protection on the same page(s); verify;
//          failure is recorded as an anomaly (never retried blindly).
//   STEP 4 compare against the retained-image expected bytes; diff_mask.
//
// SCOPE DISCIPLINE (task bounds): NO VMMAP dump (mapscan stays internal,
// igf4 correction #6), NO base scan, NO experiments beyond the two anchor
// reads + the GUA predicate-replay hook. The igf4 writeback test and the
// uncorrected predicate are NOT carried. The predicate replay runs ONLY
// when worker-predicate-gc-fix's corrected predicate was compiled in at
// build time; otherwise a status=tbd record is emitted and GUA memory is
// untouched.
//
// FAULT CONTAINMENT: no foreign dereference happens except under kernel
// mprotect SUCCESS for the exact covering pages (the syscall number risk
// is documented and accepted at the controller layer; if the shim's first
// call misbehaves, the refusal/no-read rule still holds by construction).
#include "anchor_store.h"
#include "anchor_pages.h"
#include "generated/ingame_config.h"
#include "sys_mprotect_shim.h"

#if IG_PRED_GC_FIX_PRESENT
#include "townfall_layout_probes.hpp"
#endif

extern "C" {
#include "record_sink.h"
}

#include <stdint.h>

extern "C" void ig_run_code_anchor_probe(IgSink *sink);

extern "C" unsigned g_ctor_events;
extern "C" unsigned g_fini_events;

namespace {

/* Per-anchor outcome for the summary record. */
struct AnchorOut {
    int read_performed;
    int match;        /* 1 match, 0 mismatch, -1 not read */
    int mprotect_ok;  /* mprotect_rc == 0 */
    int restored;     /* restore_rc == 0 (only meaningful when read) */
    int anomaly;      /* restore attempted and failed */
};

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

int flush_record(IgSink *s, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(s, b) != 0) s->failed = 1;
    return s->failed;
}

void mk_key(char *dst, const char *pre, const char *suf) {
    size_t i = 0;
    while (pre && *pre && i < 28) dst[i++] = *pre++;
    while (suf && *suf && i < 60) dst[i++] = *suf++;
    dst[i] = '\0';
}

#if IG_PRED_GC_FIX_PRESENT
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
}
#endif

/* One mprotect-first code-anchor read. seq 4 / 5. */
void run_anchor(IgSink *sink, int seq, const char *exp, uint64_t rt_addr,
                uint64_t vaddr, uint64_t file_off, const char *expected_hex,
                AnchorOut *out) {
    IgBuf b;
    uint8_t live[16];
    uint8_t want[16];
    char live_hex[40];
    live_hex[0] = '\0';
    int match = -1;
    unsigned diff_mask = 0;

    out->read_performed = 0;
    out->match = -1;
    out->mprotect_ok = 0;
    out->restored = 0;
    out->anomaly = 0;

    /* STEP 1a: pages covering the 16-byte read. */
    uint64_t page = 0;
    uint32_t npages = 0;
    int span_ok = igcar_page_range(rt_addr, 16, IG_PAGE_SIZE, &page, &npages);
    uint64_t plen = span_ok == 0 ? (uint64_t)npages * IG_PAGE_SIZE : 0;
    int in_span = span_ok == 0 && page >= IG_EXEC_SPAN_START &&
                  page + plen <= IG_EXEC_SPAN_END;

    /* STEP 1b: current protection = restore value. Current-invocation
       VMMAP covering span preferred; checkpoint constant only as a
       LABELED fallback. Never dumped; one span at most is copied out. */
    IgSpan cover;
    int covered = (span_ok == 0 && g_map.trusted)
                      ? ig_map_cover(page, plen, &cover)
                      : 0;
    int orig_prot = covered ? (int)cover.prot : (int)IG_EXEC_PROT_FALLBACK;
    const char *orig_src =
        covered ? "current_vmmap_cover" : "checkpoint_constant";

    /* STEP 1c: mprotect FIRST (add READ, keep EXEC). */
    long mrc = 0;
    long rrc = 0;
    int m_called = 0;
    int r_called = 0;
    if (in_span) {
        m_called = 1;
        mrc = igcar_sys_mprotect(reinterpret_cast<const void *>(
                                     static_cast<uintptr_t>(page)),
                                 static_cast<size_t>(plen),
                                 IGCAR_PROT_READ | IGCAR_PROT_EXEC);
        out->mprotect_ok = mrc == 0;
    }

    /* STEP 2: read ONLY after mprotect success. STEP 3: restore. */
    if (mrc == 0) {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(rt_addr));
        for (int i = 0; i < 16; ++i) live[i] = p[i];
        out->read_performed = 1;
        hex_encode(live, 16, live_hex);
        hex_to_bytes(expected_hex, want, 16);
        match = igcar_bytes_match(live, want, 16, &diff_mask);
        out->match = match;

        r_called = 1;
        rrc = igcar_sys_mprotect(reinterpret_cast<const void *>(
                                     static_cast<uintptr_t>(page)),
                                 static_cast<size_t>(plen), orig_prot);
        out->restored = rrc == 0;
        out->anomaly = rrc != 0;
    }

    ig_record_begin(&b, seq, exp, (int64_t)(mrc == 0 ? 0 : mrc), 0,
                    IG_BUILD_ID);
    ig_kv_hex(&b, "rt_addr", rt_addr);
    ig_kv_hex(&b, "profile_vaddr", vaddr);
    ig_kv_hex(&b, "image_file_off", file_off);
    ig_kv_str(&b, "expected_src",
              "decrypted-eboot.bin sha16=" IG_IMAGE_SHA16
              " off=vaddr+0x4000", 64);
    ig_kv_str(&b, "expected_bytes_hex", expected_hex, 32);
    ig_kv_str(&b, "method_used",
              out->read_performed ? "mprotect_then_read"
                                  : (in_span ? "refused_mprotect_failed"
                                             : "refused_outside_exec_span"),
              40);
    ig_kv_hex(&b, "page_start", page);
    ig_kv_int(&b, "page_size", (int64_t)IG_PAGE_SIZE);
    ig_kv_int(&b, "pages", (int64_t)(span_ok == 0 ? npages : 0));
    ig_kv_int(&b, "orig_prot", orig_prot);
    ig_kv_str(&b, "orig_prot_source", orig_src, 24);
    ig_kv_int(&b, "mprotect_called", m_called);
    ig_kv_int(&b, "mprotect_rc", mrc);
    ig_kv_hex(&b, "mprotect_errno_hex", mrc < 0 ? (uint64_t)(-mrc) : 0);
    ig_kv_int(&b, "mprotect_prot_requested", IGCAR_PROT_READ | IGCAR_PROT_EXEC);
    ig_kv_str(&b, "syscall_impl", "inline_syscall_SYS_mprotect_pinned_sdk",
              40);
    ig_kv_int(&b, "read_performed", out->read_performed);
    if (out->read_performed) {
        ig_kv_str(&b, "bytes_read_hex", live_hex, 32);
        ig_kv_int(&b, "bytes_read", 16);
        ig_kv_int(&b, "byte_match", match);
        ig_kv_int(&b, "diff_mask", (int64_t)diff_mask);
    }
    ig_kv_int(&b, "restore_called", r_called);
    ig_kv_int(&b, "restore_rc", rrc);
    ig_kv_int(&b, "restore_prot", orig_prot);
    ig_kv_int(&b, "mprotect_restored", out->read_performed ? out->restored : 0);
    if (out->anomaly) {
        ig_kv_int(&b, "anomaly", 1);
        ig_kv_str(&b, "anomaly_note",
                  "restore_mprotect_failed_original_protection_not_reverified",
                  64);
    }
    if (!out->read_performed) {
        ig_kv_str(&b, "refusal",
                  in_span ? "mprotect_failed_no_read_attempted"
                          : "page_range_outside_known_exec_span", 40);
    }
    flush_record(sink, &b);
}

}  // namespace

extern "C" void ig_run_code_anchor_probe(IgSink *sink) {
    IgBuf b;

    /* ---- seq 3: mapscan (internal coverage + restore-prot source; NO
       dump). Unlike igf4 the map does NOT gate the anchors: the kernel
       mprotect success is the access safety. It DOES gate the predicate
       replay feeds below. ---- */
    int trusted = ig_mapscan_run();
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
    ig_kv_str(&b, "note",
              "internal coverage + restore-prot source; anchors are "
              "mprotect-first; no map dump", 64);
    flush_record(sink, &b);

    /* ---- seq 4/5: the code anchors (THE unresolved check). ---- */
    AnchorOut tick, sco;
    run_anchor(sink, 4, "tick_anchor", IG_RT_UGAMEENGINE_TICK,
               IG_VADDR_UGAMEENGINE_TICK, IG_TICK_FILE_OFF,
               IG_TICK_EXPECTED_HEX, &tick);
    run_anchor(sink, 5, "sco_anchor", IG_RT_STATIC_CONSTRUCT_OBJECT,
               IG_VADDR_STATIC_CONSTRUCT_OBJECT, IG_SCO_FILE_OFF,
               IG_SCO_EXPECTED_HEX, &sco);

    /* ---- seq 6: GUA predicate replay with the CORRECTED GC-order check.
       Runs ONLY when worker-predicate-gc-fix's corrected predicate was
       compiled in at build time; otherwise status=tbd and GUA memory is
       untouched. ---- */
    int pred_performed = 0;
    int pred_validated = -1;  /* -1 tbd / not run, 0 no, 1 yes */
    int pred_h_verdict = -1;
    int pred_l_verdict = -1;
#if IG_PRED_GC_FIX_PRESENT
    {
        /* GUA live read (96 bytes), gated by the trusted current map —
           same gating as the igf4 lineage. */
        IgSpan cover;
        int gate = g_map.trusted
                       ? (ig_map_cover(IG_RT_GUOBJECT_ARRAY, 96, &cover)
                              ? ((cover.prot & 1u) ? 0 : 3)
                              : 2)
                       : 1;
        uint8_t gua[96];
        ig_record_begin(&b, 6, "gua_pred_replay", gate, 0, IG_BUILD_ID);
        ig_kv_str(&b, "status", "performed", 16);
        ig_kv_int(&b, "corrected_gc_order_present", 1);
        ig_kv_hex(&b, "rt_addr", IG_RT_GUOBJECT_ARRAY);
        ig_kv_hex(&b, "profile_vaddr", IG_VADDR_GUOBJECT_ARRAY);
        ig_kv_int(&b, "bytes", 96);
        if (gate == 0) {
            const uint8_t *p = reinterpret_cast<const uint8_t *>(
                static_cast<uintptr_t>(IG_RT_GUOBJECT_ARRAY));
            for (int i = 0; i < 96; ++i) gua[i] = p[i];
            ig_kv_hex(&b, "span_prot", cover.prot);
            /* cfgH: header-span-only feed (task-literal). */
            tlp::MemSpan hs[1];
            hs[0].start = IG_RT_GUOBJECT_ARRAY;
            hs[0].size = 96;
            hs[0].data = reinterpret_cast<const uint8_t *>(
                static_cast<uintptr_t>(IG_RT_GUOBJECT_ARRAY));
            tlp::GuaProbeResult rh = tlp::probe_guobject_array(
                hs, 1, IG_IMAGE_BASE, IG_VADDR_GUOBJECT_ARRAY);
            pred_h_verdict = (int)rh.verdict;
            emit_pred(&b, "ph", rh);
            /* cfgL: current readable spans feed. STATIC: the lineage keeps
               the span table out of the game-thread stack (igf4 did the
               same for its s_mem). */
            static tlp::MemSpan s_cfgl[4096];
            uint32_t n = g_map.walk.retained;
            if (n > 4096u) n = 4096u;
            uint32_t mc = 0;
            for (uint32_t i = 0; i < n; ++i) {
                if ((g_spans[i].prot & 1u) == 0) continue;
                s_cfgl[mc].start = g_spans[i].start;
                s_cfgl[mc].size = g_spans[i].end - g_spans[i].start;
                s_cfgl[mc].data = reinterpret_cast<const uint8_t *>(
                    static_cast<uintptr_t>(g_spans[i].start));
                ++mc;
            }
            tlp::GuaProbeResult rl = tlp::probe_guobject_array(
                s_cfgl, mc, IG_IMAGE_BASE, IG_VADDR_GUOBJECT_ARRAY);
            pred_l_verdict = (int)rl.verdict;
            emit_pred(&b, "pl", rl);
            pred_validated =
                (pred_h_verdict == (int)tlp::Verdict::Validated) ||
                (pred_l_verdict == (int)tlp::Verdict::Validated);
            pred_performed = 1;
        } else {
            ig_kv_str(&b, "refusal",
                      gate == 1 ? "map_untrusted"
                                : (gate == 2 ? "not_covered_current_map"
                                             : "covered_not_readable"), 32);
        }
        flush_record(sink, &b);
    }
#else
    {
        ig_record_begin(&b, 6, "gua_pred_replay", 0, 0, IG_BUILD_ID);
        ig_kv_str(&b, "status", "tbd", 16);
        ig_kv_int(&b, "corrected_gc_order_present", 0);
        ig_kv_str(&b, "reason",
                  "worker-predicate-gc-fix not delivered at build time; "
                  "hook prepared, GUA memory untouched", 64);
        ig_kv_str(&b, "hook", "rebuild with the fixed townfall_layout_probes "
                              "cpp under src/generated-gcfix to enable",
                  64);
        flush_record(sink, &b);
    }
#endif

    /* ---- seq 7: summary (indicator, NOT a claim). ---- */
    {
        int anchors_confirmed =
            (tick.read_performed && tick.match == 1) &&
            (sco.read_performed && sco.match == 1);
        int anomalies = (tick.anomaly ? 1 : 0) + (sco.anomaly ? 1 : 0);
        ig_record_begin(&b, 7, "summary", 0, 0, IG_BUILD_ID);
        ig_kv_int(&b, "map_trusted", g_map.trusted);
        ig_kv_int(&b, "tick_read_performed", tick.read_performed);
        ig_kv_int(&b, "tick_match", tick.match);
        ig_kv_int(&b, "tick_mprotect_ok", tick.mprotect_ok);
        ig_kv_int(&b, "tick_restored", tick.restored);
        ig_kv_int(&b, "sco_read_performed", sco.read_performed);
        ig_kv_int(&b, "sco_match", sco.match);
        ig_kv_int(&b, "sco_mprotect_ok", sco.mprotect_ok);
        ig_kv_int(&b, "sco_restored", sco.restored);
        ig_kv_int(&b, "mprotect_anomalies", anomalies);
        ig_kv_int(&b, "pred_replay_performed", pred_performed);
        ig_kv_int(&b, "pred_header_verdict", pred_h_verdict);
        ig_kv_int(&b, "pred_live_verdict", pred_l_verdict);
        ig_kv_int(&b, "pred_validated_any_cfg", pred_validated);
        ig_kv_int(&b, "anchors_confirmed", anchors_confirmed);
        ig_kv_int(&b, "promotion_gate_indicator_anchors_only",
                  anchors_confirmed);
        ig_kv_int(&b, "promotion_gate_indicator",
                  anchors_confirmed && pred_validated == 1);
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
