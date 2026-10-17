#include "pak_candidate.h"
#include <string.h>

void pak_candidate_limits_v1_default(pak_candidate_limits_v1 *limits)
{
    if (!limits) return;
    memset(limits, 0, sizeof(*limits));
    limits->struct_size = sizeof(*limits); limits->abi_version = 1u;
    limits->profile = PAK_CANDIDATE_PROFILE_CLASSIC_V11_UNENCRYPTED_STORED_SHA1_V1;
    limits->max_read_bytes = PAK_CANDIDATE_MAX_READ_BYTES;
    limits->max_read_calls = PAK_CANDIDATE_MAX_READ_CALLS;
    limits->max_records = PAK_GRAPH_MAX_RECORDS;
    limits->max_directories = PAK_GRAPH_MAX_DIRECTORIES;
    limits->max_name_bytes = PAK_GRAPH_MAX_NAME_BYTES;
    limits->max_graph_steps = PAK_GRAPH_MAX_STEPS;
    limits->cancel_batch_steps = 64u;
    limits->max_requested_read_bytes = PAK_CANDIDATE_MAX_TOTAL_BYTES;
    limits->max_name_comparison_bytes = PAK_GRAPH_MAX_NAME_COMPARISON_BYTES;
}

/* Root-owned composition implementation. Included after behavioral RED. */
typedef struct candidate_span { uintptr_t begin, end; } candidate_span;

static int candidate_add_span(candidate_span *spans, size_t *count,
                              const void *pointer, size_t bytes, size_t alignment)
{
    uintptr_t p = (uintptr_t)pointer;
    size_t i;
    if (!pointer || !bytes || p % alignment || bytes > UINTPTR_MAX - p) return 0;
    for (i = 0; i < *count; ++i)
        if (p < spans[i].end && spans[i].begin < p + bytes) return 0;
    spans[*count].begin = p; spans[*count].end = p + bytes; ++*count;
    return 1;
}

static int candidate_preflight(const pak_source_v1 *source,
    const pak_candidate_limits_v1 *l, uint32_t want, uint32_t selected,
    const pak_candidate_buffers_v1 *b, bz_budget *budget,
    pak_candidate_result_v1 *result)
{
    candidate_span spans[26]; size_t n = 0;
#define OBJECT(ptr, type) candidate_add_span(spans, &n, (ptr), sizeof(type), _Alignof(type))
#define BYTES(ptr, size) candidate_add_span(spans, &n, (ptr), (size), 1u)
#define ARRAY(ptr, count, type) candidate_add_span(spans, &n, (ptr), (count) * sizeof(type), _Alignof(type))
    if (!OBJECT(source, pak_source_v1) || !OBJECT(l, pak_candidate_limits_v1) ||
        !OBJECT(b, pak_candidate_buffers_v1) || !OBJECT(budget, bz_budget) ||
        !OBJECT(result, pak_candidate_result_v1)) return 0;
    if (source->struct_size != sizeof(*source) || source->abi_version != 1u ||
        !source->read_at || !source->logical_size || !source->snapshot_token || source->reserved ||
        source->snapshot_scope != PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE ||
        l->struct_size != sizeof(*l) || l->abi_version != 1u ||
        l->profile != PAK_CANDIDATE_PROFILE_CLASSIC_V11_UNENCRYPTED_STORED_SHA1_V1 ||
        !l->max_read_bytes || l->max_read_bytes > PAK_CANDIDATE_MAX_READ_BYTES ||
        !l->max_read_calls || l->max_read_calls > PAK_CANDIDATE_MAX_READ_CALLS ||
        !l->max_requested_read_bytes || l->max_requested_read_bytes > PAK_CANDIDATE_MAX_TOTAL_BYTES ||
        !l->max_records || l->max_records > PAK_GRAPH_MAX_RECORDS ||
        !l->max_directories || l->max_directories > PAK_GRAPH_MAX_DIRECTORIES ||
        !l->max_name_bytes || l->max_name_bytes > PAK_GRAPH_MAX_NAME_BYTES ||
        !l->max_graph_steps || l->max_graph_steps > PAK_GRAPH_MAX_STEPS ||
        !l->max_name_comparison_bytes || l->max_name_comparison_bytes > PAK_GRAPH_MAX_NAME_COMPARISON_BYTES ||
        !l->cancel_batch_steps || l->cancel_batch_steps > 256u || want > 1u || (!want && selected) ||
        b->struct_size != sizeof(*b) || b->abi_version != 1u ||
        budget->work_used > budget->work_limit || budget->steps_used > budget->step_limit ||
        budget->allocations_used > budget->allocation_limit) return 0;
    if (b->reader_workspace_bytes != 65536u || b->secondary_workspace_bytes != 4096u ||
        !b->primary_capacity || b->primary_capacity > 4194304u ||
        !b->directory_capture_capacity || b->directory_capture_capacity > 4194304u ||
        !b->directories_capacity || b->directories_capacity > 65536u ||
        !b->files_capacity || b->files_capacity > 65536u ||
        !b->names_capacity || b->names_capacity > 4194304u ||
        b->graph.struct_size != sizeof(b->graph) || b->graph.abi_version != 1u ||
        !b->graph.starts_capacity || b->graph.starts_capacity > 65536u ||
        !b->graph.extents_capacity || b->graph.extents_capacity > 65536u ||
        !b->graph.sort_capacity || b->graph.sort_capacity > 65536u ||
        !b->graph.seen_bytes || b->graph.seen_bytes > PAK_GRAPH_MAX_SEEN_BYTES ||
        !b->graph.decode_blocks_capacity || b->graph.decode_blocks_capacity > 64u ||
        !b->graph.selected_blocks_capacity || b->graph.selected_blocks_capacity > 64u ||
        b->physical_header_capacity < 53u || b->physical_header_capacity > PAK_PHYSICAL_HEADER_MAX_BYTES ||
        !b->physical_blocks_capacity || b->physical_blocks_capacity > 64u ||
        !b->payload.compressed_bytes || b->payload.compressed_bytes > BZ_MAX_BLOCK_BYTES ||
        !b->payload.codec_arena_bytes || b->payload.codec_arena_bytes > BZ_MAX_ARENA_BYTES ||
        !b->payload.output_capacity || b->payload.output_capacity > PAK_RECONCILED_PAYLOAD_MAX_OUTPUT_BYTES)
        return 0;
    if (!OBJECT(b->state, pak_candidate_state_v1) ||
        !BYTES(b->reader_workspace, b->reader_workspace_bytes) ||
        !BYTES(b->primary_capture, b->primary_capacity) ||
        !BYTES(b->secondary_workspace, b->secondary_workspace_bytes) ||
        !BYTES(b->directory_capture, b->directory_capture_capacity) ||
        !ARRAY(b->directories, b->directories_capacity, pak_directory_record_v1) ||
        !ARRAY(b->files, b->files_capacity, pak_directory_entry_v1) ||
        !BYTES(b->names, b->names_capacity) ||
        !ARRAY(b->graph.starts, b->graph.starts_capacity, uint32_t) ||
        !ARRAY(b->graph.extents, b->graph.extents_capacity, pak_graph_extent_v1) ||
        !ARRAY(b->graph.sort_indices, b->graph.sort_capacity, uint32_t) ||
        !BYTES(b->graph.seen, b->graph.seen_bytes) ||
        !ARRAY(b->graph.decode_blocks, b->graph.decode_blocks_capacity, pak_entry_block_v1) ||
        !ARRAY(b->graph.selected_blocks, b->graph.selected_blocks_capacity, pak_entry_block_v1) ||
        !BYTES(b->physical_header, b->physical_header_capacity) ||
        !ARRAY(b->physical_blocks, b->physical_blocks_capacity, pak_physical_block_v1) ||
        !BYTES(b->payload.compressed, b->payload.compressed_bytes) ||
        !BYTES(b->payload.codec_arena, b->payload.codec_arena_bytes) ||
        !OBJECT(b->payload.codec_state, bz_state) ||
        !BYTES(b->payload.output, b->payload.output_capacity)) return 0;
#undef OBJECT
#undef BYTES
#undef ARRAY
    return 1;
}

static void candidate_erase(void *pointer, size_t bytes)
{
    volatile uint8_t *p = (volatile uint8_t *)pointer;
    while (bytes--) *p++ = 0u;
}

static void candidate_cleanup(const pak_candidate_buffers_v1 *b, int success, int selected)
{
    candidate_erase(b->reader_workspace, b->reader_workspace_bytes);
    candidate_erase(b->primary_capture, b->primary_capacity);
    candidate_erase(b->secondary_workspace, b->secondary_workspace_bytes);
    candidate_erase(b->directory_capture, b->directory_capture_capacity);
    candidate_erase(b->graph.starts, b->graph.starts_capacity * sizeof(*b->graph.starts));
    candidate_erase(b->graph.extents, b->graph.extents_capacity * sizeof(*b->graph.extents));
    candidate_erase(b->graph.sort_indices, b->graph.sort_capacity * sizeof(*b->graph.sort_indices));
    candidate_erase(b->graph.seen, b->graph.seen_bytes);
    candidate_erase(b->graph.decode_blocks, b->graph.decode_blocks_capacity * sizeof(*b->graph.decode_blocks));
    candidate_erase(b->graph.selected_blocks, b->graph.selected_blocks_capacity * sizeof(*b->graph.selected_blocks));
    candidate_erase(b->physical_header, b->physical_header_capacity);
    candidate_erase(b->physical_blocks, b->physical_blocks_capacity * sizeof(*b->physical_blocks));
    candidate_erase(b->payload.compressed, b->payload.compressed_bytes);
    candidate_erase(b->payload.codec_arena, b->payload.codec_arena_bytes);
    candidate_erase(b->payload.codec_state, sizeof(*b->payload.codec_state));
    if (!success) {
        candidate_erase(b->directories, b->directories_capacity * sizeof(*b->directories));
        candidate_erase(b->files, b->files_capacity * sizeof(*b->files));
        candidate_erase(b->names, b->names_capacity);
    }
    if (!success || !selected) candidate_erase(b->payload.output, b->payload.output_capacity);
    candidate_erase(b->state, sizeof(*b->state));
}

static pak_status candidate_graph_status(pak_graph_status status)
{
    switch (status) {
        case PAK_GRAPH_OK: return PAK_STATUS_OK;
        case PAK_GRAPH_UNSUPPORTED: return PAK_STATUS_UNSUPPORTED;
        case PAK_GRAPH_MALFORMED: return PAK_STATUS_MALFORMED;
        case PAK_GRAPH_RESOURCE_LIMIT: return PAK_STATUS_RESOURCE_LIMIT;
        case PAK_GRAPH_BAD_ARGUMENT: return PAK_STATUS_BAD_ARGUMENT;
        case PAK_GRAPH_CANCELLED: return PAK_STATUS_CANCELLED;
        default: return PAK_STATUS_INTERNAL_ERROR;
    }
}

static pak_status candidate_payload_status(pak_reconciled_payload_status status)
{
    switch (status) {
        case PAK_RECONCILED_PAYLOAD_OK: return PAK_STATUS_OK;
        case PAK_RECONCILED_PAYLOAD_BAD_ARGUMENT: return PAK_STATUS_BAD_ARGUMENT;
        case PAK_RECONCILED_PAYLOAD_UNSUPPORTED: return PAK_STATUS_UNSUPPORTED;
        case PAK_RECONCILED_PAYLOAD_MALFORMED: return PAK_STATUS_MALFORMED;
        case PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT: return PAK_STATUS_RESOURCE_LIMIT;
        case PAK_RECONCILED_PAYLOAD_PROVIDER_ERROR: return PAK_STATUS_PROVIDER_ERROR;
        case PAK_RECONCILED_PAYLOAD_SHORT_READ: return PAK_STATUS_PROVIDER_SHORT_READ;
        case PAK_RECONCILED_PAYLOAD_SOURCE_CHANGED: return PAK_STATUS_PROVIDER_CHANGED;
        case PAK_RECONCILED_PAYLOAD_CANCELLED: return PAK_STATUS_CANCELLED;
        case PAK_RECONCILED_PAYLOAD_PROVIDER_CONTRACT: return PAK_STATUS_PROVIDER_CONTRACT;
        case PAK_RECONCILED_PAYLOAD_DIGEST_MISMATCH: return PAK_STATUS_CHECKSUM_MISMATCH;
        case PAK_RECONCILED_PAYLOAD_CODEC_REJECTED: return PAK_STATUS_MALFORMED;
        default: return PAK_STATUS_INTERNAL_ERROR;
    }
}

static void candidate_charged_provider(pak_candidate_result_v1 *r, const pak_candidate_state_v1 *s)
{
    if (s->charging.provider_observed) {
        r->provider_observed = 1u;
        r->last_provider_status = s->charging.last_provider_status;
        r->last_requested = s->charging.last_requested;
        r->last_actual = s->charging.last_actual;
    }
}

static void candidate_secondary_provider(pak_candidate_result_v1 *r, const pak_candidate_state_v1 *s)
{
    if (s->secondary.provider_observed) {
        r->provider_observed = 1u;
        r->last_provider_status = s->secondary.last_provider_status;
        r->last_requested = s->secondary.last_requested;
        r->last_actual = s->secondary.last_actual;
    }
}

static int candidate_overlap(uint64_t a, uint64_t end, uint64_t b, uint64_t bytes)
{
    /* All ranges were checked by the primary/entry readers before this call. */
    return a < b + bytes && b < end;
}

static int candidate_selected_extent(const pak_source_v1 *source,
    const pak_candidate_state_v1 *s, const pak_candidate_buffers_v1 *b,
    pak_graph_cancel_fn cancelled, void *cancel_context)
{
    const pak_primary_result_v1 *p = &s->primary.primary;
    const pak_graph_extent_v1 *selected = &b->graph.extents[s->graph.selected_record_index];
    uint32_t i;
    if (selected->archive_start >= selected->archive_end || selected->archive_end > source->logical_size ||
        source->logical_size < PAK_V11_FOOTER_BYTES ||
        candidate_overlap(selected->archive_start, selected->archive_end,
                          source->logical_size - PAK_V11_FOOTER_BYTES, PAK_V11_FOOTER_BYTES) ||
        candidate_overlap(selected->archive_start, selected->archive_end, p->primary_index_offset, p->primary_index_size) ||
        candidate_overlap(selected->archive_start, selected->archive_end, p->path_hash_index.offset, p->path_hash_index.size) ||
        candidate_overlap(selected->archive_start, selected->archive_end, p->directory_index.offset, p->directory_index.size))
        return 0;
    for (i = 0; i < s->graph.record_count; ++i) {
        const pak_graph_extent_v1 *other = &b->graph.extents[i];
        if (i % 64u == 0u && cancelled && cancelled(cancel_context)) return -1;
        if (i != s->graph.selected_record_index &&
            selected->archive_start < other->archive_end && other->archive_start < selected->archive_end)
            return 0;
    }
    return 1;
}

pak_status pak_validate_v11_candidate(
    const pak_source_v1 *source, const pak_candidate_limits_v1 *limits,
    uint32_t want_selected, uint32_t selected_file_index,
    const pak_candidate_buffers_v1 *b, bz_budget *codec_budget,
    pak_graph_cancel_fn cancelled, void *cancel_context,
    pak_candidate_result_v1 *r)
{
    pak_candidate_state_v1 *s;
    pak_status status = PAK_STATUS_INTERNAL_ERROR;
    uint32_t actual = 0u, header_copied = 0u;
    pak_provider_status provider;
    if (!candidate_preflight(source, limits, want_selected, selected_file_index, b, codec_budget, r))
        return PAK_STATUS_BAD_ARGUMENT;
    memset(r, 0, sizeof(*r)); memset(b->state, 0, sizeof(*b->state));
    r->struct_size = sizeof(*r); r->abi_version = 1u; r->profile = limits->profile;
    r->snapshot_token = source->snapshot_token;
    s = b->state;
    s->ledger.struct_size = sizeof(s->ledger); s->ledger.abi_version = 1u;
    s->ledger.remaining_requested_bytes = limits->max_requested_read_bytes;
    s->ledger.remaining_calls = limits->max_read_calls;
    s->charging.base = source; s->charging.budget = &s->ledger;
    s->charging.max_read_bytes = limits->max_read_bytes;
    s->charged_source = *source; s->charged_source.context = &s->charging;
    s->charged_source.read_at = pak_candidate_charging_read_at;
    pak_limits_v1_default(&s->reader_limits);
    s->reader_limits.max_read_bytes = limits->max_read_bytes;
    s->reader_limits.max_declared_files = limits->max_records;
    pak_secondary_limits_v1_default(&s->secondary_limits);
    if (s->secondary_limits.max_read_bytes > limits->max_read_bytes)
        s->secondary_limits.max_read_bytes = limits->max_read_bytes;
    pak_directory_limits_v1_default(&s->directory_limits);
    s->directory_limits.max_directories = limits->max_directories;
    s->directory_limits.max_entries = limits->max_records;
    s->directory_limits.max_total_name_bytes = limits->max_name_bytes;
    s->entry_limits.struct_size = sizeof(s->entry_limits); s->entry_limits.abi_version = 1u;
    s->entry_limits.max_blob_bytes = 4194304u; s->entry_limits.max_record_bytes = 4096u;
    s->entry_limits.max_blocks = 64u; s->entry_limits.max_block_size = 1048576u;
    /* Metadata extents describe stored bytes; they do not allocate payloads. */
    s->entry_limits.max_compressed_size = source->logical_size < (uint64_t)INT64_MAX
        ? source->logical_size : (uint64_t)INT64_MAX;
    s->entry_limits.max_uncompressed_size = (uint64_t)INT64_MAX;
    s->entry_limits.max_total_block_bytes = s->entry_limits.max_compressed_size;
    pak_graph_limits_v1_default(&s->graph_limits);
    s->graph_limits.max_records = limits->max_records;
    s->graph_limits.max_directories = limits->max_directories;
    s->graph_limits.max_name_bytes = limits->max_name_bytes;
    s->graph_limits.max_steps = limits->max_graph_steps;
    s->graph_limits.max_name_comparison_bytes = limits->max_name_comparison_bytes;
    s->graph_limits.cancel_batch_steps = limits->cancel_batch_steps;
    s->physical_limits.struct_size = sizeof(s->physical_limits); s->physical_limits.abi_version = 1u;
    s->physical_limits.max_header_bytes = PAK_PHYSICAL_HEADER_MAX_BYTES;
    s->physical_limits.max_blocks = 64u; s->physical_limits.max_physical_block_size = 1048576u;
    s->physical_limits.max_compressed_size = s->physical_limits.max_uncompressed_size = 4194304u;
    pak_reconciled_payload_limits_v1_default(&s->payload_limits);
    s->payload_limits.max_read_bytes = limits->max_read_bytes;

#define START_STAGE(stage_name) do { r->stage = (stage_name); \
    if (cancelled && cancelled(cancel_context)) { status = PAK_STATUS_CANCELLED; goto done; } \
    } while (0)
    START_STAGE(PAK_CANDIDATE_STAGE_PRIMARY);
    s->primary.status = pak_capture_v11_primary(&s->charged_source, &s->reader_limits,
        b->reader_workspace, b->reader_workspace_bytes, b->primary_capture, b->primary_capacity, &s->primary);
    candidate_charged_provider(r, s);
    r->child_status = s->primary.status; r->child_reason = s->primary.reason;
    if (s->primary.status != PAK_PRIMARY_CAPTURE_OK || !s->primary.primary_validated ||
        !s->primary.primary_capture_complete || !s->primary.encoded_span_located) {
        status = s->primary.status == PAK_PRIMARY_CAPTURE_READER_REJECTED ? s->primary.reader_status :
            s->primary.status == PAK_PRIMARY_CAPTURE_UNSUPPORTED ? PAK_STATUS_UNSUPPORTED : PAK_STATUS_MALFORMED;
        if (status == PAK_STATUS_OK) status = PAK_STATUS_INTERNAL_ERROR;
        goto done;
    }
    START_STAGE(PAK_CANDIDATE_STAGE_PATH_SECONDARY);
    status = pak_verify_v11_secondary_raw_sha1(source, &s->primary.primary.path_hash_index,
        &s->secondary_limits, &s->ledger, b->secondary_workspace, b->secondary_workspace_bytes, &s->secondary);
    candidate_secondary_provider(r, s);
    r->child_status = s->secondary.status; r->child_reason = s->secondary.reason;
    if (status != PAK_STATUS_OK) goto done;
    if (!s->secondary.span_complete || !s->secondary.digest_valid) { status = PAK_STATUS_INTERNAL_ERROR; goto done; }
    START_STAGE(PAK_CANDIDATE_STAGE_DIRECTORY_SECONDARY);
    if (s->primary.primary.directory_index.size > b->directory_capture_capacity) {
        status = PAK_STATUS_RESOURCE_LIMIT; goto done;
    }
    s->tee.raw_base = source; s->tee.span_offset = s->primary.primary.directory_index.offset;
    s->tee.span_bytes = s->primary.primary.directory_index.size;
    s->tee.capture = b->directory_capture; s->tee.capture_capacity = b->directory_capture_capacity;
    s->tee_source = *source; s->tee_source.context = &s->tee;
    s->tee_source.read_at = pak_candidate_directory_tee_read_at;
    status = pak_verify_v11_secondary_raw_sha1(&s->tee_source, &s->primary.primary.directory_index,
        &s->secondary_limits, &s->ledger, b->secondary_workspace, b->secondary_workspace_bytes, &s->secondary);
    candidate_secondary_provider(r, s);
    r->child_status = s->secondary.status; r->child_reason = s->secondary.reason;
    if (status != PAK_STATUS_OK) goto done;
    if (!s->secondary.span_complete || !s->secondary.digest_valid || s->tee.copied != s->tee.span_bytes) {
        status = PAK_STATUS_INTERNAL_ERROR; goto done;
    }
    START_STAGE(PAK_CANDIDATE_STAGE_DIRECTORY_PARSE);
    s->directory.status = pak_directory_read_classic_v11(b->directory_capture, (size_t)s->tee.span_bytes,
        s->primary.encoded_span_bytes, &s->directory_limits, b->directories, b->directories_capacity,
        b->files, b->files_capacity, b->names, b->names_capacity, &s->directory);
    r->child_status = s->directory.status; r->child_reason = s->directory.reason;
    if (s->directory.status != PAK_DIRECTORY_STATUS_OK || !s->directory.metadata_complete) {
        status = s->directory.status == PAK_DIRECTORY_STATUS_RESOURCE_LIMIT ? PAK_STATUS_RESOURCE_LIMIT :
            s->directory.status == PAK_DIRECTORY_STATUS_UNSUPPORTED ? PAK_STATUS_UNSUPPORTED : PAK_STATUS_MALFORMED;
        goto done;
    }
    START_STAGE(PAK_CANDIDATE_STAGE_REFERENCE_GRAPH);
    s->graph_input.struct_size = sizeof(s->graph_input); s->graph_input.abi_version = 1u;
    s->graph_input.primary_file_count = s->primary.primary.declared_file_count;
    s->graph_input.directory_count = s->directory.directory_count;
    s->graph_input.file_count = s->directory.entry_count;
    s->graph_input.names_bytes = s->directory.string_bytes;
    s->graph_input.logical_archive_size = source->logical_size;
    s->graph_input.encoded_blob = b->primary_capture + s->primary.encoded_span_offset;
    s->graph_input.encoded_blob_bytes = s->primary.encoded_span_bytes;
    s->graph_input.methods = &s->primary.method_table;
    s->graph_input.entry_limits = &s->entry_limits;
    s->graph_input.directories = b->directories; s->graph_input.files = b->files; s->graph_input.names = b->names;
    s->graph_input.want_selected = want_selected; s->graph_input.selected_file_index = selected_file_index;
    s->graph.status = pak_validate_v11_reference_graph(&s->graph_input, &s->graph_limits, &b->graph,
        cancelled, cancel_context, &s->graph);
    r->graph_steps = s->graph.steps_used; r->name_comparison_bytes = s->graph.name_comparison_bytes;
    r->child_status = s->graph.status; r->child_reason = s->graph.reason;
    status = candidate_graph_status(s->graph.status);
    if (status != PAK_STATUS_OK) goto done;
    if (!s->graph.metadata_complete || (want_selected && !s->graph.selected_complete)) {
        status = PAK_STATUS_INTERNAL_ERROR; goto done;
    }
    if (!want_selected) goto commit;
    START_STAGE(PAK_CANDIDATE_STAGE_SELECTED_EXTENT);
    /* Keep the original selected-payload cap before any physical-header I/O. */
    if (s->graph.selected_entry.compressed_size > s->physical_limits.max_compressed_size ||
        s->graph.selected_entry.uncompressed_size > s->physical_limits.max_uncompressed_size) {
        status = PAK_STATUS_RESOURCE_LIMIT; goto done;
    }
    {
        int extent_ok = candidate_selected_extent(source, s, b, cancelled, cancel_context);
        if (extent_ok < 0) { status = PAK_STATUS_CANCELLED; goto done; }
        if (!extent_ok) { status = PAK_STATUS_MALFORMED; goto done; }
    }
    START_STAGE(PAK_CANDIDATE_STAGE_PHYSICAL_READ);
    if (s->graph.selected_entry.header_bytes > b->physical_header_capacity) {
        status = PAK_STATUS_RESOURCE_LIMIT; goto done;
    }
    while (header_copied < s->graph.selected_entry.header_bytes) {
        uint32_t request = s->graph.selected_entry.header_bytes - header_copied;
        if (request > limits->max_read_bytes) request = limits->max_read_bytes;
        if (cancelled && cancelled(cancel_context)) { status = PAK_STATUS_CANCELLED; goto done; }
        provider = pak_candidate_charging_read_at(&s->charging,
            s->graph.selected_entry.archive_offset + header_copied,
            b->physical_header + header_copied, request, &actual);
        candidate_charged_provider(r, s); r->child_status = provider; r->child_reason = 0u;
        switch (provider) {
            case PAK_PROVIDER_OK: status = actual == request ? PAK_STATUS_OK : PAK_STATUS_PROVIDER_SHORT_READ; break;
            case PAK_PROVIDER_CHANGED: status = PAK_STATUS_PROVIDER_CHANGED; break;
            case PAK_PROVIDER_CANCELLED: status = PAK_STATUS_CANCELLED; break;
            case PAK_PROVIDER_IO_ERROR: case PAK_PROVIDER_UNAVAILABLE: status = PAK_STATUS_PROVIDER_ERROR; break;
            default: status = PAK_STATUS_PROVIDER_CONTRACT; break;
        }
        if (status != PAK_STATUS_OK) goto done;
        header_copied += actual;
    }
    START_STAGE(PAK_CANDIDATE_STAGE_PHYSICAL_PARSE);
    s->physical.status = pak_physical_header_parse_v11(b->physical_header, header_copied, &s->primary.method_table,
        &s->physical_limits, b->physical_blocks, b->physical_blocks_capacity, &s->physical);
    r->child_status = s->physical.status; r->child_reason = s->physical.reason;
    if (s->physical.status != PAK_PHYSICAL_STATUS_OK) {
        status = s->physical.status == PAK_PHYSICAL_STATUS_RESOURCE_LIMIT ? PAK_STATUS_RESOURCE_LIMIT :
            s->physical.status == PAK_PHYSICAL_STATUS_UNSUPPORTED ? PAK_STATUS_UNSUPPORTED : PAK_STATUS_MALFORMED;
        goto done;
    }
    START_STAGE(PAK_CANDIDATE_STAGE_RECONCILE);
    r->child_status = pak_candidate_reconcile_v11(&s->graph.selected_entry,
        b->graph.selected_blocks, s->graph.selected_entry.block_count, &s->physical,
        b->physical_blocks, s->physical.block_count, source->logical_size);
    r->child_reason = 0u;
    if (r->child_status != PAK_RECONCILE_OK) {
        status = r->child_status == PAK_RECONCILE_UNSUPPORTED ? PAK_STATUS_UNSUPPORTED : PAK_STATUS_MALFORMED;
        goto done;
    }
    START_STAGE(PAK_CANDIDATE_STAGE_PAYLOAD);
    s->payload = pak_decode_reconciled_v11_payload(&s->charged_source, &s->graph.selected_entry,
        b->graph.selected_blocks, s->graph.selected_entry.block_count, s->physical.sha1,
        &s->payload_limits, b->payload, codec_budget);
    candidate_charged_provider(r, s);
    r->child_status = s->payload.status; r->child_reason = s->payload.codec_status;
    status = candidate_payload_status(s->payload.status);
    if (status != PAK_STATUS_OK) goto done;
    if (!s->payload.complete || !s->payload.digest_valid) { status = PAK_STATUS_INTERNAL_ERROR; goto done; }

commit:
    if (cancelled && cancelled(cancel_context)) { status = PAK_STATUS_CANCELLED; goto done; }
    status = PAK_STATUS_OK; r->stage = PAK_CANDIDATE_STAGE_COMPLETE;
    r->child_status = r->child_reason = 0u; r->metadata_complete = 1u;
    r->directory_count = s->directory.directory_count; r->file_count = s->directory.entry_count;
    r->name_bytes = s->directory.string_bytes;
    if (want_selected) {
        r->selected_payload_complete = 1u; r->selected_file_index = selected_file_index;
        r->selected_directory_index = s->graph.selected_directory_index;
        r->decoded_bytes = s->payload.decoded_bytes;
    }
done:
    r->io_cause = s->charging.cause != PAK_CANDIDATE_IO_NONE ? s->charging.cause : s->tee.cause;
    if (s->charging.cause == PAK_CANDIDATE_IO_BYTE_BUDGET ||
        s->charging.cause == PAK_CANDIDATE_IO_CALL_BUDGET) status = PAK_STATUS_RESOURCE_LIMIT;
    r->status = status;
    r->requested_read_bytes = limits->max_requested_read_bytes - s->ledger.remaining_requested_bytes;
    r->read_calls = limits->max_read_calls - s->ledger.remaining_calls;
    candidate_cleanup(b, status == PAK_STATUS_OK, want_selected != 0u);
    return status;
#undef START_STAGE
}
