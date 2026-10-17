/* Host integration test for the proposed CreateMutexA adapter (worker-
 * createmutexa). Compiles the PATCHED mb_win_mutex_abi.c against the
 * unchanged canonical mb_win_mutex.c/mb_win_threads.c on an x86_64 host
 * (ms_abi + Rosetta). Verifies: ANSI->UTF-16 zero-extension (including a
 * non-ASCII byte, which fails if the conversion ever sign-extends), same-
 * object identity between CreateMutexA and CreateMutexW names, case
 * folding, Local\ prefix parity, NULL/empty unnamed rules, MAX_PATH bound
 * with the W path's ERROR_FILENAME_EXCED_RANGE, backslash refusal 87, and
 * LastError preservation on success. */
#include "mb_win_mutex_abi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK failed at %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
#define WIN __attribute__((ms_abi))
typedef void*(WIN *create_a_fn)(void*,int32_t,const char*);
typedef void*(WIN *create_w_fn)(void*,int32_t,const uint16_t*);
typedef void*(WIN *open_w_fn)(uint32_t,int32_t,const uint16_t*);
typedef uint32_t(WIN *wait_fn)(void*,uint32_t);
typedef int32_t(WIN *release_fn)(void*);
static mb_win_mutex owner;
static mb_win_thread_domain domain;
static create_a_fn mu_create_a;
static create_w_fn mu_create_w;
static open_w_fn mu_open_w;
static wait_fn mu_wait;
static release_fn mu_release;
static const uint32_t MARKER=0x0ddc0ffe;
static void *bind2(mb_win_mutex_session *s,const char *name){mb_pe_import i={0};i.library=(mb_pe_string){"KeRnEl32.DlL",12};i.name=(mb_pe_string){name,strlen(name)};uint64_t address=0;CHECK(mb_win_mutex_bind(s,&i,&address)==MB_PE_BIND_BOUND);return (void*)(uintptr_t)address;}
static void enter(void){CHECK(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);*mb_win_threads_last_error(&domain)=MARKER;}
static void leave(void){CHECK(*mb_win_threads_last_error(&domain)==MARKER);CHECK(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);}
static uint32_t last_error(void){return *mb_win_threads_last_error(&domain);}
static void clear_last_error(void){*mb_win_threads_last_error(&domain)=MARKER;}
static void close_ok(uint64_t handle){CHECK(mb_win_mutex_close_handle(&owner,handle)==1&&last_error()==MARKER);}
static const uint16_t w_single[]={'s','i','n','g','l','e','.','o','b','j',0};
static const uint16_t w_folded[]={'f','o','l','d','.','C','A','S','E',0};
static const uint16_t w_80tail[]={0x80,'t','a','i','l',0};
static const uint16_t w_ff80tail[]={0xFF80,'t','a','i','l',0};
static const uint16_t w_localpfx[]={'l','o','c','a','l','\\','P','F','X',0};
static const uint16_t w_nosuch[]={'n','o','s','u','c','h',0};
int main(void){
    alarm(30); /* never-hang guard */
    CHECK(mb_win_threads_open(&domain,NULL)==MB_WIN_THREADS_OK);enter();
    mb_win_mutex_config config={&domain,10};
    CHECK(mb_win_mutex_open(&owner,&config,NULL)==MB_WIN_MUTEX_OK);
    mb_pe_module module={0};mb_win_mutex_session session={0};
    CHECK(mb_win_mutex_begin_session(&session,&owner,&module)==MB_WIN_MUTEX_OK);module.state=MB_PE_MODULE_ACTIVE;
    mu_create_a=bind2(&session,"CreateMutexA");
    mu_create_w=bind2(&session,"CreateMutexW");
    mu_open_w=bind2(&session,"OpenMutexW");
    mu_release=bind2(&session,"ReleaseMutex");
    mu_wait=bind2(&session,"WaitForSingleObject");
    /* Binder rejects wrong-case import names (import match is exact). */
    {mb_pe_import i={0};i.library=(mb_pe_string){"kernel32.dll",12};i.name=(mb_pe_string){"createmytexA",12};uint64_t address=0;
     CHECK(mb_win_mutex_bind(&session,&i,&address)==MB_PE_BIND_UNRESOLVED);}

    /* PASS 1: ASCII name -> UTF-16 -> same object as the W entry point. */
    clear_last_error();
    void *hA=mu_create_a(NULL,0,"single.obj");
    CHECK(hA&&last_error()==MARKER);
    clear_last_error();
    void *hW=mu_create_w(NULL,0,w_single);
    CHECK(hW&&last_error()==183); /* ERROR_ALREADY_EXISTS: same folded name */
    clear_last_error(); /* success preserves LastError: neutralize the 183 */
    CHECK(mu_wait(hA,0xffffffffu)==0u&&last_error()==MARKER); /* grant on the A handle */
    CHECK(mu_wait(hW,30)==0x102u&&last_error()==MARKER); /* owner re-wait on the SAME object */
    CHECK(mu_release(hA)==1&&last_error()==MARKER);
    CHECK(mu_wait(hW,0)==0u&&last_error()==MARKER); /* unowned again: immediate grant via W handle */
    CHECK(mu_release(hW)==1&&last_error()==MARKER);
    close_ok((uint64_t)(uintptr_t)hA);close_ok((uint64_t)(uintptr_t)hW);

    /* PASS 2: case folding across and within the A entry point. */
    clear_last_error();
    void *f1=mu_create_a(NULL,0,"fold.Case");CHECK(f1&&last_error()==MARKER);
    void *f2=mu_create_a(NULL,0,"FOLD.case");CHECK(f2&&last_error()==183);
    clear_last_error(); /* success preserves LastError: neutralize the 183 */
    void *f3=mu_open_w(0,0,w_folded);CHECK(f3&&last_error()==MARKER); /* W sees the A-created name */
    clear_last_error();
    close_ok((uint64_t)(uintptr_t)f1);close_ok((uint64_t)(uintptr_t)f2);close_ok((uint64_t)(uintptr_t)f3);
    clear_last_error();
    CHECK(!mu_open_w(0,0,w_folded)&&last_error()==2); /* destroyed with its last handle */
    clear_last_error();
    CHECK(!mu_open_w(0,0,w_nosuch)&&last_error()==2); /* unknown name */

    /* PASS 3: non-ASCII ANSI byte 0x80 zero-extends to U+0080 (a signed
     * cast would produce 0xFF80 and must NOT resolve). */
    clear_last_error();
    void *n1=mu_create_a(NULL,0,"\x80""tail");CHECK(n1&&last_error()==MARKER);
    void *n2=mu_open_w(0,0,w_80tail);CHECK(n2&&last_error()==MARKER);
    clear_last_error();
    CHECK(!mu_open_w(0,0,w_ff80tail)&&last_error()==2);
    clear_last_error();
    close_ok((uint64_t)(uintptr_t)n1);close_ok((uint64_t)(uintptr_t)n2);

    /* PASS 4: Local\ prefix parity between A and W. */
    clear_last_error();
    void *p1=mu_create_a(NULL,0,"Local\\pfx");CHECK(p1&&last_error()==MARKER);
    void *p2=mu_create_w(NULL,0,w_localpfx);CHECK(p2&&last_error()==183);
    clear_last_error();
    close_ok((uint64_t)(uintptr_t)p1);close_ok((uint64_t)(uintptr_t)p2);

    /* PASS 5: NULL and empty names create unnamed objects. */
    clear_last_error();
    void *u1=mu_create_a(NULL,0,NULL);CHECK(u1&&last_error()==MARKER);
    void *u2=mu_create_a(NULL,0,"");CHECK(u2&&last_error()==MARKER);
    close_ok((uint64_t)(uintptr_t)u1);close_ok((uint64_t)(uintptr_t)u2);

    /* PASS 6: MAX_PATH bound parity with the W path (260 ok, 261 fails
     * ERROR_FILENAME_EXCED_RANGE from the wide path itself). */
    static char name260[261],name261[262];
    memset(name260,'a',260);memset(name261,'a',261);
    clear_last_error();
    void *m1=mu_create_a(NULL,0,name260);CHECK(m1&&last_error()==MARKER);
    close_ok((uint64_t)(uintptr_t)m1);
    clear_last_error();
    void *m2=mu_create_a(NULL,0,name261);CHECK(!m2&&last_error()==206);
    clear_last_error();
    CHECK(!mu_create_a(NULL,0,"bad\\name")&&last_error()==87); /* backslash refusal parity */

    /* PASS 7: bInitialOwner. */
    clear_last_error();
    void *o1=mu_create_a(NULL,1,"ownme");CHECK(o1&&last_error()==MARKER);
    CHECK(mu_wait(o1,0)==0x102u&&last_error()==MARKER); /* already owned */
    CHECK(mu_release(o1)==1&&last_error()==MARKER);
    close_ok((uint64_t)(uintptr_t)o1);

    /* Teardown: EMPTY module, end session, close owner and domain. */
    module.state=MB_PE_MODULE_EMPTY;
    CHECK(mb_win_mutex_end_session(&session)==MB_WIN_MUTEX_OK);
    CHECK(mb_win_mutex_close(&owner,NULL)==MB_WIN_MUTEX_OK);
    leave();
    CHECK(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_OK);
    puts("PASS: CreateMutexA adapter (7 passes: identity, folding, zero-extend, Local\\ prefix, unnamed, MAX_PATH/backslash, initial-owner)");
    return 0;
}
