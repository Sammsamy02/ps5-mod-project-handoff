#include "mb_win_critical_abi.h"
#include <stdlib.h>
#if !defined(__x86_64__)
#error "Critical section Win64 ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void*)==8&&sizeof(uint32_t)==4&&sizeof(mb_win_critical_storage)==40&&_Alignof(mb_win_critical_storage)==8,"Windows critical section ABI");
static mb_win_critical *installed;
static mb_win_critical_session *sessions;
static mb_win_critical_session **find(mb_win_critical_session *s){mb_win_critical_session **at=&sessions;while(*at&&*at!=s)at=&(*at)->next;return at;}
mb_win_critical_status mb_win_critical_begin(mb_win_critical_session *s,mb_win_critical *o,mb_pe_module *m){
    if(!s||!o||!m)return MB_WIN_CRITICAL_INVALID_ARGUMENT;
    if(s->active||s->pin.owner||s->pin.next||s->module||s->next||m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_CRITICAL_BAD_STATE;
    for(mb_win_critical_session *p=sessions;p;p=p->next)if(p==s||p->module==m)return MB_WIN_CRITICAL_BAD_STATE;
    if(sessions&&installed!=o)return MB_WIN_CRITICAL_BUSY;
    mb_win_critical_status status=mb_win_critical_acquire(o,&s->pin);if(status)return status;
    if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_CRITICAL_OK;
}
mb_win_critical_status mb_win_critical_end(mb_win_critical_session *s){
    if(!s)return MB_WIN_CRITICAL_INVALID_ARGUMENT;mb_win_critical_session **at=find(s);
    if(!*at||!s->active)return MB_WIN_CRITICAL_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_CRITICAL_BUSY;
    mb_win_critical_status status=mb_win_critical_release_pin(&s->pin);if(status)return status;
    *at=s->next;*s=(mb_win_critical_session){0};if(!sessions)installed=NULL;return MB_WIN_CRITICAL_OK;
}
static uint32_t *error_slot(void){return mb_win_threads_last_error(mb_win_critical_domain(installed));}
static void require(mb_win_critical_status status){if(status!=MB_WIN_CRITICAL_OK)abort();}
static WIN64 void initialize(void *p){uint32_t *e=error_slot(),saved=*e;require(mb_win_critical_initialize(installed,p,0));*e=saved;}
static WIN64 void enter(void *p){uint32_t *e=error_slot(),saved=*e;require(mb_win_critical_enter(installed,p));*e=saved;}
static WIN64 void leave(void *p){uint32_t *e=error_slot(),saved=*e;require(mb_win_critical_leave(installed,p));*e=saved;}
static WIN64 void destroy(void *p){uint32_t *e=error_slot(),saved=*e;require(mb_win_critical_delete(installed,p));*e=saved;}
static WIN64 int32_t attempt(void *p){uint32_t *e=error_slot(),saved=*e;int32_t got=0;require(mb_win_critical_try_enter(installed,p,&got));*e=saved;return got;}
static WIN64 int32_t initialize_spin(void *p,uint32_t spin){
    uint32_t *e=error_slot(),saved=*e;mb_win_critical_status s=mb_win_critical_initialize(installed,p,spin);
    if(!s){*e=saved;return 1;}*e=s==MB_WIN_CRITICAL_LIMIT?8:s==MB_WIN_CRITICAL_NATIVE_ERROR?31:87;return 0;
}
static WIN64 uint32_t set_spin(void *p,uint32_t spin){uint32_t *e=error_slot(),saved=*e,previous=0;require(mb_win_critical_set_spin(installed,p,spin,&previous));*e=saved;return previous;}
static int match(mb_pe_string view,const char *s,size_t n,int fold){if(view.size!=n)return 0;for(size_t i=0;i<n;++i){unsigned char c=(unsigned char)view.data[i];if(fold&&c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)s[i])return 0;}return 1;}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_critical_bind(void *ctx,const mb_pe_import *i,uint64_t *out){
    mb_win_critical_session *s=ctx;
    if(!s||!*find(s)||!s->active||!i||!out||(!i->library.data&&i->library.size)||(!i->name.data&&i->name.size)||(s->module->state!=MB_PE_MODULE_LOADING&&s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
    if(i->by_ordinal||!MATCH(i->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    if(MATCH(i->name,"InitializeCriticalSection",0))*out=(uint64_t)(uintptr_t)&initialize;
    else if(MATCH(i->name,"EnterCriticalSection",0))*out=(uint64_t)(uintptr_t)&enter;
    else if(MATCH(i->name,"LeaveCriticalSection",0))*out=(uint64_t)(uintptr_t)&leave;
    else if(MATCH(i->name,"DeleteCriticalSection",0))*out=(uint64_t)(uintptr_t)&destroy;
    else if(MATCH(i->name,"TryEnterCriticalSection",0))*out=(uint64_t)(uintptr_t)&attempt;
    else if(MATCH(i->name,"InitializeCriticalSectionAndSpinCount",0))*out=(uint64_t)(uintptr_t)&initialize_spin;
    else if(MATCH(i->name,"SetCriticalSectionSpinCount",0))*out=(uint64_t)(uintptr_t)&set_spin;
    else return MB_PE_BIND_UNRESOLVED;
    return MB_PE_BIND_BOUND;
}
