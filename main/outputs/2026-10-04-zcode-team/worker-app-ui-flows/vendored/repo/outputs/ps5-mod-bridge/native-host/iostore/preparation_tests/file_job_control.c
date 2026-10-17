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

typedef struct ledger {void *p[64];size_t n[64];unsigned count,allocs,releases;uint64_t live,peak;} ledger;
static void *tracked_allocate(void *v,size_t n){ledger *l=v;if(l->count==64)return NULL;
 void *p=malloc(n);if(!p)return NULL;l->p[l->count]=p;l->n[l->count++]=n;l->live+=n;++l->allocs;
 if(l->live>l->peak)l->peak=l->live;return p;}
static void tracked_release(void *v,void *p){ledger *l=v;for(unsigned i=0;i<l->count;++i)if(l->p[i]==p){
 l->live-=l->n[i];l->p[i]=NULL;++l->releases;free(p);return;}abort();}
static unsigned assertions;
#define CHECK(c) do{++assertions;if(!(c)){fprintf(stderr,"assertion %u failed: %s\n",assertions,#c);goto done;}}while(0)
static int borrowed_ok(const int fds[5],const off_t offsets[4]){
 for(unsigned i=0;i<5;++i)if(fcntl(fds[i],F_GETFL)<0)return 0;
 for(unsigned i=0;i<4;++i)if(lseek(fds[i],0,SEEK_CUR)!=offsets[i])return 0;return 1;
}
static int equal_files(int directory,const char *name,int oracle){
 int left=openat(directory,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW),right=openat(oracle,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
 int equal=0;struct stat a,b;uint8_t x[65536],y[65536];
 if(left<0||right<0||fstat(left,&a)||fstat(right,&b)||a.st_size!=b.st_size)goto out;
 for(off_t at=0;at<a.st_size;){size_t n=(size_t)(a.st_size-at);if(n>sizeof(x))n=sizeof(x);
  if(pread(left,x,n,at)!=(ssize_t)n||pread(right,y,n,at)!=(ssize_t)n||memcmp(x,y,n))goto out;at+=(off_t)n;}equal=1;
 out:if(left>=0)close(left);if(right>=0)close(right);return equal;
}
static int forced_partial_write(void *context,int fd,uint64_t offset,const uint8_t *data,uint32_t requested,uint32_t *actual){
 unsigned *calls=context;++*calls;*actual=0;uint32_t n=requested<17?requested:17;
 ssize_t made=pwrite(fd,data,n,(off_t)offset);if(made>0)*actual=(uint32_t)made;return 1;
}
static void print_result(const char *label,const mb_iostore_file_job_result_v1 *r,double seconds){
 printf("\"%s\":{\"status\":%u,\"stage\":%u,\"prepared\":%u,\"created_mask\":%u,\"failed_artifact\":%u,"
  "\"managed_allocation_peak_bytes\":%"PRIu64",\"managed_live_after_cleanup\":%"PRIu64",\"allocations\":%u,\"releases\":%u,"
  "\"owned_fds_closed\":%u,\"source_reads\":%"PRIu64",\"source_bytes\":%"PRIu64",\"original_mod_chunks\":%u,\"emitted_chunks\":%u,"
  "\"incoming_asset_hashes_verified\":%u,\"header_hashes_verified\":%u,\"base_container_id\":%"PRIu64",\"merged_packages\":%u,"
  "\"replaced_packages\":%u,\"added_packages\":%u,\"sidecar_blocks\":%u,\"directory_files\":%u,\"directory_overrides\":%u,"
  "\"final_chunks\":%u,\"source_snapshots_stable\":%u,\"installation_authority\":%u,\"seconds\":%.6f}",
  label,r->status,r->stage,r->prepared,r->created_artifact_mask,r->failed_artifact_index,r->allocation_peak_bytes,
  r->allocation_live_after_cleanup,r->allocations,r->releases,r->owned_fds_closed,r->source_read_calls,r->source_read_bytes,
  r->original_mod_chunks,r->emitted_chunks,r->incoming_asset_hashes_verified,r->source_header_hash_verified&&r->captured_base_header_hash_verified,
  r->base_container_id,r->merged_packages,r->replaced_packages,r->added_packages,r->sidecar_blocks,r->directory_files,r->directory_overrides,
  r->final_chunks,r->source_snapshots_stable,r->installation_authority,seconds);
}
int main(int argc,char **argv){
 if(argc!=8){fprintf(stderr,"usage: file_job_control mod_toc mod_ucas base_toc base_header base_ucas_length fresh_trial_dir post_job_oracle_dir\n");return 2;}
 int okay=0,fds[5]={-1,-1,-1,-1,-1},trial=-1,oracle=-1,failed_dir=-1;off_t offsets[4];ledger l={0};
 mb_iostore_file_job_result_v1 success,conflict,failed,aliased;memset(&success,0,sizeof(success));
 for(unsigned i=0;i<4;++i){fds[i]=open(argv[i+1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);CHECK(fds[i]>=0);
  offsets[i]=(off_t)(9+i);CHECK(lseek(fds[i],offsets[i],SEEK_SET)==offsets[i]);}
 char *end;errno=0;uint64_t base_length=strtoull(argv[5],&end,10);CHECK(!errno&&!*end&&base_length);
 trial=open(argv[6],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(trial>=0);
 CHECK(!mkdirat(trial,"success",0700));CHECK(!mkdirat(trial,"partial-failure",0700));
 fds[4]=openat(trial,"success",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(fds[4]>=0);
 mb_iostore_file_job_params_v1 p={.struct_size=sizeof(p),.abi_version=1,.mod_toc_fd=fds[0],.mod_ucas_fd=fds[1],
  .base_toc_fd=fds[2],.base_header_fd=fds[3],.destination_dir_fd=fds[4],.immutable_inputs=1,.private_unpublished_directory=1,
  .observed_base_ucas_bytes=base_length,.allocator_context=&l,.allocate=tracked_allocate,.release=tracked_release};
 struct timespec start,finish;clock_gettime(CLOCK_MONOTONIC,&start);
 CHECK(mb_iostore_original_file_job_v1(&p,&success)==MB_IOSTORE_FILE_JOB_OK);clock_gettime(CLOCK_MONOTONIC,&finish);
 double seconds=(double)(finish.tv_sec-start.tv_sec)+(double)(finish.tv_nsec-start.tv_nsec)/1e9;
 CHECK(success.prepared&&success.created_artifact_mask==31&&success.installation_authority==0&&success.owned_fds_closed==5);
 CHECK(!l.live&&success.allocations==success.releases&&success.allocation_live_after_cleanup==0&&success.allocation_peak_bytes==l.peak);
 CHECK(borrowed_ok(fds,offsets));
 /* Oracle is opened only after preparation, and is never passed to the job. */
 oracle=open(argv[7],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(oracle>=0);
 struct stat identities[5];for(unsigned i=0;i<5;++i){const char *name=mb_iostore_file_job_artifact_name_v1(i);
  CHECK(equal_files(fds[4],name,oracle));CHECK(!fstatat(fds[4],name,identities+i,AT_SYMLINK_NOFOLLOW));}
 /* A repeated call sees existing artifacts and must not own, overwrite or close them. */
 CHECK(mb_iostore_original_file_job_v1(&p,&conflict)==MB_IOSTORE_FILE_JOB_CREATE_ERROR);
 CHECK(!conflict.prepared&&!conflict.created_artifact_mask&&!conflict.owned_fds_closed&&!l.live&&conflict.allocations==conflict.releases);
 CHECK(borrowed_ok(fds,offsets));
 for(unsigned i=0;i<5;++i){struct stat now;CHECK(!fstatat(fds[4],mb_iostore_file_job_artifact_name_v1(i),&now,AT_SYMLINK_NOFOLLOW));
  CHECK(now.st_dev==identities[i].st_dev&&now.st_ino==identities[i].st_ino&&now.st_size==identities[i].st_size);}
 failed_dir=openat(trial,"partial-failure",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(failed_dir>=0);
 unsigned writes=0;p.destination_dir_fd=failed_dir;p.io_context=&writes;p.write_at=forced_partial_write;
 CHECK(mb_iostore_original_file_job_v1(&p,&failed)==MB_IOSTORE_FILE_JOB_WRITE_ERROR);
 CHECK(!failed.prepared&&failed.created_artifact_mask==1&&failed.owned_fds_closed==1&&!l.live&&failed.allocations==failed.releases&&writes==1);
 struct stat partial;CHECK(!fstatat(failed_dir,mb_iostore_file_job_artifact_name_v1(0),&partial,AT_SYMLINK_NOFOLLOW)&&partial.st_size==17);
 for(unsigned i=1;i<5;++i){struct stat unused;CHECK(fstatat(failed_dir,mb_iostore_file_job_artifact_name_v1(i),&unused,AT_SYMLINK_NOFOLLOW)<0&&errno==ENOENT);}
 CHECK(borrowed_ok(fds,offsets)&&fcntl(failed_dir,F_GETFL)>=0);
 p.write_at=NULL;p.io_context=NULL;p.base_header_fd=p.mod_toc_fd;
 CHECK(mb_iostore_original_file_job_v1(&p,&aliased)==MB_IOSTORE_FILE_JOB_INPUT_ALIAS);
 CHECK(!aliased.prepared&&!aliased.created_artifact_mask&&!aliased.allocations&&!l.live&&borrowed_ok(fds,offsets));
 union {mb_iostore_file_job_params_v1 params;mb_iostore_file_job_result_v1 result;} alias,before;
 memset(&alias,0xa5,sizeof(alias));alias.params=p;before=alias;
 CHECK(mb_iostore_original_file_job_v1(&alias.params,&alias.result)==MB_IOSTORE_FILE_JOB_BAD_ARGUMENT);
 CHECK(!memcmp(&before,&alias,sizeof(alias)));CHECK(l.allocs==l.releases&&!l.live);
 printf("{\"phase\":\"passed\",\"assertions\":%u,\"repeated_requests_in_one_process\":5,\"borrowed_fds_preserved\":true,"
  "\"borrowed_source_positions_preserved\":true,\"post_job_artifact_equality\":true,\"partial_file_retained_unpublished\":true,",assertions);
 print_result("success",&success,seconds);printf(",");print_result("existing_artifact_refusal",&conflict,0);printf(",");
 print_result("forced_write_failure",&failed,0);printf("}\n");okay=1;
 done:for(unsigned i=0;i<5;++i)if(fds[i]>=0)close(fds[i]);if(trial>=0)close(trial);if(oracle>=0)close(oracle);if(failed_dir>=0)close(failed_dir);
 return okay?0:1;
}
