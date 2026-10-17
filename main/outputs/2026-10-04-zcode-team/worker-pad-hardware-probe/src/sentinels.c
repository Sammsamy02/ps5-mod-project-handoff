// sentinels.c — VERBATIM VENDOR from worker-ingame-probe-v3/src/sentinels.c
// (lineage: native-full-runtime-aligned-entry-1/lifecycle/sentinels.c).
// Init/fini sentinels proving the explicit array walk executes in-game;
// ONLY functions — the array slots and boundary symbols live in
// boundaries.S.
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
