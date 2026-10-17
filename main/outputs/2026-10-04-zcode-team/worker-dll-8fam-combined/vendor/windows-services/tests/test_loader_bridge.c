#include "../mb_win_loader_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void lock(void *p){(void)p;}
static int acquire(void *p,const mb_pe_loader_catalog_entry *e,const void **f,size_t *n,void **t){(void)p;(void)e;(void)f;(void)n;(void)t;return -1;}
static int drop(void *p,void *t){(void)p;(void)t;return 0;}
static void *allocate(void *p,size_t n,size_t a){(void)p;(void)n;(void)a;return NULL;}
static int protect(void *p,void *b,size_t o,size_t n,unsigned r){(void)p;(void)b;(void)o;(void)n;(void)r;return 0;}
static int region(void *p,void *b,size_t n){(void)p;(void)b;(void)n;return 0;}
int main(void){
 mb_win_thread_domain domain={0};int native=0;CHECK(mb_win_threads_open(&domain,&native)==MB_WIN_THREADS_OK);
 mb_pe_export_lease imports[1]={0},procs[1]={0};mb_pe_loader_record record={0};
 mb_pe_loader_catalog_entry catalog={{"fixture.dll",11},NULL,NULL,imports,1,procs,1};
 mb_pe_loader_config options={.catalog=&catalog,.catalog_count=1,.records=&record,
 .memory={NULL,16384,allocate,protect,region,region},.source={NULL,acquire,drop}};
 mb_pe_loader owner={0};mb_pe_loader_error error={0};
 CHECK(mb_pe_loader_open(&owner,&options,&error)==MB_PE_LOADER_OK);
 mb_win_loader_bridge_config config={&owner,&domain,NULL,lock,lock};
 mb_pe_module module={0},other={0};mb_win_loader_bridge_session s={0},t={0};
 CHECK(mb_win_loader_bridge_begin(&s,&config,&module)==MB_WIN_LOADER_BRIDGE_OK);
 CHECK(mb_pe_loader_close(&owner,&error)==MB_PE_LOADER_BUSY && mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_BUSY);
 CHECK(mb_win_loader_bridge_begin(&t,&config,&module)==MB_WIN_LOADER_BRIDGE_BAD_STATE);
 CHECK(mb_win_loader_bridge_begin(&t,&config,&other)==MB_WIN_LOADER_BRIDGE_OK);
 mb_win_loader_bridge_session copy=s;CHECK(mb_win_loader_bridge_end(&copy)==MB_WIN_LOADER_BRIDGE_BAD_STATE);
 mb_pe_import import={{"KERNEL32.dll",12},{"LoadLibraryA",12},0,0,0,0};uint64_t value=9;
 CHECK(mb_win_loader_bridge_bind(&s,&import,&value)==MB_PE_BIND_ERROR && value==9);
 module.state=MB_PE_MODULE_LOADING;
 CHECK(mb_win_loader_bridge_bind(&s,&import,&value)==MB_PE_BIND_BOUND && value!=9);
 CHECK(mb_win_loader_bridge_bind(&copy,&import,&value)==MB_PE_BIND_ERROR);
 import.name=(mb_pe_string){"loadlibrarya",12};value=9;
 CHECK(mb_win_loader_bridge_bind(&s,&import,&value)==MB_PE_BIND_UNRESOLVED && value==9);
 import.name=(mb_pe_string){NULL,1};CHECK(mb_win_loader_bridge_bind(&s,&import,&value)==MB_PE_BIND_ERROR);
 CHECK(mb_win_loader_bridge_end(&s)==MB_WIN_LOADER_BRIDGE_BUSY);
 module.state=MB_PE_MODULE_CLEANUP;CHECK(mb_win_loader_bridge_end(&s)==MB_WIN_LOADER_BRIDGE_BUSY);
 CHECK(mb_win_loader_bridge_end(&t)==MB_WIN_LOADER_BRIDGE_OK);
 module.state=MB_PE_MODULE_EMPTY;CHECK(mb_win_loader_bridge_end(&s)==MB_WIN_LOADER_BRIDGE_OK);
 CHECK(mb_pe_loader_close(&owner,&error)==MB_PE_LOADER_OK && mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);
 puts("Loader bridge stable-session, original-pin and cleanup-lifetime checks passed");return 0;
}
