// module_entry.c
//
// PRX module entry for the MODBRIDGE IN-GAME FACTS probe. Mirrors the proven
// lineage entry contract (.superpowers/native-dependency-spike/
// native-full-runtime-aligned-entry-1/lifecycle/module_entry.c):
//   - exported module_start/module_stop carry force_align_arg_pointer so a
//     nonstandard ploader caller alignment cannot corrupt our frame;
//   - a single entry_guard serializes both entries;
//   - the module OWNS its init/fini arrays explicitly (packager sets
//     explicitLifecycleOwnsArrays; DT_INIT/DT_FINI point here; the loader
//     never walks the arrays) — the ctor walk below is bounded and guarded;
//   - COOPERATIVE QUIESCE: everything runs synchronously inside
//     module_start; NO threads are ever created; the log channel is closed;
//     module_start returns 0 and the module stays loaded but INERT. No
//     unload is attempted (sceKernelUnloadProcessModules is not imported and
//     the lineage never unloads; ploader mode 2 ties lifetime to the game).
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
    /* Explicit init-array ownership: bounded, guarded walk (sentinel only
       by construction; count is recorded in the G7 summary). */
    unsigned ctor_count = 0;
    for (void (**c)(void) = __init_array_start; c != __init_array_end &&
         ctor_count < 64; ++c) {
        if (*c == 0) break;
        (*c)();
        ++ctor_count;
    }
    /* Early span snapshot for fault containment (emitted as G5 later). */
    ig_collect_vmmap();
    /* G1..G7, one bounded JSONL record each, in listed order. */
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
