#include "dispatch.h"
#include <string.h>
mb_adapter_registration_v1 mb_pak_adapter_registration_v1(void) {
    mb_adapter_registration_v1 a={0}; a.struct_size=sizeof(a); a.abi_version=1;
    a.descriptor.struct_size=sizeof(a.descriptor); a.descriptor.abi_version=1;
    strcpy(a.descriptor.adapter_id,"format.unreal.pak.classic_v11");
    strcpy(a.descriptor.adapter_version,"1.0.0"); strcpy(a.descriptor.format_id,"unreal_pak");
    strcpy(a.descriptor.profile_id,"classic_v11_unencrypted_stored_sha1_v1"); a.descriptor.operation_mask=3;
    return a;
}
mb_dispatch_code mb_registry_seal_v1(uint64_t g,uint32_t c,const mb_adapter_registration_v1 *a,
    uint32_t n,mb_adapter_registry_v1 *r) { (void)g;(void)c;(void)a;(void)n;(void)r;return MB_DISPATCH_INTERNAL_ERROR; }
int mb_format_registry_valid_v1(const mb_adapter_registry_v1 *r) { (void)r; return 0; }
int mb_format_result_valid_v1(const mb_dispatch_result_v1 *r) { (void)r; return 0; }
mb_dispatch_code mb_probe_charge_v1(mb_probe_budget_v1 *b,uint64_t n) { (void)b;(void)n;return MB_DISPATCH_INTERNAL_ERROR; }
mb_dispatch_code mb_dispatch_probe_v1(const mb_adapter_registry_v1 *r,const mb_probe_input_v1 *i,
    uint32_t o,mb_probe_budget_v1 *b,mb_dispatch_result_v1 *v) {(void)r;(void)i;(void)o;(void)b;(void)v;return MB_DISPATCH_INTERNAL_ERROR;}
pak_status mb_pak_adapter_validate_v1(const mb_adapter_registry_v1 *r,const mb_dispatch_result_v1 *p,
    const pak_source_v1 *s,const pak_candidate_limits_v1 *l,uint32_t w,uint32_t i,
    const pak_candidate_buffers_v1 *b,bz_budget *z,pak_graph_cancel_fn f,void *c,mb_dispatch_result_v1 *v) {
    (void)r;(void)p;(void)s;(void)l;(void)w;(void)i;(void)b;(void)z;(void)f;(void)c;(void)v;
    return PAK_STATUS_INTERNAL_ERROR;
}
int mb_core_format_report_json(const mb_dispatch_result_v1 *r,char *o,size_t c,size_t *n) {
    (void)r;(void)o;(void)c;(void)n;return MB_CORE_INTERNAL_ERROR;
}
