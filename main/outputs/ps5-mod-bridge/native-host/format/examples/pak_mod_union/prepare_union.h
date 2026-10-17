#ifndef MB_PAK_UNION_PREPARE_H
#define MB_PAK_UNION_PREPARE_H
#include "pak_stream_writer.h"

#define MB_PAK_UNION_ABI_V1 1u
#define MB_PAK_UNION_MAX_IO_BYTES UINT64_C(2147483648)
#define MB_PAK_UNION_MAX_IO_CALLS 131072u
#define MB_PAK_UNION_MAX_ALLOCATED_BYTES UINT64_C(67108864)
#define MB_PAK_UNION_STAGE_BYTES 48u
#define MB_PAK_UNION_ERROR_BYTES 96u

typedef enum mb_pak_union_status_v1 {
    MB_PAK_UNION_OK = 0,
    MB_PAK_UNION_BAD_ARGUMENT,
    MB_PAK_UNION_FD_REFUSED,
    MB_PAK_UNION_PREPARATION_REFUSED,
    MB_PAK_UNION_SOURCE_CHANGED,
    MB_PAK_UNION_OUTPUT_SIZE_MISMATCH,
    MB_PAK_UNION_STOPPED
} mb_pak_union_status_v1;

typedef struct mb_pak_union_params_v1 {
    uint32_t struct_size, abi_version;
    int base_fd, mod_fd, output_fd;
    uint32_t immutable_inputs, unpublished_output, reserved;
    uint64_t base_bytes, mod_bytes;
} mb_pak_union_params_v1;

typedef struct mb_pak_union_result_v1 {
    uint32_t struct_size, abi_version;
    mb_pak_union_status_v1 status;
    char stage[MB_PAK_UNION_STAGE_BYTES], error[MB_PAK_UNION_ERROR_BYTES];
    uint32_t component_status, component_detail, failure_row;
    uint32_t archive_complete, source_snapshot_stable, output_size_valid;
    uint64_t base_source_bytes, mod_source_bytes, path_hash_seed;
    uint32_t mount_bytes, mod_mount_bytes;
    uint8_t mount[MB_PAK_STREAM_MAX_PATH_BYTES + 1u];
    uint8_t mod_mount[MB_PAK_STREAM_MAX_PATH_BYTES + 1u];
    uint32_t base_file_count, mod_file_count, input_file_count, final_file_count;
    uint32_t replaced, added, base_metadata_complete, base_reference_graph_complete, mod_metadata_complete;
    uint32_t chosen_payloads_sha1_checked, chosen_base_payloads_sha1_checked, chosen_mod_payloads_sha1_checked;
    uint64_t payload_bytes, archive_bytes, observed_output_bytes;
    uint64_t base_graph_steps, base_graph_comparison_bytes, path_comparison_bytes;
    uint64_t requested_job_io_bytes, allocation_peak_bytes, allocation_live_after_cleanup;
    uint32_t job_io_calls, allocation_calls, release_calls;
    mb_pak_stream_result_v1 writer;
} mb_pak_union_result_v1;

/* Ordinary host preparation from BORROWED FDs. All descriptors remain open
 * and positional I/O preserves their cursors. Inputs must be distinct regular
 * read-only FDs matching declared positive lengths <=512MiB. Output must be
 * a distinct empty regular inode, writable, nonappend, exclusively owned and
 * unpublished. Caller asserts immutable inputs for the job; size/inode/mtime/
 * ctime observations detect ordinary changes but do not seal external writers.
 * Source names/mount/seed are derived, never destinations or profile inputs.
 *
 * No open, close, filename operation, fsync, stdout, exit, subprocess, network,
 * or publication. All owned allocations are released on every admitted exit.
 * archive_complete means preparation only, not durability/install/runtime.
 * Failure may leave partial output; caller must keep it unpublished. Known
 * params/result aliases and invalid arguments return BAD_ARGUMENT without any
 * output/result mutation. Once preflight storage/arguments pass, an owning
 * initialized result records first failure stage/cause and resource metrics.
 * Caller-owned backing and result remain valid/disjoint for the whole call. */
mb_pak_union_status_v1 mb_pak_union_prepare_v1(
    const mb_pak_union_params_v1 *params, mb_pak_union_result_v1 *result);
/* Same ownership/strict v1 layouts, with one caller-owned cooperative gate.
 * NULL/NULL is ungated; NULL gate/non-NULL context is BAD_ARGUMENT without
 * mutation. A stop is latched through reads/writes, graph and bounded CPU
 * work; all allocations are still released. Final artifact completion is
 * gated after snapshots and owned cleanup. Partial output remains private.
 * No clock, fsync, descriptor close or publication is added. */
mb_pak_union_status_v1 mb_pak_union_prepare_gated_v1(
    const mb_pak_union_params_v1 *params, mb_pak_union_result_v1 *result,
    mb_pak_prepare_gate_v1 gate, void *gate_context);
#endif
