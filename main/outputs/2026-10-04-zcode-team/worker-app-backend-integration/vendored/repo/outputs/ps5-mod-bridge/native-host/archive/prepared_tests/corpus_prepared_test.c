#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#include "prepared.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CAP 128u
static unsigned checks,failures;
#define CHECK(x) do {++checks;if(!(x)){++failures;fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);}} while(0)
static mb_archive_buffers_v1 buffers(void){
    mb_archive_buffers_v1 b={0};b.row_capacity=b.sort_capacity=CAP;b.name_capacity=65536;b.chunk_capacity=65536;
    b.rows=calloc(CAP,sizeof(*b.rows));b.sort_indices=calloc(CAP,sizeof(*b.sort_indices));
    b.names=calloc(1,b.name_capacity);b.chunk=calloc(1,b.chunk_capacity);
    if(!b.rows||!b.sort_indices||!b.names||!b.chunk)exit(3);return b;
}
static void release(mb_archive_buffers_v1 *b){free(b->rows);free(b->sort_indices);free(b->names);free(b->chunk);}
static int unhex(const char *s,uint8_t hash[32]){
    unsigned i,x;if(strlen(s)!=64)return 0;
    for(i=0;i<32;i++){if(sscanf(s+2*i,"%2x",&x)!=1)return 0;hash[i]=(uint8_t)x;}return 1;
}
int main(int argc,char **argv){
    mb_archive_buffers_v1 first=buffers(),replay=buffers();mb_archive_source_v1 source={0};
    mb_archive_limits_v1 limits;mb_archive_budget_v1 budget;mb_archive_inventory_v1 inventory;
    mb_archive_group_v1 groups[CAP];mb_archive_group_member_v1 members[CAP];char keys[65536];uint32_t sort[CAP],selected[CAP];
    mb_archive_prepared_workspace_v1 workspace={sort,CAP,groups,CAP,members,CAP,keys,sizeof(keys)};
    mb_archive_prepared_view_v1 view;mb_archive_prepared_profile_v1 profile={0};
    mb_archive_prepared_file_v1 candidates[CAP],prepared[CAP];mb_archive_prepared_result_v1 candidate,result;
    mb_archive_owned_file_v1 journal[CAP+3]={0};mb_archive_materialized_v1 materialized;
    mb_archive_materialize_request_v1 request={0};mb_archive_prepared_materialization_v1 binding;
    struct stat st;int qfd,e;
    if(argc!=4||!unhex(argv[2],source.sha256))return 3;
    source.struct_size=sizeof(source);source.abi_version=1;source.fd=open(argv[1],O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(source.fd<0||fstat(source.fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0)return 3;
    source.size=(uint64_t)st.st_size;memset(source.target_token,0x51,32);
    mb_archive_limits_default_v1(&limits);limits.entries=CAP;limits.name_bytes=first.name_capacity;mb_archive_budget_default_v1(&budget);
    e=mb_archive_scan_v1(&source,&limits,&budget,&first,&inventory);CHECK(e==0);if(e)return 1;
    view=(mb_archive_prepared_view_v1){&inventory,first.rows,CAP,first.names,first.name_capacity};
    profile.struct_size=sizeof(profile);profile.abi_version=1;memcpy(profile.target_token,source.target_token,32);
    strcpy(profile.nonce,"00112233445566778899aabbccddeeff");strcpy(profile.prefix,"pakchunk9-ModBridge_");strcpy(profile.suffix,"_P");
    e=mb_archive_prepare_groups_v1(&view,&profile,&workspace,selected,CAP,candidates,CAP,&candidate);
    CHECK(e==0);CHECK(candidate.complete==1&&candidate.file_count>0);if(e||!candidate.file_count)return 1;
    qfd=open(argv[3],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(qfd<0||fstat(qfd,&st))return 3;
    request.source=&source;request.inventory=&inventory;request.rows=first.rows;request.row_capacity=CAP;
    request.names=first.names;request.name_capacity=first.name_capacity;request.selected=selected;request.selected_count=candidate.file_count;
    memcpy(request.target_token,source.target_token,32);request.quarantine_fd=qfd;
    request.quarantine_device=(uint64_t)st.st_dev;request.quarantine_inode=(uint64_t)st.st_ino;
    request.journal=journal;request.journal_capacity=CAP+3;request.replay=replay;
    e=mb_archive_materialize_v1(&request,&limits,&budget,&materialized);CHECK(e==0);if(e)return 1;
    binding=(mb_archive_prepared_materialization_v1){&materialized,journal,CAP+3,request.quarantine_device,request.quarantine_inode};
    e=mb_archive_bind_prepared_v1(&view,&profile,&workspace,&candidate,candidates,CAP,&binding,prepared,CAP,&result);
    CHECK(e==0);CHECK(result.complete==1&&result.phase==MB_ARCHIVE_PREPARED_MATERIALIZED);if(e)return 1;
    CHECK(result.file_count==candidate.file_count&&result.total_bytes==candidate.total_bytes);
    CHECK(result.installation_supported==0&&result.runtime_supported==0&&result.platform_compatible==0&&result.format_validated==0);
    for(uint32_t i=0;i<result.file_count;i++){
        const mb_archive_prepared_file_v1 *f=prepared+i;
        printf("FILE\t%u\t%u\t%"PRIu64"\t",f->row_index,f->role,f->bytes);
        for(unsigned j=0;j<32;j++)printf("%02x",f->sha256[j]);
        printf("\t%s\t%s\t%"PRIu64"\t%"PRIu64"\n",f->source_name,f->destination_name,f->device,f->inode);
    }
    printf("SUMMARY rows=%u groups=%u files=%u bytes=%"PRIu64" io=%"PRIu64" operations=%"PRIu64" checks=%u failures=%u\n",
        inventory.row_count,result.group_count,result.file_count,result.total_bytes,budget.io_used,budget.operations_used,checks,failures);
    CHECK(close(source.fd)==0);CHECK(close(qfd)==0);release(&first);release(&replay);return failures?1:0;
}
