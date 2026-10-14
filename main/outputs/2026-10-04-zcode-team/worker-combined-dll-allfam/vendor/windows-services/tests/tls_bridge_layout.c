#include "../mb_win_tls_bridge.h"
#include <stddef.h>
const unsigned long long mb_tls_bridge_layout[]={
 sizeof(void *),sizeof(size_t),sizeof(uint32_t),sizeof(int32_t),
 sizeof(mb_win_thread_domain),_Alignof(mb_win_thread_domain),offsetof(mb_win_thread_domain,state),
 sizeof(mb_win_thread_pin),_Alignof(mb_win_thread_pin),offsetof(mb_win_thread_pin,owner),offsetof(mb_win_thread_pin,next),
 sizeof(mb_pe_module),_Alignof(mb_pe_module),offsetof(mb_pe_module,state),
 sizeof(mb_win_tls_bridge_session),offsetof(mb_win_tls_bridge_session,pin),offsetof(mb_win_tls_bridge_session,module),
 offsetof(mb_win_tls_bridge_session,next),offsetof(mb_win_tls_bridge_session,active),
 sizeof(mb_win_heap_bridge_session),offsetof(mb_win_heap_bridge_session,module),
 sizeof(mb_win_runtime_bind_context),offsetof(mb_win_runtime_bind_context,tls),offsetof(mb_win_runtime_bind_context,process)
};
