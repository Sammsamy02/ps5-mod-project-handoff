#include "dispatch.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL %u: %s\n",(unsigned)__LINE__,#x); } } while(0)
#define REQUIRE(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL %u: %s\n",(unsigned)__LINE__,#x); return; } } while(0)
static mb_adapter_registry_v1 registry;
static mb_dispatch_result_v1 candidate, receipt;
static mb_probe_budget_v1 work;
static uint8_t image[16384]; static size_t image_bytes;
static uint8_t reader[65536], primary[4096], secondary[4096], directory[4096], names[2048];
static uint8_t physical[1081], compressed[16384], arena[65536], sink[32768], seen[8];
static pak_directory_record_v1 directories[8]; static pak_directory_entry_v1 files[8];
static uint32_t starts[8], sorted[8]; static pak_graph_extent_v1 extents[8];
static pak_entry_block_v1 decode_blocks[64], selected_blocks[64];
static pak_physical_block_v1 physical_blocks[64]; static bz_state codec_state;
static pak_candidate_state_v1 state; static pak_candidate_buffers_v1 buffers;
static pak_candidate_limits_v1 limits; static bz_budget codec_budget; static pak_source_v1 source;
static uint32_t calls, fail_call, fault; static uint64_t requested_bytes;
static const char *fixture_root, *synthetic_root;

static int bytes_are(const void *v,size_t n,uint8_t value) {
    const uint8_t *p=v; for(size_t i=0;i<n;++i) if(p[i]!=value) return 0; return 1;
}
static int load(const char *root,const char *name) {
    char path[1024]; int n=snprintf(path,sizeof(path),"%s/%s",root,name);
    if(n<0 || (size_t)n>=sizeof(path)) return 0;
    FILE *f=fopen(path,"rb"); if(!f) return 0;
    image_bytes=fread(image,1,sizeof(image),f);
    int ok=!ferror(f) && fgetc(f)==EOF && image_bytes>0;
    return fclose(f)==0 && ok;
}
static mb_probe_input_v1 input(void) {
    uint32_t n=image_bytes<4096 ? (uint32_t)image_bytes : 4096u;
    mb_probe_input_v1 i={sizeof(i),1,image_bytes,UINT64_C(0x464f524d4154),image_bytes-n,
        image,n,0,image+image_bytes-n,n,0}; return i;
}
static void reset_work(uint64_t n) { work=(mb_probe_budget_v1){sizeof(work),1,n,0}; }
static int seal(void) {
    mb_adapter_registration_v1 a=mb_pak_adapter_registration_v1();
    return mb_registry_seal_v1(7,MB_FORMAT_HOST_FIXTURE,&a,1,&registry)==MB_DISPATCH_OK;
}
static int probe(uint32_t op) {
    mb_probe_input_v1 i=input(); reset_work(MB_FORMAT_MAX_PROBE_STEPS);
    return mb_dispatch_probe_v1(&registry,&i,op,&work,&candidate)==MB_DISPATCH_OK;
}
static pak_provider_status read_image(void *ctx,uint64_t off,uint8_t *dst,uint32_t n,uint32_t *actual) {
    (void)ctx; ++calls; requested_bytes+=n; *actual=0;
    if(off>image_bytes || n>image_bytes-off) return PAK_PROVIDER_CONTRACT_ERROR;
    if(calls==fail_call) {
        if(fault==1) { memcpy(dst,image+(size_t)off,n-1); *actual=n-1; return PAK_PROVIDER_OK; }
        return fault==2 ? PAK_PROVIDER_CHANGED : PAK_PROVIDER_IO_ERROR;
    }
    memcpy(dst,image+(size_t)off,n); *actual=n; return PAK_PROVIDER_OK;
}
static void prepare(void) {
    memset(sink,0xa5,sizeof(sink)); codec_budget=bz_default_budget();
    pak_candidate_limits_v1_default(&limits); calls=0; requested_bytes=0; fail_call=0; fault=0;
    source=(pak_source_v1){sizeof(source),1,image_bytes,UINT64_C(0x464f524d4154),1,0,NULL,read_image};
    buffers=(pak_candidate_buffers_v1){0}; buffers.struct_size=sizeof(buffers); buffers.abi_version=1;
    buffers.state=&state; buffers.reader_workspace=reader; buffers.reader_workspace_bytes=sizeof(reader);
    buffers.primary_capture=primary; buffers.primary_capacity=sizeof(primary);
    buffers.secondary_workspace=secondary; buffers.secondary_workspace_bytes=sizeof(secondary);
    buffers.directory_capture=directory; buffers.directory_capture_capacity=sizeof(directory);
    buffers.directories=directories; buffers.directories_capacity=8; buffers.files=files; buffers.files_capacity=8;
    buffers.names=names; buffers.names_capacity=sizeof(names);
    buffers.graph=(pak_graph_workspace_v1){sizeof(buffers.graph),1,starts,8,extents,8,sorted,8,seen,sizeof(seen),
        decode_blocks,64,selected_blocks,64};
    buffers.physical_header=physical; buffers.physical_header_capacity=sizeof(physical);
    buffers.physical_blocks=physical_blocks; buffers.physical_blocks_capacity=64;
    buffers.payload=(pak_reconciled_payload_buffers_v1){compressed,sizeof(compressed),arena,sizeof(arena),
        &codec_state,sink,sizeof(sink)};
}
static pak_status validate(uint32_t want,uint32_t selected) {
    return mb_pak_adapter_validate_v1(&registry,&candidate,&source,&limits,want,selected,&buffers,
        &codec_budget,NULL,NULL,&receipt);
}
static void authority_false(const mb_dispatch_result_v1 *r) {
    CHECK(r->compatibility_state==MB_FORMAT_UNVERIFIED);
    CHECK(r->engine_build.state==MB_FORMAT_UNVERIFIED && r->asset_platform.state==MB_FORMAT_UNVERIFIED);
    CHECK(r->install_authority.state==MB_FORMAT_UNSUPPORTED);
    CHECK(!r->runtime_supported && !r->installation_supported && !r->authenticity_verified && !r->game_acceptance_verified);
}
static mb_dispatch_code claim_candidate(const mb_probe_input_v1 *i,uint32_t op,
    mb_probe_budget_v1 *b,mb_probe_claim_v1 *c) {
    (void)i; mb_dispatch_code rc=mb_probe_charge_v1(b,1); if(rc) return rc;
    *c=(mb_probe_claim_v1){sizeof(*c),1,MB_PROBE_CANDIDATE,MB_FORMAT_CANDIDATE_REQUIRES_VALIDATION,op,0};
    return MB_DISPATCH_OK;
}
static mb_dispatch_code claim_incomplete(const mb_probe_input_v1 *i,uint32_t op,
    mb_probe_budget_v1 *b,mb_probe_claim_v1 *c) {
    mb_dispatch_code rc=claim_candidate(i,op,b,c); c->disposition=MB_PROBE_INCOMPLETE;
    c->reason=MB_FORMAT_INPUT_UNAVAILABLE; return rc;
}
static mb_dispatch_code claim_refund(const mb_probe_input_v1 *i,uint32_t op,
    mb_probe_budget_v1 *b,mb_probe_claim_v1 *c) {
    (void)i; ++b->remaining_steps;
    *c=(mb_probe_claim_v1){sizeof(*c),1,MB_PROBE_CANDIDATE,MB_FORMAT_CANDIDATE_REQUIRES_VALIDATION,op,0};
    return MB_DISPATCH_OK;
}
static void registry_controls(void) {
    REQUIRE(seal()); CHECK(mb_format_registry_valid_v1(&registry));
    mb_adapter_registration_v1 a[2]={mb_pak_adapter_registration_v1(),mb_pak_adapter_registration_v1()};
    mb_adapter_registry_v1 saved=registry;
    CHECK(mb_registry_seal_v1(8,1,a,2,&registry)==MB_DISPATCH_INVALID_REGISTRY);
    CHECK(memcmp(&saved,&registry,sizeof(registry))==0);
    a[0].descriptor.reserved=1;
    CHECK(mb_registry_seal_v1(8,1,a,1,&registry)==MB_DISPATCH_INVALID_REGISTRY);
    a[0]=mb_pak_adapter_registration_v1(); a[0].abi_version=2;
    CHECK(mb_registry_seal_v1(8,1,a,1,&registry)==MB_DISPATCH_INVALID_REGISTRY);
    CHECK(mb_registry_seal_v1(0,1,NULL,0,&registry)==MB_DISPATCH_BAD_ARGUMENT);
    CHECK(mb_registry_seal_v1(8,3,NULL,0,&registry)==MB_DISPATCH_BAD_ARGUMENT);
    CHECK(mb_registry_seal_v1(8,1,a,17,&registry)==MB_DISPATCH_BAD_ARGUMENT);
    CHECK(mb_registry_seal_v1(8,1,&registry.entries[0],1,&registry)==MB_DISPATCH_BAD_ARGUMENT);
    CHECK(memcmp(&saved,&registry,sizeof(registry))==0);
    CHECK(mb_registry_seal_v1(8,1,NULL,0,&registry)==MB_DISPATCH_OK);
    image_bytes=256; memset(image,0,sizeof(image)); mb_probe_input_v1 i=input(); reset_work(10);
    CHECK(mb_dispatch_probe_v1(&registry,&i,1,&work,&receipt)==MB_DISPATCH_OK);
    CHECK(receipt.operation_state==MB_FORMAT_UNSUPPORTED && !receipt.adapter_present);
}
static void probe_controls(void) {
    REQUIRE(seal()); REQUIRE(load(fixture_root,"pack_v11.pak")); REQUIRE(probe(1));
    CHECK(candidate.format_candidate.state==MB_FORMAT_UNVERIFIED);
    CHECK(candidate.format_candidate.reason==MB_FORMAT_CANDIDATE_REQUIRES_VALIDATION);
    CHECK(!candidate.format_candidate.scope && candidate.adapter_present==1); authority_false(&candidate);
    char json[MB_FORMAT_REPORT_LIMIT]; size_t length=0; uint64_t consumed=work.used_steps;
    CHECK(mb_core_format_report_json(&candidate,json,sizeof(json),&length)==MB_CORE_OK);
    CHECK(length==strlen(json) && work.used_steps==consumed);
    printf("PROBE_REPORT %s\n",json);
    uint64_t needed=work.used_steps; CHECK(needed>0 && needed<=MB_FORMAT_MAX_PROBE_STEPS);
    mb_probe_input_v1 i=input(); reset_work(needed);
    CHECK(mb_dispatch_probe_v1(&registry,&i,1,&work,&receipt)==MB_DISPATCH_OK && !work.remaining_steps);
    reset_work(needed-1);
    CHECK(mb_dispatch_probe_v1(&registry,&i,1,&work,&receipt)==MB_DISPATCH_RESOURCE_LIMIT);
    CHECK(receipt.operation_state==MB_FORMAT_UNVERIFIED && !receipt.format_candidate.scope);
    i.tail_bytes=24; i.tail_offset=i.logical_size-24; i.tail=image+image_bytes-24; reset_work(100);
    CHECK(mb_dispatch_probe_v1(&registry,&i,1,&work,&receipt)==MB_DISPATCH_OK);
    CHECK(receipt.format_candidate.state==MB_FORMAT_UNVERIFIED);
    image[image_bytes-221+16]=1; CHECK(probe(1));
    CHECK(candidate.operation_state==MB_FORMAT_UNSUPPORTED); image[image_bytes-221+16]=0;
    image[image_bytes-221+21]=12; CHECK(probe(1));
    CHECK(candidate.operation_state==MB_FORMAT_UNSUPPORTED); image[image_bytes-221+21]=11;
    mb_adapter_registration_v1 a[2]={mb_pak_adapter_registration_v1(),mb_pak_adapter_registration_v1()};
    memset(a[1].descriptor.adapter_id,0,sizeof(a[1].descriptor.adapter_id));
    strcpy(a[1].descriptor.adapter_id,"format.test.second"); a[1].probe=claim_candidate;
    CHECK(mb_registry_seal_v1(9,1,a,2,&registry)==MB_DISPATCH_OK); REQUIRE(probe(1));
    CHECK(candidate.operation_state==MB_FORMAT_UNVERIFIED && candidate.format_candidate.reason==MB_FORMAT_AMBIGUOUS_ADAPTER);
    a[1].probe=claim_incomplete; CHECK(mb_registry_seal_v1(10,1,a,2,&registry)==MB_DISPATCH_OK);
    REQUIRE(probe(1)); CHECK(candidate.format_candidate.reason==MB_FORMAT_INPUT_UNAVAILABLE);
    a[1].probe=claim_refund; CHECK(mb_registry_seal_v1(11,1,a,2,&registry)==MB_DISPATCH_OK);
    i=input(); reset_work(100);
    CHECK(mb_dispatch_probe_v1(&registry,&i,1,&work,&receipt)==MB_DISPATCH_INTERNAL_ERROR);
    CHECK(receipt.operation_state==MB_FORMAT_UNVERIFIED && !receipt.format_candidate.scope);
    REQUIRE(seal()); image_bytes=256; memset(image,0,sizeof(image)); REQUIRE(probe(1));
    CHECK(candidate.operation_state==MB_FORMAT_UNSUPPORTED && !candidate.adapter_present);
}
static void real_candidates(void) {
    REQUIRE(seal()); const char *archives[]={"pack_v11.pak","pack_v11_compress.pak"};
    for(unsigned a=0;a<2;++a) {
        REQUIRE(load(fixture_root,archives[a])); REQUIRE(probe(1)); prepare();
        CHECK(validate(0,0)==PAK_STATUS_OK); CHECK(receipt.metadata_complete && !receipt.selected_payload_complete);
        CHECK(receipt.format_candidate.state==MB_FORMAT_SUPPORTED && receipt.format_candidate.scope==1);
        CHECK(receipt.child_stage==PAK_CANDIDATE_STAGE_COMPLETE && calls==receipt.read_calls);
        CHECK(requested_bytes==receipt.requested_read_bytes); authority_false(&receipt);
        REQUIRE(probe(2)); prepare(); CHECK(validate(1,2)==PAK_STATUS_OK);
        CHECK(receipt.selected_payload_complete && receipt.want_selected && receipt.selected_file_index==2);
        CHECK(receipt.format_candidate.scope==3 && receipt.decoded_bytes==2048);
        CHECK(bytes_are(sink,2048,0)); authority_false(&receipt);
    }
    REQUIRE(load(synthetic_root,"empty.pak")); REQUIRE(probe(1)); prepare();
    CHECK(validate(0,0)==PAK_STATUS_OK && receipt.metadata_complete && !receipt.want_selected);
    REQUIRE(probe(2)); prepare(); CHECK(validate(1,0)!=PAK_STATUS_OK);
    CHECK(!receipt.format_candidate.scope && receipt.format_candidate.state!=MB_FORMAT_SUPPORTED);
    CHECK(calls>0 && calls==receipt.read_calls && mb_format_result_valid_v1(&receipt));
    CHECK(bytes_are(sink,sizeof(sink),0));
}
static int cancel_now(void *ctx) { (void)ctx; return 1; }
static int cancel_after_read(void *ctx) { (void)ctx; return calls!=0; }
static void binding_failures(void) {
    REQUIRE(seal()); REQUIRE(load(fixture_root,"pack_v11.pak")); REQUIRE(probe(2)); prepare();
    source.snapshot_token++; CHECK(validate(1,2)!=PAK_STATUS_OK); CHECK(!calls);
    CHECK(receipt.format_candidate.reason==MB_FORMAT_RESULT_BINDING_MISMATCH); authority_false(&receipt);
    source.snapshot_token--; candidate.registry_generation++; CHECK(validate(1,2)!=PAK_STATUS_OK && !calls);
    candidate.registry_generation--; candidate.execution_context=MB_FORMAT_PS5_NATIVE;
    CHECK(validate(1,2)!=PAK_STATUS_OK && !calls);
    CHECK(receipt.format_candidate.reason==MB_FORMAT_RESULT_BINDING_MISMATCH);
    candidate.execution_context=MB_FORMAT_HOST_FIXTURE; source.logical_size++;
    CHECK(validate(1,2)!=PAK_STATUS_OK && !calls);
    CHECK(receipt.format_candidate.reason==MB_FORMAT_RESULT_BINDING_MISMATCH); source.logical_size--;
    CHECK(validate(0,0)==PAK_STATUS_BAD_ARGUMENT && !calls);
    REQUIRE(probe(1)); prepare(); image[image_bytes-221+41]^=1;
    CHECK(validate(0,0)==PAK_STATUS_CHECKSUM_MISMATCH);
    CHECK(receipt.format_candidate.state==MB_FORMAT_UNSUPPORTED && !receipt.format_candidate.scope);
    CHECK(bytes_are(sink,sizeof(sink),0)); image[image_bytes-221+41]^=1;
    for(unsigned f=1;f<=3;++f) {
        REQUIRE(probe(2)); prepare(); fail_call=1; fault=f;
        CHECK(validate(1,2)!=PAK_STATUS_OK && calls==1);
        CHECK(receipt.operation_state==MB_FORMAT_UNVERIFIED && !receipt.format_candidate.scope);
        CHECK(bytes_are(sink,sizeof(sink),0));
    }
    REQUIRE(probe(2)); prepare();
    CHECK(mb_pak_adapter_validate_v1(&registry,&candidate,&source,&limits,1,2,&buffers,&codec_budget,
        cancel_now,NULL,&receipt)==PAK_STATUS_CANCELLED);
    CHECK(!calls && receipt.operation_state==MB_FORMAT_UNVERIFIED);
    prepare();
    CHECK(mb_pak_adapter_validate_v1(&registry,&candidate,&source,&limits,1,2,&buffers,&codec_budget,
        cancel_after_read,NULL,&receipt)==PAK_STATUS_CANCELLED);
    printf("CANCEL_AFTER_READ provider_calls=%u charged_calls=%u\n",calls,receipt.read_calls);
    CHECK(calls>0 && receipt.read_calls==calls && !receipt.format_candidate.scope);
    CHECK(bytes_are(sink,sizeof(sink),0));
    prepare(); CHECK(validate(1,2)==PAK_STATUS_OK); uint32_t complete_calls=calls;
    prepare(); fail_call=complete_calls; fault=1;
    CHECK(validate(1,2)==PAK_STATUS_PROVIDER_SHORT_READ && calls==complete_calls);
    CHECK(receipt.operation_state==MB_FORMAT_UNVERIFIED && !receipt.format_candidate.scope);
    CHECK(bytes_are(sink,sizeof(sink),0));
    REQUIRE(probe(1)); prepare(); CHECK(validate(0,0)==PAK_STATUS_OK);
    uint64_t needed_bytes=requested_bytes; uint32_t needed_calls=calls;
    prepare(); limits.max_requested_read_bytes=needed_bytes; limits.max_read_calls=needed_calls;
    CHECK(validate(0,0)==PAK_STATUS_OK);
    prepare(); limits.max_requested_read_bytes=needed_bytes-1;
    CHECK(validate(0,0)==PAK_STATUS_RESOURCE_LIMIT && !receipt.format_candidate.scope);
    prepare(); limits.max_read_calls=needed_calls-1;
    CHECK(validate(0,0)==PAK_STATUS_RESOURCE_LIMIT && !receipt.format_candidate.scope);
}
static void report_aliases(void) {
    REQUIRE(seal()); REQUIRE(load(fixture_root,"pack_v11_compress.pak")); REQUIRE(probe(2)); prepare();
    CHECK(validate(1,2)==PAK_STATUS_OK); char json[MB_FORMAT_REPORT_LIMIT]; size_t n=0;
    CHECK(mb_core_format_report_json(&receipt,NULL,0,&n)==MB_CORE_BUFFER_TOO_SMALL && n>0);
    uint64_t saved_work=work.used_steps; uint32_t saved_calls=calls; memset(json,0xa5,sizeof(json));
    CHECK(mb_core_format_report_json(&receipt,json,n,&n)==MB_CORE_BUFFER_TOO_SMALL && !json[0]);
    CHECK(mb_core_format_report_json(&receipt,json,sizeof(json),&n)==MB_CORE_OK);
    CHECK(n==strlen(json) && saved_work==work.used_steps && calls==saved_calls);
    CHECK(strstr(json,"\"runtime_supported\":false") && strstr(json,"\"installation_supported\":false"));
    printf("REPORT %s\n",json);
    mb_dispatch_result_v1 saved=receipt;
    CHECK(mb_core_format_report_json(&receipt,(char *)&receipt,16,&n)==MB_CORE_INVALID_ARGUMENT);
    CHECK(memcmp(&saved,&receipt,sizeof(receipt))==0);
    receipt.installation_supported=1;
    CHECK(mb_core_format_report_json(&receipt,json,sizeof(json),&n)==MB_CORE_INVALID_ARGUMENT && !json[0] && !n);
    receipt=saved; receipt.engine_build.state=MB_FORMAT_SUPPORTED;
    CHECK(mb_core_format_report_json(&receipt,json,sizeof(json),&n)==MB_CORE_INVALID_ARGUMENT);
    mb_probe_input_v1 i=input(); reset_work(100); saved=candidate;
    CHECK(mb_dispatch_probe_v1(&registry,&i,1,&work,(mb_dispatch_result_v1 *)i.tail)==MB_DISPATCH_BAD_ARGUMENT);
    CHECK(memcmp(&saved,&candidate,sizeof(candidate))==0 && !work.used_steps);
    prepare(); REQUIRE(probe(2)); mb_adapter_registry_v1 rs=registry;
    buffers.payload.output=(uint8_t *)&registry; buffers.payload.output_capacity=sizeof(registry);
    CHECK(validate(1,2)==PAK_STATUS_BAD_ARGUMENT && !calls);
    CHECK(memcmp(&registry,&rs,sizeof(registry))==0);
    prepare();
    CHECK(mb_pak_adapter_validate_v1(&registry,&candidate,&source,&limits,1,2,&buffers,&codec_budget,NULL,NULL,
        (mb_dispatch_result_v1 *)buffers.state)==PAK_STATUS_BAD_ARGUMENT && !calls);
}
int main(int argc,char **argv) {
    if(argc!=3) return 2; fixture_root=argv[1]; synthetic_root=argv[2];
    struct group {const char *name; void (*run)(void);} groups[]={
        {"registry",registry_controls},{"probe",probe_controls},{"real_candidate",real_candidates},
        {"binding_failures",binding_failures},{"report_aliases",report_aliases}};
    for(size_t i=0;i<sizeof(groups)/sizeof(groups[0]);++i) { unsigned before=failures;
        groups[i].run(); printf("GROUP %s %s\n",groups[i].name,failures==before ? "PASS" : "FAIL"); }
    printf("checks=%u failures=%u\n",checks,failures); return failures ? 1 : 0;
}
