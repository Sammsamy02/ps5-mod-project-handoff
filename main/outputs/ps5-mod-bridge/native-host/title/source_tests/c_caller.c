#include "source.h"
#include <stdio.h>
#include <string.h>

_Static_assert(MB_TITLE_SOURCE_WORKSPACE_BYTES==MB_TITLE_METADATA_INPUT_LIMIT,"decoder byte bound");
_Static_assert(sizeof(((mb_title_source_result_v1 *)0)->metadata)==sizeof(mb_title_metadata_result_v1),"owning metadata");

/* Invalid profile exercises the actual C ABI without opening any OS path.
 * Native mode links this caller but never executes it. */
int main(void) {
    static const char candidate[]="Any_Custom-42";
    static uint8_t workspace[MB_TITLE_SOURCE_WORKSPACE_BYTES];
    mb_title_source_request_v1 request={sizeof(request),1,0,sizeof(candidate)-1,candidate,{0,0}};
    mb_title_source_budget_v1 budget={sizeof(budget),1,128,0,65537,0,{0,0}};
    mb_title_source_result_v1 result,old_result;
    memset(&result,0x5a,sizeof(result)); old_result=result;
    memset(workspace,0x5a,sizeof(workspace));
    if(mb_title_source_capture_v1(&request,&budget,workspace,sizeof(workspace),&result)!=MB_TITLE_SOURCE_BAD_ARGUMENT)
        return 1;
    if(memcmp(&result,&old_result,sizeof(result)) || workspace[0]!=0x5a || budget.reserved_operations) return 1;
    printf("SOURCE_C_ABI PASS request=%zu budget=%zu result=%zu no_io=true\n",
        sizeof(request),sizeof(budget),sizeof(result));
    return 0;
}
