// module_entry.c — worker-writability-probe-build VARIANT (team 2026-10-04).
// Vendored from worker-combined-igf7/src/module_entry.c (lineage:
// worker-anchor-validation -> worker-ingame-probe-v3 -> v2 -> v1 ->
// native-full-runtime-aligned-entry-1). Labeled changes ONLY:
//   - DUAL SINK: the dedicated record channel
//     /data/modbridge-exec-write-probe.jsonl (primary; same open/append/
//     write-through discipline as the loaded-and-logged lineage) PLUS a
//     write-through MIRROR of every record appended to
//     /data/plugins/UE4SS.log (best-effort: mirror failures are counted
//     and recorded, never fatal).
//   - ig_run_c2_data_probe REPLACED by ig_run_exec_write_probe
//     (exec_write_probe.c): the transition-table mprotect probe skeleton.
//   - FAIL-OPEN DELTA (brief-mandated): module_start returns 0 on EVERY
//     path — including dead-channel paths the lineage answered with -22.
//     If the jsonl channel cannot open, the failure notice is appended to
//     the mirror sink instead; only if BOTH channels are dead can the
//     no-op be silent (still returning 0).
//   - re-entry answers 0 (lineage answered -20) per the same fail-open rule.
// Entry contract otherwise UNCHANGED: exported module_start/module_stop
// with force_align_arg_pointer; single entry_guard; ENTRY MARKER (seq=1)
// written+flushed FIRST after the channel opens; bounded guarded init-array
// walk (64, sentinel slot); explicit DT_INIT/DT_FINI; COOPERATIVE QUIESCE:
// everything synchronous inside module_start; NO threads; module_start
// returns 0; module stays loaded but INERT; no unload attempted.
#include "anchor_store.h"
#include "exec_write_probe.h"
#include "generated/ingame_config.h"
#include "ingame_abi.h"
#include "ingame_libc.h"
#include "record_sink.h"

#include <stdint.h>

#define XWP_JSONL_PATH IG_LOG_PATH              /* /data/modbridge-exec-write-probe.jsonl */
#define XWP_MIRROR_PATH IG_MIRROR_LOG_PATH      /* /data/plugins/UE4SS.log */

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

unsigned g_ctor_events;
unsigned g_fini_events;

static unsigned char entry_guard;

__attribute__((visibility("default"), force_align_arg_pointer))
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return 0; /* FAIL-OPEN DELTA: lineage returned -20 */
    static IgSink jsonl;
    static IgSink mirror;
    jsonl.fd = -1; jsonl.failed = 0; jsonl.open_errno = 0;
    mirror.fd = -1; mirror.failed = 0; mirror.open_errno = 0;

    if (ig_sink_open(&jsonl, XWP_JSONL_PATH) != 0) {
        /* Primary channel dead. Best-effort notice through the mirror,
           then fail-open. Without either channel nothing is observable;
           returning 0 keeps the loader (and the game) untouched. */
        int jeno = jsonl.open_errno;
        if (ig_sink_open(&mirror, XWP_MIRROR_PATH) == 0) {
            IgBuf nb;
            ig_record_begin(&nb, 0, "channel_error", -1, jeno, IG_BUILD_ID);
            ig_kv_str(&nb, "phase", "jsonl_open_failed", 32);
            ig_kv_str(&nb, "jsonl_path", XWP_JSONL_PATH, 64);
            ig_record_flush(&nb);
            (void)ig_sink_write_record(&mirror, &nb);
            (void)ig_sink_close(&mirror);
        }
        __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
        return 0; /* FAIL-OPEN DELTA: lineage returned -22 */
    }
    /* Mirror is best-effort: a dead mirror is recorded in every summary. */
    (void)ig_sink_open(&mirror, XWP_MIRROR_PATH);

    /* ---- ENTRY MARKER: first action after the channel opens. ---- */
    IgBuf eb;
    ig_record_begin(&eb, 1, "entry", 0, 0, IG_BUILD_ID);
    ig_kv_str(&eb, "phase", "module_start", 32);
    ig_kv_str(&eb, "note", "exec-write probe skeleton; transition table "
                           "drives everything; no write before the design's "
                           "confirmed far page", 96);
    ig_kv_str(&eb, "jsonl_path", XWP_JSONL_PATH, 64);
    ig_kv_str(&eb, "mirror_path", XWP_MIRROR_PATH, 64);
    ig_kv_int(&eb, "mirror_open_errno", (int64_t)mirror.open_errno);
    ig_kv_int(&eb, "armed", IG_XWP_MPROTECT_ARMED);
    ig_kv_int(&eb, "table_rows", (int64_t)xwp_transitions_count);
    ig_record_flush(&eb);
    (void)ig_sink_write_record(&jsonl, &eb);
    if (mirror.fd >= 0 && ig_sink_write_record(&mirror, &eb) != 0)
        mirror.failed = 1;

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
    ig_kv_int(&sb, "channel_open_rc", 0);          /* jsonl open succeeded */
    ig_kv_int(&sb, "channel_open_errno", (int64_t)jsonl.open_errno);
    ig_kv_int(&sb, "mirror_open_rc", mirror.fd >= 0 ? 0 : -1);
    ig_kv_int(&sb, "mirror_open_errno", (int64_t)mirror.open_errno);
    ig_kv_int(&sb, "entry_marker_written", 1);     /* this far, seq1 flushed */
    ig_kv_int(&sb, "write_failures_so_far", (int64_t)jsonl.failed);
    ig_kv_int(&sb, "armed", IG_XWP_MPROTECT_ARMED);
    ig_kv_int(&sb, "table_rows", (int64_t)xwp_transitions_count);
    ig_kv_int(&sb, "attempt_budget", XWP_ATTEMPT_BUDGET);
    ig_record_flush(&sb);
    (void)ig_sink_write_record(&jsonl, &sb);
    if (mirror.fd >= 0 && ig_sink_write_record(&mirror, &sb) != 0)
        mirror.failed = 1;

    /* Focused run: seq 3.., each record flushed BEFORE the next operation
       (fault containment; the only memory touched is gated on the
       design-confirmed rows; budget-capped; restored + re-proved after
       every attempt). */
    (void)ig_run_exec_write_probe(&jsonl, &mirror);

    /* Quiesce: close the channels, keep no state beyond counters, no
       unload. */
    (void)ig_sink_close(&jsonl);
    (void)ig_sink_close(&mirror);
    (void)g_fini_events;
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return 0; /* FAIL-OPEN DELTA: lineage returned -20 */
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
