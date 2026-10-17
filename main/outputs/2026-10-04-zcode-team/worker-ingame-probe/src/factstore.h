// factstore.h — cross-experiment facts for the in-game probe (single-threaded,
// module_start scope only; no other thread can observe these).
#ifndef INGAME_FACTSTORE_H
#define INGAME_FACTSTORE_H

#include "ingame_abi.h"
#include <stdint.h>

#define IG_SPAN_MAX 1024        // vmmap entries retained for validation
#define IG_VMMAP_MAX (4u << 20) // brief bound: 4 MiB buffer growth
#define IG_LINKMAP_MAX 64       // brief bound: link-map walk
#define IG_MODULELIST_CAP 128   // brief bound: module handles

typedef struct IgSpan {
    uint64_t start;   // inclusive
    uint64_t end;     // exclusive
    uint32_t prot;    // raw kve_protection, uninterpreted
} IgSpan;

typedef struct IgModuleFacts {
    int attempted;
    int64_t raw;
    int plausible;
    int info_rc;
    int info_errno;
    int size_echo_ok;
    char name[96];             // bounded copy of ModuleInfo.name
    uint64_t seg0_addr;
    uint32_t seg0_size;
    int32_t seg0_prot;
    uint32_t segment_count;
} IgModuleFacts;

typedef struct IgFacts {
    /* Collected EARLY (before G1) for fault containment; EMITTED as G5. */
    IgSpan spans[IG_SPAN_MAX];
    uint32_t span_count;
    int span_truncated;
    uint64_t vmmap_buf;
    uint64_t vmmap_entries;
    uint64_t vmmap_kve_size;
    unsigned vmmap_attempts;
    int vmmap_rc;
    int vmmap_errno;

    /* G1 */
    IgModuleFacts exec;

    /* G2 */
    int g2_dt_debug_present;
    uint64_t g2_dt_debug_value;
    uint64_t g2_r_map;
    int g2_entries;
    int g2_truncated;
    int g2_blocked;            // dereferences refused by readability checks
    int g2_eboot_linkmap_laddr;
    uint64_t g2_eboot_laddr;
    char g2_eboot_lname[96];

    /* G3 */
    char eboot_path[160];      // candidate path from G1/G2
    int eboot_path_ok;

    /* G4 */
    int g4_rc_4n, g4_rc_n;
    int g4_eno_4n, g4_eno_n;
    uint64_t g4_avail_4n, g4_avail_n;
    uint32_t g4_handles_4n[IG_MODULELIST_CAP];
    uint32_t g4_handles_n[IG_MODULELIST_CAP];
    uint32_t g4_count_4n, g4_count_n;   // plausible handle counts
    int g4_eboot_index;         // -1 none
    uint64_t g4_eboot_seg0;
    char g4_eboot_name[96];

    /* G6 */
    uint64_t base;
    int base_ok;
    char base_provenance[40];
    uint64_t base_g1, base_g4, base_g2;
} IgFacts;

extern IgFacts g_facts;

/* errno read through the imported __error word; 0 when unavailable. */
static inline int ig_errno(void) {
    int *e = __error();
    return e ? *e : 0;
}

/* True iff [addr, addr+len) lies fully inside one READABLE (prot bit0) span.
   Used BEFORE every foreign-pointer dereference (G2 link-map, eboot ELF
   headers, G6 layout reads): a false value means the dereference is refused
   and recorded, never attempted. */
int ig_range_readable(uint64_t addr, uint64_t len);

/* Copy at most cap-1 bytes; returns bytes copied; refuses unreadable ranges. */
size_t ig_copy_checked(uint64_t addr, void *dst, size_t len);

#endif  // INGAME_FACTSTORE_H
