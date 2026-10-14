#include "catalog.h"

int main(void) {
    mb_title_catalog_request_v1 request={0};
    mb_title_catalog_ledger_v1 ledger={0};
    char names[MB_TITLE_CATALOG_ENTRY_NAME_BYTES];
    char candidate[MB_TITLE_CATALOG_CANDIDATE_BYTES];
    uint8_t workspace[MB_TITLE_CATALOG_SOURCE_WORKSPACE_BYTES];
    mb_title_metadata_result_v1 records[MB_TITLE_CATALOG_MAX_RECORDS];
    uint32_t issues[MB_TITLE_CATALOG_MAX_ISSUES];
    mb_title_catalog_result_v1 result={0};
    request.struct_size=sizeof(request);request.abi_version=MB_TITLE_CATALOG_ABI_VERSION;
    request.profile=MB_TITLE_CACHE_SYSTEM_PRIVATE;request.scan_assertion=MB_TITLE_CATALOG_ASSERT_IMMUTABLE_SCAN;
    ledger.struct_size=sizeof(ledger);ledger.abi_version=MB_TITLE_CATALOG_ABI_VERSION;
    ledger.source_io.struct_size=sizeof(ledger.source_io);ledger.source_io.abi_version=MB_TITLE_SOURCE_ABI_VERSION;
    ledger.source_io.remaining_operations=MB_TITLE_CATALOG_OPERATION_LIMIT;
    ledger.source_io.remaining_read_bytes=MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT;
    return (int)mb_title_catalog_scan_v1(&request,&ledger,names,sizeof(names),candidate,sizeof(candidate),
        workspace,sizeof(workspace),records,MB_TITLE_CATALOG_MAX_RECORDS,issues,MB_TITLE_CATALOG_MAX_ISSUES,&result);
}
