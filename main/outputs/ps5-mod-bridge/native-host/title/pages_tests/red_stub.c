#include "../pages.h"
#include <string.h>

mb_title_pages_status mb_title_pages_measure_v1(
    const mb_title_pages_view_v1 *view,mb_title_pages_plan_v1 *plan) {
    (void)view;
    memset(plan,0,sizeof(*plan));
    return MB_TITLE_PAGES_OK;
}
mb_title_pages_status mb_title_pages_prepare_v1(
    const mb_title_pages_view_v1 *view,const mb_title_pages_tokens_v1 *tokens,
    uint8_t *backing,size_t backing_bytes,mb_title_pages_cache_v1 *cache) {
    (void)view; (void)tokens; (void)backing; (void)backing_bytes;
    memset(cache,0,sizeof(*cache));
    return MB_TITLE_PAGES_OK;
}
