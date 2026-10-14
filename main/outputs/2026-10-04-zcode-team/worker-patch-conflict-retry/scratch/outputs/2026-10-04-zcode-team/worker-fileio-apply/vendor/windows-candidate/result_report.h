#ifndef MB_OWNED_RESULT_REPORT_H
#define MB_OWNED_RESULT_REPORT_H
#include "owned_runtime.h"
#include "../windows-execution/pe_ps5_memory.h"
#include <stddef.h>
#include <stdint.h>
#define MB_OWNED_RESULT_CAPACITY 2048u
#define MB_OWNED_WRITE_ATTEMPTS 32u
enum mb_owned_result_phase {
    MB_OWNED_RESULT_STARTING=1, MB_OWNED_RESULT_INITIALIZED,
    MB_OWNED_RESULT_RUN_RETURNED, MB_OWNED_RESULT_CLEANUP_PENDING,
    MB_OWNED_RESULT_RETIRED
};
typedef struct mb_owned_delivery {
    int32_t first_file_error, last_file_error, first_stdout_error, last_stdout_error;
    uint32_t file_records, stdout_records;
} mb_owned_delivery;
typedef struct mb_owned_result_snapshot {
    const char *build_id; /* Exactly 32 lowercase hexadecimal characters. */
    uint32_t sequence, phase, memory_status;
    mb_owned_dll_report runtime;
    mb_pe_ps5_memory_diagnostics memory;
    mb_owned_delivery delivery;
} mb_owned_result_snapshot;
/* Returns bytes including the terminating newline, excluding the NUL; zero
 * refuses invalid input or insufficient capacity. No partial record is valid. */
size_t mb_owned_result_format(char *,size_t,const mb_owned_result_snapshot *);
typedef ptrdiff_t (*mb_owned_record_writer)(void *,const void *,size_t,int32_t *);
typedef struct mb_owned_write_result {size_t bytes;uint32_t calls;int32_t error;} mb_owned_write_result;
/* Externally serialized; writer returns promptly with POSIX errno via error.
 * At most 32 calls, including EINTR and positive short writes. This bounds
 * retries, not the duration of an individual native I/O syscall. */
int mb_owned_write_record(mb_owned_record_writer,void *,const void *,size_t,mb_owned_write_result *);
#endif
