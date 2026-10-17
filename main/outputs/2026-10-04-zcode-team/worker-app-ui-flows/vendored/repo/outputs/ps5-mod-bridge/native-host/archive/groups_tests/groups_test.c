#include "groups.h"
#include "corpus_generated.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROWS 128u
#define KEYS 16384u
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); } } while (0)

static unsigned checks, failures, cases;
static mb_archive_inventory_v1 inv;
static mb_archive_row_v1 row_store[ROWS];
static char names_store[KEYS];
static uint32_t sort_store[ROWS];
static mb_archive_group_v1 group_store[ROWS];
static mb_archive_group_member_v1 member_store[ROWS];
static char key_store[KEYS];
static mb_archive_groups_result_v1 result_store;

static void setup(void) {
    memset(&inv,0,sizeof(inv)); inv.struct_size=sizeof(inv); inv.abi_version=MB_ARCHIVE_ABI_V1;
    inv.status=MB_ARCHIVE_OK; inv.complete=1; inv.installation_supported=0;
    memset(inv.source_sha256,0x11,sizeof(inv.source_sha256));
    memset(inv.inventory_sha256,0x22,sizeof(inv.inventory_sha256));
    memset(inv.target_token,0x33,sizeof(inv.target_token));
    memset(row_store,0,sizeof(row_store)); memset(names_store,0,sizeof(names_store)); inv.name_bytes=0;
    memset(sort_store,0xa5,sizeof(sort_store)); memset(group_store,0xa5,sizeof(group_store));
    memset(member_store,0xa5,sizeof(member_store)); memset(key_store,0xa5,sizeof(key_store));
    memset(&result_store,0xa5,sizeof(result_store)); inv.row_count=0;
}

static uint32_t add(const char *path,uint32_t kind) {
    const size_t length=strlen(path); const uint32_t i=inv.row_count++;
    if (i>=ROWS || length>UINT32_MAX-inv.name_bytes || inv.name_bytes+length>KEYS) return UINT32_MAX;
    row_store[i].kind=kind; row_store[i].path_offset=inv.name_bytes;
    row_store[i].path_length=(uint32_t)length; memcpy(names_store+inv.name_bytes,path,length);
    inv.name_bytes+=(uint32_t)length; return i;
}

static int run(uint32_t gc,uint32_t mc,uint32_t kc) {
    return mb_archive_groups_v1(&inv,row_store,ROWS,names_store,KEYS,sort_store,ROWS,
        group_store,gc,member_store,mc,key_store,kc,&result_store);
}

static int span_equals(uint32_t offset,uint32_t length,const char *expected) {
    return length==strlen(expected)&&memcmp(key_store+offset,expected,length)==0;
}

static const mb_archive_group_v1 *find_group(const char *parent,const char *stem) {
    uint32_t i;
    for(i=0;i<result_store.group_count;++i)
        if(span_equals(group_store[i].parent_offset,group_store[i].parent_length,parent)&&
           span_equals(group_store[i].stem_offset,group_store[i].stem_length,stem)) return &group_store[i];
    return NULL;
}

static int all_zero(const void *p,size_t n) {
    const unsigned char *b=(const unsigned char *)p; size_t i;
    for(i=0;i<n;++i) if(b[i]) return 0;
    return 1;
}

static int all_byte(const void *p,size_t n,unsigned char value) {
    const unsigned char *b=(const unsigned char *)p; size_t i;
    for(i=0;i<n;++i) if(b[i]!=value) return 0;
    return 1;
}

static void success_result(uint32_t groups,uint32_t members) {
    CHECK(result_store.struct_size==sizeof(result_store));
    CHECK(result_store.abi_version==MB_ARCHIVE_GROUPS_ABI_V1);
    CHECK(result_store.status==MB_ARCHIVE_GROUPS_OK&&result_store.complete==1);
    CHECK(result_store.group_count==groups&&result_store.member_count==members);
    CHECK(result_store.installation_supported==0&&result_store.runtime_supported==0&&result_store.format_validated==0);
    CHECK(memcmp(result_store.source_sha256,inv.source_sha256,32)==0);
    CHECK(memcmp(result_store.inventory_sha256,inv.inventory_sha256,32)==0);
    CHECK(memcmp(result_store.target_token,inv.target_token,32)==0);
}

static void one_pak_and_empty_inventory(void) {
    ++cases; setup();
    CHECK(add("Only/Readme.txt",MB_ARCHIVE_REGULAR)!=UINT32_MAX);
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_OK); success_result(0,0);
    setup(); CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_OK); success_result(0,0);
    CHECK(result_store.key_bytes==0);
}

static void exact_key_wrappers_and_roles(void) {
    ++cases; setup();
    const uint32_t pak=add("Mods/One/Bundle.PAK",MB_ARCHIVE_REGULAR);
    const uint32_t toc=add("Mods/One/Bundle.uToC",MB_ARCHIVE_REGULAR);
    const uint32_t cas=add("Mods/One/Bundle.ucas",MB_ARCHIVE_REGULAR);
    add("Mods/Two/Bundle.pak",MB_ARCHIVE_REGULAR);
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_OK); success_result(2,4);
    CHECK(span_equals(group_store[0].parent_offset,group_store[0].parent_length,"Mods/One"));
    CHECK(span_equals(group_store[0].stem_offset,group_store[0].stem_length,"Bundle"));
    CHECK(group_store[0].kind==MB_ARCHIVE_GROUP_IOSTORE&&group_store[0].members_complete==1);
    CHECK(group_store[0].status==MB_ARCHIVE_GROUP_STATUS_CANDIDATE);
    CHECK(group_store[0].reason_flags==0&&group_store[0].validation_unverified==1);
    CHECK(group_store[0].installation_supported==0&&group_store[0].runtime_supported==0&&group_store[0].format_validated==0);
    CHECK(member_store[group_store[0].member_begin+0].role==MB_ARCHIVE_GROUP_ROLE_PAK);
    CHECK(member_store[group_store[0].member_begin+0].row_index==pak);
    CHECK(member_store[group_store[0].member_begin+1].role==MB_ARCHIVE_GROUP_ROLE_UCAS);
    CHECK(member_store[group_store[0].member_begin+1].row_index==cas);
    CHECK(member_store[group_store[0].member_begin+2].role==MB_ARCHIVE_GROUP_ROLE_UTOC);
    CHECK(member_store[group_store[0].member_begin+2].row_index==toc);
    CHECK(group_store[1].kind==MB_ARCHIVE_GROUP_PAK&&group_store[1].members_complete==1);
}

static void missing_case_and_partition_rules(void) {
    ++cases; setup();
    add("lost.uToC",MB_ARCHIVE_REGULAR); add("lonely.ucas",MB_ARCHIVE_REGULAR);
    add("ORBIT.utoc",MB_ARCHIVE_REGULAR); add("orbit.ucas",MB_ARCHIVE_REGULAR);
    add("shared.utoc",MB_ARCHIVE_REGULAR); add("shared.ucas",MB_ARCHIVE_REGULAR);
    add("shared_s12.ucas",MB_ARCHIVE_REGULAR); add("shared_s1.pak",MB_ARCHIVE_REGULAR);
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_OK); success_result(7,8);
    const mb_archive_group_v1 *g=find_group("","lost");
    CHECK(g&&g->kind==MB_ARCHIVE_GROUP_IOSTORE&&g->members_complete==0);
    CHECK(g&&g->status==MB_ARCHIVE_GROUP_STATUS_UNRESOLVED);
    CHECK(g&&(g->reason_flags&MB_ARCHIVE_GROUP_REASON_MEMBER_MISSING));
    g=find_group("","lonely"); CHECK(g&&(g->reason_flags&MB_ARCHIVE_GROUP_REASON_MEMBER_MISSING));
    g=find_group("","ORBIT"); CHECK(g&&(g->reason_flags&MB_ARCHIVE_GROUP_REASON_CASE_AMBIGUITY));
    g=find_group("","orbit"); CHECK(g&&(g->reason_flags&MB_ARCHIVE_GROUP_REASON_CASE_AMBIGUITY));
    g=find_group("","shared"); CHECK(g&&(g->reason_flags&MB_ARCHIVE_GROUP_REASON_PARTITION_UNRESOLVED));
    g=find_group("","shared_s12"); CHECK(g&&(g->reason_flags&MB_ARCHIVE_GROUP_REASON_PARTITION_UNRESOLVED));
    g=find_group("","shared_s1"); CHECK(g&&g->kind==MB_ARCHIVE_GROUP_PAK&&g->members_complete==1);
}

static void duplicate_role_fails_and_clears(void) {
    ++cases; setup(); add("base.pak",MB_ARCHIVE_REGULAR); add("base.PAK",MB_ARCHIVE_REGULAR);
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_DUPLICATE_ROLE);
    CHECK(result_store.status==MB_ARCHIVE_GROUPS_DUPLICATE_ROLE&&result_store.complete==0);
    CHECK(result_store.group_count==0&&result_store.member_count==0&&result_store.key_bytes==0);
    CHECK(all_zero(group_store,sizeof(group_store))&&all_zero(member_store,sizeof(member_store)));
    CHECK(all_zero(key_store,sizeof(key_store))&&all_zero(sort_store,sizeof(sort_store)));
}

static void only_regular_rows_and_exact_order(void) {
    ++cases; setup(); add("folder/z.pak",MB_ARCHIVE_DIRECTORY); add("folder/a.pak",MB_ARCHIVE_REGULAR);
    add("folder/a.txt",MB_ARCHIVE_REGULAR); add("folder/B.PAK",MB_ARCHIVE_REGULAR);
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_OK); success_result(2,2);
    CHECK(span_equals(group_store[0].stem_offset,group_store[0].stem_length,"B"));
    CHECK(span_equals(group_store[1].stem_offset,group_store[1].stem_length,"a"));
}

static void incomplete_and_invalid_rows_fail_closed(void) {
    ++cases; setup(); add("base.pak",MB_ARCHIVE_REGULAR); inv.complete=0;
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_INCOMPLETE_INVENTORY);
    CHECK(result_store.status==MB_ARCHIVE_GROUPS_INCOMPLETE_INVENTORY&&result_store.complete==0);
    CHECK(result_store.group_count==0&&all_zero(group_store,sizeof(group_store))&&all_zero(key_store,sizeof(key_store)));
    setup(); add("base.pak",MB_ARCHIVE_REGULAR); row_store[0].path_offset=UINT32_MAX;
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_BAD_ARGUMENT);
    CHECK(all_byte(group_store,sizeof(group_store),0xa5)&&all_byte(key_store,sizeof(key_store),0xa5));
    CHECK(all_byte(&result_store,sizeof(result_store),0xa5)&&sort_store[0]==0xa5a5a5a5u);
}

static void late_capacity_failure_clears_full_backing(void) {
    ++cases; setup(); add("base.pak",MB_ARCHIVE_REGULAR);
    CHECK(run(ROWS,0,KEYS)==MB_ARCHIVE_GROUPS_LIMIT);
    CHECK(result_store.status==MB_ARCHIVE_GROUPS_LIMIT&&result_store.complete==0);
    CHECK(all_zero(group_store,sizeof(group_store)));
    CHECK(all_zero(key_store,sizeof(key_store))&&all_zero(sort_store,sizeof(sort_store)));
}

static void preflight_alias_is_no_write(void) {
    ++cases; setup(); add("base.pak",MB_ARCHIVE_REGULAR);
    union { max_align_t align; unsigned char bytes[sizeof(mb_archive_group_v1)*ROWS]; } shared;
    memset(shared.bytes,0xa5,sizeof(shared.bytes));
    unsigned char saved[sizeof(shared.bytes)]; memcpy(saved,shared.bytes,sizeof(saved));
    mb_archive_groups_result_v1 saved_result; memset(&saved_result,0x5a,sizeof(saved_result));
    result_store=saved_result;
    const int status=mb_archive_groups_v1(&inv,row_store,ROWS,names_store,KEYS,sort_store,ROWS,
        (mb_archive_group_v1 *)shared.bytes,ROWS,(mb_archive_group_member_v1 *)shared.bytes,ROWS,
        key_store,KEYS,&result_store);
    CHECK(status==MB_ARCHIVE_GROUPS_BAD_ARGUMENT);
    CHECK(memcmp(shared.bytes,saved,sizeof(saved))==0);
    CHECK(memcmp(&result_store,&saved_result,sizeof(saved_result))==0);
    CHECK(sort_store[0]==0xa5a5a5a5u);
}

static void host_leon_receipt_four_rows(void) {
    ++cases; setup();
    const char *paths[]={"README_v1.1_2K.txt","zzz_Leon_Townfall_2K_P.pak",
        "zzz_Leon_Townfall_2K_P.ucas","zzz_Leon_Townfall_2K_P.utoc"};
    const uint64_t sizes[]={4927,347,185446608,50364}; uint32_t i;
    for(i=0;i<4;++i){uint32_t r=add(paths[i],MB_ARCHIVE_REGULAR);row_store[r].size=sizes[i];}
    static const uint8_t archive_sha[32]={0x66,0x1a,0xab,0xf0,0xe6,0xa1,0xcb,0x91,0xe9,0x16,0x45,0x01,0x10,0x8b,0x5d,0x19,
      0xdb,0x13,0xc2,0xca,0x21,0xce,0xe6,0x51,0xf6,0x76,0xe0,0x70,0x6f,0x3f,0xee,0xf9};
    memcpy(inv.source_sha256,archive_sha,sizeof(archive_sha));
    CHECK(run(ROWS,ROWS,KEYS)==MB_ARCHIVE_GROUPS_OK); success_result(1,3);
    const mb_archive_group_v1 *g=find_group("","zzz_Leon_Townfall_2K_P");
    CHECK(g&&g->kind==MB_ARCHIVE_GROUP_IOSTORE&&g->members_complete==1&&g->reason_flags==0);
    CHECK(g&&g->status==MB_ARCHIVE_GROUP_STATUS_CANDIDATE);
    CHECK(g&&member_store[g->member_begin].row_index==1&&member_store[g->member_begin].role==MB_ARCHIVE_GROUP_ROLE_PAK);
    CHECK(g&&member_store[g->member_begin+1].row_index==2&&member_store[g->member_begin+1].role==MB_ARCHIVE_GROUP_ROLE_UCAS);
    CHECK(g&&member_store[g->member_begin+2].row_index==3&&member_store[g->member_begin+2].role==MB_ARCHIVE_GROUP_ROLE_UTOC);
    CHECK(result_store.path_work_used>0&&result_store.path_work_used<=MB_ARCHIVE_GROUPS_MAX_PATH_WORK);
}

static uint8_t hex_nibble(char c) {
    if(c>='0'&&c<='9')return (uint8_t)(c-'0');
    if(c>='a'&&c<='f')return (uint8_t)(c-'a'+10);
    if(c>='A'&&c<='F')return (uint8_t)(c-'A'+10);
    return 0xff;
}

static const char *role_name(uint32_t role) {
    switch(role) {
        case MB_ARCHIVE_GROUP_ROLE_PAK:return "pak";
        case MB_ARCHIVE_GROUP_ROLE_UCAS:return "ucas";
        case MB_ARCHIVE_GROUP_ROLE_UTOC:return "utoc";
        default:return "invalid";
    }
}

static void actual_receipt_corpus(void) {
    uint32_t fixture_index;
    for(fixture_index=0;fixture_index<GROUPS_CORPUS_COUNT;++fixture_index) {
        const groups_corpus_fixture *fixture=&groups_corpus[fixture_index];
        uint32_t i; uint8_t hash[32];
        ++cases; setup();
        if(strlen(fixture->archive_sha256)!=64) { CHECK(0); continue; }
        for(i=0;i<32;++i) {
            const uint8_t hi=hex_nibble(fixture->archive_sha256[i*2]);
            const uint8_t lo=hex_nibble(fixture->archive_sha256[i*2+1]);
            if(hi>15||lo>15)break;
            hash[i]=(uint8_t)((hi<<4)|lo);
        }
        CHECK(i==32);
        if(i!=32)continue;
        memcpy(inv.source_sha256,hash,sizeof(hash));
        for(i=0;i<fixture->row_count;++i) {
            const groups_corpus_row *src=&fixture->rows[i];
            const uint32_t row_index=add(src->path,src->kind);
            CHECK(row_index==i);
            if(row_index!=UINT32_MAX)row_store[row_index].size=src->size;
        }
        const int status=run(ROWS,ROWS,KEYS);
        CHECK(status==MB_ARCHIVE_GROUPS_OK);
        success_result(result_store.group_count,result_store.member_count);
        printf("CASE\t%s\t%u\n",fixture->name,result_store.group_count);
        for(i=0;i<result_store.group_count;++i) {
            const mb_archive_group_v1 *g=&group_store[i];
            const char *kind=g->kind==MB_ARCHIVE_GROUP_IOSTORE?"unreal_iostore":"unreal_pak";
            printf("GROUP\t%s\t",fixture->name);
            fwrite(key_store+g->parent_offset,1,g->parent_length,stdout); putchar('\t');
            fwrite(key_store+g->stem_offset,1,g->stem_length,stdout);
            printf("\t%s\t%u\t%u\t%u\n",kind,g->status,g->reason_flags,g->members_complete);
            uint32_t m;
            for(m=0;m<g->member_count;++m) {
                const mb_archive_group_member_v1 *member=&member_store[g->member_begin+m];
                const groups_corpus_row *row=&fixture->rows[member->row_index];
                printf("MEMBER\t%s\t%s\t%u\t%s\n",fixture->name,
                    role_name(member->role),member->row_index,row->path);
            }
        }
        success_result(result_store.group_count,result_store.member_count);
    }
}

int main(void) {
    one_pak_and_empty_inventory(); exact_key_wrappers_and_roles(); missing_case_and_partition_rules();
    duplicate_role_fails_and_clears(); only_regular_rows_and_exact_order();
    incomplete_and_invalid_rows_fail_closed(); late_capacity_failure_clears_full_backing();
    preflight_alias_is_no_write(); host_leon_receipt_four_rows(); actual_receipt_corpus();
    printf("archive-groups cases=%u checks=%u failures=%u runtime_supported=false installation_supported=false\n",cases,checks,failures);
    return failures?1:0;
}
