#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "archive_selection.h"
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

/* Single-process host example. No logical archive path is a disk destination. */
#define ROWS 512u
#define NAMES 262144u
#define MEMBER_CAP (MB_IOSTORE_ARCHIVE_MAX_PROBES+2u)
static mb_archive_row_v1 rows[ROWS],replay_rows[ROWS];
static char names[NAMES],replay_names[NAMES],group_keys[NAMES];
static uint32_t sort_rows[ROWS],replay_sort[ROWS],group_sort[ROWS],selected[ROWS];
static uint8_t transfer[65536],replay_transfer[65536],probe_scratch[65536],primary_scratch[65536];
static mb_archive_group_v1 groups[ROWS];
static mb_archive_group_member_v1 group_members[ROWS];
static mb_iostore_archive_member_v1 members[ROWS];
static mb_archive_owned_file_v1 journal[MEMBER_CAP+3];

static int decimal(const char *s,uint64_t *out) {
 if(!s||!*s)return 0;uint64_t n=0;
 for(const unsigned char *p=(const unsigned char *)s;*p;++p){
  if(*p<'0'||*p>'9'||n>(UINT64_MAX-(*p-'0'))/10)return 0;n=n*10+(*p-'0');}
 *out=n;return 1;
}
static int unhex(const char *s,uint8_t *out) {
 if(!s||strlen(s)!=64)return 0;
 for(unsigned i=0;i<32;++i){unsigned values[2];
  for(unsigned j=0;j<2;++j){unsigned char c=(unsigned char)s[2*i+j];
   values[j]=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:99;}
  if(values[0]>15||values[1]>15)return 0;out[i]=(uint8_t)(values[0]*16+values[1]);}
 return 1;
}
static int same_inode(const struct stat *a,const struct stat *b) {
 return a->st_dev==b->st_dev&&a->st_ino==b->st_ino;
}
static int stable(const struct stat *a,const struct stat *b) {
 if(!same_inode(a,b)||a->st_mode!=b->st_mode||a->st_size!=b->st_size)return 0;
#ifdef __APPLE__
 return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
  a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
 return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
  a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int read_all(int fd,uint8_t *out,size_t bytes) {
 size_t at=0;unsigned calls=0;
 while(at<bytes){if(++calls>65536)return 0;size_t wanted=bytes-at;if(wanted>65536)wanted=65536;
  ssize_t n=pread(fd,out+at,wanted,(off_t)at);if(n<0&&errno==EINTR)continue;
  if(n<=0||(size_t)n>wanted)return 0;at+=(size_t)n;}
 return 1;
}
static const mb_archive_owned_file_v1 *owned_member(uint32_t row,uint32_t count) {
 const mb_archive_owned_file_v1 *found=NULL;
 for(uint32_t i=0;i<count;++i)if(journal[i].row_index==row&&journal[i].state==MB_ARCHIVE_CREATED){
  if(found)return NULL;found=journal+i;}
 return found;
}
static int row_fd(uint32_t row,const uint32_t *opened_rows,const int *fds,uint32_t count) {
 for(uint32_t i=0;i<count;++i)if(opened_rows[i]==row)return fds[i];return -1;
}
#define REQUIRE(x,label) do {if(!(x)){fprintf(stderr,"%s; errno=%d\n",label,errno);goto done;}}while(0)
int main(int argc,char **argv) {
 if(argc!=10){fprintf(stderr,"usage: archive_job ARCHIVE SHA256 BYTES TOKEN BASE_TOC BASE_HEADER BASE_UCAS_BYTES QUARANTINE OUTPUT_DIRECTORY\n");return 2;}
 int archive_fd=-1,base_toc_fd=-1,base_header_fd=-1,qfd=-1,outfd=-1,rc=1;
 int materialized_fds[MEMBER_CAP];uint32_t opened_rows[MEMBER_CAP],opened_count=0,probe_count=0;
 mb_iostore_archive_probe_v1 probes[MB_IOSTORE_ARCHIVE_MAX_PROBES]={{0}};
 for(unsigned i=0;i<MEMBER_CAP;++i)materialized_fds[i]=-1;
 mb_archive_source_v1 source={.struct_size=sizeof(source),.abi_version=1};
 uint64_t base_ucas_bytes=0;
 REQUIRE(unhex(argv[2],source.sha256)&&decimal(argv[3],&source.size)&&source.size&&
  unhex(argv[4],source.target_token)&&decimal(argv[7],&base_ucas_bytes)&&base_ucas_bytes,"invalid source observation");
 struct stat archive_before,archive_after,qstat,outstat;
 archive_fd=open(argv[1],O_RDONLY|O_NOFOLLOW|O_CLOEXEC);source.fd=archive_fd;
 REQUIRE(archive_fd>=0&&!fstat(archive_fd,&archive_before)&&S_ISREG(archive_before.st_mode)&&
  archive_before.st_size>0&&(uint64_t)archive_before.st_size==source.size,"archive source refused");
 mb_archive_limits_v1 limits;mb_archive_limits_default_v1(&limits);
 limits.raw_bytes=UINT64_C(512)<<20;limits.member_bytes=UINT64_C(512)<<20;
 limits.expanded_bytes=UINT64_C(1024)<<20;limits.entries=ROWS;limits.name_bytes=NAMES;
 mb_archive_budget_v1 budget;mb_archive_budget_default_v1(&budget);
 budget.io_limit=UINT64_C(8)<<30;budget.operation_limit=1000000;budget.path_work_limit=UINT64_C(16)<<20;
 mb_archive_buffers_v1 scan={rows,ROWS,names,NAMES,sort_rows,ROWS,transfer,sizeof(transfer)};
 mb_archive_inventory_v1 inventory={0};
 struct timespec started,ended;clock_gettime(CLOCK_MONOTONIC,&started);
 int intake_status=mb_archive_scan_v1(&source,&limits,&budget,&scan,&inventory);
 REQUIRE(intake_status==MB_ARCHIVE_OK&&inventory.complete==1,"archive scan incomplete");
 mb_archive_prepared_view_v1 view={&inventory,rows,ROWS,names,NAMES};
 mb_iostore_archive_workspace_v1 workspace={sizeof(workspace),1,
  {group_sort,ROWS,groups,ROWS,group_members,ROWS,group_keys,NAMES},
  probe_scratch,sizeof(probe_scratch),primary_scratch,sizeof(primary_scratch)};
 mb_iostore_archive_selection_v1 selection={0};
 mb_iostore_archive_status_v1 selection_status=mb_iostore_archive_select_v1(
  &view,NULL,0,&workspace,selected,ROWS,members,ROWS,&selection);
 if(selection_status!=MB_IOSTORE_ARCHIVE_OK&&selection_status!=MB_IOSTORE_ARCHIVE_PROBES_REQUIRED){
  fprintf(stderr,"archive scope refused status=%u candidates=%u problem_row=%u\n",selection_status,selection.candidate_count,selection.problem_row);goto done;}
 REQUIRE(selection.selection_complete==1&&selection.member_count&&selection.member_count<=MEMBER_CAP&&
  !memcmp(selection.source_sha256,source.sha256,32)&&!memcmp(selection.inventory_sha256,inventory.inventory_sha256,32)&&
  !memcmp(selection.target_token,source.target_token,32),"selection identity mismatch");
 const uint32_t selected_count=selection.member_count;
 qfd=open(argv[8],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 outfd=open(argv[9],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 REQUIRE(qfd>=0&&outfd>=0&&!fstat(qfd,&qstat)&&!fstat(outfd,&outstat)&&
  S_ISDIR(qstat.st_mode)&&S_ISDIR(outstat.st_mode)&&!same_inode(&qstat,&outstat)&&
  qstat.st_uid==geteuid()&&outstat.st_uid==geteuid()&&!(qstat.st_mode&077)&&!(outstat.st_mode&077),"private directories refused");
 mb_archive_materialize_request_v1 request={0};
 request.source=&source;request.inventory=&inventory;request.rows=rows;request.row_capacity=ROWS;
 request.names=names;request.name_capacity=NAMES;request.selected=selected;request.selected_count=selected_count;
 memcpy(request.target_token,source.target_token,32);request.quarantine_fd=qfd;
 request.quarantine_device=(uint64_t)qstat.st_dev;request.quarantine_inode=(uint64_t)qstat.st_ino;
 request.journal=journal;request.journal_capacity=MEMBER_CAP+3;
 request.replay=(mb_archive_buffers_v1){replay_rows,ROWS,replay_names,NAMES,replay_sort,ROWS,replay_transfer,sizeof(replay_transfer)};
 mb_archive_materialized_v1 materialized={0};
 intake_status=mb_archive_materialize_v1(&request,&limits,&budget,&materialized);
 if(intake_status!=MB_ARCHIVE_OK){fprintf(stderr,"materialization refused status=%d complete=%u cleanup=%u journal=%u io=%"PRIu64"\n",
  intake_status,materialized.complete,materialized.cleanup_complete,materialized.journal_count,budget.io_used);goto done;}
 REQUIRE(intake_status==MB_ARCHIVE_OK&&materialized.complete==1&&materialized.selected_count==selected_count&&
  materialized.journal_count<=MEMBER_CAP+3&&!memcmp(materialized.inventory_sha256,inventory.inventory_sha256,32)&&
  !memcmp(materialized.target_token,source.target_token,32),"materialization incomplete");
 for(uint32_t i=0;i<selected_count;++i){
  const mb_iostore_archive_member_v1 *m=members+i;
  const mb_archive_owned_file_v1 *owned=owned_member(m->row_index,materialized.journal_count);
  REQUIRE(owned&&memchr(owned->name,0,sizeof(owned->name))&&owned->name[0]&&!strchr(owned->name,'/')&&
   owned->expected_size==m->bytes&&owned->written_bytes==m->bytes&&!memcmp(owned->sha256,m->sha256,32),"materialized member binding failed");
  int fd=openat(qfd,owned->name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
  REQUIRE(fd>=0,"open bound member failed");materialized_fds[opened_count]=fd;opened_rows[opened_count++]=m->row_index;
  struct stat st;REQUIRE(!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size>=0&&(uint64_t)st.st_size==m->bytes&&
   (uint64_t)st.st_dev==owned->device&&(uint64_t)st.st_ino==owned->inode,"bound member inode changed");
  if(m->probe_required){
   uint64_t cap=m->role==MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION?MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES:MB_IOSTORE_ARCHIVE_MAX_DOCUMENT_BYTES;
   REQUIRE(probe_count<MB_IOSTORE_ARCHIVE_MAX_PROBES&&m->bytes<=cap,"probe input too large");
   uint8_t *bytes=malloc(m->bytes?(size_t)m->bytes:1);REQUIRE(bytes,"probe allocation failed");
   probes[probe_count++]=(mb_iostore_archive_probe_v1){m->row_index,0,bytes,(size_t)m->bytes};
   REQUIRE(read_all(fd,bytes,(size_t)m->bytes),"probe read failed");
  }
 }
 selection_status=mb_iostore_archive_select_v1(&view,probes,probe_count,&workspace,selected,ROWS,members,ROWS,&selection);
 if(selection_status!=MB_IOSTORE_ARCHIVE_OK){fprintf(stderr,"content probes refused status=%u row=%u pak_status=%u reason=%u\n",
  selection_status,selection.problem_row,selection.pak_inner_status,selection.pak_inner_reason);goto done;}
 REQUIRE(selection_status==MB_IOSTORE_ARCHIVE_OK&&selection.selection_complete==1&&selection.preparation_ready==1&&
  selection.member_count==selected_count&&!memcmp(selection.inventory_sha256,inventory.inventory_sha256,32),"companion/content scope refused");
 for(uint32_t i=0;i<probe_count;++i){free((void *)probes[i].data);probes[i].data=NULL;}
 int mod_toc_fd=row_fd(selection.toc_row,opened_rows,materialized_fds,opened_count);
 int mod_ucas_fd=row_fd(selection.ucas_row,opened_rows,materialized_fds,opened_count);
 REQUIRE(mod_toc_fd>=0&&mod_ucas_fd>=0,"selected container descriptors absent");
 base_toc_fd=open(argv[5],O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 base_header_fd=open(argv[6],O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 REQUIRE(base_toc_fd>=0&&base_header_fd>=0,"base metadata open failed");
 mb_iostore_file_job_params_v1 job={.struct_size=sizeof(job),.abi_version=1,
  .mod_toc_fd=mod_toc_fd,.mod_ucas_fd=mod_ucas_fd,.base_toc_fd=base_toc_fd,.base_header_fd=base_header_fd,
  .destination_dir_fd=outfd,.immutable_inputs=1,.private_unpublished_directory=1,.observed_base_ucas_bytes=base_ucas_bytes};
 mb_iostore_file_job_result_v1 result={0};
 mb_iostore_file_job_status_v1 job_status=mb_iostore_original_file_job_v1(&job,&result);
 if(job_status!=MB_IOSTORE_FILE_JOB_OK||!result.prepared){fprintf(stderr,"preparation refused status=%u stage=%u system=%d owned_mask=%u\n",job_status,result.stage,result.system_error,result.created_artifact_mask);goto done;}
 REQUIRE(!result.allocation_live_after_cleanup&&result.allocations==result.releases&&!result.installation_authority&&
  !fstat(archive_fd,&archive_after)&&stable(&archive_before,&archive_after),"composition completion snapshot failed");
 clock_gettime(CLOCK_MONOTONIC,&ended);
 double seconds=(double)(ended.tv_sec-started.tv_sec)+(double)(ended.tv_nsec-started.tv_nsec)/1e9;
 rc=0;
done:
 for(uint32_t i=0;i<probe_count;++i)free((void *)probes[i].data);
 for(uint32_t i=0;i<opened_count;++i)if(materialized_fds[i]>=0&&close(materialized_fds[i])&&!rc)rc=1;
 if(base_toc_fd>=0&&close(base_toc_fd)&&!rc)rc=1;if(base_header_fd>=0&&close(base_header_fd)&&!rc)rc=1;
 if(outfd>=0&&close(outfd)&&!rc)rc=1;if(qfd>=0&&close(qfd)&&!rc)rc=1;if(archive_fd>=0&&close(archive_fd)&&!rc)rc=1;
 if(!rc){
 printf("{\"phase\":\"archive_prepared\",\"seconds\":%.6f,\"inventory_rows\":%u,\"selected_members\":%u,\"toc_row\":%u,\"ucas_row\":%u,"
  "\"verified_empty_paks\":%u,\"verified_documents\":%u,\"journal_count\":%u,\"io_budget_used\":%"PRIu64","
  "\"source_asset_hashes_verified\":%u,\"output_chunks\":%u,\"sidecar_bytes\":%"PRIu64",\"final_toc_bytes\":%"PRIu64","
  "\"job_peak_allocated_bytes\":%"PRIu64",\"allocations\":%u,\"releases\":%u,\"source_snapshot_stable\":true,"
  "\"pre_extracted_inputs\":0,\"per_mod_member_facts\":0,\"preparation_subprocesses\":0,\"installation_authority\":false}\n",
  seconds,inventory.row_count,selected_count,selection.toc_row,selection.ucas_row,selection.omitted_empty_paks,selection.omitted_documents,
  materialized.journal_count,budget.io_used,result.incoming_asset_hashes_verified,result.emitted_chunks,result.artifact_bytes[0],result.artifact_bytes[4],
  result.allocation_peak_bytes,result.allocations,result.releases);
 }
 /* Quarantine journals and any created output stay unpublished on failure. */
 return rc;
}
