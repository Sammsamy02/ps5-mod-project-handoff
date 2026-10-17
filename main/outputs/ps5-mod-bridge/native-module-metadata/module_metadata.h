#ifndef MB_MODULE_METADATA_H
#define MB_MODULE_METADATA_H

#include "prosperodev_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_MODULE_METADATA_LIMIT 256u

typedef enum mb_module_metadata_status {
    MB_MODULE_METADATA_OK = 0,
    MB_MODULE_METADATA_INVALID_ARGUMENT,
    MB_MODULE_METADATA_LIST_FAILED,
    MB_MODULE_METADATA_INFO_FAILED,
    MB_MODULE_METADATA_COUNT_OUT_OF_RANGE,
    MB_MODULE_METADATA_DUPLICATE_HANDLE,
    MB_MODULE_METADATA_INFO_SIZE_MISMATCH,
    MB_MODULE_METADATA_INVALID_NAME,
    MB_MODULE_METADATA_SEGMENT_COUNT_OUT_OF_RANGE,
    MB_MODULE_METADATA_INVALID_SEGMENT,
    MB_MODULE_METADATA_ADDRESS_OVERFLOW,
    MB_MODULE_METADATA_LIST_ABI_UNVERIFIED
} mb_module_metadata_status;

typedef struct mb_module_metadata_result {
    mb_module_metadata_status status;
    int provider_code; /* Exact nonzero provider return, otherwise zero. */
    size_t module_index; /* SIZE_MAX when not an enumeration item. */
    uint32_t handle; /* Meaningful for read-one and indexed errors. */
    uint32_t segment_index; /* UINT32_MAX when not a segment error. */
} mb_module_metadata_result;

typedef struct mb_module_metadata_segment {
    uint64_t reported_address; /* Integer metadata; never dereferenced. */
    uint32_t reported_size;
    int32_t reported_protection; /* Uninterpreted, not read/execute authority. */
} mb_module_metadata_segment;

typedef struct mb_module_metadata_record {
    uint32_t handle;
    char name[256]; /* Nonempty NUL-terminated bytes, no encoding guarantee. */
    uint8_t fingerprint[20]; /* Opaque reported bytes, not verified identity. */
    uint32_t segment_count;
    mb_module_metadata_segment segments[4];
} mb_module_metadata_record;

typedef struct mb_module_metadata_snapshot {
    size_t count;
    mb_module_metadata_record records[MB_MODULE_METADATA_LIMIT];
} mb_module_metadata_snapshot;

/* Trusted synchronous callbacks must respect buffer bounds and retain no pointers.
 * list capacity and count are ELEMENTS for this adapter API. This does not assert
 * the unit of sceKernelGetModuleList's undocumented size parameter.
 * Return zero on success; get_info receives a zeroed structure with size=352.
 * Provider, context and output must not alias; caller exclusively owns output.
 */
typedef struct mb_module_metadata_provider {
    void *context;
    int (*get_list)(void *, mb_prospero_module_handle *, size_t capacity, size_t *count);
    int (*get_info)(void *, mb_prospero_module_handle, mb_prospero_module_info *);
} mb_module_metadata_provider;

/* No allocation, retention, retries or target-memory access. Output is fully
 * cleared on failure. A success is a sequential metadata observation, not an
 * atomic loader snapshot, mapping ownership, lifetime lease, or read permission.
 */
mb_module_metadata_result mb_module_metadata_capture(
    const mb_module_metadata_provider *, mb_module_metadata_snapshot *);
mb_module_metadata_result mb_module_metadata_read(
    const mb_module_metadata_provider *, uint32_t handle, mb_module_metadata_record *);

/* Separate native TU: direct pinned info API only. Enumeration currently returns
 * LIST_ABI_UNVERIFIED, clears output and makes no native calls. No entry point.
 */
mb_module_metadata_result mb_module_metadata_native_read(uint32_t handle, mb_module_metadata_record *);
mb_module_metadata_result mb_module_metadata_native_capture(mb_module_metadata_snapshot *);

#ifdef __cplusplus
}
#endif
#endif
