// exec_write_probe.h — worker-writability-probe-build (team 2026-10-04).
//
// PRE-BUILT SKELETON for the worker-exec-writability mprotect-transition
// design. This header defines the TRANSITION TABLE row shape and the
// per-attempt machinery contract; the design worker fills transitions via
// TRANSITIONS.json -> generated/xwp_transitions.h (build-time injection,
// machinery files untouched — see FILL-IN-FOR-DESIGN-WORKER.md).
//
// SAFETY CONTRACT (baked into exec_write_probe.c, not negotiable by config):
//   - SKELETON builds are compile-time unarmed: igx_sys_mprotect is a local
//     stub that NEVER performs a protection change; records prove logic only.
//   - ARMED builds: an attempt requires ALL of (a) IG_XWP_ARMED=1,
//     (b) row.confirmed != 0 (the design's designated far-from-hooks page),
//     (c) attempts_used < XWP_ATTEMPT_BUDGET (8),
//     (d) a FRESH same-invocation sysctl KERN_PROC_VMMAP walk that covers
//         [addr, addr + page_count*0x1000) fully,
//     (e) the covering span's observed prot == row.from_prot (bit0 R,
//         bit1 W, bit2 X — VMMAP semantics; exec-only = 0x4),
//     otherwise the refusal is recorded and NOTHING is attempted.
//   - Restore is attempted ALWAYS (success or failure), to row.from_prot,
//     and a post-restore VMMAP re-read is recorded.
//   - Zero raw syscalls (igf6/igf7 inverted syscall gate); the ARMED mprotect
//     route is the imported sceKernelMprotect spelling ONLY.
//   - module_start returns 0 on every path (fail-open).
//
// X->RX first, then X->RWX on ONE far-from-hooks page, is the DESIGN's
// sequencing decision — the machinery executes table rows in order and
// records everything; ordering belongs to TRANSITIONS.json.
#ifndef EXEC_WRITE_PROBE_H
#define EXEC_WRITE_PROBE_H

#include <stddef.h>
#include <stdint.h>

#include "record_sink.h"

/* VMMAP kve_protection bits (lineage semantics: bit0 readable, bit1
   writable, bit2 executable; igf6 proved prot=0x4 spans are kernel policy
   for exec, NOT hardware read enforcement). */
#define XWP_PROT_R 1u
#define XWP_PROT_W 2u
#define XWP_PROT_X 4u

/* Total attempt budget across ALL table rows (brief: e.g. 8). */
#define XWP_ATTEMPT_BUDGET 8u
/* Upper bound on table rows accepted from the generated header (the design
   may list more; rows beyond this are recorded as overflow, never run). */
#define XWP_MAX_ROWS 32u
#define XWP_NOTE_MAX 96

/* One design-injected transition. EMPTY placeholder rows ship by default;
   the design fills them (see FILL-IN-FOR-DESIGN-WORKER.md). */
typedef struct XwpTransition {
    uint64_t addr;        /* absolute target vaddr (design resolves rt) */
    uint64_t from_prot;   /* expected VMMAP prot before the attempt */
    uint64_t to_prot;     /* requested prot for the attempt */
    uint64_t page_count;  /* pages covered (attempt length = *0x1000) */
    uint64_t confirmed;   /* 1 ONLY on the design's designated far page */
    const char *note;     /* design's row note (bounded at record time) */
} XwpTransition;

/* The table itself is GENERATED (generated/xwp_transitions.h):
 *   xwp_transitions[]  — const XwpTransition[...]
 *   xwp_transitions_count — const unsigned
 * Every binary carries at least the empty placeholder table. */
#include "generated/xwp_transitions.h"

/* Attempt/refusal outcome codes (recorded, never propagated). */
typedef enum XwpOutcome {
    XWP_OK = 0,
    XWP_SKIP_UNARMED = 1,        /* build-time unarmed (skeleton) */
    XWP_SKIP_NOT_CONFIRMED = 2,  /* row.confirmed == 0 */
    XWP_SKIP_BUDGET = 3,         /* attempt budget exhausted */
    XWP_SKIP_ROW_OVERFLOW = 4,   /* row index >= XWP_MAX_ROWS */
    XWP_REFUSE_NOT_COVERED = 5,  /* fresh VMMAP does not cover [addr,len) */
    XWP_REFUSE_PROT_MISMATCH = 6,/* observed prot != from_prot */
    XWP_ATTEMPT_MPROTECT_FAIL = 7,   /* mprotect rc<0 (errno recorded) */
    XWP_ATTEMPT_OK = 8,          /* mprotect rc==0 */
    XWP_RESTORE_FAIL = 9,        /* restore rc<0 (errno recorded) */
    XWP_RESTORE_OK = 10,
    XWP_MPROTECT_IMPORT_ABSENT = 11  /* skeleton stub reached */
} XwpOutcome;

/* The mprotect binding point. SKELETON: local hidden stub (returns -1 with
   errno EINVAL, outcome XWP_MPROTECT_IMPORT_ABSENT, performs NOTHING).
   ARMED: extern int sceKernelMprotect(void *, size_t, int) — the ONLY
   mprotect-class identity this probe may ever bind (see CLAIM.json for the
   loud provenance flag). The indirection exists so the host test can inject
   a MOCK without touching the machinery. */
typedef int (*xwp_mprotect_fn)(void *, size_t, int);

/* Attempt counters (single-threaded module_start contract; plain globals
   are fine, but atomics cost nothing and keep the lineage shape). */
extern unsigned g_xwp_attempts_used;
extern unsigned g_xwp_rows_seen;
extern unsigned g_xwp_rows_attempted;
extern unsigned g_xwp_rows_refused;
extern unsigned g_xwp_rows_skipped;

/* Run the WHOLE transition table against the two sinks (jsonl primary,
   UE4SS.log mirror). Purely sequential; every record is flushed before the
   next operation. Always returns 0 (fail-open). */
int ig_run_exec_write_probe(IgSink *jsonl, IgSink *mirror);

/* Host-test hook: swap the mprotect binding before ig_run_exec_write_probe.
   Passing 0 restores the default binding for the current build mode. */
void ig_xwp_set_mprotect_for_host_test(xwp_mprotect_fn fn);

#endif  // EXEC_WRITE_PROBE_H
