// fc1pool_driver.h — worker-fc1-validator-probe (fc1pool1, team 2026-10-04).
// NEW FILE. Data-side FName-pool validation chain, exactly the chain specified
// by worker-fname-ctor-audit FNAME-VERDICT.md sec 5 ("in-game data-side
// validation of the pool first") and restated in the fc1-validator-probe task:
//   1. CurrentBlock (u32 @ pool+0x8) >= 1;
//   2. Blocks[0] (@ pool+0x10) nonzero and inside a current readable span;
//   3. entry id at pool+0x14040 resolves via Blocks[] to the ASCII string
//      "None" (and pool+0x14044 -> "ByteProperty") — header u16 then chars.
// The base is re-derived per launch from the GUA anchor
// (validated runtime GUA 0x97ba110 - label 0x93ba110), and the pool label
// (file-image 0x9365ec0) is only ever read at base+label, every read gated by
// the CURRENT per-invocation sysctl KERN_PROC_VMMAP scan (controller
// correction #1 discipline). READS ONLY: no write, no mprotect, no raw
// syscall anywhere (syscall gate == 0 asserted at build). Fail-open: every
// refusal is a recorded terminal verdict, never a retry, never a guess.
#ifndef FC1POOL_DRIVER_H
#define FC1POOL_DRIVER_H

#include "record_sink.h"

#include <stdint.h>

/* Grace policy: pool initialization (engine FName static init) may lag a
   late (mode-2, min_mod 16) module load; the probe retries ONLY the
   transient stages (map untrusted / anchor admission / CurrentBlock==0),
   at most kGracePolls polls spaced kGraceSleepMs milliseconds apart, then
   records refused_after_grace. Hard pool-stage refusals stop immediately. */
#define FC1_GRACE_POLLS 8
#define FC1_GRACE_SLEEP_MS 1000

/* Pool-window constants (byte offsets into the FNameEntryAllocator, cited
   from worker-fname-ctor-audit FNAME-VERDICT.md sec 4: callees read
   [pool+0x8] CurrentBlock, [pool+0xC] CurrentByteOffset, [pool+0x10]
   Blocks[0], [pool+0x18] Blocks[1]; init stores bootstrap ids at
   pool+0x14040 ("None") / pool+0x14044 ("ByteProperty")). */
#define FC1_OFF_CURRENT_BLOCK 0x8u
#define FC1_OFF_CURRENT_BO    0xCu
#define FC1_OFF_BLOCKS0       0x10u
#define FC1_OFF_ID_NONE       0x14040u
#define FC1_OFF_ID_BYTEPROP   0x14044u
#define FC1_POOL_WINDOW       0x20u       /* [pool, pool+0x20) */
#define FC1_HANDLES_WINDOW    0x8u        /* [+0x14040, +0x14048) */
#define FC1_ENTRY_WINDOW      40u         /* header u16 + up to 38 chars */
#define FC1_MAX_BLOCKS        8192u       /* FNameEntryAllocator::Blocks[8192] */

/* Verdict + refusal codes (recorded in "reason_code"; "" when validated). */
#define FC1_RC_MAP_UNTRUSTED        "map_untrusted"
#define FC1_RC_ANCHOR_NOT_ADMITTED  "anchor_not_admitted"
#define FC1_RC_POOL_UNCOVERED       "pool_window_uncovered"
#define FC1_RC_POOL_UNREADABLE      "pool_window_not_readable"
#define FC1_RC_UNINITIALIZED        "pool_uninitialized_after_grace"
#define FC1_RC_CB_INSANE            "current_block_insane"
#define FC1_RC_BLOCKS0_INVALID      "blocks0_invalid"
#define FC1_RC_BLOCKS0_UNCOVERED    "blocks0_uncovered"
#define FC1_RC_HANDLES_UNCOVERED    "handles_window_uncovered"
#define FC1_RC_HANDLES_DEGENERATE   "bootstrap_ids_degenerate"
#define FC1_RC_HANDLE_OOB           "handle_block_index_oob"
#define FC1_RC_BLOCKPTR_UNCOVERED   "block_pointer_window_uncovered"
#define FC1_RC_BLOCKPTR_INVALID     "block_pointer_invalid"
#define FC1_RC_ENTRY_UNCOVERED      "entry_window_uncovered"
#define FC1_RC_ENTRY_WIDE           "entry_wide_flag_set"
#define FC1_RC_NONE_MISMATCH        "none_ascii_mismatch"
#define FC1_RC_BP_MISMATCH          "byteproperty_ascii_mismatch"

/* forward: anchor_store.h owns the definition */
struct IgSpan;

typedef struct Fc1Ops {
    int (*mapscan_run)(void);                          /* 1 = trusted map   */
    int (*cover)(uint64_t addr, uint64_t len, struct IgSpan *out);
    void (*deref)(void *dst, uint64_t src, unsigned n);
    int (*sleep_ms)(long ms);                          /* 0 ok              */
    int (*log_line)(const char *line, unsigned len);   /* UE4SS.log line    */
} Fc1Ops;

/* One-shot synchronous run: entry/startup records are already written by
   module_start (seq 1/2); this emits per-poll envelopes (seq 3..),
   ONE "fc1pool_result" detail record, ONE "summary" record, and exactly one
   UE4SS.log line at the terminal verdict. Always returns 0 (all refusals are
   recorded, never surfaced as module failure). */
int fc1pool_driver_run(IgSink *sink);

#endif  /* FC1POOL_DRIVER_H */
