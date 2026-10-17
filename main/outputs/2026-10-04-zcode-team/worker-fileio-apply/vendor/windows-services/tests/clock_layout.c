#include "../mb_win_clock.h"
#include <stddef.h>
const unsigned long long mb_win_clock_layout[]={
 sizeof(void *),sizeof(size_t),sizeof(int64_t),_Alignof(int64_t),sizeof(int32_t),
 sizeof(mb_win_thread_domain),sizeof(mb_win_thread_pin),offsetof(mb_win_thread_pin,owner),
 sizeof(mb_pe_module),offsetof(mb_pe_module,state),
 sizeof(mb_win_clock_session),offsetof(mb_win_clock_session,pin),offsetof(mb_win_clock_session,module),
 offsetof(mb_win_clock_session,next),offsetof(mb_win_clock_session,active)
};
