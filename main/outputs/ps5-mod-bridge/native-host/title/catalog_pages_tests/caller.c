#include "../catalog_test.h"
#include "../pages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Development fixture only: arbitrary roots and deterministic tokens cannot
 * enter the native executable. This caller composes the actual four modules. */
static char entry_name[MB_TITLE_CATALOG_ENTRY_NAME_BYTES];
static char candidate[MB_TITLE_CATALOG_CANDIDATE_BYTES];
static uint8_t workspace[MB_TITLE_CATALOG_SOURCE_WORKSPACE_BYTES];
static mb_title_metadata_result_v1 records[MB_TITLE_CATALOG_MAX_RECORDS];
static uint32_t issues[MB_TITLE_CATALOG_MAX_ISSUES];
static uint8_t backing[MB_TITLE_PAGES_BACKING_LIMIT];
static char keys[MB_TITLE_PAGES_RECORD_LIMIT][33];
static char cursors[MB_TITLE_PAGES_PAGE_LIMIT-1][33];
static unsigned checks;
#define REQUIRE(value) do { ++checks; if (!(value)) { \
    fprintf(stderr,"FAIL line %u: %s\n",(unsigned)__LINE__,#value); return 1; } } while (0)

static void token(char value[33],unsigned n) {
    (void)snprintf(value,33,"%032x",n);
}
static int zero_bytes(const uint8_t *bytes,size_t count) {
    size_t n;
    for(n=0;n<count;++n) if(bytes[n]) return 0;
    return 1;
}
static int write_wire(const char *directory,const char *name,const void *bytes,size_t count) {
    char path[4096]; FILE *file; int good;
    int length=snprintf(path,sizeof(path),"%s/%s",directory,name);
    if(length<0 || (size_t)length>=sizeof(path)) return 0;
    file=fopen(path,"wb"); if(!file) return 0;
    good=fwrite(bytes,1,count,file)==count && fflush(file)==0 && !ferror(file);
    if(fclose(file)!=0) good=0;
    return good;
}
int main(int argc,char **argv) {
    mb_title_catalog_request_v1 request={0};
    mb_title_catalog_ledger_v1 ledger={0};
    mb_title_catalog_result_v1 catalog={0};
    mb_title_pages_view_v1 view={0};
    mb_title_pages_plan_v1 plan={0};
    mb_title_pages_tokens_v1 tokens={0};
    mb_title_pages_cache_v1 cache={0};
    mb_title_catalog_status status;
    unsigned expected,expected_records,n,total=0;
    REQUIRE(argc==5);
    expected=(unsigned)strtoul(argv[3],NULL,10);
    expected_records=(unsigned)strtoul(argv[4],NULL,10);
    request.struct_size=sizeof(request); request.abi_version=1;
    request.profile=MB_TITLE_CACHE_SYSTEM_PRIVATE;
    request.scan_assertion=MB_TITLE_CATALOG_ASSERT_IMMUTABLE_SCAN;
    ledger.struct_size=sizeof(ledger); ledger.abi_version=1;
    ledger.source_io.struct_size=sizeof(ledger.source_io); ledger.source_io.abi_version=1;
    ledger.source_io.remaining_operations=MB_TITLE_CATALOG_OPERATION_LIMIT;
    ledger.source_io.remaining_read_bytes=MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT;
    status=mb_title_catalog_scan_fixture_v1(&request,&ledger,
        entry_name,sizeof(entry_name),candidate,sizeof(candidate),workspace,sizeof(workspace),
        records,MB_TITLE_CATALOG_MAX_RECORDS,issues,MB_TITLE_CATALOG_MAX_ISSUES,
        &catalog,argv[1],strlen(argv[1]),NULL,NULL);
    REQUIRE((unsigned)status==expected && catalog.status==expected);
    REQUIRE(catalog.struct_size==sizeof(catalog) && catalog.abi_version==1);
    REQUIRE(catalog.record_count==expected_records);
    REQUIRE(catalog.cleanup_complete==1);
    REQUIRE(catalog.operations_reserved<=MB_TITLE_CATALOG_OPERATION_LIMIT);
    REQUIRE(catalog.operations_attempted<=catalog.operations_reserved);
    REQUIRE(catalog.requested_read_bytes+catalog.entry_name_bytes<=MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT);
    REQUIRE(zero_bytes(workspace,sizeof(workspace)));
    for(n=0;n<catalog.record_count;++n) {
        const mb_title_metadata_result_v1 *r=&records[n];
        REQUIRE(r->status==MB_TITLE_METADATA_OK && r->metadata_complete==1);
        REQUIRE(!r->installed_observed && !r->running_observed && !r->build_fingerprint_available);
        REQUIRE(!r->engine_acceptance_verified && !r->runtime_supported && !r->installation_supported);
        REQUIRE(!r->authenticity_verified && !r->game_acceptance_verified);
    }
    view.struct_size=sizeof(view); view.abi_version=1; view.catalog=&catalog;
    view.records=records; view.records_capacity=MB_TITLE_CATALOG_MAX_RECORDS;
    view.issues=issues; view.issues_capacity=MB_TITLE_CATALOG_MAX_ISSUES;
    REQUIRE(mb_title_pages_measure_v1(&view,&plan)==MB_TITLE_PAGES_OK && plan.valid==1);
    if(status==MB_TITLE_CATALOG_STATUS_OBSERVED) {
        tokens.struct_size=sizeof(tokens); tokens.abi_version=1;
        tokens.title_count=catalog.record_count; tokens.cursor_count=plan.page_count-1;
        token(tokens.generation,1);
        for(n=0;n<tokens.title_count;++n) token(keys[n],n+2);
        for(n=0;n<tokens.cursor_count;++n) token(cursors[n],1000+n);
        tokens.title_keys=tokens.title_count?(const char (*)[33])keys:NULL;
        tokens.cursors=tokens.cursor_count?(const char (*)[33])cursors:NULL;
    }
    REQUIRE(mb_title_pages_prepare_v1(&view,status==MB_TITLE_CATALOG_STATUS_OBSERVED?&tokens:NULL,
        backing,sizeof(backing),&cache)==MB_TITLE_PAGES_OK && cache.prepared==1);
    REQUIRE(cache.installation_supported==0 && cache.build_fingerprint_available==0);
    for(n=0;n<cache.plan.page_count;++n) {
        const mb_title_pages_descriptor_v1 *page=&cache.plan.pages[n];
        char name[64]; (void)snprintf(name,sizeof(name),"integration-page-%03u.json",n);
        REQUIRE(page->byte_count>0 && page->byte_count<=MB_TITLE_PAGES_WIRE_LIMIT);
        REQUIRE(page->item_offset==total); total+=page->item_count;
        REQUIRE(write_wire(argv[2],name,backing+page->byte_offset,page->byte_count));
    }
    REQUIRE(total==(status==MB_TITLE_CATALOG_STATUS_OBSERVED?expected_records:0));
    if(cache.plan.current_bytes) REQUIRE(write_wire(argv[2],"integration-current.json",
        backing+cache.plan.current_offset,cache.plan.current_bytes));
    printf("PASS catalog -> pages checks=%u status=%u metadata=%u wire_rows=%u pages=%u\n",
        checks,(unsigned)status,catalog.record_count,total,cache.plan.page_count);
    return 0;
}
