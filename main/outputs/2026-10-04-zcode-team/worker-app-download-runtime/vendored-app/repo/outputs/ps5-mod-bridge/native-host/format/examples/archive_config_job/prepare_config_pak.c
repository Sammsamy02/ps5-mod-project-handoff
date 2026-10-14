#define _POSIX_C_SOURCE 200809L
#include "pak_stream_writer.h"
#include "ini_scalar_merge.h"
#include "prepare_config_pak.h"
#include "pak_primary_capture.h"
#include "pak_secondary_integrity.h"
#include "pak_directory_parser.h"
#include "pak_physical_header.h"
#include <mbedtls/sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

/* Ordinary host-file control only. Shared writer remains provider-independent. */
typedef struct input_file { uint64_t data_offset,bytes;uint32_t replacement; } input_file;
typedef struct file_io {
 FILE *original,*replacement,*output;
 input_file *inputs;uint32_t input_count;
 uint64_t original_bytes,replacement_bytes;
 const uint8_t *merged_payload;uint64_t merged_bytes;
} file_io;
static int exact_file_read(FILE *file,uint64_t offset,uint8_t *dst,uint32_t n,uint32_t *actual) {
 *actual=0;if(offset>INT64_MAX||fseeko(file,(off_t)offset,SEEK_SET))return 1;
 *actual=(uint32_t)fread(dst,1,n,file);return ferror(file)?1:0;
}
static pak_provider_status original_read(void *ctx,uint64_t offset,uint8_t *dst,uint32_t n,uint32_t *actual) {
 file_io *io=ctx;
 if(offset>io->original_bytes||n>io->original_bytes-offset)return PAK_PROVIDER_CONTRACT_ERROR;
 return exact_file_read(io->original,offset,dst,n,actual)?PAK_PROVIDER_IO_ERROR:PAK_PROVIDER_OK;
}
static int payload_read(void *ctx,uint32_t index,uint64_t offset,uint8_t *dst,uint32_t n,uint32_t *actual) {
 file_io *io=ctx;*actual=0;if(index>=io->input_count)return 1;
 input_file *f=&io->inputs[index];if(offset>f->bytes||n>f->bytes-offset)return 1;
 if(f->replacement){
  if(offset>io->merged_bytes||n>io->merged_bytes-offset)return 1;
  memcpy(dst,io->merged_payload+(size_t)offset,n);*actual=n;return 0;
 }
 return exact_file_read(io->original,f->data_offset+offset,dst,n,actual);
}
static int output_write(void *ctx,uint64_t offset,const uint8_t *src,uint32_t n,uint32_t *actual) {
 file_io *io=ctx;*actual=0;
 if(offset>MB_PAK_STREAM_MAX_ARCHIVE_BYTES||n>MB_PAK_STREAM_MAX_ARCHIVE_BYTES-offset||
 fseeko(io->output,(off_t)offset,SEEK_SET))return 1;
 *actual=(uint32_t)fwrite(src,1,n,io->output);return ferror(io->output)?1:0;
}
static int same_file_identity(const struct stat *a,const struct stat *b) {
 if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size)return 0;
#ifdef __APPLE__
 /* Darwin's strict POSIX stat exposes seconds/nanoseconds as separate fields. */
 return a->st_mtime==b->st_mtime&&a->st_mtimensec==b->st_mtimensec&&
 a->st_ctime==b->st_ctime&&a->st_ctimensec==b->st_ctimensec;
#else
 return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
 a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int read_all_file(FILE *file,uint64_t offset,uint8_t *output,size_t bytes) {
 size_t pos=0;
 while(pos<bytes){uint32_t n=(uint32_t)(bytes-pos>MB_PAK_STREAM_TRANSFER_BYTES?MB_PAK_STREAM_TRANSFER_BYTES:bytes-pos),actual=0;
  if(exact_file_read(file,offset+pos,output+pos,n,&actual)||actual!=n)return 1;pos+=n;
 }
 return 0;
}
static void fail(const char *text) { fprintf(stderr,"%s\n",text);exit(1); }
static void *alloc(size_t n,size_t s) { void *p=calloc(n,s);if(!p)fail("allocation failed");return p; }
int pm_prepare_config_pak_host(int argc,char **argv,FILE *selected_member,pm_prepare_output_owner *output_owner) {
 if(!output_owner)return 1;
 if(argc<8||argc>7+(int)PM_INI_MAX_SECTIONS)fail("usage: host_config_rebuild original.pak target-entry source.ini mount seed fresh-output.pak section [section...]");
 char *end=NULL;uint64_t seed=strtoull(argv[5],&end,10);if(!end||*end)fail("bad profile seed");
 file_io io={0};io.original=fopen(argv[1],"rb");io.replacement=selected_member;
 if(!io.original||!io.replacement)fail("source open failed");
 struct stat original_before,replacement_before,original_after,replacement_after;
 if(fstat(fileno(io.original),&original_before)||fstat(fileno(io.replacement),&replacement_before)||
 !S_ISREG(original_before.st_mode)||!S_ISREG(replacement_before.st_mode)||original_before.st_size<=0||
 replacement_before.st_size<0||(uint64_t)original_before.st_size>MB_PAK_STREAM_MAX_ARCHIVE_BYTES||
 (uint64_t)replacement_before.st_size>PM_INI_MAX_INPUT_BYTES)fail("source size/type refused");
 io.original_bytes=(uint64_t)original_before.st_size;io.replacement_bytes=(uint64_t)replacement_before.st_size;
 pak_source_v1 source={sizeof(source),1,io.original_bytes,1,PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE,0,&io,original_read};
 pak_limits_v1 limits;pak_limits_v1_default(&limits);
 uint8_t *reader=alloc(1,PAK_READER_WORKSPACE_BYTES),*primary=alloc(1,PAK_READER_MAX_PRIMARY_BYTES);
 pak_primary_capture_result_v1 captured;
 if(pak_capture_v11_primary(&source,&limits,reader,PAK_READER_WORKSPACE_BYTES,primary,PAK_READER_MAX_PRIMARY_BYTES,&captured)!=PAK_PRIMARY_CAPTURE_OK){
 fprintf(stderr,"primary failed status=%u reader=%u reason=%u\n",captured.status,captured.reader_status,captured.reader_reason);return 1;}
 pak_primary_result_v1 *p=&captured.primary;
 if(p->mount_encoding!=PAK_MOUNT_ENCODING_ANSI_BYTES||p->mount_bytes!=strlen(argv[4])||
 memcmp(p->mount_raw,argv[4],p->mount_bytes)||p->path_hash_seed!=seed||p->declared_file_count>MB_PAK_STREAM_MAX_FILES)
 fail("source/profile metadata mismatch");
 pak_secondary_limits_v1 secondary_limits;pak_secondary_limits_v1_default(&secondary_limits);
 pak_read_budget_v1 budget={sizeof(budget),1,PAK_READER_MAX_TOTAL_READ_BYTES,2048,0};
 uint8_t *secondary_scratch=alloc(1,PAK_SECONDARY_WORKSPACE_BYTES);
 pak_secondary_result_v1 verified;
 const pak_secondary_index *spans[2]={&p->path_hash_index,&p->directory_index};
 for(unsigned i=0;i<2;++i)if(pak_verify_v11_secondary_raw_sha1(&source,spans[i],&secondary_limits,&budget,
 secondary_scratch,PAK_SECONDARY_WORKSPACE_BYTES,&verified)!=PAK_STATUS_OK||!verified.digest_valid)
 fail("source secondary digest rejected");
 if(p->directory_index.size>PAK_DIRECTORY_MAX_INDEX_BYTES)fail("directory over cap");
 uint8_t *directory=alloc(1,(size_t)p->directory_index.size);uint64_t at=0;
 while(at<p->directory_index.size){uint32_t n=(uint32_t)(p->directory_index.size-at>PAK_READER_MAX_READ_BYTES?
 PAK_READER_MAX_READ_BYTES:p->directory_index.size-at),actual=0;
 if(original_read(&io,p->directory_index.offset+at,directory+(size_t)at,n,&actual)!=PAK_PROVIDER_OK||actual!=n)
 fail("directory short read");at+=n;}
 uint32_t count=p->declared_file_count;
 pak_directory_record_v1 *dirs=alloc(MB_PAK_STREAM_MAX_DIRECTORIES,sizeof(*dirs));
 pak_directory_entry_v1 *entries=alloc(count,sizeof(*entries));
 uint8_t *names=alloc(1,PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES);
 pak_directory_limits_v1 dirlimits;pak_directory_limits_v1_default(&dirlimits);pak_directory_result_v1 parsed;
 if(pak_directory_read_classic_v11(directory,(size_t)p->directory_index.size,captured.encoded_span_bytes,&dirlimits,
 dirs,MB_PAK_STREAM_MAX_DIRECTORIES,entries,count,names,PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES,&parsed)!=PAK_DIRECTORY_STATUS_OK||
 !parsed.metadata_complete||parsed.entry_count!=count||parsed.tombstone_count)fail("source directory refused");
 mb_pak_stream_file_v1 *files=alloc(count,sizeof(*files));io.inputs=alloc(count,sizeof(*io.inputs));io.input_count=count;
 uint8_t *path_arena=alloc(1,MB_PAK_STREAM_MAX_NAME_BYTES);size_t path_used=0;uint32_t replace_count=0,selected_index=UINT32_MAX;

 pak_entry_limits_v1 el={sizeof(el),1,PAK_ENTRY_DECODER_MAX_BLOB_BYTES,128,0,UINT32_MAX,
 MB_PAK_STREAM_MAX_PAYLOAD_BYTES,MB_PAK_STREAM_MAX_PAYLOAD_BYTES,MB_PAK_STREAM_MAX_PAYLOAD_BYTES};
 pak_physical_limits_v1 pl={sizeof(pl),1,53,0,UINT32_MAX,0,MB_PAK_STREAM_MAX_PAYLOAD_BYTES,MB_PAK_STREAM_MAX_PAYLOAD_BYTES};
 for(uint32_t d=0;d<parsed.directory_count;++d){
 pak_directory_record_v1 *dir=&dirs[d];const uint8_t *prefix=names+dir->name_offset;
 uint32_t prefix_bytes=dir->name_length;if(prefix_bytes==1&&prefix[0]=='/')prefix_bytes=0;
 for(uint32_t j=0;j<dir->entry_count;++j){
 uint32_t i=dir->first_entry+j;pak_directory_entry_v1 *e=&entries[i];
 size_t n=(size_t)prefix_bytes+e->name_length;
 if(n>MB_PAK_STREAM_MAX_PATH_BYTES||n+1>MB_PAK_STREAM_MAX_NAME_BYTES-path_used||e->location<0)fail("source path/location refused");
 uint8_t *path=path_arena+path_used;memcpy(path,prefix,prefix_bytes);memcpy(path+prefix_bytes,names+e->name_offset,e->name_length);path[n]=0;path_used+=n+1;
 pak_entry_result_v1 decoded;
 if(pak_entry_decode_v11(primary+captured.encoded_span_offset,captured.encoded_span_bytes,(size_t)e->location,
 io.original_bytes,&captured.method_table,&el,NULL,0,&decoded)!=PAK_ENTRY_STATUS_OK||!decoded.record_complete||
 decoded.method_index!=0||decoded.header_bytes!=53||decoded.compressed_size!=decoded.uncompressed_size||
 decoded.data_offset>p->primary_index_offset||decoded.compressed_size>p->primary_index_offset-decoded.data_offset)
 fail("source compact entry refused");
 uint8_t physical_bytes[53];uint32_t actual=0;pak_physical_result_v1 physical;
 if(original_read(&io,decoded.archive_offset,physical_bytes,53,&actual)!=PAK_PROVIDER_OK||actual!=53||
 pak_physical_header_parse_v11(physical_bytes,53,&captured.method_table,&pl,NULL,0,&physical)!=PAK_PHYSICAL_STATUS_OK||
 !physical.record_complete||physical.method_index!=0||physical.compressed_size!=decoded.compressed_size||
 physical.uncompressed_size!=decoded.uncompressed_size||physical.flags)fail("source repeated header refused");
 files[i].struct_size=sizeof(files[i]);files[i].abi_version=1;files[i].path=path;files[i].path_bytes=(uint32_t)n;
 files[i].verify_sha1=1;files[i].payload_bytes=decoded.uncompressed_size;memcpy(files[i].expected_sha1,physical.sha1,20);
 io.inputs[i].data_offset=decoded.data_offset;io.inputs[i].bytes=decoded.uncompressed_size;
 if(!strcmp((const char *)path,argv[2])){
 ++replace_count;selected_index=i;}
 }}
 if(replace_count!=1)fail("profile replacement must match exactly one original entry");
 /* Base is read from the admitted original row, never from a prepared file. */
 size_t base_bytes=(size_t)files[selected_index].payload_bytes;
 if(base_bytes>PM_INI_MAX_INPUT_BYTES)fail("base INI over scalar-merge cap");
 uint8_t *base_data=alloc(1,base_bytes?base_bytes:1),*source_data=alloc(1,io.replacement_bytes?io.replacement_bytes:1);
 if(read_all_file(io.original,io.inputs[selected_index].data_offset,base_data,base_bytes)||
 read_all_file(io.replacement,0,source_data,(size_t)io.replacement_bytes))fail("base/source INI short read");
 uint8_t base_sha1[20];
 if(mbedtls_sha1(base_data,base_bytes,base_sha1)||memcmp(base_sha1,files[selected_index].expected_sha1,20))
 fail("base INI original SHA1 mismatch");
 pm_ini_bytes base_view={base_data,base_bytes},source_view={source_data,(size_t)io.replacement_bytes};
 size_t section_count=(size_t)(argc-7);pm_ini_bytes *sections=alloc(section_count,sizeof(*sections));
 for(size_t i=0;i<section_count;++i){sections[i].data=(const uint8_t *)argv[7+i];sections[i].size=strlen(argv[7+i]);}
 pm_ini_workspace *ini_workspace=alloc(1,sizeof(*ini_workspace));uint8_t *merged=alloc(1,PM_INI_MAX_OUTPUT_BYTES);pm_ini_result ini_result;
 pm_ini_status ini_status=pm_ini_merge(&base_view,&source_view,1,sections,section_count,ini_workspace,merged,PM_INI_MAX_OUTPUT_BYTES,&ini_result);
 if(ini_status!=PM_INI_OK){fprintf(stderr,"INI merge rejected: %s\n",pm_ini_status_name(ini_status));return 1;}
 io.merged_payload=merged;io.merged_bytes=ini_result.output_bytes;
 files[selected_index].payload_bytes=io.merged_bytes;
 if(mbedtls_sha1(merged,(size_t)io.merged_bytes,files[selected_index].expected_sha1))fail("merged INI SHA1 failed");
 io.inputs[selected_index].replacement=1;io.inputs[selected_index].data_offset=0;io.inputs[selected_index].bytes=io.merged_bytes;
 printf("{\"phase\":\"ini_merge\",\"status\":\"PASS\",\"base_bytes\":%zu,\"source_bytes\":%" PRIu64
 ",\"merged_bytes\":%zu,\"scalar_winners\":%u,\"replacements\":%u,\"additions\":%u,\"workspace_bytes\":%zu,\"base_sha1_verified\":true,\"output_fed_from_memory\":true}\n",
 base_bytes,io.replacement_bytes,ini_result.output_bytes,ini_result.scalar_winners,ini_result.replacements,ini_result.additions,sizeof(*ini_workspace));

 mb_pak_stream_profile_v1 profile={sizeof(profile),1,(const uint8_t *)argv[4],(uint32_t)strlen(argv[4]),0,seed};
 mb_pak_stream_limits_v1 writerlimits;mb_pak_stream_limits_v1_default(&writerlimits);
 mb_pak_stream_workspace_v1 w={0};w.records_capacity=count;w.records=alloc(count,sizeof(*w.records));
 w.order_capacity=count;w.order=alloc(count,sizeof(*w.order));w.directories_capacity=MB_PAK_STREAM_MAX_DIRECTORIES;
 w.directories=alloc(w.directories_capacity,sizeof(*w.directories));w.index_capacity=MB_PAK_STREAM_MAX_INDEX_BYTES;w.index_arena=alloc(1,w.index_capacity);
 w.transfer_bytes=MB_PAK_STREAM_TRANSFER_BYTES;w.transfer=alloc(1,w.transfer_bytes);
 mb_pak_stream_io_v1 writerio={sizeof(writerio),1,&io,payload_read,output_write,1,1};
 /* O_EXCL with O_CREAT refuses a preexisting symlink as well as a file. */
 int fd=open(argv[6],O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);
 if(fd<0){fprintf(stderr,"fresh output creation failed\n");return 1;}
 struct stat output_stat;
 if(fstat(fd,&output_stat)||!S_ISREG(output_stat.st_mode)){close(fd);return 1;}
 output_owner->device=(uint64_t)output_stat.st_dev;output_owner->inode=(uint64_t)output_stat.st_ino;output_owner->created=1;
 io.output=fdopen(fd,"w+b");if(!io.output){close(fd);fprintf(stderr,"output stream failed\n");return 1;}
 mb_pak_stream_result_v1 result;mb_pak_stream_status status=mb_pak_stream_write_v11_none(&profile,files,count,&writerlimits,&writerio,&w,&result);
 int stable=!fstat(fileno(io.original),&original_after)&&!fstat(fileno(io.replacement),&replacement_after)&&
 same_file_identity(&original_before,&original_after)&&same_file_identity(&replacement_before,&replacement_after);
 int flush_error=fflush(io.output)||fsync(fileno(io.output));int close_error=fclose(io.output);io.output=NULL;
 if(status!=MB_PAK_STREAM_OK||!result.archive_complete||!stable||flush_error||close_error){
 fprintf(stderr,"writer failed status=%u complete=%u stable=%d flush=%d close=%d\n",status,result.archive_complete,stable,flush_error,close_error);
 return 1;}
 printf("{\"status\":\"PASS\",\"archive_complete\":1,\"source_snapshot_stable\":true,\"file_count\":%u,\"directory_count\":%u,\"archive_bytes\":%" PRIu64
 ",\"index_bytes\":%" PRIu64 ",\"transfer_bytes\":%zu,\"work_used\":%" PRIu64 ",\"requested_read_bytes\":%" PRIu64
 ",\"requested_write_bytes\":%" PRIu64 ",\"read_calls\":%u,\"write_calls\":%u,\"source_entries_sha1_checked\":%u,\"replaced_entries\":1}\n",
 result.file_count,result.directory_count,result.archive_bytes,result.index_bytes,w.transfer_bytes,result.work_used,
 result.requested_read_bytes,result.requested_write_bytes,result.read_calls,result.write_calls,count-1);
 fclose(io.original);return 0;
}
