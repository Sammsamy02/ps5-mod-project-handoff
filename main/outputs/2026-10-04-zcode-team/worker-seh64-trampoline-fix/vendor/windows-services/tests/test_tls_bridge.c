#include "../mb_win_tls_bridge.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    mb_win_thread_domain domain={0},other={0};int native=0;
    mb_win_tls_bridge_session s={0},t={0};mb_pe_module m={0},n={0};
    mb_pe_import import={{"KERNEL32.dll",12},{"GetLastError",12},0,0,0,0};uint64_t value=9;
    CHECK(mb_win_threads_open(&domain,&native)==MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_open(&other,&native)==MB_WIN_THREADS_OK);
    CHECK(mb_win_tls_bridge_bind(&s,&import,&value)==MB_PE_BIND_ERROR && value==9);
    CHECK(mb_win_tls_bridge_begin(&s,&domain,&m)==MB_WIN_TLS_BRIDGE_OK);
    CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_BUSY);
    CHECK(mb_win_tls_bridge_begin(&t,&domain,&m)==MB_WIN_TLS_BRIDGE_BAD_STATE);
    CHECK(mb_win_tls_bridge_begin(&t,&other,&n)==MB_WIN_TLS_BRIDGE_BUSY);
    CHECK(mb_win_tls_bridge_begin(&t,&domain,&n)==MB_WIN_TLS_BRIDGE_OK);
    mb_win_tls_bridge_session copied=s;
    CHECK(mb_win_tls_bridge_end(&copied)==MB_WIN_TLS_BRIDGE_BAD_STATE);
    CHECK(mb_win_tls_bridge_bind(&s,&import,&value)==MB_PE_BIND_ERROR);
    m.state=MB_PE_MODULE_LOADING;
    CHECK(mb_win_tls_bridge_bind(&s,&import,&value)==MB_PE_BIND_BOUND && value!=9);
    CHECK(mb_win_tls_bridge_bind(&copied,&import,&value)==MB_PE_BIND_ERROR);
    import.library=(mb_pe_string){"kernel32.DLL",12};value=9;
    CHECK(mb_win_tls_bridge_bind(&s,&import,&value)==MB_PE_BIND_BOUND && value!=9);
    import.name=(mb_pe_string){"getlasterror",12};value=9;
    CHECK(mb_win_tls_bridge_bind(&s,&import,&value)==MB_PE_BIND_UNRESOLVED && value==9);
    import.name=(mb_pe_string){NULL,1};CHECK(mb_win_tls_bridge_bind(&s,&import,&value)==MB_PE_BIND_ERROR);
    import.name=(mb_pe_string){"GetLastError",12};
    mb_win_heap heap={0};mb_win_heap_ops hops={&domain,mb_win_threads_allocate,mb_win_threads_release,mb_win_threads_errno};
    mb_win_heap_bridge_config config={&heap,&domain,mb_win_threads_lock,mb_win_threads_unlock};
    mb_win_heap_bridge_session process={0};
    CHECK(mb_win_heap_open(&heap,&hops)==MB_WIN_HEAP_OK);
    m.state=MB_PE_MODULE_EMPTY;
    CHECK(mb_win_heap_bridge_begin(&process,&config,&m)==MB_WIN_HEAP_BRIDGE_OK);
    mb_win_runtime_bind_context context={&s,&process};m.state=MB_PE_MODULE_LOADING;
    CHECK(mb_win_runtime_bind(&context,&import,&value)==MB_PE_BIND_BOUND);
    mb_win_heap_bridge_session fake=process;context.process=&fake;value=9;
    CHECK(mb_win_runtime_bind(&context,&import,&value)==MB_PE_BIND_ERROR && value==9);
    context.process=&process;context.tls=&copied;
    CHECK(mb_win_runtime_bind(&context,&import,&value)==MB_PE_BIND_ERROR && value==9);
    context.tls=&t;n.state=MB_PE_MODULE_LOADING;
    CHECK(mb_win_runtime_bind(&context,&import,&value)==MB_PE_BIND_ERROR && value==9);
    context.tls=&s;import.library=(mb_pe_string){"api-ms-win-crt-runtime-l1-1-0.dll",33};import.name=(mb_pe_string){"_errno",6};
    CHECK(mb_win_runtime_bind(&context,&import,&value)==MB_PE_BIND_BOUND && value!=9);
    m.state=MB_PE_MODULE_CLEANUP;
    CHECK(mb_win_tls_bridge_end(&s)==MB_WIN_TLS_BRIDGE_BUSY && s.active);
    n.state=MB_PE_MODULE_EMPTY;CHECK(mb_win_tls_bridge_end(&t)==MB_WIN_TLS_BRIDGE_OK);
    CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_BUSY);
    m.state=MB_PE_MODULE_EMPTY;
    CHECK(mb_win_tls_bridge_end(&s)==MB_WIN_TLS_BRIDGE_OK && !s.active);
    CHECK(mb_win_heap_bridge_end(&process)==MB_WIN_HEAP_BRIDGE_OK);
    CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_OK);
    CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_close(&other,&native)==MB_WIN_THREADS_OK);
    puts("TLS bridge session, composed binder, and pin lifetime checks passed");return 0;
}
