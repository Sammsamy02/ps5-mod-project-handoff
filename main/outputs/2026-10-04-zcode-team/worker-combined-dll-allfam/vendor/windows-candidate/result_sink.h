#ifndef MB_OWNED_RESULT_SINK_H
#define MB_OWNED_RESULT_SINK_H
#include "result_report.h"
#define MB_OWNED_MAX_RECORDS 8u
typedef struct mb_owned_result_sink {
    const char *path; /* Borrowed stable profile string; empty disables file. */
    int stdout_fd; /* Borrowed; -1 disables. Never closed or reconfigured. */
    uint32_t attempts, file_disabled;
    mb_owned_delivery delivery;
} mb_owned_result_sink;
/* Zero-init with path/stdout_fd, serialize calls. Each file attempt owns one
 * O_APPEND regular-file descriptor and closes it once, including error paths.
 * Close is never retried: an interrupted close may already consume the fd.
 * A close error is surfaced and disables the file; if the OS kept the fd open,
 * process teardown is the fallback. It is never reported as clean delivery.
 * File errors disable subsequent file writes; partial final records may exist.
 * stdout supports regular files and sockets (nonblocking, no SIGPIPE); other
 * types fail with ENOTSUP. There is no descriptor/global flag mutation.
 * At most eight records and 32 writes per destination per record. Syscall
 * duration is OS-dependent; this is a retry bound, not an I/O deadline.
 * A record describes delivery state before itself; stdout additionally sees
 * the same attempt's file outcome. Final stdout failure remains in this owner.
 */
int mb_owned_result_emit(mb_owned_result_sink *,const mb_owned_result_snapshot *);
#endif
