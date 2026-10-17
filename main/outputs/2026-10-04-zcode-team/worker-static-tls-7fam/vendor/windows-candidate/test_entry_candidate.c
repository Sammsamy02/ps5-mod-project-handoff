#include "sdk_driver.h"
#include "owned_profile.h"
#if MB_OWNED_WORKERS
#include "tests/worker-state.h"
#include "../windows-services/mb_win_workers.h"
mb_win_worker_status mb_test_worker_leave(mb_win_workers *owner){return mb_win_workers_leave_call(owner);}
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned release_fault,release_calls,sleeps;
static const char *report_path;
int mb_pe_ps5_test_pagesize(void){return 16384;}
void *mb_pe_ps5_test_map(void *p,size_t n,int rights,int flags,int fd,int64_t offset){
 if(p || rights!=3 || flags!=0x1002 || fd!=-1 || offset){errno=EINVAL;return MAP_FAILED;}
 return mmap(NULL,n,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
}
int mb_pe_ps5_test_protect(const void *p,size_t n,int rights){return mprotect((void *)p,n,rights);}
int mb_pe_ps5_test_unmap(void *p,size_t n){
 ++release_calls;if(release_fault && release_calls>=(MB_OWNED_REJECT_ATTACH?1u:2u) && sleeps<2){errno=EAGAIN;return -1;}return munmap(p,n);
}
static void *wrong_thread_cleanup(void *value){*(int *)value=mb_owned_ps5_cleanup();return NULL;}
unsigned mb_test_sleep(unsigned seconds){
 if(seconds!=1)abort();++sleeps;
 mb_owned_dll_report r;mb_owned_ps5_result(&r);
 if(r.entry_attach_calls[0]!=1 || r.entry_attach_calls[1]!=!MB_OWNED_REJECT_ATTACH || r.entry_detach_calls[0]!=1 || r.entry_detach_calls[1]!=!MB_OWNED_REJECT_ATTACH || r.entry_contract_errors)abort();
#if MB_OWNED_CLOCK
 if(!r.clock_enabled || r.clock_session_begins[0]!=1 || r.clock_session_ends[0]!=MB_OWNED_REJECT_WORKER_START ||
    r.clock_session_begins[1]!=!MB_OWNED_REJECT_ATTACH || r.clock_session_ends[1]!=(!MB_OWNED_REJECT_ATTACH && !MB_OWNED_REJECT_WORKER_START))abort();
#endif
 if(sleeps==1){pthread_t thread;int result=0;if(pthread_create(&thread,NULL,wrong_thread_cleanup,&result) || pthread_join(thread,NULL) || !result)abort();
  mb_owned_dll_report after;mb_owned_ps5_result(&after);
  if(after.entry_detach_calls[0]!=1 || after.source_releases[0]!=r.source_releases[0] || after.failure_stage!=r.failure_stage)abort();}
 int fd=open(report_path,O_RDONLY);if(fd<0)abort();char bytes[16384];ssize_t n=read(fd,bytes,sizeof(bytes)-1);if(n<0)abort();bytes[n]=0;close(fd);
 if(!strstr(bytes,"\"phase\":4") || !strstr(bytes,"\"pending\":1") || sleeps>2)abort();return 0;
}
int main(int argc,char **argv){
 CHECK(argc==3);report_path=argv[2];release_fault=strstr(argv[1],"release")!=NULL;
 CHECK(mb_owned_ps5_execute(report_path,-1)==(MB_OWNED_REJECT_ATTACH || MB_OWNED_REJECT_WORKER_START || release_fault));
 mb_owned_dll_report r;mb_owned_ps5_result(&r);
 CHECK(r.entry_enabled && !r.entry_contract_errors && !r.cleanup_pending);
 CHECK(r.entry_attach_calls[0]==1 && r.entry_detach_calls[0]==1);
 CHECK(r.entry_attach_result[0]==(MB_OWNED_REJECT_ATTACH?0:7) && !r.entry_detach_result[0]);
 CHECK(r.entry_attach_calls[1]==(MB_OWNED_REJECT_ATTACH?0u:1u) && r.entry_detach_calls[1]==r.entry_attach_calls[1]);
 CHECK(r.dll_calls_started==(MB_OWNED_REJECT_ATTACH?0u:MB_OWNED_REJECT_WORKER_START?1u:3u) && r.dll_calls_returned==r.dll_calls_started);
 CHECK(r.completed==(!MB_OWNED_REJECT_ATTACH && !MB_OWNED_REJECT_WORKER_START) && r.results[0]==(MB_OWNED_REJECT_ATTACH?0u:86u) && r.results[1]==(MB_OWNED_REJECT_ATTACH?0u:93u));
 CHECK(r.failure_stage==(MB_OWNED_REJECT_ATTACH?5u:MB_OWNED_REJECT_WORKER_START?24u:release_fault?14u:0u));
 if(MB_OWNED_REJECT_WORKER_START)CHECK(r.native_status==0xffffffd8u);
 if(MB_OWNED_REJECT_ATTACH)CHECK(r.first_module_cause==MB_PE_MODULE_ENTRY_REJECTED);
 CHECK(sleeps==(release_fault?2u:0u));
 CHECK(r.source_releases[0]==1 && r.source_releases[1]==!MB_OWNED_REJECT_ATTACH);
 if(!MB_OWNED_REJECT_ATTACH)CHECK(r.consumer_imports==(MB_OWNED_CLOCK?18u:16u)+(MB_OWNED_WORKERS?5u:0u)+(MB_OWNED_SYNC?21u:0u)+(MB_OWNED_INIT_ONCE?2u:0u)+(MB_OWNED_MUTEX?5u:0u)+(MB_OWNED_FILEIO?10u:0u)+(MB_OWNED_CRT_STARTUP?7u:0u)+(MB_OWNED_MODULE_ENUM?5u:0u)+(MB_OWNED_VM?5u:0u)+(MB_OWNED_STATIC_TLS?4u:0u) && r.helper_imports==(MB_OWNED_CLOCK?10u:8u) && r.entry_attach_result[1]==7 && !r.entry_detach_result[1]);
 CHECK(r.sync_enabled==MB_OWNED_SYNC);
 CHECK(r.init_once_enabled==MB_OWNED_INIT_ONCE);
 CHECK(r.mutex_enabled==MB_OWNED_MUTEX);
 CHECK(r.fileio_enabled==MB_OWNED_FILEIO);
 CHECK(r.crt_startup_enabled==MB_OWNED_CRT_STARTUP);
 CHECK(r.module_enum_enabled==MB_OWNED_MODULE_ENUM);
 CHECK(r.vm_enabled==MB_OWNED_VM);
#if MB_OWNED_CLOCK
 CHECK(r.clock_enabled && r.clock_session_begins[0]==1 && r.clock_session_ends[0]==1);
 CHECK(r.clock_session_begins[1]==!MB_OWNED_REJECT_ATTACH && r.clock_session_ends[1]==!MB_OWNED_REJECT_ATTACH);
#endif
#if MB_OWNED_WORKERS
 CHECK(r.worker_enabled && r.worker_calls_started==(MB_OWNED_REJECT_ATTACH?0u:MB_OWNED_REJECT_WORKER_START?1u:2u+(MB_OWNED_FILEIO?2u:0u)) && r.worker_calls_returned==r.worker_calls_started);
 if(!MB_OWNED_REJECT_ATTACH && !MB_OWNED_REJECT_WORKER_START){
  CHECK(r.worker_begin[0] && r.worker_begin[1] && r.worker_begin[2]==258 && r.worker_begin[3]==259);
  CHECK(r.worker_result[0]==MB_WORKER_EXPECTED_EXIT && r.worker_result[1]==MB_WORKER_PARAMETER_COOKIE && r.worker_result[2]==MB_WORKER_LAST_ERROR && !r.worker_result[3]);
 }
#else
 CHECK(!r.worker_enabled && !r.worker_calls_started && !r.worker_calls_returned);
#endif
 CHECK(mb_owned_ps5_execute(report_path,-1)!=0);
 puts(MB_OWNED_REJECT_ATTACH?"FALSE DllMain attach immediately detaches once and retires with no exported calls":release_fault?"Both DllMain detach callbacks run once before retained release retries":"Real DllMain attach gates 86/93 exports; both detach callbacks use live heap/TLS services");return 0;
}
