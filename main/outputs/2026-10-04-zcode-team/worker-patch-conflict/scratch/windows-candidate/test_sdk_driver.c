#include "sdk_driver.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned deny,release_fault,release_calls,sleeps;
static const char *report_path;
int mb_pe_ps5_test_pagesize(void){return 16384;}
void *mb_pe_ps5_test_map(void *p,size_t n,int rights,int flags,int fd,int64_t offset){
 if(p || rights!=3 || flags!=0x1002 || fd!=-1 || offset){errno=EINVAL;return MAP_FAILED;}
 return mmap(NULL,n,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
}
int mb_pe_ps5_test_protect(const void *p,size_t n,int rights){
 if(deny && (rights&4))return (int32_t)0x8002000d;
 return mprotect((void *)p,n,rights);
}
int mb_pe_ps5_test_unmap(void *p,size_t n){
 ++release_calls;if(release_fault && release_calls>=2 && sleeps<3){errno=EAGAIN;return -1;}return munmap(p,n);
}
unsigned mb_test_sleep(unsigned seconds){
 if(seconds!=1)abort();++sleeps;
 /* A persistent cleanup failure is already in the file before sleeping. */
 int fd=open(report_path,O_RDONLY);if(fd<0)abort();char bytes[16384];ssize_t n=read(fd,bytes,sizeof(bytes)-1);if(n<0)abort();bytes[n]=0;close(fd);
 if(!strstr(bytes,"\"phase\":4") || !strstr(bytes,"\"pending\":1") || strstr(bytes,"\"phase\":5"))abort();
 if(sleeps>3)abort();return 0;
}
int main(int argc,char **argv){
 CHECK(argc==3);report_path=argv[2];deny=!strcmp(argv[1],"deny");release_fault=!strcmp(argv[1],"release");
 int result=mb_owned_ps5_execute(report_path,-1);
 CHECK(result==(deny || release_fault));mb_owned_dll_report r;mb_owned_ps5_result(&r);
 CHECK(!r.cleanup_pending && r.run_attempted && r.stage==MB_OWNED_STAGE_RETIRED);
 CHECK(!r.entry_enabled && !r.entry_attach_calls[0] && !r.entry_attach_calls[1] && !r.entry_detach_calls[0] && !r.entry_detach_calls[1]);
 CHECK(r.dll_calls_started==(deny?0u:3u) && r.dll_calls_returned==r.dll_calls_started);
 CHECK(r.completed==!deny && r.results[0]==(deny?0u:86u) && r.results[1]==(deny?0u:93u));
 CHECK(r.failure_stage==(deny?5u:release_fault?14u:0u));CHECK(sleeps==(release_fault?3u:0u));
 mb_owned_delivery d;mb_owned_ps5_delivery(&d);CHECK(!d.first_file_error && d.file_records==(release_fault?5u:4u));
 CHECK(mb_owned_ps5_execute(report_path,-1)!=0); /* no second execution */
 puts(deny?"Driver reports native protection denial without DLL entry":release_fault?"Driver publishes retained cleanup before bounded sleeps and retires without rerun":"Driver reports actual 86/93 execution and retirement");return 0;
}
