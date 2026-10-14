#include "../mb_win_heap_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
struct backend {int32_t error; unsigned locked,locks;};
static void *alloc(void *c,size_t n) {(void)c;return malloc(n);}
static void release(void *c,void *p) {(void)c;free(p);}
static int32_t *slot(void *c) {return &((struct backend *)c)->error;}
static void lock(void *c) {struct backend *b=c;if(b->locked) abort();b->locked=1;++b->locks;}
static void unlock(void *c) {struct backend *b=c;if(!b->locked) abort();b->locked=0;}
int main(void) {
 struct backend b={0}; mb_win_heap h={0}, other={0}; mb_pe_module m={0}, n={0};
 mb_win_heap_ops ops={&b,alloc,release,slot};
 mb_win_heap_bridge_config config={&h,&b,lock,unlock};
 mb_win_heap_bridge_session s={0}, t={0};
 mb_pe_import imp={{"api-ms-win-crt-heap-l1-1-0.dll",30},{"malloc",6},0,0,0,0};
 uint64_t value=9;
 CHECK(mb_win_heap_open(&h,&ops)==MB_WIN_HEAP_OK);
 CHECK(mb_win_heap_open(&other,&ops)==MB_WIN_HEAP_OK);
 CHECK(mb_win_process_bind(&s,&imp,&value)==MB_PE_BIND_ERROR && value==9);
 CHECK(mb_win_heap_bridge_begin(&s,&config,&m)==MB_WIN_HEAP_BRIDGE_OK && h.pin_count==1);
 CHECK(mb_win_heap_close(&h)==MB_WIN_HEAP_BUSY);
 CHECK(mb_win_heap_bridge_begin(&t,&config,&m)==MB_WIN_HEAP_BRIDGE_BAD_STATE);
 mb_win_heap_bridge_config changed=config;changed.heap=&other;
 CHECK(mb_win_heap_bridge_begin(&t,&changed,&n)==MB_WIN_HEAP_BRIDGE_BUSY && other.pin_count==0);
 CHECK(mb_win_heap_bridge_begin(&t,&config,&n)==MB_WIN_HEAP_BRIDGE_OK && h.pin_count==2);
 mb_win_heap_bridge_session copied=s;
 CHECK(mb_win_heap_bridge_end(&copied)==MB_WIN_HEAP_BRIDGE_BAD_STATE);
 CHECK(mb_win_process_bind(&s,&imp,&value)==MB_PE_BIND_ERROR);
 m.state=MB_PE_MODULE_LOADING;
 CHECK(mb_win_heap_bridge_bind(&s,&imp,&value)==MB_PE_BIND_BOUND && value!=9);
 imp.library=(mb_pe_string){"api-ms-win-crt-string-l1-1-0.dll",32};imp.name=(mb_pe_string){"strlen",6};
 value=9;CHECK(mb_win_heap_bridge_bind(&s,&imp,&value)==MB_PE_BIND_UNRESOLVED && value==9);
 CHECK(mb_win_process_bind(&s,&imp,&value)==MB_PE_BIND_BOUND && value!=9);
 imp.name=(mb_pe_string){"Strlen",6};value=9;
 CHECK(mb_win_process_bind(&s,&imp,&value)==MB_PE_BIND_UNRESOLVED && value==9);
 imp.name=(mb_pe_string){NULL,1};CHECK(mb_win_process_bind(&s,&imp,&value)==MB_PE_BIND_ERROR);
 m.state=MB_PE_MODULE_CLEANUP;
 CHECK(mb_win_heap_bridge_end(&s)==MB_WIN_HEAP_BRIDGE_BUSY && h.pin_count==2);
 CHECK(mb_win_heap_bridge_end(&t)==MB_WIN_HEAP_BRIDGE_OK && h.pin_count==1);
 m.state=MB_PE_MODULE_EMPTY;lock(&b);void *p=mb_win_heap_malloc(&h,7);unlock(&b);CHECK(p);
 CHECK(mb_win_heap_bridge_end(&s)==MB_WIN_HEAP_BRIDGE_BUSY && s.active && h.pin_count==1);
 lock(&b);mb_win_heap_free(&h,p);unlock(&b);
 CHECK(mb_win_heap_bridge_end(&s)==MB_WIN_HEAP_BRIDGE_OK && !s.active && !h.pin_count);
 CHECK(mb_win_heap_bridge_end(&s)==MB_WIN_HEAP_BRIDGE_BAD_STATE);
 CHECK(mb_win_heap_close(&h)==MB_WIN_HEAP_OK && mb_win_heap_close(&other)==MB_WIN_HEAP_OK && !b.locked && b.locks>0);
 puts("heap bridge session/binder/lifetime checks passed");return 0;
}
