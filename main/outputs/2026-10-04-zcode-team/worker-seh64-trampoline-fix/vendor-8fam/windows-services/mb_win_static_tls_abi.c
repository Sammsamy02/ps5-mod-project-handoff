/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_srw_abi.c; that original file is unchanged). */
#include "mb_win_static_tls_abi.h"
#if !defined(__x86_64__)
#error "Static TLS Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
static mb_win_static_tls *installed;
static mb_win_static_tls_session *sessions;
static mb_win_static_tls_session **find(mb_win_static_tls_session *s){mb_win_static_tls_session **p=&sessions;while(*p&&*p!=s)p=&(*p)->next;return p;}
mb_win_static_tls_status mb_win_static_tls_begin_session(mb_win_static_tls_session *s,mb_win_static_tls *o,mb_pe_module *m){
    if(!s||!o||!m)return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    if(s->active||s->pin.owner||s->pin.next||s->module||s->next||m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_STATIC_TLS_BAD_STATE;
    for(mb_win_static_tls_session *p=sessions;p;p=p->next)if(p==s||p->module==m)return MB_WIN_STATIC_TLS_BAD_STATE;
    if(sessions&&installed!=o)return MB_WIN_STATIC_TLS_BUSY;
    mb_win_static_tls_status status=mb_win_static_tls_acquire_pin(o,&s->pin);if(status)return status;
    if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_STATIC_TLS_OK;
}
mb_win_static_tls_status mb_win_static_tls_end_session(mb_win_static_tls_session *s){
    if(!s)return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;mb_win_static_tls_session **p=find(s);
    if(!*p||!s->active)return MB_WIN_STATIC_TLS_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_STATIC_TLS_BUSY;
    mb_win_static_tls_status status=mb_win_static_tls_release_pin(&s->pin);if(status)return status;
    *p=s->next;*s=(mb_win_static_tls_session){0};if(!sessions)installed=NULL;return MB_WIN_STATIC_TLS_OK;
}
static mb_win_static_tls_module *session_module(mb_win_static_tls_session *s){
    /* Registered record for the session's mapped image (mb_pe_module::image);
     * NULL when the module carries no registered TLS directory. */
    return (s&&s->active&&installed)?mb_win_static_tls_module_for(installed,s->module->image):0;
}
void *mb_win_static_tls_import(uint32_t index,uint64_t offset){
    if(!installed)return 0;
    mb_win_thread_domain *domain=mb_win_static_tls_domain(installed);
    void *block=mb_win_threads_tls_get(domain,index);
    return block?(void*)((uint8_t*)block+offset):0;
}
static WIN64 void *tls_import_thunk(uint32_t index,uint64_t offset){
    return mb_win_static_tls_import(index,offset);
}
static int match(mb_pe_string v,const char *s,size_t n){if(v.size!=n)return 0;for(size_t i=0;i<n;++i)if((unsigned char)v.data[i]!=(unsigned char)s[i])return 0;return 1;}
#define MATCH(v,s) match((v),(s),sizeof(s)-1)
mb_pe_bind_result mb_win_static_tls_bind(void *context,const mb_pe_import *i,uint64_t *out){
    mb_win_static_tls_session *s=context;
    if(!s||!*find(s)||!s->active||!i||!out||(!i->library.data&&i->library.size)||(!i->name.data&&i->name.size)||(s->module->state!=MB_PE_MODULE_LOADING&&s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
    if(i->by_ordinal)return MB_PE_BIND_UNRESOLVED;
    mb_win_static_tls_module *m=session_module(s);
    if(MATCH(i->name,"__tls_index")||MATCH(i->name,"_tls_index")||MATCH(i->name,"__tls_start")||MATCH(i->name,"_tls_start")||MATCH(i->name,"__tls_end")||MATCH(i->name,"_tls_end")||MATCH(i->name,"__tls_import")||MATCH(i->name,"_imp___tls_import")){
        if(!m)return MB_PE_BIND_ERROR; /* name requested, record unregistered */
        if(MATCH(i->name,"__tls_index")||MATCH(i->name,"_tls_index"))*out=(uint64_t)(uintptr_t)m->index_slot;
        else if(MATCH(i->name,"__tls_start")||MATCH(i->name,"_tls_start"))*out=(uint64_t)(uintptr_t)m->raw_data;
        else if(MATCH(i->name,"__tls_end")||MATCH(i->name,"_tls_end"))*out=(uint64_t)(uintptr_t)(m->raw_data+m->raw_bytes);
        else *out=(uint64_t)(uintptr_t)&tls_import_thunk;
        return MB_PE_BIND_BOUND;
    }
    return MB_PE_BIND_UNRESOLVED; /* __tls_array and unknown names fall through */
}
