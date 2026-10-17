// experiments.c
//
// V1..V5 loader-fact experiments for the v2 ISOLATION probe PRX
// (worker-ingame-probe-v2, team 2026-10-04). Vendored from
// worker-ingame-probe/src/experiments.c (G1..G7) with every derived-import
// call path REMOVED: no sceKernelGetModuleList/Info, no exec-handle, no
// dlopen/dlclose/dlsym — the import set is exactly the 7 console-proven
// verbatim identities. Base discovery is now IMPORT-FREE: the sysctl
// KERN_PROC_VMMAP span set feeds an in-memory ELF scan (V2).
//
// Record order: entry(1), startup(2) [written by module_entry.c], then
//   V1 vmmap_full   — complete bounded mapping list {start,end,prot} +
//                     histogram (chunked v1_vmmap_page records);
//   V2 elf_base_scan— candidate image bases from RX spans (+readable spans
//                     adjacent to exec spans), ELF magic + program headers
//                     read IN-MEMORY, dynamic-section parse evidence, main
//                     eboot candidate = largest executable image whose
//                     dynamic parses (chunked v2_scan_page records);
//   V3 dtdebug      — DT_DEBUG at the chosen base's _DYNAMIC + bounded
//                     link-map walk (v1 G2 logic adapted, no imports);
//   V4 layout_predicates — vendored worker-5 predicates at chosen_base +
//                     profile vaddrs; needs-base skip ONLY if no candidate
//                     parsed;
//   V5 fw_summary   — sysctl KERN_VERSION + summary.
//
// Fault containment (v1 discipline, kept): the VMMAP span snapshot is
// collected FIRST and EVERY foreign dereference — ELF header, each phdr,
// each dynamic entry, r_debug, each link_map node, each l_name string — is
// validated against readable (prot bit0) spans BEFORE the dereference;
// refused reads increment blocked counters and stop that walk, never fault.
#include "experiments.h"
#include "factstore.h"
#include "ingame_abi.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>
#include <errno.h>
#include <sys/types.h>   /* u_int* for SDK elf headers, ahead of them */
#include <sys/elf64.h>
#include <sys/link_elf.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <time.h>
#include <unistd.h>

/* Fallback spellings (normally provided by the pinned SDK elf headers). */
#ifndef PT_LOAD
#define PT_LOAD 1
#endif
#ifndef PT_DYNAMIC
#define PT_DYNAMIC 2
#endif
#ifndef PF_X
#define PF_X 0x1
#endif
#ifndef DT_NULL
#define DT_NULL 0
#endif
#ifndef DT_DEBUG
#define DT_DEBUG 21
#endif
#ifndef ELFCLASS64
#define ELFCLASS64 2
#endif

IgFacts g_facts;
unsigned g_ctor_events;
unsigned g_fini_events;

/* ------------------------------------------------------------------ */
/* Readability validation against the early VMMAP snapshot (V1 data). */
int ig_range_readable(uint64_t addr, uint64_t len) {
    if (len == 0) return 1;
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        if ((s->prot & 1u) && addr >= s->start &&
            addr - s->start < s->end - s->start &&
            s->end - addr >= len)
            return 1;
    }
    return 0;
}

size_t ig_copy_checked(uint64_t addr, void *dst, size_t len) {
    if (!ig_range_readable(addr, len)) return 0;
    ig_memcpy(dst, (const void *)(uintptr_t)addr, len);
    return len;
}

static void record_out(IgBuf *b, int *seq, const char *exp,
                       int64_t rc, int64_t eno) {
    ig_record_begin(b, ++(*seq), exp, rc, eno, IG_BUILD_ID);
}

/* Common tail: flush + bounded write; write failures counted for V5. */
static void emit(IgSink *sink, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(sink, b) != 0) sink->failed = 1;
}

/* ------------------------------------------------------------------ */
/* Early collection (EMITTED as V1): KERN_PROC_VMMAP bounded to 4 MiB.
   Identical to v1 ig_collect_vmmap (proven sysctl import only). */
static uint8_t g_vmmap_buf[IG_VMMAP_MAX] __attribute__((aligned(16)));

void ig_collect_vmmap(void) {
    int mib_pid[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid()};
    size_t len = 65536;
    int rc = -1;
    unsigned attempts = 0;
    for (;;) {
        ++attempts;
        rc = sysctl(mib_pid, 4, g_vmmap_buf, &len, 0, 0);
        if (rc == 0) break;
        int eno = ig_errno();
        if (eno == ENOMEM && len > 0 && len * 2 <= IG_VMMAP_MAX) {
            len *= 2;
            continue;
        }
        break;  /* non-ENOMEM failure or the 4 MiB growth cap */
    }
    g_facts.vmmap_rc = rc;
    g_facts.vmmap_errno = ig_errno();
    g_facts.vmmap_buf = (uint64_t)len;
    g_facts.vmmap_attempts = attempts;
    g_facts.vmmap_kve_size = (uint64_t)sizeof(struct kinfo_vmentry);
    g_facts.span_count = 0;
    g_facts.span_truncated = 0;
    g_facts.vmmap_entries = 0;
    if (rc == 0) {
        const size_t ks = sizeof(struct kinfo_vmentry);
        uint64_t entries = (uint64_t)(len / ks);
        g_facts.vmmap_entries = entries;
        for (uint64_t i = 0; i < entries; ++i) {
            const struct kinfo_vmentry *kve =
                (const struct kinfo_vmentry *)
                (const void *)(g_vmmap_buf + (size_t)(i * ks));
            if (g_facts.span_count >= IG_SPAN_MAX) {
                g_facts.span_truncated = 1;
                break;
            }
            IgSpan *s = &g_facts.spans[g_facts.span_count++];
            s->start = (uint64_t)kve->kve_start;
            s->end = (uint64_t)kve->kve_end;
            s->prot = (uint32_t)kve->kve_protection;
        }
    }
}

/* ------------------------------------------------------------------ */
/* V1: full VMMAP summary + histogram, then the COMPLETE bounded mapping
   list in chunked v1_vmmap_page records (20 entries per record, <=48
   parts => <=960 entries listed; a larger set is explicitly truncated). */
#define IG_VM_ENTRIES_PER_PART 20
#define IG_VM_PARTS_MAX 48

static void v1_vmmap_page(IgSink *sink, IgBuf *b, int *seq,
                          uint32_t part, uint32_t parts) {
    record_out(b, seq, "v1_vmmap_page", 0, 0);
    ig_kv_int(b, "part", (int64_t)part);
    ig_kv_int(b, "parts", (int64_t)parts);
    uint32_t from = part * IG_VM_ENTRIES_PER_PART;
    uint32_t to = from + IG_VM_ENTRIES_PER_PART;
    if (to > g_facts.span_count) to = g_facts.span_count;
    ig_buf_raw(b, ",\"entries\":[");
    for (uint32_t i = from; i < to; ++i) {
        if (i != from) ig_buf_ch(b, ',');
        ig_buf_ch(b, '{');
        ig_kv_hex(b, "start", g_facts.spans[i].start);
        ig_kv_hex(b, "end", g_facts.spans[i].end);
        ig_kv_int(b, "prot", (int64_t)g_facts.spans[i].prot);
        ig_buf_ch(b, '}');
        if (b->truncated) break;
    }
    ig_buf_ch(b, ']');
    emit(sink, b);
}

static void v1_vmmap_full(IgSink *sink, IgBuf *b, int *seq) {
    int64_t frc = (int64_t)g_facts.vmmap_rc;
    if (g_facts.vmmap_rc != 0 && g_facts.vmmap_errno == ENOMEM) frc = -3;
    uint32_t listable = g_facts.span_count;
    uint32_t cap = IG_VM_ENTRIES_PER_PART * IG_VM_PARTS_MAX;
    int list_trunc = 0;
    if (listable > cap) { listable = cap; list_trunc = 1; }
    uint32_t parts = (listable + IG_VM_ENTRIES_PER_PART - 1) /
                     IG_VM_ENTRIES_PER_PART;
    record_out(b, seq, "v1_vmmap_full", frc, (int64_t)g_facts.vmmap_errno);
    ig_kv_int(b, "final_buf", (int64_t)g_facts.vmmap_buf);
    ig_kv_int(b, "kve_size", (int64_t)g_facts.vmmap_kve_size);
    ig_kv_int(b, "entries", (int64_t)g_facts.vmmap_entries);
    ig_kv_int(b, "attempts", (int64_t)g_facts.vmmap_attempts);
    ig_kv_int(b, "spans_kept", (int64_t)g_facts.span_count);
    ig_kv_int(b, "spans_truncated", g_facts.span_truncated);
    ig_kv_int(b, "entries_per_part", IG_VM_ENTRIES_PER_PART);
    ig_kv_int(b, "parts", (int64_t)parts);
    ig_kv_int(b, "list_truncated", list_trunc);
    ig_kv_str(b, "list_bound",
              "complete bounded list: min(spans_kept, 20*48); histogram "
              "below covers every kept span", 96);
    /* Bit population + distinct-prot histogram (covers ALL kept spans). */
    uint64_t r = 0, w = 0, x = 0;
    for (uint32_t i = 0; i < g_facts.span_count; ++i) {
        uint32_t p = g_facts.spans[i].prot;
        if (p & 1u) ++r;
        if (p & 2u) ++w;
        if (p & 4u) ++x;
    }
    ig_kv_int(b, "spans_r", (int64_t)r);
    ig_kv_int(b, "spans_w", (int64_t)w);
    ig_kv_int(b, "spans_x", (int64_t)x);
    uint32_t dv[8]; uint32_t dc[8]; int dn = 0;
    for (uint32_t i = 0; i < g_facts.span_count && dn < 8; ++i) {
        uint32_t p = g_facts.spans[i].prot;
        int f = -1;
        for (int k = 0; k < dn; ++k) if (dv[k] == p) { f = k; break; }
        if (f < 0 && dn < 8) { dv[dn] = p; dc[dn] = 1; ++dn; }
        else if (f >= 0) ++dc[f];
    }
    ig_buf_raw(b, ",\"prot_histogram\":[");
    for (int k = 0; k < dn; ++k) {
        if (k != 0) ig_buf_ch(b, ',');
        ig_buf_ch(b, '{');
        ig_kv_hex(b, "prot", (uint64_t)dv[k]);
        ig_kv_int(b, "count", (int64_t)dc[k]);
        ig_buf_ch(b, '}');
    }
    ig_buf_ch(b, ']');
    emit(sink, b);
    for (uint32_t p = 0; p < parts; ++p) v1_vmmap_page(sink, b, seq, p, parts);
}

/* ------------------------------------------------------------------ */
/* V2: import-free image-base scan over the VMMAP span set. */
#define IG_CANDS_PER_PART 7
#define IG_CAND_PARTS_MAX 10

/* Walk a dynamic section at dyn_addr (already span-validated per entry by
   ig_copy_checked). Bounded to min(filesz/16, 256) entries. */
static int dyn_walk(uint64_t dyn_addr, uint64_t filesz, int *entries_out,
                    int *dt_null_seen, int *dt_debug_present,
                    uint64_t *dt_debug_value) {
    unsigned steps = 256;
    if (filesz != 0) {
        uint64_t s = filesz / sizeof(Elf64_Dyn);
        if (s < steps) steps = (unsigned)s;
        if (steps == 0) steps = 1;
    }
    int scanned = 0, sane = 0, nul = 0, dbg = 0;
    uint64_t dbgval = 0;
    for (unsigned i = 0; i < steps; ++i) {
        Elf64_Dyn d;
        if (ig_copy_checked(dyn_addr + (uint64_t)i * sizeof d, &d,
                            sizeof d) != sizeof d)
            break;  /* unreadable: stop the walk (containment) */
        uint64_t tag = (uint64_t)d.d_tag;
        ++scanned;
        int tag_sane = (tag <= 0x40) ||
                       (tag >= 0x60000000ULL && tag < 0x70000000ULL) ||
                       (tag >= 0x70000000ULL && tag < 0x80000000ULL);
        if (tag_sane) ++sane;
        if (tag == DT_NULL) { nul = 1; break; }
        if (tag == DT_DEBUG) { dbg = 1; dbgval = (uint64_t)d.d_un.d_ptr; }
    }
    if (entries_out) *entries_out = scanned;
    if (dt_null_seen) *dt_null_seen = nul;
    if (dt_debug_present) *dt_debug_present = dbg;
    if (dt_debug_value) *dt_debug_value = dbgval;
    return scanned > 0 && (nul || sane == scanned);
}

/* Read the phdr table at base (span-validated per entry). Fills dyn info
   and phdr-derived image extent. Returns 1 when the table fully read. */
static int phdr_table_read(uint64_t base, const Elf64_Ehdr *eh,
                           uint64_t *dyn_vaddr, uint64_t *dyn_filesz,
                           int *dyn_found, uint64_t *extent,
                           int *has_exec) {
    *dyn_found = 0; *extent = 0; *has_exec = 0;
    *dyn_vaddr = 0; *dyn_filesz = 0;
    for (unsigned p = 0; p < eh->e_phnum; ++p) {
        Elf64_Phdr ph;
        if (ig_copy_checked(base + eh->e_phoff +
                                (uint64_t)p * eh->e_phentsize,
                            &ph, sizeof ph) != sizeof ph)
            return 0;  /* any unreadable phdr => table not fully read */
        if (ph.p_type == PT_DYNAMIC) {
            *dyn_vaddr = (uint64_t)ph.p_vaddr;
            *dyn_filesz = (uint64_t)ph.p_filesz;
            *dyn_found = 1;
        } else if (ph.p_type == PT_LOAD) {
            if (ph.p_flags & PF_X) *has_exec = 1;
            uint64_t e2 = (uint64_t)ph.p_vaddr + (uint64_t)ph.p_memsz;
            if (e2 > *extent) *extent = e2;
        }
    }
    return 1;
}

static void scan_image_candidates(void) {
    g_facts.cand_count = 0;
    g_facts.cand_truncated = 0;
    g_facts.chosen_index = -1;
    g_facts.chosen_base = 0;
    g_facts.chosen_extent = 0;
    g_facts.chosen_dyn_addr = 0;
    g_facts.chosen_dyn_mode = 0;
    for (uint32_t i = 0; i < g_facts.span_count &&
                        g_facts.cand_count < IG_CAND_MAX; ++i) {
        const IgSpan *s = &g_facts.spans[i];
        int rd = (s->prot & 1u) != 0;
        int ex = (s->prot & 4u) != 0;
        const char *where = 0;
        if (rd && ex) {
            where = "rx_span";
        } else if (rd && !ex && i + 1 < g_facts.span_count) {
            /* Typical image layout: header PT_LOAD readable, next PT_LOAD
               executable and adjacent — also a candidate base. */
            const IgSpan *n = &g_facts.spans[i + 1];
            if ((n->prot & 1u) && (n->prot & 4u) && n->start == s->end)
                where = "r_adjacent_exec";
        }
        if (where == 0) continue;
        if (s->end - s->start < sizeof(Elf64_Ehdr)) continue;
        if (g_facts.cand_count >= IG_CAND_MAX) break;
        IgCand *c = &g_facts.cands[g_facts.cand_count];
        ig_memset(c, 0, sizeof *c);
        c->base = s->start;
        c->span_start = s->start;
        c->span_end = s->end;
        c->prot = s->prot;
        ig_strlcpy_bounded(c->where, sizeof c->where, where,
                           ig_strlen(where));
        Elf64_Ehdr eh;
        ig_memset(&eh, 0, sizeof eh);
        if (ig_copy_checked(c->base, &eh, sizeof eh) == sizeof eh &&
            eh.e_ident[0] == 0x7f && eh.e_ident[1] == 'E' &&
            eh.e_ident[2] == 'L' && eh.e_ident[3] == 'F') {
            c->magic_ok = 1;
            c->class_ok = (eh.e_ident[4] == ELFCLASS64);
            c->e_type = eh.e_type;
            c->phnum = eh.e_phnum;
            if (c->class_ok && eh.e_phentsize >= sizeof(Elf64_Phdr) &&
                eh.e_phnum >= 1 && eh.e_phnum <= 128) {
                c->phdr_ok = phdr_table_read(c->base, &eh, &c->dyn_vaddr,
                                             &c->dyn_filesz, &c->dyn_found,
                                             &c->extent, &c->has_exec_segment);
                if (c->phdr_ok && c->dyn_found) {
                    /* Two address hypotheses (recorded, never guessed):
                       mode 1: bias=base, dyn at base+p_vaddr (ET_DYN style,
                       the v1 G2 spelling); mode 2: p_vaddr absolute
                       (ET_EXEC style). Both validated by span reads inside
                       dyn_walk; better score wins, tie -> mode 1. */
                    int e1 = 0, n1 = 0, d1p = 0; uint64_t d1v = 0;
                    int ok1 = dyn_walk(c->base + c->dyn_vaddr, c->dyn_filesz,
                                       &e1, &n1, &d1p, &d1v);
                    int e2 = 0, n2 = 0, d2p = 0; uint64_t d2v = 0;
                    uint64_t abs_addr = c->dyn_vaddr;
                    int ok2 = 0;
                    if (abs_addr != c->base + c->dyn_vaddr)
                        ok2 = dyn_walk(abs_addr, c->dyn_filesz,
                                       &e2, &n2, &d2p, &d2v);
                    int score1 = (ok1 ? 2 : 0) + n1;
                    int score2 = (ok2 ? 2 : 0) + n2;
                    if (ok1 && score1 >= score2) {
                        c->dyn_mode = 1; c->dyn_entries = e1;
                        c->dt_null_seen = n1; c->dyn_parses = 1;
                    } else if (ok2) {
                        c->dyn_mode = 2; c->dyn_entries = e2;
                        c->dt_null_seen = n2; c->dyn_parses = 1;
                    }
                }
            }
        }
        ++g_facts.cand_count;
    }
    if (g_facts.span_count > 0 && g_facts.cand_count == IG_CAND_MAX) {
        /* Only a bound flag: further would-be candidates were not probed. */
        g_facts.cand_truncated = 1;
    }
    /* Main eboot candidate: LARGEST executable image whose dynamic parses.
       Ties resolve to the lowest base (deterministic). */
    int best = -1;
    for (uint32_t i = 0; i < g_facts.cand_count; ++i) {
        IgCand *c = &g_facts.cands[i];
        if (!(c->magic_ok && c->class_ok && c->phdr_ok && c->dyn_parses &&
              c->has_exec_segment && c->extent != 0))
            continue;
        if (best < 0 || c->extent > g_facts.cands[best].extent) best = (int)i;
    }
    if (best >= 0) {
        IgCand *c = &g_facts.cands[best];
        c->chosen = 1;
        g_facts.chosen_index = best;
        g_facts.chosen_base = c->base;
        g_facts.chosen_extent = c->extent;
        g_facts.chosen_dyn_mode = c->dyn_mode;
        g_facts.chosen_dyn_addr = (c->dyn_mode == 2)
            ? c->dyn_vaddr
            : c->base + c->dyn_vaddr;
    }
}

static void v2_scan_page(IgSink *sink, IgBuf *b, int *seq,
                         uint32_t part, uint32_t parts) {
    record_out(b, seq, "v2_scan_page", 0, 0);
    ig_kv_int(b, "part", (int64_t)part);
    ig_kv_int(b, "parts", (int64_t)parts);
    uint32_t from = part * IG_CANDS_PER_PART;
    uint32_t to = from + IG_CANDS_PER_PART;
    if (to > g_facts.cand_count) to = g_facts.cand_count;
    ig_buf_raw(b, ",\"candidates\":[");
    for (uint32_t i = from; i < to; ++i) {
        const IgCand *c = &g_facts.cands[i];
        if (i != from) ig_buf_ch(b, ',');
        ig_buf_ch(b, '{');
        ig_kv_hex(b, "base", c->base);
        ig_kv_str(b, "where", c->where, 16);
        ig_kv_int(b, "prot", (int64_t)c->prot);
        ig_kv_hex(b, "span_start", c->span_start);
        ig_kv_hex(b, "span_end", c->span_end);
        ig_kv_int(b, "magic", c->magic_ok);
        ig_kv_int(b, "cls", c->class_ok);
        ig_kv_int(b, "etype", (int64_t)c->e_type);
        ig_kv_int(b, "phnum", (int64_t)c->phnum);
        ig_kv_int(b, "phdr_ok", c->phdr_ok);
        ig_kv_int(b, "exec", c->has_exec_segment);
        ig_kv_hex(b, "extent", c->extent);
        ig_kv_int(b, "dyn_found", c->dyn_found);
        ig_kv_int(b, "dyn_mode", c->dyn_mode);
        ig_kv_int(b, "dynok", c->dyn_parses);
        ig_kv_int(b, "dynn", (int64_t)c->dyn_entries);
        ig_kv_int(b, "dtnull", c->dt_null_seen);
        ig_kv_int(b, "chosen", c->chosen);
        ig_buf_ch(b, '}');
        if (b->truncated) break;
    }
    ig_buf_ch(b, ']');
    emit(sink, b);
}

static void v2_elf_base_scan(IgSink *sink, IgBuf *b, int *seq) {
    scan_image_candidates();
    uint32_t listable = g_facts.cand_count;
    uint32_t cap = IG_CANDS_PER_PART * IG_CAND_PARTS_MAX;
    if (listable > cap) listable = cap;  /* cand list bound (IG_CAND_MAX=64) */
    uint32_t parts = (listable + IG_CANDS_PER_PART - 1) / IG_CANDS_PER_PART;
    record_out(b, seq, "v2_elf_base_scan",
               g_facts.span_count > 0 ? 0 : -1, 0);
    ig_kv_int(b, "spans_scanned", (int64_t)g_facts.span_count);
    ig_kv_int(b, "candidates", (int64_t)g_facts.cand_count);
    ig_kv_int(b, "cands_truncated", g_facts.cand_truncated);
    ig_kv_int(b, "parts", (int64_t)parts);
    ig_kv_str(b, "method",
              "RX span starts + readable spans adjacent to exec spans; "
              "ELF magic + phdrs read in-memory, span-validated", 96);
    ig_kv_int(b, "chosen_index", (int64_t)g_facts.chosen_index);
    ig_kv_hex(b, "chosen_base", g_facts.chosen_base);
    ig_kv_hex(b, "chosen_extent", g_facts.chosen_extent);
    if (g_facts.chosen_index >= 0) {
        ig_kv_int(b, "chosen_etype",
                  (int64_t)g_facts.cands[g_facts.chosen_index].e_type);
        ig_kv_int(b, "chosen_dyn_mode", g_facts.chosen_dyn_mode);
        ig_kv_hex(b, "chosen_dyn_addr", g_facts.chosen_dyn_addr);
        ig_kv_str(b, "provenance", "v2_elf_base_scan", 24);
        ig_kv_str(b, "rule",
                  "largest executable image whose dynamic section parses",
                  64);
    } else {
        ig_kv_str(b, "provenance", "needs-base", 16);
        ig_kv_str(b, "note",
                  "no candidate image parsed a dynamic section; V3/V4 skip",
                  64);
    }
    emit(sink, b);
    for (uint32_t p = 0; p < parts; ++p) v2_scan_page(sink, b, seq, p, parts);
}

/* ------------------------------------------------------------------ */
/* V3: DT_DEBUG at the chosen base's _DYNAMIC + bounded link-map walk
   (v1 G2 logic adapted to the scanned base; NO imports). */
static int contains_sub(const char *hay, const char *needle) {
    size_t hl = ig_strlen(hay), nl = ig_strlen(needle);
    if (nl == 0 || hl < nl) return 0;
    for (size_t i = 0; i + nl <= hl; ++i) {
        size_t j = 0;
        while (j < nl && hay[i + j] == needle[j]) ++j;
        if (j == nl) return 1;
    }
    return 0;
}

static size_t copy_name_checked(uint64_t addr, char *dst, size_t cap) {
    /* Try decreasing lengths; returns bytes copied (0 = unreadable). */
    static const size_t tries[] = {96, 48, 16, 8, 4, 2, 1};
    for (unsigned t = 0; t < sizeof tries / sizeof tries[0]; ++t) {
        size_t want = tries[t];
        if (want > cap - 1) continue;
        if (ig_range_readable(addr, want)) {
            ig_memcpy(dst, (const void *)(uintptr_t)addr, want);
            dst[want] = '\0';
            size_t n = ig_strlen(dst);
            if (n < want) return n;
            return want;
        }
    }
    dst[0] = '\0';
    return 0;
}

static void v3_dtdebug(IgSink *sink, IgBuf *b, int *seq) {
    int64_t rc = 0;
    uint64_t dt_val = 0;
    int dt_present = 0;
    struct link_map *first = 0;
    uint64_t base = g_facts.chosen_base;
    uint64_t dyn_addr = g_facts.chosen_dyn_addr;
    if (g_facts.chosen_index < 0) {
        rc = -1;  /* needs-base: V2 produced no parsed candidate */
    } else {
        int entries = 0, nul = 0;
        int ok = dyn_walk(dyn_addr, 0 /*steps=cap*/, &entries, &nul,
                          &dt_present, &dt_val);
        if (!ok)
            rc = -5;  /* dynamic unreadable/no sane entries at chosen base */
        else if (!dt_present)
            rc = -6;
        else if (dt_val == 0)
            rc = -7;
    }
    int emitted = 0;
    int truncated = 0;
    int blocked = 0;
    uint64_t visited[IG_LINKMAP_MAX];
    static uint64_t rec_addr[16];
    static char rec_name[16][48];
    int rec_n = 0;
    if (rc == 0) {
        struct r_debug rd;
        if (ig_copy_checked(dt_val, &rd, sizeof rd) != sizeof rd ||
            rd.r_map == 0) {
            rc = -8;
        } else {
            first = rd.r_map;
            struct link_map *it = first;
            while (it != 0) {
                if (emitted >= IG_LINKMAP_MAX) { truncated = 1; break; }
                int seen = 0;
                for (int i = 0; i < emitted; ++i)
                    if (visited[i] == (uint64_t)(uintptr_t)it) { seen = 1; break; }
                if (seen) break;  /* cycle contained */
                struct link_map lm;
                if (ig_copy_checked((uint64_t)(uintptr_t)it, &lm, sizeof lm) !=
                    sizeof lm) {
                    ++blocked;
                    break;  /* node unreadable: stop the walk, record it */
                }
                visited[emitted] = (uint64_t)(uintptr_t)it;
                char nm[96];
                size_t got = 0;
                if (lm.l_name != 0)
                    got = copy_name_checked((uint64_t)(uintptr_t)lm.l_name,
                                            nm, sizeof nm);
                else { nm[0] = '\0'; got = 0; }
                if (lm.l_name != 0 && got == 0) ++blocked;
                if (contains_sub(nm, "eboot") &&
                    !g_facts.v3_eboot_linkmap_laddr) {
                    g_facts.v3_eboot_linkmap_laddr = 1;
                    g_facts.v3_eboot_laddr = (uint64_t)(uintptr_t)lm.l_addr;
                    ig_strlcpy_bounded(g_facts.v3_eboot_lname,
                                       sizeof g_facts.v3_eboot_lname, nm,
                                       sizeof nm - 1);
                }
                ++emitted;
                if (rec_n < 16) {
                    rec_addr[rec_n] = (uint64_t)(uintptr_t)lm.l_addr;
                    ig_strlcpy_bounded(rec_name[rec_n], 48, nm, 47);
                    ++rec_n;
                }
                it = lm.l_next;
            }
            g_facts.v3_entries = emitted;
            g_facts.v3_truncated = truncated;
            g_facts.v3_blocked = blocked;
        }
    }
    g_facts.v3_dt_debug_present = dt_present;
    g_facts.v3_dt_debug_value = dt_val;
    g_facts.v3_r_map = (uint64_t)(uintptr_t)first;

    record_out(b, seq, "v3_dtdebug", rc, 0);
    ig_kv_hex(b, "base_used", base);
    ig_kv_hex(b, "dyn_addr", dyn_addr);
    ig_kv_int(b, "dyn_mode", (int64_t)g_facts.chosen_dyn_mode);
    ig_kv_int(b, "dt_debug_present", dt_present);
    ig_kv_hex(b, "dt_debug_value", dt_val);
    ig_kv_hex(b, "r_map", (uint64_t)(uintptr_t)first);
    ig_kv_int(b, "entries", g_facts.v3_entries);
    ig_kv_int(b, "entries_truncated", g_facts.v3_truncated);
    ig_kv_int(b, "validation_blocked_derefs", g_facts.v3_blocked);
    ig_buf_raw(b, ",\"entries\":[");
    for (int i = 0; i < rec_n; ++i) {
        if (i != 0) ig_buf_ch(b, ',');
        ig_buf_ch(b, '{');
        ig_kv_hex(b, "l_addr", rec_addr[i]);
        ig_kv_str(b, "l_name", rec_name[i], 32);
        ig_buf_ch(b, '}');
        if (b->truncated) break;
    }
    ig_buf_ch(b, ']');
    if (g_facts.v3_eboot_linkmap_laddr) {
        ig_kv_hex(b, "eboot_linkmap_l_addr", g_facts.v3_eboot_laddr);
        ig_kv_str(b, "eboot_linkmap_l_name", g_facts.v3_eboot_lname, 80);
    }
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* V4: layout predicates at the V2-scanned base (vendored worker-5
   predicates; needs-base skip ONLY if no candidate base parsed). */
static void v4_layout_predicates(IgSink *sink, IgBuf *b, int *seq) {
    if (g_facts.chosen_index < 0) {
        record_out(b, seq, "v4_layout_predicates", -1, 0);
        ig_kv_str(b, "status", "needs-base", 16);
        ig_kv_str(b, "note",
                  "V2 found no parsed candidate base; predicates skipped, "
                  "never guessed", 80);
        emit(sink, b);
        return;
    }
    uint64_t base = g_facts.chosen_base;
    const char *prov = "v2_elf_base_scan";

    IgGuaOutC gua;
    ig_memset(&gua, 0, sizeof gua);
    ig_layout_guobject(g_facts.spans, g_facts.span_count, base,
                       IG_VADDR_GUOBJECT_ARRAY, &gua);
    IgCodeOutC tick, sco;
    ig_memset(&tick, 0, sizeof tick);
    ig_memset(&sco, 0, sizeof sco);
    ig_layout_code_shape(g_facts.spans, g_facts.span_count, base,
                         IG_VADDR_UGAMEENGINE_TICK, &tick);
    ig_layout_code_shape(g_facts.spans, g_facts.span_count, base,
                         IG_VADDR_STATIC_CONSTRUCT_OBJECT, &sco);

    record_out(b, seq, "v4_layout_predicates", 0, 0);
    ig_kv_hex(b, "base", base);
    ig_kv_str(b, "base_provenance", prov, 24);
    ig_kv_int(b, "base_cand_index", (int64_t)g_facts.chosen_index);
    ig_kv_hex(b, "base_extent", g_facts.chosen_extent);
    ig_kv_hex(b, "guobject_vaddr", (uint64_t)IG_VADDR_GUOBJECT_ARRAY);
    ig_kv_str(b, "gua_verdict", ig_verdict_string(gua.verdict), 16);
    ig_kv_str(b, "gua_layout_matched",
              gua.layout_matched == 1 ? "FUObjectArray_outer"
              : gua.layout_matched == 2 ? "TUObjectArray_direct" : "none",
              24);
    ig_kv_str(b, "gua_a_verdict", ig_verdict_string(gua.layout_a.verdict), 16);
    ig_kv_str(b, "gua_b_verdict", ig_verdict_string(gua.layout_b.verdict), 16);
    if (gua.code_count > 0)
        ig_kv_str(b, "gua_code0", ig_rcode_string(gua.codes[0]), 40);
    ig_kv_int(b, "gua_code_count", gua.code_count);
    ig_kv_int(b, "gua_num_elements", (int64_t)gua.num_elements);
    ig_kv_int(b, "gua_max_elements", (int64_t)gua.max_elements);
    ig_kv_int(b, "gua_num_chunks", (int64_t)gua.num_chunks);
    ig_kv_int(b, "gua_max_chunks", (int64_t)gua.max_chunks);
    ig_kv_hex(b, "gua_objects_ptr", gua.objects_ptr);
    ig_kv_int(b, "gua_chunks_scanned", (int64_t)gua.chunks_scanned);
    ig_kv_str(b, "tick_verdict", ig_verdict_string(tick.verdict), 16);
    if (tick.code_count > 0)
        ig_kv_str(b, "tick_code0", ig_rcode_string(tick.codes[0]), 40);
    ig_kv_int(b, "tick_bytes_read", (int64_t)tick.bytes_read);
    ig_kv_str(b, "sco_verdict", ig_verdict_string(sco.verdict), 16);
    if (sco.code_count > 0)
        ig_kv_str(b, "sco_code0", ig_rcode_string(sco.codes[0]), 40);
    ig_kv_int(b, "sco_bytes_read", (int64_t)sco.bytes_read);
    ig_buf_raw(b, ",\"tick_first_bytes\":\"");
    for (int i = 0; i < 16 && i < tick.bytes_read; ++i) {
        static const char hd[] = "0123456789abcdef";
        char hx[3];
        hx[0] = hd[tick.first_bytes[i] >> 4];
        hx[1] = hd[tick.first_bytes[i] & 15];
        hx[2] = 0;
        ig_buf_raw(b, hx);
    }
    ig_buf_ch(b, '"');
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* V5: firmware signal + summary. kernel_get_fw_version is crt1-provided and
   a PRX links no crt1 (v1 G7 note stands), so sysctl KERN_VERSION is
   attempted; whatever returns is recorded verbatim. */
static void v5_fw_summary(IgSink *sink, IgBuf *b, int *seq) {
    char vers[96];
    ig_memset(vers, 0, sizeof vers);
    size_t vlen = sizeof vers - 1;
#ifdef KERN_VERSION
    int mib[2] = {CTL_KERN, KERN_VERSION};
    int rc = sysctl(mib, 2, vers, &vlen, 0, 0);
#else
    int rc = -100;  /* KERN_VERSION not in the pinned headers */
#endif
    int eno = ig_errno();
    record_out(b, seq, "v5_fw_summary", (int64_t)rc, (int64_t)eno);
    ig_kv_str(b, "kern_version", vers, 88);
    ig_kv_str(b, "fw_note",
#ifdef KERN_VERSION
              "sysctl KERN_VERSION recorded; crt1 kernel_get_fw_version is "
              "not linkable in a PRX (no crt1) and is NOT claimed",
#else
              "KERN_VERSION absent from pinned headers; crt1 route absent",
#endif
              120);
    ig_kv_int(b, "records_written", (int64_t)(*seq));
    ig_kv_int(b, "write_failures", (int64_t)sink->failed);
    ig_kv_int(b, "ctor_events", (int64_t)g_ctor_events);
    ig_kv_int(b, "span_count", (int64_t)g_facts.span_count);
    ig_kv_int(b, "cand_count", (int64_t)g_facts.cand_count);
    if (g_facts.chosen_index >= 0)
        ig_kv_hex(b, "chosen_base", g_facts.chosen_base);
    ig_kv_str(b, "import_profile",
              "7 verbatim console-proven identities only", 48);
    ig_kv_str(b, "dropped_identities",
              "g1/g3/g4 removed: sceKernelGetModuleList, "
              "sceKernelGetModuleInfo, sceKernelGetExecutableModuleHandle, "
              "dlopen, dlclose (+ weak dl rows)", 160);
    ig_kv_str(b, "quiesce", "inert_no_threads_module_stays_loaded", 40);
    ig_kv_str(b, "unload_attempted", "no", 4);
    ig_kv_str(b, "log_path", IG_LOG_PATH, 64);
    emit(sink, b);
}

void ig_run_all_experiments(IgSink *sink) {
    IgBuf b;
    int seq = 2;  /* seq 1 (entry) and 2 (startup) written by module_entry.c */
    v1_vmmap_full(sink, &b, &seq);
    v2_elf_base_scan(sink, &b, &seq);
    v3_dtdebug(sink, &b, &seq);
    v4_layout_predicates(sink, &b, &seq);
    v5_fw_summary(sink, &b, &seq);
}
