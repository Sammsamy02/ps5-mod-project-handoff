#include "pak_reference_graph.h"
#include <limits.h>
#include <string.h>

typedef struct memory_span { const void *data; size_t bytes; } memory_span;
typedef struct graph_state {
    const pak_graph_input_v1 *in;
    const pak_graph_limits_v1 *limits;
    const pak_graph_workspace_v1 *ws;
    pak_graph_cancel_fn cancelled;
    void *cancel_context;
    uint64_t steps, compared, next_poll;
    pak_graph_status status;
    pak_graph_reason reason;
    pak_entry_status decoder_status;
    pak_entry_reason decoder_reason;
} graph_state;

static int span_valid(memory_span span)
{
    return span.bytes == 0 || (span.data != NULL && span.bytes - 1 <= UINTPTR_MAX - (uintptr_t)span.data);
}

static int overlap(memory_span a, memory_span b)
{
    uintptr_t left = (uintptr_t)a.data, right = (uintptr_t)b.data;
    if (a.bytes == 0 || b.bytes == 0) return 0;
    return left <= right ? right - left < a.bytes : left - right < b.bytes;
}

static int valid_methods(const pak_entry_method_table_v1 *t)
{
    uint32_t i, j;
    if (!t || t->struct_size != sizeof(*t) || t->abi_version != PAK_ENTRY_DECODER_ABI_VERSION ||
        t->reserved || !t->count || t->count > PAK_ENTRY_DECODER_MAX_METHODS ||
        t->methods[0].length != 4 || memcmp(t->methods[0].bytes, "None", 4)) return 0;
    for (i = 0; i < t->count; ++i) {
        const pak_entry_method_name *m = &t->methods[i];
        if (!m->length || m->length > 32 || m->reserved[0] || m->reserved[1] || m->reserved[2]) return 0;
        for (j = 0; j < m->length; ++j) if (m->bytes[j] < 0x21 || m->bytes[j] > 0x7e) return 0;
    }
    return 1;
}

static int valid_entry_limits(const pak_entry_limits_v1 *l)
{
    return l && l->struct_size == sizeof(*l) && l->abi_version == PAK_ENTRY_DECODER_ABI_VERSION &&
        l->max_blob_bytes && l->max_blob_bytes <= PAK_ENTRY_DECODER_MAX_BLOB_BYTES &&
        l->max_record_bytes >= 4 && l->max_record_bytes <= l->max_blob_bytes &&
        l->max_blocks <= PAK_ENTRY_DECODER_MAX_BLOCKS && l->max_compressed_size <= INT64_MAX &&
        l->max_uncompressed_size <= INT64_MAX && l->max_total_block_bytes <= INT64_MAX;
}

static int preflight(const pak_graph_input_v1 *in, const pak_graph_limits_v1 *l,
                      const pak_graph_workspace_v1 *w, pak_graph_result_v1 *result)
{
    memory_span inputs[9], outputs[7];
    size_t i, j;
    if (!in || !l || !w || !result ||
        !span_valid((memory_span){in, sizeof(*in)}) || !span_valid((memory_span){l, sizeof(*l)}) ||
        !span_valid((memory_span){w, sizeof(*w)}) || !span_valid((memory_span){result, sizeof(*result)})) return 0;
    if (in->struct_size != sizeof(*in) || in->abi_version != PAK_REFERENCE_GRAPH_ABI_VERSION ||
        in->reserved[0] || in->reserved[1] || in->primary_file_count > PAK_GRAPH_MAX_RECORDS ||
        in->file_count > PAK_GRAPH_MAX_RECORDS || in->directory_count > PAK_GRAPH_MAX_DIRECTORIES ||
        in->names_bytes > PAK_GRAPH_MAX_NAME_BYTES || in->encoded_blob_bytes > PAK_GRAPH_MAX_BLOB_BYTES ||
        !in->logical_archive_size || in->logical_archive_size > INT64_MAX || in->want_selected > 1 ||
        (in->want_selected ? in->selected_file_index >= in->file_count : in->selected_file_index != 0)) return 0;
    if (l->struct_size != sizeof(*l) || l->abi_version != PAK_REFERENCE_GRAPH_ABI_VERSION || l->reserved ||
        !l->max_records || l->max_records > PAK_GRAPH_MAX_RECORDS ||
        !l->max_directories || l->max_directories > PAK_GRAPH_MAX_DIRECTORIES ||
        !l->max_name_bytes || l->max_name_bytes > PAK_GRAPH_MAX_NAME_BYTES ||
        !l->max_steps || l->max_steps > PAK_GRAPH_MAX_STEPS ||
        !l->max_name_comparison_bytes || l->max_name_comparison_bytes > PAK_GRAPH_MAX_NAME_COMPARISON_BYTES ||
        !l->cancel_batch_steps || l->cancel_batch_steps > 256) return 0;
    if (w->struct_size != sizeof(*w) || w->abi_version != PAK_REFERENCE_GRAPH_ABI_VERSION ||
        w->starts_capacity > PAK_GRAPH_MAX_RECORDS || w->extents_capacity > PAK_GRAPH_MAX_RECORDS ||
        w->sort_capacity > PAK_GRAPH_MAX_RECORDS || w->seen_bytes > PAK_GRAPH_MAX_SEEN_BYTES ||
        w->decode_blocks_capacity > PAK_ENTRY_DECODER_MAX_BLOCKS ||
        w->selected_blocks_capacity > PAK_ENTRY_DECODER_MAX_BLOCKS) return 0;

    inputs[0] = (memory_span){in, sizeof(*in)};
    inputs[1] = (memory_span){l, sizeof(*l)};
    inputs[2] = (memory_span){w, sizeof(*w)};
    inputs[3] = (memory_span){in->methods, sizeof(*in->methods)};
    inputs[4] = (memory_span){in->entry_limits, sizeof(*in->entry_limits)};
    inputs[5] = (memory_span){in->encoded_blob, in->encoded_blob_bytes};
    inputs[6] = (memory_span){in->directories, (size_t)in->directory_count * sizeof(*in->directories)};
    inputs[7] = (memory_span){in->files, (size_t)in->file_count * sizeof(*in->files)};
    inputs[8] = (memory_span){in->names, in->names_bytes};
    outputs[0] = (memory_span){result, sizeof(*result)};
    outputs[1] = (memory_span){w->starts, w->starts_capacity * sizeof(*w->starts)};
    outputs[2] = (memory_span){w->extents, w->extents_capacity * sizeof(*w->extents)};
    outputs[3] = (memory_span){w->sort_indices, w->sort_capacity * sizeof(*w->sort_indices)};
    outputs[4] = (memory_span){w->seen, w->seen_bytes};
    outputs[5] = (memory_span){w->decode_blocks, w->decode_blocks_capacity * sizeof(*w->decode_blocks)};
    outputs[6] = (memory_span){w->selected_blocks, w->selected_blocks_capacity * sizeof(*w->selected_blocks)};
    for (i = 0; i < 9; ++i) if (!span_valid(inputs[i])) return 0;
    for (i = 0; i < 7; ++i) {
        if (!span_valid(outputs[i])) return 0;
        for (j = 0; j < 9; ++j) if (overlap(outputs[i], inputs[j])) return 0;
        for (j = 0; j < i; ++j) if (overlap(outputs[i], outputs[j])) return 0;
    }
    return valid_methods(in->methods) && valid_entry_limits(in->entry_limits);
}

static void clear_workspace(const pak_graph_workspace_v1 *w)
{
    if (w->starts_capacity) memset(w->starts, 0, w->starts_capacity * sizeof(*w->starts));
    if (w->extents_capacity) memset(w->extents, 0, w->extents_capacity * sizeof(*w->extents));
    if (w->sort_capacity) memset(w->sort_indices, 0, w->sort_capacity * sizeof(*w->sort_indices));
    if (w->seen_bytes) memset(w->seen, 0, w->seen_bytes);
    if (w->decode_blocks_capacity) memset(w->decode_blocks, 0, w->decode_blocks_capacity * sizeof(*w->decode_blocks));
    if (w->selected_blocks_capacity) memset(w->selected_blocks, 0, w->selected_blocks_capacity * sizeof(*w->selected_blocks));
}

static int reject(graph_state *s, pak_graph_status status, pak_graph_reason reason)
{
    s->status = status; s->reason = reason; return 0;
}

static int poll_cancel(graph_state *s)
{
    if (s->cancelled && s->cancelled(s->cancel_context))
        return reject(s, PAK_GRAPH_CANCELLED, PAK_GRAPH_REASON_CANCELLED);
    return 1;
}

static int steps(graph_state *s, uint64_t cost)
{
    if (cost > s->limits->max_steps - s->steps) return reject(s, PAK_GRAPH_RESOURCE_LIMIT, PAK_GRAPH_REASON_STEPS);
    s->steps += cost;
    if (s->steps >= s->next_poll) {
        s->next_poll = s->steps + s->limits->cancel_batch_steps;
        return poll_cancel(s);
    }
    return 1;
}

static int name_view(graph_state *s, uint32_t offset, uint32_t length, uint32_t encoding, uint32_t reserved)
{
    if (!steps(s, 1)) return 0;
    if (reserved || (encoding != PAK_DIRECTORY_STRING_UTF8 && encoding != PAK_DIRECTORY_STRING_UTF16LE) ||
        length > PAK_DIRECTORY_MAX_COMPONENT_UTF8_BYTES || offset >= s->in->names_bytes ||
        length >= s->in->names_bytes - offset || s->in->names[offset + length] != 0)
        return reject(s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_NAME_VIEW);
    return 1;
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int find_start(graph_state *s, uint32_t count, uint32_t offset, uint32_t *found)
{
    uint32_t low = 0, high = count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        if (!steps(s, 1)) return 0;
        if (s->ws->starts[mid] < offset) low = mid + 1; else high = mid;
    }
    if (low == count || s->ws->starts[low] != offset)
        return reject(s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_REFERENCE_NOT_START);
    *found = low;
    return 1;
}

static int compare_name(graph_state *s, uint32_t ao, uint32_t an, uint32_t bo, uint32_t bn, int *order)
{
    uint32_t i, n = an < bn ? an : bn;
    if (!steps(s, 1)) return 0;
    for (i = 0; i < n; ++i) {
        uint8_t a, b;
        if (2 > s->limits->max_name_comparison_bytes - s->compared)
            return reject(s, PAK_GRAPH_RESOURCE_LIMIT, PAK_GRAPH_REASON_NAME_COMPARISON_BYTES);
        s->compared += 2;
        a = s->in->names[ao + i]; b = s->in->names[bo + i];
        if (a != b) { *order = a < b ? -1 : 1; return 1; }
    }
    *order = an < bn ? -1 : an > bn ? 1 : 0;
    return 1;
}

static int compare(graph_state *s, uint32_t a, uint32_t b, int tuples, int *order)
{
    const pak_graph_extent_v1 *left = &s->ws->extents[a], *right = &s->ws->extents[b];
    if (!steps(s, 1)) return 0;
    if (tuples) {
        const pak_directory_record_v1 *ld = &s->in->directories[left->directory_index];
        const pak_directory_record_v1 *rd = &s->in->directories[right->directory_index];
        const pak_directory_entry_v1 *lf = &s->in->files[left->file_index];
        const pak_directory_entry_v1 *rf = &s->in->files[right->file_index];
        if (!compare_name(s, ld->name_offset, ld->name_length, rd->name_offset, rd->name_length, order)) return 0;
        if (*order != 0) return 1;
        return compare_name(s, lf->name_offset, lf->name_length, rf->name_offset, rf->name_length, order);
    }
    if (left->archive_start != right->archive_start) *order = left->archive_start < right->archive_start ? -1 : 1;
    else if (left->archive_end != right->archive_end) *order = left->archive_end < right->archive_end ? -1 : 1;
    else *order = a < b ? -1 : a > b ? 1 : 0;
    return 1;
}

static int swap_index(graph_state *s, uint32_t a, uint32_t b)
{
    uint32_t temporary;
    if (!steps(s, 1)) return 0;
    temporary = s->ws->sort_indices[a]; s->ws->sort_indices[a] = s->ws->sort_indices[b];
    s->ws->sort_indices[b] = temporary;
    return 1;
}

static int sift(graph_state *s, uint32_t root, uint32_t count, int tuples)
{
    while (root < count / 2) {
        uint32_t child = root * 2 + 1;
        int order;
        if (child + 1 < count) {
            if (!compare(s, s->ws->sort_indices[child], s->ws->sort_indices[child + 1], tuples, &order)) return 0;
            if (order < 0) ++child;
        }
        if (!compare(s, s->ws->sort_indices[root], s->ws->sort_indices[child], tuples, &order)) return 0;
        if (order >= 0) break;
        if (!swap_index(s, root, child)) return 0;
        root = child;
    }
    return 1;
}

static int sort_indices(graph_state *s, uint32_t count, int tuples)
{
    uint32_t i;
    for (i = count / 2; i != 0; --i) if (!sift(s, i - 1, count, tuples)) return 0;
    for (i = count; i > 1; --i)
        if (!swap_index(s, 0, i - 1) || !sift(s, 0, i - 1, tuples)) return 0;
    return 1;
}

void pak_graph_limits_v1_default(pak_graph_limits_v1 *limits)
{
    if (!limits) return;
    memset(limits, 0, sizeof(*limits));
    limits->struct_size = sizeof(*limits); limits->abi_version = PAK_REFERENCE_GRAPH_ABI_VERSION;
    limits->max_records = PAK_GRAPH_MAX_RECORDS; limits->max_directories = PAK_GRAPH_MAX_DIRECTORIES;
    limits->max_name_bytes = PAK_GRAPH_MAX_NAME_BYTES; limits->max_steps = PAK_GRAPH_MAX_STEPS;
    limits->max_name_comparison_bytes = PAK_GRAPH_MAX_NAME_COMPARISON_BYTES; limits->cancel_batch_steps = 64;
}

pak_graph_status pak_validate_v11_reference_graph(const pak_graph_input_v1 *in,
    const pak_graph_limits_v1 *limits, const pak_graph_workspace_v1 *ws,
    pak_graph_cancel_fn cancelled, void *cancel_context, pak_graph_result_v1 *result)
{
    graph_state s;
    pak_entry_result_v1 selected;
    uint32_t count = 0, selected_record = 0, selected_found = 0;
    uint32_t d, row = 0, i;
    size_t cursor = 0;
    uint64_t decoder_base_cost = 32;
    if (!preflight(in, limits, ws, result)) return PAK_GRAPH_BAD_ARGUMENT;
    memset(&s, 0, sizeof(s)); memset(&selected, 0, sizeof(selected));
    s.in = in; s.limits = limits; s.ws = ws; s.cancelled = cancelled; s.cancel_context = cancel_context;
    s.next_poll = limits->cancel_batch_steps;
    clear_workspace(ws); memset(result, 0, sizeof(*result));
    if (!poll_cancel(&s)) goto finish;
    if (in->file_count > limits->max_records || in->primary_file_count > limits->max_records ||
        in->directory_count > limits->max_directories || in->names_bytes > limits->max_name_bytes) {
        reject(&s, PAK_GRAPH_RESOURCE_LIMIT, PAK_GRAPH_REASON_OUTPUT_CAPACITY); goto finish;
    }
    for (i = 0; i < in->methods->count; ++i) decoder_base_cost += 4u + in->methods->methods[i].length;

    while (cursor < in->encoded_blob_bytes) {
        pak_entry_result_v1 entry;
        pak_entry_status status;
        uint32_t declared_blocks = in->encoded_blob_bytes - cursor >= 4 ?
            (read_le32(in->encoded_blob + cursor) >> 6) & 0xffffu : 0;
        if (!steps(&s, 1)) goto finish;
        if (count >= limits->max_records || count >= ws->starts_capacity || count >= ws->extents_capacity ||
            count >= ws->sort_capacity || count / 8 >= ws->seen_bytes) {
            reject(&s, PAK_GRAPH_RESOURCE_LIMIT, PAK_GRAPH_REASON_OUTPUT_CAPACITY); goto finish;
        }
        /* Conservative finite child work reservation, no refund on child failure. */
        if (!steps(&s, decoder_base_cost + 8u * declared_blocks) || !poll_cancel(&s)) goto finish;
        status = pak_entry_decode_v11(in->encoded_blob, in->encoded_blob_bytes, cursor,
            in->logical_archive_size, in->methods, in->entry_limits,
            ws->decode_blocks, ws->decode_blocks_capacity, &entry);
        s.decoder_status = status; s.decoder_reason = entry.reason;
        if (!poll_cancel(&s)) goto finish;
        if (status != PAK_ENTRY_STATUS_OK) {
            reject(&s, status == PAK_ENTRY_STATUS_UNSUPPORTED ? PAK_GRAPH_UNSUPPORTED :
                status == PAK_ENTRY_STATUS_RESOURCE_LIMIT ? PAK_GRAPH_RESOURCE_LIMIT : PAK_GRAPH_MALFORMED,
                PAK_GRAPH_REASON_ENTRY_DECODER); goto finish;
        }
        if (!entry.record_complete || !entry.bytes_consumed || entry.bytes_consumed > in->encoded_blob_bytes - cursor) {
            reject(&s, PAK_GRAPH_INTERNAL_ERROR, PAK_GRAPH_REASON_NO_PROGRESS); goto finish;
        }
        if (!((entry.method_index == 0 && entry.method_name_bytes == 4 && !memcmp(entry.method_name, "None", 4) && !entry.block_count) ||
              (entry.method_index != 0 && entry.method_name_bytes == 4 && !memcmp(entry.method_name, "Zlib", 4) && entry.block_count))) {
            reject(&s, PAK_GRAPH_UNSUPPORTED, PAK_GRAPH_REASON_CODEC_UNSUPPORTED); goto finish;
        }
        ws->starts[count] = (uint32_t)cursor;
        ws->extents[count].archive_start = entry.archive_offset;
        ws->extents[count].archive_end = entry.data_offset + entry.compressed_size;
        ws->extents[count].encoded_offset = (uint32_t)cursor;
        ws->extents[count].encoded_bytes = entry.bytes_consumed;
        ws->sort_indices[count] = count;
        if (in->want_selected && in->files[in->selected_file_index].location >= 0 &&
            (uint32_t)in->files[in->selected_file_index].location == cursor) {
            if (entry.block_count > ws->selected_blocks_capacity) {
                reject(&s, PAK_GRAPH_RESOURCE_LIMIT, PAK_GRAPH_REASON_OUTPUT_CAPACITY); goto finish;
            }
            if (!steps(&s, entry.block_count)) goto finish;
            selected = entry; selected_record = count; selected_found = 1;
            if (entry.block_count) memcpy(ws->selected_blocks, ws->decode_blocks, entry.block_count * sizeof(*ws->selected_blocks));
        }
        cursor += entry.bytes_consumed; ++count;
    }
    if (count != in->file_count || count != in->primary_file_count) {
        reject(&s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_COUNT_MISMATCH); goto finish;
    }

    for (d = 0; d < in->directory_count; ++d) {
        const pak_directory_record_v1 *directory = &in->directories[d];
        uint32_t end;
        if (!steps(&s, 1)) goto finish;
        if (directory->first_entry != row || directory->entry_count > in->file_count - row) {
            reject(&s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_DIRECTORY_VIEW); goto finish;
        }
        if (!name_view(&s, directory->name_offset, directory->name_length, directory->input_encoding, directory->reserved)) goto finish;
        end = row + directory->entry_count;
        for (; row < end; ++row) {
            const pak_directory_entry_v1 *file = &in->files[row];
            uint32_t record;
            if (!steps(&s, 1)) goto finish;
            if (file->location < 0 || file->flags) {
                reject(&s, PAK_GRAPH_UNSUPPORTED, PAK_GRAPH_REASON_REFERENCE_UNSUPPORTED); goto finish;
            }
            if (!name_view(&s, file->name_offset, file->name_length, file->input_encoding, file->reserved) ||
                !find_start(&s, count, (uint32_t)file->location, &record)) goto finish;
            if (ws->seen[record / 8] & (1u << (record % 8))) {
                reject(&s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_REFERENCE_ALIAS); goto finish;
            }
            ws->seen[record / 8] |= (uint8_t)(1u << (record % 8));
            ws->extents[record].directory_index = d; ws->extents[record].file_index = row;
        }
    }
    if (row != in->file_count) { reject(&s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_DIRECTORY_VIEW); goto finish; }
    for (i = 0; i < count; ++i) {
        if (!steps(&s, 1)) goto finish;
        if (!(ws->seen[i / 8] & (1u << (i % 8)))) {
            reject(&s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_ORPHAN); goto finish;
        }
    }
    if (!sort_indices(&s, count, 1)) goto finish;
    for (i = 1; i < count; ++i) {
        int order;
        if (!compare(&s, ws->sort_indices[i - 1], ws->sort_indices[i], 1, &order)) goto finish;
        if (!order) { reject(&s, PAK_GRAPH_MALFORMED, PAK_GRAPH_REASON_DUPLICATE_TUPLE); goto finish; }
    }
    if (!sort_indices(&s, count, 0) || !poll_cancel(&s)) goto finish;
    if (in->want_selected && (!selected_found || ws->extents[selected_record].file_index != in->selected_file_index)) {
        reject(&s, PAK_GRAPH_INTERNAL_ERROR, PAK_GRAPH_REASON_SELECTED_BINDING); goto finish;
    }

finish:
    if (s.status != PAK_GRAPH_OK) clear_workspace(ws);
    else if (ws->decode_blocks_capacity) memset(ws->decode_blocks, 0, ws->decode_blocks_capacity * sizeof(*ws->decode_blocks));
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result); result->abi_version = PAK_REFERENCE_GRAPH_ABI_VERSION;
    result->status = s.status; result->reason = s.reason;
    result->steps_used = s.steps; result->name_comparison_bytes = s.compared;
    result->decoder_status = s.decoder_status; result->decoder_reason = s.decoder_reason;
    if (s.status == PAK_GRAPH_OK) {
        result->metadata_complete = 1; result->directory_count = in->directory_count;
        result->file_count = in->file_count; result->record_count = count;
        if (in->want_selected) {
            result->selected_complete = 1; result->selected_file_index = in->selected_file_index;
            result->selected_record_index = selected_record;
            result->selected_directory_index = ws->extents[selected_record].directory_index;
            result->selected_entry = selected;
        }
    }
    return s.status;
}
