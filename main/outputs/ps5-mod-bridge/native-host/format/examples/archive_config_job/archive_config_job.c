#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "candidate_selector.h"
#include "config_archive_scope.h"
#include "prepare_config_pak.h"
#include <mbedtls/sha256.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ROWS 16u
#define NAMES 4096u
static mb_archive_row_v1 rows[ROWS],replay_rows[ROWS];
static uint32_t order[ROWS],replay_order[ROWS],selected[1];
static char names[NAMES],replay_names[NAMES];
static uint8_t chunk[MB_ARCHIVE_CHUNK_BYTES],replay_chunk[MB_ARCHIVE_CHUNK_BYTES];
static int unhex(const char *s,uint8_t *output,size_t count) {
 if(!s||strlen(s)!=count*2)return 0;
 for(size_t i=0;i<count;++i){unsigned a,b;char x=s[2*i],y=s[2*i+1];
 a=x>='0'&&x<='9'?(unsigned)(x-'0'):x>='a'&&x<='f'?(unsigned)(x-'a'+10):99;
 b=y>='0'&&y<='9'?(unsigned)(y-'0'):y>='a'&&y<='f'?(unsigned)(y-'a'+10):99;
 if(a>15||b>15)return 0;output[i]=(uint8_t)((a<<4)|b);}
 return 1;
}
static int parse_u64(const char *s,uint64_t *value) {
 if(!s||!*s)return 0;uint64_t v=0;
 for(const unsigned char *p=(const unsigned char *)s;*p;++p){
 if(*p<'0'||*p>'9'||v>(UINT64_MAX-(uint64_t)(*p-'0'))/10)return 0;
 v=v*10+(uint64_t)(*p-'0');}*value=v;return 1;
}
static void hex(const uint8_t *p,char *out,size_t bytes) {
 static const char digits[]="0123456789abcdef";
 for(size_t i=0;i<bytes;++i){out[2*i]=digits[p[i]>>4];out[2*i+1]=digits[p[i]&15];}out[2*bytes]=0;
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
static int fd_sha256(int fd,uint64_t bytes,uint8_t digest[32]) {
 uint8_t data[MB_ARCHIVE_CHUNK_BYTES];uint64_t at=0;mbedtls_sha256_context c;mbedtls_sha256_init(&c);
 int rc=mbedtls_sha256_starts(&c,0);
 while(!rc&&at<bytes){size_t n=(size_t)(bytes-at>sizeof(data)?sizeof(data):bytes-at);
 ssize_t got=pread(fd,data,n,(off_t)at);if(got!=(ssize_t)n){rc=1;break;}
 rc=mbedtls_sha256_update(&c,data,n);at+=n;}
 if(!rc)rc=mbedtls_sha256_finish(&c,digest);mbedtls_sha256_free(&c);return rc;
}
static int mixed_control(void) {
 mb_archive_inventory_v1 inventory={0};mb_archive_row_v1 fixture[2]={{0},{0}};
 mb_archive_config_candidate_v1 candidate={0};inventory.complete=1;inventory.status=MB_ARCHIVE_OK;inventory.row_count=2;
 fixture[0].kind=fixture[1].kind=MB_ARCHIVE_REGULAR;candidate.complete=1;candidate.status=MB_ARCHIVE_CONFIG_SELECTOR_OK;
 candidate.candidate_count=1;candidate.row_index=0;
 int rc=pm_config_archive_scope(&inventory,fixture,2,&candidate);
 if(rc!=PM_CONFIG_SCOPE_UNHANDLED_REGULAR)return 1;
 puts("{\"control\":\"additional_regular_member\",\"refused\":true,\"materialization_calls\":0,\"preparation_calls\":0}");return 0;
}
int main(int argc,char **argv) {
 if(argc==2&&!strcmp(argv[1],"--mixed-member-control"))return mixed_control();
 if(argc<14||argc>13+64){fprintf(stderr,"usage: archive_config_job ORIGINAL_PAK ARCHIVE SHA256 BYTES TOKEN BASENAME MAX_MEMBER_BYTES TARGET_ENTRY MOUNT SEED FRESH_OUTPUT QUARANTINE SECTION [SECTION...]\n");return 2;}
 uint8_t expected_archive[32],target_token[32];uint64_t expected_size,max_member;
 if(!unhex(argv[3],expected_archive,32)||!parse_u64(argv[4],&expected_size)||!unhex(argv[5],target_token,32)||
 !parse_u64(argv[7],&max_member)||!max_member||max_member>MB_ARCHIVE_CONFIG_SELECTOR_MAX_SOURCE_BYTES)return 3;
 int archive_fd=-1,qfd=-1,member_fd=-1,rc=1;FILE *member_stream=NULL;pm_prepare_output_owner output_owner={0};
 struct stat archive_before,archive_after,qstat,member_before,member_after;
 archive_fd=open(argv[2],O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 if(archive_fd<0||fstat(archive_fd,&archive_before)||!S_ISREG(archive_before.st_mode)||archive_before.st_size<0||
 (uint64_t)archive_before.st_size!=expected_size)goto done;
 mb_archive_source_v1 source={0};source.struct_size=sizeof(source);source.abi_version=MB_ARCHIVE_ABI_V1;
 source.fd=archive_fd;source.size=expected_size;memcpy(source.sha256,expected_archive,32);memcpy(source.target_token,target_token,32);
 mb_archive_limits_v1 limits;mb_archive_limits_default_v1(&limits);
 limits.raw_bytes=1048576;limits.member_bytes=1048576;limits.expanded_bytes=2097152;limits.entries=ROWS;limits.name_bytes=NAMES;
 mb_archive_budget_v1 budget;mb_archive_budget_default_v1(&budget);
 budget.io_limit=16777216;budget.operation_limit=100000;budget.path_work_limit=4194304;
 mb_archive_buffers_v1 scan={rows,ROWS,names,NAMES,order,ROWS,chunk,sizeof(chunk)};
 mb_archive_inventory_v1 inventory={0};
 rc=mb_archive_scan_v1(&source,&limits,&budget,&scan,&inventory);
 if(rc||inventory.complete!=1||inventory.status!=MB_ARCHIVE_OK||inventory.installation_supported){rc=1;goto done;}
 mb_archive_config_selector_profile_v1 profile={0};profile.struct_size=sizeof(profile);profile.abi_version=MB_ARCHIVE_CONFIG_SELECTOR_ABI_V1;
 profile.basename=argv[6];profile.basename_bytes=(uint32_t)strlen(argv[6]);profile.max_source_bytes=max_member;memcpy(profile.target_token,target_token,32);
 mb_archive_config_candidate_v1 candidate={0};
 rc=mb_archive_config_select_v1(&inventory,rows,ROWS,names,NAMES,&profile,&candidate);
 /* Corrected selector refuses bad aliases without resetting output: return first. */
 if(rc!=MB_ARCHIVE_CONFIG_SELECTOR_OK){rc=1;goto done;}
 if(candidate.complete!=1||candidate.candidate_count!=1||candidate.member_bytes>max_member||
 memcmp(candidate.source_sha256,expected_archive,32)||
 candidate.format_validated||candidate.platform_compatible||candidate.installation_supported||candidate.runtime_supported){rc=1;goto done;}
 rc=pm_config_archive_scope(&inventory,rows,ROWS,&candidate);
 if(rc!=PM_CONFIG_SCOPE_OK){fprintf(stderr,"config-only archive refused unhandled regular members status=%d\n",rc);rc=1;goto done;}
 qfd=open(argv[12],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(qfd<0||fstat(qfd,&qstat)||!S_ISDIR(qstat.st_mode)){rc=1;goto done;}
 mb_archive_owned_file_v1 journal[4]={{0}};mb_archive_materialized_v1 materialized={0};
 mb_archive_materialize_request_v1 request={0};
 request.source=&source;request.inventory=&inventory;request.rows=rows;request.row_capacity=ROWS;request.names=names;request.name_capacity=NAMES;
 selected[0]=candidate.row_index;request.selected=selected;request.selected_count=1;memcpy(request.target_token,target_token,32);
 request.quarantine_fd=qfd;request.quarantine_device=(uint64_t)qstat.st_dev;request.quarantine_inode=(uint64_t)qstat.st_ino;
 request.journal=journal;request.journal_capacity=4;
 request.replay=(mb_archive_buffers_v1){replay_rows,ROWS,replay_names,NAMES,replay_order,ROWS,replay_chunk,sizeof(replay_chunk)};
 rc=mb_archive_materialize_v1(&request,&limits,&budget,&materialized);
 if(rc||materialized.complete!=1||materialized.selected_count!=1||materialized.installation_supported){rc=1;goto done;}
 const mb_archive_owned_file_v1 *owned=NULL;
 for(uint32_t i=0;i<materialized.journal_count;++i)if(journal[i].row_index==candidate.row_index&&journal[i].state==MB_ARCHIVE_CREATED){
 if(owned){rc=1;goto done;}owned=&journal[i];}
 if(!owned||!memchr(owned->name,0,sizeof(owned->name))||owned->expected_size!=candidate.member_bytes||
 owned->written_bytes!=candidate.member_bytes||memcmp(owned->sha256,candidate.member_sha256,32)){rc=1;goto done;}
 member_fd=openat(qfd,owned->name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 if(member_fd<0||fstat(member_fd,&member_before)||!S_ISREG(member_before.st_mode)||member_before.st_size<0||
 (uint64_t)member_before.st_dev!=owned->device||(uint64_t)member_before.st_ino!=owned->inode||
 (uint64_t)member_before.st_size!=owned->expected_size){rc=1;goto done;}
 uint8_t digest[32];if(fd_sha256(member_fd,owned->expected_size,digest)||memcmp(digest,owned->sha256,32)){rc=1;goto done;}
 member_stream=fdopen(member_fd,"rb");if(!member_stream){rc=1;goto done;}member_fd=-1;
 char member_hex[65],archive_hex[65],inventory_hex[65];hex(digest,member_hex,32);hex(inventory.source_sha256,archive_hex,32);hex(inventory.inventory_sha256,inventory_hex,32);
 printf("{\"phase\":\"archive_selection\",\"status\":\"PASS\",\"inventory_rows\":%u,\"selected_row\":%u,\"regular_members\":1,\"member_bytes\":%" PRIu64
 ",\"member_sha256\":\"%s\",\"source_sha256\":\"%s\",\"inventory_sha256\":\"%s\",\"quarantine_name\":\"%s\",\"journal_count\":%u,\"journal_device\":%" PRIu64
 ",\"journal_inode\":%" PRIu64 ",\"journal_written_bytes\":%" PRIu64 ",\"journal_row_name_identity_hash_bound\":true,\"member_identity_derived_from_inventory\":true,\"preknown_member_identity_required\":false,\"format_validated\":false,\"platform_compatible\":false,\"installation_supported\":false,\"runtime_supported\":false}\n",
 inventory.row_count,candidate.row_index,candidate.member_bytes,member_hex,archive_hex,inventory_hex,owned->name,materialized.journal_count,owned->device,owned->inode,owned->written_bytes);
 /* Pass descriptor in-process; generated journal name is only a label. */
 size_t section_count=(size_t)(argc-13);char **prep=calloc(7+section_count,sizeof(*prep));if(!prep){rc=1;goto done;}
 prep[0]=(char *)"inprocess_config_pak";prep[1]=argv[1];prep[2]=argv[8];prep[3]=(char *)owned->name;prep[4]=argv[9];prep[5]=argv[10];prep[6]=argv[11];
 for(size_t i=0;i<section_count;++i)prep[7+i]=argv[13+i];
 rc=pm_prepare_config_pak_host((int)(7+section_count),prep,member_stream,&output_owner);free(prep);
 if(rc||fstat(archive_fd,&archive_after)||fstat(fileno(member_stream),&member_after)||
 !stable(&archive_before,&archive_after)||!stable(&member_before,&member_after)){rc=1;goto done;}
 puts("{\"phase\":\"composition_complete\",\"status\":\"PASS\",\"archive_and_member_snapshot_stable\":true,\"pre_extracted_input_used\":false,\"premerged_input_used\":false,\"external_preparation_helpers\":0}");
 rc=0;
done:
 if(rc)fprintf(stderr,"archive/config preparation refused status=%d errno=%d; quarantine journal retained\n",rc,errno);
 if(member_stream&&fclose(member_stream)&&!rc)rc=1;
 if(member_fd>=0&&close(member_fd)&&!rc)rc=1;
 if(qfd>=0&&close(qfd)&&!rc)rc=1;
 if(archive_fd>=0&&close(archive_fd)&&!rc)rc=1;
 /* Output path is private to this local job. Never remove a replacement or
  * any path the job did not create. Retained journal stays unpublished. */
 if(rc&&output_owner.created){struct stat current;
  if(!lstat(argv[11],&current)&&S_ISREG(current.st_mode)&&
   (uint64_t)current.st_dev==output_owner.device&&(uint64_t)current.st_ino==output_owner.inode){
   if(unlink(argv[11]))fprintf(stderr,"owned output cleanup failed errno=%d\n",errno);
  }else fprintf(stderr,"owned output identity changed; output path retained\n");
 }
 return rc;
}
