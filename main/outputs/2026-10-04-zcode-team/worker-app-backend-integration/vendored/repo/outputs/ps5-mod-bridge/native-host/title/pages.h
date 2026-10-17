#ifndef MODBRIDGE_TITLE_PAGES_H
#define MODBRIDGE_TITLE_PAGES_H

#include "catalog.h"

#define MB_TITLE_PAGES_ABI_VERSION 1u
#define MB_TITLE_PAGES_RECORD_LIMIT 128u
#define MB_TITLE_PAGES_ISSUE_LIMIT 32u
#define MB_TITLE_PAGES_ITEMS_PER_PAGE 8u
#define MB_TITLE_PAGES_PAGE_LIMIT 128u
#define MB_TITLE_PAGES_WIRE_LIMIT 8191u
#define MB_TITLE_PAGES_CURRENT_LIMIT 128u
#define MB_TITLE_PAGES_CAPTURE_LIMIT 524288u
#define MB_TITLE_PAGES_BACKING_LIMIT 1048576u
#define MB_TITLE_PAGES_METADATA_ONLY_SCOPE 1u

typedef enum mb_title_pages_status {
    MB_TITLE_PAGES_OK=0, MB_TITLE_PAGES_BAD_ARGUMENT,
    MB_TITLE_PAGES_BAD_CATALOG, MB_TITLE_PAGES_BAD_TOKEN,
    MB_TITLE_PAGES_RESOURCE_LIMIT, MB_TITLE_PAGES_INTERNAL_ERROR
} mb_title_pages_status;
typedef enum mb_title_pages_reason {
    MB_TITLE_PAGES_REASON_NONE=0, MB_TITLE_PAGES_CATALOG_INVALID,
    MB_TITLE_PAGES_RECORD_INVALID, MB_TITLE_PAGES_DUPLICATE_TITLE,
    MB_TITLE_PAGES_ISSUE_INVALID, MB_TITLE_PAGES_TOKEN_INVALID,
    MB_TITLE_PAGES_TOKEN_DUPLICATE, MB_TITLE_PAGES_CAPTURE_TOO_LARGE,
    MB_TITLE_PAGES_PAGE_TOO_LARGE, MB_TITLE_PAGES_BACKING_TOO_SMALL,
    MB_TITLE_PAGES_INTERNAL_FAILURE
} mb_title_pages_reason;

typedef struct mb_title_pages_view_v1 {
    uint32_t struct_size, abi_version;
    const mb_title_catalog_result_v1 *catalog;
    const mb_title_metadata_result_v1 *records;
    uint32_t records_capacity;
    const uint32_t *issues;
    uint32_t issues_capacity, reserved[2];
} mb_title_pages_view_v1;
typedef struct mb_title_pages_tokens_v1 {
    uint32_t struct_size, abi_version, title_count, cursor_count;
    char generation[33];
    const char (*title_keys)[33];
    const char (*cursors)[33];
    uint32_t reserved[2];
} mb_title_pages_tokens_v1;
typedef struct mb_title_pages_descriptor_v1 {
    uint32_t byte_offset, byte_count, item_offset, item_count;
    /* Empty on page zero/measurement. Page N>0 is addressed by this token. */
    char cursor[33];
} mb_title_pages_descriptor_v1;
typedef struct mb_title_pages_plan_v1 {
    uint32_t struct_size, abi_version, status, reason;
    uint32_t valid, observed_complete, page_count, record_count, issue_count;
    uint32_t source_scope, installation_supported, build_fingerprint_available;
    uint64_t logical_capture_bytes, required_backing_bytes;
    uint32_t current_offset, current_bytes, reserved[2];
    mb_title_pages_descriptor_v1 pages[MB_TITLE_PAGES_PAGE_LIMIT];
} mb_title_pages_plan_v1;
typedef struct mb_title_pages_cache_v1 {
    uint32_t struct_size, abi_version, status, reason, prepared;
    char snapshot_id[33];
    mb_title_pages_plan_v1 plan;
    uint32_t installation_supported, build_fingerprint_available, reserved[2];
} mb_title_pages_cache_v1;

#ifdef __cplusplus
#define MB_TITLE_PAGES_NOEXCEPT noexcept
extern "C" {
#else
#define MB_TITLE_PAGES_NOEXCEPT
#endif

/* Pure bounded catalog-to-wire transport. No heap/JSON DOM, RNG, source reads,
 * HTTP/session/provider registration, retained pointer or title-ID branches.
 * The view borrows owning catalog/record/issue arrays only during the call;
 * capacities describe valid full backing, not just initialized elements.
 * Exact view/token ABI/reserved/cap/span checks and known overlaps are no-write
 * preflight. Owning catalog/record invariants are operational validation.
 * Caller must discard any previous result when the function returns non-OK.
 * After successful preflight, every failure replaces the whole result/cache
 * with safe constant status/reason and clears the full disjoint byte capacity.
 *
 * Measurement uses fixed-width placeholders, publishing geometry but no IDs.
 * Prepare repeats measurement internally, requires exact used token counts
 * and distinct canonical Hex32 generation/title/cursor tokens, and publishes
 * prepared=1 only after all exact wire pages + current response are complete.
 * For nonobserved catalog status, tokens must be NULL: one terminal page hides
 * every row/ID, and no current response is emitted. Complete empty observed
 * catalogs still require a generation and a current response.
 *
 * Cache owns descriptors/tokens, with no pointer into the input. Caller owns
 * immutable backing until discarding/replacing the entire cache; a page is
 * backing+byte_offset for exactly byte_count bytes, without a NUL terminator.
 * Whole logical capture<=512KiB, <=128 rows/pages, <=8 rows/page, <=8191 exact
 * compact sorted ASCII bytes/page; total cache+current<=1MiB. Text projection
 * matches current provider limits: optional empty text becomes unavailable;
 * representable UTF-8 text violating trim/control rules becomes error.
 * Malformed UTF-8/owning tails, unsafe flags or ASCII-case-fold duplicate IDs
 * invalidate all. Valid title-ID spelling is preserved exactly in output.
 * Captured metadata SHA256 is never a build fingerprint; build_fingerprint is
 * always null and all build/installation authority flags remain zero.
 */
mb_title_pages_status mb_title_pages_measure_v1(
    const mb_title_pages_view_v1 *view,
    mb_title_pages_plan_v1 *plan) MB_TITLE_PAGES_NOEXCEPT;
mb_title_pages_status mb_title_pages_prepare_v1(
    const mb_title_pages_view_v1 *view,
    const mb_title_pages_tokens_v1 *tokens,
    uint8_t *backing,size_t backing_bytes,
    mb_title_pages_cache_v1 *cache) MB_TITLE_PAGES_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_TITLE_PAGES_NOEXCEPT
#endif
