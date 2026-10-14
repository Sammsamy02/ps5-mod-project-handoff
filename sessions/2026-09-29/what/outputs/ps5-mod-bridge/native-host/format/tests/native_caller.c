#include "dispatch.h"
_Static_assert(sizeof(uint64_t)==8,"64-bit ABI");
_Static_assert(MB_FORMAT_MAX_ADAPTERS==16,"bounded registry");
static mb_adapter_registry_v1 registry;
static mb_dispatch_result_v1 candidate, result;
static uint8_t bytes[221];
int main(void) {
    mb_adapter_registration_v1 entry=mb_pak_adapter_registration_v1();
    mb_probe_budget_v1 work={sizeof(work),1,131072,0};
    mb_probe_input_v1 input={sizeof(input),1,221,1,0,bytes,221,0,bytes,221,0};
    pak_source_v1 source={0}; pak_candidate_limits_v1 limits={0};
    pak_candidate_buffers_v1 buffers={0}; bz_budget codec={0};
    char json[MB_FORMAT_REPORT_LIMIT]; size_t length=0;
    if(mb_registry_seal_v1(1,MB_FORMAT_PS5_NATIVE,&entry,1,&registry)!=MB_DISPATCH_OK) return 1;
    if(mb_dispatch_probe_v1(&registry,&input,1,&work,&candidate)!=MB_DISPATCH_OK) return 2;
    (void)mb_pak_adapter_validate_v1(&registry,&candidate,&source,&limits,0,0,&buffers,&codec,NULL,NULL,&result);
    return mb_core_format_report_json(&candidate,json,sizeof(json),&length)==MB_CORE_OK ? 0 : 3;
}
