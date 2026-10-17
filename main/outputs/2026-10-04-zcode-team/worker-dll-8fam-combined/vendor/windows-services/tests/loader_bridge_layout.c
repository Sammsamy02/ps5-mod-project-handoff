#include "../mb_win_loader_bridge.h"
#include <stddef.h>
const unsigned long long mb_loader_bridge_layout[]={
 sizeof(void *),sizeof(size_t),sizeof(uint32_t),sizeof(int32_t),
 sizeof(mb_pe_loader),_Alignof(mb_pe_loader),offsetof(mb_pe_loader,config),offsetof(mb_pe_loader,pins),
 offsetof(mb_pe_loader,pin_count),offsetof(mb_pe_loader,active),
 sizeof(mb_pe_loader_config),offsetof(mb_pe_loader_config,catalog),offsetof(mb_pe_loader_config,catalog_count),offsetof(mb_pe_loader_config,records),
 sizeof(mb_pe_loader_record),offsetof(mb_pe_loader_record,state),offsetof(mb_pe_loader_record,handle),offsetof(mb_pe_loader_record,link),
 sizeof(mb_pe_loader_pin),offsetof(mb_pe_loader_pin,owner),offsetof(mb_pe_loader_pin,next),
 sizeof(mb_win_thread_pin),offsetof(mb_win_thread_pin,owner),offsetof(mb_win_thread_pin,next),
 sizeof(mb_win_loader_bridge_config),offsetof(mb_win_loader_bridge_config,loader),offsetof(mb_win_loader_bridge_config,domain),
 offsetof(mb_win_loader_bridge_config,context),offsetof(mb_win_loader_bridge_config,lock),offsetof(mb_win_loader_bridge_config,unlock),
 sizeof(mb_win_loader_bridge_session),offsetof(mb_win_loader_bridge_session,loader_pin),offsetof(mb_win_loader_bridge_session,domain_pin),
 offsetof(mb_win_loader_bridge_session,module),offsetof(mb_win_loader_bridge_session,next),offsetof(mb_win_loader_bridge_session,active),
 sizeof(mb_win_loader_runtime_context),offsetof(mb_win_loader_runtime_context,loader),offsetof(mb_win_loader_runtime_context,runtime),
 sizeof(mb_win_runtime_bind_context),offsetof(mb_win_runtime_bind_context,tls),
 sizeof(mb_win_tls_bridge_session),offsetof(mb_win_tls_bridge_session,module),
 sizeof(mb_pe_module),offsetof(mb_pe_module,state)
};
