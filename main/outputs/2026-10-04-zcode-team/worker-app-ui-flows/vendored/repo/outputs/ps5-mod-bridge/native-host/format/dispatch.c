#include "dispatch.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct span {uintptr_t begin,end;} span;
static int region(const void *p,size_t n,size_t alignment,span *s) {
    uintptr_t at=(uintptr_t)p;
    if((n && !p) || (p && at%alignment) || n>UINTPTR_MAX-at) return 0;
    *s=(span){at,at+n}; return 1;
}
static int overlap(span a,span b) {return a.begin<a.end && b.begin<b.end && a.begin<b.end && b.begin<a.end;}
static int zero(const void *p,size_t n) {const uint8_t *b=p; for(size_t i=0;i<n;++i) if(b[i]) return 0; return 1;}
static int text_valid(const char *p,size_t n,int version) {
    size_t end=0; while(end<n && p[end]) ++end;
    if(!end || end==n || !zero(p+end,n-end)) return 0;
    int component=0;
    for(size_t i=0;i<end;++i) {
        unsigned char c=(unsigned char)p[i];
        if(version) {
            if(c=='.') {if(!component) return 0; component=0;}
            else if(c>='0' && c<='9') component=1; else return 0;
        } else if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-')) return 0;
    }
    return !version || component;
}
static int desc_valid(const mb_adapter_desc_v1 *d) {
    return d->struct_size==sizeof(*d) && d->abi_version==1 && !d->reserved &&
        d->operation_mask && !(d->operation_mask&~3u) &&
        text_valid(d->adapter_id,sizeof(d->adapter_id),0) &&
        text_valid(d->adapter_version,sizeof(d->adapter_version),1) &&
        text_valid(d->format_id,sizeof(d->format_id),0) && text_valid(d->profile_id,sizeof(d->profile_id),0);
}
int mb_format_registry_valid_v1(const mb_adapter_registry_v1 *r) {
    span s;
    if(!region(r,sizeof(*r),_Alignof(mb_adapter_registry_v1),&s) ||
        r->struct_size!=sizeof(*r) || r->abi_version!=1 || !r->generation ||
        (r->execution_context!=1 && r->execution_context!=2) || r->count>16 ||
        r->sealed!=MB_FORMAT_REGISTRY_SEALED || r->reserved) return 0;
    for(uint32_t i=0;i<r->count;++i) {
        const mb_adapter_registration_v1 *a=&r->entries[i];
        if(a->struct_size!=sizeof(*a) || a->abi_version!=1 || !a->probe || !desc_valid(&a->descriptor) ||
            (i && strcmp(r->entries[i-1].descriptor.adapter_id,a->descriptor.adapter_id)>=0)) return 0;
    }
    return zero(r->entries+r->count,(16-r->count)*sizeof(r->entries[0]));
}
mb_dispatch_code mb_registry_seal_v1(uint64_t generation,uint32_t context,
    const mb_adapter_registration_v1 *entries,uint32_t count,mb_adapter_registry_v1 *out) {
    span a,b;
    if(!generation || (context!=1 && context!=2) || count>16 ||
        !region(out,sizeof(*out),_Alignof(mb_adapter_registry_v1),&a) ||
        !region(entries,count*sizeof(*entries),_Alignof(mb_adapter_registration_v1),&b) || overlap(a,b))
        return MB_DISPATCH_BAD_ARGUMENT;
    mb_adapter_registry_v1 r={0}; r.struct_size=sizeof(r); r.abi_version=1;
    r.generation=generation; r.execution_context=context; r.count=count; r.sealed=MB_FORMAT_REGISTRY_SEALED;
    for(uint32_t i=0;i<count;++i) {
        if(entries[i].struct_size!=sizeof(entries[i]) || entries[i].abi_version!=1 ||
            !entries[i].probe || !desc_valid(&entries[i].descriptor)) return MB_DISPATCH_INVALID_REGISTRY;
        r.entries[i]=entries[i];
        for(uint32_t j=i;j>0 && strcmp(r.entries[j-1].descriptor.adapter_id,r.entries[j].descriptor.adapter_id)>0;--j) {
            mb_adapter_registration_v1 temp=r.entries[j-1]; r.entries[j-1]=r.entries[j]; r.entries[j]=temp;
        }
    }
    if(!mb_format_registry_valid_v1(&r)) return MB_DISPATCH_INVALID_REGISTRY;
    *out=r; return MB_DISPATCH_OK;
}
static int budget_valid(const mb_probe_budget_v1 *b) {
    return b->struct_size==sizeof(*b) && b->abi_version==1 && b->remaining_steps<=MB_FORMAT_MAX_PROBE_STEPS &&
        b->used_steps<=MB_FORMAT_MAX_PROBE_STEPS-b->remaining_steps;
}
mb_dispatch_code mb_probe_charge_v1(mb_probe_budget_v1 *b,uint64_t steps) {
    span s;
    if(!region(b,sizeof(*b),_Alignof(mb_probe_budget_v1),&s) || !budget_valid(b)) return MB_DISPATCH_BAD_ARGUMENT;
    if(steps>b->remaining_steps) return MB_DISPATCH_RESOURCE_LIMIT;
    b->remaining_steps-=steps; b->used_steps+=steps; return MB_DISPATCH_OK;
}
static mb_scoped_state_v1 scoped(uint32_t state,uint32_t reason) {
    return (mb_scoped_state_v1){sizeof(mb_scoped_state_v1),1,state,reason,0,0};
}
static mb_dispatch_result_v1 initial(const mb_adapter_registry_v1 *r,const mb_probe_input_v1 *i,uint32_t op) {
    mb_dispatch_result_v1 v={0}; v.struct_size=sizeof(v); v.abi_version=1;
    v.registry_generation=r->generation; v.execution_context=r->execution_context;
    v.logical_size=i->logical_size; v.snapshot_token=i->snapshot_token; v.operation=op;
    v.format_candidate=scoped(MB_FORMAT_UNVERIFIED,MB_FORMAT_INPUT_UNAVAILABLE);
    v.engine_build=scoped(MB_FORMAT_UNVERIFIED,MB_FORMAT_TARGET_EVIDENCE_MISSING);
    v.asset_platform=scoped(MB_FORMAT_UNVERIFIED,MB_FORMAT_PLATFORM_ACCEPTANCE_MISSING);
    v.install_authority=scoped(MB_FORMAT_UNSUPPORTED,MB_FORMAT_INSTALL_OPERATION_UNAVAILABLE);
    return v;
}
static int claim_valid(const mb_probe_claim_v1 *c,uint32_t op) {
    if(c->struct_size!=sizeof(*c) || c->abi_version!=1 || c->reserved || c->operation_mask!=op) return 0;
    switch(c->disposition) {
    case MB_PROBE_NO_MATCH: return c->reason==MB_FORMAT_REASON_NONE;
    case MB_PROBE_CANDIDATE: return c->reason==MB_FORMAT_CANDIDATE_REQUIRES_VALIDATION;
    case MB_PROBE_UNSUPPORTED_PROFILE: return c->reason==MB_FORMAT_PROFILE_UNAVAILABLE || c->reason==MB_FORMAT_ENCRYPTED_PROFILE_UNAVAILABLE;
    case MB_PROBE_INCOMPLETE: return c->reason==MB_FORMAT_INPUT_UNAVAILABLE;
    case MB_PROBE_INVALID_ARCHIVE: return c->reason==MB_FORMAT_INVALID_ARCHIVE;
    default: return 0;
    }
}
mb_dispatch_code mb_dispatch_probe_v1(const mb_adapter_registry_v1 *r,const mb_probe_input_v1 *i,
    uint32_t op,mb_probe_budget_v1 *b,mb_dispatch_result_v1 *out) {
    span fixed[4],windows[2];
    if(!region(r,sizeof(*r),_Alignof(mb_adapter_registry_v1),&fixed[0]) ||
        !region(i,sizeof(*i),_Alignof(mb_probe_input_v1),&fixed[1]) ||
        !region(b,sizeof(*b),_Alignof(mb_probe_budget_v1),&fixed[2]) ||
        !region(out,sizeof(*out),_Alignof(mb_dispatch_result_v1),&fixed[3])) return MB_DISPATCH_BAD_ARGUMENT;
    for(size_t x=0;x<4;++x) for(size_t y=x+1;y<4;++y) if(overlap(fixed[x],fixed[y])) return MB_DISPATCH_BAD_ARGUMENT;
    if(!mb_format_registry_valid_v1(r)) return MB_DISPATCH_INVALID_REGISTRY;
    if(i->struct_size!=sizeof(*i) || i->abi_version!=1 || i->reserved || i->reserved2 || !i->snapshot_token ||
        i->prefix_bytes>4096 || i->tail_bytes>4096 || i->prefix_bytes>i->logical_size ||
        i->tail_offset>i->logical_size || i->tail_bytes!=i->logical_size-i->tail_offset ||
        (op!=1 && op!=2) || !budget_valid(b) ||
        !region(i->prefix,i->prefix_bytes,1,&windows[0]) || !region(i->tail,i->tail_bytes,1,&windows[1]))
        return MB_DISPATCH_BAD_ARGUMENT;
    for(size_t x=0;x<2;++x) for(size_t y=2;y<4;++y) if(overlap(windows[x],fixed[y])) return MB_DISPATCH_BAD_ARGUMENT;
    mb_dispatch_result_v1 v=initial(r,i,op); uint32_t claims=0,incomplete=0,slot=0;
    mb_probe_claim_v1 selected={0}; mb_dispatch_code status=MB_DISPATCH_OK;
    for(uint32_t x=0;x<r->count;++x) {
        if(!(r->entries[x].descriptor.operation_mask&op)) continue;
        mb_probe_claim_v1 c={0}; c.struct_size=sizeof(c); c.abi_version=1;
        mb_probe_budget_v1 before=*b;
        mb_dispatch_code rc=r->entries[x].probe(i,op,b,&c);
        if(!budget_valid(b) || b->remaining_steps>before.remaining_steps || b->used_steps<before.used_steps ||
            before.remaining_steps-b->remaining_steps!=b->used_steps-before.used_steps) {
            status=MB_DISPATCH_INTERNAL_ERROR; v.format_candidate.reason=MB_FORMAT_INTERNAL_FAILURE; break;
        }
        if(rc!=MB_DISPATCH_OK) {
            status=rc==MB_DISPATCH_RESOURCE_LIMIT ? rc : MB_DISPATCH_INTERNAL_ERROR;
            v.format_candidate.reason=rc==MB_DISPATCH_RESOURCE_LIMIT ? MB_FORMAT_WORK_LIMIT : MB_FORMAT_INTERNAL_FAILURE; break;
        }
        if(!claim_valid(&c,op)) {status=MB_DISPATCH_INTERNAL_ERROR; v.format_candidate.reason=MB_FORMAT_INTERNAL_FAILURE; break;}
        if(c.disposition==MB_PROBE_INCOMPLETE) incomplete=1;
        else if(c.disposition!=MB_PROBE_NO_MATCH) {++claims; slot=x; selected=c;}
    }
    v.probe_steps=b->used_steps;
    if(status==MB_DISPATCH_OK) {
        if(incomplete) v.format_candidate.reason=MB_FORMAT_INPUT_UNAVAILABLE;
        else if(claims>1) v.format_candidate.reason=MB_FORMAT_AMBIGUOUS_ADAPTER;
        else if(!claims) {v.operation_state=MB_FORMAT_UNSUPPORTED; v.format_candidate=scoped(MB_FORMAT_UNSUPPORTED,MB_FORMAT_NO_ADAPTER);}
        else {
            v.adapter_present=1; v.adapter_slot=slot; v.descriptor=r->entries[slot].descriptor;
            v.probe_disposition=selected.disposition; v.format_candidate.reason=selected.reason;
            if(selected.disposition!=MB_PROBE_CANDIDATE) v.operation_state=v.format_candidate.state=MB_FORMAT_UNSUPPORTED;
        }
    }
    *out=v; return status;
}
static int scope_valid(const mb_scoped_state_v1 *s,uint32_t state,uint32_t reason) {
    return s->struct_size==sizeof(*s) && s->abi_version==1 && !s->reserved &&
        s->state==state && s->reason==reason && !s->scope;
}
int mb_format_result_valid_v1(const mb_dispatch_result_v1 *r) {
    span s;
    if(!region(r,sizeof(*r),_Alignof(mb_dispatch_result_v1),&s) || r->struct_size!=sizeof(*r) || r->abi_version!=1 ||
        !r->registry_generation || !r->snapshot_token || (r->execution_context!=1 && r->execution_context!=2) ||
        (r->operation!=1 && r->operation!=2) || r->reserved || r->adapter_present>1 ||
        r->operation_state>2 || r->compatibility_state!=MB_FORMAT_UNVERIFIED || r->probe_disposition>MB_PROBE_INVALID_ARCHIVE ||
        r->probe_steps>MB_FORMAT_MAX_PROBE_STEPS || r->want_selected>1 ||
        (!r->want_selected && r->selected_file_index) || (r->operation==1 && r->want_selected) ||
        r->runtime_supported || r->installation_supported || r->authenticity_verified || r->game_acceptance_verified ||
        !scope_valid(&r->engine_build,MB_FORMAT_UNVERIFIED,MB_FORMAT_TARGET_EVIDENCE_MISSING) ||
        !scope_valid(&r->asset_platform,MB_FORMAT_UNVERIFIED,MB_FORMAT_PLATFORM_ACCEPTANCE_MISSING) ||
        !scope_valid(&r->install_authority,MB_FORMAT_UNSUPPORTED,MB_FORMAT_INSTALL_OPERATION_UNAVAILABLE)) return 0;
    if(r->adapter_present) {
        if(r->adapter_slot>=16 || !desc_valid(&r->descriptor) || !(r->descriptor.operation_mask&r->operation)) return 0;
    } else if(r->adapter_slot || !zero(&r->descriptor,sizeof(r->descriptor))) return 0;
    const mb_scoped_state_v1 *f=&r->format_candidate;
    if(f->struct_size!=sizeof(*f) || f->abi_version!=1 || f->reserved || f->state>2 ||
        f->state!=r->operation_state || f->reason>MB_FORMAT_INTERNAL_FAILURE || f->reason==MB_FORMAT_REASON_NONE) return 0;
    if(f->state==MB_FORMAT_SUPPORTED) return r->adapter_present && f->reason==MB_FORMAT_VALIDATED_CANDIDATE &&
        r->metadata_complete==1 && r->child_status==PAK_STATUS_OK && r->child_stage==PAK_CANDIDATE_STAGE_COMPLETE &&
        ((r->operation==1 && !r->want_selected && !r->selected_payload_complete && f->scope==1) ||
         (r->operation==2 && r->want_selected==1 && r->selected_payload_complete==1 && f->scope==3));
    return !f->scope && !r->metadata_complete && !r->selected_payload_complete && !r->decoded_bytes;
}
static const char *reason_name(uint32_t reason) {
    static const char *const names[]={"none","candidate_requires_validation","validated_candidate","no_format_adapter",
        "ambiguous_adapter","profile_unavailable","encrypted_profile_unavailable","invalid_archive","integrity_failed",
        "input_unavailable","source_changed","work_limit","cancelled","target_evidence_missing","platform_acceptance_missing",
        "install_operation_unavailable","result_binding_mismatch","internal_failure"};
    return names[reason];
}
int mb_core_format_report_json(const mb_dispatch_result_v1 *r,char *out,size_t capacity,size_t *length) {
    span a,b,c;
    if(capacity>MB_FORMAT_REPORT_LIMIT || !region(r,sizeof(*r),_Alignof(mb_dispatch_result_v1),&a) ||
        !region(out,capacity,1,&b) || !region(length,sizeof(*length),_Alignof(size_t),&c) ||
        overlap(a,b) || overlap(a,c) || overlap(b,c)) return MB_CORE_INVALID_ARGUMENT;
    *length=0; if(capacity) out[0]=0;
    if(!mb_format_result_valid_v1(r)) return MB_CORE_INVALID_ARGUMENT;
    static const char *const states[]={"unverified","supported","unsupported"};
    const char *scope=r->format_candidate.scope==3 ? "[\"classic_v11_metadata\",\"selected_payload\"]" :
        r->format_candidate.scope==1 ? "[\"classic_v11_metadata\"]" : "[]";
    char json[MB_FORMAT_REPORT_LIMIT];
    int n=snprintf(json,sizeof(json),
        "{\"schema_version\":1,\"operation\":\"%s\",\"execution_context\":\"%s\","
        "\"adapter_present\":%s,\"adapter_id\":\"%s\",\"adapter_version\":\"%s\",\"format_id\":\"%s\",\"profile_id\":\"%s\","
        "\"operation_state\":\"%s\",\"compatibility_state\":\"unverified\","
        "\"format_candidate\":{\"state\":\"%s\",\"reason\":\"%s\",\"scope\":%s},"
        "\"engine_build\":{\"state\":\"unverified\",\"reason\":\"target_evidence_missing\",\"scope\":[]},"
        "\"asset_platform\":{\"state\":\"unverified\",\"reason\":\"platform_acceptance_missing\",\"scope\":[]},"
        "\"install_authority\":{\"state\":\"unsupported\",\"reason\":\"install_operation_unavailable\",\"scope\":[]},"
        "\"want_selected\":%s,\"selected_file_index\":%u,\"metadata_complete\":%s,\"selected_payload_complete\":%s,"
        "\"runtime_supported\":false,\"installation_supported\":false,\"authenticity_verified\":false,\"game_acceptance_verified\":false,"
        "\"diagnostics\":{\"child_status\":%u,\"child_stage\":%u,\"child_reason\":%u,\"read_calls\":%u,"
        "\"requested_read_bytes\":%" PRIu64 ",\"decoded_bytes\":%" PRIu64 ",\"probe_steps\":%" PRIu64 "}}",
        r->operation==1 ? "inspect_metadata" : "validate_selected_payload",r->execution_context==1 ? "host_fixture" : "ps5_native",
        r->adapter_present ? "true" : "false",r->descriptor.adapter_id,r->descriptor.adapter_version,r->descriptor.format_id,r->descriptor.profile_id,
        states[r->operation_state],states[r->format_candidate.state],reason_name(r->format_candidate.reason),scope,
        r->want_selected ? "true" : "false",r->selected_file_index,r->metadata_complete ? "true" : "false",
        r->selected_payload_complete ? "true" : "false",r->child_status,r->child_stage,r->child_reason,r->read_calls,
        r->requested_read_bytes,r->decoded_bytes,r->probe_steps);
    if(n<0 || (size_t)n>=sizeof(json)) return MB_CORE_INTERNAL_ERROR;
    *length=(size_t)n; if(capacity<=(size_t)n) return MB_CORE_BUFFER_TOO_SMALL;
    memcpy(out,json,(size_t)n+1); return MB_CORE_OK;
}
