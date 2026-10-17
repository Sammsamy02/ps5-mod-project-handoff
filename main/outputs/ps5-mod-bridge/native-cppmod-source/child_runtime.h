#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
enum mb_cppmod_child_status {
    MB_CPPMOD_CHILD_OK=0,
    MB_CPPMOD_CHILD_BUSY=-1,
    MB_CPPMOD_CHILD_NOT_ACTIVE=-2,
    MB_CPPMOD_CHILD_INVALID_LAYOUT=-3,
    MB_CPPMOD_CHILD_FAILED=-4,
    MB_CPPMOD_CHILD_FINISHED=-5,
    MB_CPPMOD_CHILD_BAD_RELEASE=-6
};

int module_start(size_t argc,const void* args);
int module_stop(size_t argc,const void* args);

/* Link these private helpers into the child. Acquire before constructing a mod;
 * release after a failed construction or successful destruction. The parent
 * must serialize stop against calls and retire all callbacks before unloading.
 * A busy stop is a source-level refusal; target loader acceptance is unproven. */
__attribute__((visibility("hidden"))) int mb_cppmod_child_acquire(void);
__attribute__((visibility("hidden"))) int mb_cppmod_child_release(void);
#ifdef __cplusplus
}
#endif
