#include "../pages.h"
#include <stdio.h>
#include <string.h>

static mb_title_catalog_result_v1 catalog;
static mb_title_metadata_result_v1 records[MB_TITLE_PAGES_RECORD_LIMIT];
static uint32_t issues[MB_TITLE_PAGES_ISSUE_LIMIT];
static char keys[MB_TITLE_PAGES_RECORD_LIMIT][33];
static char cursors[MB_TITLE_PAGES_PAGE_LIMIT-1][33];
static uint8_t backing[MB_TITLE_PAGES_BACKING_LIMIT];
static mb_title_pages_plan_v1 plan;
static mb_title_pages_cache_v1 cache;
static mb_title_pages_view_v1 view;
static mb_title_pages_tokens_v1 tokens;
static unsigned checks,failures;

#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; \
    printf("FAIL line %u: %s\n",(unsigned)__LINE__,#expr); } } while (0)

static void hex32(char value[33],unsigned number) {
    static const char hex[]="0123456789abcdef";
    unsigned n;
    memset(value,'0',32); value[32]=0;
    for (n=32;n && number;number >>= 4) value[--n]=hex[number & 15u];
}
static int all_zero(const void *data,size_t size) {
    const unsigned char *p=data;
    size_t n;
    for (n=0;n<size;++n) if (p[n]) return 0;
    return 1;
}
static void initialize(unsigned count,unsigned profile) {
    unsigned n;
    memset(&catalog,0,sizeof(catalog)); memset(records,0,sizeof(records));
    memset(issues,0,sizeof(issues)); memset(&view,0,sizeof(view));
    memset(&tokens,0,sizeof(tokens)); memset(keys,0,sizeof(keys));
    memset(cursors,0,sizeof(cursors));
    catalog.struct_size=sizeof(catalog); catalog.abi_version=1;
    catalog.status=MB_TITLE_CATALOG_STATUS_OBSERVED;
    catalog.profile=profile; catalog.source_scope=profile;
    catalog.complete=1; catalog.record_count=count; catalog.entries_seen=count;
    catalog.candidates_seen=count;
    catalog.root_identity_checked=catalog.enumeration_eof=catalog.cleanup_complete=1;
    catalog.operations_attempted=7u+count*32u; catalog.operations_reserved=8u+count*48u;
    catalog.actual_read_bytes=count*1024u; catalog.requested_read_bytes=count*1025u;
    catalog.entry_name_bytes=count*16u;
    for (n=0;n<count;++n) {
        mb_title_metadata_result_v1 *r=&records[n];
        unsigned j;
        r->struct_size=sizeof(*r); r->abi_version=1;
        r->captured_bytes=1024; r->source_scope=MB_TITLE_SOURCE_METADATA_CAPTURE;
        r->metadata_identity_scope=MB_TITLE_IDENTITY_CAPTURED_BYTES_SHA256;
        r->metadata_complete=r->title_id_available=r->metadata_sha256_available=1;
        r->name_available=r->version_available=1;
        (void)snprintf(r->title_id,sizeof(r->title_id),"Any.Custom-%03u",n);
        if (n==0) {
            memcpy(r->name,"Caf\xc3\xa9 \xf0\x9f\xa7\xad \"quote\" \\ tail",sizeof("Caf\xc3\xa9 \xf0\x9f\xa7\xad \"quote\" \\ tail"));
            memcpy(r->version,"1.\xf0\x9f\x94\xa7",sizeof("1.\xf0\x9f\x94\xa7"));
        } else {
            for (j=0;j<128;++j) { r->name[j*2]=(char)0xc3; r->name[j*2+1]=(char)0xa9; }
            for (j=0;j<64;++j) { r->version[j*2]=(char)0xc2; r->version[j*2+1]=(char)0x81; }
        }
        memset(r->metadata_sha256,'a',64);
        hex32(keys[n],n+2);
    }
    for(n=0;n<MB_TITLE_PAGES_PAGE_LIMIT-1;++n) hex32(cursors[n],1000+n);
    view.struct_size=sizeof(view); view.abi_version=1; view.catalog=&catalog;
    view.records=records; view.records_capacity=MB_TITLE_PAGES_RECORD_LIMIT;
    view.issues=issues; view.issues_capacity=MB_TITLE_PAGES_ISSUE_LIMIT;
    tokens.struct_size=sizeof(tokens); tokens.abi_version=1; tokens.title_count=count;
    hex32(tokens.generation,1); tokens.title_keys=count?(const char (*)[33])keys:NULL;
}
static mb_title_pages_status prepare(size_t size) {
    mb_title_pages_status status=mb_title_pages_measure_v1(&view,&plan);
    if(status!=MB_TITLE_PAGES_OK) return status;
    tokens.cursor_count=plan.page_count?plan.page_count-1:0;
    if(!tokens.title_count) tokens.title_keys=NULL;
    tokens.cursors=tokens.cursor_count?(const char (*)[33])cursors:NULL;
    return mb_title_pages_prepare_v1(&view,&tokens,backing,size,&cache);
}
static void cleared(mb_title_pages_status status,unsigned expected,unsigned reason) {
    CHECK((unsigned)status==expected);
    CHECK(cache.prepared==0 && cache.status==expected && cache.reason==reason);
    CHECK(cache.plan.valid==0 && cache.snapshot_id[0]==0);
    CHECK(cache.installation_supported==0 && cache.build_fingerprint_available==0);
    CHECK(all_zero(backing,sizeof(backing)));
}
static void dirty(void) {
    memset(backing,0xa5,sizeof(backing)); memset(&cache,0xa5,sizeof(cache));
}
static int write_bytes(const char *base,const char *name,const void *data,size_t bytes) {
    char path[4096]; FILE *f; int good;
    if (snprintf(path,sizeof(path),"%s/%s",base,name)<0 || strlen(path)>=sizeof(path)-1) return 0;
    f=fopen(path,"wb"); if(!f) return 0;
    good=fwrite(data,1,bytes,f)==bytes && fflush(f)==0 && !ferror(f);
    if(fclose(f)!=0) good=0;
    return good;
}
static void emit(const char *base,const char *prefix) {
    unsigned n; char name[128];
    if(!base || !cache.prepared) return;
    for(n=0;n<cache.plan.page_count;++n) {
        const mb_title_pages_descriptor_v1 *d=&cache.plan.pages[n];
        (void)snprintf(name,sizeof(name),"%s-page-%03u.json",prefix,n);
        CHECK(write_bytes(base,name,backing+d->byte_offset,d->byte_count));
    }
    if(cache.plan.current_bytes) {
        (void)snprintf(name,sizeof(name),"%s-current.json",prefix);
        CHECK(write_bytes(base,name,backing+cache.plan.current_offset,cache.plan.current_bytes));
    }
    printf("FIXTURE %s pages=%u rows=%u logical=%llu backing=%llu\n",prefix,
        cache.plan.page_count,cache.plan.record_count,
        (unsigned long long)cache.plan.logical_capture_bytes,
        (unsigned long long)cache.plan.required_backing_bytes);
}
int main(int argc,char **argv) {
    const char *out=argc==2?argv[1]:NULL;
    mb_title_pages_status status;
    unsigned n,offset;
    size_t exact;
    mb_title_pages_cache_v1 saved;
    printf("GROUP complete generic128 Unicode canonical wire\n");
    initialize(128,MB_TITLE_CACHE_SYSTEM_PRIVATE);
    catalog.issue_count=32;
    for(n=0;n<32;++n) issues[n]=1+n%6;
    status=prepare(sizeof(backing));
    CHECK(status==MB_TITLE_PAGES_OK && cache.prepared==1 && plan.valid==1);
    CHECK(cache.plan.record_count==128 && cache.plan.page_count>16);
    CHECK(cache.plan.logical_capture_bytes<=MB_TITLE_PAGES_CAPTURE_LIMIT);
    CHECK(cache.plan.required_backing_bytes<=sizeof(backing));
    CHECK(cache.plan.current_bytes>0 && cache.plan.current_bytes<=128);
    CHECK(cache.plan.installation_supported==0 && cache.plan.build_fingerprint_available==0);
    offset=0;
    for(n=0;n<cache.plan.page_count && n<128;++n) {
        const mb_title_pages_descriptor_v1 *d=&cache.plan.pages[n];
        CHECK(d->byte_count>0 && d->byte_count<=8191 && d->item_count<=8);
        CHECK(d->item_offset==offset); offset+=d->item_count;
        CHECK(n==0?d->cursor[0]==0:memcmp(d->cursor,cursors[n-1],33)==0);
    }
    CHECK(offset==128);
    CHECK(cache.plan.current_offset+cache.plan.current_bytes==cache.plan.required_backing_bytes);
    emit(out,"generic128");
    exact=(size_t)cache.plan.required_backing_bytes; saved=cache;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,exact,&cache)==MB_TITLE_PAGES_OK);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0);
    if(exact) {
        memset(backing,0xa5,sizeof(backing));
        status=mb_title_pages_prepare_v1(&view,&tokens,backing,exact-1,&cache);
        CHECK(status==MB_TITLE_PAGES_RESOURCE_LIMIT && cache.prepared==0);
        CHECK(all_zero(backing,exact-1) && backing[exact-1]==0xa5);
    }
    printf("GROUP empty, optional projection, terminal hiding\n");
    initialize(0,MB_TITLE_CACHE_USER);
    CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK && cache.prepared==1);
    CHECK(cache.plan.page_count==1 && cache.plan.record_count==0);
    emit(out,"empty");
    initialize(1,MB_TITLE_CACHE_USER);
    memset(records[0].name,0,sizeof(records[0].name));
    memset(records[0].version,0,sizeof(records[0].version)); records[0].version_available=0;
    CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    emit(out,"optional-empty");
    initialize(1,MB_TITLE_CACHE_USER);
    memset(records[0].name,0,sizeof(records[0].name)); memcpy(records[0].name," Bad",5);
    memset(records[0].version,0,sizeof(records[0].version)); records[0].version[0]=127;
    CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    emit(out,"optional-error");
    initialize(1,MB_TITLE_CACHE_USER);
    memset(records[0].name,0,sizeof(records[0].name)); memcpy(records[0].name,"\xef\xbb\xbfOK\xc2\x81",8);
    memset(records[0].version,0,sizeof(records[0].version)); memcpy(records[0].version,"\xc2\x85" "Bad",6);
    CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    emit(out,"trim-policy");
    initialize(2,MB_TITLE_CACHE_SYSTEM_PRIVATE);
    catalog.status=MB_TITLE_CATALOG_STATUS_PARTIAL; catalog.complete=0;
    catalog.reason=MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE;
    catalog.issue_count=1; issues[0]=MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED;
    CHECK(mb_title_pages_prepare_v1(&view,NULL,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_OK);
    CHECK(cache.prepared==1 && cache.snapshot_id[0]==0 && cache.plan.record_count==0);
    CHECK(cache.plan.page_count==1 && cache.plan.current_bytes==0);
    emit(out,"partial");
    initialize(0,MB_TITLE_CACHE_USER);
    catalog.status=MB_TITLE_CATALOG_STATUS_UNAVAILABLE; catalog.complete=0;
    catalog.root_identity_checked=catalog.enumeration_eof=0;
    catalog.reason=MB_TITLE_CATALOG_REASON_PLATFORM_PROVIDER_UNAVAILABLE;
    CHECK(mb_title_pages_prepare_v1(&view,NULL,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_OK);
    emit(out,"unavailable");
    printf("GROUP late input failure clears whole output\n");
    initialize(128,MB_TITLE_CACHE_USER); dirty(); records[127].name[0]=(char)0xc0;
    status=mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache);
    cleared(status,MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    initialize(128,MB_TITLE_CACHE_USER); dirty(); records[127].name[0]=(char)0xed;
    records[127].name[1]=(char)0xa0; records[127].name[2]=(char)0x80;
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); records[1].name[256]=(char)0xc3;
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); records[1].title_id[64]='z';
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); records[1].installation_supported=1;
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); records[1].metadata_sha256[63]='G';
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); strcpy(records[1].title_id,records[0].title_id);
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_DUPLICATE_TITLE);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); strcpy(records[1].title_id,"any.custom-000");
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_DUPLICATE_TITLE);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); catalog.complete=0;
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_CATALOG_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); dirty(); catalog.issue_count=1; issues[0]=7;
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_ISSUE_INVALID);
    printf("GROUP bounded catalog and owning metadata controls\n");
    for(n=0;n<15;++n) {
        initialize(2,MB_TITLE_CACHE_USER); dirty();
        switch(n) {
        case 0: catalog.profile=3; break;
        case 1: catalog.source_scope=1; break;
        case 2: catalog.cleanup_complete=0; break;
        case 3: catalog.enumeration_eof=0; break;
        case 4: catalog.root_identity_checked=0; break;
        case 5: catalog.reason=1; break;
        case 6: catalog.record_count=129; break;
        case 7: catalog.issue_count=33; break;
        case 8: catalog.candidates_seen=3; break;
        case 9: catalog.entries_seen=257; break;
        case 10: catalog.operations_attempted=catalog.operations_reserved+1; break;
        case 11: catalog.operations_reserved=MB_TITLE_CATALOG_OPERATION_LIMIT+1; break;
        case 12: catalog.actual_read_bytes=catalog.requested_read_bytes+1; break;
        case 13: catalog.requested_read_bytes=MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT+1; break;
        case 14: catalog.entry_name_bytes=MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT; break;
        }
        cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
            MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_CATALOG_INVALID);
    }
    for(n=0;n<15;++n) {
        initialize(2,MB_TITLE_CACHE_USER); dirty();
        switch(n) {
        case 0: records[1].struct_size--; break;
        case 1: records[1].abi_version=2; break;
        case 2: records[1].status=MB_TITLE_METADATA_SCHEMA_ERROR; break;
        case 3: records[1].captured_bytes=0; break;
        case 4: records[1].captured_bytes=65537; break;
        case 5: records[1].source_scope=2; break;
        case 6: records[1].metadata_complete=0; break;
        case 7: records[1].title_id_available=0; break;
        case 8: records[1].name_available=0; break;
        case 9: records[1].version_available=2; break;
        case 10: records[1].metadata_sha256_available=0; break;
        case 11: records[1].reserved[1]=1; break;
        case 12: records[1].build_fingerprint_available=1; break;
        case 13: records[1].title_id[0]='.'; break;
        case 14: records[1].locale[0]=(char)0xf4; records[1].locale[1]=(char)0x90;
            records[1].locale[2]=(char)0x80; records[1].locale[3]=(char)0x80; break;
        }
        cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
            MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    }
    initialize(2,MB_TITLE_CACHE_USER); dirty(); catalog.status=MB_TITLE_CATALOG_STATUS_PARTIAL;
    catalog.complete=0; catalog.reason=MB_TITLE_CATALOG_REASON_SOURCE_CHANGED;
    records[1].name[0]=(char)0x80;
    cleared(mb_title_pages_prepare_v1(&view,NULL,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_CATALOG,MB_TITLE_PAGES_RECORD_INVALID);
    printf("GROUP token uniqueness, canonical form, count\n");
    initialize(2,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); memcpy(keys[1],keys[0],33);
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_DUPLICATE);
    initialize(2,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); memcpy(keys[0],tokens.generation,33);
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_DUPLICATE);
    initialize(128,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); CHECK(tokens.cursor_count>0);
    if(tokens.cursor_count) memcpy(cursors[tokens.cursor_count-1],keys[127],33);
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_DUPLICATE);
    initialize(2,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); keys[0][0]='A';
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_INVALID);
    initialize(128,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); memcpy(cursors[tokens.cursor_count-1],cursors[0],33);
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_DUPLICATE);
    initialize(128,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); memcpy(cursors[0],tokens.generation,33);
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_DUPLICATE);
    initialize(2,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); keys[1][32]='x';
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); dirty();
    cleared(mb_title_pages_prepare_v1(&view,NULL,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_INVALID);
    initialize(0,MB_TITLE_CACHE_USER); dirty(); catalog.status=MB_TITLE_CATALOG_STATUS_UNAVAILABLE;
    catalog.complete=0; catalog.reason=MB_TITLE_CATALOG_REASON_PLATFORM_PROVIDER_UNAVAILABLE;
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_INVALID);
    initialize(2,MB_TITLE_CACHE_USER); CHECK(prepare(sizeof(backing))==MB_TITLE_PAGES_OK);
    dirty(); tokens.title_count=1;
    cleared(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache),
        MB_TITLE_PAGES_BAD_TOKEN,MB_TITLE_PAGES_TOKEN_INVALID);
    printf("GROUP invalid preflight/known aliases do not touch outputs\n");
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache; view.reserved[0]=1;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0 && backing[0]==0xa5);
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,(uint8_t *)&cache,sizeof(cache),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0);
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing)+1,&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0 && backing[0]==0xa5);
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache; view.records_capacity=129;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0 && backing[0]==0xa5);
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache;
    view.catalog=(const mb_title_catalog_result_v1 *)(const void *)((const uint8_t *)&catalog+1);
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0 && backing[0]==0xa5);
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache;
    view.issues=(const uint32_t *)(const void *)records;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0 && records[0].title_id[0]=='A');
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,(uint8_t *)records,sizeof(records),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0 && records[0].title_id[0]=='A');
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache;
    tokens.title_keys=(const char (*)[33])&tokens.generation;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0 && backing[0]==0xa5);
    initialize(1,MB_TITLE_CACHE_USER); dirty(); saved=cache; tokens.reserved[0]=1;
    CHECK(mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(memcmp(&cache,&saved,sizeof(cache))==0);
    initialize(1,MB_TITLE_CACHE_USER); memset(&plan,0xa5,sizeof(plan));
    CHECK(mb_title_pages_measure_v1(&view,(mb_title_pages_plan_v1 *)(void *)records)==MB_TITLE_PAGES_BAD_ARGUMENT);
    CHECK(records[0].title_id[0]=='A');
    printf("RESULT %u checks %u failures\n",checks,failures);
    return failures?1:0;
}
