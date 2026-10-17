// code_anchor_direct.cpp — worker-igf5-diagnosis-rebuild (team 2026-10-04).
//
// igf6 MISSION: resolve the Tick/SCO code-anchor reads with the SIMPLEST
// possible mechanism — a PLAIN DIRECT DEREFERENCE (16-byte memcpy-class
// read). This rebuild follows the Part 1 binary diagnosis of igf5
// (igf5-4186048a6dd6): the igf5 binary was structurally IDENTICAL to the
// known-good igf4 at every loader-parsed level (same fself wrapper fields,
// same program/section layout, same 36-entry dynamic table, same 7 NID
// imports, same 2 exports, same 23 relocations, same 1-entry init/fini
// arrays); the ONLY execution-relevant delta was the inline mprotect
// syscall shim (raw `syscall`, rax=74) — 2 sites in igf5, 0 sites in igf4.
// The shim is REMOVED here entirely.
//
// WHY A DIRECT READ IS SAFE HERE (task Part 2 justification, recorded):
//   (1) both addresses are KNOWN code addresses inside the mapped exec
//       span [IG_EXEC_SPAN_START, IG_EXEC_SPAN_END) from the igf4 VMMAP
//       evidence (single-load bias base=0x400000);
//   (2) x86-64 hardware: the NX bit gates EXECUTE, not READ; pages mapped
//       executable are hardware-readable (no execute-only PTE read-fault);
//   (3) the VMMAP prot=0x4 is kernel policy accounting, not hardware
//       enforcement of read faults;
//   (4) the igf4 gate REFUSED on (prot & 1)==0; the hardware would have
//       allowed the read. igf4's refusal — not a fault — is the proof the
//       gate never tested hardware readability.
// The read itself is `live[i] = p[i]` over 16 bytes. NO mprotect, NO raw
// syscall, NO permission gate, NO handler. If hardware ever DID fault, the
// write-through record channel (sceKernelWrite per record) leaves seq 1..3
// on disk and the failure localizes to the first missing read record.
//
// SEQUENCE (one invocation, one build_id):
//   seq 3 mapscan         — console-proven sysctl KERN_PROC_VMMAP walk,
//                           recorded for context. INFORMATIONAL ONLY: it
//                           does NOT gate the direct reads (prot bits are
//                           policy, per (2)-(4) above).
//   seq 4 tick_anchor     — direct read 16B @ IG_RT_UGAMEENGINE_TICK
//                           (0x33ac8c0), compare vs retained-image bytes.
//   seq 5 sco_anchor      — direct read 16B @ IG_RT_STATIC_CONSTRUCT_OBJECT
//                           (0x890aa0), compare vs retained-image bytes.
//   seq 6 gua_pred_replay — direct read 96B @ IG_RT_GUOBJECT_ARRAY into a
//                           STACK copy; the worker-predicate-gc-fix
//                           CORRECTED predicate replays the structural
//                           layout check over the cfgH header-span feed
//                           (data pointer = stack copy: the predicate
//                           performs ZERO live-memory dereferences of its
//                           own; out-of-span pointer targets are
//                           Refuted-class by construction). cfgL is
//                           recorded as not_run (span-chase feed withheld
//                           by design in the igf6 minimal rebuild).
//   seq 7 summary         — indicators, NOT claims.
#include "anchor_pages.h"
#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "townfall_layout_probes.hpp"

extern "C" {
#include "record_sink.h"
}

#include <stdint.h>

extern "C" void ig_run_direct_anchor_probe(IgSink *sink);

extern "C" unsigned g_ctor_events;
extern "C" unsigned g_fini_events;

namespace {

void hex_encode(const uint8_t *b, size_t n, char *out) {
    static const char hexd[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
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
        v |= (uint8_t)((lo >= '0' && lo <= '9') ? (lo - '0')
                                                : (lo - 'a' + 10));
        out[i] = v;
    }
}

int flush_record(IgSink *s, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(s, b) != 0) s->failed = 1;
    return s->failed;
}

void emit_pred(IgBuf *b, const char *pre, const tlp::GuaProbeResult &r) {
    char k[64];
    auto mk = [&](const char *suf) {
        size_t i = 0;
        while (*pre && i < 28) k[i++] = *pre++;
        const char *s = suf;
        while (*s && i < 60) k[i++] = *s++;
        k[i] = '\0';
    };
    mk("_verdict");
    ig_kv_str(b, k, tlp::verdict_string(r.verdict), 24);
    mk("_rc0");
    ig_kv_str(b, k, r.code_count > 0 ? tlp::rcode_string(r.codes[0]) : "", 40);
    mk("_la_verdict");
    ig_kv_str(b, k, tlp::verdict_string(r.layout_a.verdict), 24);
    mk("_la_rc");
    ig_kv_str(b, k, r.layout_a.code_count > 0
                        ? tlp::rcode_string(r.layout_a.codes[0]) : "", 40);
    mk("_lb_verdict");
    ig_kv_str(b, k, tlp::verdict_string(r.layout_b.verdict), 24);
    mk("_lb_rc");
    ig_kv_str(b, k, r.layout_b.code_count > 0
                        ? tlp::rcode_string(r.layout_b.codes[0]) : "", 40);
}

/* Per-anchor outcome for the summary record. */
struct AnchorOut {
    int read_performed;
    int match;      /* 1 match, 0 mismatch, -1 not read */
    int in_span;    /* informational: page span inside checkpoint exec span */
};

/* One DIRECT (no-syscall) code-anchor read. seq 4 / 5. */
void run_direct_anchor(IgSink *sink, int seq, const char *exp,
                       uint64_t rt_addr, uint64_t vaddr, uint64_t file_off,
                       const char *expected_hex, AnchorOut *out) {
    IgBuf b;
    uint8_t live[16];
    uint8_t want[16];
    char live_hex[40];
    live_hex[0] = '\0';

    out->read_performed = 0;
    out->match = -1;
    out->in_span = 0;

    /* Informational page bookkeeping (pure arithmetic; NEVER gates). */
    uint64_t page = 0;
    uint32_t npages = 0;
    int span_ok = igcar_page_range(rt_addr, 16, IG_PAGE_SIZE, &page, &npages);
    uint64_t plen = span_ok == 0 ? (uint64_t)npages * IG_PAGE_SIZE : 0;
    out->in_span = (span_ok == 0 && page >= IG_EXEC_SPAN_START &&
                    page + plen <= IG_EXEC_SPAN_END) ? 1 : 0;

    /* THE DIRECT READ — plain dereference, no gate, no syscall. */
    const uint8_t *p = reinterpret_cast<const uint8_t *>(
        static_cast<uintptr_t>(rt_addr));
    for (int i = 0; i < 16; ++i) live[i] = p[i];
    out->read_performed = 1;
    hex_encode(live, 16, live_hex);
    hex_to_bytes(expected_hex, want, 16);
    unsigned diff_mask = 0;
    out->match = igcar_bytes_match(live, want, 16, &diff_mask);

    /* Record AFTER the read so a fault localizes by absence of this seq. */
    ig_record_begin(&b, seq, exp, 0, 0, IG_BUILD_ID);
    ig_kv_hex(&b, "rt_addr", rt_addr);
    ig_kv_hex(&b, "profile_vaddr", vaddr);
    ig_kv_hex(&b, "image_file_off", file_off);
    ig_kv_str(&b, "expected_src",
              "decrypted-eboot.bin sha16=" IG_IMAGE_SHA16
              " off=vaddr+0x4000", 64);
    ig_kv_str(&b, "expected_bytes_hex", expected_hex, 32);
    ig_kv_str(&b, "method_used", "direct_dereference_no_syscall", 40);
    ig_kv_hex(&b, "page_start", page);
    ig_kv_int(&b, "page_size", (int64_t)IG_PAGE_SIZE);
    ig_kv_int(&b, "pages", (int64_t)(span_ok == 0 ? npages : 0));
    ig_kv_int(&b, "in_known_exec_span", out->in_span);
    ig_kv_str(&b, "bytes_read_hex", live_hex, 32);
    ig_kv_int(&b, "bytes_read", 16);
    ig_kv_int(&b, "byte_match", out->match);
    ig_kv_int(&b, "diff_mask", (int64_t)diff_mask);
    flush_record(sink, &b);
}

}  // namespace

extern "C" void ig_run_direct_anchor_probe(IgSink *sink) {
    IgBuf b;

    /* ---- seq 3: mapscan (context record only; does NOT gate the direct
       reads — the igf4 lineage proved the sysctl path loads and logs; the
       VMMAP prot bits are policy, not hardware enforcement). ---- */
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
              "context only; direct reads are UNGATED (NX gates exec, not "
              "read); no map dump", 64);
    flush_record(sink, &b);

    /* ---- seq 4/5: the code anchors (THE unresolved check), direct. ---- */
    AnchorOut tick, sco;
    run_direct_anchor(sink, 4, "tick_anchor", IG_RT_UGAMEENGINE_TICK,
                      IG_VADDR_UGAMEENGINE_TICK, IG_TICK_FILE_OFF,
                      IG_TICK_EXPECTED_HEX, &tick);
    run_direct_anchor(sink, 5, "sco_anchor", IG_RT_STATIC_CONSTRUCT_OBJECT,
                      IG_VADDR_STATIC_CONSTRUCT_OBJECT, IG_SCO_FILE_OFF,
                      IG_SCO_EXPECTED_HEX, &sco);

    /* ---- seq 6: GUA predicate replay, CORRECTED GC-order predicate
       (worker-predicate-gc-fix vendored-fixed), cfgH header-span feed over
       a STACK copy of the 96 direct-read bytes. The predicate's data
       pointer is the stack copy: it performs zero live-memory dereferences
       beyond this TU's own reads. cfgL deliberately not run (minimal
       rebuild). ---- */
    int pred_performed = 0;
    int pred_validated = 0;
    int pred_h_verdict = -1;
    {
        uint8_t gua[96];
        const uint8_t *p = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(IG_RT_GUOBJECT_ARRAY));
        for (int i = 0; i < 96; ++i) gua[i] = p[i];

        ig_record_begin(&b, 6, "gua_pred_replay", 0, 0, IG_BUILD_ID);
        ig_kv_str(&b, "status", "performed", 16);
        ig_kv_int(&b, "corrected_gc_order_present", 1);
        ig_kv_hex(&b, "rt_addr", IG_RT_GUOBJECT_ARRAY);
        ig_kv_hex(&b, "profile_vaddr", IG_VADDR_GUOBJECT_ARRAY);
        ig_kv_int(&b, "bytes", 96);
        ig_kv_str(&b, "feed", "cfgH_header_span_stack_copy", 32);
        tlp::MemSpan hs[1];
        hs[0].start = IG_RT_GUOBJECT_ARRAY;
        hs[0].size = 96;
        hs[0].data = gua;  /* stack copy: no live deref inside predicate */
        tlp::GuaProbeResult rh = tlp::probe_guobject_array(
            hs, 1, IG_IMAGE_BASE, IG_VADDR_GUOBJECT_ARRAY);
        pred_h_verdict = (int)rh.verdict;
        emit_pred(&b, "ph", rh);
        pred_performed = 1;
        pred_validated = (pred_h_verdict == (int)tlp::Verdict::Validated);
        ig_kv_str(&b, "cfgL_status",
                  "not_run_minimal_rebuild_cfgH_only", 40);
        flush_record(sink, &b);
    }

    /* ---- seq 7: summary (indicator, NOT a claim). ---- */
    {
        int anchors_confirmed =
            (tick.read_performed && tick.match == 1) &&
            (sco.read_performed && sco.match == 1);
        ig_record_begin(&b, 7, "summary", 0, 0, IG_BUILD_ID);
        ig_kv_int(&b, "map_trusted", g_map.trusted);
        ig_kv_int(&b, "tick_read_performed", tick.read_performed);
        ig_kv_int(&b, "tick_match", tick.match);
        ig_kv_int(&b, "tick_in_known_exec_span", tick.in_span);
        ig_kv_int(&b, "sco_read_performed", sco.read_performed);
        ig_kv_int(&b, "sco_match", sco.match);
        ig_kv_int(&b, "sco_in_known_exec_span", sco.in_span);
        ig_kv_int(&b, "pred_replay_performed", pred_performed);
        ig_kv_int(&b, "pred_header_verdict", pred_h_verdict);
        ig_kv_int(&b, "pred_validated_any_cfg", pred_validated);
        ig_kv_int(&b, "anchors_confirmed", anchors_confirmed);
        ig_kv_int(&b, "promotion_gate_indicator_anchors_only",
                  anchors_confirmed);
        ig_kv_int(&b, "promotion_gate_indicator",
                  anchors_confirmed && pred_validated == 1);
        ig_kv_int(&b, "ctor_events", (int64_t)g_ctor_events);
        ig_kv_int(&b, "fini_events", (int64_t)g_fini_events);
        ig_kv_int(&b, "write_failures", (int64_t)sink->failed);
        flush_record(sink, &b);
    }
}
