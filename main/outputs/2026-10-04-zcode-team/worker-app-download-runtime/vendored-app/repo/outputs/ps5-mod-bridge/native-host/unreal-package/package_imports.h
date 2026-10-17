#ifndef MODBRIDGE_ZEN_PACKAGE_IMPORTS_H
#define MODBRIDGE_ZEN_PACKAGE_IMPORTS_H

#include "import_map.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_ZEN_PACKAGE_IMPORTS_ABI_VERSION 1u
#define MB_ZEN_PACKAGE_IMPORTS_MAX_NAMES MB_ZEN_SUMMARY_MAX_NAMES
#define MB_ZEN_PACKAGE_IMPORTS_MAX_NAME_BYTES MB_ZEN_SUMMARY_MAX_NAME_BYTES
#define MB_ZEN_PACKAGE_IMPORTS_MAX_BINDINGS MB_ZEN_IMPORT_MAP_MAX_ENTRIES
#define MB_ZEN_PACKAGE_NAME_HASH_VERSION UINT64_C(0xC1640000)

typedef enum mb_zen_package_imports_status {
    MB_ZEN_PACKAGE_IMPORTS_OK = 0,
    MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT,
    MB_ZEN_PACKAGE_IMPORTS_TRUNCATED,
    MB_ZEN_PACKAGE_IMPORTS_MALFORMED,
    MB_ZEN_PACKAGE_IMPORTS_UNSUPPORTED,
    MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT,
    MB_ZEN_PACKAGE_IMPORTS_OUT_OF_RANGE
} mb_zen_package_imports_status;

typedef enum mb_zen_package_imports_reason {
    MB_ZEN_PACKAGE_IMPORTS_REASON_NONE = 0,
    MB_ZEN_PACKAGE_IMPORTS_REASON_ABI,
    MB_ZEN_PACKAGE_IMPORTS_REASON_ALIAS,
    MB_ZEN_PACKAGE_IMPORTS_REASON_CAPTURE,
    MB_ZEN_PACKAGE_IMPORTS_REASON_SUMMARY,
    MB_ZEN_PACKAGE_IMPORTS_REASON_IMPORT_MAP,
    MB_ZEN_PACKAGE_IMPORTS_REASON_OFFSET,
    MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_COUNT,
    MB_ZEN_PACKAGE_IMPORTS_REASON_HASH_VERSION,
    MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_HEADERS,
    MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_BYTES,
    MB_ZEN_PACKAGE_IMPORTS_REASON_NUMBER_ARRAY,
    MB_ZEN_PACKAGE_IMPORTS_REASON_PACKAGE_INDEX,
    MB_ZEN_PACKAGE_IMPORTS_REASON_PUBLIC_HASH_INDEX,
    MB_ZEN_PACKAGE_IMPORTS_REASON_OUTPUT_CAPACITY,
    MB_ZEN_PACKAGE_IMPORTS_REASON_TRAILING_BYTES
} mb_zen_package_imports_reason;

typedef struct mb_zen_package_imports_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_names;
    uint32_t max_name_bytes;
    uint32_t max_bindings;
    uint32_t reserved;
} mb_zen_package_imports_limits_v1;

typedef struct mb_zen_imported_package_name_v1 {
    uint64_t hash_raw;
    uint32_t arena_offset;
    uint32_t byte_length;
    uint32_t code_units;
    uint32_t encoding; /* MB_ZEN_NAME_UTF8 or MB_ZEN_NAME_UTF16LE. */
    int32_t number;    /* Preserved raw signed FName number; no suffix reconstruction. */
    uint32_t reserved;
} mb_zen_imported_package_name_v1;

typedef struct mb_zen_package_import_binding_v1 {
    uint32_t import_reference_index;
    uint32_t imported_package_index; /* Index into the owning name array. */
    uint32_t public_export_hash_index;
    uint32_t reserved;
    uint64_t object_index_raw;
    uint64_t public_export_hash;
} mb_zen_package_import_binding_v1;

typedef struct mb_zen_package_imports_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t status;
    uint32_t reason;
    uint32_t complete;
    uint32_t table_offset;
    uint32_t table_bytes;
    uint32_t name_count;
    uint32_t name_bytes_used;
    uint32_t declared_string_bytes_raw;
    uint32_t package_binding_count;
    uint64_t name_hash_version_raw;
} mb_zen_package_imports_result_v1;

void mb_zen_package_imports_limits_v1_default(mb_zen_package_imports_limits_v1 *limits);

/* Pure bounded parsing of the UE5.3+ imported-package-name tail. The caller
 * supplies the same immutable package header, successful summary and import
 * map outputs, and the complete owning object-index array. Raw offsets and all
 * object-index words are rebound to the header. The table must end exactly at
 * HeaderSize. Names and signed numbers are preserved as metadata; no
 * FPackageId hashing, package-store join, dependency resolution, or runtime,
 * platform, cook, or install conclusion is made. The function allocates and
 * performs I/O nowhere. Admitted failures clear every supplied output byte;
 * rejected ABI/pointer/known-alias preflight leaves outputs untouched.
 */
mb_zen_package_imports_status mb_zen_read_package_imports_v1(
    const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_result_v1 *summary,
    const mb_zen_import_result_v1 *import_map,
    const mb_zen_import_ref_v1 *references,
    uint32_t references_capacity,
    const mb_zen_package_imports_limits_v1 *limits,
    mb_zen_package_imports_result_v1 *result,
    mb_zen_imported_package_name_v1 *names,
    uint32_t names_capacity,
    uint8_t *name_arena,
    uint32_t name_arena_capacity,
    mb_zen_package_import_binding_v1 *bindings,
    uint32_t bindings_capacity);

#ifdef __cplusplus
}
#endif
#endif
