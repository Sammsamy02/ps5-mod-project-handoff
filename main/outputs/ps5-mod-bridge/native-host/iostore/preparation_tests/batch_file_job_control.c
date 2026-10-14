#define _POSIX_C_SOURCE 200809L
#include "original_file_job.h"
#include "toc_reader.h"
#include "none_range_reader.h"
#include "blake3.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static unsigned assertions;
#define CHECK(c) do{++assertions;if(!(c)){fprintf(stderr,"assertion %u failed: %s\n",assertions,#c);goto done;}}while(0)
static int exact_pread(int fd,uint64_t offset,uint8_t *out,size_t bytes){size_t at=0;while(at<bytes){ssize_t n=pread(fd,out+at,bytes-at,(off_t)(offset+at));if(n<0&&errno==EINTR)continue;if(n<=0)return 0;at+=(size_t)n;}return 1;}
static int exact_pwrite(int fd,const uint8_t *data,size_t bytes){size_t at=0;while(at<bytes){ssize_t n=pwrite(fd,data+at,bytes-at,(off_t)at);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;at+=(size_t)n;}return 1;}
static uint64_t le(const uint8_t *p,unsigned bytes){uint64_t n=0;for(unsigned i=0;i<bytes;++i)n|=(uint64_t)p[i]<<(8u*i);return n;}
static int fixture_source(void *context,uint32_t partition,uint64_t offset,uint8_t *out,size_t requested,size_t *actual){int fd=*(int *)context;*actual=0;if(partition||!exact_pread(fd,offset,out,requested))return 1;*actual=requested;return 0;}
/* Generic tiny fixture clone. Variant1 is byte-identical; variant2 changes a
 * nonheader raw ID only (overlapping store packages); variant3 changes that
 * package-ID vector element too (same path but disjoint IDs). No originals change. */
static int clone_fixture(int directory,unsigned variant,int original_toc,int original_ucas,int out[2]){
 struct stat ts,us;if(fstat(original_toc,&ts)||fstat(original_ucas,&us)||ts.st_size<=0||us.st_size<=0||ts.st_size>(1<<20)||us.st_size>(1<<20))return 0;
 size_t tn=(size_t)ts.st_size,un=(size_t)us.st_size;uint8_t *toc=malloc(tn),*ucas=malloc(un);int okay=0,writers[2]={-1,-1};
 if(!toc||!ucas||!exact_pread(original_toc,0,toc,tn)||!exact_pread(original_ucas,0,ucas,un))goto done;
 mb_iostore_header_result_v1 h;mb_iostore_view_v1 view={sizeof(view),1,{toc,tn<144?tn:144,0},tn};mb_iostore_limits_v1 limits;mb_iostore_limits_v1_default(&limits);
 if(mb_iostore_read_header_v1(&view,&limits,&h)!=MB_IOSTORE_OK||h.declared_layout_end!=tn)goto done;
 uint32_t asset=UINT32_MAX,header=UINT32_MAX;
 for(uint32_t i=0;i<h.chunk_count;++i){if(toc[(size_t)h.chunk_ids.offset+(size_t)i*12+11]==6)header=i;else if(asset==UINT32_MAX)asset=i;}
 if(asset==UINT32_MAX||header==UINT32_MAX)goto done;
 if(variant>=2)toc[(size_t)h.chunk_ids.offset+(size_t)asset*12]^=0x80;
 if(variant==3){uint64_t length=un;mb_none_range_source_v1 source={sizeof(source),1,{toc,tn,0},&length,1,0,fixture_source,&original_ucas};
  mb_none_range_plan_v1 plan;mb_none_range_result_v1 measured;
  if(mb_iostore_none_plan_v1(&source,header,&plan,&measured)!=MB_NONE_RANGE_OK||!measured.full_chunk_coverage_scanned||plan.first_block!=plan.last_block||plan.logical_bytes<28)goto done;
  const uint8_t *block=toc+(size_t)h.compression_blocks.offset+(size_t)plan.first_block*12;
  uint64_t physical=le(block,5)+(plan.logical_offset%h.compression_block_bytes),bytes=plan.logical_bytes;
  if(physical>un||bytes>un-physical)goto done;uint8_t *raw=ucas+(size_t)physical;
  if(le(raw+4,4)!=5||le(raw+16,4)!=1)goto done;
  raw[20]^=0x80;blake3_hasher hasher;blake3_hasher_init(&hasher);blake3_hasher_update(&hasher,raw,(size_t)bytes);
  blake3_hasher_finalize(&hasher,toc+(size_t)h.entry_metadata.offset+(size_t)header*24,20);
 }
 char name[64];for(unsigned i=0;i<2;++i){snprintf(name,sizeof(name),"variant-%u-%s.bin",variant,i?"ucas":"toc");
  writers[i]=openat(directory,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(writers[i]<0)goto done;
  if(!exact_pwrite(writers[i],i?ucas:toc,i?un:tn)||close(writers[i])){writers[i]=-1;goto done;}writers[i]=-1;
  out[i]=openat(directory,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(out[i]<0)goto done;
 }okay=1;
 done:for(unsigned i=0;i<2;++i)if(writers[i]>=0)close(writers[i]);free(toc);free(ucas);return okay;
}
static int sources_open_same_positions(const int fds[6],const off_t offsets[6]){
 for(unsigned i=0;i<6;++i)if(fcntl(fds[i],F_GETFL)<0||lseek(fds[i],0,SEEK_CUR)!=offsets[i])return 0;return 1;
}
static void print_preparation(const mb_iostore_batch_file_job_result_v1 *b,double seconds){const mb_iostore_file_job_result_v1 *r=&b->preparation;
 printf("{\"status\":%u,\"stage\":%u,\"prepared\":%u,\"source_count\":%u,\"created_mask\":%u,\"allocation_peak_bytes\":%"PRIu64","
  "\"allocation_live_after_cleanup\":%"PRIu64",\"allocations\":%u,\"releases\":%u,\"owned_fds_closed\":%u,\"source_reads\":%"PRIu64","
  "\"source_bytes\":%"PRIu64",\"original_mod_chunks\":%u,\"emitted_chunks\":%u,\"incoming_asset_hashes_verified\":%u,"
  "\"base_container_id\":%"PRIu64",\"merged_packages\":%u,\"replaced_packages\":%u,\"added_packages\":%u,\"sidecar_blocks\":%u,"
  "\"directory_files\":%u,\"directory_overrides\":%u,\"final_chunks\":%u,\"source_snapshots_stable\":%u,\"installation_authority\":%u,\"seconds\":%.6f}",
  r->status,r->stage,r->prepared,b->source_count,r->created_artifact_mask,r->allocation_peak_bytes,r->allocation_live_after_cleanup,r->allocations,r->releases,
  r->owned_fds_closed,r->source_read_calls,r->source_read_bytes,r->original_mod_chunks,r->emitted_chunks,r->incoming_asset_hashes_verified,r->base_container_id,
  r->merged_packages,r->replaced_packages,r->added_packages,r->sidecar_blocks,r->directory_files,r->directory_overrides,r->final_chunks,r->source_snapshots_stable,r->installation_authority,seconds);
}
int main(int argc,char **argv){
 if(argc!=9){fprintf(stderr,"usage: batch_control modA_toc modA_ucas modB_toc modB_ucas base_toc base_header base_ucas_length fresh_trial_directory\n");return 2;}
 int okay=0,inputs[6]={-1,-1,-1,-1,-1,-1},trial=-1,output=-1,fixtures=-1,conflicts[3]={-1,-1,-1},clones[3][2]={{-1,-1},{-1,-1},{-1,-1}};off_t offsets[6];
 mb_iostore_batch_file_job_result_v1 result,refused[3];
 for(unsigned i=0;i<6;++i){inputs[i]=open(argv[1+i],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);CHECK(inputs[i]>=0);offsets[i]=11+(off_t)i;CHECK(lseek(inputs[i],offsets[i],SEEK_SET)==offsets[i]);}
 char *end;errno=0;uint64_t base_length=strtoull(argv[7],&end,10);CHECK(!errno&&!*end&&base_length);
 trial=open(argv[8],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(trial>=0);
 CHECK(!mkdirat(trial,"output",0700));CHECK(!mkdirat(trial,"fixtures",0700));
 output=openat(trial,"output",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);fixtures=openat(trial,"fixtures",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(output>=0&&fixtures>=0);
 mb_iostore_batch_source_v1 sources[2]={{inputs[0],inputs[1]},{inputs[2],inputs[3]}};
 mb_iostore_batch_file_job_params_v1 p={.struct_size=sizeof(p),.abi_version=1,.sources=sources,.source_count=2,.base_toc_fd=inputs[4],
  .base_header_fd=inputs[5],.destination_dir_fd=output,.immutable_inputs=1,.private_unpublished_directory=1,.observed_base_ucas_bytes=base_length};
 struct timespec start,finish;clock_gettime(CLOCK_MONOTONIC,&start);
 CHECK(mb_iostore_batch_file_job_v1(&p,&result)==MB_IOSTORE_FILE_JOB_OK);clock_gettime(CLOCK_MONOTONIC,&finish);
 double seconds=(double)(finish.tv_sec-start.tv_sec)+(double)(finish.tv_nsec-start.tv_nsec)/1e9;
 CHECK(result.preparation.prepared&&result.preparation.created_artifact_mask==31&&!result.conflict_kind&&result.preparation.source_snapshots_stable);
 CHECK(!result.preparation.allocation_live_after_cleanup&&result.preparation.allocations==result.preparation.releases&&result.preparation.owned_fds_closed==5);
 CHECK(sources_open_same_positions(inputs,offsets)&&fcntl(output,F_GETFL)>=0);
 for(unsigned v=0;v<3;++v){char name[48];snprintf(name,sizeof(name),"conflict-%u",v+1u);CHECK(!mkdirat(trial,name,0700));
  conflicts[v]=openat(trial,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(conflicts[v]>=0);
  CHECK(clone_fixture(fixtures,v+1u,inputs[2],inputs[3],clones[v]));
  sources[0]=(mb_iostore_batch_source_v1){inputs[2],inputs[3]};sources[1]=(mb_iostore_batch_source_v1){clones[v][0],clones[v][1]};p.destination_dir_fd=conflicts[v];
  CHECK(mb_iostore_batch_file_job_v1(&p,refused+v)==MB_IOSTORE_FILE_JOB_CONFLICT);
  CHECK(refused[v].conflict_kind==(uint32_t)(MB_IOSTORE_BATCH_CONFLICT_CHUNK_ID+v)&&refused[v].conflict_source_a==0&&refused[v].conflict_source_b==1);
  CHECK(!refused[v].preparation.prepared&&!refused[v].preparation.created_artifact_mask&&!refused[v].preparation.owned_fds_closed&&!refused[v].preparation.source_snapshots_stable);
  CHECK(!refused[v].preparation.allocation_live_after_cleanup&&refused[v].preparation.allocations==refused[v].preparation.releases);
  for(unsigned i=0;i<5;++i){struct stat st;CHECK(fstatat(conflicts[v],mb_iostore_file_job_artifact_name_v1(i),&st,AT_SYMLINK_NOFOLLOW)<0&&errno==ENOENT);}
  CHECK(sources_open_same_positions(inputs,offsets)&&fcntl(clones[v][0],F_GETFL)>=0&&fcntl(clones[v][1],F_GETFL)>=0&&lseek(clones[v][0],0,SEEK_CUR)==0&&lseek(clones[v][1],0,SEEK_CUR)==0);
 }
 printf("{\"phase\":\"passed\",\"assertions\":%u,\"borrowed_fds_and_positions_preserved\":true,\"expectation_inputs\":0,\"batch\":",assertions);
 print_preparation(&result,seconds);printf(",\"conflicts\":[");
 for(unsigned i=0;i<3;++i){if(i)printf(",");printf("{\"kind\":%u,\"source_a\":%u,\"source_b\":%u,\"prepared\":%u,\"created_mask\":%u,\"source_snapshots_stable\":%u,\"allocations\":%u,\"releases\":%u}",
  refused[i].conflict_kind,refused[i].conflict_source_a,refused[i].conflict_source_b,refused[i].preparation.prepared,refused[i].preparation.created_artifact_mask,
  refused[i].preparation.source_snapshots_stable,refused[i].preparation.allocations,refused[i].preparation.releases);}
 printf("]}\n");okay=1;
 done:for(unsigned i=0;i<6;++i)if(inputs[i]>=0)close(inputs[i]);if(trial>=0)close(trial);if(output>=0)close(output);if(fixtures>=0)close(fixtures);
 for(unsigned v=0;v<3;++v){if(conflicts[v]>=0)close(conflicts[v]);for(unsigned i=0;i<2;++i)if(clones[v][i]>=0)close(clones[v][i]);}return okay?0:1;
}
