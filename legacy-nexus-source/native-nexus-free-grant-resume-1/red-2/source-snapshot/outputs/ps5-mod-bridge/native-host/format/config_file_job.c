#define _POSIX_C_SOURCE 200809L
#include "config_file_job.h"
#include "pak_primary_capture.h"
#include "pak_secondary_integrity.h"
#include "pak_directory_parser.h"
#include "pak_physical_header.h"
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

typedef struct input_file { uint64_t data_offset,bytes;uint32_t replacement; } input_file;
typedef struct owned_allocation {void *pointer;size_t bytes;} owned_allocation;
typedef struct job_state {
 const pm_config_file_job_params_v1 *params;
 pm_config_file_job_result_v1 *result;
 owned_allocation allocations[24];uint32_t allocation_count;
 uint64_t allocation_live;
 input_file *inputs;uint32_t input_count;
 const uint8_t *merged;uint64_t merged_bytes;
 mb_pak_prepare_gate_v1 gate;void *gate_context;uint32_t stopped;
} job_state;
static int spans_overlap(const void *a,size_t an,const void *b,size_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;
 return x<y+bn&&y<x+an;
}
static int same_inode(const struct stat *a,const struct stat *b) {
 return a->st_dev==b->st_dev&&a->st_ino==b->st_ino;
}
static int stable(const struct stat *a,const struct stat *b) {
 if(!same_inode(a,b)||a->st_size!=b->st_size)return 0;
#ifdef __APPLE__
 return a->st_mtime==b->st_mtime&&a->st_mtimensec==b->st_mtimensec&&
 a->st_ctime==b->st_ctime&&a->st_ctimensec==b->st_ctimensec;
#else
 return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
 a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int gate_stopped(void *context) {
 job_state *job=context;
 if(job->stopped||(job->gate&&job->gate(job->gate_context))){job->stopped=1;return 1;}
 return 0;
}
static int check_gate(job_state *job) {return !gate_stopped(job);}
static void *job_alloc(job_state *job,size_t count,size_t width) {
 if(!check_gate(job))return NULL;
 if(!count||!width||count>SIZE_MAX/width||job->allocation_count>=24)return NULL;
 size_t bytes=count*width;
 if(bytes>PM_CONFIG_FILE_JOB_MAX_ALLOCATED_BYTES-job->allocation_live)return NULL;
 ++job->result->allocation_calls;
 void *p=job->params->allocate(job->params->allocator_context,bytes);
 if(!p){(void)check_gate(job);return NULL;}
 /* Register ownership before a post-callback stop; cleanup must release a
  * returned allocation even when the operation ends before initialization. */
 job->allocations[job->allocation_count++]=(owned_allocation){p,bytes};
 job->allocation_live+=bytes;
 if(job->allocation_live>job->result->allocation_peak_bytes)job->result->allocation_peak_bytes=job->allocation_live;
 if(!check_gate(job))return NULL;
 memset(p,0,bytes);
 if(!check_gate(job))return NULL;
 return p;
}
static int reserve_io(job_state *job,uint32_t n,int write) {
 pm_config_file_job_result_v1 *r=job->result;
 uint64_t total=r->requested_read_bytes+r->requested_write_bytes;
 if(n>MB_PAK_STREAM_TRANSFER_BYTES||total>PM_CONFIG_FILE_JOB_MAX_IO_BYTES||
 n>PM_CONFIG_FILE_JOB_MAX_IO_BYTES-total||r->read_calls+r->write_calls>=PM_CONFIG_FILE_JOB_MAX_IO_CALLS)return 1;
 if(write){r->requested_write_bytes+=n;++r->write_calls;}
 else {r->requested_read_bytes+=n;++r->read_calls;}
 return 0;
}
static int exact_read(job_state *job,int fd,uint64_t bytes,uint64_t offset,uint8_t *dst,uint32_t n,uint32_t *actual) {
 *actual=0;
 if(!check_gate(job)||offset>bytes||n>bytes-offset||reserve_io(job,n,0))return 1;
 int rc=job->params->read_at(job->params->io_context,fd,offset,dst,n,actual);
 if(!check_gate(job))return 1;
 return rc||*actual!=n;
}
static pak_provider_status original_read(void *ctx,uint64_t offset,uint8_t *dst,uint32_t n,uint32_t *actual) {
 job_state *job=ctx;
 int failed=exact_read(job,job->params->original_fd,job->params->original_bytes,offset,dst,n,actual);
 return job->stopped?PAK_PROVIDER_CANCELLED:failed?PAK_PROVIDER_IO_ERROR:PAK_PROVIDER_OK;
}
static int payload_read(void *ctx,uint32_t index,uint64_t offset,uint8_t *dst,uint32_t n,uint32_t *actual) {
 job_state *job=ctx;*actual=0;
 if(!check_gate(job)||index>=job->input_count)return 1;
 const input_file *f=&job->inputs[index];if(offset>f->bytes||n>f->bytes-offset)return 1;
 if(f->replacement){
  if(offset>job->merged_bytes||n>job->merged_bytes-offset)return 1;
  memcpy(dst,job->merged+(size_t)offset,n);*actual=n;return check_gate(job)?0:1;
 }
 return exact_read(job,job->params->original_fd,job->params->original_bytes,f->data_offset+offset,dst,n,actual);
}
static int output_write(void *ctx,uint64_t offset,const uint8_t *src,uint32_t n,uint32_t *actual) {
 job_state *job=ctx;*actual=0;
 if(!check_gate(job)||offset>MB_PAK_STREAM_MAX_ARCHIVE_BYTES||n>MB_PAK_STREAM_MAX_ARCHIVE_BYTES-offset||reserve_io(job,n,1))return 1;
 int rc=job->params->write_at(job->params->io_context,job->params->output_fd,offset,src,n,actual);
 if(!check_gate(job))return 1;
 return rc||*actual!=n;
}
static int read_all(job_state *job,int fd,uint64_t bytes,uint64_t offset,uint8_t *dst,size_t n) {
 size_t at=0;
 while(at<n){uint32_t requested=(uint32_t)(n-at>MB_PAK_STREAM_TRANSFER_BYTES?MB_PAK_STREAM_TRANSFER_BYTES:n-at),actual=0;
  if(exact_read(job,fd,bytes,offset+at,dst+at,requested,&actual))return 1;
  at+=requested;
 }
 return 0;
}
#define STOP(code) do {status=(code);goto cleanup;} while(0)
#define CHECK_GATE() do {if(!check_gate(&job))STOP(PM_CONFIG_FILE_JOB_STOPPED);} while(0)
#define ALLOC(type,name,count) type *name=job_alloc(&job,(count),sizeof(type));if(!(name))STOP(PM_CONFIG_FILE_JOB_NO_MEMORY)
#define ASSIGN_ALLOC(dest,count,width) do {(dest)=job_alloc(&job,(count),(width));if(!(dest))STOP(PM_CONFIG_FILE_JOB_NO_MEMORY);} while(0)

static int valid_params(const pm_config_file_job_params_v1 *p,void *output,size_t output_bytes,int automatic) {
 if(!p||!output||spans_overlap(p,sizeof(*p),output,output_bytes))return 0;
 if(p->struct_size!=sizeof(*p)||p->abi_version!=1||p->reserved||p->original_fd<0||p->member_fd<0||p->output_fd<0||
 p->immutable_inputs!=1||p->unpublished_output!=1||!p->read_at||!p->write_at||!p->allocate||!p->release||
 !p->original_bytes||p->original_bytes>MB_PAK_STREAM_MAX_ARCHIVE_BYTES||p->member_bytes>PM_INI_MAX_INPUT_BYTES||
 !p->sections||!p->section_count||p->section_count>PM_INI_MAX_SECTIONS)return 0;
 if(automatic){if(p->target_entry.data||p->target_entry.size||p->mount.data||p->mount.size||p->path_hash_seed)return 0;}
 else if(!p->target_entry.data||!p->target_entry.size||p->target_entry.size>MB_PAK_STREAM_MAX_PATH_BYTES||
 !p->mount.data||!p->mount.size||p->mount.size>PAK_READER_MAX_MOUNT_BYTES)return 0;
 if(spans_overlap(p->target_entry.data,p->target_entry.size,output,output_bytes)||spans_overlap(p->mount.data,p->mount.size,output,output_bytes)||
 spans_overlap(p->sections,p->section_count*sizeof(*p->sections),output,output_bytes))return 0;
 for(uint32_t i=0;i<p->section_count;++i)if(!p->sections[i].data||!p->sections[i].size||
 spans_overlap(p->sections[i].data,p->sections[i].size,output,output_bytes))return 0;
 return 1;
}
static int valid_gate(const pm_config_file_job_params_v1 *p,const void *params,size_t params_bytes,
                     const void *output,size_t output_bytes,const pm_ini_bytes *platform,
                     mb_pak_prepare_gate_v1 gate,void *context) {
 if(!gate&&context)return 0;
 if(!context)return 1;
 if(spans_overlap(context,1,params,params_bytes)||spans_overlap(context,1,output,output_bytes)||
 spans_overlap(context,1,p->target_entry.data,p->target_entry.size)||spans_overlap(context,1,p->mount.data,p->mount.size)||
 spans_overlap(context,1,p->sections,p->section_count*sizeof(*p->sections))||
 (platform&&spans_overlap(context,1,platform->data,platform->size)))return 0;
 for(uint32_t i=0;i<p->section_count;++i)
  if(spans_overlap(context,1,p->sections[i].data,p->sections[i].size))return 0;
 return 1;
}
static pm_config_file_job_status run_job(const pm_config_file_job_params_v1 *p,pm_config_file_job_result_v1 *r,
                                       const pm_ini_bytes *platform,pm_config_file_job_result_v2 *automatic,
                                       mb_pak_prepare_gate_v1 gate,void *gate_context) {
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;r->stage=PM_CONFIG_FILE_JOB_PREFLIGHT;
 job_state job={0};job.params=p;job.result=r;job.gate=gate;job.gate_context=gate_context;
 pm_config_file_job_status status=PM_CONFIG_FILE_JOB_FD_REFUSED;
 mb_pak_stream_result_v1 written={0};
 uint8_t merged_sha256[32]={0};
 struct stat original_before,member_before,output_before,original_after,member_after,output_after;
 CHECK_GATE();
 if(fstat(p->original_fd,&original_before)||fstat(p->member_fd,&member_before)||fstat(p->output_fd,&output_before)||
 !S_ISREG(original_before.st_mode)||!S_ISREG(member_before.st_mode)||!S_ISREG(output_before.st_mode)||
 original_before.st_size<0||member_before.st_size<0||output_before.st_size<0)goto cleanup;
 CHECK_GATE();
 if(same_inode(&original_before,&output_before)||same_inode(&member_before,&output_before))STOP(PM_CONFIG_FILE_JOB_OUTPUT_ALIAS);
 if(output_before.st_size!=0||(uint64_t)original_before.st_size!=p->original_bytes||(uint64_t)member_before.st_size!=p->member_bytes)goto cleanup;
 int original_flags=fcntl(p->original_fd,F_GETFL),member_flags=fcntl(p->member_fd,F_GETFL),output_flags=fcntl(p->output_fd,F_GETFL);
 if(original_flags<0||member_flags<0||output_flags<0||(original_flags&O_ACCMODE)!=O_RDONLY||(member_flags&O_ACCMODE)!=O_RDONLY||
 ((output_flags&O_ACCMODE)!=O_WRONLY&&(output_flags&O_ACCMODE)!=O_RDWR)||(output_flags&O_APPEND))goto cleanup;
 CHECK_GATE();
 r->source_bytes=p->member_bytes;
 pak_source_v1 source={sizeof(source),1,p->original_bytes,1,PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE,0,&job,original_read};
 pak_limits_v1 limits;pak_limits_v1_default(&limits);
 ALLOC(uint8_t,reader,PAK_READER_WORKSPACE_BYTES);ALLOC(uint8_t,primary,PAK_READER_MAX_PRIMARY_BYTES);
 pak_primary_capture_result_v1 captured;
 r->stage=PM_CONFIG_FILE_JOB_PRIMARY;
 CHECK_GATE();
 pak_primary_capture_status cap_status=pak_capture_v11_primary(&source,&limits,reader,PAK_READER_WORKSPACE_BYTES,primary,PAK_READER_MAX_PRIMARY_BYTES,&captured);
 if(cap_status!=PAK_PRIMARY_CAPTURE_OK){r->component_status=(uint32_t)cap_status;r->component_detail=captured.reader_status;STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);}
 CHECK_GATE();
 const pak_primary_result_v1 *meta=&captured.primary;
 if(meta->mount_encoding!=PAK_MOUNT_ENCODING_ANSI_BYTES||!meta->mount_bytes||meta->mount_bytes>PAK_READER_MAX_MOUNT_BYTES||
 !meta->declared_file_count||meta->declared_file_count>MB_PAK_STREAM_MAX_FILES)STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);
 if(!automatic&&(meta->mount_bytes!=p->mount.size||memcmp(meta->mount_raw,p->mount.data,p->mount.size)||meta->path_hash_seed!=p->path_hash_seed))STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);
 pm_ini_bytes writer_mount=automatic?(pm_ini_bytes){meta->mount_raw,meta->mount_bytes}:p->mount;
 uint64_t writer_seed=automatic?meta->path_hash_seed:p->path_hash_seed;
 r->stage=PM_CONFIG_FILE_JOB_SECONDARY;
 CHECK_GATE();
 pak_secondary_limits_v1 secondary_limits;pak_secondary_limits_v1_default(&secondary_limits);
 pak_read_budget_v1 budget={sizeof(budget),1,PAK_READER_MAX_TOTAL_READ_BYTES,2048,0};
 ALLOC(uint8_t,secondary_scratch,PAK_SECONDARY_WORKSPACE_BYTES);
 pak_secondary_result_v1 verified;const pak_secondary_index *spans[2]={&meta->path_hash_index,&meta->directory_index};
 for(unsigned i=0;i<2;++i){
  CHECK_GATE();
  pak_status secondary_status=pak_verify_v11_secondary_raw_sha1(&source,spans[i],&secondary_limits,&budget,secondary_scratch,PAK_SECONDARY_WORKSPACE_BYTES,&verified);
  if(secondary_status!=PAK_STATUS_OK||!verified.digest_valid){r->component_status=(uint32_t)secondary_status;STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);}
  CHECK_GATE();
 }
 if(!meta->directory_index.size||meta->directory_index.size>PAK_DIRECTORY_MAX_INDEX_BYTES)STOP(PM_CONFIG_FILE_JOB_RESOURCE_LIMIT);
 r->stage=PM_CONFIG_FILE_JOB_DIRECTORY;
 CHECK_GATE();
 ALLOC(uint8_t,directory,(size_t)meta->directory_index.size);
 if(read_all(&job,p->original_fd,p->original_bytes,meta->directory_index.offset,directory,(size_t)meta->directory_index.size))STOP(PM_CONFIG_FILE_JOB_READ_ERROR);
 uint32_t count=meta->declared_file_count;
 ALLOC(pak_directory_record_v1,dirs,MB_PAK_STREAM_MAX_DIRECTORIES);
 ALLOC(pak_directory_entry_v1,entries,count);ALLOC(uint8_t,names,PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES);
 pak_directory_limits_v1 dirlimits;pak_directory_limits_v1_default(&dirlimits);pak_directory_result_v1 parsed;
 CHECK_GATE();
 pak_directory_status dir_status=pak_directory_read_classic_v11(directory,(size_t)meta->directory_index.size,captured.encoded_span_bytes,&dirlimits,
  dirs,MB_PAK_STREAM_MAX_DIRECTORIES,entries,count,names,PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES,&parsed);
 if(dir_status!=PAK_DIRECTORY_STATUS_OK||!parsed.metadata_complete||parsed.entry_count!=count||parsed.tombstone_count){r->component_status=(uint32_t)dir_status;STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);}
 CHECK_GATE();
 ALLOC(mb_pak_stream_file_v1,files,count);ASSIGN_ALLOC(job.inputs,count,sizeof(*job.inputs));job.input_count=count;
 ALLOC(uint8_t,path_arena,MB_PAK_STREAM_MAX_NAME_BYTES);size_t path_used=0;uint32_t replace_count=0,selected_index=UINT32_MAX;
 pak_entry_limits_v1 el={sizeof(el),1,PAK_ENTRY_DECODER_MAX_BLOB_BYTES,128,0,UINT32_MAX,MB_PAK_STREAM_MAX_PAYLOAD_BYTES,MB_PAK_STREAM_MAX_PAYLOAD_BYTES,MB_PAK_STREAM_MAX_PAYLOAD_BYTES};
 pak_physical_limits_v1 pl={sizeof(pl),1,53,0,UINT32_MAX,0,MB_PAK_STREAM_MAX_PAYLOAD_BYTES,MB_PAK_STREAM_MAX_PAYLOAD_BYTES};
 r->stage=PM_CONFIG_FILE_JOB_ENTRIES;
 CHECK_GATE();
 for(uint32_t d=0;d<parsed.directory_count;++d){
  CHECK_GATE();
  const pak_directory_record_v1 *dir=&dirs[d];const uint8_t *prefix=names+dir->name_offset;
  uint32_t prefix_bytes=dir->name_length;if(prefix_bytes==1&&prefix[0]=='/')prefix_bytes=0;
  for(uint32_t j=0;j<dir->entry_count;++j){
   CHECK_GATE();
   uint32_t i=dir->first_entry+j;const pak_directory_entry_v1 *e=&entries[i];
   size_t n=(size_t)prefix_bytes+e->name_length;
   if(n>MB_PAK_STREAM_MAX_PATH_BYTES||n+1>MB_PAK_STREAM_MAX_NAME_BYTES-path_used||e->location<0)STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);
   uint8_t *path=path_arena+path_used;memcpy(path,prefix,prefix_bytes);memcpy(path+prefix_bytes,names+e->name_offset,e->name_length);path[n]=0;path_used+=n+1;
   pak_entry_result_v1 decoded;pak_entry_status entry_status=pak_entry_decode_v11(primary+captured.encoded_span_offset,captured.encoded_span_bytes,(size_t)e->location,
    p->original_bytes,&captured.method_table,&el,NULL,0,&decoded);
   if(entry_status!=PAK_ENTRY_STATUS_OK||!decoded.record_complete||decoded.method_index!=0||decoded.header_bytes!=53||
    decoded.compressed_size!=decoded.uncompressed_size||decoded.data_offset>meta->primary_index_offset||
    decoded.compressed_size>meta->primary_index_offset-decoded.data_offset){r->component_status=(uint32_t)entry_status;STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);}
   uint8_t physical_bytes[53];uint32_t actual=0;pak_physical_result_v1 physical;
   if(original_read(&job,decoded.archive_offset,physical_bytes,53,&actual)!=PAK_PROVIDER_OK||actual!=53)STOP(PM_CONFIG_FILE_JOB_READ_ERROR);
   pak_physical_status physical_status=pak_physical_header_parse_v11(physical_bytes,53,&captured.method_table,&pl,NULL,0,&physical);
   if(physical_status!=PAK_PHYSICAL_STATUS_OK||!physical.record_complete||physical.method_index!=0||physical.compressed_size!=decoded.compressed_size||
   physical.uncompressed_size!=decoded.uncompressed_size||physical.flags){r->component_status=(uint32_t)physical_status;STOP(PM_CONFIG_FILE_JOB_FORMAT_REFUSED);}
   CHECK_GATE();
   files[i].struct_size=sizeof(files[i]);files[i].abi_version=1;files[i].path=path;files[i].path_bytes=(uint32_t)n;
   files[i].verify_sha1=1;files[i].payload_bytes=decoded.uncompressed_size;memcpy(files[i].expected_sha1,physical.sha1,20);
   job.inputs[i].data_offset=decoded.data_offset;job.inputs[i].bytes=decoded.uncompressed_size;
   if(!automatic&&n==p->target_entry.size&&!memcmp(path,p->target_entry.data,n)){++replace_count;selected_index=i;}
  }
 }
 if(automatic){
  CHECK_GATE();
  mb_unreal_config_target_profile_v1 target_profile={sizeof(target_profile),1,platform->data,(uint32_t)platform->size,0};
  mb_unreal_config_target_result_v1 selected={0};
  mb_unreal_config_target_status_v1 selected_status=mb_unreal_config_target_select_v1(files,count,&target_profile,&selected);
  automatic->selector_status=(uint32_t)selected_status;
  automatic->selector_root_count=selected.root_count;automatic->selector_target_count=selected.target_count;
  if(selected_status!=MB_UNREAL_CONFIG_TARGET_OK||!selected.complete||selected.target_row_index>=count||
   !selected.target_path_bytes||selected.target_path_bytes>MB_PAK_STREAM_MAX_PATH_BYTES)STOP(PM_CONFIG_FILE_JOB_TARGET_NOT_UNIQUE);
  CHECK_GATE();
  selected_index=selected.target_row_index;replace_count=1;
  automatic->resolved_row=selected_index;automatic->resolved_path_bytes=selected.target_path_bytes;
  memcpy(automatic->resolved_path,selected.target_path,selected.target_path_bytes);
  automatic->resolved_mount_bytes=(uint32_t)writer_mount.size;automatic->resolved_path_hash_seed=writer_seed;
  memcpy(automatic->resolved_mount,writer_mount.data,writer_mount.size);
 }
 if(replace_count!=1)STOP(PM_CONFIG_FILE_JOB_TARGET_NOT_UNIQUE);
 size_t base_bytes=(size_t)files[selected_index].payload_bytes;
 if(base_bytes>PM_INI_MAX_INPUT_BYTES)STOP(PM_CONFIG_FILE_JOB_RESOURCE_LIMIT);
 r->base_bytes=base_bytes;r->stage=PM_CONFIG_FILE_JOB_INI;
 CHECK_GATE();
 ALLOC(uint8_t,base_data,base_bytes?base_bytes:1);ALLOC(uint8_t,source_data,p->member_bytes?(size_t)p->member_bytes:1);
 if(read_all(&job,p->original_fd,p->original_bytes,job.inputs[selected_index].data_offset,base_data,base_bytes)||
 read_all(&job,p->member_fd,p->member_bytes,0,source_data,(size_t)p->member_bytes))STOP(PM_CONFIG_FILE_JOB_READ_ERROR);
 uint8_t base_sha1[20];
 CHECK_GATE();
 if(mbedtls_sha1(base_data,base_bytes,base_sha1)||memcmp(base_sha1,files[selected_index].expected_sha1,20))STOP(PM_CONFIG_FILE_JOB_HASH_ERROR);
 CHECK_GATE();
 pm_ini_bytes base_view={base_data,base_bytes},source_view={source_data,(size_t)p->member_bytes};
 ALLOC(pm_ini_workspace,ini_workspace,1);ALLOC(uint8_t,merged,PM_INI_MAX_OUTPUT_BYTES);pm_ini_result ini_result;
 CHECK_GATE();
 pm_ini_status ini_status=pm_ini_merge(&base_view,&source_view,1,p->sections,p->section_count,ini_workspace,merged,PM_INI_MAX_OUTPUT_BYTES,&ini_result);
 if(ini_status!=PM_INI_OK){r->component_status=(uint32_t)ini_status;STOP(PM_CONFIG_FILE_JOB_INI_REFUSED);}
 CHECK_GATE();
 job.merged=merged;job.merged_bytes=ini_result.output_bytes;files[selected_index].payload_bytes=job.merged_bytes;
 r->merged_bytes=ini_result.output_bytes;r->scalar_winners=ini_result.scalar_winners;r->replacements=ini_result.replacements;r->additions=ini_result.additions;
 CHECK_GATE();
 if(mbedtls_sha1(merged,(size_t)job.merged_bytes,files[selected_index].expected_sha1))STOP(PM_CONFIG_FILE_JOB_HASH_ERROR);
 CHECK_GATE();
 if(mbedtls_sha256(merged,(size_t)job.merged_bytes,merged_sha256,0))STOP(PM_CONFIG_FILE_JOB_HASH_ERROR);
 CHECK_GATE();
 job.inputs[selected_index].replacement=1;job.inputs[selected_index].data_offset=0;job.inputs[selected_index].bytes=job.merged_bytes;
 mb_pak_stream_profile_v1 profile={sizeof(profile),1,writer_mount.data,(uint32_t)writer_mount.size,0,writer_seed};
 mb_pak_stream_limits_v1 writerlimits;mb_pak_stream_limits_v1_default(&writerlimits);mb_pak_stream_workspace_v1 w={0};
 w.records_capacity=count;ASSIGN_ALLOC(w.records,count,sizeof(*w.records));w.order_capacity=count;ASSIGN_ALLOC(w.order,count,sizeof(*w.order));
 w.directories_capacity=MB_PAK_STREAM_MAX_DIRECTORIES;ASSIGN_ALLOC(w.directories,w.directories_capacity,sizeof(*w.directories));
 w.index_capacity=MB_PAK_STREAM_MAX_INDEX_BYTES;ASSIGN_ALLOC(w.index_arena,1,w.index_capacity);
 w.transfer_bytes=MB_PAK_STREAM_TRANSFER_BYTES;ASSIGN_ALLOC(w.transfer,1,w.transfer_bytes);
 mb_pak_stream_io_v1 writerio={sizeof(writerio),1,&job,payload_read,output_write,1,1};r->stage=PM_CONFIG_FILE_JOB_WRITER;
 CHECK_GATE();
 mb_pak_stream_status writer_status=mb_pak_stream_write_v11_none_gated_v1(&profile,files,count,&writerlimits,&writerio,&w,&written,
  gate?gate_stopped:NULL,gate?&job:NULL);
 r->component_status=(uint32_t)writer_status;
 if(writer_status!=MB_PAK_STREAM_OK||!written.archive_complete)STOP(PM_CONFIG_FILE_JOB_WRITE_ERROR);
 CHECK_GATE();
 r->source_entries_sha1_checked=count-1;r->stage=PM_CONFIG_FILE_JOB_SNAPSHOT;
 CHECK_GATE();
 if(fstat(p->original_fd,&original_after)||fstat(p->member_fd,&member_after)||!stable(&original_before,&original_after)||!stable(&member_before,&member_after))STOP(PM_CONFIG_FILE_JOB_SOURCE_CHANGED);
 CHECK_GATE();
 r->source_snapshot_stable=1;
 if(fstat(p->output_fd,&output_after)||!same_inode(&output_before,&output_after)||output_after.st_size<0||
 (uint64_t)output_after.st_size!=written.archive_bytes)STOP(PM_CONFIG_FILE_JOB_OUTPUT_SIZE_MISMATCH);
 CHECK_GATE();
 status=PM_CONFIG_FILE_JOB_OK;
cleanup:
 while(job.allocation_count){
  owned_allocation a=job.allocations[--job.allocation_count];
  (void)check_gate(&job);
  p->release(p->allocator_context,a.pointer);job.allocation_live-=a.bytes;++r->release_calls;
  (void)check_gate(&job);
 }
 r->allocation_live_after_cleanup=job.allocation_live;
 /* Preserve child diagnostics while withholding completion throughout caller
  * release callbacks. Clear local digest backing before the final sample. */
 uint32_t writer_complete=written.archive_complete;
 r->writer=written;r->writer.archive_complete=0;memset(&written,0,sizeof(written));
 memcpy(r->merged_sha256,merged_sha256,sizeof(merged_sha256));memset(merged_sha256,0,sizeof(merged_sha256));
 if(gate_stopped(&job)){
  status=PM_CONFIG_FILE_JOB_STOPPED;
  r->writer.archive_bytes=0;r->writer.file_count=0;r->writer.directory_count=0;
  memset(r->merged_sha256,0,sizeof(r->merged_sha256));
  memset(r->writer.primary_sha1,0,sizeof(r->writer.primary_sha1));
  memset(r->writer.path_index_sha1,0,sizeof(r->writer.path_index_sha1));
  memset(r->writer.directory_sha1,0,sizeof(r->writer.directory_sha1));
 }else{
  r->writer.archive_complete=writer_complete;
  if(status==PM_CONFIG_FILE_JOB_OK){r->archive_complete=1;r->stage=PM_CONFIG_FILE_JOB_COMPLETE;}
 }
 r->status=status;
 return status;
}

pm_config_file_job_status pm_config_file_job_v1(const pm_config_file_job_params_v1 *p,pm_config_file_job_result_v1 *r) {
 return pm_config_file_job_gated_v1(p,r,NULL,NULL);
}
pm_config_file_job_status pm_config_file_job_gated_v1(const pm_config_file_job_params_v1 *p,pm_config_file_job_result_v1 *r,
                                                   mb_pak_prepare_gate_v1 gate,void *gate_context) {
 if((!gate&&gate_context)||!valid_params(p,r,sizeof(*r),0)||
 !valid_gate(p,p,sizeof(*p),r,sizeof(*r),NULL,gate,gate_context))return PM_CONFIG_FILE_JOB_BAD_ARGUMENT;
 return run_job(p,r,NULL,NULL,gate,gate_context);
}
static int valid_platform(pm_ini_bytes span) {
 for(size_t i=0;i<span.size;++i){unsigned c=span.data[i];
  if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;
 }
 return 1;
}
pm_config_file_job_status pm_config_file_job_v2(const pm_config_file_job_params_v2 *p,pm_config_file_job_result_v2 *r) {
 return pm_config_file_job_gated_v2(p,r,NULL,NULL);
}
pm_config_file_job_status pm_config_file_job_gated_v2(const pm_config_file_job_params_v2 *p,pm_config_file_job_result_v2 *r,
                                                   mb_pak_prepare_gate_v1 gate,void *gate_context) {
 if((!gate&&gate_context)||!p||!r||spans_overlap(p,sizeof(*p),r,sizeof(*r))||p->struct_size!=sizeof(*p)||p->abi_version!=2||
 !p->platform.data||!p->platform.size||p->platform.size>MB_UNREAL_CONFIG_TARGET_SELECTOR_MAX_PLATFORM_BYTES||spans_overlap(p->platform.data,p->platform.size,r,sizeof(*r))||
 !valid_params(&p->base,r,sizeof(*r),1)||!valid_platform(p->platform)||
 !valid_gate(&p->base,p,sizeof(*p),r,sizeof(*r),&p->platform,gate,gate_context))return PM_CONFIG_FILE_JOB_BAD_ARGUMENT;
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=2;r->resolved_row=UINT32_MAX;
 return run_job(&p->base,&r->preparation,&p->platform,r,gate,gate_context);
}
