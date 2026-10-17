#define _POSIX_C_SOURCE 200809L
#include "original_file_job.h"
#include "none_range_reader.h"
#include "container_header_merge.h"
#include "none_packer.h"
#include "directory_merge.h"
#include "toc_overlay.h"
#include "blake3.h"
#if defined(__PROSPERO__)
#include "../archive/private/posix_compat.h"
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SLOTS 128u
#define INPUTS (2u*MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES+2u)
#define ALLOCATION_CAP MB_IOSTORE_FILE_JOB_ALLOCATION_CAP
static const char *const names[5]={"generated-sidecar.ucas","generated-small.utoc",
 "merged-container-header.bin","generated-directory.bin","generated-final.utoc"};
const char *mb_iostore_file_job_artifact_name_v1(uint32_t index){return index<5?names[index]:NULL;}
typedef struct allocation {void *pointer;size_t bytes;} allocation;
typedef struct caller {
 allocation allocations[SLOTS];size_t count;uint64_t live,peak;
 int inputs[INPUTS],directory_fd,sidecar_fd,owned[5];struct stat snapshots[INPUTS];
 unsigned input_count;uint64_t input_caps[INPUTS];
 mb_iostore_batch_file_job_result_v1 *batch_result;
 uint64_t source_reads,source_bytes,write_calls;
 mb_none_range_plan_v1 *plans;uint32_t *source_rows;
 const uint8_t *merged_header;size_t merged_header_bytes;
 const mb_iostore_none_chunk_v1 *chunks;uint32_t chunk_count;
 const mb_iostore_file_job_params_v1 *params;mb_iostore_file_job_result_v1 *result;
 const mb_iostore_file_job_control_v1 *control;int stopped;
} caller;
static int spans_overlap(const void *a,size_t an,const void *b,size_t bn){
 uintptr_t av=(uintptr_t)a,bv=(uintptr_t)b;
 if(an>UINTPTR_MAX-av||bn>UINTPTR_MAX-bv)return 1;
 return an&&bn&&av<bv+bn&&bv<av+an;
}
static int fail(caller *c,mb_iostore_file_job_status_v1 status,int error){
 if(c->result->status==MB_IOSTORE_FILE_JOB_OK){c->result->status=status;c->result->system_error=error;}return 0;
}
static int check_gate(caller *c){
 if(c->stopped)return 0;
 if(c->control&&c->control->gate&&!c->control->gate(c->control->gate_context)){
  c->stopped=1;return fail(c,MB_IOSTORE_FILE_JOB_STOPPED,0);
 }return 1;
}
static int known_context_alias(const void *context,const void *params,size_t params_bytes,
 const void *result,size_t result_bytes,const mb_iostore_file_job_control_v1 *control,
 const void *sources,size_t source_bytes){
 return context&&(spans_overlap(context,1,params,params_bytes)||spans_overlap(context,1,result,result_bytes)||
  spans_overlap(context,1,control,sizeof(*control))||spans_overlap(context,1,sources,source_bytes));
}
static int valid_control(const mb_iostore_file_job_control_v1 *control,const void *params,size_t params_bytes,
 const void *result,size_t result_bytes,const void *sources,size_t source_bytes){
 if(!control)return 1;
 if(spans_overlap(control,sizeof(*control),params,params_bytes)||
  spans_overlap(control,sizeof(*control),result,result_bytes)||spans_overlap(control,sizeof(*control),sources,source_bytes))return 0;
 if(control->struct_size!=sizeof(*control)||control->abi_version!=MB_IOSTORE_FILE_JOB_CONTROL_ABI_V1||
  (!control->gate&&control->gate_context)||(!control->artifact_event&&control->artifact_context))return 0;
 return !known_context_alias(control->gate_context,params,params_bytes,result,result_bytes,control,sources,source_bytes)&&
  !known_context_alias(control->artifact_context,params,params_bytes,result,result_bytes,control,sources,source_bytes);
}
static void *alloc(caller *c,size_t count,size_t width) {
 if(!check_gate(c))return NULL;
 if(!count||width>SIZE_MAX/count||c->count==SLOTS){fail(c,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT,0);return NULL;}
 size_t n=count*width;if(n>ALLOCATION_CAP-c->live){fail(c,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT,0);return NULL;}
 void *p=c->params->allocate?c->params->allocate(c->params->allocator_context,n):malloc(n);
 if(!p){fail(c,MB_IOSTORE_FILE_JOB_NO_MEMORY,0);check_gate(c);return NULL;}
 /* Register returned ownership before a post-allocation stop can escape. */
 c->allocations[c->count++]=(allocation){p,n};c->live+=n;if(c->live>c->peak)c->peak=c->live;
 return check_gate(c)?p:NULL;
}
static void cleanup(caller *c) {
 for(unsigned i=0;i<5;++i)if(c->owned[i]>=0){
  int fd=c->owned[i],closed=close(fd),error=errno;c->owned[i]=-1;
  if(closed)fail(c,MB_IOSTORE_FILE_JOB_CLOSE_ERROR,error);else ++c->result->owned_fds_closed;
  if(c->control&&c->control->artifact_event)c->control->artifact_event(c->control->artifact_context,i,fd,
   closed?MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSE_FAILED:MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSED);
 }
 for(size_t i=c->count;i>0;--i){void *p=c->allocations[i-1].pointer;c->allocations[i-1].pointer=NULL;
  if(c->params->release)c->params->release(c->params->allocator_context,p);else free(p);
  c->live-=c->allocations[i-1].bytes;++c->result->releases;
 }
 c->result->allocation_peak_bytes=c->peak;c->result->allocation_live_after_cleanup=c->live;c->result->allocations=(uint32_t)c->count;
 c->result->source_read_calls=c->source_reads;c->result->source_read_bytes=c->source_bytes;c->result->write_calls=c->write_calls;
}
static int same_stat(const struct stat *a,const struct stat *b) {
 if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_mode!=b->st_mode||a->st_size!=b->st_size)return 0;
#ifdef __APPLE__
 return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
  a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
 return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
  a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int exact_read(caller *c,int fd,uint64_t offset,uint8_t *out,size_t bytes) {
 if(!check_gate(c))return 0;
 if(offset>INT64_MAX||bytes>(uint64_t)INT64_MAX-offset)return fail(c,MB_IOSTORE_FILE_JOB_READ_ERROR,0);
 size_t done=0;uint32_t attempts=0;
 while(done<bytes){if(!check_gate(c))return 0;if(++attempts>65536)return fail(c,MB_IOSTORE_FILE_JOB_READ_ERROR,0);size_t wanted=bytes-done;if(wanted>65536)wanted=65536;
  if(c->params->read_at){uint32_t actual=0;int status=c->params->read_at(c->params->io_context,fd,offset+done,out+done,(uint32_t)wanted,&actual);
   if(status||actual!=wanted){fail(c,MB_IOSTORE_FILE_JOB_READ_ERROR,0);check_gate(c);return 0;}done+=wanted;
  }else {ssize_t n=pread(fd,out+done,wanted,(off_t)(offset+done));
   int error=errno;if(n<0&&error==EINTR){if(!check_gate(c))return 0;continue;}
   if(n<=0||(size_t)n>wanted){fail(c,MB_IOSTORE_FILE_JOB_READ_ERROR,n<0?error:0);check_gate(c);return 0;}done+=(size_t)n;}
  if(!check_gate(c))return 0;
 }return 1;
}
static int exact_write(caller *c,int fd,uint64_t offset,const uint8_t *data,size_t bytes) {
 if(!check_gate(c))return 0;
 if(offset>INT64_MAX||bytes>(uint64_t)INT64_MAX-offset)return fail(c,MB_IOSTORE_FILE_JOB_WRITE_ERROR,0);
 size_t done=0;uint32_t attempts=0;
 while(done<bytes){if(!check_gate(c))return 0;if(++attempts>65536)return fail(c,MB_IOSTORE_FILE_JOB_WRITE_ERROR,0);size_t wanted=bytes-done;if(wanted>65536)wanted=65536;
  if(c->params->write_at){uint32_t actual=0;int status=c->params->write_at(c->params->io_context,fd,offset+done,data+done,(uint32_t)wanted,&actual);
   if(status||actual!=wanted){fail(c,MB_IOSTORE_FILE_JOB_WRITE_ERROR,0);check_gate(c);return 0;}done+=wanted;
  }else {ssize_t n=pwrite(fd,data+done,wanted,(off_t)(offset+done));
   int error=errno;if(n<0&&error==EINTR){if(!check_gate(c))return 0;continue;}
   if(n<=0||(size_t)n>wanted){fail(c,MB_IOSTORE_FILE_JOB_WRITE_ERROR,n<0?error:0);check_gate(c);return 0;}done+=(size_t)n;}
  if(!check_gate(c))return 0;
 }return 1;
}
static int preflight_fds(caller *c){
 const uint64_t caps[4]={MB_IOSTORE_MAX_TOC_BYTES,MB_IOSTORE_NONE_MAX_PAYLOAD_BYTES,MB_IOSTORE_MAX_TOC_BYTES,CCH_HEADER_BYTES_CAP};
 for(unsigned i=0;i<c->input_count;++i){if(!check_gate(c))return 0;int flags=fcntl(c->inputs[i],F_GETFL);
  if(flags<0)return fail(c,MB_IOSTORE_FILE_JOB_FD_REFUSED,errno);
  if((flags&O_ACCMODE)!=O_RDONLY)return fail(c,MB_IOSTORE_FILE_JOB_FD_REFUSED,0);
  if(fstat(c->inputs[i],c->snapshots+i))return fail(c,MB_IOSTORE_FILE_JOB_FD_REFUSED,errno);
  if(!S_ISREG(c->snapshots[i].st_mode)||c->snapshots[i].st_size<=0||
   (uint64_t)c->snapshots[i].st_size>(c->batch_result?c->input_caps[i]:caps[i])||(uint64_t)c->snapshots[i].st_size>SIZE_MAX)
   return fail(c,MB_IOSTORE_FILE_JOB_FD_REFUSED,0);
  for(unsigned j=0;j<i;++j)if(c->snapshots[i].st_dev==c->snapshots[j].st_dev&&c->snapshots[i].st_ino==c->snapshots[j].st_ino){
   if(c->batch_result&&i<2u*c->batch_result->source_count&&j/2u!=i/2u){
    c->batch_result->conflict_kind=MB_IOSTORE_BATCH_CONFLICT_SOURCE_IDENTITY;
    c->batch_result->conflict_source_a=j/2u;c->batch_result->conflict_source_b=i/2u;
    return fail(c,MB_IOSTORE_FILE_JOB_CONFLICT,0);
   }return fail(c,MB_IOSTORE_FILE_JOB_INPUT_ALIAS,0);
  }
  if(!check_gate(c))return 0;
 }
 struct stat directory;if(fstat(c->directory_fd,&directory))return fail(c,MB_IOSTORE_FILE_JOB_FD_REFUSED,errno);
 if(!S_ISDIR(directory.st_mode))return fail(c,MB_IOSTORE_FILE_JOB_FD_REFUSED,0);
 return check_gate(c);
}
static int load_input(caller *c,unsigned index,uint8_t **loaded,size_t *bytes){
 *bytes=(size_t)c->snapshots[index].st_size;*loaded=alloc(c,*bytes,1);
 return *loaded&&exact_read(c,c->inputs[index],0,*loaded,*bytes);
}
static int stable_inputs(caller *c) {
 for(unsigned i=0;i<c->input_count;++i){if(!check_gate(c))return 0;struct stat now;
  if(fstat(c->inputs[i],&now)||!same_stat(c->snapshots+i,&now))return 0;if(!check_gate(c))return 0;}return 1;
}
static int read_source(void *opaque,uint32_t partition,uint64_t offset,uint8_t *out,size_t requested,size_t *actual) {
 caller *c=opaque;*actual=0;
 if(partition||offset>(uint64_t)c->snapshots[1].st_size||requested>(uint64_t)c->snapshots[1].st_size-offset||
  !exact_read(c,c->inputs[1],offset,out,requested))return 1;
 ++c->source_reads;c->source_bytes+=requested;*actual=requested;return 0;
}
static int read_chunk(void *opaque,uint32_t row,uint64_t offset,uint8_t *out,uint32_t requested,uint32_t *actual) {
 caller *c=opaque;*actual=0;if(!check_gate(c)||row>=c->chunk_count)return 1;
 if(offset>c->chunks[row].bytes||requested>c->chunks[row].bytes-offset)return 1;
 if(c->source_rows[row]==UINT32_MAX){
  if(offset>c->merged_header_bytes||requested>c->merged_header_bytes-offset)return 1;
  memcpy(out,c->merged_header+(size_t)offset,requested);if(!check_gate(c))return 1;*actual=requested;return 0;
 }
 mb_none_range_result_v1 result;
 if(mb_iostore_none_range_v1(c->plans+c->source_rows[row],offset,requested,out,requested,&result)!=MB_NONE_RANGE_OK||
  !result.range_complete||result.read_bytes!=requested||!check_gate(c))return 1;
 *actual=requested;
 return 0;
}
static int write_sidecar(void *opaque,uint64_t offset,const uint8_t *data,uint32_t requested,uint32_t *actual) {
 caller *c=opaque;*actual=0;if(!exact_write(c,c->sidecar_fd,offset,data,requested))return 1;
 ++c->write_calls;*actual=requested;return 0;
}
static int readback(caller *c,int fd,const uint8_t *expected,size_t bytes) {
 if(!check_gate(c))return 0;
 uint8_t observed[65536];struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0||(uint64_t)st.st_size!=bytes)return fail(c,MB_IOSTORE_FILE_JOB_READBACK_ERROR,0);
 for(size_t at=0;at<bytes;){size_t n=bytes-at;if(n>sizeof(observed))n=sizeof(observed);
  if(!exact_read(c,fd,at,observed,n))return 0;
  if(memcmp(expected+at,observed,n))return fail(c,MB_IOSTORE_FILE_JOB_READBACK_ERROR,0);at+=n;
  if(!check_gate(c))return 0;}return check_gate(c);
}
static int create_new(caller *c,uint32_t index){
 if(!check_gate(c))return 0;
 c->result->failed_artifact_index=index;
#if defined(__PROSPERO__)
 int fd=mb_archive_posix_openat(c->directory_fd,names[index],O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
#else
 int fd=openat(c->directory_fd,names[index],O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
#endif
 if(fd<0)return fail(c,MB_IOSTORE_FILE_JOB_CREATE_ERROR,errno);
 c->owned[index]=fd;c->result->created_artifact_mask|=1u<<index;
 if(c->control&&c->control->artifact_event&&!c->control->artifact_event(c->control->artifact_context,index,fd,
  MB_IOSTORE_FILE_JOB_ARTIFACT_CREATED))return fail(c,MB_IOSTORE_FILE_JOB_CREATE_ERROR,0);
 return check_gate(c);
}
static int sync_owned(caller *c,int fd){
 if(!check_gate(c))return 0;
 int status=fsync(fd),error=errno;if(status)fail(c,MB_IOSTORE_FILE_JOB_SYNC_ERROR,error);
 return check_gate(c)&&!status;
}
static int write_new(caller *c,uint32_t index,const uint8_t *data,size_t bytes) {
 if(!create_new(c,index))return 0;int fd=c->owned[index];
 if(!exact_write(c,fd,0,data,bytes))return 0;
 if(!sync_owned(c,fd))return 0;
 if(!readback(c,fd,data,bytes))return 0;c->result->artifact_bytes[index]=bytes;return 1;
}
static void put_le(uint8_t *p,uint64_t n,unsigned width) {
 for(unsigned i=0;i<width;++i){p[i]=(uint8_t)n;n>>=8;}
}
static uint64_t be40(const uint8_t *p) {
 uint64_t n=0;for(unsigned i=0;i<5;++i)n=(n<<8)|p[i];return n;
}
static int parse_toc(const uint8_t *data,size_t bytes,mb_iostore_header_result_v1 *h) {
 mb_iostore_view_v1 view={sizeof(view),1,{data,bytes<144?bytes:144,0},bytes};
 mb_iostore_limits_v1 limits;mb_iostore_limits_v1_default(&limits);
 return mb_iostore_read_header_v1(&view,&limits,h)==MB_IOSTORE_OK&&h->declared_layout_end==bytes;
}
static int digest_equal(const uint8_t *data,size_t bytes,const uint8_t digest[20]) {
 uint8_t actual[20];blake3_hasher h;blake3_hasher_init(&h);blake3_hasher_update(&h,data,bytes);blake3_hasher_finalize(&h,actual,sizeof(actual));
 return !memcmp(actual,digest,20);
}
static int base_has_id(const mb_toc_overlay_id_row_v1 *rows,uint32_t count,const uint8_t id[12]) {
 uint32_t lo=0,hi=count;while(lo<hi){uint32_t mid=lo+(hi-lo)/2;int cmp=memcmp(rows[mid].id,id,12);
  if(cmp<0)lo=mid+1;else if(cmp>0)hi=mid;else return 1;}return 0;
}
#define CHECK_GATE() do{if(!check_gate(&c))goto done;}while(0)
/* Pure bounded child algorithms are gated at phase boundaries; callbacks and
 * the job's own row loops have finer cooperative checks. No hard preemption. */
#define REQUIRE(condition,status) do{CHECK_GATE();if(!(condition)){fail(&c,status,0);check_gate(&c);goto done;}CHECK_GATE();}while(0)
mb_iostore_file_job_status_v1 mb_iostore_original_file_job_v1(
 const mb_iostore_file_job_params_v1 *params,mb_iostore_file_job_result_v1 *result){
 return mb_iostore_original_file_job_gated_v1(params,result,NULL);
}
mb_iostore_file_job_status_v1 mb_iostore_original_file_job_gated_v1(
 const mb_iostore_file_job_params_v1 *params,mb_iostore_file_job_result_v1 *result,
 const mb_iostore_file_job_control_v1 *control){
 if(!params||!result||spans_overlap(params,sizeof(*params),result,sizeof(*result)))return MB_IOSTORE_FILE_JOB_BAD_ARGUMENT;
 if(params->struct_size!=sizeof(*params)||params->abi_version!=1||params->mod_toc_fd<0||params->mod_ucas_fd<0||
  params->base_toc_fd<0||params->base_header_fd<0||params->destination_dir_fd<0||params->immutable_inputs!=1||
  params->private_unpublished_directory!=1||!params->observed_base_ucas_bytes||params->observed_base_ucas_bytes>INT64_MAX||
  ((params->allocate==NULL)!=(params->release==NULL))||
  !valid_control(control,params,sizeof(*params),result,sizeof(*result),NULL,0))return MB_IOSTORE_FILE_JOB_BAD_ARGUMENT;
 memset(result,0,sizeof(*result));result->struct_size=sizeof(*result);result->abi_version=1;result->failed_artifact_index=UINT32_MAX;
 caller c={.inputs={params->mod_toc_fd,params->mod_ucas_fd,params->base_toc_fd,params->base_header_fd},
  .directory_fd=params->destination_dir_fd,.sidecar_fd=-1,.owned={-1,-1,-1,-1,-1},.input_count=4,.params=params,.result=result,.control=control};
 uint64_t base_ucas_bytes=params->observed_base_ucas_bytes;
 uint8_t *mod_toc=NULL,*base_toc=NULL,*base_header=NULL;size_t mod_bytes=0,base_bytes=0,base_header_bytes=0;
 REQUIRE(preflight_fds(&c),MB_IOSTORE_FILE_JOB_FD_REFUSED);
 result->stage=MB_IOSTORE_FILE_JOB_INPUTS;
 REQUIRE(load_input(&c,0,&mod_toc,&mod_bytes)&&load_input(&c,2,&base_toc,&base_bytes)&&load_input(&c,3,&base_header,&base_header_bytes),
  MB_IOSTORE_FILE_JOB_READ_ERROR);
 result->stage=MB_IOSTORE_FILE_JOB_HEADERS;
 mb_iostore_header_result_v1 mod_h,base_h;
 REQUIRE(parse_toc(mod_toc,mod_bytes,&mod_h)&&parse_toc(base_toc,base_bytes,&base_h),MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 REQUIRE(mod_h.chunk_count&&mod_h.chunk_count<=MB_IOSTORE_NONE_MAX_CHUNKS&&mod_h.partition_count==1&&
  mod_h.directory_index.bytes<=MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint8_t base_header_id[12]={0};put_le(base_header_id,base_h.container_id,8);base_header_id[11]=6;
 uint32_t base_header_row=UINT32_MAX,foreign_header_row=UINT32_MAX,base_matches=0,foreign_matches=0;
 for(uint32_t i=0;i<base_h.chunk_count;++i){if(!(i&63u))CHECK_GATE();
  if(!memcmp(base_toc+(size_t)base_h.chunk_ids.offset+(size_t)i*12,base_header_id,12)){base_header_row=i;++base_matches;}}
 for(uint32_t i=0;i<mod_h.chunk_count;++i){if(!(i&63u))CHECK_GATE();
  if(mod_toc[(size_t)mod_h.chunk_ids.offset+(size_t)i*12+11]==6){foreign_header_row=i;++foreign_matches;}}
 REQUIRE(base_matches==1&&foreign_matches==1,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 REQUIRE(be40(base_toc+(size_t)base_h.chunk_offset_lengths.offset+(size_t)base_header_row*10+5)==base_header_bytes&&
  digest_equal(base_header,base_header_bytes,base_toc+(size_t)base_h.entry_metadata.offset+(size_t)base_header_row*24),MB_IOSTORE_FILE_JOB_HASH_MISMATCH);
 result->stage=MB_IOSTORE_FILE_JOB_PLANS;
 c.plans=alloc(&c,mod_h.chunk_count,sizeof(*c.plans));c.source_rows=alloc(&c,mod_h.chunk_count,sizeof(*c.source_rows));
 mb_iostore_none_chunk_v1 *chunks=alloc(&c,mod_h.chunk_count,sizeof(*chunks));uint8_t *small_ids=alloc(&c,mod_h.chunk_count,12);
 REQUIRE(c.plans&&c.source_rows&&chunks&&small_ids,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 uint64_t source_length=(uint64_t)c.snapshots[1].st_size;
 mb_none_range_source_v1 source={sizeof(source),1,{mod_toc,mod_bytes,0},&source_length,1,0,read_source,&c};
 for(uint32_t i=0;i<mod_h.chunk_count;++i){mb_none_range_result_v1 result;
  REQUIRE(mb_iostore_none_plan_v1(&source,i,c.plans+i,&result)==MB_NONE_RANGE_OK&&result.full_chunk_coverage_scanned,
   MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);}
 size_t foreign_bytes=(size_t)c.plans[foreign_header_row].logical_bytes;
 REQUIRE(foreign_bytes<=CCH_HEADER_BYTES_CAP,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
 uint8_t *foreign_header=alloc(&c,foreign_bytes,1);REQUIRE(foreign_header,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 for(size_t at=0;at<foreign_bytes;){uint32_t n=(uint32_t)(foreign_bytes-at>65536?65536:foreign_bytes-at);mb_none_range_result_v1 result;
  REQUIRE(mb_iostore_none_range_v1(c.plans+foreign_header_row,at,n,foreign_header+at,n,&result)==MB_NONE_RANGE_OK&&result.range_complete,
   MB_IOSTORE_FILE_JOB_READ_ERROR);at+=n;}
 REQUIRE(digest_equal(foreign_header,foreign_bytes,mod_toc+(size_t)mod_h.entry_metadata.offset+(size_t)foreign_header_row*24),
  MB_IOSTORE_FILE_JOB_HASH_MISMATCH);
 result->stage=MB_IOSTORE_FILE_JOB_HEADER_MERGE;
 struct cch_workspace header_ws={0};header_ws.package_ref_capacity=CCH_MAX_REFS;header_ws.range_capacity=CCH_MAX_RANGES;
 header_ws.package_refs=alloc(&c,header_ws.package_ref_capacity,sizeof(*header_ws.package_refs));
 header_ws.ranges=alloc(&c,header_ws.range_capacity,sizeof(*header_ws.ranges));REQUIRE(header_ws.package_refs&&header_ws.ranges,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 struct cch_result header_result;
 REQUIRE(cch_merge_v5(base_header,base_header_bytes,foreign_header,foreign_bytes,&header_ws,NULL,0,&header_result)==CCH_OUTPUT_TOO_SMALL&&
  header_result.required_bytes&&header_result.required_bytes<=CCH_HEADER_BYTES_CAP,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 size_t merged_header_bytes=header_result.required_bytes;uint8_t *merged_header=alloc(&c,merged_header_bytes,1);REQUIRE(merged_header,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 REQUIRE(cch_merge_v5(base_header,base_header_bytes,foreign_header,foreign_bytes,&header_ws,merged_header,merged_header_bytes,&header_result)==CCH_OK&&
  header_result.container_id_preserved&&header_result.base_container_id==base_h.container_id&&header_result.mod_container_id==mod_h.container_id,
  MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 c.merged_header=merged_header;c.merged_header_bytes=merged_header_bytes;
 uint32_t emitted=0;
 for(uint32_t i=0;i<mod_h.chunk_count;++i){if(!(i&63u))CHECK_GATE();if(i!=foreign_header_row){
  memcpy(chunks[emitted].raw_id,c.plans[i].chunk_id,12);chunks[emitted].bytes=c.plans[i].logical_bytes;c.source_rows[emitted]=i;
  memcpy(small_ids+(size_t)emitted*12,chunks[emitted].raw_id,12);++emitted;}}
 memcpy(chunks[emitted].raw_id,base_header_id,12);chunks[emitted].bytes=merged_header_bytes;c.source_rows[emitted]=UINT32_MAX;
 memcpy(small_ids+(size_t)emitted*12,base_header_id,12);++emitted;c.chunk_count=emitted;c.chunks=chunks;
 /* Remap the incoming index to emitted rows using the unchanged source twice.
  * Exact self-collisions collapse without discarding aliases/empty directories.
  * Any path naming the foreign header refuses, as that ID is absent. */
 result->stage=MB_IOSTORE_FILE_JOB_SMALL_DIRECTORY;
 uint8_t *directory_storage=alloc(&c,MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES,1),*directory_paths=alloc(&c,MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES,1);
 uint8_t *small_directory=alloc(&c,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,1),*merged_directory=alloc(&c,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,1);
 REQUIRE(directory_storage&&directory_paths&&small_directory&&merged_directory,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 mb_directory_merge_workspace_v1 directory_ws={sizeof(directory_ws),1,directory_storage,MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES,
  directory_paths,MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES};
 mb_directory_merge_bytes_v1 incoming_directory={mod_toc+(size_t)mod_h.directory_index.offset,(size_t)mod_h.directory_index.bytes};
 mb_directory_merge_ids_v1 incoming_ids={mod_toc+(size_t)mod_h.chunk_ids.offset,mod_h.chunk_count};
 mb_directory_merge_params_v1 directory_params={sizeof(directory_params),1,incoming_directory,incoming_directory,incoming_ids,incoming_ids,{small_ids,emitted}};
 mb_directory_merge_result_v1 small_directory_result;
 REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,small_directory,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&small_directory_result)==MB_DIRECTORY_MERGE_OK&&
  small_directory_result.metadata_complete,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 result->stage=MB_IOSTORE_FILE_JOB_PACK;
 uint8_t *packed_toc=alloc(&c,MB_IOSTORE_NONE_MAX_TOC_BYTES,1),*transfer=alloc(&c,65536,1);uint32_t *sort_rows=alloc(&c,emitted,sizeof(*sort_rows));
 REQUIRE(packed_toc&&transfer&&sort_rows,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 REQUIRE(create_new(&c,0),MB_IOSTORE_FILE_JOB_CREATE_ERROR);c.sidecar_fd=c.owned[0];
 mb_iostore_none_params_v1 pack_params={.struct_size=sizeof(pack_params),.abi_version=1,.chunks=chunks,.chunk_count=emitted,
  .directory_index=small_directory,.directory_bytes=(uint32_t)small_directory_result.output_bytes,.io_context=&c,.read_chunk=read_chunk,.write_ucas=write_sidecar};
 put_le(pack_params.container_id,base_h.container_id,8);
 mb_iostore_none_workspace_v1 pack_ws={sizeof(pack_ws),1,packed_toc,MB_IOSTORE_NONE_MAX_TOC_BYTES,transfer,65536,sort_rows,emitted};
 mb_iostore_none_result_v1 pack_result;
 REQUIRE(mb_iostore_none_pack_v1(&pack_params,&pack_ws,&pack_result)==MB_IOSTORE_NONE_OK&&pack_result.complete,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 if(!sync_owned(&c,c.sidecar_fd))goto done;struct stat sidecar_stat;
 REQUIRE(!fstat(c.sidecar_fd,&sidecar_stat)&&sidecar_stat.st_size>=0&&(uint64_t)sidecar_stat.st_size==pack_result.ucas_bytes,MB_IOSTORE_FILE_JOB_WRITE_ERROR);
 result->artifact_bytes[0]=pack_result.ucas_bytes;
 mb_iostore_header_result_v1 packed_h;REQUIRE(parse_toc(packed_toc,(size_t)pack_result.toc_bytes,&packed_h),MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint32_t assets_verified=0;
 for(uint32_t row=0;row<emitted;++row)if(c.source_rows[row]!=UINT32_MAX){uint32_t inputrow=c.source_rows[row];
  REQUIRE(!memcmp(packed_toc+(size_t)packed_h.entry_metadata.offset+(size_t)row*24,
   mod_toc+(size_t)mod_h.entry_metadata.offset+(size_t)inputrow*24,20),MB_IOSTORE_FILE_JOB_HASH_MISMATCH);++assets_verified;}
 result->stage=MB_IOSTORE_FILE_JOB_OVERLAY;
 uint32_t scratch_count=base_h.block_count>packed_h.block_count?base_h.block_count+1:packed_h.block_count+1;
 if(scratch_count<base_h.overflow_chunk_count)scratch_count=base_h.overflow_chunk_count;
 if(scratch_count<packed_h.overflow_chunk_count)scratch_count=packed_h.overflow_chunk_count;
 mb_toc_overlay_id_row_v1 *base_rows=alloc(&c,base_h.chunk_count,sizeof(*base_rows)),*small_rows=alloc(&c,emitted,sizeof(*small_rows));
 uint32_t *scratch=alloc(&c,scratch_count,sizeof(*scratch));REQUIRE(base_rows&&small_rows&&scratch,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 mb_toc_overlay_workspace_v1 overlay_ws={sizeof(overlay_ws),1,base_rows,small_rows,base_h.chunk_count,emitted,scratch,scratch_count};
 mb_toc_overlay_params_v1 overlay_params={sizeof(overlay_params),1,{base_toc,base_bytes,0},{packed_toc,(size_t)pack_result.toc_bytes,0},
  {base_toc+(size_t)base_h.directory_index.offset,(size_t)base_h.directory_index.bytes,0},base_ucas_bytes,pack_result.ucas_bytes};
 mb_toc_overlay_result_v1 overlay_result;
 REQUIRE(mb_iostore_toc_overlay_v1(&overlay_params,&overlay_ws,NULL,0,&overlay_result)==MB_TOC_OVERLAY_OUTPUT_TOO_SMALL,
  MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint32_t final_count=base_h.chunk_count;
 for(uint32_t i=0;i<emitted;++i){if(!(i&63u))CHECK_GATE();if(!base_has_id(base_rows,base_h.chunk_count,small_rows[i].id))++final_count;}
 REQUIRE(final_count<=MB_IOSTORE_MAX_CHUNKS,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
 uint8_t *final_ids=alloc(&c,final_count,12);REQUIRE(final_ids,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 memcpy(final_ids,base_toc+(size_t)base_h.chunk_ids.offset,(size_t)base_h.chunk_count*12);uint32_t next=base_h.chunk_count;
 for(uint32_t i=0;i<emitted;++i){if(!(i&63u))CHECK_GATE();if(!base_has_id(base_rows,base_h.chunk_count,small_rows[i].id)){
  memcpy(final_ids+(size_t)next*12,small_rows[i].id,12);++next;}}
 result->stage=MB_IOSTORE_FILE_JOB_DIRECTORY;
 directory_params.base_directory=(mb_directory_merge_bytes_v1){base_toc+(size_t)base_h.directory_index.offset,(size_t)base_h.directory_index.bytes};
 directory_params.mod_directory=incoming_directory;directory_params.base_ids=(mb_directory_merge_ids_v1){base_toc+(size_t)base_h.chunk_ids.offset,base_h.chunk_count};
 directory_params.mod_ids=incoming_ids;directory_params.final_ids=(mb_directory_merge_ids_v1){final_ids,final_count};
 mb_directory_merge_result_v1 directory_result;
 REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,merged_directory,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&directory_result)==MB_DIRECTORY_MERGE_OK&&
  directory_result.metadata_complete,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint64_t final_toc_bytes=overlay_result.required_output_bytes-base_h.directory_index.bytes+directory_result.output_bytes;
 REQUIRE(final_toc_bytes&&final_toc_bytes<=MB_IOSTORE_MAX_TOC_BYTES,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
 uint8_t *final_toc=alloc(&c,(size_t)final_toc_bytes,1);REQUIRE(final_toc,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 overlay_params.merged_directory=(mb_iostore_span_v1){merged_directory,(size_t)directory_result.output_bytes,0};
 REQUIRE(mb_iostore_toc_overlay_v1(&overlay_params,&overlay_ws,final_toc,(size_t)final_toc_bytes,&overlay_result)==MB_TOC_OVERLAY_OK&&
  overlay_result.metadata_complete&&overlay_result.output_bytes==final_toc_bytes,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 result->stage=MB_IOSTORE_FILE_JOB_METADATA;
 REQUIRE(write_new(&c,1,packed_toc,(size_t)pack_result.toc_bytes)&&
  write_new(&c,2,merged_header,merged_header_bytes)&&
  write_new(&c,3,merged_directory,(size_t)directory_result.output_bytes)&&
  write_new(&c,4,final_toc,(size_t)final_toc_bytes),MB_IOSTORE_FILE_JOB_WRITE_ERROR);
 result->stage=MB_IOSTORE_FILE_JOB_SNAPSHOT;
 REQUIRE(stable_inputs(&c),MB_IOSTORE_FILE_JOB_SOURCE_CHANGED);
 result->original_mod_chunks=mod_h.chunk_count;result->emitted_chunks=emitted;
 result->incoming_asset_hashes_verified=assets_verified;result->source_header_hash_verified=1;result->captured_base_header_hash_verified=1;
 result->base_container_id=base_h.container_id;result->merged_packages=header_result.merged_package_count;
 result->replaced_packages=header_result.replaced_package_count;result->added_packages=header_result.added_package_count;
 result->sidecar_blocks=pack_result.block_count;result->directory_files=directory_result.merged_files;
 result->directory_overrides=directory_result.exact_path_overrides;result->final_chunks=overlay_result.final_chunks;
 result->source_snapshots_stable=1;result->failed_artifact_index=UINT32_MAX;
 done:
 cleanup(&c);
 check_gate(&c);
 if(result->status==MB_IOSTORE_FILE_JOB_OK&&!c.stopped){result->prepared=1;result->stage=MB_IOSTORE_FILE_JOB_COMPLETE;}
 return (mb_iostore_file_job_status_v1)result->status;
}

typedef struct batch_read_context {caller *job;unsigned input;} batch_read_context;
typedef struct batch_mod {
 uint8_t *toc,*header;size_t toc_bytes,header_bytes;
 mb_iostore_header_result_v1 geometry;
 uint32_t header_row,plan_base;
 batch_read_context read_context;
} batch_mod;
typedef struct batch_id_row {uint8_t id[12];uint32_t source,row;} batch_id_row;
static int compare_batch_ids(const void *a,const void *b){return memcmp(((const batch_id_row *)a)->id,((const batch_id_row *)b)->id,12);}
static int batch_read_source(void *opaque,uint32_t partition,uint64_t offset,uint8_t *out,size_t requested,size_t *actual){
 batch_read_context *provider=opaque;caller *c=provider->job;unsigned input=provider->input;*actual=0;
 if(partition||offset>(uint64_t)c->snapshots[input].st_size||requested>(uint64_t)c->snapshots[input].st_size-offset||
  !exact_read(c,c->inputs[input],offset,out,requested))return 1;
 ++c->source_reads;c->source_bytes+=requested;*actual=requested;return 0;
}
static int batch_conflict(caller *c,uint32_t kind,uint32_t a,uint32_t b,const uint8_t *id){
 if(a>b){uint32_t swap=a;a=b;b=swap;}
 c->batch_result->conflict_kind=kind;c->batch_result->conflict_source_a=a;c->batch_result->conflict_source_b=b;
 if(id)memcpy(c->batch_result->conflict_chunk_id,id,12);return fail(c,MB_IOSTORE_FILE_JOB_CONFLICT,0);
}
mb_iostore_file_job_status_v1 mb_iostore_batch_file_job_v1(
 const mb_iostore_batch_file_job_params_v1 *params,mb_iostore_batch_file_job_result_v1 *batch_result){
 return mb_iostore_batch_file_job_gated_v1(params,batch_result,NULL);
}
mb_iostore_file_job_status_v1 mb_iostore_batch_file_job_gated_v1(
 const mb_iostore_batch_file_job_params_v1 *params,mb_iostore_batch_file_job_result_v1 *batch_result,
 const mb_iostore_file_job_control_v1 *control){
 if(!params||!batch_result||spans_overlap(params,sizeof(*params),batch_result,sizeof(*batch_result)))return MB_IOSTORE_FILE_JOB_BAD_ARGUMENT;
 if(params->struct_size!=sizeof(*params)||params->abi_version!=1||!params->sources||!params->source_count||
  params->source_count>MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES||params->base_toc_fd<0||params->base_header_fd<0||
  params->destination_dir_fd<0||params->immutable_inputs!=1||params->private_unpublished_directory!=1||
  !params->observed_base_ucas_bytes||params->observed_base_ucas_bytes>INT64_MAX||
  ((params->allocate==NULL)!=(params->release==NULL))||
  spans_overlap(params->sources,(size_t)params->source_count*sizeof(*params->sources),batch_result,sizeof(*batch_result))||
  !valid_control(control,params,sizeof(*params),batch_result,sizeof(*batch_result),params->sources,
   (size_t)params->source_count*sizeof(*params->sources)))
  return MB_IOSTORE_FILE_JOB_BAD_ARGUMENT;
 for(uint32_t s=0;s<params->source_count;++s)if(params->sources[s].mod_toc_fd<0||params->sources[s].mod_ucas_fd<0)
  return MB_IOSTORE_FILE_JOB_BAD_ARGUMENT;
 memset(batch_result,0,sizeof(*batch_result));batch_result->struct_size=sizeof(*batch_result);batch_result->abi_version=1;
 batch_result->source_count=params->source_count;batch_result->conflict_source_a=UINT32_MAX;batch_result->conflict_source_b=UINT32_MAX;
 mb_iostore_file_job_result_v1 *result=&batch_result->preparation;
 result->struct_size=sizeof(*result);result->abi_version=1;result->failed_artifact_index=UINT32_MAX;
 mb_iostore_file_job_params_v1 common={.struct_size=sizeof(common),.abi_version=1,.io_context=params->io_context,
  .read_at=params->read_at,.write_at=params->write_at,.allocator_context=params->allocator_context,.allocate=params->allocate,.release=params->release};
 caller c={.directory_fd=params->destination_dir_fd,.sidecar_fd=-1,.owned={-1,-1,-1,-1,-1},
  .input_count=2u*params->source_count+2u,.batch_result=batch_result,.params=&common,.result=result,.control=control};
 unsigned base_input=2u*params->source_count,header_input=base_input+1u;
 for(uint32_t s=0;s<params->source_count;++s){c.inputs[2u*s]=params->sources[s].mod_toc_fd;c.inputs[2u*s+1u]=params->sources[s].mod_ucas_fd;
  c.input_caps[2u*s]=MB_IOSTORE_MAX_TOC_BYTES;c.input_caps[2u*s+1u]=MB_IOSTORE_NONE_MAX_PAYLOAD_BYTES;}
 c.inputs[base_input]=params->base_toc_fd;c.inputs[header_input]=params->base_header_fd;
 c.input_caps[base_input]=MB_IOSTORE_MAX_TOC_BYTES;c.input_caps[header_input]=CCH_HEADER_BYTES_CAP;
 batch_mod mods[MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES]={0};
 uint8_t *base_toc=NULL,*base_header=NULL;size_t base_bytes=0,base_header_bytes=0;
 REQUIRE(preflight_fds(&c),MB_IOSTORE_FILE_JOB_FD_REFUSED);
 result->stage=MB_IOSTORE_FILE_JOB_INPUTS;
 REQUIRE(load_input(&c,base_input,&base_toc,&base_bytes)&&load_input(&c,header_input,&base_header,&base_header_bytes),MB_IOSTORE_FILE_JOB_READ_ERROR);
 for(uint32_t s=0;s<params->source_count;++s)REQUIRE(load_input(&c,2u*s,&mods[s].toc,&mods[s].toc_bytes),MB_IOSTORE_FILE_JOB_READ_ERROR);
 result->stage=MB_IOSTORE_FILE_JOB_HEADERS;
 mb_iostore_header_result_v1 base_h;REQUIRE(parse_toc(base_toc,base_bytes,&base_h),MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint8_t base_header_id[12]={0};put_le(base_header_id,base_h.container_id,8);base_header_id[11]=6;
 uint32_t base_header_row=UINT32_MAX,base_matches=0;
 for(uint32_t i=0;i<base_h.chunk_count;++i){if(!(i&63u))CHECK_GATE();
  if(!memcmp(base_toc+(size_t)base_h.chunk_ids.offset+(size_t)i*12,base_header_id,12)){base_header_row=i;++base_matches;}}
 REQUIRE(base_matches==1,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 REQUIRE(be40(base_toc+(size_t)base_h.chunk_offset_lengths.offset+(size_t)base_header_row*10+5)==base_header_bytes&&
  digest_equal(base_header,base_header_bytes,base_toc+(size_t)base_h.entry_metadata.offset+(size_t)base_header_row*24),MB_IOSTORE_FILE_JOB_HASH_MISMATCH);
 uint32_t planned_count=0,emitted=1;
 for(uint32_t s=0;s<params->source_count;++s){batch_mod *m=mods+s;
  REQUIRE(parse_toc(m->toc,m->toc_bytes,&m->geometry),MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  REQUIRE(m->geometry.chunk_count&&m->geometry.chunk_count<=MB_IOSTORE_NONE_MAX_CHUNKS&&m->geometry.partition_count==1&&
   m->geometry.directory_index.bytes<=MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  uint32_t count=0;m->header_row=UINT32_MAX;
  for(uint32_t i=0;i<m->geometry.chunk_count;++i){if(!(i&63u))CHECK_GATE();
   if(m->toc[(size_t)m->geometry.chunk_ids.offset+(size_t)i*12+11]==6){m->header_row=i;++count;}}
  REQUIRE(count==1,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  REQUIRE(m->geometry.chunk_count-1u<=MB_IOSTORE_NONE_MAX_CHUNKS-emitted,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
  emitted+=m->geometry.chunk_count-1u;m->plan_base=planned_count;planned_count+=m->geometry.chunk_count;
 }
 result->stage=MB_IOSTORE_FILE_JOB_PLANS;
 c.plans=alloc(&c,planned_count,sizeof(*c.plans));c.source_rows=alloc(&c,emitted,sizeof(*c.source_rows));
 mb_iostore_none_chunk_v1 *chunks=alloc(&c,emitted,sizeof(*chunks));uint8_t *small_ids=alloc(&c,emitted,12);
 batch_id_row *id_rows=alloc(&c,emitted,sizeof(*id_rows));uint32_t *owners=alloc(&c,emitted,sizeof(*owners));
 REQUIRE(c.plans&&c.source_rows&&chunks&&small_ids&&id_rows&&owners,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 uint32_t next=0;
 for(uint32_t s=0;s<params->source_count;++s){batch_mod *m=mods+s;m->read_context=(batch_read_context){&c,2u*s+1u};
  uint64_t length=(uint64_t)c.snapshots[2u*s+1u].st_size;
  mb_none_range_source_v1 source={sizeof(source),1,{m->toc,m->toc_bytes,0},&length,1,0,batch_read_source,&m->read_context};
  for(uint32_t row=0;row<m->geometry.chunk_count;++row){mb_none_range_result_v1 planned;
   REQUIRE(mb_iostore_none_plan_v1(&source,row,c.plans+m->plan_base+row,&planned)==MB_NONE_RANGE_OK&&planned.full_chunk_coverage_scanned,
    MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
   if(row!=m->header_row){const mb_none_range_plan_v1 *plan=c.plans+m->plan_base+row;
    memcpy(chunks[next].raw_id,plan->chunk_id,12);chunks[next].bytes=plan->logical_bytes;c.source_rows[next]=m->plan_base+row;
    memcpy(small_ids+(size_t)next*12,plan->chunk_id,12);memcpy(id_rows[next].id,plan->chunk_id,12);
    id_rows[next].source=s;id_rows[next].row=row;owners[next]=s;++next;}
  }
 }
 CHECK_GATE();qsort(id_rows,next,sizeof(*id_rows),compare_batch_ids);CHECK_GATE();
 for(uint32_t i=1;i<next;++i){if(!(i&63u))CHECK_GATE();if(!memcmp(id_rows[i-1].id,id_rows[i].id,12)){
  if(id_rows[i-1].source==id_rows[i].source){fail(&c,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED,0);goto done;}
  batch_conflict(&c,MB_IOSTORE_BATCH_CONFLICT_CHUNK_ID,id_rows[i-1].source,id_rows[i].source,id_rows[i].id);goto done;
 }}
 for(uint32_t s=0;s<params->source_count;++s){batch_mod *m=mods+s;
  m->header_bytes=(size_t)c.plans[m->plan_base+m->header_row].logical_bytes;
  REQUIRE(m->header_bytes<=CCH_HEADER_BYTES_CAP,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
  m->header=alloc(&c,m->header_bytes,1);REQUIRE(m->header,MB_IOSTORE_FILE_JOB_NO_MEMORY);
  for(size_t at=0;at<m->header_bytes;){uint32_t n=(uint32_t)(m->header_bytes-at>65536?65536:m->header_bytes-at);mb_none_range_result_v1 read;
   REQUIRE(mb_iostore_none_range_v1(c.plans+m->plan_base+m->header_row,at,n,m->header+at,n,&read)==MB_NONE_RANGE_OK&&read.range_complete,
    MB_IOSTORE_FILE_JOB_READ_ERROR);at+=n;}
  REQUIRE(digest_equal(m->header,m->header_bytes,m->toc+(size_t)m->geometry.entry_metadata.offset+(size_t)m->header_row*24),MB_IOSTORE_FILE_JOB_HASH_MISMATCH);
 }
 result->stage=MB_IOSTORE_FILE_JOB_HEADER_MERGE;
 struct cch_workspace header_ws={0};header_ws.package_ref_capacity=CCH_MAX_REFS;header_ws.range_capacity=CCH_MAX_RANGES;
 header_ws.package_refs=alloc(&c,header_ws.package_ref_capacity,sizeof(*header_ws.package_refs));header_ws.ranges=alloc(&c,header_ws.range_capacity,sizeof(*header_ws.ranges));
 REQUIRE(header_ws.package_refs&&header_ws.ranges,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 struct cch_result header_result;
 for(uint32_t a=0;a<params->source_count;++a)for(uint32_t b=a+1u;b<params->source_count;++b){
  REQUIRE(cch_merge_v5(mods[a].header,mods[a].header_bytes,mods[b].header,mods[b].header_bytes,&header_ws,NULL,0,&header_result)==CCH_OUTPUT_TOO_SMALL,
   MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  if(header_result.replaced_package_count){batch_conflict(&c,MB_IOSTORE_BATCH_CONFLICT_PACKAGE_ID,a,b,NULL);goto done;}
 }
 const uint8_t *merged_header=base_header;size_t merged_header_bytes=base_header_bytes;uint32_t replacements=0,additions=0;
 for(uint32_t s=0;s<params->source_count;++s){batch_mod *m=mods+s;
  REQUIRE(cch_merge_v5(merged_header,merged_header_bytes,m->header,m->header_bytes,&header_ws,NULL,0,&header_result)==CCH_OUTPUT_TOO_SMALL&&
   header_result.required_bytes&&header_result.required_bytes<=CCH_HEADER_BYTES_CAP,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  size_t wanted=header_result.required_bytes;uint8_t *combined=alloc(&c,wanted,1);REQUIRE(combined,MB_IOSTORE_FILE_JOB_NO_MEMORY);
  REQUIRE(cch_merge_v5(merged_header,merged_header_bytes,m->header,m->header_bytes,&header_ws,combined,wanted,&header_result)==CCH_OK&&
   header_result.container_id_preserved&&header_result.base_container_id==base_h.container_id&&header_result.mod_container_id==m->geometry.container_id,
   MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  replacements+=header_result.replaced_package_count;additions+=header_result.added_package_count;merged_header=combined;merged_header_bytes=wanted;
 }
 c.merged_header=merged_header;c.merged_header_bytes=merged_header_bytes;
 memcpy(chunks[next].raw_id,base_header_id,12);chunks[next].bytes=merged_header_bytes;c.source_rows[next]=UINT32_MAX;
 memcpy(small_ids+(size_t)next*12,base_header_id,12);owners[next]=UINT32_MAX;++next;
 REQUIRE(next==emitted,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);c.chunks=chunks;c.chunk_count=emitted;
 uint64_t total_content=0;
 for(uint32_t row=0;row<emitted;++row){REQUIRE(chunks[row].bytes<=MB_IOSTORE_NONE_MAX_PAYLOAD_BYTES-total_content,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
  total_content+=chunks[row].bytes;}
 result->stage=MB_IOSTORE_FILE_JOB_SMALL_DIRECTORY;
 uint8_t *directory_storage=alloc(&c,MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES,1),*directory_paths=alloc(&c,MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES,1);
 uint8_t *directory_a=alloc(&c,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,1),*directory_b=alloc(&c,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,1),
  *merged_directory=alloc(&c,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,1);
 REQUIRE(directory_storage&&directory_paths&&directory_a&&directory_b&&merged_directory,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 mb_directory_merge_workspace_v1 directory_ws={sizeof(directory_ws),1,directory_storage,MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES,directory_paths,MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES};
 mb_directory_merge_bytes_v1 mod_directories[MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES];mb_directory_merge_ids_v1 mod_ids[MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES];
 for(uint32_t s=0;s<params->source_count;++s){mod_directories[s]=(mb_directory_merge_bytes_v1){mods[s].toc+(size_t)mods[s].geometry.directory_index.offset,(size_t)mods[s].geometry.directory_index.bytes};
  mod_ids[s]=(mb_directory_merge_ids_v1){mods[s].toc+(size_t)mods[s].geometry.chunk_ids.offset,mods[s].geometry.chunk_count};}
 mb_directory_merge_params_v1 directory_params={.struct_size=sizeof(directory_params),.abi_version=1,.final_ids={small_ids,emitted}};
 mb_directory_merge_result_v1 directory_result;
 for(uint32_t a=0;a<params->source_count;++a)for(uint32_t b=a+1u;b<params->source_count;++b){
  directory_params.base_directory=mod_directories[a];directory_params.mod_directory=mod_directories[b];directory_params.base_ids=mod_ids[a];directory_params.mod_ids=mod_ids[b];
  REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,merged_directory,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&directory_result)==MB_DIRECTORY_MERGE_OK&&
   directory_result.metadata_complete,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  if(directory_result.exact_path_overrides){batch_conflict(&c,MB_IOSTORE_BATCH_CONFLICT_DIRECTORY_PATH,a,b,NULL);goto done;}
 }
 directory_params.base_directory=mod_directories[0];directory_params.mod_directory=mod_directories[0];directory_params.base_ids=mod_ids[0];directory_params.mod_ids=mod_ids[0];
 REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,directory_a,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&directory_result)==MB_DIRECTORY_MERGE_OK&&
  directory_result.metadata_complete,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint8_t *small_directory=directory_a,*spare_directory=directory_b;size_t small_directory_bytes=(size_t)directory_result.output_bytes;
 for(uint32_t s=1;s<params->source_count;++s){directory_params.base_directory=(mb_directory_merge_bytes_v1){small_directory,small_directory_bytes};
  directory_params.mod_directory=mod_directories[s];directory_params.base_ids=(mb_directory_merge_ids_v1){small_ids,emitted};directory_params.mod_ids=mod_ids[s];
  REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,spare_directory,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&directory_result)==MB_DIRECTORY_MERGE_OK&&
   directory_result.metadata_complete&&!directory_result.exact_path_overrides,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
  uint8_t *old=small_directory;small_directory=spare_directory;spare_directory=old;small_directory_bytes=(size_t)directory_result.output_bytes;
 }
 result->stage=MB_IOSTORE_FILE_JOB_PACK;
 uint8_t *packed_toc=alloc(&c,MB_IOSTORE_NONE_MAX_TOC_BYTES,1),*transfer=alloc(&c,65536,1);uint32_t *sort_rows=alloc(&c,emitted,sizeof(*sort_rows));
 REQUIRE(packed_toc&&transfer&&sort_rows,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 REQUIRE(create_new(&c,0),MB_IOSTORE_FILE_JOB_CREATE_ERROR);c.sidecar_fd=c.owned[0];
 mb_iostore_none_params_v1 pack_params={.struct_size=sizeof(pack_params),.abi_version=1,.chunks=chunks,.chunk_count=emitted,
  .directory_index=small_directory,.directory_bytes=(uint32_t)small_directory_bytes,.io_context=&c,.read_chunk=read_chunk,.write_ucas=write_sidecar};
 put_le(pack_params.container_id,base_h.container_id,8);
 mb_iostore_none_workspace_v1 pack_ws={sizeof(pack_ws),1,packed_toc,MB_IOSTORE_NONE_MAX_TOC_BYTES,transfer,65536,sort_rows,emitted};mb_iostore_none_result_v1 pack_result;
 REQUIRE(mb_iostore_none_pack_v1(&pack_params,&pack_ws,&pack_result)==MB_IOSTORE_NONE_OK&&pack_result.complete,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 if(!sync_owned(&c,c.sidecar_fd))goto done;struct stat sidecar_stat;
 REQUIRE(!fstat(c.sidecar_fd,&sidecar_stat)&&sidecar_stat.st_size>=0&&(uint64_t)sidecar_stat.st_size==pack_result.ucas_bytes,MB_IOSTORE_FILE_JOB_WRITE_ERROR);
 result->artifact_bytes[0]=pack_result.ucas_bytes;
 mb_iostore_header_result_v1 packed_h;REQUIRE(parse_toc(packed_toc,(size_t)pack_result.toc_bytes,&packed_h),MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint32_t assets_verified=0;
 for(uint32_t row=0;row<emitted;++row)if(c.source_rows[row]!=UINT32_MAX){batch_mod *m=mods+owners[row];uint32_t inputrow=c.source_rows[row]-m->plan_base;
  REQUIRE(!memcmp(packed_toc+(size_t)packed_h.entry_metadata.offset+(size_t)row*24,
   m->toc+(size_t)m->geometry.entry_metadata.offset+(size_t)inputrow*24,20),MB_IOSTORE_FILE_JOB_HASH_MISMATCH);++assets_verified;}
 result->stage=MB_IOSTORE_FILE_JOB_OVERLAY;
 uint32_t scratch_count=base_h.block_count>packed_h.block_count?base_h.block_count+1:packed_h.block_count+1;
 if(scratch_count<base_h.overflow_chunk_count)scratch_count=base_h.overflow_chunk_count;
 if(scratch_count<packed_h.overflow_chunk_count)scratch_count=packed_h.overflow_chunk_count;
 mb_toc_overlay_id_row_v1 *base_rows=alloc(&c,base_h.chunk_count,sizeof(*base_rows)),*small_rows=alloc(&c,emitted,sizeof(*small_rows));
 uint32_t *scratch=alloc(&c,scratch_count,sizeof(*scratch));REQUIRE(base_rows&&small_rows&&scratch,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 mb_toc_overlay_workspace_v1 overlay_ws={sizeof(overlay_ws),1,base_rows,small_rows,base_h.chunk_count,emitted,scratch,scratch_count};
 mb_toc_overlay_params_v1 overlay_params={sizeof(overlay_params),1,{base_toc,base_bytes,0},{packed_toc,(size_t)pack_result.toc_bytes,0},
  {base_toc+(size_t)base_h.directory_index.offset,(size_t)base_h.directory_index.bytes,0},params->observed_base_ucas_bytes,pack_result.ucas_bytes};mb_toc_overlay_result_v1 overlay_result;
 REQUIRE(mb_iostore_toc_overlay_v1(&overlay_params,&overlay_ws,NULL,0,&overlay_result)==MB_TOC_OVERLAY_OUTPUT_TOO_SMALL,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint32_t final_count=base_h.chunk_count;
 for(uint32_t i=0;i<emitted;++i){if(!(i&63u))CHECK_GATE();if(!base_has_id(base_rows,base_h.chunk_count,small_rows[i].id))++final_count;}
 REQUIRE(final_count<=MB_IOSTORE_MAX_CHUNKS,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
 uint8_t *final_ids=alloc(&c,final_count,12);REQUIRE(final_ids,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 memcpy(final_ids,base_toc+(size_t)base_h.chunk_ids.offset,(size_t)base_h.chunk_count*12);next=base_h.chunk_count;
 for(uint32_t i=0;i<emitted;++i){if(!(i&63u))CHECK_GATE();
  if(!base_has_id(base_rows,base_h.chunk_count,small_rows[i].id)){memcpy(final_ids+(size_t)next*12,small_rows[i].id,12);++next;}}
 result->stage=MB_IOSTORE_FILE_JOB_DIRECTORY;
 directory_params.base_directory=(mb_directory_merge_bytes_v1){base_toc+(size_t)base_h.directory_index.offset,(size_t)base_h.directory_index.bytes};
 directory_params.mod_directory=(mb_directory_merge_bytes_v1){small_directory,small_directory_bytes};
 directory_params.base_ids=(mb_directory_merge_ids_v1){base_toc+(size_t)base_h.chunk_ids.offset,base_h.chunk_count};directory_params.mod_ids=(mb_directory_merge_ids_v1){small_ids,emitted};
 directory_params.final_ids=(mb_directory_merge_ids_v1){final_ids,final_count};
 REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,merged_directory,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&directory_result)==MB_DIRECTORY_MERGE_OK&&
  directory_result.metadata_complete,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 uint64_t final_toc_bytes=overlay_result.required_output_bytes-base_h.directory_index.bytes+directory_result.output_bytes;
 REQUIRE(final_toc_bytes&&final_toc_bytes<=MB_IOSTORE_MAX_TOC_BYTES,MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT);
 uint8_t *final_toc=alloc(&c,(size_t)final_toc_bytes,1);REQUIRE(final_toc,MB_IOSTORE_FILE_JOB_NO_MEMORY);
 overlay_params.merged_directory=(mb_iostore_span_v1){merged_directory,(size_t)directory_result.output_bytes,0};
 REQUIRE(mb_iostore_toc_overlay_v1(&overlay_params,&overlay_ws,final_toc,(size_t)final_toc_bytes,&overlay_result)==MB_TOC_OVERLAY_OK&&
  overlay_result.metadata_complete&&overlay_result.output_bytes==final_toc_bytes,MB_IOSTORE_FILE_JOB_FORMAT_REFUSED);
 result->stage=MB_IOSTORE_FILE_JOB_METADATA;
 REQUIRE(write_new(&c,1,packed_toc,(size_t)pack_result.toc_bytes)&&write_new(&c,2,merged_header,merged_header_bytes)&&
  write_new(&c,3,merged_directory,(size_t)directory_result.output_bytes)&&write_new(&c,4,final_toc,(size_t)final_toc_bytes),MB_IOSTORE_FILE_JOB_WRITE_ERROR);
 result->stage=MB_IOSTORE_FILE_JOB_SNAPSHOT;REQUIRE(stable_inputs(&c),MB_IOSTORE_FILE_JOB_SOURCE_CHANGED);
 result->original_mod_chunks=planned_count;result->emitted_chunks=emitted;result->incoming_asset_hashes_verified=assets_verified;
 result->source_header_hash_verified=1;result->captured_base_header_hash_verified=1;result->base_container_id=base_h.container_id;
 result->merged_packages=header_result.merged_package_count;result->replaced_packages=replacements;result->added_packages=additions;
 result->sidecar_blocks=pack_result.block_count;result->directory_files=directory_result.merged_files;result->directory_overrides=directory_result.exact_path_overrides;
 result->final_chunks=overlay_result.final_chunks;result->source_snapshots_stable=1;result->failed_artifact_index=UINT32_MAX;
 done:cleanup(&c);
 check_gate(&c);
 if(result->status==MB_IOSTORE_FILE_JOB_OK&&!c.stopped){result->prepared=1;result->stage=MB_IOSTORE_FILE_JOB_COMPLETE;}
 return (mb_iostore_file_job_status_v1)result->status;
}
