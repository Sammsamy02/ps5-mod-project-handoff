#define LoaderExercise OwnedBaseExercise
#include "../../windows-execution/tests/loader_consumer.c"
#undef LoaderExercise
#include "entry_state.h"
#if MB_OWNED_CRT_STARTUP
#include "../tests/worker-crtstartup.h"
#endif
#if MB_OWNED_MODULE_ENUM
#include "../tests/worker-moduleenum.h"
#endif
#if MB_OWNED_VM
#include "../tests/worker-vm.h"
#endif
__declspec(dllexport) int LoaderExercise(usize *results){
 if(!entry_check())return -92;
#if MB_OWNED_CLOCK
 SetLastError(0xc10c1234u);if(!clock_check())return -93;
#endif
 /* Combined families run in the exercise window with the module ACTIVE; each
  * returns 1 or a unique negative code, keeping the 86/93 base results. */
#if MB_OWNED_CRT_STARTUP
 {mb_crt_i32 family=mb_worker_crtstartup_exercise();if(family!=1)return family;}
#endif
#if MB_OWNED_MODULE_ENUM
 {mb_mod_i32 family=mb_worker_moduleenum_exercise();if(family!=1)return family;}
#endif
#if MB_OWNED_VM
 {mb_vm_i32 family=mb_worker_vm_exercise();if(family!=1)return family;}
#endif
 return OwnedBaseExercise(results);
}
#if MB_OWNED_WORKERS
#include "../tests/worker-fixture.c"
#endif
