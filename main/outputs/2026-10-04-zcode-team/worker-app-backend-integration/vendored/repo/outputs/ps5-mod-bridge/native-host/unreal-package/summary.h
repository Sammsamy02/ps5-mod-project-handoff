#ifndef MODBRIDGE_ZEN_PACKAGE_SUMMARY_H
#define MODBRIDGE_ZEN_PACKAGE_SUMMARY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_ZEN_SUMMARY_ABI_VERSION 1u
#define MB_ZEN_SUMMARY_MAX_HEADER_BYTES 262144u
#define MB_ZEN_SUMMARY_MAX_NAMES 4096u
#define MB_ZEN_SUMMARY_MAX_NAME_BYTES 262144u

typedef enum mb_zen_summary_status {
    MB_ZEN_SUMMARY_OK = 0,
    MB_ZEN_SUMMARY_BAD_ARGUMENT,
    MB_ZEN_SUMMARY_TRUNCATED,
    MB_ZEN_SUMMARY_MALFORMED,
    MB_ZEN_SUMMARY_UNSUPPORTED,
    MB_ZEN_SUMMARY_RESOURCE_LIMIT,
    MB_ZEN_SUMMARY_OUT_OF_RANGE
} mb_zen_summary_status;

typedef enum mb_zen_summary_reason {
    MB_ZEN_SUMMARY_REASON_NONE = 0,
    MB_ZEN_SUMMARY_REASON_ABI,
    MB_ZEN_SUMMARY_REASON_ALIAS,
    MB_ZEN_SUMMARY_REASON_PROFILE,
    MB_ZEN_SUMMARY_REASON_CAPTURE,
    MB_ZEN_SUMMARY_REASON_VERSIONING,
    MB_ZEN_SUMMARY_REASON_HEADER_SIZE,
    MB_ZEN_SUMMARY_REASON_OFFSET,
    MB_ZEN_SUMMARY_REASON_NAME_COUNT,
    MB_ZEN_SUMMARY_REASON_NAME_MAP,
    MB_ZEN_SUMMARY_REASON_NAME_TYPE,
    MB_ZEN_SUMMARY_REASON_NAME_INDEX,
    MB_ZEN_SUMMARY_REASON_OUTPUT_CAPACITY
} mb_zen_summary_reason;

typedef enum mb_zen_summary_layout {
    MB_ZEN_LAYOUT_UE5_3_PLUS = 1
} mb_zen_summary_layout;

typedef enum mb_zen_name_encoding {
    MB_ZEN_NAME_UTF8 = 1,
    MB_ZEN_NAME_UTF16LE = 2
} mb_zen_name_encoding;

typedef enum mb_zen_mapped_name_type {
    MB_ZEN_MAPPED_PACKAGE = 0,
    MB_ZEN_MAPPED_CONTAINER = 1,
    MB_ZEN_MAPPED_GLOBAL = 2
} mb_zen_mapped_name_type;

typedef struct mb_zen_summary_profile_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t summary_layout;
    uint32_t verse_cells_present;
    uint32_t reserved;
} mb_zen_summary_profile_v1;

typedef struct mb_zen_summary_view_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    const uint8_t *header_bytes; /* Starts at package logical offset zero. */
    size_t captured_bytes;
    uint64_t logical_package_bytes;
} mb_zen_summary_view_v1;

typedef struct mb_zen_summary_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_header_bytes;
    uint32_t max_names;
    uint32_t max_name_bytes;
    uint32_t reserved;
} mb_zen_summary_limits_v1;

typedef struct mb_zen_name_v1 {
    uint64_t hash_raw;
    uint32_t arena_offset;
    uint32_t byte_length;
    uint32_t code_units;
    uint32_t encoding;
} mb_zen_name_v1;

typedef struct mb_zen_summary_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t status;
    uint32_t reason;
    uint32_t summary_complete;
    uint32_t name_map_complete;
    uint32_t versioning_info_flag_raw;
    uint32_t header_size;
    uint32_t mapped_name_type;
    uint32_t mapped_name_index;
    uint32_t mapped_name_extra_index;
    uint32_t mapped_name_resolved;
    uint32_t package_flags_raw;
    uint32_t cooked_header_size;
    uint32_t name_count;
    uint32_t declared_name_string_bytes;
    uint64_t name_hash_version_raw;
    uint32_t name_bytes_used;
    uint32_t cell_offsets_present;
    int32_t cell_import_map_offset;
    int32_t cell_export_map_offset;
    int32_t imported_public_export_hashes_offset;
    int32_t import_map_offset;
    int32_t export_map_offset;
    int32_t export_bundle_entries_offset;
    int32_t dependency_bundle_headers_offset;
    int32_t dependency_bundle_entries_offset;
    int32_t imported_package_names_offset;
} mb_zen_summary_result_v1;

void mb_zen_summary_limits_v1_default(mb_zen_summary_limits_v1 *limits);

/* Pure bounded inspection of one already-decoded UE5.3+ Zen package prefix.
 * The caller supplies trusted layout facts, coherent immutable backing, and
 * caller-owned record/byte outputs. v1 accepts only bHasVersioningInfo==0.
 * The local name-map hash bytes and declared string-byte count are preserved
 * as raw metadata; hashes and their aggregate are not verified. UTF-16 names
 * are copied as UTF-16LE bytes and are not normalized/transcoded. Only the
 * Package mapped-name type resolves into this local map; Container/Global
 * references remain external metadata. No pointers persist and no allocation,
 * I/O, payload decoding, platform, cook, runtime, or install claims are made.
 *
 * Input, profile, limits, result, name records and byte arena must be valid
 * caller backing. All output capacities and the header span are capped by the
 * constants above. Outputs must not alias one another or any input. A rejected
 * ABI/pointer/known-alias preflight leaves every byte untouched. After preflight
 * succeeds, outputs are cleared; a later error publishes only status/reason,
 * with no partial summary/name records/bytes.
 */
mb_zen_summary_status mb_zen_read_summary_names_v1(
    const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_profile_v1 *profile,
    const mb_zen_summary_limits_v1 *limits,
    mb_zen_summary_result_v1 *result,
    mb_zen_name_v1 *names,
    uint32_t names_capacity,
    uint8_t *name_arena,
    uint32_t name_arena_capacity);

#ifdef __cplusplus
}
#endif
#endif
