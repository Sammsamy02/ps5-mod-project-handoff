// experiments.c
//
// G1..G7 loader-fact experiments for the MODBRIDGE IN-GAME FACTS PRX
// (worker-ingame-probe, team 2026-10-04). Runs synchronously inside
// module_start at ploader late-load time, BEFORE any UE4SS initialization,
// in the RUNNING Townfall game process.
//
// Experiment order = record order (G1..G7). Fault containment: the VMMAP
// span snapshot is COLLECTED FIRST (ig_collect_vmmap) and every foreign-
// pointer dereference in G2/G6 is validated against readable spans BEFORE
// the dereference happens; a failed validation is recorded, never attempted.
//
// Every experiment emits EXACTLY ONE bounded JSONL record and reports what
// the OS ACTUALLY returned; failures are explicit rc/errno fields, never
// fabricated successes.
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

IgFacts g_facts;
unsigned g_ctor_events;
unsigned g_fini_events;

/* ------------------------------------------------------------------ */
/* Readability validation against the early VMMAP snapshot (G5 data). */
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

/* Common tail: flush + bounded write; write failures counted for G7. */
static void emit(IgSink *sink, IgBuf *b) {
    ig_record_flush(b);
    if (ig_sink_write_record(sink, b) != 0) sink->failed = 1;
}

/* ------------------------------------------------------------------ */
/* Early collection (EMITTED as G5): KERN_PROC_VMMAP bounded to 4 MiB. */
static uint8_t g_vmmap_buf[IG_VMMAP_MAX] __attribute__((aligned(16)));

void ig_collect_vmmap(void) {
    static const int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, 0};
    int mib_pid[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid()};
    (void)mib;
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
/* G1: exec-handle + 352-byte module info (the GAME eboot case). */
static void g1_exec_handle(IgSink *sink, IgBuf *b, int *seq) {
    int eno = 0;
    int64_t raw = 0;
    int plausible = 0;
    eno = ig_errno();  /* clear observation point */
    raw = sceKernelGetExecutableModuleHandle();
    eno = ig_errno();
    plausible = (raw > 0 && raw <= 0xffffffffLL) ? 1 : 0;
    g_facts.exec.attempted = 1;
    g_facts.exec.raw = raw;
    g_facts.exec.plausible = plausible;

    if (plausible) {
        IgModuleInfo info;
        ig_memset(&info, 0, sizeof info);
        info.size = 352;
        eno = ig_errno();
        int mr = sceKernelGetModuleInfo((IgModuleHandle)raw, &info);
        int meno = ig_errno();
        g_facts.exec.info_rc = mr;
        g_facts.exec.info_errno = meno;
        g_facts.exec.size_echo_ok = (info.size == 352) ? 1 : 0;
        g_facts.exec.segment_count = info.segmentCount;
        ig_strlcpy_bounded(g_facts.exec.name, sizeof g_facts.exec.name,
                           info.name, sizeof info.name - 1);
        if (mr == 0 && info.size == 352 && info.segmentCount > 0) {
            g_facts.exec.seg0_addr = (uint64_t)(uintptr_t)
                info.segmentInfo[0].address;
            g_facts.exec.seg0_size = info.segmentInfo[0].size;
            g_facts.exec.seg0_prot = info.segmentInfo[0].prot;
        }
        record_out(b, seq, "g1_exec_handle", plausible ? 0 : -1, eno);
        ig_kv_int(b, "raw", raw);
        ig_kv_hex(b, "raw_hex", (uint64_t)raw);
        ig_kv_int(b, "plausible_handle", plausible);
        ig_kv_int(b, "info_rc", mr);
        ig_kv_int(b, "info_errno", meno);
        ig_kv_int(b, "size_echo", (int64_t)info.size);
        ig_kv_int(b, "size_echo_ok", g_facts.exec.size_echo_ok);
        ig_kv_int(b, "segment_count", (int64_t)info.segmentCount);
        ig_kv_str(b, "name", g_facts.exec.name, 88);
        ig_kv_hex(b, "seg0_addr", g_facts.exec.seg0_addr);
        ig_kv_hex(b, "seg0_size", (uint64_t)g_facts.exec.seg0_size);
        ig_kv_hex(b, "seg0_prot", (uint64_t)(uint32_t)g_facts.exec.seg0_prot);
        ig_buf_raw(b, ",\"fingerprint_hex\":\"");
        for (int i = 0; i < 8; ++i) {
            char hx[3];
            static const char hd[] = "0123456789abcdef";
            hx[0] = hd[info.fingerprint[i] >> 4];
            hx[1] = hd[info.fingerprint[i] & 15];
            hx[2] = 0;
            ig_buf_raw(b, hx);
        }
        ig_buf_ch(b, '"');
    } else {
        record_out(b, seq, "g1_exec_handle", -1, eno);
        ig_kv_int(b, "raw", raw);
        ig_kv_hex(b, "raw_hex", (uint64_t)raw);
        ig_kv_int(b, "plausible_handle", 0);
        ig_kv_str(b, "note",
                  "raw not a plausible handle; eboot case unresolved in-game",
                  64);
    }
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* G2: GAME eboot _DYNAMIC -> DT_DEBUG -> link-map walk (bounded 64). */
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
            /* Re-bound at the actual NUL if present. */
            size_t n = ig_strlen(dst);
            if (n < want) return n;
            return want;
        }
    }
    dst[0] = '\0';
    return 0;
}

static void g2_dtdebug(IgSink *sink, IgBuf *b, int *seq) {
    uint64_t base = 0;
    int have_base = 0;
    if (g_facts.exec.plausible && g_facts.exec.info_rc == 0 &&
        g_facts.exec.size_echo_ok && g_facts.exec.seg0_addr != 0 &&
        ig_range_readable(g_facts.exec.seg0_addr, 64)) {
        base = g_facts.exec.seg0_addr;
        have_base = 1;
    }
    int64_t rc = 0;
    uint64_t dt_val = 0;
    int dt_present = 0;
    struct link_map *first = 0;
    if (!have_base) {
        rc = -1;  /* needs-base: G1 gave no validated eboot base */
    } else {
        Elf64_Ehdr eh;
        ig_memset(&eh, 0, sizeof eh);
        if (ig_copy_checked(base, &eh, sizeof eh) != sizeof eh ||
            eh.e_ident[0] != 0x7f || eh.e_ident[1] != 'E' ||
            eh.e_ident[2] != 'L' || eh.e_ident[3] != 'F') {
            rc = -2;  /* eboot ELF header unreadable/not ELF */
        } else if (eh.e_phnum == 0 || eh.e_phentsize < 56 ||
                   eh.e_phnum > 128) {
            rc = -3;  /* phdr table implausible */
        } else {
            uint64_t dyn_vaddr = 0, dyn_filesz = 0;
            int found_dyn = 0;
            for (unsigned p = 0; p < eh.e_phnum; ++p) {
                Elf64_Phdr ph;
                if (ig_copy_checked(base + eh.e_phoff +
                                        (uint64_t)p * eh.e_phentsize,
                                    &ph, sizeof ph) != sizeof ph) {
                    break;
                }
                if (ph.p_type == PT_DYNAMIC) {
                    dyn_vaddr = (uint64_t)ph.p_vaddr;
                    dyn_filesz = (uint64_t)ph.p_filesz;
                    found_dyn = 1;
                    break;
                }
            }
            if (!found_dyn) {
                rc = -4;  /* no PT_DYNAMIC in the eboot image */
            } else {
                uint64_t dyn = base + dyn_vaddr;
                unsigned steps = (unsigned)(dyn_filesz / sizeof(Elf64_Dyn));
                if (steps > 256) steps = 256;
                for (unsigned i = 0; i < steps; ++i) {
                    Elf64_Dyn d;
                    if (ig_copy_checked(dyn + (uint64_t)i * sizeof d, &d,
                                        sizeof d) != sizeof d)
                        break;
                    if (d.d_tag == DT_NULL) break;
                    if (d.d_tag == DT_DEBUG) {
                        dt_present = 1;
                        dt_val = (uint64_t)d.d_un.d_ptr;
                        break;
                    }
                }
                if (!dt_present) rc = -6;
                else if (dt_val == 0) rc = -7;
            }
        }
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
                if (contains_sub(nm, "eboot") && !g_facts.g2_eboot_linkmap_laddr) {
                    g_facts.g2_eboot_linkmap_laddr = 1;
                    g_facts.g2_eboot_laddr = (uint64_t)(uintptr_t)lm.l_addr;
                    ig_strlcpy_bounded(g_facts.g2_eboot_lname,
                                       sizeof g_facts.g2_eboot_lname, nm,
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
            g_facts.g2_entries = emitted;
            g_facts.g2_truncated = truncated;
            g_facts.g2_blocked = blocked;
        }
    }
    g_facts.g2_dt_debug_present = dt_present;
    g_facts.g2_dt_debug_value = dt_val;
    g_facts.g2_r_map = (uint64_t)(uintptr_t)first;

    record_out(b, seq, "g2_dtdebug", rc, 0);
    ig_kv_int(b, "dt_debug_present", dt_present);
    ig_kv_hex(b, "dt_debug_value", dt_val);
    ig_kv_hex(b, "r_map", (uint64_t)(uintptr_t)first);
    ig_kv_hex(b, "eboot_base_used", base);
    ig_kv_int(b, "entries", g_facts.g2_entries);
    ig_kv_int(b, "entries_truncated", g_facts.g2_truncated);
    ig_kv_int(b, "validation_blocked_derefs", g_facts.g2_blocked);
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
    if (g_facts.g2_eboot_linkmap_laddr) {
        ig_kv_hex(b, "eboot_linkmap_l_addr", g_facts.g2_eboot_laddr);
        ig_kv_str(b, "eboot_linkmap_l_name", g_facts.g2_eboot_lname, 80);
    }
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* G3: dlopen(eboot path, RTLD_NOLOAD|RTLD_NOW) — weak dl routes only. */
static void g3_noload_eboot(IgSink *sink, IgBuf *b, int *seq) {
    int64_t rc = 0;
    int64_t eno = 0;
    const char *route = "none";
    void *h = 0;
    int64_t dlc = -1;
    const char *src = "none";
    if (g_facts.eboot_path_ok) {
        src = contains_sub(g_facts.g2_eboot_lname, "eboot") ? "g2_linkmap"
            : "g1_module_info";
    } else {
        rc = -9;  /* needs-path: neither G1 nor G2 yielded a path */
    }
    if (rc == 0) {
        int mode = (int)(IG_RTLD_NOLOAD | IG_RTLD_NOW);  /* observed 0x2002 */
        void *(*volatile dlp)(const char *, int) = dlopen;
        void *(*volatile dlu)(const char *, int) = __dlopen;
        int (*volatile dcp)(void *) = dlclose;
        int (*volatile dcu)(void *) = __dlclose;
        if (dlp != 0) { route = "weak_plain"; h = dlp(g_facts.eboot_path, mode); eno = ig_errno(); }
        else if (dlu != 0) { route = "weak_under"; h = dlu(g_facts.eboot_path, mode); eno = ig_errno(); }
        else rc = -8;  /* dl unavailable: weak imports unresolved */
        if (h != 0) {
            if (dcp != 0) dlc = (int64_t)dcp(h);
            else if (dcu != 0) dlc = (int64_t)dcu(h);
        }
    }
    record_out(b, seq, "g3_noload_eboot", rc, eno);
    ig_kv_int(b, "mode", (int64_t)(IG_RTLD_NOLOAD | IG_RTLD_NOW));
    ig_kv_str(b, "path_source", src, 16);
    ig_kv_str(b, "path", g_facts.eboot_path, 128);
    ig_kv_str(b, "dl_route", route, 16);
    ig_kv_int(b, "handle_nonnull", h != 0 ? 1 : 0);
    ig_kv_hex(b, "handle", (uint64_t)(uintptr_t)h);
    ig_kv_int(b, "dlclose_rc", dlc);
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* G4: 3-arg module list, BOTH size units, capacity 128; info on 8. */
static int validate_handle_info(uint32_t h, IgModuleInfo *info) {
    ig_memset(info, 0, sizeof *info);
    info->size = 352;
    return sceKernelGetModuleInfo((IgModuleHandle)h, info);
}

static void g4_module_list(IgSink *sink, IgBuf *b, int *seq) {
    static IgModuleHandle region_a[IG_MODULELIST_CAP];  /* size=4N call */
    static IgModuleHandle region_b[IG_MODULELIST_CAP];  /* size=N call */
    size_t avail = 0;
    ig_memset(region_a, 0, sizeof region_a);
    ig_memset(region_b, 0, sizeof region_b);
    int ra = sceKernelGetModuleList(region_a, 4 * IG_MODULELIST_CAP, &avail);
    int ea = ig_errno();
    uint64_t avail_a = (uint64_t)avail;
    avail = 0;
    int rb = sceKernelGetModuleList(region_b, IG_MODULELIST_CAP, &avail);
    int eb = ig_errno();
    uint64_t avail_b = (uint64_t)avail;
    uint32_t count_a = 0, count_b = 0;
    for (int i = 0; i < IG_MODULELIST_CAP; ++i) {
        uint32_t v = (uint32_t)region_a[i];
        if (v != 0 && v <= 0xffffffffu) {
            if (count_a < IG_MODULELIST_CAP) {
                g_facts.g4_handles_4n[count_a] = v;
                ++count_a;
            }
        }
    }
    for (int i = 0; i < IG_MODULELIST_CAP; ++i) {
        uint32_t v = (uint32_t)region_b[i];
        if (v != 0) {
            if (count_b < IG_MODULELIST_CAP) {
                g_facts.g4_handles_n[count_b] = v;
                ++count_b;
            }
        }
    }
    g_facts.g4_rc_4n = ra; g_facts.g4_eno_4n = ea; g_facts.g4_avail_4n = avail_a;
    g_facts.g4_rc_n = rb;  g_facts.g4_eno_n = eb;  g_facts.g4_avail_n = avail_b;
    g_facts.g4_count_4n = count_a; g_facts.g4_count_n = count_b;

    /* First plausible candidate of each variant validated by 352B echo. */
    int64_t va_rc = 0x7fffffff, va_se = 0, va_eno = 0;
    int64_t vb_rc = 0x7fffffff, vb_se = 0, vb_eno = 0;
    IgModuleInfo info;
    if (count_a > 0) {
        va_rc = validate_handle_info(g_facts.g4_handles_4n[0], &info);
        va_se = (int64_t)info.size; va_eno = ig_errno();
    }
    if (count_b > 0) {
        vb_rc = validate_handle_info(g_facts.g4_handles_n[0], &info);
        vb_se = (int64_t)info.size; vb_eno = ig_errno();
    }
    /* Unit discrimination in the multi-module game process. */
    const char *unit = "ambiguous";
    int a_ok = (ra == 0 && count_a > 1);
    int b_ok = (rb == 0 && count_b > 1);
    if (a_ok && !b_ok) unit = "bytes_4N";
    else if (b_ok && !a_ok) unit = "elements_N";
    else if (a_ok && b_ok) unit = "both_filled";
    else if (ra == 0 && rb == 0) unit = "single_module_undiscriminated";
    else if (ra == 0) unit = "bytes_4n_only_rc0";
    else if (rb == 0) unit = "elements_n_only_rc0";

    /* Prefer the variant with more plausible handles for detail + scan. */
    const uint32_t *handles = (count_b > count_a) ? g_facts.g4_handles_n
                                                  : g_facts.g4_handles_4n;
    uint32_t handle_count = (count_b > count_a) ? count_b : count_a;
    const char *detail_variant = (count_b > count_a) ? "elements_N" : "bytes_4N";

    /* Bounded scan (<=128) for the eboot entry; detail on the first 8. */
    g_facts.g4_eboot_index = -1;
    static char first8_names[8][40];
    static uint64_t first8_seg0[8];
    static uint32_t first8_segc[8];
    static int first8_rc[8];
    for (int i = 0; i < 8; ++i) {
        first8_names[i][0] = '\0';
        first8_seg0[i] = 0;
        first8_segc[i] = 0;
        first8_rc[i] = 0x7fffffff;
    }
    uint32_t scan = handle_count > 128 ? 128 : handle_count;
    for (uint32_t i = 0; i < scan; ++i) {
        IgModuleInfo mi;
        int mr = validate_handle_info(handles[i], &mi);
        if (mr != 0 || mi.size != 352) continue;
        char nm[64];
        ig_strlcpy_bounded(nm, sizeof nm, mi.name, sizeof mi.name - 1);
        if (i < 8) {
            ig_strlcpy_bounded(first8_names[i], 40, nm, 39);
            first8_seg0[i] = (uint64_t)(uintptr_t)mi.segmentInfo[0].address;
            first8_segc[i] = mi.segmentCount;
            first8_rc[i] = mr;
        }
        if (g_facts.g4_eboot_index < 0 && contains_sub(nm, "eboot")) {
            g_facts.g4_eboot_index = (int)i;
            g_facts.g4_eboot_seg0 =
                (uint64_t)(uintptr_t)mi.segmentInfo[0].address;
            ig_strlcpy_bounded(g_facts.g4_eboot_name,
                               sizeof g_facts.g4_eboot_name, nm,
                               sizeof nm - 1);
        }
    }

    record_out(b, seq, "g4_module_list", (int64_t)(ra == 0 ? 0 : ra), ea);
    ig_kv_int(b, "capacity", IG_MODULELIST_CAP);
    ig_kv_int(b, "api", 3);  /* 3-arg prosperodev spelling (resolved ABI) */
    ig_kv_int(b, "rc_4n", ra);
    ig_kv_int(b, "eno_4n", ea);
    ig_kv_int(b, "avail_4n", (int64_t)avail_a);
    ig_kv_int(b, "count_4n", (int64_t)count_a);
    ig_kv_int(b, "rc_n", rb);
    ig_kv_int(b, "eno_n", eb);
    ig_kv_int(b, "avail_n", (int64_t)avail_b);
    ig_kv_int(b, "count_n", (int64_t)count_b);
    ig_kv_str(b, "discriminated_unit", unit, 40);
    ig_kv_str(b, "detail_variant", detail_variant, 16);
    ig_kv_int(b, "v4n_rc", va_rc);
    ig_kv_int(b, "v4n_size_echo", va_se);
    ig_kv_int(b, "v4n_eno", va_eno);
    ig_kv_int(b, "vn_rc", vb_rc);
    ig_kv_int(b, "vn_size_echo", vb_se);
    ig_kv_int(b, "vn_eno", vb_eno);
    ig_kv_int(b, "eboot_index", (int64_t)g_facts.g4_eboot_index);
    if (g_facts.g4_eboot_index >= 0) {
        ig_kv_hex(b, "eboot_seg0", g_facts.g4_eboot_seg0);
        ig_kv_str(b, "eboot_name", g_facts.g4_eboot_name, 80);
    }
    ig_buf_raw(b, ",\"first8\":[");
    int shown = handle_count < 8 ? (int)handle_count : 8;
    for (int i = 0; i < shown; ++i) {
        if (i != 0) ig_buf_ch(b, ',');
        ig_buf_ch(b, '{');
        ig_kv_hex(b, "h", (uint64_t)handles[i]);
        ig_kv_str(b, "name", first8_names[i], 32);
        ig_kv_int(b, "segc", (int64_t)first8_segc[i]);
        ig_kv_hex(b, "seg0", first8_seg0[i]);
        ig_kv_int(b, "rc", first8_rc[i]);
        ig_buf_ch(b, '}');
        if (b->truncated) break;
    }
    ig_buf_ch(b, ']');
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* G5: emit the early-collected VMMAP summary (per-entry start/end/prot). */
static void g5_vmmap(IgSink *sink, IgBuf *b, int *seq) {
    int64_t frc = (int64_t)g_facts.vmmap_rc;
    if (g_facts.vmmap_rc != 0 && g_facts.vmmap_errno == ENOMEM) frc = -3;
    record_out(b, seq, "g5_vmmap", frc, (int64_t)g_facts.vmmap_errno);
    ig_kv_int(b, "final_buf", (int64_t)g_facts.vmmap_buf);
    ig_kv_int(b, "kve_size", (int64_t)g_facts.vmmap_kve_size);
    ig_kv_int(b, "entries", (int64_t)g_facts.vmmap_entries);
    ig_kv_int(b, "attempts", (int64_t)g_facts.vmmap_attempts);
    ig_kv_int(b, "spans_kept", (int64_t)g_facts.span_count);
    ig_kv_int(b, "spans_truncated", g_facts.span_truncated);
    /* Distinct raw protection values with counts (bounded 6). */
    uint32_t dv[6]; uint32_t dc[6]; int dn = 0;
    for (uint32_t i = 0; i < g_facts.span_count && dn < 6; ++i) {
        uint32_t p = g_facts.spans[i].prot;
        int f = -1;
        for (int k = 0; k < dn; ++k) if (dv[k] == p) { f = k; break; }
        if (f < 0 && dn < 6) { dv[dn] = p; dc[dn] = 1; ++dn; }
        else if (f >= 0) ++dc[f];
    }
    ig_buf_raw(b, ",\"prot_values\":[");
    for (int k = 0; k < dn; ++k) {
        if (k != 0) ig_buf_ch(b, ',');
        ig_buf_ch(b, '{');
        ig_kv_hex(b, "prot", (uint64_t)dv[k]);
        ig_kv_int(b, "count", (int64_t)dc[k]);
        ig_buf_ch(b, '}');
    }
    ig_buf_ch(b, ']');
    /* First 16 entries {start,end,prot}; truncation flagged by the record. */
    ig_buf_raw(b, ",\"entries\":[");
    int emit_n = g_facts.span_count < 16 ? (int)g_facts.span_count : 16;
    for (int i = 0; i < emit_n; ++i) {
        if (i != 0) ig_buf_ch(b, ',');
        ig_buf_ch(b, '{');
        ig_kv_hex(b, "start", g_facts.spans[i].start);
        ig_kv_hex(b, "end", g_facts.spans[i].end);
        ig_kv_hex(b, "prot", (uint64_t)g_facts.spans[i].prot);
        ig_buf_ch(b, '}');
        if (b->truncated) break;
    }
    ig_buf_ch(b, ']');
    emit(sink, b);
}

/* ------------------------------------------------------------------ */
/* G6: layout predicates at the established eboot base. */
static void g6_layout_predicates(IgSink *sink, IgBuf *b, int *seq) {
    uint64_t base = 0;
    const char *prov = "needs-base";
    int have = 0;
    if (g_facts.exec.plausible && g_facts.exec.info_rc == 0 &&
        g_facts.exec.size_echo_ok && g_facts.exec.seg0_addr != 0 &&
        ig_range_readable(g_facts.exec.seg0_addr, 64)) {
        base = g_facts.exec.seg0_addr;
        prov = "g1_exec_module_info_seg0";
        have = 1;
    } else if (g_facts.g4_eboot_index >= 0 && g_facts.g4_eboot_seg0 != 0 &&
               ig_range_readable(g_facts.g4_eboot_seg0, 64)) {
        base = g_facts.g4_eboot_seg0;
        prov = "g4_module_info_eboot_seg0";
        have = 1;
    } else if (g_facts.g2_eboot_linkmap_laddr &&
               ig_range_readable(g_facts.g2_eboot_laddr, 64)) {
        base = g_facts.g2_eboot_laddr;
        prov = "g2_link_map_eboot_l_addr";
        have = 1;
    }
    g_facts.base = base;
    g_facts.base_ok = have;
    ig_strlcpy_bounded(g_facts.base_provenance, sizeof g_facts.base_provenance,
                       prov, 39);
    g_facts.base_g1 = g_facts.exec.seg0_addr;
    g_facts.base_g4 = g_facts.g4_eboot_seg0;
    g_facts.base_g2 = g_facts.g2_eboot_linkmap_laddr ? g_facts.g2_eboot_laddr : 0;

    if (!have) {
        record_out(b, seq, "g6_layout_predicates", -1, 0);
        ig_kv_str(b, "status", "needs-base", 16);
        ig_kv_str(b, "note",
                  "no validated eboot base; predicates skipped, never guessed",
                  64);
        emit(sink, b);
        return;
    }

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

    record_out(b, seq, "g6_layout_predicates", 0, 0);
    ig_kv_hex(b, "base", base);
    ig_kv_str(b, "base_provenance", prov, 39);
    ig_kv_hex(b, "base_g1", g_facts.base_g1);
    ig_kv_hex(b, "base_g4", g_facts.base_g4);
    ig_kv_hex(b, "base_g2", g_facts.base_g2);
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
/* G7: firmware signal + summary. kernel_get_fw_version is crt1-provided and
   a PRX links no crt1, so the sysctl KERN_VERSION string is attempted
   instead; whatever returns is recorded verbatim. */
static void g7_fw_summary(IgSink *sink, IgBuf *b, int *seq) {
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
    record_out(b, seq, "g7_fw_summary", (int64_t)rc, (int64_t)eno);
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
    ig_kv_str(b, "quiesce", "inert_no_threads_module_stays_loaded", 40);
    ig_kv_str(b, "unload_attempted", "no", 4);
    ig_kv_str(b, "log_path", IG_LOG_PATH, 64);
    emit(sink, b);
}

void ig_resolve_eboot_path(void) {
    /* G3 input: prefer a G2 link-map path (full path), else a G1 module
       info name if it already contains a slash. Never guessed. */
    g_facts.eboot_path_ok = 0;
    g_facts.eboot_path[0] = '\0';
    if (g_facts.g2_eboot_linkmap_laddr && g_facts.g2_eboot_lname[0] == '/') {
        ig_strlcpy_bounded(g_facts.eboot_path, sizeof g_facts.eboot_path,
                           g_facts.g2_eboot_lname, sizeof g_facts.g2_eboot_lname - 1);
        g_facts.eboot_path_ok = 1;
    } else if (g_facts.exec.plausible && g_facts.exec.info_rc == 0) {
        size_t n = ig_strlen(g_facts.exec.name);
        int slash = 0;
        for (size_t i = 0; i < n; ++i)
            if (g_facts.exec.name[i] == '/') { slash = 1; break; }
        if (slash && n > 0) {
            ig_strlcpy_bounded(g_facts.eboot_path, sizeof g_facts.eboot_path,
                               g_facts.exec.name,
                               sizeof g_facts.exec.name - 1);
            g_facts.eboot_path_ok = 1;
        }
    }
}

void ig_run_all_experiments(IgSink *sink) {
    IgBuf b;
    int seq = 0;
    g1_exec_handle(sink, &b, &seq);
    g2_dtdebug(sink, &b, &seq);
    ig_resolve_eboot_path();
    g3_noload_eboot(sink, &b, &seq);
    g4_module_list(sink, &b, &seq);
    g5_vmmap(sink, &b, &seq);
    g6_layout_predicates(sink, &b, &seq);
    g7_fw_summary(sink, &b, &seq);
}
