// carrier_driver.h — worker-carrier-probe-build (cc1, team 2026-10-04).
// NEW FILE. The cc1 ridealong probe mission, exactly as specified by
// worker-r3-carrier-prep/CARRIER-CANDIDATES.md sec 6 (fc1 -> qdrv1 -> cc1
// lineage):
//
//   (a) DATA-SIDE census of the LIVE FName pool for the fixed candidate
//       list below (3 ranked carrier candidates + 19 walk class-strings +
//       the AActor/UAnimInstance ctor-family decoder controls + the
//       "None"/"ByteProperty" positive controls + the "PostRender" /
//       "ZZZNotARealFunctionQQ" negative controls). Every name in the
//       table is image-confirmed (exact UTF-16 in the pinned decrypted
//       eboot, sha 3b6cfad4…) with the linked-label vaddr recorded in the
//       comment column; runtime = 0x400000 + vaddr. The census MATCHES
//       names only; the vaddrs are offline provenance, never dereferenced.
//   (b) GUA admission + survival evidence on the qdrv1 pattern
//       (header-window rcodes 1-7, 10000-count layout gate, 30 x 4 s
//       cadence, one detached thread, NO mutex imports).
//   (c) Pool decode reuses fc1's VALIDATED structural offsets and decoder
//       (CurrentBlock @ pool+0x8, Blocks[0] @ pool+0x10, bootstrap ids at
//       pool+0x14040/+0x14044, entry u16 header bit0 = bIsWide,
//       Blocks[id>>16] + (id&0xFFFF)*2, ASCII at entry+2, NUL required).
//
// cc1 does NOT deliver FUNC_Native, invocation rate, or thread identity
// (CANDIDATES sec 6.1): those stay with the in-runtime R3 calibration.
// READS ONLY: no write to game memory, no mprotect, no raw syscall
// (syscall gate == 0 asserted at build). Fail-open: every refusal is a
// recorded terminal verdict, never a retry, never a guess.
#ifndef CARRIER_DRIVER_H
#define CARRIER_DRIVER_H

#include "record_sink.h"

#include <stdint.h>

/* Cadence (CANDIDATES sec 6.2): ONE detached thread, 30 polls x 4 s
   (~2 min bounded window), bounded spin, no mutex. */
#define CC1_POLL_COUNT 30u
#define CC1_POLL_SECONDS 4L

/* Pool-window constants — byte-identical to fc1pool_driver.h (fc1's
   validated decoder, FNAME-VERDICT.md sec 4). */
#define CC1_OFF_CURRENT_BLOCK 0x8u
#define CC1_OFF_CURRENT_BO    0xCu
#define CC1_OFF_BLOCKS0       0x10u
#define CC1_OFF_ID_NONE       0x14040u
#define CC1_OFF_ID_BYTEPROP   0x14044u
#define CC1_POOL_WINDOW       0x20u
#define CC1_HANDLES_WINDOW    0x8u
#define CC1_ENTRY_WINDOW      40u        /* header u16 + up to 38 chars */
#define CC1_MAX_BLOCKS        8192u      /* FNameEntryAllocator::Blocks[8192] */

/* qdrv1 admission gates (qh_driver.cpp): GUA header 96 bytes; the census
   arms only on an admitted header whose NumElements rides STRICTLY ABOVE
   the harness's kMinimumPlausibleElementCount (= 10000) threshold. */
#define CC1_GUA_HEADER_BYTES 96u
#define CC1_GATE_THRESHOLD 10000LL

/* Census budget (CANDIDATES sec 6.2): bounded entries-scanned counter;
   hard stop at budget with census_truncated:1 honesty field; progress
   marker every 4096 entries. Matches ONLY the fixed table below. */
#ifndef CC1_CENSUS_BUDGET_ENTRIES
#define CC1_CENSUS_BUDGET_ENTRIES 1048576u
#endif
#define CC1_PROGRESS_EVERY 4096u
#define CC1_MAX_MATCHES 64u          /* census_match line cap; overflow flagged */

/* The census name table. kind: 0 = candidate carrier, 1 = ctor-family
   decoder control, 2 = walk class-string, 3 = positive control,
   4 = NEGATIVE control (counted in negative_hits, never in matched[]).
   Image-confirmed UTF-16 vaddrs (linked-label convention) from
   CARRIER-CANDIDATES.md sec 3/4 — offline provenance for every row:
   candidates: ReceiveTick 0x76c0264, BlueprintUpdateAnimation 0x773e91e,
   Tick 0x7521050; classes: Actor 0x75dd4a4, Pawn 0x78275c4,
   Character 0x75f1fc2, PlayerController 0x7765882, GameModeBase 0x783a91a,
   GameStateBase 0x76c22e0, GameViewportClient 0x761c342, HUD 0x7577a14,
   PlayerCameraManager 0x77bb500, WorldSettings 0x78b4150,
   LevelScriptActor 0x7812e02, AnimInstance 0x75f17ee, UserWidget
   0x773d344, ItemDatabaseSubsystem 0x77eff56, Info 0x7a151a4,
   Engine 0x782706e, PlayerState 0x77927fe, Controller 0x78b53b6,
   CameraComponent 0x759e336; family rows: AActor ctor blob 0x2b45000-
   0x2b48000 (region-names.json, 54 names) / UAnimInstance blob
   0x2bba000-0x2bbc000; negative controls PostRender / ZZZNotARealFunctionQQ
   are NAME-REFUTED in the image (sec 3 rank 4) and must stay at 0 live. */
#define CC1_NAME_MAX 42u  /* longest table name (41 chars) + 1 */
typedef struct Cc1Name {
    const char *name;
    uint8_t kind;
} Cc1Name;
extern const Cc1Name cc1_name_table[];
extern const unsigned cc1_name_table_len;
/* table indexes of the controls the census_result reports explicitly
   (candidates 0-2, family 3-29, classes 30-48, positives 49-50,
   negatives 51-52; 53 rows total, inside the sec-6.2 <= 96 budget) */
#define CC1_IDX_NONE 49
#define CC1_IDX_BYTEPROPERTY 50
#define CC1_IDX_POSTRENDER 51
#define CC1_IDX_ZZZNEG 52

/* Verdict + refusal codes (recorded in "reason_code"; "none" if validated).
   The first four codes are CANDIDATES sec 6.3's census refusal vocabulary;
   the rest are fc1-lineage chain codes reused verbatim, plus the two
   census-walk codes (documented extension, same fail-open discipline). */
#define CC1_RC_MAP_UNTRUSTED        "map_untrusted"
#define CC1_RC_ANCHOR_NOT_ADMITTED  "anchor_not_admitted"
#define CC1_RC_GATE_NEVER_OPENED    "census_gate_closed_after_grace"
#define CC1_RC_UNINITIALIZED        "pool_uninitialized_after_grace"
#define CC1_RC_POOL_UNCOVERED       "pool_window_uncovered"
#define CC1_RC_POOL_UNREADABLE      "pool_window_not_readable"
#define CC1_RC_CB_INSANE            "current_block_insane"
#define CC1_RC_BLOCKS0_INVALID      "blocks0_invalid"
#define CC1_RC_BLOCKS0_UNCOVERED    "blocks0_uncovered"
#define CC1_RC_HANDLES_UNCOVERED    "handles_window_uncovered"
#define CC1_RC_HANDLES_DEGENERATE   "bootstrap_ids_degenerate"
#define CC1_RC_HANDLE_OOB           "handle_block_index_oob"
#define CC1_RC_BLOCKPTR_UNCOVERED   "block_pointer_window_uncovered"
#define CC1_RC_BLOCKPTR_INVALID     "block_pointer_invalid"
#define CC1_RC_ENTRY_UNCOVERED      "entry_window_uncovered"
#define CC1_RC_ENTRY_WIDE           "entry_wide_flag_set"
#define CC1_RC_NONE_MISMATCH        "none_ascii_mismatch"
#define CC1_RC_BP_MISMATCH          "byteproperty_ascii_mismatch"
#define CC1_RC_CENSUS_BLOCKPTR      "census_block_pointer_invalid"
#define CC1_RC_CENSUS_ENTRY         "census_entry_window_uncovered"

/* forward: anchor_store.h owns the definition */
struct IgSpan;

typedef struct Cc1Ops {
    int (*mapscan_run)(void);                          /* 1 = trusted map   */
    int (*cover)(uint64_t addr, uint64_t len, struct IgSpan *out);
    void (*deref)(void *dst, uint64_t src, unsigned n);
    int (*sleep_sec)(long sec);                        /* 0 ok              */
    int (*log_line)(const char *line, unsigned len);   /* UE4SS.log line    */
} Cc1Ops;

/* Spawns the ONE bounded census thread (30 x 4 s), like qdrv1's
   qh_driver_start. seq 1/2 (entry/startup) are already written by
   module_start; the thread emits the mapscan record (seq 3), one
   qh_counter envelope per poll, census_match lines (one per matched
   name), the census_result verdict record, and the summary record; then
   closes the channel and exits. Returns 0 on thread spawn ok.
   Host tests (CC1_HOST_TEST) run the loop synchronously instead. */
int carrier_driver_start(IgSink *sink);

#endif  /* CARRIER_DRIVER_H */
