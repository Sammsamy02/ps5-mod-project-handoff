#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct Cap10ExecStats
{
    uint64_t mmap_calls, mprotect_calls, executable_requests, failed_requests, recorded_events, dropped_events;
};
void cap10_exec_reset(void);
void cap10_exec_start(void);
void cap10_exec_stop(void);
struct Cap10ExecStats cap10_exec_stats(void);
void cap10_exec_report(void);
#ifdef __cplusplus
}
#endif
