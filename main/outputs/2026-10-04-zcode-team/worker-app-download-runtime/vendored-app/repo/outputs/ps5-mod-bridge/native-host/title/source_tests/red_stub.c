#include "source_test.h"

mb_title_source_status mb_title_source_capture_v1(
    const mb_title_source_request_v1 *request,mb_title_source_budget_v1 *budget,
    uint8_t *workspace,size_t capacity,mb_title_source_result_v1 *result) {
    (void)request; (void)budget; (void)workspace; (void)capacity; (void)result;
    return MB_TITLE_SOURCE_INTERNAL_ERROR;
}
mb_title_source_status mb_title_source_capture_fixture_v1(
    const mb_title_source_request_v1 *request,mb_title_source_budget_v1 *budget,
    uint8_t *workspace,size_t capacity,mb_title_source_result_v1 *result,
    const char *root,size_t root_bytes,const mb_title_source_test_ops_v1 *ops) {
    (void)root; (void)root_bytes; (void)ops;
    return mb_title_source_capture_v1(request,budget,workspace,capacity,result);
}
