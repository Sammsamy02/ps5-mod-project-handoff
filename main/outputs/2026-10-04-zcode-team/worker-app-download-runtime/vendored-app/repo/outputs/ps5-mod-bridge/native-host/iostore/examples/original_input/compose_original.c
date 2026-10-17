#define _POSIX_C_SOURCE 200809L
#include "none_range_reader.h"
#include "container_header_merge.h"
#include "none_packer.h"
#include "directory_merge.h"
#include "toc_overlay.h"
#include "blake3.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ALLOCATION_CAP (UINT64_C(512)<<20)
#define SLOTS 48u
typedef struct allocation {void *pointer;size_t bytes;} allocation;
typedef struct caller {
 allocation allocations[SLOTS];size_t count;uint64_t live,peak;
 int inputs[4],directory_fd,sidecar_fd;struct stat snapshots[4];
 uint64_t source_reads,source_bytes,write_calls;
 mb_none_range_plan_v1 *plans;uint32_t *source_rows;
 const uint8_t *merged_header;size_t merged_header_bytes;
 const mb_iostore_none_chunk_v1 *chunks;uint32_t chunk_count;
} caller;
static void *alloc(caller *c,size_t count,size_t width) {
 if(!count||width>SIZE_MAX/count||c->count==SLOTS)return NULL;
 size_t n=count*width;if(n>ALLOCATION_CAP-c->live)return NULL;
 void *p=malloc(n);if(!p)return NULL;
 c->allocations[c->count++]=(allocation){p,n};c->live+=n;if(c->live>c->peak)c->peak=c->live;
 return p;
}
static void cleanup(caller *c) {
 for(size_t i=c->count;i>0;--i){free(c->allocations[i-1].pointer);c->live-=c->allocations[i-1].bytes;}
 for(unsigned i=0;i<4;++i)if(c->inputs[i]>=0)(void)close(c->inputs[i]);
 if(c->sidecar_fd>=0)(void)close(c->sidecar_fd);
 if(c->directory_fd>=0)(void)close(c->directory_fd);
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
static int exact_read(int fd,uint64_t offset,uint8_t *out,size_t bytes) {
 if(offset>INT64_MAX||bytes>(uint64_t)INT64_MAX-offset)return 0;
 size_t done=0;uint32_t attempts=0;
 while(done<bytes){if(++attempts>65536)return 0;size_t wanted=bytes-done;if(wanted>65536)wanted=65536;
  ssize_t n=pread(fd,out+done,wanted,(off_t)(offset+done));
  if(n<0&&errno==EINTR)continue;if(n<=0||(size_t)n>wanted)return 0;done+=(size_t)n;}
 return 1;
}
static int exact_write(int fd,uint64_t offset,const uint8_t *data,size_t bytes) {
 if(offset>INT64_MAX||bytes>(uint64_t)INT64_MAX-offset)return 0;
 size_t done=0;uint32_t attempts=0;
 while(done<bytes){if(++attempts>65536)return 0;size_t wanted=bytes-done;if(wanted>65536)wanted=65536;
  ssize_t n=pwrite(fd,data+done,wanted,(off_t)(offset+done));
  if(n<0&&errno==EINTR)continue;if(n<=0||(size_t)n>wanted)return 0;done+=(size_t)n;}
 return 1;
}
static int open_input(caller *c,unsigned index,const char *path,uint64_t cap,uint8_t **loaded,size_t *bytes) {
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;c->inputs[index]=fd;
 struct stat *st=c->snapshots+index;
 if(fstat(fd,st)||!S_ISREG(st->st_mode)||st->st_size<=0||(uint64_t)st->st_size>cap||
  (uint64_t)st->st_size>SIZE_MAX)return 0;
 if(loaded){*bytes=(size_t)st->st_size;*loaded=alloc(c,*bytes,1);if(!*loaded||!exact_read(fd,0,*loaded,*bytes))return 0;}
 return 1;
}
static int stable_inputs(caller *c) {
 for(unsigned i=0;i<4;++i){struct stat now;if(fstat(c->inputs[i],&now)||!same_stat(c->snapshots+i,&now))return 0;}
 return 1;
}
static int read_source(void *opaque,uint32_t partition,uint64_t offset,uint8_t *out,size_t requested,size_t *actual) {
 caller *c=opaque;*actual=0;
 if(partition||offset>(uint64_t)c->snapshots[1].st_size||requested>(uint64_t)c->snapshots[1].st_size-offset||
  !exact_read(c->inputs[1],offset,out,requested))return 1;
 ++c->source_reads;c->source_bytes+=requested;*actual=requested;return 0;
}
static int read_chunk(void *opaque,uint32_t row,uint64_t offset,uint8_t *out,uint32_t requested,uint32_t *actual) {
 caller *c=opaque;*actual=0;if(row>=c->chunk_count)return 1;
 if(offset>c->chunks[row].bytes||requested>c->chunks[row].bytes-offset)return 1;
 if(c->source_rows[row]==UINT32_MAX){
  if(offset>c->merged_header_bytes||requested>c->merged_header_bytes-offset)return 1;
  memcpy(out,c->merged_header+(size_t)offset,requested);*actual=requested;return 0;
 }
 mb_none_range_result_v1 result;
 if(mb_iostore_none_range_v1(c->plans+c->source_rows[row],offset,requested,out,requested,&result)!=MB_NONE_RANGE_OK||
  !result.range_complete||result.read_bytes!=requested)return 1;
 *actual=requested;return 0;
}
static int write_sidecar(void *opaque,uint64_t offset,const uint8_t *data,uint32_t requested,uint32_t *actual) {
 caller *c=opaque;*actual=0;if(!exact_write(c->sidecar_fd,offset,data,requested))return 1;
 ++c->write_calls;*actual=requested;return 0;
}
static int readback(int fd,const uint8_t *expected,size_t bytes) {
 uint8_t observed[65536];struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0||(uint64_t)st.st_size!=bytes)return 0;
 for(size_t at=0;at<bytes;){size_t n=bytes-at;if(n>sizeof(observed))n=sizeof(observed);
  if(!exact_read(fd,at,observed,n)||memcmp(expected+at,observed,n))return 0;at+=n;}
 return 1;
}
static int write_new(caller *c,const char *name,const uint8_t *data,size_t bytes) {
 int fd=openat(c->directory_fd,name,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd<0)return 0;int okay=exact_write(fd,0,data,bytes)&&!fsync(fd)&&readback(fd,data,bytes);
 if(close(fd))okay=0;return okay;
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
static int parse_length(const char *p,uint64_t *out) {
 if(!p||!*p||*p=='-')return 0;char *end=NULL;errno=0;unsigned long long n=strtoull(p,&end,10);
 if(errno||*end||!n)return 0;*out=n;return 1;
}
#define REQUIRE(condition,message) do{if(!(condition)){fprintf(stderr,"%s\n",message);goto done;}}while(0)
int main(int argc,char **argv) {
 caller c={.inputs={-1,-1,-1,-1},.directory_fd=-1,.sidecar_fd=-1};int exit_code=1;
 uint64_t base_ucas_bytes;uint8_t *mod_toc=NULL,*base_toc=NULL,*base_header=NULL;size_t mod_bytes=0,base_bytes=0,base_header_bytes=0;
 struct timespec start,end;clock_gettime(CLOCK_MONOTONIC,&start);
 if(argc!=7||!parse_length(argv[5],&base_ucas_bytes)){
  fprintf(stderr,"usage: compose_original <original-mod.utoc> <original-mod.ucas> <original-base.utoc> <captured-base-header.bin> <observed-base-ucas-bytes> <fresh-output-dir>\n");return 2;}
 REQUIRE(open_input(&c,0,argv[1],MB_IOSTORE_MAX_TOC_BYTES,&mod_toc,&mod_bytes)&&
  open_input(&c,1,argv[2],MB_IOSTORE_NONE_MAX_PAYLOAD_BYTES,NULL,NULL)&&
  open_input(&c,2,argv[3],MB_IOSTORE_MAX_TOC_BYTES,&base_toc,&base_bytes)&&
  open_input(&c,3,argv[4],CCH_HEADER_BYTES_CAP,&base_header,&base_header_bytes),"original input open/read failed");
 mb_iostore_header_result_v1 mod_h,base_h;
 REQUIRE(parse_toc(mod_toc,mod_bytes,&mod_h)&&parse_toc(base_toc,base_bytes,&base_h),"original TOC header geometry refused");
 REQUIRE(mod_h.chunk_count&&mod_h.chunk_count<=MB_IOSTORE_NONE_MAX_CHUNKS&&mod_h.partition_count==1&&
  mod_h.directory_index.bytes<=MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,"incoming source exceeds single-bundle caps");
 uint8_t base_header_id[12]={0};put_le(base_header_id,base_h.container_id,8);base_header_id[11]=6;
 uint32_t base_header_row=UINT32_MAX,foreign_header_row=UINT32_MAX,base_matches=0,foreign_matches=0;
 for(uint32_t i=0;i<base_h.chunk_count;++i)if(!memcmp(base_toc+(size_t)base_h.chunk_ids.offset+(size_t)i*12,base_header_id,12)){
  base_header_row=i;++base_matches;}
 for(uint32_t i=0;i<mod_h.chunk_count;++i)if(mod_toc[(size_t)mod_h.chunk_ids.offset+(size_t)i*12+11]==6){foreign_header_row=i;++foreign_matches;}
 REQUIRE(base_matches==1&&foreign_matches==1,"unique source-derived ContainerHeaders required");
 REQUIRE(be40(base_toc+(size_t)base_h.chunk_offset_lengths.offset+(size_t)base_header_row*10+5)==base_header_bytes&&
  digest_equal(base_header,base_header_bytes,base_toc+(size_t)base_h.entry_metadata.offset+(size_t)base_header_row*24),"captured base header does not bind to original TOC length/digest");
 c.plans=alloc(&c,mod_h.chunk_count,sizeof(*c.plans));c.source_rows=alloc(&c,mod_h.chunk_count,sizeof(*c.source_rows));
 mb_iostore_none_chunk_v1 *chunks=alloc(&c,mod_h.chunk_count,sizeof(*chunks));uint8_t *small_ids=alloc(&c,mod_h.chunk_count,12);
 REQUIRE(c.plans&&c.source_rows&&chunks&&small_ids,"planned source descriptor allocation refused");
 uint64_t source_length=(uint64_t)c.snapshots[1].st_size;
 mb_none_range_source_v1 source={sizeof(source),1,{mod_toc,mod_bytes,0},&source_length,1,0,read_source,&c};
 for(uint32_t i=0;i<mod_h.chunk_count;++i){mb_none_range_result_v1 result;
  REQUIRE(mb_iostore_none_plan_v1(&source,i,c.plans+i,&result)==MB_NONE_RANGE_OK&&result.full_chunk_coverage_scanned,
   "incoming chunk planning/coverage refused");}
 size_t foreign_bytes=(size_t)c.plans[foreign_header_row].logical_bytes;
 REQUIRE(foreign_bytes<=CCH_HEADER_BYTES_CAP,"incoming ContainerHeader exceeds merger cap");
 uint8_t *foreign_header=alloc(&c,foreign_bytes,1);REQUIRE(foreign_header,"incoming header allocation refused");
 for(size_t at=0;at<foreign_bytes;){uint32_t n=(uint32_t)(foreign_bytes-at>65536?65536:foreign_bytes-at);mb_none_range_result_v1 result;
  REQUIRE(mb_iostore_none_range_v1(c.plans+foreign_header_row,at,n,foreign_header+at,n,&result)==MB_NONE_RANGE_OK&&result.range_complete,
   "incoming header extraction failed");at+=n;}
 REQUIRE(digest_equal(foreign_header,foreign_bytes,mod_toc+(size_t)mod_h.entry_metadata.offset+(size_t)foreign_header_row*24),
  "incoming ContainerHeader digest mismatch");
 struct cch_workspace header_ws={0};header_ws.package_ref_capacity=CCH_MAX_REFS;header_ws.range_capacity=CCH_MAX_RANGES;
 header_ws.package_refs=alloc(&c,header_ws.package_ref_capacity,sizeof(*header_ws.package_refs));
 header_ws.ranges=alloc(&c,header_ws.range_capacity,sizeof(*header_ws.ranges));REQUIRE(header_ws.package_refs&&header_ws.ranges,"header merger workspace refused");
 struct cch_result header_result;
 REQUIRE(cch_merge_v5(base_header,base_header_bytes,foreign_header,foreign_bytes,&header_ws,NULL,0,&header_result)==CCH_OUTPUT_TOO_SMALL&&
  header_result.required_bytes&&header_result.required_bytes<=CCH_HEADER_BYTES_CAP,"header merge measurement refused");
 size_t merged_header_bytes=header_result.required_bytes;uint8_t *merged_header=alloc(&c,merged_header_bytes,1);REQUIRE(merged_header,"merged header allocation refused");
 REQUIRE(cch_merge_v5(base_header,base_header_bytes,foreign_header,foreign_bytes,&header_ws,merged_header,merged_header_bytes,&header_result)==CCH_OK&&
  header_result.container_id_preserved&&header_result.base_container_id==base_h.container_id&&header_result.mod_container_id==mod_h.container_id,
  "source-bound header merge refused");
 c.merged_header=merged_header;c.merged_header_bytes=merged_header_bytes;
 uint32_t emitted=0;
 for(uint32_t i=0;i<mod_h.chunk_count;++i)if(i!=foreign_header_row){
  memcpy(chunks[emitted].raw_id,c.plans[i].chunk_id,12);chunks[emitted].bytes=c.plans[i].logical_bytes;c.source_rows[emitted]=i;
  memcpy(small_ids+(size_t)emitted*12,chunks[emitted].raw_id,12);++emitted;}
 memcpy(chunks[emitted].raw_id,base_header_id,12);chunks[emitted].bytes=merged_header_bytes;c.source_rows[emitted]=UINT32_MAX;
 memcpy(small_ids+(size_t)emitted*12,base_header_id,12);++emitted;c.chunk_count=emitted;c.chunks=chunks;
 /* Remap the incoming index to emitted rows using the unchanged source twice.
  * Exact self-collisions collapse without discarding aliases/empty directories.
  * Any path naming the foreign header refuses, as that ID is absent. */
 uint8_t *directory_storage=alloc(&c,MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES,1),*directory_paths=alloc(&c,MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES,1);
 uint8_t *small_directory=alloc(&c,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,1),*merged_directory=alloc(&c,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,1);
 REQUIRE(directory_storage&&directory_paths&&small_directory&&merged_directory,"bounded directory storage refused");
 mb_directory_merge_workspace_v1 directory_ws={sizeof(directory_ws),1,directory_storage,MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES,
  directory_paths,MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES};
 mb_directory_merge_bytes_v1 incoming_directory={mod_toc+(size_t)mod_h.directory_index.offset,(size_t)mod_h.directory_index.bytes};
 mb_directory_merge_ids_v1 incoming_ids={mod_toc+(size_t)mod_h.chunk_ids.offset,mod_h.chunk_count};
 mb_directory_merge_params_v1 directory_params={sizeof(directory_params),1,incoming_directory,incoming_directory,incoming_ids,incoming_ids,{small_ids,emitted}};
 mb_directory_merge_result_v1 small_directory_result;
 REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,small_directory,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&small_directory_result)==MB_DIRECTORY_MERGE_OK&&
  small_directory_result.metadata_complete,"incoming directory remapping refused");
 uint8_t *packed_toc=alloc(&c,MB_IOSTORE_NONE_MAX_TOC_BYTES,1),*transfer=alloc(&c,65536,1);uint32_t *sort_rows=alloc(&c,emitted,sizeof(*sort_rows));
 REQUIRE(packed_toc&&transfer&&sort_rows,"packer storage refused");
 c.directory_fd=open(argv[6],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);REQUIRE(c.directory_fd>=0,"fresh output directory open failed");
 c.sidecar_fd=openat(c.directory_fd,"generated-sidecar.ucas",O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
 REQUIRE(c.sidecar_fd>=0,"fresh sidecar creation failed");
 mb_iostore_none_params_v1 pack_params={.struct_size=sizeof(pack_params),.abi_version=1,.chunks=chunks,.chunk_count=emitted,
  .directory_index=small_directory,.directory_bytes=(uint32_t)small_directory_result.output_bytes,.io_context=&c,.read_chunk=read_chunk,.write_ucas=write_sidecar};
 put_le(pack_params.container_id,base_h.container_id,8);
 mb_iostore_none_workspace_v1 pack_ws={sizeof(pack_ws),1,packed_toc,MB_IOSTORE_NONE_MAX_TOC_BYTES,transfer,65536,sort_rows,emitted};
 mb_iostore_none_result_v1 pack_result;
 REQUIRE(mb_iostore_none_pack_v1(&pack_params,&pack_ws,&pack_result)==MB_IOSTORE_NONE_OK&&pack_result.complete,"streamed source packing failed");
 REQUIRE(!fsync(c.sidecar_fd),"sidecar sync failed");struct stat sidecar_stat;
 REQUIRE(!fstat(c.sidecar_fd,&sidecar_stat)&&sidecar_stat.st_size>=0&&(uint64_t)sidecar_stat.st_size==pack_result.ucas_bytes,"sidecar size mismatch");
 mb_iostore_header_result_v1 packed_h;REQUIRE(parse_toc(packed_toc,(size_t)pack_result.toc_bytes,&packed_h),"generated small TOC geometry rejected");
 uint32_t assets_verified=0;
 for(uint32_t row=0;row<emitted;++row)if(c.source_rows[row]!=UINT32_MAX){uint32_t inputrow=c.source_rows[row];
  REQUIRE(!memcmp(packed_toc+(size_t)packed_h.entry_metadata.offset+(size_t)row*24,
   mod_toc+(size_t)mod_h.entry_metadata.offset+(size_t)inputrow*24,20),"streamed asset digest differs from original TOC");++assets_verified;}
 uint32_t scratch_count=base_h.block_count>packed_h.block_count?base_h.block_count+1:packed_h.block_count+1;
 if(scratch_count<base_h.overflow_chunk_count)scratch_count=base_h.overflow_chunk_count;
 if(scratch_count<packed_h.overflow_chunk_count)scratch_count=packed_h.overflow_chunk_count;
 mb_toc_overlay_id_row_v1 *base_rows=alloc(&c,base_h.chunk_count,sizeof(*base_rows)),*small_rows=alloc(&c,emitted,sizeof(*small_rows));
 uint32_t *scratch=alloc(&c,scratch_count,sizeof(*scratch));REQUIRE(base_rows&&small_rows&&scratch,"overlay workspace refused");
 mb_toc_overlay_workspace_v1 overlay_ws={sizeof(overlay_ws),1,base_rows,small_rows,base_h.chunk_count,emitted,scratch,scratch_count};
 mb_toc_overlay_params_v1 overlay_params={sizeof(overlay_params),1,{base_toc,base_bytes,0},{packed_toc,(size_t)pack_result.toc_bytes,0},
  {base_toc+(size_t)base_h.directory_index.offset,(size_t)base_h.directory_index.bytes,0},base_ucas_bytes,pack_result.ucas_bytes};
 mb_toc_overlay_result_v1 overlay_result;
 REQUIRE(mb_iostore_toc_overlay_v1(&overlay_params,&overlay_ws,NULL,0,&overlay_result)==MB_TOC_OVERLAY_OUTPUT_TOO_SMALL,
  "overlay admission/measurement refused");
 uint32_t final_count=base_h.chunk_count;
 for(uint32_t i=0;i<emitted;++i)if(!base_has_id(base_rows,base_h.chunk_count,small_rows[i].id))++final_count;
 REQUIRE(final_count<=MB_IOSTORE_MAX_CHUNKS,"derived final ID count exceeds cap");
 uint8_t *final_ids=alloc(&c,final_count,12);REQUIRE(final_ids,"final ID storage refused");
 memcpy(final_ids,base_toc+(size_t)base_h.chunk_ids.offset,(size_t)base_h.chunk_count*12);uint32_t next=base_h.chunk_count;
 for(uint32_t i=0;i<emitted;++i)if(!base_has_id(base_rows,base_h.chunk_count,small_rows[i].id)){
  memcpy(final_ids+(size_t)next*12,small_rows[i].id,12);++next;}
 directory_params.base_directory=(mb_directory_merge_bytes_v1){base_toc+(size_t)base_h.directory_index.offset,(size_t)base_h.directory_index.bytes};
 directory_params.mod_directory=incoming_directory;directory_params.base_ids=(mb_directory_merge_ids_v1){base_toc+(size_t)base_h.chunk_ids.offset,base_h.chunk_count};
 directory_params.mod_ids=incoming_ids;directory_params.final_ids=(mb_directory_merge_ids_v1){final_ids,final_count};
 mb_directory_merge_result_v1 directory_result;
 REQUIRE(mb_iostore_directory_merge_v1(&directory_params,&directory_ws,merged_directory,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&directory_result)==MB_DIRECTORY_MERGE_OK&&
  directory_result.metadata_complete,"final directory merge refused");
 uint64_t final_toc_bytes=overlay_result.required_output_bytes-base_h.directory_index.bytes+directory_result.output_bytes;
 REQUIRE(final_toc_bytes&&final_toc_bytes<=MB_IOSTORE_MAX_TOC_BYTES,"final TOC allocation exceeds cap");
 uint8_t *final_toc=alloc(&c,(size_t)final_toc_bytes,1);REQUIRE(final_toc,"final TOC storage refused");
 overlay_params.merged_directory=(mb_iostore_span_v1){merged_directory,(size_t)directory_result.output_bytes,0};
 REQUIRE(mb_iostore_toc_overlay_v1(&overlay_params,&overlay_ws,final_toc,(size_t)final_toc_bytes,&overlay_result)==MB_TOC_OVERLAY_OK&&
  overlay_result.metadata_complete&&overlay_result.output_bytes==final_toc_bytes,"final overlay preparation refused");
 REQUIRE(write_new(&c,"generated-small.utoc",packed_toc,(size_t)pack_result.toc_bytes)&&
  write_new(&c,"merged-container-header.bin",merged_header,merged_header_bytes)&&
  write_new(&c,"generated-directory.bin",merged_directory,(size_t)directory_result.output_bytes)&&
  write_new(&c,"generated-final.utoc",final_toc,(size_t)final_toc_bytes),"fresh metadata output/readback failed");
 REQUIRE(stable_inputs(&c),"source FD snapshot changed during preparation");
 clock_gettime(CLOCK_MONOTONIC,&end);
 double elapsed=(double)(end.tv_sec-start.tv_sec)+(double)(end.tv_nsec-start.tv_nsec)/1e9;
 printf("{\"phase\":\"prepared\",\"elapsed_seconds\":%.6f,\"allocation_peak_bytes\":%"PRIu64",\"allocation_count\":%zu,"
  "\"source_reads\":%"PRIu64",\"source_bytes\":%"PRIu64",\"original_mod_chunks\":%u,\"emitted_chunks\":%u,\"incoming_asset_hashes_verified\":%u,"
  "\"source_header_hash_verified\":true,\"captured_base_header_hash_verified\":true,\"base_container_id\":%"PRIu64","
  "\"merged_header_bytes\":%zu,\"merged_packages\":%u,\"replaced_packages\":%u,\"added_packages\":%u,"
  "\"packed_toc_bytes\":%"PRIu64",\"sidecar_bytes\":%"PRIu64",\"sidecar_blocks\":%u,\"directory_bytes\":%"PRIu64","
  "\"directory_files\":%u,\"directory_overrides\":%u,\"final_toc_bytes\":%"PRIu64",\"final_chunks\":%u,"
  "\"source_snapshots_stable\":true,\"oracle_inputs\":0,\"raw_chunk_inputs\":0,\"external_preparation_helpers\":0,\"base_ucas_opened\":false,\"installation_supported\":false}\n",
  elapsed,c.peak,c.count,c.source_reads,c.source_bytes,mod_h.chunk_count,emitted,assets_verified,base_h.container_id,
  merged_header_bytes,header_result.merged_package_count,header_result.replaced_package_count,header_result.added_package_count,
  pack_result.toc_bytes,pack_result.ucas_bytes,pack_result.block_count,directory_result.output_bytes,directory_result.merged_files,
  directory_result.exact_path_overrides,final_toc_bytes,overlay_result.final_chunks);
 exit_code=0;
done:
 cleanup(&c);if(c.live){fprintf(stderr,"allocation cleanup imbalance\n");exit_code=1;}
 return exit_code;
}
