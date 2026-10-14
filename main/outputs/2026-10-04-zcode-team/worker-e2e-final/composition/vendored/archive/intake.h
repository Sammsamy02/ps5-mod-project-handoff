#ifndef MODBRIDGE_ARCHIVE_INTAKE_H
#define MODBRIDGE_ARCHIVE_INTAKE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MB_ARCHIVE_ABI_V1 1u
#define MB_ARCHIVE_MAX_RAW_BYTES (UINT64_C(8) << 30)
#define MB_ARCHIVE_MAX_MEMBER_BYTES (UINT64_C(8) << 30)
#define MB_ARCHIVE_MAX_EXPANDED_BYTES (UINT64_C(16) << 30)
#define MB_ARCHIVE_MAX_IO_BYTES (UINT64_C(128) << 30)
#define MB_ARCHIVE_MAX_ENTRIES 10000u
#define MB_ARCHIVE_MAX_NAME_BYTES (8u << 20)
#define MB_ARCHIVE_MAX_PATH_BYTES 4096u
#define MB_ARCHIVE_CHUNK_BYTES 65536u
#define MB_ARCHIVE_MAX_OPERATIONS UINT64_C(16777216)
#define MB_ARCHIVE_MAX_PATH_WORK UINT64_C(268435456)
enum mb_archive_status_v1 {
 MB_ARCHIVE_OK=0, MB_ARCHIVE_BAD_ARGUMENT=1, MB_ARCHIVE_LIMIT=2,
 MB_ARCHIVE_SOURCE_CHANGED=3, MB_ARCHIVE_READER_REJECTED=4,
 MB_ARCHIVE_UNSAFE_PATH=5, MB_ARCHIVE_UNSUPPORTED_ENTRY=6,
 MB_ARCHIVE_CANCELLED=7, MB_ARCHIVE_IO_ERROR=8,
 MB_ARCHIVE_IDENTITY_MISMATCH=9, MB_ARCHIVE_CLEANUP_INCOMPLETE=10
};
enum mb_archive_row_kind_v1 { MB_ARCHIVE_REGULAR=1, MB_ARCHIVE_DIRECTORY=2 };
typedef struct mb_archive_source_v1 {
 uint32_t struct_size, abi_version; int fd; uint32_t reserved;
 uint64_t size; uint8_t sha256[32], target_token[32];
} mb_archive_source_v1;
typedef struct mb_archive_limits_v1 {
 uint64_t raw_bytes, member_bytes, expanded_bytes;
 uint32_t entries, name_bytes;
} mb_archive_limits_v1;
typedef int (*mb_archive_cancel_v1)(void *context);
typedef struct mb_archive_budget_v1 {
 uint64_t io_limit, operation_limit, path_work_limit;
 uint64_t io_used, operations_used, path_work_used;
 /* Absolute CLOCK_MONOTONIC milliseconds; zero disables deadline. */
 uint64_t deadline_ms; mb_archive_cancel_v1 cancelled; void *context;
} mb_archive_budget_v1;
typedef struct mb_archive_row_v1 {
 /* Owning UTF-8 names reported by libarchive, not raw encoded header bytes.
  * A host reader may normalize Unicode (Darwin reports NFD). No name is
  * ever used as an on-disk destination; original/logical names are metadata. */
 uint32_t kind, original_offset, original_length, path_offset, path_length;
 uint32_t reserved; uint64_t size; uint8_t sha256[32];
} mb_archive_row_v1;
typedef struct mb_archive_buffers_v1 {
 mb_archive_row_v1 *rows; uint32_t row_capacity;
 char *names; uint32_t name_capacity;
 uint32_t *sort_indices; uint32_t sort_capacity;
 uint8_t *chunk; size_t chunk_capacity;
} mb_archive_buffers_v1;
typedef struct mb_archive_inventory_v1 {
 uint32_t struct_size, abi_version, status, complete;
 uint32_t row_count, name_bytes, format_code, filter_count;
 uint64_t expanded_bytes, source_size, source_device, source_inode;
 uint8_t source_sha256[32], target_token[32], inventory_sha256[32];
 uint32_t installation_supported, reserved;
} mb_archive_inventory_v1;
/* The journal is separate from the positive result: never erase it on failure.
 * Names are library-generated ordinal/control names, never archive paths.
 * A surviving intent file records every planned name for process-loss recovery.
 */
enum mb_archive_owned_state_v1 { MB_ARCHIVE_PLANNED=1, MB_ARCHIVE_CREATED=2,
 MB_ARCHIVE_REMOVED=3, MB_ARCHIVE_RETAINED=4 };
typedef struct mb_archive_owned_file_v1 {
 char name[40]; uint32_t state, row_index;
 uint64_t device, inode, expected_size, written_bytes;
 uint8_t sha256[32];
} mb_archive_owned_file_v1;
typedef struct mb_archive_materialized_v1 {
 uint32_t struct_size, abi_version, status, complete;
 uint32_t selected_count, journal_count, cleanup_complete, installation_supported;
 uint64_t decoded_bytes; uint8_t inventory_sha256[32], target_token[32];
 char manifest_name[40];
} mb_archive_materialized_v1;
typedef struct mb_archive_materialize_request_v1 {
 const mb_archive_source_v1 *source;
 const mb_archive_inventory_v1 *inventory;
 const mb_archive_row_v1 *rows; uint32_t row_capacity;
 const char *names; uint32_t name_capacity;
 const uint32_t *selected; uint32_t selected_count;
 uint8_t target_token[32];
 int quarantine_fd; uint32_t reserved;
 uint64_t quarantine_device, quarantine_inode;
 /* Fresh all-zero journal, capacity >= selected_count+3. After any admitted
  * operation retain/export it; it cannot be reused or implicitly erased. */
 mb_archive_owned_file_v1 *journal; uint32_t journal_capacity;
 /* Independent reread owning buffers: do not alias the supplied inventory. */
 mb_archive_buffers_v1 replay;
} mb_archive_materialize_request_v1;
void mb_archive_limits_default_v1(mb_archive_limits_v1 *limits);
void mb_archive_budget_default_v1(mb_archive_budget_v1 *budget);
/* Callers must inspect the return code. Invalid pointers/caps/overlapping
 * spans are rejected before output admission and leave caller storage alone.
 * Admitted failures clear all inventory/replay positive bytes. Caller source
 * and quarantine descriptors remain open. Source immutability and private
 * quarantine exclusivity are caller obligations, not authenticity claims.
 * Reader warnings, including unavailable locale/path conversion, fail closed.
 * There are no hard libarchive allocation/CPU or target execution guarantees. */
int mb_archive_scan_v1(const mb_archive_source_v1 *source,
 const mb_archive_limits_v1 *limits, mb_archive_budget_v1 *budget,
 const mb_archive_buffers_v1 *buffers, mb_archive_inventory_v1 *result);
int mb_archive_materialize_v1(const mb_archive_materialize_request_v1 *request,
 const mb_archive_limits_v1 *limits, mb_archive_budget_v1 *budget,
 mb_archive_materialized_v1 *result);
#ifdef __cplusplus
}
#endif
#endif
