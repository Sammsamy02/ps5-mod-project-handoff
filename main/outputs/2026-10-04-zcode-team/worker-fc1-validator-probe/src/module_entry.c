// module_entry.c — worker-fc1-validator-probe VARIANT (fc1pool1, team
// 2026-10-04). Vendored from worker-record-sink-addon/src/module_entry.c
// (lineage: worker-igf5-diagnosis-rebuild -> worker-anchor-validation ->
// worker-ingame-probe-v3 -> v2 -> v1 -> native-full-runtime-aligned-entry-1).
// Labeled changes ONLY:
//   - qh_driver_start(&sink) REPLACED by fc1pool_driver_run(&sink): the
//     fc1pool1 mission is the DATA-SIDE FNAME-POOL VALIDATION CHAIN
//     (worker-fname-ctor-audit FNAME-VERDICT.md sec 5, step "in-game
//     data-side validation of the pool first"): GUA-anchor base
//     re-derivation + span-gated plain reads of the FNamePool header /
//     bootstrap ids / Blocks[] / entries at runtime 0x9765ec0. It is a
//     SYNCHRONOUS ONE-SHOT bounded run INSIDE module_start (no thread, no
//     pthread import at all): mapscan + admission are the only waits
//     (<= 8 grace polls x 1 s nanosleep, self-terminating), far inside the
//     ploader per-plugin wait cap (25 s, mode 2).
//   - note strings updated to the fc1pool1 mission.
// Entry contract UNCHANGED from the loaded-and-logged v2/v3/igf4/qdrv1
// lineage: exported module_start/module_stop with force_align_arg_pointer;
// single entry_guard; ENTRY MARKER (seq=1 exp="entry") written+flushed FIRST
// before any scan; STARTUP record (seq=2) after the bounded init-array walk;
// explicit DT_INIT/DT_FINI with loader-unowned arrays walked by the module
// itself (bounded 64, sentinel slot). module_start returns 0 in every path
// except the sink-open failure (-22) and re-entry (-20), so a refused
// validation NEVER makes ploader flag this plugin FAIL.
#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "fc1pool_driver.h"
#include "ingame_abi.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

extern int fc1pool_driver_run(IgSink *sink);  /* fc1pool_driver.c: one-shot */

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
    ig_kv_str(&eb, "note", "fc1pool data-side FName-pool validator; entry "
                           "precedes all checks", 64);
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
    ig_record_flush(&sb);
    if (ig_sink_write_record(&sink, &sb) != 0) sink.failed = 1;

    /* Focused run: the fc1pool1 mission — the data-side pool validation
       chain. Synchronous, bounded (<= 8 polls x 1 s), single-writer on its
       OWN channel; writes the ONE UE4SS.log line at the terminal verdict.
       Nonzero rc is impossible by contract (the driver records every
       refusal and returns 0); kept defensive anyway. */
    (void)fc1pool_driver_run(&sink);

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
