#include "../mb_win_heap_bridge.h"
#include <stddef.h>
const unsigned long long mb_heap_bridge_layout[]={
 sizeof(void *),sizeof(size_t),sizeof(int32_t),
 sizeof(mb_win_heap),_Alignof(mb_win_heap),offsetof(mb_win_heap,ops),offsetof(mb_win_heap,pins),
 offsetof(mb_win_heap,pin_count),offsetof(mb_win_heap,live_count),offsetof(mb_win_heap,live_bytes),offsetof(mb_win_heap,active),
 sizeof(mb_win_heap_ops),offsetof(mb_win_heap_ops,errno_slot),
 sizeof(mb_win_heap_pin),offsetof(mb_win_heap_pin,owner),offsetof(mb_win_heap_pin,next),
 sizeof(mb_pe_module),_Alignof(mb_pe_module),offsetof(mb_pe_module,state),
 sizeof(mb_win_heap_bridge_config),offsetof(mb_win_heap_bridge_config,heap),offsetof(mb_win_heap_bridge_config,context),
 offsetof(mb_win_heap_bridge_config,lock),offsetof(mb_win_heap_bridge_config,unlock),
 sizeof(mb_win_heap_bridge_session),offsetof(mb_win_heap_bridge_session,pin),offsetof(mb_win_heap_bridge_session,module),
 offsetof(mb_win_heap_bridge_session,next),offsetof(mb_win_heap_bridge_session,active)
};
