// exec_write_probe.c — worker-writability-probe-build (team 2026-10-04).
//
// PRE-BUILT mprotect-transition machinery for the worker-exec-writability
// design. EVERYTHING here is host-tested (tests/ suite, mocked mprotect);
// NOTHING has executed on console. The design fills transitions via
// TRANSITIONS.json -> generated/xwp_transitions.h; THIS FILE IS NOT EDITED
// by the design step (injection without rebuilding the machinery).
//
// Per-attempt sequence (igf7 discipline, brief-mandated):
//   1. row record (what the design asked for, verbatim)
//   2. fresh same-invocation sysctl KERN_PROC_VMMAP scan (mapscan.c; the
//      anchor-validation rule: never trust a previous invocation's map)
//   3. vmmap_persist record: the covering span's FULL pre-state BEFORE the
//      attempt (start/end/prot + walk trust stats)
//   4. mprotect attempt (the ONLY protection-change call; import-only —
//      zero raw syscalls; skeleton builds bind a stub that does nothing)
//   5. attempt record (rc + errno immediately captured)
//   6. gated read/write test with recovery: read of ONE byte (console-
//      proven safe by igf6: x86-64 NX gates EXECUTE, not READ), and ONLY
//      when to_prot carries W, a content-preserving write of that same byte
//      back (recovery = the pre-read byte value is rewritten; no signal
//      handlers exist in this lineage — fault localization is by write-
//      through records already on disk, the igf6 safety rationale)
//   7. restore attempt to from_prot (ALWAYS, even after a failed attempt)
//   8. restore record + post_restore record (fresh VMMAP re-read proving
//      the observed prot is back to from_prot — or loudly not)
//
// Fail-open: every path through module_start returns 0; this file never
// returns nonzero and never terminates the caller.
#include "exec_write_probe.h"

#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "ingame_libc.h"

#include <stdint.h>

/* ---- mprotect binding ------------------------------------------------ */
#if !IG_XWP_MPROTECT_ARMED
/* SKELETON build: compile-time unarmed. The stub performs NO protection
   change and NEVER touches memory; it exists so the full record/decision
   sequence is exercised byte-for-byte on console with the EXACT 7
   console-proven imports (zero new identities). */
static int igx_sys_mprotect_stub(void *a, size_t n, int p) {
    (void)a; (void)n; (void)p;
    int *e = __error();
    if (e) *e = 22; /* EINVAL */
    return -1;
}
static const char *const igx_mprotect_route = "stub_unarmed";
#else
/* ARMED build: the ONE derived import row (sceKernelMprotect, libkernel —
   SharpProspero StubCatalog spelling; NOT console-proven; LOUD flag in
   CLAIM.json + import-contract.json). The typed reference below is what
   makes the packager carry the 8th row; with an EMPTY table this build
   still performs zero mprotect calls (load-resolution sanity stage). */
extern int sceKernelMprotect(void *, size_t, int);
static const char *const igx_mprotect_route = "sceKernelMprotect_import";
#endif

static xwp_mprotect_fn g_xwp_mprotect =
#if !IG_XWP_MPROTECT_ARMED
    igx_sys_mprotect_stub;
#else
    sceKernelMprotect;
#endif

void ig_xwp_set_mprotect_for_host_test(xwp_mprotect_fn fn) {
    g_xwp_mprotect = fn; /* host tests only; never called on console */
}

unsigned g_xwp_attempts_used;
unsigned g_xwp_rows_seen;
unsigned g_xwp_rows_attempted;
unsigned g_xwp_rows_refused;
unsigned g_xwp_rows_skipped;

static unsigned g_xwp_mirror_failures;

static int xwp_errno(void) {
    int *e = __error();
    return e ? *e : 0;
}

/* Write one record: jsonl primary first, mirror second (best-effort).
   Returns 0 iff the PRIMARY channel accepted the record. */
static int xwp_write_record(IgSink *jsonl, IgSink *mirror, IgBuf *b) {
    int rc = ig_sink_write_record(jsonl, b);
    if (mirror != 0 && mirror->fd >= 0) {
        if (ig_sink_write_record(mirror, b) != 0) ++g_xwp_mirror_failures;
    }
    return rc;
}

static void xwp_kv_outcome(IgBuf *b, const char *key, XwpOutcome o) {
    ig_kv_int(b, key, (int64_t)o);
}

/* The single place an mprotect-class call may happen. */
static int xwp_mprotect_call(uint64_t addr, uint64_t len, uint64_t prot,
                             int *eno, XwpOutcome *out_ok) {
    int rc = g_xwp_mprotect((void *)(uintptr_t)addr, (size_t)len,
                            (int)prot);
    *eno = xwp_errno();
    if (rc == 0) {
        *out_ok = XWP_ATTEMPT_OK;
    } else if (!IG_XWP_MPROTECT_ARMED) {
        *out_ok = XWP_MPROTECT_IMPORT_ABSENT; /* stub reached: recorded, safe */
    } else {
        *out_ok = XWP_ATTEMPT_MPROTECT_FAIL;
    }
    return rc;
}

int ig_run_exec_write_probe(IgSink *jsonl, IgSink *mirror) {
    IgBuf b;
    unsigned seq = 3; /* 1 entry + 2 startup are module_entry's */
    g_xwp_rows_seen = 0;
    g_xwp_rows_attempted = 0;
    g_xwp_rows_refused = 0;
    g_xwp_rows_skipped = 0;
    g_xwp_mirror_failures = 0;

    if (xwp_transitions_count == 0) {
        ig_record_begin(&b, (int)seq, "table_empty", 0, 0, IG_BUILD_ID);
        ig_kv_str(&b, "phase", "exec_write_probe", 32);
        ig_kv_str(&b, "mprotect_route", igx_mprotect_route, 32);
        ig_kv_int(&b, "armed", IG_XWP_MPROTECT_ARMED);
        ig_kv_int(&b, "attempt_budget", XWP_ATTEMPT_BUDGET);
        ig_kv_str(&b, "note", "EMPTY placeholder table: design fills "
                              "TRANSITIONS.json and rebuilds", 80);
        ig_record_flush(&b);
        (void)xwp_write_record(jsonl, mirror, &b);
        return 0;
    }

    for (unsigned i = 0; i < xwp_transitions_count; ++i) {
        const XwpTransition *t = &xwp_transitions[i];
        ++g_xwp_rows_seen;

        /* ---- (1) row record: the design's request, verbatim. ---- */
        ig_record_begin(&b, (int)seq, "row", 0, 0, IG_BUILD_ID);
        ig_kv_int(&b, "row_index", (int64_t)i);
        ig_kv_hex(&b, "addr", t->addr);
        ig_kv_hex(&b, "from_prot", t->from_prot);
        ig_kv_hex(&b, "to_prot", t->to_prot);
        ig_kv_int(&b, "page_count", (int64_t)t->page_count);
        ig_kv_int(&b, "confirmed", (int64_t)t->confirmed);
        ig_kv_str(&b, "note", t->note ? t->note : "", XWP_NOTE_MAX);
        ig_kv_int(&b, "attempts_used", (int64_t)g_xwp_attempts_used);
        ig_record_flush(&b);
        if (xwp_write_record(jsonl, mirror, &b) != 0) return 0;
        ++seq;

        /* ---- gates in fail-closed order; every refusal recorded.
           NOTE: there is deliberately NO unarmed skip gate here — the
           SKELETON stage runs the FULL sequence (persist -> stub attempt
           -> records -> restore -> re-proof) with the compile-time stub,
           proving the machinery on console with ZERO memory risk. The
           stub cannot change any protection. ---- */
        XwpOutcome why;
        int gate_failed = 0;
        if (i >= XWP_MAX_ROWS) {
            why = XWP_SKIP_ROW_OVERFLOW;
            gate_failed = 1;
        } else if (t->confirmed == 0) {
            why = XWP_SKIP_NOT_CONFIRMED;
            gate_failed = 1;
        } else if (g_xwp_attempts_used >= XWP_ATTEMPT_BUDGET) {
            why = XWP_SKIP_BUDGET;
            gate_failed = 1;
        }

        uint64_t len = t->page_count * 0x1000ull;
        IgSpan cover;
        int covered = 0;
        if (!gate_failed) {
            /* Fresh CURRENT-invocation scan; nothing is trusted stale. */
            int trusted = ig_mapscan_run();
            covered = trusted && ig_map_cover(t->addr, len, &cover);
            if (!covered) {
                why = XWP_REFUSE_NOT_COVERED;
                gate_failed = 1;
            } else if ((uint64_t)cover.prot != t->from_prot) {
                why = XWP_REFUSE_PROT_MISMATCH;
                gate_failed = 1;
            }
        }

        if (gate_failed) {
            if (why == XWP_SKIP_UNARMED || why == XWP_SKIP_NOT_CONFIRMED ||
                why == XWP_SKIP_BUDGET || why == XWP_SKIP_ROW_OVERFLOW) {
                ++g_xwp_rows_skipped;
            } else {
                ++g_xwp_rows_refused;
            }
            ig_record_begin(&b, (int)seq, "refusal", 0, 0, IG_BUILD_ID);
            ig_kv_int(&b, "row_index", (int64_t)i);
            xwp_kv_outcome(&b, "outcome", why);
            ig_kv_int(&b, "no_memory_touched", 1);
            if (covered) {
                ig_kv_hex(&b, "observed_span_start", cover.start);
                ig_kv_hex(&b, "observed_span_end", cover.end);
                ig_kv_hex(&b, "observed_prot", cover.prot);
            }
            ig_record_flush(&b);
            if (xwp_write_record(jsonl, mirror, &b) != 0) return 0;
            ++seq;
            continue;
        }

        /* ---- (3) vmmap_persist: PRE-state evidence before the attempt. */
        ig_record_begin(&b, (int)seq, "vmmap_persist", 0, 0, IG_BUILD_ID);
        ig_kv_int(&b, "row_index", (int64_t)i);
        ig_kv_hex(&b, "span_start", cover.start);
        ig_kv_hex(&b, "span_end", cover.end);
        ig_kv_hex(&b, "span_prot", cover.prot);
        ig_kv_hex(&b, "attempt_addr", t->addr);
        ig_kv_int(&b, "attempt_len", (int64_t)len);
        ig_kv_int(&b, "walk_trusted", 1);
        ig_kv_int(&b, "walk_retained", (int64_t)g_map.walk.retained);
        ig_kv_int(&b, "walk_insane", (int64_t)g_map.walk.insane);
        ig_record_flush(&b);
        if (xwp_write_record(jsonl, mirror, &b) != 0) return 0;
        ++seq;

        /* ---- (4)+(5) the attempt + its rc/errno record. ---- */
        ++g_xwp_attempts_used;
        ++g_xwp_rows_attempted;
        int eno = 0;
        XwpOutcome ok_code = XWP_ATTEMPT_MPROTECT_FAIL;
        int rc = xwp_mprotect_call(t->addr, len, t->to_prot, &eno, &ok_code);
        ig_record_begin(&b, (int)seq, "attempt", rc, eno, IG_BUILD_ID);
        ig_kv_int(&b, "row_index", (int64_t)i);
        ig_kv_int(&b, "attempt_index", (int64_t)g_xwp_attempts_used);
        xwp_kv_outcome(&b, "outcome", ok_code);
        ig_kv_hex(&b, "requested_prot", t->to_prot);
        ig_kv_str(&b, "mprotect_route", igx_mprotect_route, 32);
        ig_record_flush(&b);
        if (xwp_write_record(jsonl, mirror, &b) != 0) return 0;
        ++seq;

        /* ---- (6) gated read/write test with recovery. ---- */
        if (rc == 0) {
            volatile uint8_t *p =
                (volatile uint8_t *)(uintptr_t)t->addr;
            uint8_t orig = *p;   /* NX gates EXECUTE not READ (igf6-proven) */
            int write_tested = (t->to_prot & XWP_PROT_W) ? 1 : 0;
            int write_ok = 0;
            int recovered = 0;
            if (write_tested) {
                *p = orig;       /* content-preserving: the same byte back */
                uint8_t back = *p;
                write_ok = 1;
                recovered = (back == orig);
            }
            ig_record_begin(&b, (int)seq, "rw_test", 0, 0, IG_BUILD_ID);
            ig_kv_int(&b, "row_index", (int64_t)i);
            ig_kv_hex(&b, "first_byte", orig);
            ig_kv_int(&b, "read_done", 1);
            ig_kv_int(&b, "write_tested", write_tested);
            ig_kv_int(&b, "write_ok", write_ok);
            ig_kv_int(&b, "recovered", recovered);
            ig_record_flush(&b);
            if (xwp_write_record(jsonl, mirror, &b) != 0) return 0;
            ++seq;
        }

        /* ---- (7)+(8) restore ALWAYS, then post-restore VMMAP proof. */
        int reno = 0;
        XwpOutcome restore_code = XWP_RESTORE_FAIL;
        int rrc = xwp_mprotect_call(t->addr, len, t->from_prot, &reno,
                                    &restore_code);
        if (rrc == 0) restore_code = XWP_RESTORE_OK;
        ig_record_begin(&b, (int)seq, "restore", rrc, reno, IG_BUILD_ID);
        ig_kv_int(&b, "row_index", (int64_t)i);
        xwp_kv_outcome(&b, "outcome", restore_code);
        ig_kv_hex(&b, "restored_prot", t->from_prot);
        ig_record_flush(&b);
        if (xwp_write_record(jsonl, mirror, &b) != 0) return 0;
        ++seq;

        int restored_matches = 0;
        int post_covered = 0;
        uint32_t post_prot = 0;
        int post_trusted = ig_mapscan_run();
        if (post_trusted && ig_map_cover(t->addr, len, &cover)) {
            post_covered = 1;
            post_prot = cover.prot;
            restored_matches = ((uint64_t)post_prot == t->from_prot);
        }
        ig_record_begin(&b, (int)seq, "post_restore", 0, 0, IG_BUILD_ID);
        ig_kv_int(&b, "row_index", (int64_t)i);
        ig_kv_int(&b, "post_walk_trusted", post_trusted);
        ig_kv_int(&b, "post_covered", post_covered);
        ig_kv_hex(&b, "post_observed_prot", post_prot);
        ig_kv_int(&b, "restored_matches_from_prot", restored_matches);
        ig_record_flush(&b);
        if (xwp_write_record(jsonl, mirror, &b) != 0) return 0;
        ++seq;
    }

    /* ---- summary: budget state + mirror health, still fail-open. ---- */
    ig_record_begin(&b, (int)seq, "summary", 0, 0, IG_BUILD_ID);
    ig_kv_str(&b, "phase", "exec_write_probe_done", 32);
    ig_kv_int(&b, "rows_seen", (int64_t)g_xwp_rows_seen);
    ig_kv_int(&b, "rows_attempted", (int64_t)g_xwp_rows_attempted);
    ig_kv_int(&b, "rows_refused", (int64_t)g_xwp_rows_refused);
    ig_kv_int(&b, "rows_skipped", (int64_t)g_xwp_rows_skipped);
    ig_kv_int(&b, "attempts_used", (int64_t)g_xwp_attempts_used);
    ig_kv_int(&b, "attempt_budget", XWP_ATTEMPT_BUDGET);
    ig_kv_int(&b, "budget_exhausted",
              g_xwp_attempts_used >= XWP_ATTEMPT_BUDGET);
    ig_kv_int(&b, "mirror_failures", (int64_t)g_xwp_mirror_failures);
    ig_kv_int(&b, "armed", IG_XWP_MPROTECT_ARMED);
    ig_kv_str(&b, "mprotect_route", igx_mprotect_route, 32);
    ig_record_flush(&b);
    (void)xwp_write_record(jsonl, mirror, &b);
    return 0; /* fail-open, always */
}
