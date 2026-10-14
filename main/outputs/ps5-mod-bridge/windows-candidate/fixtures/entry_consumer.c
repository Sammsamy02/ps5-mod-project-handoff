#define LoaderExercise OwnedBaseExercise
#include "../../windows-execution/tests/loader_consumer.c"
#undef LoaderExercise
#include "entry_state.h"
__declspec(dllexport) int LoaderExercise(usize *results){
 if(!entry_check())return -92;
#if MB_OWNED_CLOCK
 SetLastError(0xc10c1234u);if(!clock_check())return -93;
#endif
 return OwnedBaseExercise(results);
}
#if MB_OWNED_WORKERS
#include "../tests/worker-fixture.c"
#endif
