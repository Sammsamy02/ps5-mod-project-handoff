#include "dispatch.h"
#include <string.h>

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static mb_dispatch_code pak_probe(const mb_probe_input_v1 *i,uint32_t op,
    mb_probe_budget_v1 *budget,mb_probe_claim_v1 *claim) {
    *claim=(mb_probe_claim_v1){sizeof(*claim),1,MB_PROBE_NO_MATCH,MB_FORMAT_REASON_NONE,op,0};
    if(i->logical_size<PAK_V11_FOOTER_BYTES) return MB_DISPATCH_OK;
    if(i->tail_bytes<PAK_V11_FOOTER_BYTES) {
        claim->disposition=MB_PROBE_INCOMPLETE; claim->reason=MB_FORMAT_INPUT_UNAVAILABLE; return MB_DISPATCH_OK;
    }
    const uint8_t *footer=i->tail+i->tail_bytes-PAK_V11_FOOTER_BYTES;
    mb_dispatch_code rc=mb_probe_charge_v1(budget,4); if(rc) return rc;
    if(le32(footer+17)!=UINT32_C(0x5a6f12e1)) return MB_DISPATCH_OK;
    rc=mb_probe_charge_v1(budget,4); if(rc) return rc;
    if(le32(footer+21)!=11) {
        claim->disposition=MB_PROBE_UNSUPPORTED_PROFILE; claim->reason=MB_FORMAT_PROFILE_UNAVAILABLE; return MB_DISPATCH_OK;
    }
    rc=mb_probe_charge_v1(budget,1); if(rc) return rc;
    if(footer[16]) {
        claim->disposition=MB_PROBE_UNSUPPORTED_PROFILE; claim->reason=MB_FORMAT_ENCRYPTED_PROFILE_UNAVAILABLE; return MB_DISPATCH_OK;
    }
    claim->disposition=MB_PROBE_CANDIDATE; claim->reason=MB_FORMAT_CANDIDATE_REQUIRES_VALIDATION;
    return MB_DISPATCH_OK;
}
mb_adapter_registration_v1 mb_pak_adapter_registration_v1(void) {
    mb_adapter_registration_v1 a={0}; a.struct_size=sizeof(a); a.abi_version=1;
    a.descriptor.struct_size=sizeof(a.descriptor); a.descriptor.abi_version=1;
    memcpy(a.descriptor.adapter_id,"format.unreal.pak.classic_v11",sizeof("format.unreal.pak.classic_v11"));
    memcpy(a.descriptor.adapter_version,"1.0.0",sizeof("1.0.0"));
    memcpy(a.descriptor.format_id,"unreal_pak",sizeof("unreal_pak"));
    memcpy(a.descriptor.profile_id,"classic_v11_unencrypted_stored_sha1_v1",sizeof("classic_v11_unencrypted_stored_sha1_v1"));
    a.descriptor.operation_mask=3; a.probe=pak_probe; return a;
}
typedef struct span {uintptr_t start,end;} span;
static int make_span(const void *p,size_t bytes,size_t alignment,span *s) {
    uintptr_t at=(uintptr_t)p;
    if((bytes && !p) || (p && at%alignment) || bytes>UINTPTR_MAX-at) return 0;
    *s=(span){at,at+bytes}; return 1;
}
static int overlap(span a,span b) {return a.start<a.end && b.start<b.end && a.start<b.end && b.start<a.end;}
static int add(span *spans,size_t *count,const void *p,size_t elements,size_t bytes,size_t alignment) {
    if(*count>=24 || elements>SIZE_MAX/bytes || !make_span(p,elements*bytes,alignment,&spans[*count])) return 0;
    ++*count; return 1;
}
static int child_spans(const pak_candidate_buffers_v1 *b,span *spans,size_t *n) {
#define O(p,t) add(spans,n,(p),1,sizeof(t),_Alignof(t))
#define B(p,bytes) add(spans,n,(p),(bytes),1,1)
#define A(p,count,t) add(spans,n,(p),(count),sizeof(t),_Alignof(t))
    *n=0;
    return O(b->state,pak_candidate_state_v1) && B(b->reader_workspace,b->reader_workspace_bytes) &&
        B(b->primary_capture,b->primary_capacity) && B(b->secondary_workspace,b->secondary_workspace_bytes) &&
        B(b->directory_capture,b->directory_capture_capacity) && A(b->directories,b->directories_capacity,pak_directory_record_v1) &&
        A(b->files,b->files_capacity,pak_directory_entry_v1) && B(b->names,b->names_capacity) &&
        A(b->graph.starts,b->graph.starts_capacity,uint32_t) && A(b->graph.extents,b->graph.extents_capacity,pak_graph_extent_v1) &&
        A(b->graph.sort_indices,b->graph.sort_capacity,uint32_t) && B(b->graph.seen,b->graph.seen_bytes) &&
        A(b->graph.decode_blocks,b->graph.decode_blocks_capacity,pak_entry_block_v1) &&
        A(b->graph.selected_blocks,b->graph.selected_blocks_capacity,pak_entry_block_v1) &&
        B(b->physical_header,b->physical_header_capacity) && A(b->physical_blocks,b->physical_blocks_capacity,pak_physical_block_v1) &&
        B(b->payload.compressed,b->payload.compressed_bytes) && B(b->payload.codec_arena,b->payload.codec_arena_bytes) &&
        O(b->payload.codec_state,bz_state) && B(b->payload.output,b->payload.output_capacity);
#undef O
#undef B
#undef A
}
static void clear_spans(span *spans,size_t n) {
    for(size_t i=0;i<n;++i) {volatile uint8_t *p=(volatile uint8_t *)spans[i].start;
        size_t bytes=spans[i].end-spans[i].start; while(bytes--) *p++=0;}
}
static void failed(mb_dispatch_result_v1 *r,uint32_t reason,uint32_t state) {
    r->operation_state=state; r->format_candidate.state=state;
    r->format_candidate.reason=reason; r->format_candidate.scope=0;
    r->metadata_complete=0; r->selected_payload_complete=0; r->decoded_bytes=0;
}
pak_status mb_pak_adapter_validate_v1(const mb_adapter_registry_v1 *registry,
    const mb_dispatch_result_v1 *candidate,const pak_source_v1 *source,
    const pak_candidate_limits_v1 *limits,uint32_t want,uint32_t selected,
    const pak_candidate_buffers_v1 *buffers,bz_budget *codec_budget,
    pak_graph_cancel_fn cancelled,void *cancel_context,mb_dispatch_result_v1 *out) {
    span fixed[7],mutable[24]; size_t n;
#define F(index,p,t) make_span((p),sizeof(t),_Alignof(t),&fixed[index])
    if(!F(0,registry,mb_adapter_registry_v1) || !F(1,candidate,mb_dispatch_result_v1) ||
        !F(2,source,pak_source_v1) || !F(3,limits,pak_candidate_limits_v1) ||
        !F(4,buffers,pak_candidate_buffers_v1) || !F(5,codec_budget,bz_budget) || !F(6,out,mb_dispatch_result_v1))
        return PAK_STATUS_BAD_ARGUMENT;
#undef F
    for(size_t i=0;i<7;++i) for(size_t j=i+1;j<7;++j) if(overlap(fixed[i],fixed[j])) return PAK_STATUS_BAD_ARGUMENT;
    if(!mb_format_registry_valid_v1(registry) || !mb_format_result_valid_v1(candidate) ||
        !candidate->adapter_present || candidate->probe_disposition!=MB_PROBE_CANDIDATE ||
        candidate->format_candidate.reason!=MB_FORMAT_CANDIDATE_REQUIRES_VALIDATION ||
        candidate->operation_state!=MB_FORMAT_UNVERIFIED || want>1 || (!want && selected) ||
        (candidate->operation==1 && want) || (candidate->operation==2 && !want) ||
        source->struct_size!=sizeof(*source) || source->abi_version!=1 || !source->read_at ||
        !source->snapshot_token || !source->logical_size || source->reserved || source->snapshot_scope!=1 ||
        limits->struct_size!=sizeof(*limits) || limits->abi_version!=1 ||
        limits->profile!=PAK_CANDIDATE_PROFILE_CLASSIC_V11_UNENCRYPTED_STORED_SHA1_V1 ||
        buffers->struct_size!=sizeof(*buffers) || buffers->abi_version!=1 ||
        !child_spans(buffers,mutable,&n)) return PAK_STATUS_BAD_ARGUMENT;
    for(size_t i=0;i<n;++i) for(size_t j=0;j<7;++j) if(overlap(mutable[i],fixed[j])) return PAK_STATUS_BAD_ARGUMENT;
    mb_adapter_registration_v1 builtin=mb_pak_adapter_registration_v1();
    mb_dispatch_result_v1 r=*candidate; r.want_selected=want; r.selected_file_index=selected;
    if(candidate->registry_generation!=registry->generation || candidate->execution_context!=registry->execution_context ||
        candidate->adapter_slot>=registry->count || candidate->logical_size!=source->logical_size ||
        candidate->snapshot_token!=source->snapshot_token ||
        memcmp(&candidate->descriptor,&builtin.descriptor,sizeof(builtin.descriptor)) ||
        memcmp(&registry->entries[candidate->adapter_slot].descriptor,&builtin.descriptor,sizeof(builtin.descriptor)) ||
        registry->entries[candidate->adapter_slot].probe!=builtin.probe) {
        failed(&r,MB_FORMAT_RESULT_BINDING_MISMATCH,MB_FORMAT_UNVERIFIED);
        r.child_status=PAK_STATUS_BAD_ARGUMENT; *out=r; return PAK_STATUS_BAD_ARGUMENT;
    }
    pak_candidate_result_v1 child={0};
    pak_status status=pak_validate_v11_candidate(source,limits,want,selected,buffers,codec_budget,cancelled,cancel_context,&child);
    /* Candidate preflight writes nothing; operational child BAD_ARGUMENT can
     * still follow charged reads (for example, selecting an empty inventory).
     * Preserve that failed operation rather than leaving an old positive out. */
    if(status==PAK_STATUS_BAD_ARGUMENT && child.struct_size==0 && child.abi_version==0) return status;
    r.child_status=status; r.child_stage=child.stage; r.child_reason=child.child_reason;
    r.read_calls=child.read_calls; r.requested_read_bytes=child.requested_read_bytes;
    if(status==PAK_STATUS_OK) {
        if(child.status!=status || child.stage!=PAK_CANDIDATE_STAGE_COMPLETE || !child.metadata_complete ||
            child.snapshot_token!=candidate->snapshot_token || child.installation_supported || child.authenticity_verified ||
            child.game_acceptance_verified || (want && (!child.selected_payload_complete || child.selected_file_index!=selected)) ||
            (!want && child.selected_payload_complete)) {
            clear_spans(mutable,n); failed(&r,MB_FORMAT_RESULT_BINDING_MISMATCH,MB_FORMAT_UNVERIFIED);
            status=PAK_STATUS_INTERNAL_ERROR; r.child_status=status;
        } else {
            r.operation_state=r.format_candidate.state=MB_FORMAT_SUPPORTED;
            r.format_candidate.reason=MB_FORMAT_VALIDATED_CANDIDATE; r.format_candidate.scope=want ? 3u : 1u;
            r.metadata_complete=1; r.selected_payload_complete=want; r.decoded_bytes=child.decoded_bytes;
        }
    } else if(status==PAK_STATUS_CHECKSUM_MISMATCH) failed(&r,MB_FORMAT_INTEGRITY_FAILED,MB_FORMAT_UNSUPPORTED);
    else if(status==PAK_STATUS_UNSUPPORTED) failed(&r,MB_FORMAT_PROFILE_UNAVAILABLE,MB_FORMAT_UNSUPPORTED);
    else if(status==PAK_STATUS_MALFORMED) failed(&r,MB_FORMAT_INVALID_ARCHIVE,MB_FORMAT_UNSUPPORTED);
    else if(status==PAK_STATUS_RESOURCE_LIMIT) failed(&r,MB_FORMAT_WORK_LIMIT,MB_FORMAT_UNVERIFIED);
    else if(status==PAK_STATUS_PROVIDER_CHANGED) failed(&r,MB_FORMAT_SOURCE_CHANGED,MB_FORMAT_UNVERIFIED);
    else if(status==PAK_STATUS_CANCELLED) failed(&r,MB_FORMAT_CANCELLED,MB_FORMAT_UNVERIFIED);
    else failed(&r,status==PAK_STATUS_INTERNAL_ERROR ? MB_FORMAT_INTERNAL_FAILURE : MB_FORMAT_INPUT_UNAVAILABLE,MB_FORMAT_UNVERIFIED);
    *out=r; return status;
}
