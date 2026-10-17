// fc1_probe.cpp — worker-fname-runtime-probe (team 2026-10-04).
//
// FC-1 runtime verification ADD-ON for the NEXT probe iteration (igf6 or a
// combined igf5 build). Validates the FName ctor call-site candidate at
// runtime 0x63d590 (vaddr 0x23d590, exec span) per
// worker-fname-fc1-materialize/FC1-CHECKS.md:
//
//   FC1-A  64-byte prologue read at the candidate (first 32 bytes hard
//          gate + full 64-byte compare; diff masks + first divergence)
//   FC1-B  the 9 recorded call-site windows, decoded branch-free:
//          LEA RSI,[rip+d32]->anchor string, exactly one LEA RDI store,
//          exactly one MOV EDX,1, no branch/RSI write, E8 terminator
//          resolving to 0x63d590
//   FC1-C  fail-closed uniqueness posture: any disagreeing site fails the
//          whole verdict (recorded in fc1_summary)
//   FC1-D  UTF-16LE anchor strings (data region) — gated behind a trusted
//          readable-span map check, run only when A+B pass; never gates
//          the code verdict
//
// MPROTECT-FIRST (igf5 corrected design, NON-NEGOTIABLE):
//   page-range(16 KiB) -> orig prot (current map, labeled fallback) ->
//   mprotect(page, READ|EXEC) -> READ ONLY ON SUCCESS -> restore orig ->
//   restore failure recorded as anomaly. mprotect failure => refusal
//   record and NO read. No raw read of an exec-only page anywhere.
//
// Integration: call ig_run_fc1_probe(&sink) from the next probe's
// module_start chain AFTER its mapscan (the probe uses g_map only for the
// restore-protection source and the FC1-D gate; it does NOT require the
// map to be trusted). See INTEGRATION.md.
#include "fc1_probe.h"

#include "anchor_store.h"
#include "fc1_config.h"
#include "fc1_pages.h"
#include "fc1_windows.h"
#if defined(IGFC1_HOST_TEST)
/* Host test build: deterministic SOFTWARE protection model over the
   synthetic arenas (tests/host-shims). The real syscall shim cannot run
   on this host. */
#include "fc1_host_mprotect_shim.h"
#else
/* PRX/target build: the UNMODIFIED igf5-lineage inline-syscall shim. */
#include "sys_mprotect_shim.h"
#endif

#include <stdint.h>

namespace {

/* One mprotect-first read of [addr, addr+len) into out (len <= 69).
   Returns 0 when the read was performed, else the refusal code:
   -1 page range invalid, -2 outside the known exec span,
   -3 mprotect failed (NO read attempted). */
struct Fc1ReadOut {
    uint64_t page;
    uint32_t npages;
    int page_range_rc;
    int in_span;
    int m_called;
    long mrc;
    int read_done;
    int r_called;
    long rrc;
    int restored;
    int orig_prot;
    const char *orig_src;  /* "current_vmmap_cover" | "checkpoint_constant" */
};

int fc1_mprotected_read(uint64_t addr, uint32_t len, uint8_t *out,
                        Fc1ReadOut *ro) {
    ro->page = 0;
    ro->npages = 0;
    ro->page_range_rc = 0;
    ro->in_span = 0;
    ro->m_called = 0;
    ro->mrc = 0;
    ro->read_done = 0;
    ro->r_called = 0;
    ro->rrc = 0;
    ro->restored = 0;
    ro->orig_prot = 0;
    ro->orig_src = "";

    /* STEP 1a: covering 16 KiB page range. */
    ro->page_range_rc = igfc1_page_range(addr, len, IGFC1_PAGE_SIZE,
                                         &ro->page, &ro->npages);
    if (ro->page_range_rc != 0) return -1;
    uint64_t plen = (uint64_t)ro->npages * IGFC1_PAGE_SIZE;
    ro->in_span = ro->page >= IGFC1_EXEC_SPAN_START &&
                  ro->page + plen <= IGFC1_EXEC_SPAN_END;
    if (!ro->in_span) return -2;          /* refusal: outside known exec span */

    /* STEP 1b: original protection = restore value (current map preferred,
       checkpoint constant as LABELED fallback; never dumped). */
    IgSpan cover;
    int covered = g_map.trusted ? ig_map_cover(ro->page, plen, &cover) : 0;
    ro->orig_prot = covered ? (int)cover.prot : (int)IGFC1_EXEC_PROT_FALLBACK;
    ro->orig_src = covered ? "current_vmmap_cover" : "checkpoint_constant";

    /* STEP 1c: mprotect FIRST (add READ, keep EXEC). */
    ro->m_called = 1;
    ro->mrc = igcar_sys_mprotect(
        reinterpret_cast<const void *>(static_cast<uintptr_t>(ro->page)),
        static_cast<size_t>(plen), IGFC1_PROT_READ | IGFC1_PROT_EXEC);
    if (ro->mrc != 0) return -3;          /* refusal: NO read attempted */

    /* STEP 2: read (kernel just validated the exact range). */
    const uint8_t *p =
        reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(addr));
    for (uint32_t i = 0; i < len; ++i) out[i] = p[i];
    ro->read_done = 1;

    /* STEP 3: restore the ORIGINAL protection; anomaly on failure. */
    ro->r_called = 1;
    ro->rrc = igcar_sys_mprotect(
        reinterpret_cast<const void *>(static_cast<uintptr_t>(ro->page)),
        static_cast<size_t>(plen), ro->orig_prot);
    ro->restored = ro->rrc == 0;
    return 0;
}

void record_read_facts(IgBuf *b, const Fc1ReadOut *ro) {
    ig_kv_hex(b, "page_start", ro->page);
    ig_kv_int(b, "page_size", (int64_t)IGFC1_PAGE_SIZE);
    ig_kv_int(b, "pages", (int64_t)ro->npages);
    ig_kv_int(b, "orig_prot", ro->orig_prot);
    ig_kv_str(b, "orig_prot_source", ro->orig_src, 24);
    ig_kv_int(b, "mprotect_called", ro->m_called);
    ig_kv_int(b, "mprotect_rc", ro->mrc);
    ig_kv_hex(b, "mprotect_errno_hex", ro->mrc < 0 ? (uint64_t)(-ro->mrc) : 0);
    ig_kv_int(b, "mprotect_prot_requested", IGFC1_PROT_READ | IGFC1_PROT_EXEC);
    ig_kv_str(b, "syscall_impl", "inline_syscall_SYS_mprotect_pinned_sdk", 40);
    ig_kv_int(b, "read_performed", ro->read_done);
    ig_kv_int(b, "restore_called", ro->r_called);
    ig_kv_int(b, "restore_rc", ro->rrc);
    ig_kv_int(b, "restore_prot", ro->orig_prot);
    ig_kv_int(b, "mprotect_restored", ro->read_done ? ro->restored : 0);
    if (ro->read_done && !ro->restored) {
        ig_kv_int(b, "anomaly", 1);
        ig_kv_str(b, "anomaly_note",
                  "restore_mprotect_failed_original_protection_not_reverified",
                  64);
    }
}

void record_refusal(IgBuf *b, const Fc1ReadOut *ro) {
    ig_kv_str(b, "method_used", "refused", 16);
    if (ro->page_range_rc != 0) {
        ig_kv_int(b, "refusal_code", ro->page_range_rc);
        ig_kv_str(b, "refusal", "page_range_invalid", 32);
    } else if (!ro->in_span) {
        ig_kv_int(b, "refusal_code", -2);
        ig_kv_str(b, "refusal", "page_range_outside_known_exec_span", 40);
    } else {
        ig_kv_int(b, "refusal_code", -3);
        ig_kv_str(b, "refusal", "mprotect_failed_no_read_attempted", 40);
    }
}

/* Tiny hex parser for the config byte strings. Returns byte count written,
   negative on malformed input. */
int fc1_hex_to_bytes(const char *hex, uint8_t *out, uint32_t max_bytes) {
    uint32_t n = 0;
    while (hex[2 * n] && hex[2 * n + 1]) {
        if (n >= max_bytes) return -1;
        uint8_t v = 0;
        for (int k = 0; k < 2; ++k) {
            char c = k == 0 ? hex[2 * n] : hex[2 * n + 1];
            uint8_t d;
            if (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
            else if (c >= 'a' && c <= 'f') d = (uint8_t)(c - 'a' + 10);
            else return -1;
            v = (uint8_t)((v << 4) | d);
        }
        out[n++] = v;
    }
    return (int)n;
}

int flush_record(IgSink *s, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(s, b) != 0) s->failed = 1;
    return s->failed;
}

const char *const kAnchorNames[FC1_NANCHORS] = {
    IGFC1_ANCHOR0_NAME, IGFC1_ANCHOR1_NAME, IGFC1_ANCHOR2_NAME,
    IGFC1_ANCHOR3_NAME, IGFC1_ANCHOR4_NAME};

/* ---- FC1-A: candidate prologue read + compare ----------------------- */

struct Fc1AOut {
    int read_performed;
    int match32;
    int match64;
    unsigned diff32;
    unsigned diff64;
    int first_divergence;   /* -1 none */
    int mprotect_ok;
    int restored;
    int anomaly;
};

void run_fc1_a(IgSink *sink, int seq, const Fc1Env *env, Fc1AOut *ao) {
    IgBuf b;
    uint8_t live[64];
    uint8_t want[64];
    char live_hex[129];
    live_hex[0] = '\0';

    ao->read_performed = 0;
    ao->match32 = -1;
    ao->match64 = -1;
    ao->diff32 = 0;
    ao->diff64 = 0;
    ao->first_divergence = -1;
    ao->mprotect_ok = 0;
    ao->restored = 0;
    ao->anomaly = 0;

    int want_n = fc1_hex_to_bytes(IGFC1_TARGET64_HEX, want, 64);
    if (want_n != 64) {
        /* Config malformed: loud config_error record, NO memory access. */
        ig_record_begin(&b, seq, "fc1_a_candidate", -99, 0, IGFC1_BUILD_ID);
        ig_kv_int(&b, "config_error", 1);
        ig_kv_str(&b, "config_error_note",
                  "IGFC1_TARGET64_HEX must be exactly 64 bytes", 56);
        flush_record(sink, &b);
        return;
    }

    Fc1ReadOut ro;
    int rc = fc1_mprotected_read(env->target_rt, 64, live, &ro);
    ao->mprotect_ok = ro.m_called && ro.mrc == 0;
    if (rc == 0) {
        ao->read_performed = 1;
        igfc1_hex_encode(live, 64, live_hex);
        igfc1_bytes_match(live, want, 32, &ao->diff32);
        ao->match32 = ao->diff32 == 0 ? 1 : 0;
        igfc1_bytes_match(live + 32, want + 32, 32, &ao->diff64);
        ao->match64 = (ao->diff32 | ao->diff64) == 0 ? 1 : 0;
        if (ao->match64 == 0) {
            unsigned all = ao->diff32 | ao->diff64;
            for (int i = 0; i < 64; ++i)
                if (all & (1u << i)) { ao->first_divergence = i; break; }
        }
        ao->restored = ro.restored;
        ao->anomaly = !ro.restored;
    }

    ig_record_begin(&b, seq, "fc1_a_candidate",
                    rc == 0 ? 0 : (rc == -3 ? ro.mrc : rc), 0,
                    IGFC1_BUILD_ID);
    ig_kv_hex(&b, "rt_addr", env->target_rt);
    ig_kv_hex(&b, "profile_vaddr", env->target_vaddr);
    ig_kv_hex(&b, "image_file_off", env->target_vaddr + env->load0_delta);
    ig_kv_str(&b, "expected_src",
              "decrypted-eboot.bin sha16=" IGFC1_IMAGE_SHA16
              " off=vaddr+0x4000; cross=expected-bytes+fc1-materialize", 96);
    ig_kv_str(&b, "expected_bytes_hex", IGFC1_TARGET64_HEX, 128);
    ig_kv_str(&b, "check_contract",
              "FC1-A: first 32 bytes hard gate; full 64-byte compare", 64);
    record_read_facts(&b, &ro);
    if (rc == 0) {
        ig_kv_str(&b, "method_used", "mprotect_then_read", 40);
        ig_kv_str(&b, "bytes_read_hex", live_hex, 128);
        ig_kv_int(&b, "bytes_read", 64);
        ig_kv_int(&b, "match32", ao->match32);
        ig_kv_hex(&b, "diff_mask32", ao->diff32);
        ig_kv_int(&b, "match64", ao->match64);
        ig_kv_hex(&b, "diff_mask64", ao->diff64);
        ig_kv_int(&b, "first_divergence", ao->first_divergence);
    } else {
        record_refusal(&b, &ro);
    }
    flush_record(sink, &b);
}

/* ---- FC1-B: one call-site window ------------------------------------ */

void run_fc1_site(IgSink *sink, int seq, const Fc1Env *env,
                  const Fc1SiteSpec *site, int site_index, int *site_pass) {
    IgBuf b;
    uint8_t buf[80];
    char read_hex[161];
    read_hex[0] = '\0';

    uint32_t len = (uint32_t)(site->call_rt - site->window_rt) + 5;
    Fc1ReadOut ro;
    Fc1WindowOut wo;
    int t12 = -1;
    uint64_t t12_target = 0;

    int rc = len <= 69 ? fc1_mprotected_read(site->window_rt, len, buf, &ro)
                       : -1;
    if (rc == 0) {
        igfc1_hex_encode(buf, len, read_hex);
        /* T1/T2 standalone check (recorded even if the walk fails later). */
        t12 = igfc1_check_call_bytes(buf + (len - 5), site->call_rt,
                                     env->target_rt, &t12_target);
        igfc1_decode_window(buf, len, site->window_rt, site->call_rt,
                            env->target_rt, site->anchor_str_rt, &wo);
    } else {
        wo.status = FC1_E_UNSUPPORTED;
        wo.decoded_len = 0;
        wo.lea_rsi_target = 0;
        wo.rdi_writes = 0;
        wo.rdx_writes = 0;
        wo.rdi_disp = 0;
        wo.call_target_rt = 0;
        wo.call_byte = 0;
    }
    *site_pass = (rc == 0 && wo.status == FC1_WOK && t12 == 0);

    ig_record_begin(&b, seq, "fc1_b_site",
                    rc == 0 ? (wo.status == FC1_WOK ? 0 : wo.status) : rc, 0,
                    IGFC1_BUILD_ID);
    ig_kv_int(&b, "site_index", site_index);
    ig_kv_str(&b, "anchor",
              site->anchor < FC1_NANCHORS ? kAnchorNames[site->anchor] : "?",
              24);
    ig_kv_hex(&b, "window_rt", site->window_rt);
    ig_kv_hex(&b, "window_vaddr", site->window_vaddr);
    ig_kv_hex(&b, "call_rt", site->call_rt);
    ig_kv_hex(&b, "call_vaddr", site->call_vaddr);
    ig_kv_hex(&b, "anchor_str_rt", site->anchor_str_rt);
    ig_kv_int(&b, "window_len", (int64_t)len);
    record_read_facts(&b, &ro);
    if (rc == 0) {
        ig_kv_str(&b, "method_used", "mprotect_then_read", 40);
        ig_kv_str(&b, "bytes_read_hex", read_hex, 160);
        ig_kv_int(&b, "bytes_read", (int64_t)len);
        ig_kv_int(&b, "t1_e8_ok", buf[len - 5] == 0xe8 ? 1 : 0);
        ig_kv_int(&b, "t2_rel32_target_ok", t12 == 0 ? 1 : 0);
        ig_kv_hex(&b, "t2_rel32_target", t12_target);
        ig_kv_str(&b, "decode_status", igfc1_status_string(wo.status), 40);
        ig_kv_int(&b, "decoded_len", (int64_t)wo.decoded_len);
        ig_kv_hex(&b, "lea_rsi_target", wo.lea_rsi_target);
        ig_kv_int(&b, "lea_rsi_target_ok",
                  wo.lea_rsi_target == site->anchor_str_rt ? 1 : 0);
        ig_kv_int(&b, "rdi_writes", (int64_t)wo.rdi_writes);
        ig_kv_hex(&b, "rdi_disp", (uint64_t)wo.rdi_disp);
        ig_kv_int(&b, "rdx_writes", (int64_t)wo.rdx_writes);
        ig_kv_hex(&b, "call_target_rt", wo.call_target_rt);
        ig_kv_int(&b, "site_pass", *site_pass);
    } else {
        record_refusal(&b, &ro);
        ig_kv_int(&b, "site_pass", 0);
    }
    flush_record(sink, &b);
}

/* ---- FC1-D: gated anchor strings (data region) -----------------------
   ONE record with INDEX-PREFIXED keys (d0_*, d1_*, ...) so it stays a
   single strict JSON object; FC1-D never gates the code verdict. */

/* Compose "<dN>_<suffix>" into out (cap >= 24). Returns out. */
const char *fc1_dkey(int idx, const char *suffix, char *out, uint32_t cap) {
    uint32_t p = 0;
    out[p++] = 'd';
    out[p++] = (char)('0' + idx);
    out[p++] = '_';
    while (*suffix && p + 1 < cap) out[p++] = *suffix++;
    out[p] = '\0';
    return out;
}

void run_fc1_d(IgSink *sink, int seq, const Fc1Env *env, int a_ok, int b_ok,
               int *performed, int *matches) {
    IgBuf b;
    *performed = 0;
    *matches = 0;
    int gate = 0;  /* 0 run, 4 a/b failed, 1 map untrusted */
    if (!a_ok || !b_ok) gate = 4;
    else if (!g_map.trusted) gate = 1;

    ig_record_begin(&b, seq, "fc1_d_anchor_strings", gate, 0, IGFC1_BUILD_ID);
    ig_kv_str(&b, "gating",
              "only_if_fc1a_and_fc1b_pass; readable-span map gate; "
              "never gates the code verdict", 72);
    if (gate != 0) {
        ig_kv_str(&b, "status",
                  gate == 4 ? "skipped_a_or_b_failed"
                            : "refused_map_untrusted", 32);
        if (gate == 4)
            ig_kv_str(&b, "note",
                      "code verdict already failed; no data reads", 48);
        else
            ig_kv_str(&b, "note",
                      "anchor strings NOT dereferenced without a trusted "
                      "readable covering span", 64);
        flush_record(sink, &b);
        return;
    }

    for (uint32_t i = 0; i < env->nanchors; ++i) {
        const Fc1AnchorSpec *an = &env->anchors[i];
        char k[32];
        int ok = -1;
        int refused = 0;
        uint8_t live[48];
        char live_hex[97];
        live_hex[0] = '\0';
        IgSpan cover;
        int covered = ig_map_cover(an->content_rt, an->u16_len, &cover);
        if (!covered) {
            refused = 2;
        } else if ((cover.prot & 1u) == 0) {
            refused = 3;
        } else {
            const uint8_t *p = reinterpret_cast<const uint8_t *>(
                static_cast<uintptr_t>(an->content_rt));
            for (uint16_t kk = 0; kk < an->u16_len && kk < 48; ++kk)
                live[kk] = p[kk];
            unsigned dm = 0;
            ok = igfc1_bytes_match(live, an->u16, (int)an->u16_len, &dm) == -1
                     ? 1
                     : 0;
            igfc1_hex_encode(live, an->u16_len, live_hex);
            *performed = 1;
            *matches += ok;
        }
        ig_kv_str(&b, fc1_dkey((int)i, "name", k, sizeof k), an->name, 24);
        ig_kv_hex(&b, fc1_dkey((int)i, "content_rt", k, sizeof k),
                  an->content_rt);
        ig_kv_int(&b, fc1_dkey((int)i, "expect_len", k, sizeof k),
                  an->u16_len);
        ig_kv_int(&b, fc1_dkey((int)i, "covered", k, sizeof k), covered);
        if (covered)
            ig_kv_hex(&b, fc1_dkey((int)i, "span_prot", k, sizeof k),
                      cover.prot);
        if (ok >= 0) {
            ig_kv_str(&b, fc1_dkey((int)i, "bytes_read_hex", k, sizeof k),
                      live_hex, 96);
            ig_kv_int(&b, fc1_dkey((int)i, "utf16_match", k, sizeof k), ok);
        } else {
            ig_kv_str(&b, fc1_dkey((int)i, "refusal", k, sizeof k),
                      refused == 2 ? "not_covered_current_map"
                                   : "covered_not_readable", 32);
        }
    }
    flush_record(sink, &b);
}

}  // namespace

extern "C" void ig_run_fc1_probe_env(IgSink *sink, const Fc1Env *env) {
    IgBuf b;
    Fc1AOut ao;

    /* FC1-A (base+0). */
    run_fc1_a(sink, IGFC1_SEQ_BASE, env, &ao);

    /* FC1-B (base+1..base+nsites). */
    int site_pass[FC1_NSITES];
    uint32_t nsites = env->nsites > FC1_NSITES ? FC1_NSITES : env->nsites;
    for (uint32_t i = 0; i < nsites; ++i)
        run_fc1_site(sink, (int)(IGFC1_SEQ_BASE + 1 + i), env,
                     &env->sites[i], (int)i, &site_pass[i]);

    /* FC1-C: fail-closed tally.
       - a site disagreeing while the candidate prologue holds => ambiguity
         => fail closed (FC1-CHECKS.md section 3);
       - a refuted A with all sites passing => specific refutation, recorded
         via match32/match64 (Tick/SCO outcomes live in the host probe). */
    uint32_t passed = 0;
    for (uint32_t i = 0; i < nsites; ++i)
        if (site_pass[i]) ++passed;
    int a_hard = ao.read_performed && ao.match32 == 1;
    int a_full = a_hard && ao.match64 == 1;
    int fail_closed = a_hard && passed != nsites;
    int fc1_validated = a_full && passed == nsites;

    /* FC1-D (base+10): gated. */
    int d_performed = 0;
    int d_matches = 0;
    run_fc1_d(sink, (int)(IGFC1_SEQ_BASE + 10), env, a_full, passed == nsites,
              &d_performed, &d_matches);

    /* FC1 summary (base+11). */
    ig_record_begin(&b, (int)(IGFC1_SEQ_BASE + 11), "fc1_summary", 0, 0,
                    IGFC1_BUILD_ID);
    ig_kv_int(&b, "fc1a_read_performed", ao.read_performed);
    ig_kv_int(&b, "fc1a_match32", ao.match32);
    ig_kv_int(&b, "fc1a_match64", ao.match64);
    ig_kv_hex(&b, "fc1a_diff_mask32", ao.diff32);
    ig_kv_hex(&b, "fc1a_diff_mask64", ao.diff64);
    ig_kv_int(&b, "fc1a_first_divergence", ao.first_divergence);
    ig_kv_int(&b, "fc1a_mprotect_ok", ao.mprotect_ok);
    ig_kv_int(&b, "fc1a_restored", ao.restored);
    ig_kv_int(&b, "fc1a_anomaly", ao.anomaly);
    ig_kv_int(&b, "sites_total", (int64_t)nsites);
    ig_kv_int(&b, "sites_passed", (int64_t)passed);
    ig_kv_int(&b, "fc1_c_fail_closed", fail_closed);
    ig_kv_int(&b, "fc1d_performed", d_performed);
    ig_kv_int(&b, "fc1d_matches", (int64_t)d_matches);
    ig_kv_int(&b, "fc1_validated_indicator", fc1_validated);
    ig_kv_str(&b, "fc1_note",
              "indicator only; A-mismatch refutes the FName candidate "
              "specifically while Tick/SCO anchors hold (controller holds "
              "those outcomes); D mismatch would refute the data bias, "
              "not the code candidate", 96);
    ig_kv_int(&b, "write_failures", (int64_t)sink->failed);
    flush_record(sink, &b);
}

/* ---- config-driven tables (the igf6 build path) ---------------------- */

extern "C" void ig_run_fc1_probe(IgSink *sink) {
    static Fc1SiteSpec sites[FC1_NSITES];
    static Fc1AnchorSpec anchors[FC1_NANCHORS];
    static uint8_t anchor_u16[FC1_NANCHORS][48];

    static const struct {
        uint8_t anchor;
        uint64_t wv, cv, w_rt, c_rt, s_rt;
        const char *call_hex;
    } cfg[FC1_NSITES] = {
        {0, IGFC1_SITE0_WINDOW_VADDR, IGFC1_SITE0_CALL_VADDR,
         IGFC1_SITE0_WINDOW_RT, IGFC1_SITE0_CALL_RT, IGFC1_SITE0_ANCHOR_RT,
         IGFC1_SITE0_CALL_HEX},
        {0, IGFC1_SITE1_WINDOW_VADDR, IGFC1_SITE1_CALL_VADDR,
         IGFC1_SITE1_WINDOW_RT, IGFC1_SITE1_CALL_RT, IGFC1_SITE1_ANCHOR_RT,
         IGFC1_SITE1_CALL_HEX},
        {1, IGFC1_SITE2_WINDOW_VADDR, IGFC1_SITE2_CALL_VADDR,
         IGFC1_SITE2_WINDOW_RT, IGFC1_SITE2_CALL_RT, IGFC1_SITE2_ANCHOR_RT,
         IGFC1_SITE2_CALL_HEX},
        {1, IGFC1_SITE3_WINDOW_VADDR, IGFC1_SITE3_CALL_VADDR,
         IGFC1_SITE3_WINDOW_RT, IGFC1_SITE3_CALL_RT, IGFC1_SITE3_ANCHOR_RT,
         IGFC1_SITE3_CALL_HEX},
        {1, IGFC1_SITE4_WINDOW_VADDR, IGFC1_SITE4_CALL_VADDR,
         IGFC1_SITE4_WINDOW_RT, IGFC1_SITE4_CALL_RT, IGFC1_SITE4_ANCHOR_RT,
         IGFC1_SITE4_CALL_HEX},
        {1, IGFC1_SITE5_WINDOW_VADDR, IGFC1_SITE5_CALL_VADDR,
         IGFC1_SITE5_WINDOW_RT, IGFC1_SITE5_CALL_RT, IGFC1_SITE5_ANCHOR_RT,
         IGFC1_SITE5_CALL_HEX},
        {2, IGFC1_SITE6_WINDOW_VADDR, IGFC1_SITE6_CALL_VADDR,
         IGFC1_SITE6_WINDOW_RT, IGFC1_SITE6_CALL_RT, IGFC1_SITE6_ANCHOR_RT,
         IGFC1_SITE6_CALL_HEX},
        {3, IGFC1_SITE7_WINDOW_VADDR, IGFC1_SITE7_CALL_VADDR,
         IGFC1_SITE7_WINDOW_RT, IGFC1_SITE7_CALL_RT, IGFC1_SITE7_ANCHOR_RT,
         IGFC1_SITE7_CALL_HEX},
        {4, IGFC1_SITE8_WINDOW_VADDR, IGFC1_SITE8_CALL_VADDR,
         IGFC1_SITE8_WINDOW_RT, IGFC1_SITE8_CALL_RT, IGFC1_SITE8_ANCHOR_RT,
         IGFC1_SITE8_CALL_HEX},
    };
    for (int i = 0; i < FC1_NSITES; ++i) {
        sites[i].anchor = cfg[i].anchor;
        sites[i].window_vaddr = cfg[i].wv;
        sites[i].call_vaddr = cfg[i].cv;
        sites[i].window_rt = cfg[i].w_rt;
        sites[i].call_rt = cfg[i].c_rt;
        sites[i].anchor_str_rt = cfg[i].s_rt;
        fc1_hex_to_bytes(cfg[i].call_hex, sites[i].call_bytes, 5);
    }

    static const char *const u16hex[FC1_NANCHORS] = {
        IGFC1_ANCHOR0_U16_HEX, IGFC1_ANCHOR1_U16_HEX, IGFC1_ANCHOR2_U16_HEX,
        IGFC1_ANCHOR3_U16_HEX, IGFC1_ANCHOR4_U16_HEX};
    static const uint64_t content_rt[FC1_NANCHORS] = {
        IGFC1_ANCHOR0_CONTENT_RT, IGFC1_ANCHOR1_CONTENT_RT,
        IGFC1_ANCHOR2_CONTENT_RT, IGFC1_ANCHOR3_CONTENT_RT,
        IGFC1_ANCHOR4_CONTENT_RT};
    for (int i = 0; i < FC1_NANCHORS; ++i) {
        anchors[i].name = kAnchorNames[i];
        anchors[i].content_rt = content_rt[i];
        anchors[i].u16 = anchor_u16[i];
        int n = fc1_hex_to_bytes(u16hex[i], anchor_u16[i], 48);
        anchors[i].u16_len = (uint16_t)(n > 0 ? n : 0);
    }

    Fc1Env env;
    env.target_vaddr = IGFC1_VADDR_FNAME_CTOR;
    env.target_rt = IGFC1_RT_FNAME_CTOR;
    env.image_base = IGFC1_IMAGE_BASE;
    env.load0_delta = IGFC1_LOAD0_DELTA;
    env.sites = sites;
    env.nsites = FC1_NSITES;
    env.anchors = anchors;
    env.nanchors = FC1_NANCHORS;
    ig_run_fc1_probe_env(sink, &env);
}
