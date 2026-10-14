#define _POSIX_C_SOURCE 200809L
#include "mb_win_clock.h"
#include <time.h>
_Static_assert(sizeof(time_t)==8 && sizeof(long)==8 && sizeof(clockid_t)==4 &&
    sizeof(struct timespec)==16 && offsetof(struct timespec,tv_nsec)==8,
    "Clock provider requires the native 64-bit timespec layout");
#if defined(__PROSPERO__)
_Static_assert(CLOCK_MONOTONIC==4,"Pinned PS5 CLOCK_MONOTONIC changed");
#endif
#if defined(MB_WIN_CLOCK_TEST)
extern int mb_win_clock_test_gettime(clockid_t,struct timespec *);
#define READ_CLOCK mb_win_clock_test_gettime
#else
#define READ_CLOCK clock_gettime
#endif
mb_win_clock_status mb_win_clock_read_counter(int64_t *out)
{
    struct timespec sample;
    int64_t seconds, nanoseconds;
    if(!out) return MB_WIN_CLOCK_INVALID_ARGUMENT;
    if(READ_CLOCK(CLOCK_MONOTONIC,&sample)) return MB_WIN_CLOCK_NATIVE_ERROR;
    seconds=sample.tv_sec; nanoseconds=sample.tv_nsec;
    if(seconds<0 || nanoseconds<0 || nanoseconds>=MB_WIN_CLOCK_FREQUENCY ||
       seconds>INT64_MAX/MB_WIN_CLOCK_FREQUENCY ||
       (seconds==INT64_MAX/MB_WIN_CLOCK_FREQUENCY && nanoseconds>INT64_MAX%MB_WIN_CLOCK_FREQUENCY))
        return MB_WIN_CLOCK_RANGE_ERROR;
    *out=seconds*MB_WIN_CLOCK_FREQUENCY+nanoseconds;
    return MB_WIN_CLOCK_OK;
}
mb_win_clock_status mb_win_clock_read_frequency(int64_t *out)
{
    int64_t current;
    mb_win_clock_status status;
    if(!out) return MB_WIN_CLOCK_INVALID_ARGUMENT;
    status=mb_win_clock_read_counter(&current);
    if(status!=MB_WIN_CLOCK_OK) return status;
    *out=MB_WIN_CLOCK_FREQUENCY;
    return MB_WIN_CLOCK_OK;
}
