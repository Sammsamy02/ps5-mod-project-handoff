/*
 * ps5_prepare_bridge.h — installer "prepare" stage -> native archive intake.
 *
 * Origin: worker-installer-ps5io, 2026-10-04.
 *
 * Purpose: worker-7's installer starts from per-mod .mfb bundles; archives
 * (ZIP/7z/RAR) are classified and extracted by the pinned native intake
 * (outputs/ps5-mod-bridge/native-host/archive/intake.c, ABI v1). This bridge
 * is the exact call path from the installer's prepare stage to that intake:
 *
 *   downloaded archive fd
 *     -> mb_archive_scan_v1            (inventory: rows, names, sha256, format)
 *     -> row selection (default: all regular files; policy hook supplied)
 *     -> mb_archive_materialize_v1     (extract selection into the owned
 *                                       quarantine directory, journal retained)
 *     -> ps5_prepare_result            (what the installer records as
 *                                       "prepared": quarantine identity,
 *                                       manifest name, inventory sha256)
 *
 * The intake functions are reached ONLY through ps5_intake_api so that:
 *   - the console composition binds exactly { mb_archive_limits_default_v1,
 *     mb_archive_budget_default_v1, mb_archive_scan_v1,
 *     mb_archive_materialize_v1 } from the pinned 3.8.7 closure objects
 *     (controller-composed link; see dependency_build.py consumers);
 *   - host stub tests prove this bridge's sequencing/arguments without the
 *     libarchive closure.
 *
 * Filesystem needs are minimal on purpose: the intake owns extraction; the
 * bridge only opens the quarantine directory (ps5_io) to hand intake the
 * quarantine descriptor, device and inode it demands, and never writes
 * archive content itself.
 *
 * HONESTY: not executed on a console; the real intake link is the
 * controller's composition step. Journal semantics (survives failure, caller
 * retains/exports) follow intake.h verbatim.
 */
#ifndef PS5_PREPARE_BRIDGE_H
#define PS5_PREPARE_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#include "intake.h"          /* vendored, byte-identical archive intake ABI v1 */

#ifdef __cplusplus
extern "C" {
#endif

/* Dispatch over the pinned intake symbols. Console composition must install
 * exactly the four pinned functions; tests install stubs. */
typedef struct {
    void (*limits_default)(mb_archive_limits_v1 *limits);
    void (*budget_default)(mb_archive_budget_v1 *budget);
    int  (*scan)(const mb_archive_source_v1 *source,
                 const mb_archive_limits_v1 *limits, mb_archive_budget_v1 *budget,
                 const mb_archive_buffers_v1 *buffers, mb_archive_inventory_v1 *result);
    int  (*materialize)(const mb_archive_materialize_request_v1 *request,
                        const mb_archive_limits_v1 *limits, mb_archive_budget_v1 *budget,
                        mb_archive_materialized_v1 *result);
} ps5_intake_api;

/* Row admission policy: return 1 to materialize, 0 to skip. name is the
 * row's logical name from the inventory name pool. */
typedef int (*ps5_prepare_selector)(void *ctx, const mb_archive_row_v1 *row,
                                    const char *name);

typedef struct {
    /* Download-stage output (the controller's owner interface hands these in):
     * archive opened read-only; immutability of the source is the caller's
     * obligation per intake.h. */
    int       archive_fd;
    uint64_t  archive_size;
    uint8_t   archive_sha256[32];   /* identity of the downloaded bytes */
    const char *quarantine_dir;     /* absolute owned intake staging root */

    /* Policy. selector == NULL -> all MB_ARCHIVE_REGULAR rows. */
    ps5_prepare_selector selector;
    void *selector_ctx;
    uint64_t deadline_ms;           /* 0 = none; CLOCK_MONOTONIC ms */
    int (*cancelled)(void *ctx);    /* may be NULL */
    void *cancel_ctx;

    /* Caller scratch, sized per intake.h rules. rows/names/sort are the scan
     * buffers; replay_* are the independent materialize-replay buffers;
     * journal capacity must be >= selected_count + 3. */
    mb_archive_row_v1  *rows;        uint32_t row_capacity;
    char               *names;       uint32_t name_capacity;
    uint32_t           *sort;        uint32_t sort_capacity;
    uint8_t            *chunk;       size_t chunk_capacity;
    mb_archive_row_v1  *replay_rows; uint32_t replay_row_capacity;
    char               *replay_names; uint32_t replay_name_capacity;
    uint32_t           *replay_sort; uint32_t replay_sort_capacity;
    uint8_t            *replay_chunk; size_t replay_chunk_capacity;
    uint32_t           *selected;    uint32_t selected_capacity;
    mb_archive_owned_file_v1 *journal; uint32_t journal_capacity;
} ps5_prepare_request;

typedef struct {
    uint32_t status;            /* 0 = prepared; else the failing intake
                                 * status (or PS5_PREPARE_E_* below) */
    uint32_t complete;          /* intake 'complete' flag of the last step */
    uint32_t selected_count;
    uint64_t decoded_bytes;     /* intake-reported materialized bytes */
    uint8_t  inventory_sha256[32];
    uint8_t  target_token[32];  /* token the intake bound the quarantine to */
    char     manifest_name[40]; /* intake journal manifest ordinal name */
    int      quarantine_fd;     /* still open on success; caller closes */
    uint64_t quarantine_device, quarantine_inode;
    uint32_t journal_retained;  /* 1 when a journal exists and must NOT be
                                 * erased (intake rule: survives failure) */
} ps5_prepare_result;

enum {
    PS5_PREPARE_E_ARGUMENT = 100,  /* request/api/result invalid           */
    PS5_PREPARE_E_BUFFERS  = 101,  /* scratch capacities below intake rules */
    PS5_PREPARE_E_QUARANTINE = 102,/* quarantine dir missing/unusable      */
    PS5_PREPARE_E_SCAN     = 103,  /* scan returned nonzero status         */
    PS5_PREPARE_E_SELECT   = 104,  /* selection exceeded journal capacity  */
    PS5_PREPARE_E_MATERIALIZE = 105,/* materialize returned nonzero status */
    PS5_PREPARE_E_CANCELLED = 106  /* caller cancel fired before a step    */
};

/* Drive the whole prepare path. On failure the result says which step and
 * carries the intake status; the journal (if any was created) is never
 * erased by this bridge. On success the installer records the result and
 * the quarantine directory is the "prepared" artifact awaiting install. */
int ps5_prepare_archive(const ps5_intake_api *api,
                        const ps5_prepare_request *req,
                        ps5_prepare_result *out);

/* Default selector exported for tests and console reuse: regular files only. */
int ps5_prepare_select_regular(void *ctx, const mb_archive_row_v1 *row,
                               const char *name);

#ifdef __cplusplus
}
#endif

#endif /* PS5_PREPARE_BRIDGE_H */
