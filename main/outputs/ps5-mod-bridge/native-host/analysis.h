#ifndef MODBRIDGE_ARCHIVE_ANALYSIS_H
#define MODBRIDGE_ARCHIVE_ANALYSIS_H
#include <stddef.h>
#include <stdint.h>

#define MB_ARCHIVE_ANALYSIS_ABI_V1 1u
#define MB_ARCHIVE_ANALYSIS_OUTPUT_LIMIT 8192u
#define MB_ARCHIVE_ANALYSIS_SOURCE_LIMIT UINT64_C(1073741824)
#define MB_ARCHIVE_ANALYSIS_TIMEOUT_LIMIT_MS 300000u
#define MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT 2000u
#define MB_ARCHIVE_ANALYSIS_NAME_LIMIT 1048576u

typedef enum mb_archive_analysis_context_v1 {
    MB_ARCHIVE_ANALYSIS_DESKTOP_PREPARATION = 0,
    MB_ARCHIVE_ANALYSIS_PS5_NATIVE = 1
} mb_archive_analysis_context_v1;
typedef enum mb_archive_analysis_status_v1 {
    MB_ARCHIVE_ANALYSIS_OK = 0,
    MB_ARCHIVE_ANALYSIS_BAD_ARGUMENT = 1,
    MB_ARCHIVE_ANALYSIS_SOURCE_REFUSED = 2,
    MB_ARCHIVE_ANALYSIS_SCAN_FAILED = 3,
    MB_ARCHIVE_ANALYSIS_GROUP_FAILED = 4,
    MB_ARCHIVE_ANALYSIS_OUTPUT_TOO_SMALL = 5,
    MB_ARCHIVE_ANALYSIS_TIMEOUT = 6,
    MB_ARCHIVE_ANALYSIS_CANCELLED = 7,
    MB_ARCHIVE_ANALYSIS_CLOCK_FAILED = 8,
    MB_ARCHIVE_ANALYSIS_INTERNAL_ERROR = 9
} mb_archive_analysis_status_v1;
typedef int (*mb_archive_analysis_cancel_fn_v1)(void *);
typedef struct mb_archive_analysis_params_v1 {
    uint32_t struct_size, abi_version;
    int fd;
    mb_archive_analysis_context_v1 execution_context;
    uint64_t expected_device, expected_inode, expected_bytes;
    uint8_t expected_sha256[32];
    uint32_t timeout_ms, reserved;
    void *cancel_context;
    mb_archive_analysis_cancel_fn_v1 cancelled;
    char *report;
    size_t report_capacity;
} mb_archive_analysis_params_v1;
typedef struct mb_archive_analysis_result_v1 {
    uint32_t struct_size, abi_version;
    mb_archive_analysis_status_v1 status;
    uint32_t complete;
    size_t report_bytes;
    uint32_t scan_status, group_status;
    uint32_t installation_supported, runtime_supported;
} mb_archive_analysis_result_v1;

#ifdef __cplusplus
#define MB_ARCHIVE_ANALYSIS_NOEXCEPT noexcept
extern "C" {
#else
#define MB_ARCHIVE_ANALYSIS_NOEXCEPT
#endif
/* Serialized read-only inventory of one completed download. Caller exclusively
 * owns an immutable O_RDONLY regular descriptor matching the observed device,
 * inode, byte count and completed download SHA. It remains borrowed/open; no
 * path, open/write/materialization/network/installation operation exists here.
 * execution_context is the embedding application's compile-target fact, never
 * compatibility or target-game authority. All specified params/result/output
 * spans must be valid, overflow-safe and disjoint. Unsupported ABI/layout or
 * known aliases leave storage intact. Otherwise a failed report is fully wiped,
 * complete=0 and report_bytes=0. Capacity is 1..8192; no query/refetch or prefix.
 * Success publishes one owning NUL-terminated public JSON report; length excludes
 * NUL. No member paths, contents or raw reader errors are returned.
 * Limits: source/member 1GiB, expanded 2GiB, 2000 entries, 1MiB owning names.
 * Real monotonic deadline and exception-safe cancellation cover scan, grouping,
 * report construction and private cleanup. Checks are cooperative; syscalls,
 * libarchive parser allocations and LZMA dictionary/CPU work are not hard-bounded.
 * ZIP requires an ordinary single-disk central directory and an EOF-bound
 * footer. Wrapped ZIP streams, ZIP64 and additional directory record forms are
 * explicitly unsupported in this slice; ZIP extra-field work is capped at 64MiB.
 * A retained-reader EOF interpretation that disagrees with the actual footer's
 * inventory is refused, including footer-like ZIP comment collisions.
 * Counts recognize filename candidates only. Runtime/installation stay false. */
int mb_archive_analyze_v1(const mb_archive_analysis_params_v1 *,
    mb_archive_analysis_result_v1 *) MB_ARCHIVE_ANALYSIS_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef MB_ARCHIVE_ANALYSIS_NOEXCEPT
#endif
