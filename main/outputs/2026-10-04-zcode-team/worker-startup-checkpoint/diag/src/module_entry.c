// module_entry.c — STARTUP CHECKPOINT DIAGNOSTIC (worker-startup-checkpoint,
// team 2026-10-04). Entry contract vendored from worker-v3-startup-diag
// (igd1): the loader runs DT_INIT=module_start; ploader "OK" means
// sceKernelLoadStartModule returned a positive module id (loader accepted the
// file + resolved imports at injection level); module_start's own return goes
// to the kernel's start step and never back to ploader.
//
// CONTROLLER DIRECTIVE (task: worker-startup-checkpoint): module_start does
// EXACTLY
//   1. clock_gettime(CLOCK_REALTIME)
//   2. open /data/plugins/UE4SS.log (O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW,
//      0600) — the composed runtime's expected output path
//   3. write ONE line:
//      "DIAG: module_start reached at <sec>.<ms> pid=<pid> build=<id> variant=<v>"
//   4. close
//   5. return 0 — ALWAYS (igd1 lesson: a nonzero entry return is a free
//      variable; kernel-side propagation is unknown, so remove it).
// NO UE4SS, NO supplier, NO query harness, NO experiments, NO C++.
// The ONLY variable between the two frozen variants is the import surface:
//   sdg1 (min) = the 7 console-proven verbatim rows (path writability probe)
//   sdg2 (imp) = composition-4's exact 196-row import surface (resolution
//                probe) with the SAME checkpoint-only module_start.
#include "diag_config.h"
#include "ingame_abi.h"
#include "ingame_libc.h"

#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/fcntl.h>

/* Kept only so sentinels.c links unchanged (packaging recipe parity:
   init/fini arrays 1/1). Nothing reads these. */
unsigned g_ctor_events;
unsigned g_fini_events;

static unsigned char entry_guard;

static char *diag_fmt_u64(char *dst, unsigned long long v)
{
    char tmp[24];
    unsigned n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10ULL));
        v /= 10ULL;
    } while (v);
    while (n) *dst++ = tmp[--n];
    return dst;
}

static char *diag_str(char *dst, const char *s)
{
    while (*s) *dst++ = *s++;
    return dst;
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    (void)clock_gettime(CLOCK_REALTIME, &ts);

    int fd = sceKernelOpen(DIAG_LOG_PATH,
                           O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW,
                           (uint16_t)0600);
    if (fd >= 0) {
        char buf[256];
        char *p = buf;
        p = diag_str(p, "DIAG: module_start reached at ");
        p = diag_fmt_u64(p, (unsigned long long)ts.tv_sec);
        *p++ = '.';
        p = diag_fmt_u64(p, (unsigned long long)(ts.tv_nsec / 1000000ULL));
        p = diag_str(p, " pid=");
        p = diag_fmt_u64(p, (unsigned long long)getpid());
        p = diag_str(p, " build=" DIAG_BUILD_ID " variant=" DIAG_VARIANT "\n");
        (void)sceKernelWrite(fd, buf, (size_t)(p - buf));
        (void)sceKernelClose(fd);
    }
    /* Open failure stays silent by design: without the channel nothing can
       be reported, and the kernel-side start result must be identical
       whether or not /data/plugins was writable. */
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    return 0;
}
