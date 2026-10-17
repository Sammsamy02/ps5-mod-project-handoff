// c2_data_probe.c — worker-c2-probe (team 2026-10-04), igf7- build.
//
// C2(b) ALTERNATIVE CORRESPONDENCE PROBE. Mission per
// worker-c2-alt-correspondence/C2-ALTERNATIVE-SPEC.md section 7:
// read FIVE distinctive immutable DATA windows from the live eboot mapping
// and emit their RAW bytes (hex) — nothing else. The offline controller
// compares them against the retained capture. This TU contains NO expected
// bytes, NO byte_match, NO comparison of any kind (the igf4/igf6 design of
// embedding expected_hex on-console was self-referential; that is removed).
//
// SAFETY MODEL (the igf6 lesson inverted): igf6 died on the FIRST
// exec-span byte read (prot 0x4, record 4 never arrived). This probe reads
// ONLY data windows, and every read is GATED FIRST on the SAME invocation's
// own sysctl KERN_PROC_VMMAP scan (v3 stride-correct walk, mapscan.c):
//   - map untrusted            -> refusal "map_untrusted" (no deref)
//   - no covering span         -> refusal "uncovered" (no deref)
//   - window intersects the DERIVED exec span -> refusal
//     "exec_span_protected" (no deref; belt-and-suspenders: exec spans also
//     fail the prot&1 requirement)
//   - covering span without prot bit0 -> refusal "covered_not_readable"
//   - else: byte-loop read of len <= 64 bytes, ONE record
// A refusal is recorded and the remaining candidates still run (igf4
// discipline). Read order is B, C, D, E, A (A last: the only candidate with
// NO saved map evidence, phdr 8 flags-0 aux segment).
//
// MAP PERSISTENCE (closes the igf6 gap): igf6 counted 8685 readable spans
// but persisted no span list. Here the readable (prot&1) span list is
// emitted as paged "mapspans" records BEFORE the first candidate read, so a
// crash at the first data read still leaves the gating evidence on disk.
//
// GEOMETRY CROSS-CHECKS (spec section 7 step 3): base := lowest prot&4 span
// start (expected 0x400000) and the exec-span extent derived from that span
// (expected 0x747c000 bytes, phdr 0 filesz page-rounded). Both are RECORDED,
// never assumed; the candidate rt addresses are the spec's constants
// (base 0x400000 + vaddr) and the offline controller re-verifies
// rt - vaddr == recorded base == 0x400000.
//
// SEQUENCE (one invocation, one build_id; write-through after every record):
//   seq 3 mapscan          — counters, trust, derived base/exec extent
//   seq 3 mapspans (pages) — persisted readable span list
//   seq 4 cand_read/refusal — B (branch name L"UE5", 8 B)
//   seq 5 cand_read/refusal — C (UTF-16 BuildIdOverride, 32 B)
//   seq 6 cand_read/refusal — D (ASCII Current Changelist, 19 B)
//   seq 7 cand_read/refusal — E (AES S-box first 64 B)
//   seq 8 cand_read/refusal — A (NT_GNU_BUILD_ID note record, 36 B)
//   seq 9 summary          — per-candidate coverage/read/refusal flags,
//                            exec_reads_attempted (always 0), write failures
//
// FORBIDDEN BY CONTRACT AND BY BUILD GATE: raw `syscall` (0 sites asserted),
// mprotect in any form, exec-span dereferences, writes to game memory,
// expected bytes anywhere in the binary.
#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>

/* mapscan.c (vendored, igf6-identical): the same-invocation map. */
extern int ig_mapscan_run(void);
extern int ig_map_cover(uint64_t addr, uint64_t len, IgSpan *out);

extern unsigned g_ctor_events;  /* module_entry.c */
extern unsigned g_fini_events;

void ig_run_c2_data_probe(IgSink *sink);

static int flush_record(IgSink *s, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(s, b) != 0) s->failed = 1;
    return s->failed;
}

static void hex_encode(const uint8_t *b, size_t n, char *out) {
    static const char hexd[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = hexd[(b[i] >> 4) & 15];
        out[2 * i + 1] = hexd[b[i] & 15];
    }
    out[2 * n] = '\0';
}

/* THE ONLY foreign dereference in this module: a byte-loop copy of
   len <= IG_C2_MAX_LEN (64) bytes from one map-gated data window. */
#ifdef IG_C2_HOST_READ_HOOK
/* Host-test hook (offline logic test only; NEVER defined for the console
   build — the console build compiles the plain loop below verbatim). */
static void ig_c2_read_bytes(uint64_t rt, uint8_t *dst, uint32_t len) {
    IG_C2_HOST_READ_HOOK(rt, dst, len);
}
#else
static void ig_c2_read_bytes(uint64_t rt, uint8_t *dst, uint32_t len) {
    const uint8_t *p = (const uint8_t *)(uintptr_t)rt;
    for (uint32_t i = 0; i < len; ++i) dst[i] = p[i];
}
#endif

/* Candidate set, in READ ORDER (spec section 7 step 4: B, C, D, E, A). */
typedef struct C2Cand {
    const char *id;
    const char *what;
    uint64_t vaddr;
    uint64_t rt;        /* spec constant: 0x400000 + vaddr */
    uint64_t file_off;  /* retained-capture provenance for the OFFLINE compare */
    uint32_t len;
} C2Cand;

static const C2Cand g_cands[IG_C2_COUNT] = {
    {"B", "branch_name_UE5", IG_C2_B_VADDR, IG_C2_B_RT, IG_C2_B_FILEOFF,
     IG_C2_B_LEN},
    {"C", "BuildIdOverride_utf16", IG_C2_C_VADDR, IG_C2_C_RT,
     IG_C2_C_FILEOFF, IG_C2_C_LEN},
    {"D", "CurrentChangelist_ascii", IG_C2_D_VADDR, IG_C2_D_RT,
     IG_C2_D_FILEOFF, IG_C2_D_LEN},
    {"E", "AES_sbox_64B", IG_C2_E_VADDR, IG_C2_E_RT, IG_C2_E_FILEOFF,
     IG_C2_E_LEN},
    {"A", "GNU_build_id_note", IG_C2_A_VADDR, IG_C2_A_RT, IG_C2_A_FILEOFF,
     IG_C2_A_LEN},
};

/* Refusal reason codes (summary) + strings (refusal records). */
enum {
    C2_OK_READ = 0,
    C2_REFUSED_MAP_UNTRUSTED = 1,
    C2_REFUSED_UNCOVERED = 2,
    C2_REFUSED_COVERED_NOT_READABLE = 3,
    C2_REFUSED_EXEC_SPAN_PROTECTED = 4,
};

static const char *refusal_string(int code) {
    switch (code) {
    case C2_REFUSED_MAP_UNTRUSTED: return "map_untrusted";
    case C2_REFUSED_UNCOVERED: return "uncovered";
    case C2_REFUSED_COVERED_NOT_READABLE: return "covered_not_readable";
    case C2_REFUSED_EXEC_SPAN_PROTECTED: return "exec_span_protected";
    default: return "";
    }
}

/* Geometry cross-checks derived from THIS invocation's map (spec section 7):
   base := lowest prot&4 span start; exec extent := that span's extent. */
static void derive_geometry(uint64_t *base, int *have_base, uint64_t *ex_s,
                            uint64_t *ex_e) {
    uint32_t n = g_map.walk.retained;
    if (n > IG_SPAN_MAX) n = IG_SPAN_MAX;
    uint64_t b = 0;
    int have = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const IgSpan *s = &g_spans[i];
        if ((s->prot & 4u) && (!have || s->start < b)) { b = s->start; have = 1; }
    }
    *base = b;
    *have_base = have;
    *ex_s = 0;
    *ex_e = 0;
    if (!have) return;
    for (uint32_t i = 0; i < n; ++i) {
        const IgSpan *s = &g_spans[i];
        if ((s->prot & 4u) && s->start == b && s->end > *ex_e) {
            *ex_s = s->start;
            *ex_e = s->end;
        }
    }
}

/* Persist the readable (prot&1) span list as paged records BEFORE any
   candidate read (closes the igf6 unpersisted-map gap). Each span is one
   string field "sN":"0xSTART-0xEND:PROT". */
static void emit_span_pages(IgSink *sink) {
    IgBuf b;
    uint32_t n = g_map.walk.retained;
    if (n > IG_SPAN_MAX) n = IG_SPAN_MAX;
    uint32_t page = 0;
    uint32_t in_page = 0;
    uint32_t total_emitted = 0;
    static const char hexd[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char spanbuf[64];
    char key[16];

    ig_record_begin(&b, 3, "mapspans", 0, 0, IG_BUILD_ID);
    ig_kv_int(&b, "page", 0);
    for (uint32_t i = 0; i < n; ++i) {
        const IgSpan *s = &g_spans[i];
        if (!(s->prot & 1u)) continue;
        if (b.len > IG_RECORD_MAX - 512) {
            ig_kv_int(&b, "spans_in_page", (int64_t)in_page);
            flush_record(sink, &b);
            ++page;
            in_page = 0;
            ig_record_begin(&b, 3, "mapspans", 0, 0, IG_BUILD_ID);
            ig_kv_int(&b, "page", (int64_t)page);
        }
        /* "0x" + 16 + '-' + "0x" + 16 + ':' + up to 3 = 41 chars worst. */
        size_t k = 0;
        spanbuf[k++] = '0'; spanbuf[k++] = 'x';
        for (int shift = 60; shift >= 0; shift -= 4)
            spanbuf[k++] = hexd[(s->start >> (unsigned)shift) & 15];
        spanbuf[k++] = '-';
        spanbuf[k++] = '0'; spanbuf[k++] = 'x';
        for (int shift = 60; shift >= 0; shift -= 4)
            spanbuf[k++] = hexd[(s->end >> (unsigned)shift) & 15];
        spanbuf[k++] = ':';
        {   /* prot is <= 0xff (walk sanity), so <= 3 digits */
            uint32_t v = s->prot;
            char d[4];
            size_t dn = 0;
            do { d[dn++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0);
            while (dn > 0) spanbuf[k++] = d[--dn];
        }
        spanbuf[k] = '\0';

        key[0] = 's';
        size_t kn = 1;
        {   /* decimal page-local index into the key */
            uint32_t v = in_page;
            char d[12];
            size_t dn = 0;
            do { d[dn++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0);
            while (dn > 0) key[kn++] = d[--dn];
        }
        key[kn] = '\0';
        ig_kv_str(&b, key, spanbuf, 48);
        ++in_page;
        ++total_emitted;
    }
    ig_kv_int(&b, "spans_in_page", (int64_t)in_page);
    ig_kv_int(&b, "readable_spans_emitted", (int64_t)total_emitted);
    ig_kv_int(&b, "pages", (int64_t)(page + 1));
    ig_kv_int(&b, "map_trusted", g_map.trusted);
    flush_record(sink, &b);
}

/* One gated candidate attempt (seq 4..8). NEVER dereferences unless the
   same-invocation map proves a readable, non-exec covering span. */
static void run_candidate(IgSink *sink, int seq, const C2Cand *c,
                          uint64_t ex_s, uint64_t ex_e, int have_exec,
                          int *read_performed, int *refusal_code,
                          int *covered) {
    IgBuf b;
    uint8_t live[IG_C2_MAX_LEN];
    char live_hex[2 * IG_C2_MAX_LEN + 1];
    IgSpan span;
    int is_covered = ig_map_cover(c->rt, c->len, &span);
    *covered = is_covered;
    *read_performed = 0;

    int code = C2_OK_READ;
    if (!g_map.trusted) {
        code = C2_REFUSED_MAP_UNTRUSTED;
    } else if (!is_covered) {
        code = C2_REFUSED_UNCOVERED;
    } else if (have_exec && ex_s < (uint64_t)c->rt + c->len &&
               c->rt < ex_e) {
        code = C2_REFUSED_EXEC_SPAN_PROTECTED;  /* never deref exec territory */
    } else if (!(span.prot & 1u)) {
        code = C2_REFUSED_COVERED_NOT_READABLE;
    }

    if (code != C2_OK_READ) {
        *refusal_code = code;
        ig_record_begin(&b, seq, "cand_refusal", 0, 0, IG_BUILD_ID);
        ig_kv_str(&b, "cand", c->id, 4);
        ig_kv_str(&b, "refusal", refusal_string(code), 32);
        ig_kv_hex(&b, "rt_addr", c->rt);
        ig_kv_hex(&b, "vaddr", c->vaddr);
        ig_kv_hex(&b, "image_file_off", c->file_off);
        ig_kv_int(&b, "len", (int64_t)c->len);
        ig_kv_int(&b, "covered", is_covered);
        if (is_covered) {
            ig_kv_hex(&b, "span_start", span.start);
            ig_kv_hex(&b, "span_end", span.end);
            ig_kv_int(&b, "span_prot", (int64_t)span.prot);
        }
        ig_kv_str(&b, "note", "no dereference attempted", 32);
        flush_record(sink, &b);
        return;
    }

    /* GATED READ: covered, prot bit0 set, non-exec window, len <= 64. */
    ig_c2_read_bytes(c->rt, live, c->len);
    *read_performed = 1;
    *refusal_code = C2_OK_READ;
    hex_encode(live, c->len, live_hex);

    /* Record AFTER the read so a fault localizes by absence of this seq. */
    ig_record_begin(&b, seq, "cand_read", 0, 0, IG_BUILD_ID);
    ig_kv_str(&b, "cand", c->id, 4);
    ig_kv_str(&b, "what", c->what, 32);
    ig_kv_hex(&b, "rt_addr", c->rt);
    ig_kv_hex(&b, "vaddr", c->vaddr);
    ig_kv_hex(&b, "image_file_off", c->file_off);
    ig_kv_int(&b, "len", (int64_t)c->len);
    ig_kv_hex(&b, "span_start", span.start);
    ig_kv_hex(&b, "span_end", span.end);
    ig_kv_int(&b, "span_prot", (int64_t)span.prot);
    ig_kv_str(&b, "method_used", "gated_map_covered_data_read", 40);
    ig_kv_str(&b, "live_hex", live_hex, 2 * IG_C2_MAX_LEN);
    flush_record(sink, &b);
}

void ig_run_c2_data_probe(IgSink *sink) {
    IgBuf b;

    /* ---- seq 3: mapscan + geometry cross-checks. THE MAP GATES EVERY
       DEREF (the igf6 inversion is the whole point of this rebuild). ---- */
    int trusted = ig_mapscan_run();
    uint64_t base = 0, ex_s = 0, ex_e = 0;
    int have_base = 0;
    if (trusted) derive_geometry(&base, &have_base, &ex_s, &ex_e);
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
    ig_kv_hex(&b, "derived_base", base);
    ig_kv_int(&b, "have_derived_base", have_base);
    ig_kv_int(&b, "base_matches_expected", have_base && base == IG_IMAGE_BASE);
    ig_kv_hex(&b, "exec_span_start", ex_s);
    ig_kv_hex(&b, "exec_span_end", ex_e);
    ig_kv_hex(&b, "exec_span_bytes", ex_e - ex_s);
    ig_kv_int(&b, "exec_extent_matches_expected",
              have_base && (ex_e - ex_s) == IG_EXEC_EXPECTED_BYTES);
    ig_kv_str(&b, "mission",
              "C2(b) data correspondence; map gates every deref; readable "
              "spans persisted; no expected bytes on console", 96);
    flush_record(sink, &b);

    /* ---- seq 3 (pages): persisted readable span list. ---- */
    emit_span_pages(sink);

    /* ---- seq 4..8: candidates B, C, D, E, A — gated reads or refusals. */
    int read_performed[IG_C2_COUNT] = {0};
    int refusal_code[IG_C2_COUNT] = {0};
    int covered[IG_C2_COUNT] = {0};
    for (int i = 0; i < IG_C2_COUNT; ++i) {
        run_candidate(sink, 4 + i, &g_cands[i], ex_s, ex_e, have_base,
                      &read_performed[i], &refusal_code[i], &covered[i]);
    }

    /* ---- seq 9: summary (state record, NOT a claim; the comparison is
       offline, by the controller, against the retained capture). ---- */
    int reads = 0, refusals = 0;
    for (int i = 0; i < IG_C2_COUNT; ++i) {
        reads += read_performed[i];
        refusals += (refusal_code[i] != C2_OK_READ);
    }
    ig_record_begin(&b, 9, "summary", 0, 0, IG_BUILD_ID);
    ig_kv_int(&b, "map_trusted", g_map.trusted);
    ig_kv_int(&b, "readable_spans", g_map.readable_spans);
    ig_kv_hex(&b, "derived_base", base);
    ig_kv_int(&b, "base_matches_expected", have_base && base == IG_IMAGE_BASE);
    ig_kv_hex(&b, "exec_span_start", ex_s);
    ig_kv_hex(&b, "exec_span_end", ex_e);
    ig_kv_int(&b, "exec_extent_matches_expected",
              have_base && (ex_e - ex_s) == IG_EXEC_EXPECTED_BYTES);
    /* Per-candidate coverage flags under explicit id-prefixed keys:
       "B_covered"/"B_read"/"B_refusal", ... "A_refusal" — parseable without
       positional assumptions. */
    {
        ig_kv_int(&b, "B_covered", covered[0]);
        ig_kv_int(&b, "B_read", read_performed[0]);
        ig_kv_int(&b, "B_refusal", (int64_t)refusal_code[0]);
        ig_kv_int(&b, "C_covered", covered[1]);
        ig_kv_int(&b, "C_read", read_performed[1]);
        ig_kv_int(&b, "C_refusal", (int64_t)refusal_code[1]);
        ig_kv_int(&b, "D_covered", covered[2]);
        ig_kv_int(&b, "D_read", read_performed[2]);
        ig_kv_int(&b, "D_refusal", (int64_t)refusal_code[2]);
        ig_kv_int(&b, "E_covered", covered[3]);
        ig_kv_int(&b, "E_read", read_performed[3]);
        ig_kv_int(&b, "E_refusal", (int64_t)refusal_code[3]);
        ig_kv_int(&b, "A_covered", covered[4]);
        ig_kv_int(&b, "A_read", read_performed[4]);
        ig_kv_int(&b, "A_refusal", (int64_t)refusal_code[4]);
    }
    ig_kv_int(&b, "reads_performed", (int64_t)reads);
    ig_kv_int(&b, "refusals", (int64_t)refusals);
    ig_kv_int(&b, "exec_reads_attempted", 0);
    ig_kv_int(&b, "comparison_on_console", 0);
    ig_kv_int(&b, "ctor_events", (int64_t)g_ctor_events);
    ig_kv_int(&b, "fini_events", (int64_t)g_fini_events);
    ig_kv_int(&b, "write_failures", (int64_t)sink->failed);
    flush_record(sink, &b);
}
