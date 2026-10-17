#ifndef MODBRIDGE_ZEN_IMPORT_MAP_H
#define MODBRIDGE_ZEN_IMPORT_MAP_H

#include "summary.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_ZEN_IMPORT_MAP_ABI_VERSION 1u
#define MB_ZEN_IMPORT_MAP_MAX_ENTRIES 8192u

typedef enum mb_zen_import_map_status {
    MB_ZEN_IMPORT_MAP_OK = 0,
    MB_ZEN_IMPORT_MAP_BAD_ARGUMENT,
    MB_ZEN_IMPORT_MAP_TRUNCATED,
    MB_ZEN_IMPORT_MAP_MALFORMED,
    MB_ZEN_IMPORT_MAP_RESOURCE_LIMIT,
    MB_ZEN_IMPORT_MAP_OUT_OF_RANGE
} mb_zen_import_map_status;

typedef enum mb_zen_import_map_reason {
    MB_ZEN_IMPORT_MAP_REASON_NONE = 0,
    MB_ZEN_IMPORT_MAP_REASON_ABI,
    MB_ZEN_IMPORT_MAP_REASON_ALIAS,
    MB_ZEN_IMPORT_MAP_REASON_CAPTURE,
    MB_ZEN_IMPORT_MAP_REASON_SUMMARY,
    MB_ZEN_IMPORT_MAP_REASON_OFFSETS,
    MB_ZEN_IMPORT_MAP_REASON_STRIDE,
    MB_ZEN_IMPORT_MAP_REASON_PACKAGE_HASH_INDEX,
    MB_ZEN_IMPORT_MAP_REASON_OUTPUT_CAPACITY
} mb_zen_import_map_reason;

typedef enum mb_zen_import_kind {
    MB_ZEN_IMPORT_EXPORT = 0,
    MB_ZEN_IMPORT_SCRIPT = 1,
    MB_ZEN_IMPORT_PACKAGE = 2,
    MB_ZEN_IMPORT_NULL = 3
} mb_zen_import_kind;

typedef struct mb_zen_import_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_imports;
    uint32_t reserved;
} mb_zen_import_limits_v1;

typedef struct mb_zen_import_ref_v1 {
    uint64_t raw;
    uint64_t value;
    uint32_t kind;
    uint32_t is_null; /* Exact all-ones sentinel, independent of top-bit type. */
    uint32_t export_index; /* Meaningful only for MB_ZEN_IMPORT_EXPORT. */
    uint32_t imported_package_index; /* Meaningful only for PACKAGE. */
    uint32_t public_export_hash_index; /* Meaningful only for PACKAGE. */
    uint32_t reserved;
} mb_zen_import_ref_v1;

typedef struct mb_zen_import_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t status;
    uint32_t reason;
    uint32_t complete;
    uint32_t reference_count;
    uint32_t import_map_offset;
    uint32_t import_map_bytes;
    uint32_t public_hash_offset;
    uint32_t public_hash_bytes;
    uint32_t public_hash_count;
    uint32_t kind_counts[4];
} mb_zen_import_result_v1;

void mb_zen_import_limits_v1_default(mb_zen_import_limits_v1 *limits);

/* Pure bounded inspection of the 8-byte FPackageObjectIndex array between
 * ImportedPublicExportHashesOffset and ExportMapOffset. The supplied summary
 * must be a successful output of mb_zen_read_summary_names_v1 over the same
 * immutable header bytes. This function independently binds the three offsets
 * and HeaderSize back to their raw little-endian summary fields before reading.
 * PackageImport hash indices are bounded against the preceding u64 hash table;
 * package indices and ScriptImport values remain opaque external references.
 * No pointers persist, allocation, I/O, object resolution, property parsing,
 * platform/cook/runtime/install conclusion, or partial prefix is published.
 * Admitted errors clear result and all supplied reference records. Rejected
 * ABI/pointer/known-alias preflight leaves all bytes untouched.
 */
mb_zen_import_map_status mb_zen_read_import_map_v1(
    const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_result_v1 *summary,
    const mb_zen_import_limits_v1 *limits,
    mb_zen_import_result_v1 *result,
    mb_zen_import_ref_v1 *references,
    uint32_t references_capacity);

#ifdef __cplusplus
}
#endif
#endif
