// module_entry.c — worker-anchor-validation VARIANT (team 2026-10-04).
// Vendored from worker-ingame-probe-v3/src/module_entry.c (lineage:
// worker-ingame-probe-v2 -> worker-ingame-probe -> native-full-runtime-
// aligned-entry-1). Labeled changes ONLY:
//   - ig_collect_vmmap()/ig_run_all_experiments() REPLACED by the focused
//     ig_run_anchor_probe(&sink) (seq 3..10): bounded current-invocation
//     mapscan for readability gating (controller correction #1; NO map dump,
//     correction #6), Tick/SCO code anchors vs retained-image expected bytes
//     (correction #2), GUA 96-byte structural predicate under both feed
//     configurations and both layout hypotheses (correction #3), same-value
//     GUA writeback write test (correction #5, user-authorized writes),
//     summary record; everything under ONE build_id in ONE invocation
//     (correction #4).
//   - defines g_ctor_events/g_fini_events (sentinels.c externs them).
// Entry contract UNCHANGED from the loaded-and-logged v2/v3 lineage:
// exported module_start/module_stop with force_align_arg_pointer; single
// entry_guard; ENTRY MARKER (seq=1 exp="entry") written+flushed FIRST before
// any scan; STARTUP record (seq=2 exp="startup") after the bounded init-array
// walk; explicit DT_INIT/DT_FINI with loader-unowned arrays walked by the
// module itself (bounded 64, sentinel slot). COOPERATIVE QUIESCE: everything
// runs synchronously inside module_start; NO threads; channel closed;
// module_start returns 0; module stays loaded but INERT; no unload attempted.
#include "anchor_store.h"
#include "generated/ingame_config.h"
#include "ingame_abi.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

extern void ig_run_anchor_probe(IgSink *sink);  /* anchor_probe.cpp */

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
    ig_kv_str(&eb, "note", "focused anchor validation; entry precedes all "
                           "checks", 64);
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

    /* Focused validation run: seq 3..10, each bounded record written before
       the next gated dereference (fault containment). */
    ig_run_anchor_probe(&sink);

    /* Quiesce: close the channel, keep no state beyond counters, no unload. */
    (void)ig_sink_close(&sink);
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
