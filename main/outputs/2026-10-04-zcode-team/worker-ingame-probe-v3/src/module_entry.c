// module_entry.c — v2 ISOLATION VARIANT (worker-ingame-probe-v2, team
// 2026-10-04). Vendored from worker-ingame-probe/src/module_entry.c; entry
// contract unchanged, plus the CONTROLLER-ADDED entry/startup markers:
//
//   1. ENTRY MARKER FIRST: the very first action after the record channel
//      opens is writing+flushing seq=1 exp="entry" — BEFORE the VMMAP
//      snapshot, before the ctor walk, before ANY pointer read. Rationale:
//      the aligned-entry sibling proved a /data log write works from inside
//      the game in the same launch that rejected v1, so
//        file ABSENT            -> load or pre-open startup failure;
//        file = entry line only -> load+channel OK, later failure localized;
//        entry+startup+V-records -> full run.
//   2. STARTUP RECORD (seq=2, exp="startup") right after the init-array walk:
//      separates module entry from channel-open/later-experiment failures
//      (ctors_run count + channel_open_rc).
//
// Lineage entry contract (unchanged from v1 / native-full-runtime-aligned-
// entry-1): exported module_start/module_stop with force_align_arg_pointer;
// single entry_guard; explicit DT_INIT/DT_FINI with loader-unowned init/fini
// arrays walked by the module itself (bounded 64, sentinel slot proves the
// walk ran; count surfaces in the startup record and V5 summary).
// COOPERATIVE QUIESCE: everything runs synchronously inside module_start; NO
// threads; channel closed; module_start returns 0; module stays loaded but
// INERT; no unload attempted.
#include "experiments.h"
#include "factstore.h"
#include "ingame_abi.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

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

    /* ---- Controller addition 1: ENTRY MARKER, first action after open. ---- */
    IgBuf eb;
    ig_record_begin(&eb, 1, "entry", 0, 0, IG_BUILD_ID);
    ig_kv_str(&eb, "phase", "module_start", 32);
    ig_kv_str(&eb, "note", "entry marker precedes all scans", 64);
    ig_record_flush(&eb);
    if (ig_sink_write_record(&sink, &eb) != 0) sink.failed = 1;

    /* Explicit init-array ownership: bounded, guarded walk (sentinel only
       by construction; count recorded in the startup record and V5). */
    unsigned ctor_count = 0;
    for (void (**c)(void) = __init_array_start; c != __init_array_end &&
         ctor_count < 64; ++c) {
        if (*c == 0) break;
        (*c)();
        ++ctor_count;
    }

    /* ---- Controller addition 2: STARTUP RESULT record (seq=2). ---- */
    IgBuf sb;
    ig_record_begin(&sb, 2, "startup", 0, 0, IG_BUILD_ID);
    ig_kv_str(&sb, "phase", "post_init_array", 32);
    ig_kv_int(&sb, "ctors_run", (int64_t)ctor_count);
    ig_kv_int(&sb, "channel_open_rc", 0);          /* open succeeded above */
    ig_kv_int(&sb, "channel_open_errno", (int64_t)sink.open_errno);
    ig_kv_int(&sb, "entry_marker_written", 1);     /* this far, seq1 flushed */
    ig_kv_int(&sb, "write_failures_so_far", (int64_t)sink.failed);
    ig_record_flush(&sb);
    if (ig_sink_write_record(&sink, &sb) != 0) sink.failed = 1;

    /* Early span snapshot for fault containment (emitted as V1 later).
       Nothing below this line dereferences foreign memory except through
       span-validated reads (ig_copy_checked). */
    ig_collect_vmmap();
    /* V1..V5, one+ bounded JSONL records each, seq continues from 2. */
    ig_run_all_experiments(&sink);
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
