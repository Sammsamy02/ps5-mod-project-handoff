#ifndef MB_OWNED_CALL_WORKERS_H
#define MB_OWNED_CALL_WORKERS_H
#include <stdint.h>
/* Exact owned WorkerBegin/WorkerCollect Microsoft x64 prototypes. */
int32_t mb_owned_call_worker(void *address, uint64_t results[4]);
#endif
