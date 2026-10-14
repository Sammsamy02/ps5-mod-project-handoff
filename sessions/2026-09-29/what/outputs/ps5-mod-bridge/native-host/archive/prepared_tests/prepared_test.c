#include "prepared.h"
#include <stdio.h>
#include <string.h>

#define CAP 16u
#define CHECK(x) do { ++checks; if(!(x)){++failures;fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);} } while(0)
static unsigned checks,failures,cases;
static mb_archive_inventory_v1 inv;
static mb_archive_row_v1 rows[CAP];
static char names[4096],keys[4096];
static uint32_t sort[CAP],selected[CAP];
static mb_archive_group_v1 groups[CAP];
static mb_archive_group_member_v1 members[CAP];
static mb_archive_prepared_file_v1 files[CAP],bound[CAP];
static mb_archive_prepared_result_v1 candidate,result;
static mb_archive_prepared_profile_v1 profile;
static mb_archive_prepared_view_v1 view;
static mb_archive_prepared_workspace_v1 workspace;
static mb_archive_owned_file_v1 journal[CAP+3];
static mb_archive_materialized_v1 materialized;
static mb_archive_prepared_materialization_v1 materialization;
static int zero(const void *p,size_t n){const unsigned char *b=p;while(n--)if(*b++)return 0;return 1;}
static void reset(void){
    ++cases;memset(&inv,0,sizeof(inv));memset(rows,0,sizeof(rows));memset(names,0,sizeof(names));
    inv.struct_size=sizeof(inv);inv.abi_version=1;inv.complete=1;inv.source_size=73;
    inv.source_device=5;inv.source_inode=42;
    memset(inv.source_sha256,0x11,32);memset(inv.inventory_sha256,0x22,32);memset(inv.target_token,0x33,32);
    memset(&profile,0,sizeof(profile));profile.struct_size=sizeof(profile);profile.abi_version=1;
    memset(profile.target_token,0x33,32);strcpy(profile.nonce,"00112233445566778899aabbccddeeff");
    strcpy(profile.prefix,"pakchunk9-");strcpy(profile.suffix,"_P");
    view=(mb_archive_prepared_view_v1){&inv,rows,CAP,names,sizeof(names)};
    workspace=(mb_archive_prepared_workspace_v1){sort,CAP,groups,CAP,members,CAP,keys,sizeof(keys)};
    memset(files,0xa5,sizeof(files));memset(bound,0xa5,sizeof(bound));memset(selected,0xa5,sizeof(selected));
    memset(&candidate,0xa5,sizeof(candidate));memset(&result,0xa5,sizeof(result));
}
static void add(const char *path,uint64_t bytes,unsigned hash){
    mb_archive_row_v1 *r=rows+inv.row_count++;
    size_t n=strlen(path);r->kind=MB_ARCHIVE_REGULAR;r->size=bytes;
    r->original_offset=r->path_offset=inv.name_bytes;r->original_length=r->path_length=(uint32_t)n;
    memcpy(names+inv.name_bytes,path,n+1);inv.name_bytes+=(uint32_t)n+1;
    inv.expanded_bytes+=bytes;memset(r->sha256,(int)hash,32);
}
static void trio(void){add("wrapper/Data.pak",11,0x44);add("wrapper/Data.ucas",23,0x55);add("wrapper/Data.utoc",29,0x66);add("Readme.txt",10,0x77);}
static int prepare(uint32_t cap){return mb_archive_prepare_groups_v1(&view,&profile,&workspace,selected,CAP,files,cap,&candidate);}
static void no_authority(const mb_archive_prepared_result_v1 *r){
    CHECK(r->installation_supported==0);CHECK(r->runtime_supported==0);
    CHECK(r->platform_compatible==0);CHECK(r->format_validated==0);
}
static void failed(const mb_archive_prepared_result_v1 *r,const mb_archive_prepared_file_v1 *f){
    CHECK(r->complete==0&&r->file_count==0&&r->group_count==0&&r->total_bytes==0);
    CHECK(zero(r->source_sha256,32)&&zero(r->inventory_sha256,32)&&zero(r->target_token,32));
    CHECK(zero(f,sizeof(files)));CHECK(zero(sort,sizeof(sort))&&zero(keys,sizeof(keys)));no_authority(r);
}
static void journal_record(unsigned i,const char *name,unsigned row,unsigned state,uint64_t bytes,unsigned hash,uint64_t inode){
    strcpy(journal[i].name,name);journal[i].row_index=row;journal[i].state=state;
    journal[i].expected_size=journal[i].written_bytes=bytes;journal[i].device=7;journal[i].inode=inode;
    memset(journal[i].sha256,(int)hash,32);
}
static void successful_materialization(void){
    memset(journal,0,sizeof(journal));memset(&materialized,0,sizeof(materialized));
    materialized.struct_size=sizeof(materialized);materialized.abi_version=1;materialized.complete=1;
    materialized.selected_count=3;materialized.journal_count=6;materialized.cleanup_complete=1;materialized.decoded_bytes=63;
    memcpy(materialized.inventory_sha256,inv.inventory_sha256,32);memcpy(materialized.target_token,inv.target_token,32);
    strcpy(materialized.manifest_name,"manifest-v1.bin");
    journal_record(0,"intent-v1.bin",UINT32_MAX,MB_ARCHIVE_CREATED,55,0x80,100);
    journal_record(1,"member-00000001.bin",1,MB_ARCHIVE_CREATED,23,0x55,101);
    journal_record(2,"member-00000000.bin",0,MB_ARCHIVE_CREATED,11,0x44,102);
    journal_record(3,"member-00000002.bin",2,MB_ARCHIVE_CREATED,29,0x66,103);
    journal_record(4,"manifest-v1.part",UINT32_MAX,MB_ARCHIVE_REMOVED,75,0x81,104);
    journal_record(5,"manifest-v1.bin",UINT32_MAX,MB_ARCHIVE_CREATED,75,0x81,104);
    materialization=(mb_archive_prepared_materialization_v1){&materialized,journal,CAP+3,7,99};
}
static int bind(void){return mb_archive_bind_prepared_v1(&view,&profile,&workspace,&candidate,files,CAP,&materialization,bound,CAP,&result);}
int main(void){
    reset();trio();CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_OK);
    CHECK(candidate.complete==1&&candidate.phase==MB_ARCHIVE_PREPARED_CANDIDATE&&candidate.group_count==1&&candidate.file_count==3);
    CHECK(candidate.total_bytes==63);CHECK(selected[0]==1&&selected[1]==0&&selected[2]==2);
    CHECK(files[0].row_index==1&&files[0].role==MB_ARCHIVE_GROUP_ROLE_UCAS&&files[0].bytes==23);
    CHECK(strcmp(files[0].source_name,"member-00000001.bin")==0);
    CHECK(strcmp(files[0].destination_name,"pakchunk9-00112233445566778899aabbccddeeff-00000000_P.ucas")==0);
    CHECK(strcmp(files[1].destination_name,"pakchunk9-00112233445566778899aabbccddeeff-00000000_P.pak")==0);
    CHECK(strcmp(files[2].destination_name,"pakchunk9-00112233445566778899aabbccddeeff-00000000_P.utoc")==0);
    CHECK(files[0].device==0&&files[0].inode==0);no_authority(&candidate);
    CHECK(zero(sort,sizeof(sort))&&zero(groups,sizeof(groups))&&zero(members,sizeof(members))&&zero(keys,sizeof(keys)));
    successful_materialization();CHECK(bind()==MB_ARCHIVE_PREPARED_OK);
    CHECK(result.complete==1&&result.phase==MB_ARCHIVE_PREPARED_MATERIALIZED&&result.file_count==3);
    CHECK(bound[0].device==7&&bound[0].inode==101&&bound[1].inode==102&&bound[2].inode==103);
    CHECK(strcmp(result.manifest_name,"manifest-v1.bin")==0&&result.quarantine_inode==99);no_authority(&result);

    /* A no-hardlink fallback publishes the same manifest bytes at a new
     * exclusive inode; both journal records must still bind to this q device. */
    reset();trio();CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_OK);successful_materialization();
    journal[5].inode=105;CHECK(bind()==MB_ARCHIVE_PREPARED_OK);
    CHECK(result.complete==1&&result.phase==MB_ARCHIVE_PREPARED_MATERIALIZED);
    CHECK(result.file_count==3&&journal[4].inode!=journal[5].inode);no_authority(&result);

    reset();trio();CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_OK);successful_materialization();
    journal[4].inode=0;CHECK(bind()==MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);failed(&result,bound);
    reset();trio();CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_OK);successful_materialization();
    journal[5].device++;journal[5].inode=105;
    CHECK(bind()==MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);failed(&result,bound);
    reset();trio();CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_OK);successful_materialization();
    journal[5].inode=105;journal[5].sha256[0]^=1;
    CHECK(bind()==MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);failed(&result,bound);

    reset();add("b/same.ucas",2,1);add("a/same.utoc",3,2);add("a/same.ucas",4,3);add("b/same.utoc",5,4);add("z/standalone.pak",6,5);
    CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_OK);CHECK(candidate.group_count==3&&candidate.file_count==5&&candidate.total_bytes==20);
    CHECK(selected[0]==2&&selected[1]==1&&selected[2]==0&&selected[3]==3&&selected[4]==4);
    CHECK(strcmp(files[2].destination_name,"pakchunk9-00112233445566778899aabbccddeeff-00000001_P.ucas")==0);
    CHECK(strcmp(files[4].destination_name,"pakchunk9-00112233445566778899aabbccddeeff-00000002_P.pak")==0);

    reset();add("Data.utoc",1,1);CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_UNRESOLVED);failed(&candidate,files);CHECK(zero(selected,sizeof(selected)));
    reset();trio();add("wrapper/Data_s1.ucas",1,1);CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_UNRESOLVED);failed(&candidate,files);
    reset();add("A.pak",1,1);add("a.pak",1,2);CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_UNRESOLVED);failed(&candidate,files);
    reset();trio();profile.target_token[0]^=1;CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);failed(&candidate,files);
    reset();trio();CHECK(prepare(2)==MB_ARCHIVE_PREPARED_LIMIT);CHECK(zero(files,2*sizeof(*files)));CHECK(candidate.complete==0&&candidate.file_count==0);
    reset();trio();workspace.sort_capacity=3;CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_LIMIT);CHECK(candidate.complete==0);
    reset();trio();strcpy(profile.prefix,"../");CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_BAD_ARGUMENT);CHECK(candidate.complete==0);
    reset();trio();profile.nonce[1]='X';CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_BAD_ARGUMENT);CHECK(candidate.complete==0);
    reset();trio();inv.abi_version=2;CHECK(prepare(CAP)==MB_ARCHIVE_PREPARED_BAD_ARGUMENT);CHECK(candidate.complete==0xa5a5a5a5u);
    reset();trio();{
        mb_archive_row_v1 saved[CAP];memcpy(saved,rows,sizeof(saved));
        CHECK(mb_archive_prepare_groups_v1(&view,&profile,&workspace,selected,CAP,(mb_archive_prepared_file_v1 *)rows,1,&candidate)==MB_ARCHIVE_PREPARED_BAD_ARGUMENT);
        CHECK(memcmp(saved,rows,sizeof(saved))==0);CHECK(candidate.complete==0xa5a5a5a5u);
    }
    reset();trio();CHECK(prepare(CAP)==0);successful_materialization();files[0].destination_name[0]='x';CHECK(bind()==MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);failed(&result,bound);
    reset();trio();CHECK(prepare(CAP)==0);successful_materialization();journal[2].sha256[0]^=1;CHECK(bind()==MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);failed(&result,bound);
    reset();trio();CHECK(prepare(CAP)==0);successful_materialization();journal[3].row_index=1;CHECK(bind()==MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);failed(&result,bound);
    reset();trio();CHECK(prepare(CAP)==0);successful_materialization();materialized.complete=0;CHECK(bind()==MB_ARCHIVE_PREPARED_INCOMPLETE);failed(&result,bound);
    reset();add("notes.txt",10,1);CHECK(prepare(CAP)==0);CHECK(candidate.complete==1&&candidate.group_count==0&&candidate.file_count==0);no_authority(&candidate);
    printf("cases=%u checks=%u failures=%u\n",cases,checks,failures);return failures?1:0;
}
