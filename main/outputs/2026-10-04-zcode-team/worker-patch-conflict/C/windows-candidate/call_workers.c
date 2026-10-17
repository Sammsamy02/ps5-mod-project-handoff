#include "call_workers.h"
#if !defined(__x86_64__)
#error "Owned worker export calls require AMD64"
#endif
typedef int32_t (__attribute__((ms_abi)) *worker_export_fn)(uint64_t *);
int32_t mb_owned_call_worker(void *address, uint64_t results[4])
{
    return ((worker_export_fn)address)(results);
}
