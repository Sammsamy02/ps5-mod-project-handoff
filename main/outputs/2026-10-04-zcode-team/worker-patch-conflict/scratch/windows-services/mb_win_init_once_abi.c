/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_srw_abi.c; that original file is unchanged). */
#include "mb_win_init_once_abi.h"
#if !defined(__x86_64__)
#error "InitOnce Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(mb_win_once)==8&&_Alignof(mb_win_once)==8,"INIT_ONCE Windows layout");
static mb_win_init_once *installed;
static mb_win_init_once_session *sessions;
static mb_win_init_once_session **find(mb_win_init_once_session *s){mb_win_init_once_session **p=&sessions;while(*p&&*p!=s)p=&(*p)->next;return p;}
mb_win_init_once_status mb_win_init_once_begin_session(mb_win_init_once_session *s,mb_win_init_once *o,mb_pe_module *m){
    if(!s||!o||!m)return MB_WIN_INIT_ONCE_INVALID_ARGUMENT;
    if(s->active||s->pin.owner||s->pin.next||s->module||s->next||m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_INIT_ONCE_BAD_STATE;
    for(mb_win_init_once_session *p=sessions;p;p=p->next)if(p==s||p->module==m)return MB_WIN_INIT_ONCE_BAD_STATE;
    if(sessions&&installed!=o)return MB_WIN_INIT_ONCE_BUSY;
    mb_win_init_once_status status=mb_win_init_once_acquire_pin(o,&s->pin);if(status)return status;
    if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_INIT_ONCE_OK;
}
mb_win_init_once_status mb_win_init_once_end_session(mb_win_init_once_session *s){
    if(!s)return MB_WIN_INIT_ONCE_INVALID_ARGUMENT;mb_win_init_once_session **p=find(s);
    if(!*p||!s->active)return MB_WIN_INIT_ONCE_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_INIT_ONCE_BUSY;
    mb_win_init_once_status status=mb_win_init_once_release_pin(&s->pin);if(status)return status;
    *p=s->next;*s=(mb_win_init_once_session){0};if(!sessions)installed=NULL;return MB_WIN_INIT_ONCE_OK;
}
static WIN64 int32_t begin_initialize(mb_win_once *o,uint32_t flags,int32_t *pending,void **context){return mb_win_init_once_begin(installed,o,flags,pending,context);}
static WIN64 int32_t complete_initialize(mb_win_once *o,uint32_t flags,void *context){return mb_win_init_once_complete(installed,o,flags,context);}
static int match(mb_pe_string v,const char *s,size_t n,int fold){if(v.size!=n)return 0;for(size_t i=0;i<n;++i){unsigned char c=(unsigned char)v.data[i];if(fold&&c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)s[i])return 0;}return 1;}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_init_once_bind(void *context,const mb_pe_import *i,uint64_t *out){
    mb_win_init_once_session *s=context;
    if(!s||!*find(s)||!s->active||!i||!out||(!i->library.data&&i->library.size)||(!i->name.data&&i->name.size)||(s->module->state!=MB_PE_MODULE_LOADING&&s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
    if(i->by_ordinal||!MATCH(i->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    if(MATCH(i->name,"InitOnceBeginInitialize",0))*out=(uint64_t)(uintptr_t)&begin_initialize;
    else if(MATCH(i->name,"InitOnceComplete",0))*out=(uint64_t)(uintptr_t)&complete_initialize;
    else return MB_PE_BIND_UNRESOLVED;
    return MB_PE_BIND_BOUND;
}
