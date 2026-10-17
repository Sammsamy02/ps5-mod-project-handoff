#include "mb_win_heap_bridge.h"
#include "mb_win_crt_bind.h"
#if !defined(__x86_64__)
#error "Windows heap bridge requires an AMD64 target"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(size_t)==8 && sizeof(void *)==8 && sizeof(int)==4,
               "Windows heap bridge requires 64-bit pointers/sizes and 32-bit int");

static mb_win_heap_bridge_config installed;
static mb_win_heap_bridge_session *sessions;

static mb_win_heap_bridge_session **find_session(mb_win_heap_bridge_session *session)
{
    mb_win_heap_bridge_session **cursor=&sessions;
    while (*cursor && *cursor!=session) cursor=&(*cursor)->next;
    return cursor;
}
static int same_config(const mb_win_heap_bridge_config *config)
{
    return installed.heap==config->heap && installed.context==config->context &&
           installed.lock==config->lock && installed.unlock==config->unlock;
}
mb_win_heap_bridge_status mb_win_heap_bridge_begin(mb_win_heap_bridge_session *session,
    const mb_win_heap_bridge_config *config, mb_pe_module *module)
{
    if (!session || !config || !config->heap || !config->lock || !config->unlock || !module)
        return MB_WIN_HEAP_BRIDGE_INVALID_ARGUMENT;
    if (session->active || session->pin.owner || session->pin.next || session->module || session->next ||
        module->state!=MB_PE_MODULE_EMPTY)
        return MB_WIN_HEAP_BRIDGE_BAD_STATE;
    for (mb_win_heap_bridge_session *s=sessions;s;s=s->next)
        if (s==session || s->module==module) return MB_WIN_HEAP_BRIDGE_BAD_STATE;
    if (sessions && !same_config(config)) return MB_WIN_HEAP_BRIDGE_BUSY;
    config->lock(config->context);
    mb_win_heap_status status=mb_win_heap_acquire(config->heap,&session->pin);
    config->unlock(config->context);
    if (status!=MB_WIN_HEAP_OK) return MB_WIN_HEAP_BRIDGE_OWNER_ERROR;
    if (!sessions) installed=*config;
    session->module=module;
    session->next=sessions;
    session->active=1;
    sessions=session;
    return MB_WIN_HEAP_BRIDGE_OK;
}
mb_win_heap_bridge_status mb_win_heap_bridge_end(mb_win_heap_bridge_session *session)
{
    if (!session) return MB_WIN_HEAP_BRIDGE_INVALID_ARGUMENT;
    mb_win_heap_bridge_session **cursor=find_session(session);
    if (!*cursor || !session->active) return MB_WIN_HEAP_BRIDGE_BAD_STATE;
    if (session->module->state!=MB_PE_MODULE_EMPTY) return MB_WIN_HEAP_BRIDGE_BUSY;
    installed.lock(installed.context);
    if (sessions==session && !session->next && installed.heap->live_count) {
        installed.unlock(installed.context);
        return MB_WIN_HEAP_BRIDGE_BUSY;
    }
    mb_win_heap_status status=mb_win_heap_release(&session->pin);
    installed.unlock(installed.context);
    if (status!=MB_WIN_HEAP_OK) return MB_WIN_HEAP_BRIDGE_OWNER_ERROR;
    *cursor=session->next;
    *session=(mb_win_heap_bridge_session){0};
    if (!sessions) installed=(mb_win_heap_bridge_config){0};
    return MB_WIN_HEAP_BRIDGE_OK;
}

static WIN64 void *heap_malloc(size_t size)
{
    installed.lock(installed.context);
    void *result=mb_win_heap_malloc(installed.heap,size);
    installed.unlock(installed.context);
    return result;
}
static WIN64 void *heap_calloc(size_t count,size_t size)
{
    installed.lock(installed.context);
    void *result=mb_win_heap_calloc(installed.heap,count,size);
    installed.unlock(installed.context);
    return result;
}
static WIN64 void *heap_realloc(void *pointer,size_t size)
{
    installed.lock(installed.context);
    void *result=mb_win_heap_realloc(installed.heap,pointer,size);
    installed.unlock(installed.context);
    return result;
}
static WIN64 void heap_free(void *pointer)
{
    installed.lock(installed.context);
    mb_win_heap_free(installed.heap,pointer);
    installed.unlock(installed.context);
}
static WIN64 int32_t *heap_errno(void)
{
    installed.lock(installed.context);
    int32_t *result=mb_win_heap_errno(installed.heap);
    installed.unlock(installed.context);
    return result;
}
static int match(mb_pe_string view,const char *literal,size_t size,int fold)
{
    if (view.size!=size) return 0;
    for (size_t i=0;i<size;++i) {
        unsigned char a=(unsigned char)view.data[i],b=(unsigned char)literal[i];
        if (fold && a>='A' && a<='Z') a=(unsigned char)(a+'a'-'A');
        if (a!=b) return 0;
    }
    return 1;
}
#define MATCH(v,s,fold) match((v),(s),sizeof(s)-1,(fold))
#define BIND(s,fn) if (MATCH(import->name,s,0)) { *value=(uint64_t)(uintptr_t)&fn; return MB_PE_BIND_BOUND; }
mb_pe_bind_result mb_win_heap_bridge_bind(void *context,const mb_pe_import *import,uint64_t *value)
{
    mb_win_heap_bridge_session *session=context;
    if (!session || !import || !value || (!import->library.data && import->library.size) ||
        (!import->name.data && import->name.size)) return MB_PE_BIND_ERROR;
    if (!*find_session(session) || !session->active ||
        (session->module->state!=MB_PE_MODULE_LOADING && session->module->state!=MB_PE_MODULE_ACTIVE))
        return MB_PE_BIND_ERROR;
    if (import->by_ordinal) return MB_PE_BIND_UNRESOLVED;
    if (MATCH(import->library,"api-ms-win-crt-heap-l1-1-0.dll",1)) {
        BIND("malloc",heap_malloc)
        BIND("calloc",heap_calloc)
        BIND("realloc",heap_realloc)
        BIND("free",heap_free)
    } else if (MATCH(import->library,"api-ms-win-crt-runtime-l1-1-0.dll",1)) {
        BIND("_errno",heap_errno)
    }
    return MB_PE_BIND_UNRESOLVED;
}
mb_pe_bind_result mb_win_process_bind(void *context,const mb_pe_import *import,uint64_t *value)
{
    mb_pe_bind_result result=mb_win_heap_bridge_bind(context,import,value);
    return result==MB_PE_BIND_UNRESOLVED ? mb_win_crt_bind(NULL,import,value) : result;
}
