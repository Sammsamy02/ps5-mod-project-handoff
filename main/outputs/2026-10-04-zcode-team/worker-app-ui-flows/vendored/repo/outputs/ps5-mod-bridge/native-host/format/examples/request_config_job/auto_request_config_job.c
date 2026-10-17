#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "config_request.h"
#include "config_file_job.h"
#include "fd_io.h"
#include "candidate_selector.h"
#include "config_archive_scope.h"
#include "private/posix_compat.h"
#include <mbedtls/sha256.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Automatic-target host composition harness. Reusable components never open/publish files.
 * This caller owns one private inbox and retains all failed artifacts there.
 * No game path, game identifier, known member identity or console operation. */
#define ROWS 16u
#define NAMES 4096u
static mb_archive_row_v1 rows[ROWS],replay_rows[ROWS];
static uint32_t order[ROWS],replay_order[ROWS],selected[1];
static char names[NAMES],replay_names[NAMES];
static uint8_t chunk[MB_ARCHIVE_CHUNK_BYTES],replay_chunk[MB_ARCHIVE_CHUNK_BYTES];
static uint8_t request_bytes[MB_CONFIG_REQUEST_MAX_BYTES],context_bytes[MB_CONFIG_CONTEXT_MAX_BYTES];
static mb_config_request_v2 request2;
#define request (request2.common)
static mb_fd_io_v1 io={sizeof(io),MB_FD_IO_ABI_V1,UINT64_C(3221225472),0,200000,0,NULL,NULL,NULL};
static int ref_equal(const char *a,const char *b) {
 while(*a&&*b){unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
 if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 0;}return *a==*b;
}
static int flat_ref(const char *s) {
 size_t n=strlen(s);if(!n||n>=MB_CONFIG_REF_BYTES||s[0]=='.')return 0;
 for(size_t i=0;i<n;i++)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||
  (s[i]>='0'&&s[i]<='9')||s[i]=='-'||s[i]=='_'||s[i]=='.'))return 0;
 return 1;
}
static int stable(const struct stat *a,const struct stat *b) {
 if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size)return 0;
#ifdef __APPLE__
 return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
 a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
 return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
 a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int read_exact(int fd,uint64_t offset,uint8_t *p,uint32_t n) {
 uint32_t actual=0;return mb_fd_io_read_at(&io,fd,offset,p,n,&actual)||actual!=n;
}
static int write_exact(int fd,const uint8_t *p,uint32_t n) {
 uint32_t actual=0;return mb_fd_io_write_at(&io,fd,0,p,n,&actual)||actual!=n;
}
static int digest_fd(int fd,uint64_t bytes,uint8_t digest[32]) {
 uint8_t data[65536];uint64_t at=0;mbedtls_sha256_context c;mbedtls_sha256_init(&c);
 int rc=mbedtls_sha256_starts(&c,0);
 while(!rc&&at<bytes){uint32_t n=(uint32_t)(bytes-at>sizeof(data)?sizeof(data):bytes-at);
 rc=read_exact(fd,at,data,n);if(!rc)rc=mbedtls_sha256_update(&c,data,n);at+=n;}
 if(!rc)rc=mbedtls_sha256_finish(&c,digest);mbedtls_sha256_free(&c);return rc;
}
static void hex(const uint8_t *p,char *out) {
 static const char digits[]="0123456789abcdef";
 for(size_t i=0;i<32;i++){out[2*i]=digits[p[i]>>4];out[2*i+1]=digits[p[i]&15];}out[64]=0;
}
static int open_input(int root,const char *ref,uint64_t bytes,struct stat *snapshot) {
 int fd=mb_archive_posix_openat(root,ref,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
 if(fd<0)return -1;
 if(fstat(fd,snapshot)||!S_ISREG(snapshot->st_mode)||snapshot->st_size<0||
 (uint64_t)snapshot->st_size!=bytes){close(fd);return -1;}return fd;
}
static int read_small(int root,const char *ref,uint8_t *data,uint32_t cap,uint32_t *size) {
 struct stat before,after;int fd=mb_archive_posix_openat(root,ref,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
 if(fd<0)return 1;int rc=fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size<=0||
 (uint64_t)before.st_size>cap;
 if(!rc){*size=(uint32_t)before.st_size;rc=read_exact(fd,0,data,*size)||fstat(fd,&after)||!stable(&before,&after);}
 if(close(fd))rc=1;return rc;
}
static void *allocate(void *ctx,size_t bytes){(void)ctx;return malloc(bytes);}
static void release(void *ctx,void *ptr){(void)ctx;free(ptr);}
int main(int argc,char **argv) {
 if(argc!=3||!flat_ref(argv[2])){fprintf(stderr,"usage: request_config_job PRIVATE_INBOX REQUEST_REF\n");return 2;}
 int root=-1,source_fd=-1,base_fd=-1,qfd=-1,member_fd=-1,output_fd=-1,claim_fd=-1,receipt_fd=-1,rc=1;
 int claimed=0,member_bound=0,job_called=0,output_verified=0;
 pm_config_file_job_status job_return=PM_CONFIG_FILE_JOB_BAD_ARGUMENT;const char *stage="request";uint32_t request_size=0,context_size=0;
 mb_config_request_ref_v1 refs={0};uint8_t context_digest[32]={0},base_digest[32]={0},output_digest[32]={0},member_digest[32]={0};
 pm_config_file_job_result_v2 result2={0};
#define result (result2.preparation)
mb_archive_config_candidate_v1 candidate={0};
 struct stat source_before,source_after,base_before,base_after,qstat,member_stat,output_stat;
 char request_hex[65]={0},context_hex[65]={0},member_hex[65]={0},output_hex[65]={0};uint8_t request_digest[32];
 root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(root<0)goto done;
 if(read_small(root,argv[2],request_bytes,sizeof(request_bytes),&request_size)||
 mb_config_request_ref_decode_v1(request_bytes,request_size,&refs)!=MB_CONFIG_REQUEST_OK||!refs.complete)goto done;
 if(ref_equal(argv[2],refs.claim_ref)||ref_equal(argv[2],refs.receipt_ref)||ref_equal(argv[2],refs.output_ref)||
 ref_equal(argv[2],refs.quarantine_ref)||ref_equal(argv[2],refs.source_ref)||ref_equal(argv[2],refs.base_ref)||ref_equal(argv[2],refs.context_ref))goto done;
 if(read_small(root,refs.context_ref,context_bytes,sizeof(context_bytes),&context_size)||
 mbedtls_sha256(context_bytes,context_size,context_digest,0)||
 mb_config_request_decode_v2(request_bytes,request_size,context_bytes,context_size,context_digest,&request2)!=MB_CONFIG_REQUEST_OK||
 !request2.complete||!request.complete||request.role!=MB_CONFIG_ROLE_SCALAR_INI)goto done;
 if(mbedtls_sha256(request_bytes,request_size,request_digest,0))goto done;
 hex(request_digest,request_hex);hex(context_digest,context_hex);
 stage="inputs";
 source_fd=open_input(root,request.source_ref,request.source_bytes,&source_before);
 base_fd=open_input(root,request.base_ref,request.base_bytes,&base_before);
 if(source_fd<0||base_fd<0||digest_fd(base_fd,request.base_bytes,base_digest)||
 memcmp(base_digest,request.base_sha256,32))goto done;
 stage="claim";
 claim_fd=mb_archive_posix_openat(root,request.claim_ref,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
 if(claim_fd<0)goto done;claimed=1;
 if(write_exact(claim_fd,(const uint8_t *)request_hex,64)||fsync(claim_fd)||fsync(root))goto done;
 stage="scan";
 mb_archive_source_v1 source={0};source.struct_size=sizeof(source);source.abi_version=MB_ARCHIVE_ABI_V1;
 source.fd=source_fd;source.size=request.source_bytes;memcpy(source.sha256,request.source_sha256,32);memcpy(source.target_token,request.target_token,32);
 mb_archive_limits_v1 limits;mb_archive_limits_default_v1(&limits);
 limits.raw_bytes=MB_CONFIG_MAX_SOURCE_BYTES;limits.member_bytes=MB_CONFIG_MAX_MEMBER_BYTES;
 limits.expanded_bytes=2097152;limits.entries=ROWS;limits.name_bytes=NAMES;
 mb_archive_budget_v1 budget;mb_archive_budget_default_v1(&budget);
 budget.io_limit=UINT64_C(536870912);budget.operation_limit=100000;budget.path_work_limit=4194304;
 mb_archive_buffers_v1 scan={rows,ROWS,names,NAMES,order,ROWS,chunk,sizeof(chunk)};
 mb_archive_inventory_v1 inventory={0};
 if(mb_archive_scan_v1(&source,&limits,&budget,&scan,&inventory)||inventory.complete!=1||inventory.installation_supported)goto done;
 mb_archive_config_selector_profile_v1 profile={0};profile.struct_size=sizeof(profile);profile.abi_version=MB_ARCHIVE_CONFIG_SELECTOR_ABI_V1;
 profile.basename=request.basename;profile.basename_bytes=request.basename_bytes;profile.max_source_bytes=request.max_member_bytes;
 memcpy(profile.target_token,request.target_token,32);
 stage="selection";
 if(mb_archive_config_select_v1(&inventory,rows,ROWS,names,NAMES,&profile,&candidate)!=MB_ARCHIVE_CONFIG_SELECTOR_OK||
 !candidate.complete||candidate.candidate_count!=1||candidate.member_bytes>request.max_member_bytes||
 memcmp(candidate.source_sha256,request.source_sha256,32)||pm_config_archive_scope(&inventory,rows,ROWS,&candidate))goto done;
 stage="materialization";
 if(mb_archive_posix_mkdirat(root,request.quarantine_ref,0700))goto done;
 qfd=mb_archive_posix_openat(root,request.quarantine_ref,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,0);
 if(qfd<0||fstat(qfd,&qstat)||!S_ISDIR(qstat.st_mode))goto done;
 mb_archive_owned_file_v1 journal[4]={{0}};mb_archive_materialized_v1 materialized={0};mb_archive_materialize_request_v1 mat={0};
 mat.source=&source;mat.inventory=&inventory;mat.rows=rows;mat.row_capacity=ROWS;mat.names=names;mat.name_capacity=NAMES;
 selected[0]=candidate.row_index;mat.selected=selected;mat.selected_count=1;memcpy(mat.target_token,request.target_token,32);
 mat.quarantine_fd=qfd;mat.quarantine_device=(uint64_t)qstat.st_dev;mat.quarantine_inode=(uint64_t)qstat.st_ino;
 mat.journal=journal;mat.journal_capacity=4;mat.replay=(mb_archive_buffers_v1){replay_rows,ROWS,replay_names,NAMES,replay_order,ROWS,replay_chunk,sizeof(replay_chunk)};
 if(mb_archive_materialize_v1(&mat,&limits,&budget,&materialized)||!materialized.complete||materialized.selected_count!=1||materialized.installation_supported)goto done;
 const mb_archive_owned_file_v1 *owned=NULL;
 for(uint32_t i=0;i<materialized.journal_count;i++)if(journal[i].row_index==candidate.row_index&&journal[i].state==MB_ARCHIVE_CREATED){if(owned)goto done;owned=&journal[i];}
 if(!owned||!memchr(owned->name,0,sizeof(owned->name))||owned->expected_size!=candidate.member_bytes||
 owned->written_bytes!=candidate.member_bytes||memcmp(owned->sha256,candidate.member_sha256,32))goto done;
 member_fd=open_input(qfd,owned->name,owned->expected_size,&member_stat);
 if(member_fd<0||(uint64_t)member_stat.st_dev!=owned->device||(uint64_t)member_stat.st_ino!=owned->inode||
 digest_fd(member_fd,owned->expected_size,member_digest)||memcmp(member_digest,owned->sha256,32))goto done;
 hex(member_digest,member_hex);member_bound=1;
 stage="file_job";
 output_fd=mb_archive_posix_openat(root,request.output_ref,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);
 if(output_fd<0)goto done;
 pm_ini_bytes sections[MB_CONFIG_MAX_SECTIONS];for(uint32_t i=0;i<request.section_count;i++)sections[i]=(pm_ini_bytes){(const uint8_t *)request.sections[i],request.section_bytes[i]};
 pm_config_file_job_params_v2 params2={0};params2.struct_size=sizeof(params2);params2.abi_version=PM_CONFIG_FILE_JOB_ABI_V2;
#define params (params2.base)
 params.struct_size=sizeof(params);params.abi_version=PM_CONFIG_FILE_JOB_ABI_V1;
 params.original_fd=base_fd;params.member_fd=member_fd;params.output_fd=output_fd;params.immutable_inputs=1;params.unpublished_output=1;
 params.original_bytes=request.base_bytes;params.member_bytes=candidate.member_bytes;
 params2.platform=(pm_ini_bytes){(const uint8_t *)request2.platform,request2.platform_bytes};
 params.sections=sections;params.section_count=request.section_count;params.io_context=&io;
 params.read_at=mb_fd_io_read_at;params.write_at=mb_fd_io_write_at;params.allocate=allocate;params.release=release;
 job_called=1;
 job_return=pm_config_file_job_v2(&params2,&result2);
 if(job_return!=PM_CONFIG_FILE_JOB_OK||result2.struct_size!=sizeof(result2)||result2.abi_version!=PM_CONFIG_FILE_JOB_ABI_V2||!result.archive_complete||result.allocation_live_after_cleanup)goto done;
 stage="readback";
 if(fsync(output_fd)||fstat(output_fd,&output_stat)||output_stat.st_size<0||
 digest_fd(output_fd,(uint64_t)output_stat.st_size,output_digest)||fstat(source_fd,&source_after)||fstat(base_fd,&base_after)||
 !stable(&source_before,&source_after)||!stable(&base_before,&base_after))goto done;
 hex(output_digest,output_hex);output_verified=1;rc=0;stage="prepared";
done:
 if(claimed){
 char record[4096],status_text[16],bytes_text[32];
 int result_initialized=result.struct_size==sizeof(result)&&result.abi_version==PM_CONFIG_FILE_JOB_ABI_V1;
 if(result_initialized)snprintf(status_text,sizeof(status_text),"%u",(unsigned)result.status);else strcpy(status_text,"null");
 if(output_verified)snprintf(bytes_text,sizeof(bytes_text),"%" PRIu64,(uint64_t)output_stat.st_size);else strcpy(bytes_text,"null");
 int length=snprintf(record,sizeof(record),
 "{\"phase\":\"%s\",\"stage\":\"%s\",\"request_sha256\":\"%s\",\"context_sha256\":\"%s\","
 "\"job_called\":%s,\"job_return\":%u,\"job_result_initialized\":%s,\"job_status\":%s,\"job_stage\":%u,\"component_status\":%u,\"archive_complete\":%u,\"allocation_peak_bytes\":%" PRIu64
 ",\"allocation_live_after_cleanup\":%" PRIu64 ",\"allocation_calls\":%u,\"release_calls\":%u,\"selected_member_bytes\":%" PRIu64
 ",\"selected_member_sha256\":\"%s\",\"merged_bytes\":%" PRIu64 ",\"scalar_winners\":%u,\"replacements\":%u,\"additions\":%u,"
 "\"output_ref\":\"%s\",\"output_present\":%s,\"output_verified\":%s,\"output_bytes\":%s,\"output_sha256\":\"%s\",\"source_entries_sha1_checked\":%u,"
 "\"selector_status\":%u,\"selector_roots\":%u,\"selector_targets\":%u,\"resolved_target\":\"%s\",\"resolved_mount\":\"%s\",\"resolved_seed\":%" PRIu64 ",\"layout_derived_from_original\":%s,\"member_facts_derived\":%s,\"external_preparation_helpers\":0,\"native_execution\":false,\"installation_supported\":false,\"runtime_supported\":false}\n",
 rc?"failed":"prepared",stage,request_hex,context_hex,job_called?"true":"false",(unsigned)job_return,result_initialized?"true":"false",status_text,(unsigned)result.stage,result.component_status,result.archive_complete,
 result.allocation_peak_bytes,result.allocation_live_after_cleanup,result.allocation_calls,result.release_calls,candidate.member_bytes,member_hex,
 result.merged_bytes,result.scalar_winners,result.replacements,result.additions,request.output_ref,output_fd>=0?"true":"false",output_verified?"true":"false",bytes_text,output_hex,result.source_entries_sha1_checked,result2.selector_status,result2.selector_root_count,result2.selector_target_count,output_verified?(const char *)result2.resolved_path:"",output_verified?(const char *)result2.resolved_mount:"",output_verified?result2.resolved_path_hash_seed:UINT64_C(0),output_verified?"true":"false",member_bound?"true":"false");
 receipt_fd=mb_archive_posix_openat(root,request.receipt_ref,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);
 uint8_t readback[4096];struct stat receipt_stat;
 if(length<=0||(size_t)length>=sizeof(record)||receipt_fd<0||write_exact(receipt_fd,(const uint8_t *)record,(uint32_t)length)||
 fsync(receipt_fd)||fstat(receipt_fd,&receipt_stat)||receipt_stat.st_size!=length||
 read_exact(receipt_fd,0,readback,(uint32_t)length)||memcmp(readback,record,(size_t)length)||fsync(root))rc=1;
 else fputs(record,stdout);
 }
 if(receipt_fd>=0&&close(receipt_fd))rc=1;
 if(output_fd>=0&&close(output_fd))rc=1;
 if(member_fd>=0&&close(member_fd))rc=1;
 if(qfd>=0&&close(qfd))rc=1;
 if(claim_fd>=0&&close(claim_fd))rc=1;
 if(base_fd>=0&&close(base_fd))rc=1;
 if(source_fd>=0&&close(source_fd))rc=1;
 if(root>=0&&close(root))rc=1;
 if(rc)fprintf(stderr,"request config job refused at %s; owned scratch retained; errno=%d\n",stage,errno);
 return rc;
}
