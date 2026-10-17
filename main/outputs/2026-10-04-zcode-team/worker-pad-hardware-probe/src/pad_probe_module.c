// pad_probe_module.c — module entry for the standalone pad probe PRX
// (worker-pad-hardware-probe, team 2026-10-04). Vendored entry contract
// from worker-ingame-probe-v3/src/module_entry.c (lineage:
// native-full-runtime-aligned-entry-1), with the controller's ENTRY MARKER
// FIRST rule kept verbatim and the experiment body replaced by the pad
// probe run (pad_probe_core.c):
//
//   seq=1 "entry"    — written BEFORE any pad/service contact; file ABSENT
//                      -> load or channel failure; entry only -> load+channel
//                      OK with a later failure; entry+startup+probe records
//                      -> full run.
//   seq=2 "startup"  — post init-array: ctors_run, channel_open_errno, pid
//                      (getpid), CLOCK_MONOTONIC health (rc/errno/sample),
//                      sysctl KERN_VERSION (verbatim v3 v5 pattern), write
//                      failures so far.
//   seq=3..          — the probe: probe_start -> activate -> register
//                      (OPTIONS->F7) -> poll window -> probe_end/teardown.
//
// Lineage entry contract unchanged: exported module_start/module_stop with
// force_align_arg_pointer; single entry_guard; explicit DT_INIT/DT_FINI with
// loader-unowned init/fini arrays walked by the module (bounded 64, sentinel
// slot); COOPERATIVE QUIESCE — everything runs synchronously inside
// module_start (the bounded poll window), NO threads, channel closed,
// module_start returns 0, module stays loaded but INERT, no unload.
//
// The backend table is exactly the retained PS5PadNative.cpp factory table
// (sceUserServiceInitialize, scePadInit, sceUserServiceGetLoginUserIdList,
// scePadOpen, scePadReadState, scePadClose, sceUserServiceTerminate) — the
// first-ever hardware contact for native_ps5_pad_provider's backend.
#include "pad_probe_config.h"
#include "pad_abi.h"
#include "pad_probe_core.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>
#include <time.h>
#include <unistd.h>
#if defined(CTL_KERN) && defined(KERN_VERSION)
#include <sys/sysctl.h>
#endif

unsigned g_ctor_events;
unsigned g_fini_events;

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

static unsigned char entry_guard;

static const PP_PadBackend pp_native_backend = {
    sceUserServiceInitialize,            /* user_initialize(void*)           */
    scePadInit,                          /* pad_initialize(void)             */
    sceUserServiceGetLoginUserIdList,    /* user_list(int[4])                */
    scePadOpen,                          /* open(user,type,index,param)      */
    scePadReadState,                     /* read_state(handle, PS5_PadData*) */
    scePadClose,                         /* close(handle)                    */
    sceUserServiceTerminate              /* user_terminate(void)             */
};

__attribute__((visibility("default"), force_align_arg_pointer))
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    static IgSink sink;
    IgBuf rec;
    PP_ProbeConfig cfg;
    PP_ProbeDeps deps;
    PP_ProbeResult result;
    unsigned ctor_count = 0;
    int seq = 0;

    sink.fd = -1;
    sink.failed = 0;
    sink.open_errno = 0;
    if (ig_sink_open(&sink, PP_LOG_PATH) != 0) {
        /* Honest failure: without the record channel nothing can be
           reported; surface it to the loader instead of pretending. */
        __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
        return -22;
    }

    /* ---- ENTRY MARKER, first action after the channel opens. ---- */
    ig_record_begin(&rec, ++seq, "entry", 0, 0, PP_BUILD_ID);
    ig_kv_str(&rec, "phase", "module_start", 32);
    ig_kv_str(&rec, "note", "entry marker precedes all pad/service contact", 64);
    ig_record_flush(&rec);
    if (ig_sink_write_record(&sink, &rec) != 0) sink.failed = 1;

    /* Explicit init-array ownership: bounded, guarded walk. */
    for (void (**c)(void) = __init_array_start;
         c != __init_array_end && ctor_count < 64; ++c) {
        if (*c == 0) break;
        (*c)();
        ++ctor_count;
    }

    /* ---- STARTUP record (seq=2). ---- */
    {
        struct timespec ts;
        int clock_rc = clock_gettime(CLOCK_MONOTONIC, &ts);
        int *ce = __error();
        int clock_eno = (clock_rc != 0 && ce) ? *ce : 0;
        int64_t mono_ms = (clock_rc == 0)
            ? (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000 : -1;
        char vers[96];
        int sysctl_rc = -100;
        pp_memzero(vers, sizeof vers);
#if defined(CTL_KERN) && defined(KERN_VERSION)
        {
            int mib[2] = {CTL_KERN, KERN_VERSION};
            size_t vlen = sizeof vers - 1;
            sysctl_rc = sysctl(mib, 2, vers, &vlen, 0, 0);
        }
#endif
        ig_record_begin(&rec, ++seq, "startup", 0, 0, PP_BUILD_ID);
        ig_kv_str(&rec, "phase", "post_init_array", 32);
        ig_kv_int(&rec, "ctors_run", (int64_t)ctor_count);
        ig_kv_int(&rec, "channel_open_errno", (int64_t)sink.open_errno);
        ig_kv_int(&rec, "entry_marker_written", 1);
        ig_kv_int(&rec, "write_failures_so_far", (int64_t)sink.failed);
        ig_kv_int(&rec, "pid", (int64_t)getpid());
        ig_kv_int(&rec, "clock_rc", clock_rc);
        ig_kv_int(&rec, "clock_errno", clock_eno);
        ig_kv_int(&rec, "monotonic_now_ms", mono_ms);
        ig_kv_int(&rec, "sysctl_rc", sysctl_rc);
        ig_kv_str(&rec, "kern_version", vers, 88);
        ig_record_flush(&rec);
        if (ig_sink_write_record(&sink, &rec) != 0) sink.failed = 1;
    }

    /* ---- The probe: entry marker done -> register ONE keybind -> poll. */
    pp_memzero(&cfg, sizeof cfg);
    cfg.requested_user = -1; /* first logged-in user (prepared default) */
    deps.backend = pp_native_backend;
    deps.have_backend = 1;
    deps.sink = &sink;
    deps.build_id = PP_BUILD_ID;
    pp_memzero(&result, sizeof result);
    result.next_seq = seq + 1;
    (void)pp_run_probe(&cfg, &deps, &result);
    seq = result.next_seq;

    /* Quiesce: close the channel, keep no state beyond counters, no unload. */
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
    unsigned fini_count = 0;
    for (void (**f)(void) = __fini_array_end; f != __fini_array_start &&
         fini_count < 64; --f) {
        void (*fn)(void) = *(f - 1);
        if (fn == 0) continue;
        fn();
        ++fini_count;
    }
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}
