/* Shim unit: the SAME source/probe/observer.c compiled with pread shimmed
   to always fail (EIO), forcing observer_read_tail through the bounded
   read()-drain fallback. All public symbols renamed so this links alongside
   the normal observer object in one test binary. */
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

static ssize_t test_failing_pread(int fd, void *buf, size_t count, off_t offset)
{
    (void)fd; (void)buf; (void)count; (void)offset;
    errno = EIO;
    return -1;
}

#define pread test_failing_pread
#define observer_stat_path obs_drain_stat_path
#define observer_check_exists obs_drain_check_exists
#define observer_classify obs_drain_classify
#define observer_read_tail obs_drain_read_tail
#define observer_last_line obs_drain_last_line
#include "../source/probe/observer.c"
