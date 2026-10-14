#include "mb_win_tls_bridge.h"
#if !defined(__x86_64__)
#error "Windows TLS bridge requires an AMD64 target"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void *)==8 && sizeof(uint32_t)==4 && sizeof(int32_t)==4,
               "Windows TLS bridge requires 64-bit pointers and 32-bit DWORD/BOOL");
static mb_win_thread_domain *installed;
static mb_win_tls_bridge_session *sessions;

static mb_win_tls_bridge_session **find_session(mb_win_tls_bridge_session *session)
{
    mb_win_tls_bridge_session **cursor=&sessions;
    while(*cursor && *cursor!=session) cursor=&(*cursor)->next;
    return cursor;
}
static int valid_binding_session(mb_win_tls_bridge_session *session)
{
    return session && *find_session(session) && session->active &&
        (session->module->state==MB_PE_MODULE_LOADING || session->module->state==MB_PE_MODULE_ACTIVE);
}
mb_win_tls_bridge_status mb_win_tls_bridge_begin(mb_win_tls_bridge_session *session,
    mb_win_thread_domain *domain,mb_pe_module *module)
{
    if(!session || !domain || !module) return MB_WIN_TLS_BRIDGE_INVALID_ARGUMENT;
    if(session->active || session->pin.owner || session->pin.next || session->module || session->next ||
       module->state!=MB_PE_MODULE_EMPTY) return MB_WIN_TLS_BRIDGE_BAD_STATE;
    for(mb_win_tls_bridge_session *s=sessions;s;s=s->next)
        if(s==session || s->module==module) return MB_WIN_TLS_BRIDGE_BAD_STATE;
    if(sessions && installed!=domain) return MB_WIN_TLS_BRIDGE_BUSY;
    if(mb_win_threads_acquire(domain,&session->pin)!=MB_WIN_THREADS_OK)
        return MB_WIN_TLS_BRIDGE_OWNER_ERROR;
    if(!sessions) installed=domain;
    session->module=module;
    session->next=sessions;
    session->active=1;
    sessions=session;
    return MB_WIN_TLS_BRIDGE_OK;
}
mb_win_tls_bridge_status mb_win_tls_bridge_end(mb_win_tls_bridge_session *session)
{
    if(!session) return MB_WIN_TLS_BRIDGE_INVALID_ARGUMENT;
    mb_win_tls_bridge_session **cursor=find_session(session);
    if(!*cursor || !session->active) return MB_WIN_TLS_BRIDGE_BAD_STATE;
    if(session->module->state!=MB_PE_MODULE_EMPTY) return MB_WIN_TLS_BRIDGE_BUSY;
    if(mb_win_threads_release_pin(&session->pin)!=MB_WIN_THREADS_OK)
        return MB_WIN_TLS_BRIDGE_OWNER_ERROR;
    *cursor=session->next;
    *session=(mb_win_tls_bridge_session){0};
    if(!sessions) installed=NULL;
    return MB_WIN_TLS_BRIDGE_OK;
}
static WIN64 uint32_t tls_get_last_error(void) {return *mb_win_threads_last_error(installed);}
static WIN64 void tls_set_last_error(uint32_t value) {*mb_win_threads_last_error(installed)=value;}
static WIN64 uint32_t tls_alloc(void) {return mb_win_threads_tls_alloc(installed);}
static WIN64 int32_t tls_free(uint32_t index) {return mb_win_threads_tls_free(installed,index);}
static WIN64 void *tls_get_value(uint32_t index) {return mb_win_threads_tls_get(installed,index);}
static WIN64 int32_t tls_set_value(uint32_t index,void *value) {return mb_win_threads_tls_set(installed,index,value);}
static int match(mb_pe_string view,const char *literal,size_t size,int fold)
{
    if(view.size!=size) return 0;
    for(size_t i=0;i<size;++i) {
        unsigned char a=(unsigned char)view.data[i],b=(unsigned char)literal[i];
        if(fold && a>='A' && a<='Z') a=(unsigned char)(a+'a'-'A');
        if(a!=b) return 0;
    }
    return 1;
}
#define MATCH(v,s,fold) match((v),(s),sizeof(s)-1,(fold))
#define BIND(s,fn) if(MATCH(import->name,s,0)){*value=(uint64_t)(uintptr_t)&fn;return MB_PE_BIND_BOUND;}
mb_pe_bind_result mb_win_tls_bridge_bind(void *context,const mb_pe_import *import,uint64_t *value)
{
    if(!import || !value || (!import->library.data && import->library.size) ||
       (!import->name.data && import->name.size) || !valid_binding_session(context)) return MB_PE_BIND_ERROR;
    if(import->by_ordinal || !MATCH(import->library,"kernel32.dll",1)) return MB_PE_BIND_UNRESOLVED;
    BIND("GetLastError",tls_get_last_error)
    BIND("SetLastError",tls_set_last_error)
    BIND("TlsAlloc",tls_alloc)
    BIND("TlsFree",tls_free)
    BIND("TlsGetValue",tls_get_value)
    BIND("TlsSetValue",tls_set_value)
    return MB_PE_BIND_UNRESOLVED;
}
mb_pe_bind_result mb_win_runtime_bind(void *opaque,const mb_pe_import *import,uint64_t *value)
{
    mb_win_runtime_bind_context *context=opaque;
    if(!context || !context->process || !valid_binding_session(context->tls) ||
       context->tls->module!=context->process->module) return MB_PE_BIND_ERROR;
    mb_pe_bind_result result=mb_win_process_bind(context->process,import,value);
    return result==MB_PE_BIND_UNRESOLVED ? mb_win_tls_bridge_bind(context->tls,import,value) : result;
}
