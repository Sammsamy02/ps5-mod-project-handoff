// factstore.h — cross-experiment facts for the v2 isolation probe
// (worker-ingame-probe-v2). Vendored from worker-ingame-probe/src/factstore.h;
// G1/G3/G4 module/dl facts replaced by the V2 ELF base-scan state.
// Single-threaded, module_start scope only; no other thread can observe these.
#ifndef INGAME_FACTSTORE_H
#define INGAME_FACTSTORE_H

#include "ingame_abi.h"
#include <stdint.h>

#define IG_SPAN_MAX 1024        // vmmap entries retained for validation
#define IG_VMMAP_MAX (4u << 20) // brief bound: 4 MiB buffer growth
#define IG_LINKMAP_MAX 64       // brief bound: link-map walk
#define IG_CAND_MAX 64          // brief bound: ELF base candidates

typedef struct IgSpan {
    uint64_t start;   // inclusive
    uint64_t end;     // exclusive
    uint32_t prot;    // raw kve_protection, uninterpreted (bit0 observed readable)
} IgSpan;

/* One candidate image base enumerated from the VMMAP span set (V2). Every
   field is evidence: nothing is guessed, unreadable = 0/refused. */
typedef struct IgCand {
    uint64_t base;            // span start where ELF magic was probed
    uint64_t span_start;      // owning span bounds (evidence)
    uint64_t span_end;
    uint32_t prot;
    char where[16];           // "rx_span" | "r_adjacent_exec"
    int magic_ok;             // 0x7fELF at base, span-validated read
    int class_ok;             // ELFCLASS64
    uint16_t e_type;          // raw e_type (2=EXEC 3=DYN observed values)
    uint16_t phnum;
    int phdr_ok;              // phdr table fully span-readable, sane entsize
    uint64_t dyn_vaddr;       // PT_DYNAMIC p_vaddr (0 if none/unreadable)
    uint64_t dyn_filesz;
    int dyn_found;
    int dyn_mode;             // 0 none, 1 dyn@base+vaddr, 2 dyn@absolute vaddr
    int dyn_parses;           // dynamic walk saw entries (and DT_NULL or cap)
    int dyn_entries;          // entries actually scanned
    int dt_null_seen;
    int has_exec_segment;     // some PT_LOAD carries PF_X
    uint64_t extent;          // max(p_vaddr+p_memsz) over PT_LOADs (vaddr space)
    int chosen;               // 1 for the single chosen main-image candidate
} IgCand;

typedef struct IgFacts {
    /* Collected EARLY (before all experiments) for fault containment;
       EMITTED as V1 (full list, chunked records). */
    IgSpan spans[IG_SPAN_MAX];
    uint32_t span_count;
    int span_truncated;
    uint64_t vmmap_buf;
    uint64_t vmmap_entries;
    uint64_t vmmap_kve_size;
    unsigned vmmap_attempts;
    int vmmap_rc;
    int vmmap_errno;

    /* V2 scan */
    IgCand cands[IG_CAND_MAX];
    uint32_t cand_count;
    int cand_truncated;
    int chosen_index;         // -1 = no candidate parsed (V3/V4 needs-base)
    uint64_t chosen_base;
    uint64_t chosen_extent;
    uint64_t chosen_dyn_addr; // resolved dynamic address under chosen mode
    int chosen_dyn_mode;

    /* V3 */
    int v3_dt_debug_present;
    uint64_t v3_dt_debug_value;
    uint64_t v3_r_map;
    int v3_entries;
    int v3_truncated;
    int v3_blocked;           // dereferences refused by readability checks
    int v3_eboot_linkmap_laddr;
    uint64_t v3_eboot_laddr;
    char v3_eboot_lname[96];
} IgFacts;

extern IgFacts g_facts;

/* errno read through the imported __error word; 0 when unavailable. */
static inline int ig_errno(void) {
    int *e = __error();
    return e ? *e : 0;
}

/* True iff [addr, addr+len) lies fully inside one READABLE (prot bit0) span.
   Used BEFORE every foreign-pointer dereference (V2 ELF/phdr/dynamic reads,
   V3 r_debug/link_map/l_name, V4 layout reads): a false value means the
   dereference is refused and recorded, never attempted. */
int ig_range_readable(uint64_t addr, uint64_t len);

/* Copy at most len bytes; returns bytes copied; refuses unreadable ranges. */
size_t ig_copy_checked(uint64_t addr, void *dst, size_t len);

#endif  // INGAME_FACTSTORE_H
