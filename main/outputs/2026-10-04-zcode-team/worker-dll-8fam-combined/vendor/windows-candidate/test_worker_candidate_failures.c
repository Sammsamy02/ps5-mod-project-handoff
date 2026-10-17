/* Focused retained-admission regression. Only owned_runtime.c redirects
 * mb_win_workers_leave_call: the actual native worker admission/exit, PE
 * instructions, module code leases and loader cleanup all remain real. */
#include "sdk_driver.h"
#include "owned_profile.h"
#include "tests/worker-state.h"
#include "../windows-services/mb_win_workers.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#if !MB_OWNED_ENTRY || !MB_OWNED_WORKERS || MB_OWNED_REJECT_ATTACH
#error "This control requires the successful-entry worker profile"
#endif
#define REQUIRE(x) do { if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static struct {void *address;size_t size;unsigned live;} mappings[2];
static unsigned map_calls,release_calls,release_fault,release_injected;
static unsigned leave_calls,leave_injected,leave_retired,sleeps;
static pthread_t creator;
static mb_win_workers *retained_owner;
static const char *report_path;

static void completed_worker_calls(const mb_owned_dll_report *r){
 REQUIRE(r->worker_enabled && r->worker_calls_started==2 && r->worker_calls_returned==2);
 REQUIRE(r->dll_calls_started==1 && r->dll_calls_returned==1 && !r->completed);
 REQUIRE(r->results[0]==86 && r->results[1]==93);
 REQUIRE(r->worker_begin[0] && r->worker_begin[1] && r->worker_begin[2]==258 && r->worker_begin[3]==259);
 REQUIRE(r->worker_result[0]==MB_WORKER_EXPECTED_EXIT && r->worker_result[1]==MB_WORKER_PARAMETER_COOKIE);
 REQUIRE(r->worker_result[2]==MB_WORKER_LAST_ERROR && r->worker_result[3]==0);
}
static void original_failure(const mb_owned_dll_report *r){
 completed_worker_calls(r);
 REQUIRE(r->failure_stage==25 && r->native_status==MB_WIN_WORKERS_NATIVE_ERROR && !r->first_module_cause);
}

mb_win_worker_status mb_test_worker_leave(mb_win_workers *owner){
 REQUIRE(pthread_equal(pthread_self(),creator));
 ++leave_calls;
 if(leave_calls==2){
  mb_owned_dll_report r;mb_owned_ps5_result(&r);completed_worker_calls(&r);
  REQUIRE(!r.failure_stage && r.cleanup_pending && !r.entry_detach_calls[0] && !r.entry_detach_calls[1]);
  retained_owner=owner;leave_injected=1;
  /* Match real pthread_setspecific failure: no admission state is consumed. */
  return MB_WIN_WORKERS_NATIVE_ERROR;
 }
 if(leave_calls==3){
  mb_owned_dll_report r;mb_owned_ps5_result(&r);original_failure(&r);
  REQUIRE(owner==retained_owner && r.cleanup_pending && r.stage==MB_OWNED_STAGE_CLEANUP);
  REQUIRE(!r.entry_detach_calls[0] && !r.entry_detach_calls[1]);
 }
 REQUIRE(leave_calls<=3);
 mb_win_worker_status status=mb_win_workers_leave_call(owner);
 REQUIRE(status==MB_WIN_WORKERS_OK);
 if(leave_calls==3)leave_retired=1;
 return status;
}

int mb_pe_ps5_test_pagesize(void){return 16384;}
void *mb_pe_ps5_test_map(void *address,size_t size,int rights,int flags,int fd,int64_t offset){
 REQUIRE(!address && rights==3 && flags==0x1002 && fd==-1 && !offset && map_calls<2);
 void *mapped=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
 if(mapped!=MAP_FAILED){mappings[map_calls].address=mapped;mappings[map_calls].size=size;mappings[map_calls].live=1;++map_calls;}
 return mapped;
}
int mb_pe_ps5_test_protect(const void *address,size_t size,int rights){return mprotect((void *)address,size,rights);}
int mb_pe_ps5_test_unmap(void *address,size_t size){
 unsigned index=0;
 while(index<map_calls && mappings[index].address!=address)++index;
 REQUIRE(index<map_calls && mappings[index].live && mappings[index].size==size);
 REQUIRE(leave_retired); /* No image retires before the retained admission. */
 ++release_calls;
 if(release_fault && release_calls==2){release_injected=1;errno=EAGAIN;return -1;}
 int status=munmap(address,size);if(!status)mappings[index].live=0;return status;
}
static void *wrong_thread_cleanup(void *out){*(int *)out=mb_owned_ps5_cleanup();return NULL;}
unsigned mb_test_sleep(unsigned seconds){
 REQUIRE(seconds==1 && release_fault && release_injected && ++sleeps==1);
 mb_owned_dll_report r;mb_owned_ps5_result(&r);original_failure(&r);
 REQUIRE(r.cleanup_pending && leave_calls==3 && leave_retired);
 REQUIRE(r.entry_detach_calls[0]==1 && r.entry_detach_calls[1]==1 && !r.entry_contract_errors);
 REQUIRE(r.source_releases[0]==1 && r.source_releases[1]==0);
 pthread_t thread;int result=0;
 REQUIRE(!pthread_create(&thread,NULL,wrong_thread_cleanup,&result) && !pthread_join(thread,NULL) && result);
 mb_owned_dll_report after;mb_owned_ps5_result(&after);original_failure(&after);
 REQUIRE(after.entry_detach_calls[0]==1 && after.entry_detach_calls[1]==1 && after.source_releases[1]==0 && leave_calls==3);
 int fd=open(report_path,O_RDONLY);REQUIRE(fd>=0);
 char bytes[16384];ssize_t n=read(fd,bytes,sizeof(bytes)-1);REQUIRE(n>=0 && !close(fd));bytes[n]=0;
 REQUIRE(strstr(bytes,"\"phase\":4") && strstr(bytes,"\"pending\":1"));
 return 0;
}
int main(int argc,char **argv){
 REQUIRE(argc==3 && (!strcmp(argv[1],"failure") || !strcmp(argv[1],"failure-release")));
 creator=pthread_self();report_path=argv[2];release_fault=!strcmp(argv[1],"failure-release");
 REQUIRE(mb_owned_ps5_execute(report_path,-1)!=0);
 mb_owned_dll_report r;mb_owned_ps5_result(&r);original_failure(&r);
 REQUIRE(leave_injected && leave_retired && leave_calls==3);
 REQUIRE(r.started && r.run_attempted && !r.cleanup_pending && r.stage==MB_OWNED_STAGE_RETIRED);
 REQUIRE(map_calls==2 && !mappings[0].live && !mappings[1].live && release_calls==(release_fault?3u:2u));
 REQUIRE(sleeps==release_fault && release_injected==release_fault && mb_owned_ps5_memory_status()==0);
 REQUIRE(r.entry_enabled && !r.entry_contract_errors && r.source_acquires[0]==1 && r.source_acquires[1]==1);
 for(unsigned i=0;i<2;++i){
  REQUIRE(r.source_releases[i]==1 && r.entry_attach_calls[i]==1 && r.entry_detach_calls[i]==1);
  REQUIRE(r.entry_attach_result[i]==7 && !r.entry_detach_result[i]);
#if MB_OWNED_CLOCK
  REQUIRE(r.clock_session_begins[i]==1 && r.clock_session_ends[i]==1);
#endif
 }
 REQUIRE(r.consumer_imports==(MB_OWNED_CLOCK?23u:21u)+(MB_OWNED_SYNC?21u:0u)+(MB_OWNED_INIT_ONCE?2u:0u)+(MB_OWNED_MUTEX?5u:0u)+(MB_OWNED_FILEIO?10u:0u)+((MB_OWNED_ENTRY&&MB_OWNED_CRT_STARTUP)?7u:0u)+((MB_OWNED_ENTRY&&MB_OWNED_MODULE_ENUM)?5u:0u)+((MB_OWNED_ENTRY&&MB_OWNED_VM)?5u:0u)+((MB_OWNED_ENTRY&&MB_OWNED_STATIC_TLS)?4u:0u)+(MB_OWNED_SEH64?8u:0u) && r.helper_imports==(MB_OWNED_CLOCK?10u:8u));
 REQUIRE(r.sync_enabled==MB_OWNED_SYNC);
 REQUIRE(r.init_once_enabled==MB_OWNED_INIT_ONCE);
 REQUIRE(r.mutex_enabled==MB_OWNED_MUTEX);
 REQUIRE(r.fileio_enabled==MB_OWNED_FILEIO);
 REQUIRE(r.crt_startup_enabled==MB_OWNED_CRT_STARTUP);
 REQUIRE(r.module_enum_enabled==MB_OWNED_MODULE_ENUM);
 REQUIRE(r.vm_enabled==MB_OWNED_VM);
 mb_owned_delivery delivery;mb_owned_ps5_delivery(&delivery);
 REQUIRE(!delivery.first_file_error && !delivery.first_stdout_error);
 REQUIRE(!mb_owned_ps5_cleanup() && mb_owned_ps5_execute(report_path,-1)!=0);
 mb_owned_dll_report final;mb_owned_ps5_result(&final);original_failure(&final);
 REQUIRE(final.entry_detach_calls[0]==1 && final.entry_detach_calls[1]==1 && release_calls==(release_fault?3u:2u) && leave_calls==3);
 puts(release_fault?"Admission leave retries once; later image release failure preserves stage25 without repeating PE calls":"Admission leave retries once; both real worker exports and all owners retire without repeating PE calls");
 return 0;
}
