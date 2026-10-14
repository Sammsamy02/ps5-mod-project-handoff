// module_entry.c — MINIMAL DIAGNOSTIC VARIANT (worker-v3-startup-diag,
// team 2026-10-04). Vendored entry contract from worker-ingame-probe-v3
// (loader runs DT_INIT=module_start; ploader "OK" = sceKernelLoadStartModule
// returned a positive module id; module_start's own return goes to the
// kernel's start step, never to ploader).
//
// CONTROLLER DIRECTIVE (2026-10-04): module_start does EXACTLY
//   1. open log  (ig_sink_open: O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600)
//   2. write ONE entry-marker record (seq=1, exp="entry") + full write retry
//   3. close
//   4. return 0 (success) — ALWAYS, so a nonzero module_start return can no
//      longer perturb the experiment (v3 returned -22 on open failure; the
//      kernel-side propagation of a nonzero entry return is UNKNOWN, so the
//      diagnostic removes that variable entirely).
// NO experiments, NO VMMAP, NO predicates, NO base scan, NO ctor walk.
// The only operations that can fail are the file open/write themselves.
// Sentinels/boundaries remain linked (packaging recipe: init/fini slots 1/1)
// but are INERT: nothing walks the arrays in this variant.
#include "ingame_config.h"
#include "record_sink.h"
#include "ingame_abi.h"
#include "ingame_libc.h"

#include <stdint.h>

/* Inert counters kept only so sentinels.c links unchanged (recipe parity).
   No code in this variant ever calls the sentinels or reads these. */
unsigned g_ctor_events;
unsigned g_fini_events;

static unsigned char entry_guard;

__attribute__((visibility("default"), force_align_arg_pointer))
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    static IgSink sink;
    sink.fd = -1;
    sink.failed = 0;
    sink.open_errno = 0;
    if (ig_sink_open(&sink, IG_LOG_PATH) != 0) {
        /* Nothing observable can be reported without the channel; return 0
           per the diagnostic contract so the kernel-side start result is
           identical whether or not /data was writable. */
        __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
        return 0;
    }
    IgBuf eb;
    ig_record_begin(&eb, 1, "entry", 0, 0, IG_BUILD_ID);
    ig_kv_str(&eb, "phase", "module_start", 32);
    ig_kv_str(&eb, "note", "diag-min: open+marker+close only", 64);
    ig_record_flush(&eb);
    if (ig_sink_write_record(&sink, &eb) != 0) sink.failed = 1;
    (void)ig_sink_close(&sink);
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}
