#include "mb_win_workers_abi.h"
#if !defined(__x86_64__)
#error "Windows worker ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void*)==8&&sizeof(size_t)==8&&sizeof(uint32_t)==4,"Windows worker widths");
static mb_win_workers *installed;
static mb_win_worker_session *sessions;
static mb_win_worker_session **find(mb_win_worker_session *s){mb_win_worker_session **at=&sessions;while(*at&&*at!=s)at=&(*at)->next;return at;}
mb_win_worker_status mb_win_workers_begin(mb_win_worker_session *s,mb_win_workers *o,mb_pe_module *m){
    if(!s||!o||!m)return MB_WIN_WORKERS_INVALID_ARGUMENT;
    if(s->active||s->pin.owner||s->pin.next||s->module||s->next||m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_WORKERS_BAD_STATE;
    for(mb_win_worker_session *p=sessions;p;p=p->next)if(p==s||p->module==m)return MB_WIN_WORKERS_BAD_STATE;
    if(sessions&&installed!=o)return MB_WIN_WORKERS_BUSY;
    mb_win_worker_status status=mb_win_workers_acquire(o,&s->pin);if(status)return status;
    if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_WORKERS_OK;
}
mb_win_worker_status mb_win_workers_end(mb_win_worker_session *s){
    if(!s)return MB_WIN_WORKERS_INVALID_ARGUMENT;mb_win_worker_session **at=find(s);
    if(!*at||!s->active)return MB_WIN_WORKERS_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_WORKERS_BUSY;
    mb_win_worker_status status=mb_win_workers_release_pin(&s->pin);if(status)return status;
    *at=s->next;*s=(mb_win_worker_session){0};if(!sessions)installed=NULL;return MB_WIN_WORKERS_OK;
}
uint32_t mb_win_workers_invoke(void *start,void *arg){typedef uint32_t(WIN64 *start_fn)(void*);return ((start_fn)start)(arg);}
static WIN64 void *create_thread(void *attrs,size_t size,void *start,void *arg,uint32_t flags,uint32_t *id){return mb_win_workers_create(installed,attrs,size,start,arg,flags,id);}
static WIN64 uint32_t wait_thread(void *handle,uint32_t ms){return mb_win_workers_wait(installed,handle,ms);}
static WIN64 int32_t exit_code(void *handle,uint32_t *code){return mb_win_workers_exit_code(installed,handle,code);}
static WIN64 int32_t close_handle(void *handle){return mb_win_workers_close_handle(installed,handle);}
static WIN64 void sleep_thread(uint32_t ms){mb_win_workers_sleep(installed,ms);}
static int match(mb_pe_string view,const char *s,size_t n,int fold){if(view.size!=n)return 0;for(size_t i=0;i<n;++i){unsigned char c=(unsigned char)view.data[i];if(fold&&c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)s[i])return 0;}return 1;}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_workers_bind(void *ctx,const mb_pe_import *i,uint64_t *out){
    mb_win_worker_session *s=ctx;
    if(!s||!*find(s)||!s->active||!i||!out||(!i->library.data&&i->library.size)||(!i->name.data&&i->name.size)||(s->module->state!=MB_PE_MODULE_LOADING&&s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
    if(i->by_ordinal||!MATCH(i->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    if(MATCH(i->name,"CreateThread",0))*out=(uint64_t)(uintptr_t)&create_thread;
    else if(MATCH(i->name,"WaitForSingleObject",0))*out=(uint64_t)(uintptr_t)&wait_thread;
    else if(MATCH(i->name,"GetExitCodeThread",0))*out=(uint64_t)(uintptr_t)&exit_code;
    else if(MATCH(i->name,"CloseHandle",0))*out=(uint64_t)(uintptr_t)&close_handle;
    else if(MATCH(i->name,"Sleep",0))*out=(uint64_t)(uintptr_t)&sleep_thread;
    else return MB_PE_BIND_UNRESOLVED;
    return MB_PE_BIND_BOUND;
}
