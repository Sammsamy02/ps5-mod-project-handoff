#define _POSIX_C_SOURCE 200809L
#include "original_file_job.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* These controls exercise real retained container bytes and physical I/O.
 * Only uncertain close is injected, after a real close, in the product TU. */
typedef struct control_state {
 const char *mode;
 void *allocations[128];size_t sizes[128];unsigned count,allocated,released;
 uint64_t live,deadline,late_elapsed;
 unsigned gate_calls,terminal_calls,reads,writes,created,closed,close_failed;
 unsigned callback_errors,premature_publication;
 int terminal,close_target;
 struct stat identities[5];unsigned seen[5];
 mb_iostore_file_job_result_v1 *result;
} control_state;
static control_state *active;
static unsigned checks,failures;
#define CHECK(c) do{++checks;if(!(c)){++failures;fprintf(stderr,"FAIL %s: %s\n",s.mode,#c);}}while(0)
static uint64_t monotonic(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))abort();return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
static int mode_is(const control_state *s,const char *name){return !strcmp(s->mode,name);}
static int continue_gate(void *opaque){control_state *s=opaque;++s->gate_calls;
 if(s->terminal||(s->deadline&&monotonic()>=s->deadline)){++s->terminal_calls;s->terminal=1;return 0;}return 1;}
static void *allocate(void *opaque,size_t bytes){control_state *s=opaque;
 if(s->count==128)return NULL;void *p=malloc(bytes);if(!p)return NULL;
 s->allocations[s->count]=p;s->sizes[s->count++]=bytes;s->live+=bytes;++s->allocated;
 if(mode_is(s,"post-allocation-batch"))s->terminal=1;return p;}
static void release(void *opaque,void *p){control_state *s=opaque;
 if(s->result->prepared)++s->premature_publication;
 for(unsigned i=0;i<s->count;++i)if(s->allocations[i]==p){
  if(mode_is(s,"late-cleanup-batch")&&s->live==s->sizes[i]){
   uint64_t began=monotonic();s->deadline=began+UINT64_C(20000000);
   struct timespec pause={0,30000000};while(nanosleep(&pause,&pause)&&errno==EINTR){}
   s->late_elapsed=monotonic()-began;
  }
  s->live-=s->sizes[i];s->allocations[i]=NULL;++s->released;free(p);return;
 }++s->callback_errors;}
static int read_at(void *opaque,int fd,uint64_t offset,uint8_t *out,uint32_t n,uint32_t *actual){control_state *s=opaque;++s->reads;*actual=0;
 ssize_t made=pread(fd,out,n,(off_t)offset);if(made!=(ssize_t)n)return 1;*actual=n;
 if(mode_is(s,"post-read-single"))s->terminal=1;return 0;}
static int write_at(void *opaque,int fd,uint64_t offset,const uint8_t *in,uint32_t n,uint32_t *actual){control_state *s=opaque;++s->writes;*actual=0;
 int matched=0;struct stat st;if(fstat(fd,&st))return 1;
 for(unsigned i=0;i<5;++i)if(s->seen[i]&&st.st_dev==s->identities[i].st_dev&&st.st_ino==s->identities[i].st_ino)matched=1;
 if(!matched&&!mode_is(s,"admission-single")&&!mode_is(s,"admission-batch"))++s->callback_errors;
 ssize_t made=pwrite(fd,in,n,(off_t)offset);if(made!=(ssize_t)n)return 1;*actual=n;
 if(mode_is(s,"partial-output-single"))s->terminal=1;return 0;}
static int artifact_event(void *opaque,uint32_t index,int fd,uint32_t event){control_state *s=opaque;
 if(index>=5){++s->callback_errors;return 0;}
 if(s->result->prepared)++s->premature_publication;
 if(event==MB_IOSTORE_FILE_JOB_ARTIFACT_CREATED){struct stat st;++s->created;
  if(s->seen[index]||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size||(st.st_mode&0777)!=0600){++s->callback_errors;return 0;}
  s->seen[index]=1;s->identities[index]=st;
  if(mode_is(s,"create-stop-single"))s->terminal=1;
  if(mode_is(s,"close-failure-single")&&index==0)s->close_target=fd;
  return !mode_is(s,"event-refusal-single");
 }
 if(!s->seen[index])++s->callback_errors;
 if(event==MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSED){++s->closed;if(fcntl(fd,F_GETFL)>=0||errno!=EBADF)++s->callback_errors;}
 else if(event==MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSE_FAILED)++s->close_failed;
 else ++s->callback_errors;
 return 0; /* Cleanup event refusals must never prevent other cleanup. */
}
int mb_iostore_gate_test_close(int fd){int result=close(fd);
 if(active&&fd==active->close_target){active->close_target=-1;errno=EINTR;return -1;}return result;}

#if defined(MB_IOSTORE_GATE_RED_BASELINE)
/* The frozen prior implementation remains byte-identical. These adapters
 * expose the new signatures while intentionally ignoring unavailable control. */
mb_iostore_file_job_status_v1 mb_iostore_original_file_job_gated_v1(
 const mb_iostore_file_job_params_v1 *p,mb_iostore_file_job_result_v1 *r,const mb_iostore_file_job_control_v1 *control){
 (void)control;return mb_iostore_original_file_job_v1(p,r);}
mb_iostore_file_job_status_v1 mb_iostore_batch_file_job_gated_v1(
 const mb_iostore_batch_file_job_params_v1 *p,mb_iostore_batch_file_job_result_v1 *r,const mb_iostore_file_job_control_v1 *control){
 (void)control;return mb_iostore_batch_file_job_v1(p,r);}
#endif

static unsigned created_files(int directory){unsigned n=0;for(unsigned i=0;i<5;++i){struct stat st;
 if(!fstatat(directory,mb_iostore_file_job_artifact_name_v1(i),&st,AT_SYMLINK_NOFOLLOW))++n;}return n;}
int main(int argc,char **argv){
 if(argc!=10){fprintf(stderr,"usage: gate_control mode modA_toc modA_ucas modB_toc modB_ucas base_toc base_header base_ucas_bytes fresh_output_dir\n");return 2;}
 control_state s={.mode=argv[1],.close_target=-1};active=&s;
 int fds[6]={-1,-1,-1,-1,-1,-1},directory=-1;off_t positions[6];
 for(unsigned i=0;i<6;++i){fds[i]=open(argv[2+i],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);CHECK(fds[i]>=0);
  if(fds[i]<0)goto done;positions[i]=(off_t)(17+i);CHECK(lseek(fds[i],positions[i],SEEK_SET)==positions[i]);}
 char *end;errno=0;uint64_t base_length=strtoull(argv[8],&end,10);CHECK(!errno&&!*end&&base_length);
 directory=open(argv[9],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(directory>=0);if(directory<0)goto done;
 mb_iostore_file_job_result_v1 single={0};mb_iostore_batch_file_job_result_v1 batch={0};
 int is_batch=strstr(s.mode,"batch")!=NULL;s.result=is_batch?&batch.preparation:&single;
 mb_iostore_file_job_params_v1 p={.struct_size=sizeof(p),.abi_version=1,.mod_toc_fd=fds[2],.mod_ucas_fd=fds[3],
  .base_toc_fd=fds[4],.base_header_fd=fds[5],.destination_dir_fd=directory,.immutable_inputs=1,.private_unpublished_directory=1,
  .observed_base_ucas_bytes=base_length,.io_context=&s,.read_at=read_at,.write_at=write_at,
  .allocator_context=&s,.allocate=allocate,.release=release};
 mb_iostore_batch_source_v1 sources[2]={{fds[0],fds[1]},{fds[2],fds[3]}};
 mb_iostore_batch_file_job_params_v1 bp={.struct_size=sizeof(bp),.abi_version=1,.sources=sources,.source_count=2,
  .base_toc_fd=fds[4],.base_header_fd=fds[5],.destination_dir_fd=directory,.immutable_inputs=1,.private_unpublished_directory=1,
  .observed_base_ucas_bytes=base_length,.io_context=&s,.read_at=read_at,.write_at=write_at,
  .allocator_context=&s,.allocate=allocate,.release=release};
 mb_iostore_file_job_control_v1 control={sizeof(control),1,continue_gate,&s,artifact_event,&s};
 if(!strncmp(s.mode,"admission-",10))s.terminal=1;
 uint64_t began=monotonic();mb_iostore_file_job_status_v1 status;
 if(mode_is(&s,"validation")){
  unsigned char before[sizeof(single)],batch_before[sizeof(batch)];memset(&single,0xa5,sizeof(single));memcpy(before,&single,sizeof(single));
  memset(&batch,0xa5,sizeof(batch));memcpy(batch_before,&batch,sizeof(batch));
  control.gate=NULL;
  CHECK(mb_iostore_original_file_job_gated_v1(&p,&single,&control)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);
  CHECK(!memcmp(before,&single,sizeof(single)));CHECK(mb_iostore_batch_file_job_gated_v1(&bp,&batch,&control)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);
  CHECK(!memcmp(batch_before,&batch,sizeof(batch)));control.gate=continue_gate;control.gate_context=&single;
  CHECK(mb_iostore_original_file_job_gated_v1(&p,&single,&control)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);CHECK(!memcmp(before,&single,sizeof(single)));
  control.gate_context=sources;CHECK(mb_iostore_batch_file_job_gated_v1(&bp,&batch,&control)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);
  CHECK(!memcmp(batch_before,&batch,sizeof(batch)));control.gate_context=&s;control.artifact_context=&control;
  CHECK(mb_iostore_original_file_job_gated_v1(&p,&single,&control)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);CHECK(!memcmp(before,&single,sizeof(single)));
  CHECK(mb_iostore_original_file_job_gated_v1(&p,&single,(const mb_iostore_file_job_control_v1 *)&single)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);
  CHECK(!memcmp(before,&single,sizeof(single)));control.artifact_context=&s;control.struct_size=0;
  CHECK(mb_iostore_batch_file_job_gated_v1(&bp,&batch,&control)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);CHECK(!memcmp(batch_before,&batch,sizeof(batch)));
  CHECK(!s.gate_calls&&!s.reads&&!s.writes&&!s.allocated&&!created_files(directory));status=MB_IOSTORE_FILE_JOB_BAD_ARGUMENT;
 }else{
  status=is_batch?mb_iostore_batch_file_job_gated_v1(&bp,&batch,&control):mb_iostore_original_file_job_gated_v1(&p,&single,&control);
  CHECK(!s.result->prepared);CHECK(!s.live&&s.allocated==s.released);
  CHECK(s.result->allocations==s.result->releases&&!s.result->allocation_live_after_cleanup);
  CHECK(!s.callback_errors&&!s.premature_publication);
  if(mode_is(&s,"event-refusal-single")){
   CHECK(status==MB_IOSTORE_FILE_JOB_CREATE_ERROR);CHECK(s.created==1&&s.closed==1&&!s.close_failed&&!s.writes);
  }else if(mode_is(&s,"close-failure-single")){
   CHECK(status==MB_IOSTORE_FILE_JOB_CLOSE_ERROR);CHECK(s.created==5&&s.closed==4&&s.close_failed==1);CHECK(s.result->owned_fds_closed==4);
  }else{
   CHECK(status==MB_IOSTORE_FILE_JOB_STOPPED);CHECK(s.terminal_calls==1);CHECK(s.result->status==MB_IOSTORE_FILE_JOB_STOPPED);
   if(!strncmp(s.mode,"admission-",10))CHECK(!s.reads&&!s.writes&&!s.allocated&&!s.created&&s.gate_calls==1);
   if(mode_is(&s,"post-read-single"))CHECK(s.reads==1&&!s.writes&&s.allocated==1&&s.released==1&&!s.created);
   if(mode_is(&s,"post-allocation-batch"))CHECK(!s.reads&&!s.writes&&s.allocated==1&&s.released==1&&!s.created);
   if(mode_is(&s,"partial-output-single")){struct stat st;CHECK(s.writes==1&&s.created==1&&s.closed==1);
    CHECK(!fstatat(directory,mb_iostore_file_job_artifact_name_v1(0),&st,AT_SYMLINK_NOFOLLOW)&&st.st_size>0);}
   if(mode_is(&s,"create-stop-single")){struct stat st;CHECK(!s.writes&&s.created==1&&s.closed==1);
    CHECK(!fstatat(directory,mb_iostore_file_job_artifact_name_v1(0),&st,AT_SYMLINK_NOFOLLOW)&&!st.st_size);}
   if(mode_is(&s,"late-cleanup-batch"))CHECK(s.created==5&&s.closed==5&&s.late_elapsed>=UINT64_C(20000000));
  }
  CHECK(s.result->owned_fds_closed==s.closed);CHECK(created_files(directory)==s.created);
 }
 for(unsigned i=0;i<6;++i)CHECK(fcntl(fds[i],F_GETFL)>=0&&lseek(fds[i],0,SEEK_CUR)==positions[i]);
 printf("{\"mode\":\"%s\",\"checks\":%u,\"failures\":%u,\"status\":%u,\"prepared\":%u,\"gate_calls\":%u,"
  "\"terminal_calls\":%u,\"reads\":%u,\"writes\":%u,\"allocations\":%u,\"releases\":%u,\"created\":%u,"
  "\"closed\":%u,\"close_failed\":%u,\"premature_publication\":%u,\"late_elapsed_ns\":%"PRIu64",\"elapsed_ns\":%"PRIu64"}\n",
  s.mode,checks,failures,status,s.result->prepared,s.gate_calls,s.terminal_calls,s.reads,s.writes,s.allocated,s.released,s.created,
  s.closed,s.close_failed,s.premature_publication,s.late_elapsed,monotonic()-began);
 done:for(unsigned i=0;i<6;++i)if(fds[i]>=0)close(fds[i]);if(directory>=0)close(directory);active=NULL;return failures?1:0;
}
