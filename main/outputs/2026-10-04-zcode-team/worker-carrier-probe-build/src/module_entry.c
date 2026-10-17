// module_entry.c — worker-carrier-probe-build VARIANT (cc1, team
// 2026-10-04). Vendored from worker-fc1-validator-probe/src/module_entry.c
// (lineage: worker-record-sink-addon -> worker-igf5-diagnosis-rebuild ->
// worker-anchor-validation -> worker-ingame-probe-v3 -> v2 -> v1 ->
// native-full-runtime-aligned-entry-1). Labeled changes ONLY:
//   - fc1pool_driver_run(&sink) REPLACED by carrier_driver_start(&sink):
//     the cc1 mission is the DATA-SIDE CARRIER-NAME CENSUS ridealong
//     (worker-r3-carrier-prep/CARRIER-CANDIDATES.md sec 6): ONE detached
//     thread, 30 polls x 4 s (~2 min bounded window), qdrv1 admission
//     gates (GUA header rcodes 1-7 + the 10000-count layout gate), then
//     ONE budget-bounded FName-pool census matching ONLY the fixed
//     53-name table onto its OWN channel /data/modbridge-carrier-facts.jsonl.
//     NO mutex import anywhere; the census thread is the single writer
//     after module_start returns.
//   - note strings updated to the cc1 mission; the startup record gains
//     the census cadence facts (polls=30, poll_s=4) per CANDIDATES
//     sec 6.3.
// Entry contract UNCHANGED from the loaded-and-logged v2/v3/igf4/qdrv1/
// fc1pool1 lineage: exported module_start/module_stop with
// force_align_arg_pointer; single entry_guard; ENTRY MARKER (seq=1
// exp="entry") written+flushed FIRST before any scan; STARTUP record
// (seq=2) after the bounded init-array walk; explicit DT_INIT/DT_FINI with
// loader-unowned arrays walked by the module itself (bounded 64, sentinel
// slot). module_start returns 0 in every path except the sink-open failure
// (-22) and re-entry (-20), so a refused census NEVER makes ploader flag
// this plugin FAIL.
#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "carrier_driver.h"
#include "ingame_abi.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

extern int carrier_driver_start(IgSink *sink);  /* carrier_driver.c: thread */

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
        /* Honest failure: without the record channel nothing can be reported;
           surface it to the loader instead of pretending success. */
        __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
        return -22;
    }

    /* ---- ENTRY MARKER: first action after the channel opens. ---- */
    IgBuf eb;
    ig_record_begin(&eb, 1, "entry", 0, 0, IG_BUILD_ID);
    ig_kv_str(&eb, "phase", "module_start", 32);
    ig_kv_str(&eb, "module", "cc1", 8);
    ig_kv_str(&eb, "note", "carrier-name census ridealong; entry precedes "
                           "all checks", 64);
    ig_record_flush(&eb);
    if (ig_sink_write_record(&sink, &eb) != 0) sink.failed = 1;

    /* Explicit init-array ownership: bounded, guarded walk (sentinel only
       by construction; count recorded in the startup record + summary). */
    for (void (**c)(void) = __init_array_start; c != __init_array_end &&
         g_ctor_events < 64; ++c) {
        if (*c == 0) break;
        (*c)();
        ++g_ctor_events;
    }

    /* ---- STARTUP RESULT record (seq=2). ---- */
    IgBuf sb;
    ig_record_begin(&sb, 2, "startup", 0, 0, IG_BUILD_ID);
    ig_kv_str(&sb, "phase", "post_init_array", 32);
    ig_kv_int(&sb, "ctors_run", (int64_t)g_ctor_events);
    ig_kv_int(&sb, "channel_open_rc", 0);          /* open succeeded above */
    ig_kv_int(&sb, "channel_open_errno", (int64_t)sink.open_errno);
    ig_kv_int(&sb, "entry_marker_written", 1);     /* this far, seq1 flushed */
    ig_kv_int(&sb, "write_failures_so_far", (int64_t)sink.failed);
    ig_kv_int(&sb, "polls", (int64_t)CC1_POLL_COUNT);
    ig_kv_int(&sb, "poll_s", (int64_t)CC1_POLL_SECONDS);
    ig_record_flush(&sb);
    if (ig_sink_write_record(&sink, &sb) != 0) sink.failed = 1;

    /* Focused run: the cc1 mission. carrier_driver_start spawns the ONE
       bounded census thread (mapscan refresh + GUA admission + the 10000
       layout gate + the budget-bounded FName-pool census, 4 s cadence,
       30 polls, then it closes the channel). Nonzero rc means the thread
       could not be created; the channel is closed here and the failure is
       visible by record absence (honest, per the qdrv1 lineage
       discipline). */
    int drv_rc = carrier_driver_start(&sink);
    if (drv_rc != 0) {
        IgBuf db;
        ig_record_begin(&db, 3, "driver_start_failed", 0, (int64_t)drv_rc,
                        IG_BUILD_ID);
        ig_kv_int(&db, "pthread_rc", (int64_t)drv_rc);
        ig_record_flush(&db);
        (void)ig_sink_write_record(&sink, &db);
        (void)ig_sink_close(&sink);
        __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
        return 0;
    }

    (void)g_fini_events;
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    for (void (**f)(void) = __fini_array_end; f != __fini_array_start &&
         g_fini_events < 64; --f) {
        void (*fn)(void) = *(f - 1);
        if (fn == 0) continue;
        fn();
        ++g_fini_events;
    }
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}
