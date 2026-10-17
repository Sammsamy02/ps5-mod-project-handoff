#include "../pages.h"
#include <string.h>

static mb_title_catalog_result_v1 catalog;
static mb_title_metadata_result_v1 record;
static mb_title_pages_view_v1 view;
static mb_title_pages_tokens_v1 tokens;
static mb_title_pages_cache_v1 cache;
static uint8_t backing[8191+128];
static const char key[1][33]={"00000000000000000000000000000002"};

int main(void) {
    catalog.struct_size=sizeof(catalog); catalog.abi_version=1;
    catalog.status=MB_TITLE_CATALOG_STATUS_OBSERVED;
    catalog.profile=MB_TITLE_CACHE_USER; catalog.source_scope=1;
    catalog.complete=catalog.record_count=catalog.entries_seen=catalog.candidates_seen=1;
    catalog.root_identity_checked=catalog.enumeration_eof=catalog.cleanup_complete=1;
    record.struct_size=sizeof(record); record.abi_version=1; record.captured_bytes=128;
    record.source_scope=record.metadata_identity_scope=record.metadata_complete=1;
    record.title_id_available=record.metadata_sha256_available=1;
    memcpy(record.title_id,"Any.Custom-1",13); memset(record.metadata_sha256,'a',64);
    view.struct_size=sizeof(view); view.abi_version=1; view.catalog=&catalog;
    view.records=&record; view.records_capacity=1;
    tokens.struct_size=sizeof(tokens); tokens.abi_version=1; tokens.title_count=1;
    memcpy(tokens.generation,"00000000000000000000000000000001",33); tokens.title_keys=key;
    return mb_title_pages_prepare_v1(&view,&tokens,backing,sizeof(backing),&cache)!=MB_TITLE_PAGES_OK ||
        !cache.prepared || cache.installation_supported || cache.build_fingerprint_available;
}
