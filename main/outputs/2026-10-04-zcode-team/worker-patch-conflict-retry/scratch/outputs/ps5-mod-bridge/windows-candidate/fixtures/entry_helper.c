#define HelperCompute OwnedBaseCompute
#include "../../windows-execution/tests/loader_helper.c"
#undef HelperCompute
#include "entry_state.h"
__declspec(dllexport) int HelperCompute(int value){
 if(!entry_check())return -91;
#if MB_OWNED_CLOCK
 SetLastError(0xc10c1234u);if(!clock_check())return -93;
#endif
 return OwnedBaseCompute(value);
}
