#ifndef MB_OWNED_SDK_DRIVER_H
#define MB_OWNED_SDK_DRIVER_H
#include "result_sink.h"
int mb_owned_ps5_start(void);
int mb_owned_ps5_run(void);
int mb_owned_ps5_cleanup(void);
uint32_t mb_owned_ps5_memory_status(void);
void mb_owned_ps5_result(mb_owned_dll_report *);
void mb_owned_ps5_delivery(mb_owned_delivery *);
/* One run on the calling thread; profile path stays valid until this returns.
 * Persistent cleanup failure waits with a one-second sleep and retains state.
 * Emits the first pending snapshot before waiting, then only a retired record.
 * Reporting failure does not change DLL first failure or skip cleanup. */
int mb_owned_ps5_execute(const char *report_path,int stdout_fd);
#endif
