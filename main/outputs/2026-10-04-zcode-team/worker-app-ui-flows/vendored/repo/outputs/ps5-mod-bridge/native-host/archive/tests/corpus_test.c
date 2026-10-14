#define _POSIX_C_SOURCE 200809L
#include "../intake.h"
#include "../groups.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>
static int failures;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);++failures;} } while(0)
static int unhex(const char *s,uint8_t out[32]) {
 unsigned i,x; if(strlen(s)!=64)return 0;
 for(i=0;i<32;i++){if(sscanf(s+2*i,"%2x",&x)!=1)return 0;out[i]=(uint8_t)x;}
 return 1;
}
static mb_archive_buffers_v1 buffers(void) {
 mb_archive_buffers_v1 b={0}; b.row_capacity=128;b.name_capacity=65536;
 b.sort_capacity=128;b.chunk_capacity=MB_ARCHIVE_CHUNK_BYTES;
 b.rows=calloc(b.row_capacity,sizeof(*b.rows));b.names=calloc(1,b.name_capacity);
 b.sort_indices=calloc(b.sort_capacity,sizeof(*b.sort_indices));
 b.chunk=calloc(1,b.chunk_capacity);if(!b.rows||!b.names||!b.sort_indices||!b.chunk)exit(3);return b;
}
static void release(mb_archive_buffers_v1 *b){free(b->rows);free(b->names);free(b->sort_indices);free(b->chunk);}
int main(int argc,char **argv) {
 mb_archive_source_v1 source={0};mb_archive_limits_v1 limits;
 mb_archive_budget_v1 budget;mb_archive_inventory_v1 inventory;
 mb_archive_materialized_v1 result;mb_archive_materialize_request_v1 request={0};
 mb_archive_buffers_v1 first=buffers(),second=buffers();
 mb_archive_owned_file_v1 journal[131]={0};uint32_t selected[128],nsel=0,nrows=0,group_sort[128]={0};
 unsigned expected_selection[128]={0},actual_selection[128]={0};
 mb_archive_group_v1 groups[128]={0};mb_archive_group_member_v1 members[128]={0};
 char key_bytes[65536]={0};mb_archive_groups_result_v1 grouping={0};
 char line[8192],digest[65],original[4097];uint64_t bytes;unsigned kind,choose;
 struct stat st; FILE *oracle;int status;uint8_t expected[32];
 if(argc!=4){fprintf(stderr,"usage: corpus_test ARCHIVE ORACLE QUARANTINE\n");return 3;}
 /* The embedding caller chooses its process locale. The adapter does not
  * change global locale or relax reader warnings to guess a pathname. */
 (void)setlocale(LC_CTYPE,"");
 source.struct_size=sizeof(source);source.abi_version=MB_ARCHIVE_ABI_V1;
 source.fd=open(argv[1],O_RDONLY|O_CLOEXEC);if(source.fd<0)return 3;
 oracle=fopen(argv[2],"r");if(!oracle)return 3;
 if(!fgets(line,sizeof(line),oracle)||sscanf(line,"%64s %"SCNu64,digest,&source.size)!=2||!unhex(digest,source.sha256))return 3;
 memset(source.target_token,0x51,sizeof(source.target_token));
 mb_archive_limits_default_v1(&limits);limits.entries=128;limits.name_bytes=65536;
 mb_archive_budget_default_v1(&budget);
 status=mb_archive_scan_v1(&source,&limits,&budget,&first,&inventory);
 CHECK(status==MB_ARCHIVE_OK);CHECK(inventory.complete==1);CHECK(inventory.installation_supported==0);
 while(fgets(line,sizeof(line),oracle)) {
  if(sscanf(line,"%u %u %"SCNu64" %64s %4096[^\n]",&choose,&kind,&bytes,digest,original)!=5||!unhex(digest,expected))return 3;
  if(nrows>=128)return 3;
  if(inventory.complete&&nrows<inventory.row_count){const mb_archive_row_v1 *r=first.rows+nrows;
   CHECK(r->kind==kind);CHECK(r->size==bytes);CHECK(memcmp(r->sha256,expected,32)==0);
   CHECK(r->original_length==strlen(original));
   CHECK(r->original_offset+r->original_length<first.name_capacity);
   CHECK(strcmp(first.names+r->original_offset,original)==0);
  }
  expected_selection[nrows]=choose;++nrows;
 }
 CHECK(inventory.row_count==nrows);fclose(oracle);
 if(failures)return 1;
 status=mb_archive_groups_v1(&inventory,first.rows,first.row_capacity,
 first.names,first.name_capacity,group_sort,128,groups,128,members,128,
 key_bytes,sizeof(key_bytes),&grouping);
 CHECK(status==MB_ARCHIVE_GROUPS_OK);CHECK(grouping.complete==1);
 CHECK(grouping.installation_supported==0);CHECK(grouping.runtime_supported==0);
 if(failures)return 1;
 if(grouping.group_count){
  for(uint32_t g=0;g<grouping.group_count;g++){
   const mb_archive_group_v1 *group=groups+g;
   if(group->status!=MB_ARCHIVE_GROUP_STATUS_CANDIDATE||!group->members_complete)continue;
   for(uint32_t m=group->member_begin;m<group->member_begin+group->member_count;m++){
    CHECK(m<grouping.member_count);if(m>=grouping.member_count)return 1;
    CHECK(members[m].row_index<inventory.row_count);
    if(members[m].row_index>=inventory.row_count)return 1;
    actual_selection[members[m].row_index]=1;
   }
  }
 }else{
  for(uint32_t i=0;i<inventory.row_count;i++)
   if(first.rows[i].kind==MB_ARCHIVE_REGULAR)actual_selection[i]=1;
 }
 for(uint32_t i=0;i<inventory.row_count;i++){
  CHECK(actual_selection[i]==expected_selection[i]);
  if(actual_selection[i])selected[nsel++]=i;
 }
 CHECK(nsel>0);if(failures)return 1;
 for(uint32_t g=0;g<grouping.group_count;g++)
  for(uint32_t m=groups[g].member_begin;m<groups[g].member_begin+groups[g].member_count;m++)
   printf("group=%u kind=%u candidate=%u row=%u role=%u selected=%u\n",g,groups[g].kind,
   groups[g].status==MB_ARCHIVE_GROUP_STATUS_CANDIDATE,members[m].row_index,
   members[m].role,actual_selection[members[m].row_index]);
 request.source=&source;request.inventory=&inventory;
 request.rows=first.rows;request.row_capacity=first.row_capacity;
 request.names=first.names;request.name_capacity=first.name_capacity;
 request.selected=selected;request.selected_count=nsel;
 memcpy(request.target_token,source.target_token,32);
 request.quarantine_fd=open(argv[3],O_RDONLY|O_DIRECTORY|O_CLOEXEC);
 if(request.quarantine_fd<0||fstat(request.quarantine_fd,&st))return 3;
 request.quarantine_device=(uint64_t)st.st_dev;request.quarantine_inode=(uint64_t)st.st_ino;
 request.journal=journal;request.journal_capacity=131;request.replay=second;
 status=mb_archive_materialize_v1(&request,&limits,&budget,&result);
 CHECK(status==MB_ARCHIVE_OK);CHECK(result.complete==1);CHECK(result.selected_count==nsel);
 CHECK(result.installation_supported==0);CHECK(result.cleanup_complete==1);
 CHECK(result.manifest_name[0]!=0);CHECK(memcmp(result.inventory_sha256,inventory.inventory_sha256,32)==0);
 printf("rows=%u selected=%u complete=%u status=%d io=%"PRIu64" operations=%"PRIu64" failures=%d\n",
 inventory.row_count,nsel,result.complete,status,budget.io_used,budget.operations_used,failures);
 close(source.fd);close(request.quarantine_fd);release(&first);release(&second);
 return failures?1:0;
}
