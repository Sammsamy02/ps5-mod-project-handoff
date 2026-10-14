#include "pak_candidate.h"
#include <stdio.h>
#include <string.h>

/* Dedicated backing: old independent-archive fixture arrays are not resized.
 * Virtual .meta files are exact generated sources, NOT sparse actual archives. */
#define OUTPUT_BYTES ((size_t)4194304)
#define CHUNK_BYTES 65536u
#define LATE_CHUNKS 17u
static unsigned checks, failures, cases;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    printf("FAIL %u: %s\n", (unsigned)__LINE__, #x); } } while (0)

static uint8_t image[2048], reader[65536], primary[2048], secondary[4096];
static uint8_t directory[2048], names[256], physical[1081];
static uint8_t compressed[65536], arena[65536], output[OUTPUT_BYTES+16], seen[2];
static pak_directory_record_v1 directories[2];
static pak_directory_entry_v1 files[2];
static uint32_t starts[2], sort_indices[2];
static pak_graph_extent_v1 extents[2];
static pak_entry_block_v1 decode_blocks[64], selected_blocks[64];
static pak_physical_block_v1 physical_blocks[64];
static bz_state codec_state;
static bz_budget codec_budget;
static pak_candidate_state_v1 state;
static pak_candidate_buffers_v1 buffers;
static pak_candidate_limits_v1 limits;
static pak_candidate_result_v1 result;
static pak_source_v1 source;
static const char *fixture_root;
static size_t image_bytes, header_bytes;
static uint64_t stored_bytes, decoded_bytes, tail_offset, metadata_bytes;
static int virtual_payload;
static uint64_t observed_bytes;
static uint32_t observed_calls, metadata_calls, physical_calls, payload_calls;
static uint64_t trace_sum;
typedef enum { FAULT_NONE, FAULT_SHORT, FAULT_CHANGED, FAULT_CANCELLED, FAULT_OVERREPORT } fault_kind;
static fault_kind fault;
static int corrupt_payload, probe_prefix;
static uint32_t fault_hits, prefix_observed;
static uint64_t work_at_prefix, steps_at_prefix;

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p+4)<<32; }
static void put64(uint8_t *p, uint64_t n) {
    for (unsigned i=0; i<8; ++i) p[i]=(uint8_t)(n>>(8*i));
}
static uint8_t generated_byte(uint64_t i) {
    return (uint8_t)(17u*i+31u*(i>>8)+7u*(i>>16)+3u);
}
static int generated_equal(const uint8_t *p, size_t bytes) {
    for (size_t i=0; i<bytes; ++i) if (p[i]!=generated_byte(i)) return 0;
    return 1;
}
static int all_value(const void *pointer, size_t bytes, uint8_t value) {
    const uint8_t *p=pointer;
    for (size_t i=0; i<bytes; ++i) if (p[i]!=value) return 0;
    return 1;
}

static int load_fixture(const char *name, int generated) {
    char path[2048];
    const int path_bytes=snprintf(path, sizeof(path), "%s/%s", fixture_root, name);
    if (path_bytes<0 || (size_t)path_bytes>=sizeof(path)) return 0;
    FILE *file=fopen(path, "rb");
    if (!file) return 0;
    image_bytes=fread(image, 1, sizeof(image), file);
    const int tail=fgetc(file), bad=ferror(file), close_status=fclose(file);
    if (image_bytes<274 || tail!=EOF || bad || close_status) return 0;
    stored_bytes=le64(image+8); decoded_bytes=le64(image+16);
    header_bytes=le32(image+24)==0 ? 53 : 73;
    if (stored_bytes>UINT64_MAX-header_bytes) return 0;
    tail_offset=stored_bytes+header_bytes;
    virtual_payload=generated;
    if (!generated && tail_offset>image_bytes) return 0;
    metadata_bytes=image_bytes-header_bytes-(generated ? 0 : stored_bytes);
    return metadata_bytes>=221;
}

static pak_provider_status read_fixture(void *context, uint64_t offset, uint8_t *destination,
    uint32_t requested, uint32_t *actual) {
    (void)context;
    CHECK(observed_calls<limits.max_read_calls);
    CHECK(observed_bytes<=limits.max_requested_read_bytes &&
          requested<=limits.max_requested_read_bytes-observed_bytes);
    CHECK(state.ledger.remaining_calls==limits.max_read_calls-observed_calls-1);
    CHECK(state.ledger.remaining_requested_bytes==limits.max_requested_read_bytes-observed_bytes-requested);
    CHECK(requested>0 && requested<=CHUNK_BYTES && requested<=limits.max_read_bytes);
    ++observed_calls; observed_bytes+=requested; trace_sum+=requested;
    *actual=0;
    if (offset>source.logical_size || requested>source.logical_size-offset)
        return PAK_PROVIDER_CONTRACT_ERROR;
    if (offset<header_bytes && requested<=header_bytes-offset) {
        ++physical_calls;
        memcpy(destination, image+(size_t)offset, requested);
    } else if (offset>=tail_offset && offset-tail_offset<=metadata_bytes &&
               requested<=metadata_bytes-(offset-tail_offset)) {
        ++metadata_calls;
        const uint64_t at=virtual_payload ? header_bytes+offset-tail_offset : offset;
        memcpy(destination, image+(size_t)at, requested);
    } else if (offset>=header_bytes && offset-header_bytes<=stored_bytes &&
               requested<=stored_bytes-(offset-header_bytes)) {
        ++payload_calls;
        const uint64_t relative=offset-header_bytes;
        if (probe_prefix && relative==UINT64_C(17)*CHUNK_BYTES) {
            CHECK(generated_equal(output, (size_t)LATE_CHUNKS*CHUNK_BYTES));
            CHECK(!result.metadata_complete && !result.selected_payload_complete && !result.decoded_bytes);
            CHECK(!all_value(files, sizeof(files), 0) && !all_value(directories, sizeof(directories), 0));
            ++prefix_observed; work_at_prefix=codec_budget.work_used; steps_at_prefix=codec_budget.steps_used;
        }
        if (fault!=FAULT_NONE && relative==UINT64_C(17)*CHUNK_BYTES) {
            ++fault_hits;
            if (fault==FAULT_CHANGED) return PAK_PROVIDER_CHANGED;
            if (fault==FAULT_CANCELLED) return PAK_PROVIDER_CANCELLED;
            if (fault==FAULT_OVERREPORT) { *actual=requested+1; return PAK_PROVIDER_OK; }
            for (uint32_t i=0; i<requested-1; ++i) destination[i]=generated_byte(relative+i);
            *actual=requested-1;
            return PAK_PROVIDER_OK;
        }
        if (virtual_payload) {
            for (uint32_t i=0; i<requested; ++i) {
                destination[i]=generated_byte(relative+i);
                if (corrupt_payload && relative+i==stored_bytes-1) destination[i]^=1;
            }
        } else {
            memcpy(destination, image+(size_t)offset, requested);
        }
    } else {
        CHECK(0 && "request must lie in one exact fixture region");
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    *actual=requested;
    return PAK_PROVIDER_OK;
}

static void prepare(void) {
    memset(reader, 0x5a, sizeof(reader)); memset(primary, 0x5a, sizeof(primary));
    memset(secondary, 0x5a, sizeof(secondary)); memset(directory, 0x5a, sizeof(directory));
    memset(names, 0x5a, sizeof(names)); memset(physical, 0x5a, sizeof(physical));
    memset(compressed, 0x5a, sizeof(compressed)); memset(arena, 0x5a, sizeof(arena));
    memset(output, 0xa5, sizeof(output)); memset(directories, 0x5a, sizeof(directories));
    memset(files, 0x5a, sizeof(files)); memset(starts, 0x5a, sizeof(starts));
    memset(sort_indices, 0x5a, sizeof(sort_indices)); memset(extents, 0x5a, sizeof(extents));
    memset(seen, 0x5a, sizeof(seen)); memset(decode_blocks, 0x5a, sizeof(decode_blocks));
    memset(selected_blocks, 0x5a, sizeof(selected_blocks)); memset(physical_blocks, 0x5a, sizeof(physical_blocks));
    memset(&codec_state, 0x5a, sizeof(codec_state)); memset(&state, 0x5a, sizeof(state));
    memset(&result, 0x5a, sizeof(result));
    codec_budget=bz_default_budget();
    /* Existing reservations prove that neither child nor composition resets it. */
    codec_budget.work_used=11; codec_budget.steps_used=3; codec_budget.allocations_used=2;
    pak_candidate_limits_v1_default(&limits);
    observed_bytes=trace_sum=0; observed_calls=metadata_calls=physical_calls=payload_calls=0;
    fault=FAULT_NONE; fault_hits=prefix_observed=0; corrupt_payload=probe_prefix=0;
    work_at_prefix=steps_at_prefix=0;
    source=(pak_source_v1){sizeof(source), PAK_READER_ABI_VERSION, tail_offset+metadata_bytes,
        UINT64_C(0x4c415247454e4f4e), PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE, 0, NULL, read_fixture};
    buffers=(pak_candidate_buffers_v1){0};
    buffers.struct_size=sizeof(buffers); buffers.abi_version=PAK_CANDIDATE_ABI_VERSION; buffers.state=&state;
    buffers.reader_workspace=reader; buffers.reader_workspace_bytes=sizeof(reader);
    buffers.primary_capture=primary; buffers.primary_capacity=sizeof(primary);
    buffers.secondary_workspace=secondary; buffers.secondary_workspace_bytes=sizeof(secondary);
    buffers.directory_capture=directory; buffers.directory_capture_capacity=sizeof(directory);
    buffers.directories=directories; buffers.directories_capacity=2;
    buffers.files=files; buffers.files_capacity=2; buffers.names=names; buffers.names_capacity=sizeof(names);
    buffers.graph=(pak_graph_workspace_v1){sizeof(buffers.graph), PAK_REFERENCE_GRAPH_ABI_VERSION,
        starts, 2, extents, 2, sort_indices, 2, seen, sizeof(seen), decode_blocks, 64, selected_blocks, 64};
    buffers.physical_header=physical; buffers.physical_header_capacity=sizeof(physical);
    buffers.physical_blocks=physical_blocks; buffers.physical_blocks_capacity=64;
    buffers.payload=(pak_reconciled_payload_buffers_v1){compressed, sizeof(compressed),
        arena, sizeof(arena), &codec_state, output, OUTPUT_BYTES};
}

static void scratch_cleared(void) {
    CHECK(all_value(reader, sizeof(reader), 0) && all_value(primary, sizeof(primary), 0));
    CHECK(all_value(secondary, sizeof(secondary), 0) && all_value(directory, sizeof(directory), 0));
    CHECK(all_value(physical, sizeof(physical), 0) && all_value(physical_blocks, sizeof(physical_blocks), 0));
    CHECK(all_value(compressed, sizeof(compressed), 0) && all_value(arena, sizeof(arena), 0));
    CHECK(all_value(&codec_state, sizeof(codec_state), 0) && all_value(&state, sizeof(state), 0));
    CHECK(all_value(starts, sizeof(starts), 0) && all_value(extents, sizeof(extents), 0));
    CHECK(all_value(sort_indices, sizeof(sort_indices), 0) && all_value(seen, sizeof(seen), 0));
    CHECK(all_value(decode_blocks, sizeof(decode_blocks), 0) && all_value(selected_blocks, sizeof(selected_blocks), 0));
    CHECK(all_value(output+OUTPUT_BYTES, 16, 0xa5));
}
static void rollback(void) {
    CHECK(!result.metadata_complete && !result.selected_payload_complete && !result.decoded_bytes);
    CHECK(!result.directory_count && !result.file_count && !result.name_bytes);
    CHECK(all_value(output, OUTPUT_BYTES, 0));
    CHECK(all_value(directories, sizeof(directories), 0) && all_value(files, sizeof(files), 0));
    CHECK(all_value(names, sizeof(names), 0));
    scratch_cleared();
}
static pak_status run_case(const char *name, pak_status expected, pak_candidate_stage stage, int want) {
    const unsigned before=failures;
    const pak_status status=pak_validate_v11_candidate(&source, &limits, (uint32_t)want, 0,
        &buffers, &codec_budget, NULL, NULL, &result);
    CHECK(status==expected && result.status==expected && result.stage==stage);
    CHECK(result.read_calls==observed_calls && result.requested_read_bytes==observed_bytes && trace_sum==observed_bytes);
    CHECK(!result.installation_supported && !result.authenticity_verified &&
          !result.game_acceptance_verified && !result.path_hash_semantics_verified);
    if (expected!=PAK_STATUS_OK) rollback(); else scratch_cleared();
    ++cases;
    printf("case=%s status=%u stage=%u child=%u reason=%u complete=%u decoded=%llu calls=%u bytes=%llu work=%llu steps=%llu case_checks_ok=%s\n",
        name, (unsigned)status, (unsigned)result.stage, result.child_status, result.child_reason,
        result.selected_payload_complete, (unsigned long long)result.decoded_bytes, result.read_calls,
        (unsigned long long)result.requested_read_bytes, (unsigned long long)codec_budget.work_used,
        (unsigned long long)codec_budget.steps_used, before==failures ? "true" : "false");
    return status;
}

static void positives(void) {
    const char *names_list[]={"none-1048576.meta", "none-1048577.meta", "none-2742265.meta", "none-4194304.meta"};
    const uint64_t sizes[]={1048576, 1048577, 2742265, 4194304};
    for (size_t i=0; i<4; ++i) {
        CHECK(load_fixture(names_list[i], 1)); prepare();
        (void)run_case(names_list[i], PAK_STATUS_OK, PAK_CANDIDATE_STAGE_COMPLETE, 1);
        CHECK(result.metadata_complete && result.selected_payload_complete && result.directory_count==1 && result.file_count==1);
        CHECK(result.decoded_bytes==sizes[i] && generated_equal(output, (size_t)sizes[i]));
        CHECK(all_value(output+(size_t)sizes[i], OUTPUT_BYTES-(size_t)sizes[i], 0xa5));
        const uint64_t chunks=(sizes[i]+CHUNK_BYTES-1)/CHUNK_BYTES;
        CHECK(metadata_calls==18 && physical_calls==1 && payload_calls==chunks);
        CHECK(observed_bytes==metadata_bytes+53+sizes[i] && observed_calls==19+chunks);
        CHECK(codec_budget.work_used==11+sizes[i] && codec_budget.steps_used==3+chunks && codec_budget.allocations_used==2);
    }
}

static void limits_and_integrity(void) {
    CHECK(load_fixture("none-4194305.meta", 1)); prepare();
    (void)run_case("above_4MiB_prephysical", PAK_STATUS_RESOURCE_LIMIT, PAK_CANDIDATE_STAGE_SELECTED_EXTENT, 1);
    CHECK(metadata_calls==18 && !physical_calls && !payload_calls && observed_bytes==metadata_bytes);
    CHECK(codec_budget.work_used==11 && codec_budget.steps_used==3);

    CHECK(load_fixture("none-4194305.meta", 1)); prepare();
    (void)run_case("large_metadata_only", PAK_STATUS_OK, PAK_CANDIDATE_STAGE_COMPLETE, 0);
    CHECK(result.metadata_complete && !result.selected_payload_complete && !result.decoded_bytes);
    CHECK(metadata_calls==18 && !physical_calls && !payload_calls && all_value(output, OUTPUT_BYTES, 0));

    CHECK(load_fixture("none-2742265.meta", 1)); prepare(); corrupt_payload=probe_prefix=1;
    (void)run_case("late_payload_corruption", PAK_STATUS_CHECKSUM_MISMATCH, PAK_CANDIDATE_STAGE_PAYLOAD, 1);
    CHECK(result.child_status==PAK_RECONCILED_PAYLOAD_DIGEST_MISMATCH && prefix_observed==1);
    CHECK(codec_budget.work_used==11+stored_bytes && codec_budget.allocations_used==2);
    CHECK(observed_bytes==metadata_bytes+53+stored_bytes);

    CHECK(load_fixture("none-2742265.meta", 1)); image[28]^=1; prepare(); probe_prefix=1;
    (void)run_case("physical_stored_sha1_mutation", PAK_STATUS_CHECKSUM_MISMATCH, PAK_CANDIDATE_STAGE_PAYLOAD, 1);
    CHECK(result.child_status==PAK_RECONCILED_PAYLOAD_DIGEST_MISMATCH && prefix_observed==1);
    CHECK(codec_budget.work_used==11+stored_bytes && observed_bytes==metadata_bytes+53+stored_bytes);

    CHECK(load_fixture("none-2742265.meta", 1)); put64(image+8, stored_bytes-1); put64(image+16, decoded_bytes-1); prepare();
    (void)run_case("physical_index_size_conflict", PAK_STATUS_MALFORMED, PAK_CANDIDATE_STAGE_RECONCILE, 1);
    CHECK(result.child_status==PAK_RECONCILE_HEADER_CONFLICT && physical_calls==1 && !payload_calls);
    CHECK(codec_budget.work_used==11 && observed_bytes==metadata_bytes+53);
}

static void late_failures(void) {
    const fault_kind faults[]={FAULT_SHORT, FAULT_CHANGED, FAULT_CANCELLED, FAULT_OVERREPORT};
    const pak_status statuses[]={PAK_STATUS_PROVIDER_SHORT_READ, PAK_STATUS_PROVIDER_CHANGED, PAK_STATUS_CANCELLED, PAK_STATUS_PROVIDER_CONTRACT};
    const char *labels[]={"late_short", "late_changed", "late_cancelled", "late_overreport"};
    const uint64_t copied=(uint64_t)LATE_CHUNKS*CHUNK_BYTES;
    for (size_t i=0; i<4; ++i) {
        CHECK(load_fixture("none-2742265.meta", 1)); prepare(); fault=faults[i]; probe_prefix=1;
        (void)run_case(labels[i], statuses[i], PAK_CANDIDATE_STAGE_PAYLOAD, 1);
        CHECK(fault_hits==1 && prefix_observed==1 && payload_calls==LATE_CHUNKS+1);
        CHECK(codec_budget.work_used==11+copied && codec_budget.steps_used==3+LATE_CHUNKS);
        CHECK(work_at_prefix==11+copied && steps_at_prefix==3+LATE_CHUNKS);
        CHECK(observed_bytes==metadata_bytes+53+copied+CHUNK_BYTES);
    }
    CHECK(load_fixture("none-2742265.meta", 1)); prepare(); probe_prefix=1; codec_budget.work_limit=11+copied;
    (void)run_case("late_copy_work_budget", PAK_STATUS_RESOURCE_LIMIT, PAK_CANDIDATE_STAGE_PAYLOAD, 1);
    CHECK(prefix_observed==1 && codec_budget.work_used==11+copied && codec_budget.steps_used==3+LATE_CHUNKS);
    CHECK(observed_bytes==metadata_bytes+53+copied+CHUNK_BYTES);

    CHECK(load_fixture("none-2742265.meta", 1)); prepare(); probe_prefix=1; codec_budget.step_limit=3+LATE_CHUNKS;
    (void)run_case("late_copy_step_budget", PAK_STATUS_RESOURCE_LIMIT, PAK_CANDIDATE_STAGE_PAYLOAD, 1);
    CHECK(prefix_observed==1 && codec_budget.work_used==11+copied && codec_budget.steps_used==3+LATE_CHUNKS);
    CHECK(observed_bytes==metadata_bytes+53+copied+CHUNK_BYTES);

    CHECK(load_fixture("none-2742265.meta", 1)); prepare(); limits.max_requested_read_bytes=metadata_bytes+53+copied+CHUNK_BYTES-1;
    (void)run_case("late_aggregate_read_bytes", PAK_STATUS_RESOURCE_LIMIT, PAK_CANDIDATE_STAGE_PAYLOAD, 1);
    CHECK(result.io_cause==PAK_CANDIDATE_IO_BYTE_BUDGET && payload_calls==LATE_CHUNKS);
    CHECK(codec_budget.work_used==11+copied && observed_bytes==metadata_bytes+53+copied);

    CHECK(load_fixture("none-2742265.meta", 1)); prepare(); limits.max_read_calls=19+LATE_CHUNKS;
    (void)run_case("late_aggregate_read_calls", PAK_STATUS_RESOURCE_LIMIT, PAK_CANDIDATE_STAGE_PAYLOAD, 1);
    CHECK(result.io_cause==PAK_CANDIDATE_IO_CALL_BUDGET && payload_calls==LATE_CHUNKS);
    CHECK(codec_budget.work_used==11+copied && observed_bytes==metadata_bytes+53+copied);
}

static void zlib_and_alias(void) {
    CHECK(load_fixture("zlib-stored-over.meta", 1)); prepare();
    (void)run_case("Zlib_stored_block_over_1MiB", PAK_STATUS_MALFORMED, PAK_CANDIDATE_STAGE_RECONCILE, 1);
    CHECK(result.child_status==PAK_RECONCILE_BLOCK_CONFLICT && !payload_calls && physical_calls==1);
    CHECK(codec_budget.work_used==11 && codec_budget.allocations_used==2);
    CHECK(load_fixture("zlib-output-over.meta", 1)); prepare();
    (void)run_case("Zlib_output_block_over_1MiB", PAK_STATUS_RESOURCE_LIMIT, PAK_CANDIDATE_STAGE_REFERENCE_GRAPH, 1);
    CHECK(!payload_calls && !physical_calls && codec_budget.work_used==11);
    CHECK(load_fixture("zlib-small.pak", 0)); prepare();
    (void)run_case("Zlib_small_valid", PAK_STATUS_OK, PAK_CANDIDATE_STAGE_COMPLETE, 1);
    CHECK(result.selected_payload_complete && result.decoded_bytes==32 && generated_equal(output, 32));
    CHECK(codec_budget.allocations_used>2 && payload_calls==1);

    CHECK(load_fixture("none-2742265.meta", 1)); prepare(); buffers.reader_workspace=output;
    CHECK(pak_validate_v11_candidate(&source, &limits, 1, 0, &buffers, &codec_budget, NULL, NULL, &result)==PAK_STATUS_BAD_ARGUMENT);
    CHECK(!observed_calls && all_value(output, sizeof(output), 0xa5) && all_value(reader, sizeof(reader), 0x5a));
    CHECK(all_value(&result, sizeof(result), 0x5a) && all_value(&state, sizeof(state), 0x5a));
    CHECK(codec_budget.work_used==11 && codec_budget.steps_used==3 && codec_budget.allocations_used==2);
    ++cases;
    printf("case=preflight_alias untouched=true calls=%u\n", observed_calls);
}

int main(int argc, char **argv) {
    if (argc!=2) return 2;
    fixture_root=argv[1];
    positives(); limits_and_integrity(); late_failures(); zlib_and_alias();
    printf("large-none cases=%u checks=%u failures=%u synthetic_only=true installation_supported=false\n", cases, checks, failures);
    return failures ? 1 : 0;
}
