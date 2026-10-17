#include "mb_win_address_wait_abi.h"
#if !defined(__x86_64__)
#error "Address wait Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void*)==8 && sizeof(size_t)==8 && sizeof(int32_t)==4 && sizeof(uint32_t)==4,"Windows wait widths");
static mb_win_address_wait *installed;
static mb_win_address_wait_session *sessions;
static mb_win_address_wait_session **find(mb_win_address_wait_session *s){mb_win_address_wait_session **at=&sessions;while(*at && *at!=s)at=&(*at)->next;return at;}
mb_win_address_wait_status mb_win_address_wait_begin(mb_win_address_wait_session *s,mb_win_address_wait *o,mb_pe_module *m){
 if(!s || !o || !m)return MB_WIN_ADDRESS_WAIT_INVALID_ARGUMENT;
 if(s->active || s->pin.owner || s->pin.next || s->module || s->next || m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_ADDRESS_WAIT_BAD_STATE;
 for(mb_win_address_wait_session *p=sessions;p;p=p->next)if(p==s || p->module==m)return MB_WIN_ADDRESS_WAIT_BAD_STATE;
 if(sessions && installed!=o)return MB_WIN_ADDRESS_WAIT_BUSY;
 mb_win_address_wait_status status=mb_win_address_wait_acquire(o,&s->pin);if(status)return status;
 if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_ADDRESS_WAIT_OK;
}
mb_win_address_wait_status mb_win_address_wait_end(mb_win_address_wait_session *s){
 if(!s)return MB_WIN_ADDRESS_WAIT_INVALID_ARGUMENT;mb_win_address_wait_session **at=find(s);
 if(!*at || !s->active)return MB_WIN_ADDRESS_WAIT_BAD_STATE;
 if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_ADDRESS_WAIT_BUSY;
 mb_win_address_wait_status status=mb_win_address_wait_release_pin(&s->pin);if(status)return status;
 *at=s->next;*s=(mb_win_address_wait_session){0};if(!sessions)installed=NULL;return MB_WIN_ADDRESS_WAIT_OK;
}
static WIN64 int32_t wait_on_address(volatile void *address,void *compare,size_t size,uint32_t millis){return mb_win_address_wait_wait(installed,address,compare,size,millis);}
static WIN64 void wake_by_address_single(void *address){mb_win_address_wait_wake_single(installed,address);}
static WIN64 void wake_by_address_all(void *address){mb_win_address_wait_wake_all(installed,address);}
static int match(mb_pe_string view,const char *s,size_t n,int fold){if(view.size!=n)return 0;for(size_t i=0;i<n;++i){unsigned char c=(unsigned char)view.data[i];if(fold && c>='A' && c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)s[i])return 0;}return 1;}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_address_wait_bind(void *context,const mb_pe_import *i,uint64_t *out){
 mb_win_address_wait_session *s=context;
 if(!s || !*find(s) || !s->active || !i || !out || (!i->library.data && i->library.size) || (!i->name.data && i->name.size) ||
    (s->module->state!=MB_PE_MODULE_LOADING && s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
 if(i->by_ordinal || !MATCH(i->library,"api-ms-win-core-synch-l1-2-0.dll",1))return MB_PE_BIND_UNRESOLVED;
 if(MATCH(i->name,"WaitOnAddress",0))*out=(uint64_t)(uintptr_t)&wait_on_address;
 else if(MATCH(i->name,"WakeByAddressSingle",0))*out=(uint64_t)(uintptr_t)&wake_by_address_single;
 else if(MATCH(i->name,"WakeByAddressAll",0))*out=(uint64_t)(uintptr_t)&wake_by_address_all;
 else return MB_PE_BIND_UNRESOLVED;
 return MB_PE_BIND_BOUND;
}
