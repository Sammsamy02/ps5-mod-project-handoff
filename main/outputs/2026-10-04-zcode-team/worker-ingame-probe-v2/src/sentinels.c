// sentinels.c
//
// Init/fini sentinels proving the explicit array walk executes in-game.
// Following the lineage (native-full-runtime-aligned-entry-1/lifecycle/
// sentinels.c) these are ONLY functions; the array slots and the
// __init_array_start/__init_array_end boundary symbols live in boundaries.S
// so the boundaries deterministically bracket every entry of this module
// (it is the sole contributor of .init_array/.fini_array content).
#include <stdint.h>

extern unsigned g_ctor_events;
extern unsigned g_fini_events;

__attribute__((visibility("hidden")))
void ig_ctor_sentinel(void)
{
    __atomic_add_fetch(&g_ctor_events, 1, __ATOMIC_RELAXED);
}

__attribute__((visibility("hidden")))
void ig_fini_sentinel(void)
{
    __atomic_add_fetch(&g_fini_events, 1, __ATOMIC_RELAXED);
}
