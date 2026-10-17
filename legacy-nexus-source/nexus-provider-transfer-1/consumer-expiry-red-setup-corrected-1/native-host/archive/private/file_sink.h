#ifndef MB_DOWNLOAD_FILE_SINK_H
#define MB_DOWNLOAD_FILE_SINK_H
#include <stddef.h>
#include <stdint.h>
#include <mbedtls/sha256.h>

#define MB_DOWNLOAD_FILE_SINK_ABI_V1 1u
#define MB_DOWNLOAD_FILE_SINK_MAX_BYTES UINT64_C(1073741824)
#define MB_DOWNLOAD_FILE_SINK_WORK_BYTES 65536u
#define MB_DOWNLOAD_FILE_SINK_MAX_CALLBACKS 65536u
typedef enum mb_download_file_sink_status_v1 {
 MB_DOWNLOAD_FILE_SINK_OK=0, MB_DOWNLOAD_FILE_SINK_BAD_ARGUMENT,
 MB_DOWNLOAD_FILE_SINK_BAD_STATE, MB_DOWNLOAD_FILE_SINK_FD_REFUSED,
 MB_DOWNLOAD_FILE_SINK_LIMIT, MB_DOWNLOAD_FILE_SINK_WRITE_ERROR,
 MB_DOWNLOAD_FILE_SINK_HASH_ERROR, MB_DOWNLOAD_FILE_SINK_TRANSFER_INCOMPLETE,
 MB_DOWNLOAD_FILE_SINK_LENGTH_MISMATCH, MB_DOWNLOAD_FILE_SINK_FILE_CHANGED,
 MB_DOWNLOAD_FILE_SINK_READ_ERROR, MB_DOWNLOAD_FILE_SINK_READBACK_MISMATCH,
 MB_DOWNLOAD_FILE_SINK_ABORTED
} mb_download_file_sink_status_v1;
typedef enum mb_download_file_sink_phase_v1 {
 MB_DOWNLOAD_FILE_SINK_UNINITIALIZED=0, MB_DOWNLOAD_FILE_SINK_ACTIVE,
 MB_DOWNLOAD_FILE_SINK_FAILED, MB_DOWNLOAD_FILE_SINK_COMPLETE
} mb_download_file_sink_phase_v1;
typedef int (*mb_download_sink_read_at_fn)(void *,int,uint64_t,uint8_t *,uint32_t,uint32_t *);
typedef int (*mb_download_sink_write_at_fn)(void *,int,uint64_t,const uint8_t *,uint32_t,uint32_t *);
typedef struct mb_download_file_sink_params_v1 {
 uint32_t struct_size,abi_version;
 int fd;
 uint32_t reserved;
 uint64_t cap_bytes;
 void *io_context;
 mb_download_sink_read_at_fn read_at;
 mb_download_sink_write_at_fn write_at;
} mb_download_file_sink_params_v1;
typedef struct mb_download_file_sink_result_v1 {
 uint32_t struct_size,abi_version;
 mb_download_file_sink_status_v1 status;
 uint32_t complete,readback_verified;
 uint64_t bytes_received,bytes_written;
 uint8_t sha256[32];
 uint32_t feed_calls,write_calls,read_calls;
} mb_download_file_sink_result_v1;
typedef struct mb_download_file_sink_v1 {
 mb_download_file_sink_phase_v1 phase;
 mb_download_file_sink_status_v1 status;
 mb_download_file_sink_params_v1 params;
 uint64_t device,inode,bytes_received,bytes_written;
 uint32_t sha_active,feed_calls,write_calls,read_calls;
 uint8_t sha256[32];
 mbedtls_sha256_context sha;
 uint8_t scratch[MB_DOWNLOAD_FILE_SINK_WORK_BYTES];
} mb_download_file_sink_v1;

/* Independent ordinary-byte-stream file sink. This module implements no HTTP,
 * network, TLS, URL/credential, archive, pathname or installation policy.
 * Caller zero-initializes state, supplies immutable/disjoint params/feed data
 * and serialized exact positional I/O callbacks, and exclusively owns a fresh
 * unpublished O_RDWR regular nonappend FD for the whole lifecycle. FDs remain
 * borrowed/open and cursor positions must not change. Start, success or failure
 * states are single-use; finished/failed states are terminal. Partial bytes
 * remain caller-owned/unpublished on any failure. No body heap/allocation.
 * Caller admits response/body data under its own policy and MUST sync FD before
 * finish. transport_complete=1 may be passed only after the actual producing
 * operation completes successfully; optional expected_length comes from that
 * operation's framing, never a per-mod golden fact. Complete means received
 * bytes match the file after readback; it is not publication/install authority.
 * Active hash contexts are freed on success, admitted operational failure,
 * abort and dispose. Bad arguments/known aliases return before mutation or
 * file callbacks, leaving state/result untouched. If that state was active,
 * caller MUST explicitly dispose it after BAD_ARGUMENT to free its live hash
 * context and abandon the unpublished partial. Caller checks return first.
 * Explicit dispose is safe after any attempted operation. */
int mb_download_file_sink_start_v1(mb_download_file_sink_v1 *,const mb_download_file_sink_params_v1 *);
int mb_download_file_sink_feed_v1(mb_download_file_sink_v1 *,const uint8_t *,size_t);
int mb_download_file_sink_finish_v1(mb_download_file_sink_v1 *,int transport_complete,
                                  int has_expected_length,uint64_t expected_length,
                                  mb_download_file_sink_result_v1 *);
int mb_download_file_sink_abort_v1(mb_download_file_sink_v1 *);
void mb_download_file_sink_dispose_v1(mb_download_file_sink_v1 *);
#endif
