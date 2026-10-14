#include "mb_win_srw_abi.h"
#if !defined(__x86_64__)
#error "SRW Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(mb_win_srwlock)==8&&_Alignof(mb_win_srwlock)==8,"SRW Windows layout");
_Static_assert(sizeof(mb_win_condition)==8&&_Alignof(mb_win_condition)==8,"CV Windows layout");
static mb_win_srw *installed;
static mb_win_srw_session *sessions;
static mb_win_srw_session **find(mb_win_srw_session *s){mb_win_srw_session **p=&sessions;while(*p&&*p!=s)p=&(*p)->next;return p;}
mb_win_srw_status mb_win_srw_begin(mb_win_srw_session *s,mb_win_srw *o,mb_pe_module *m){
    if(!s||!o||!m)return MB_WIN_SRW_INVALID_ARGUMENT;
    if(s->active||s->pin.owner||s->pin.next||s->module||s->next||m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_SRW_BAD_STATE;
    for(mb_win_srw_session *p=sessions;p;p=p->next)if(p==s||p->module==m)return MB_WIN_SRW_BAD_STATE;
    if(sessions&&installed!=o)return MB_WIN_SRW_BUSY;
    mb_win_srw_status status=mb_win_srw_acquire_pin(o,&s->pin);if(status)return status;
    if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_SRW_OK;
}
mb_win_srw_status mb_win_srw_end(mb_win_srw_session *s){
    if(!s)return MB_WIN_SRW_INVALID_ARGUMENT;mb_win_srw_session **p=find(s);
    if(!*p||!s->active)return MB_WIN_SRW_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_SRW_BUSY;
    mb_win_srw_status status=mb_win_srw_release_pin(&s->pin);if(status)return status;
    *p=s->next;*s=(mb_win_srw_session){0};if(!sessions)installed=NULL;return MB_WIN_SRW_OK;
}
static WIN64 void initialize_srw(mb_win_srwlock *p){mb_win_srw_initialize(installed,p);}
static WIN64 void acquire_exclusive(mb_win_srwlock *p){mb_win_srw_acquire(installed,p,0);}
static WIN64 void acquire_shared(mb_win_srwlock *p){mb_win_srw_acquire(installed,p,1);}
static WIN64 void release_exclusive(mb_win_srwlock *p){mb_win_srw_release(installed,p,0);}
static WIN64 void release_shared(mb_win_srwlock *p){mb_win_srw_release(installed,p,1);}
static WIN64 uint8_t try_exclusive(mb_win_srwlock *p){return mb_win_srw_try_acquire(installed,p,0);}
static WIN64 uint8_t try_shared(mb_win_srwlock *p){return mb_win_srw_try_acquire(installed,p,1);}
static WIN64 void initialize_condition(mb_win_condition *p){mb_win_srw_condition_initialize(installed,p);}
static WIN64 int32_t sleep_condition(mb_win_condition *cv,mb_win_srwlock *p,uint32_t ms,uint32_t flags){return mb_win_srw_sleep(installed,cv,p,ms,flags);}
static WIN64 void wake_condition(mb_win_condition *p){mb_win_srw_wake(installed,p,0);}
static WIN64 void wake_all(mb_win_condition *p){mb_win_srw_wake(installed,p,1);}
static int match(mb_pe_string v,const char *s,size_t n,int fold){if(v.size!=n)return 0;for(size_t i=0;i<n;++i){unsigned char c=(unsigned char)v.data[i];if(fold&&c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)s[i])return 0;}return 1;}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_srw_bind(void *context,const mb_pe_import *i,uint64_t *out){
    mb_win_srw_session *s=context;
    if(!s||!*find(s)||!s->active||!i||!out||(!i->library.data&&i->library.size)||(!i->name.data&&i->name.size)||(s->module->state!=MB_PE_MODULE_LOADING&&s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
    if(i->by_ordinal||!MATCH(i->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    if(MATCH(i->name,"InitializeSRWLock",0))*out=(uint64_t)(uintptr_t)&initialize_srw;
    else if(MATCH(i->name,"AcquireSRWLockExclusive",0))*out=(uint64_t)(uintptr_t)&acquire_exclusive;
    else if(MATCH(i->name,"AcquireSRWLockShared",0))*out=(uint64_t)(uintptr_t)&acquire_shared;
    else if(MATCH(i->name,"ReleaseSRWLockExclusive",0))*out=(uint64_t)(uintptr_t)&release_exclusive;
    else if(MATCH(i->name,"ReleaseSRWLockShared",0))*out=(uint64_t)(uintptr_t)&release_shared;
    else if(MATCH(i->name,"TryAcquireSRWLockExclusive",0))*out=(uint64_t)(uintptr_t)&try_exclusive;
    else if(MATCH(i->name,"TryAcquireSRWLockShared",0))*out=(uint64_t)(uintptr_t)&try_shared;
    else if(MATCH(i->name,"InitializeConditionVariable",0))*out=(uint64_t)(uintptr_t)&initialize_condition;
    else if(MATCH(i->name,"SleepConditionVariableSRW",0))*out=(uint64_t)(uintptr_t)&sleep_condition;
    else if(MATCH(i->name,"WakeConditionVariable",0))*out=(uint64_t)(uintptr_t)&wake_condition;
    else if(MATCH(i->name,"WakeAllConditionVariable",0))*out=(uint64_t)(uintptr_t)&wake_all;
    else return MB_PE_BIND_UNRESOLVED;
    return MB_PE_BIND_BOUND;
}
