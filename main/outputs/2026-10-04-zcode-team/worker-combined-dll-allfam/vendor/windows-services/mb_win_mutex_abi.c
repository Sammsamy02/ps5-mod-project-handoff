/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once_abi.c; that original file is unchanged). */
#include "mb_win_mutex_abi.h"
#if !defined(__x86_64__)
#error "Mutex Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void*)==8&&sizeof(int32_t)==4&&sizeof(uint32_t)==4,"Microsoft x64 widths");
static mb_win_mutex *installed;
static mb_win_mutex_session *sessions;
static mb_win_mutex_session **find(mb_win_mutex_session *s){mb_win_mutex_session **p=&sessions;while(*p&&*p!=s)p=&(*p)->next;return p;}
mb_win_mutex_status mb_win_mutex_begin_session(mb_win_mutex_session *s,mb_win_mutex *o,mb_pe_module *m){
    if(!s||!o||!m)return MB_WIN_MUTEX_INVALID_ARGUMENT;
    if(s->active||s->pin.owner||s->pin.next||s->module||s->next||m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_MUTEX_BAD_STATE;
    for(mb_win_mutex_session *p=sessions;p;p=p->next)if(p==s||p->module==m)return MB_WIN_MUTEX_BAD_STATE;
    if(sessions&&installed!=o)return MB_WIN_MUTEX_BUSY;
    mb_win_mutex_status status=mb_win_mutex_acquire_pin(o,&s->pin);if(status)return status;
    if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_MUTEX_OK;
}
mb_win_mutex_status mb_win_mutex_end_session(mb_win_mutex_session *s){
    if(!s)return MB_WIN_MUTEX_INVALID_ARGUMENT;mb_win_mutex_session **p=find(s);
    if(!*p||!s->active)return MB_WIN_MUTEX_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_MUTEX_BUSY;
    mb_win_mutex_status status=mb_win_mutex_release_pin(&s->pin);if(status)return status;
    *p=s->next;*s=(mb_win_mutex_session){0};if(!sessions)installed=NULL;return MB_WIN_MUTEX_OK;
}
/* Exact Microsoft x64 signatures; security-descriptor pointers and access
 * masks are accepted structurally and not enforced (single-process runtime,
 * no ACL model). */
static WIN64 void *create_mutex_w(void *security,int32_t initial_owner,const uint16_t *name){
    (void)security;
    return (void*)(uintptr_t)mb_win_mutex_create(installed,name,initial_owner?MB_WIN_MUTEX_INITIAL_OWNER:0);
}
static WIN64 void *create_mutex_ex_w(void *security,const uint16_t *name,uint32_t flags,uint32_t desired_access){
    (void)security;(void)desired_access;
    return (void*)(uintptr_t)mb_win_mutex_create(installed,name,flags);
}
static WIN64 void *open_mutex_w(uint32_t desired_access,int32_t inherit_handle,const uint16_t *name){
    (void)desired_access;(void)inherit_handle;
    return (void*)(uintptr_t)mb_win_mutex_open_named(installed,name);
}
static WIN64 int32_t release_mutex(void *handle){
    return mb_win_mutex_release(installed,(uint64_t)(uintptr_t)handle);
}
static WIN64 uint32_t wait_for_single_object(void *handle,uint32_t milliseconds){
    return mb_win_mutex_wait(installed,(uint64_t)(uintptr_t)handle,milliseconds);
}
static WIN64 uint32_t wait_for_single_object_ex(void *handle,uint32_t milliseconds,int32_t alertable){
    return mb_win_mutex_wait_ex(installed,(uint64_t)(uintptr_t)handle,milliseconds,alertable);
}
static int match(mb_pe_string v,const char *s,size_t n,int fold){if(v.size!=n)return 0;for(size_t i=0;i<n;++i){unsigned char c=(unsigned char)v.data[i];if(fold&&c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)s[i])return 0;}return 1;}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_mutex_bind(void *context,const mb_pe_import *i,uint64_t *out){
    mb_win_mutex_session *s=context;
    if(!s||!*find(s)||!s->active||!i||!out||(!i->library.data&&i->library.size)||(!i->name.data&&i->name.size)||(s->module->state!=MB_PE_MODULE_LOADING&&s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
    if(i->by_ordinal||!MATCH(i->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    if(MATCH(i->name,"CreateMutexW",0))*out=(uint64_t)(uintptr_t)&create_mutex_w;
    else if(MATCH(i->name,"CreateMutexExW",0))*out=(uint64_t)(uintptr_t)&create_mutex_ex_w;
    else if(MATCH(i->name,"OpenMutexW",0))*out=(uint64_t)(uintptr_t)&open_mutex_w;
    else if(MATCH(i->name,"ReleaseMutex",0))*out=(uint64_t)(uintptr_t)&release_mutex;
    else if(MATCH(i->name,"WaitForSingleObject",0))*out=(uint64_t)(uintptr_t)&wait_for_single_object;
    else if(MATCH(i->name,"WaitForSingleObjectEx",0))*out=(uint64_t)(uintptr_t)&wait_for_single_object_ex;
    else return MB_PE_BIND_UNRESOLVED;
    return MB_PE_BIND_BOUND;
}
