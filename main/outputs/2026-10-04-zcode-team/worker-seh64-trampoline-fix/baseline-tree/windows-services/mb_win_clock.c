#include "mb_win_clock.h"
#if !defined(__x86_64__)
#error "Windows clock bridge requires an AMD64 target"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void*)==8 && sizeof(int64_t)==8 && sizeof(int32_t)==4,
    "Windows clock bridge requires AMD64 pointer and LARGE_INTEGER/BOOL widths");
static mb_win_thread_domain *installed;
static mb_win_clock_session *sessions;
static mb_win_clock_session **find_session(mb_win_clock_session *session)
{
    mb_win_clock_session **cursor=&sessions;
    while(*cursor && *cursor!=session) cursor=&(*cursor)->next;
    return cursor;
}
mb_win_clock_status mb_win_clock_begin(mb_win_clock_session *session,
    mb_win_thread_domain *domain,mb_pe_module *module)
{
    if(!session || !domain || !module) return MB_WIN_CLOCK_INVALID_ARGUMENT;
    if(session->active || session->pin.owner || session->pin.next || session->module || session->next ||
       module->state!=MB_PE_MODULE_EMPTY) return MB_WIN_CLOCK_BAD_STATE;
    for(mb_win_clock_session *s=sessions;s;s=s->next)
        if(s==session || s->module==module) return MB_WIN_CLOCK_BAD_STATE;
    if(sessions && installed!=domain) return MB_WIN_CLOCK_BUSY;
    if(mb_win_threads_acquire(domain,&session->pin)!=MB_WIN_THREADS_OK) return MB_WIN_CLOCK_OWNER_ERROR;
    if(!sessions) installed=domain;
    session->module=module; session->next=sessions; session->active=1; sessions=session;
    return MB_WIN_CLOCK_OK;
}
mb_win_clock_status mb_win_clock_end(mb_win_clock_session *session)
{
    if(!session) return MB_WIN_CLOCK_INVALID_ARGUMENT;
    mb_win_clock_session **cursor=find_session(session);
    if(!*cursor || !session->active) return MB_WIN_CLOCK_BAD_STATE;
    if(session->module->state!=MB_PE_MODULE_EMPTY) return MB_WIN_CLOCK_BUSY;
    if(mb_win_threads_release_pin(&session->pin)!=MB_WIN_THREADS_OK) return MB_WIN_CLOCK_OWNER_ERROR;
    *cursor=session->next; *session=(mb_win_clock_session){0};
    if(!sessions) installed=NULL;
    return MB_WIN_CLOCK_OK;
}
static int32_t complete(uint32_t *error,uint32_t saved,mb_win_clock_status status)
{
    if(status==MB_WIN_CLOCK_OK){*error=saved;return 1;}
    *error=status==MB_WIN_CLOCK_INVALID_ARGUMENT ? MB_WIN_ERROR_INVALID_PARAMETER :
        status==MB_WIN_CLOCK_RANGE_ERROR ? MB_WIN_CLOCK_ERROR_RANGE : MB_WIN_CLOCK_ERROR_NATIVE;
    return 0;
}
static WIN64 int32_t query_counter(int64_t *out)
{
    uint32_t *error=mb_win_threads_last_error(installed);
    uint32_t saved=*error;
    return complete(error,saved,mb_win_clock_read_counter(out));
}
static WIN64 int32_t query_frequency(int64_t *out)
{
    uint32_t *error=mb_win_threads_last_error(installed);
    uint32_t saved=*error;
    return complete(error,saved,mb_win_clock_read_frequency(out));
}
static int match(mb_pe_string view,const char *literal,size_t size,int fold)
{
    if(view.size!=size) return 0;
    for(size_t i=0;i<size;++i){
        unsigned char a=(unsigned char)view.data[i];
        if(fold && a>='A' && a<='Z') a=(unsigned char)(a+'a'-'A');
        if(a!=(unsigned char)literal[i]) return 0;
    }
    return 1;
}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_clock_bind(void *context,const mb_pe_import *import,uint64_t *value)
{
    mb_win_clock_session *session=context;
    if(!import || !value || (!import->library.data && import->library.size) ||
       (!import->name.data && import->name.size) || !session || !*find_session(session) || !session->active ||
       (session->module->state!=MB_PE_MODULE_LOADING && session->module->state!=MB_PE_MODULE_ACTIVE)) return MB_PE_BIND_ERROR;
    if(import->by_ordinal || !MATCH(import->library,"kernel32.dll",1)) return MB_PE_BIND_UNRESOLVED;
    if(MATCH(import->name,"QueryPerformanceCounter",0)){*value=(uint64_t)(uintptr_t)&query_counter;return MB_PE_BIND_BOUND;}
    if(MATCH(import->name,"QueryPerformanceFrequency",0)){*value=(uint64_t)(uintptr_t)&query_frequency;return MB_PE_BIND_BOUND;}
    return MB_PE_BIND_UNRESOLVED;
}
