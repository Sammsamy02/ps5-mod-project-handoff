#include "container_header_merge.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define CCH_MAGIC UINT32_C(0x496f436e)
#define CCH_VERSION 5u
#define CCH_ARRAY_ITEMS_CAP 4194304u
#define CCH_OPAQUE_SUFFIX_CAP 15u

struct cch_header_view {
    const uint8_t *data;
    size_t bytes;
    uint64_t container_id;
    uint32_t package_count;
    size_t ids_offset;
    size_t store_size_offset;
    size_t store_offset;
    uint32_t store_bytes;
    size_t tail_offset;
    uint8_t soft_serial_absent;
    size_t suffix_offset;
    uint8_t suffix_bytes;
};

static int add_size(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) return 0;
    *out = a + b;
    return 1;
}

static int mul_size(size_t a, size_t b, size_t *out) {
    if (b && a > SIZE_MAX / b) return 0;
    *out = a * b;
    return 1;
}

static int span_ok(size_t total, size_t offset, size_t length) {
    return offset <= total && length <= total - offset;
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(const uint8_t *p) {
    return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p + 4) << 32);
}

static void write_u32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void write_u64(uint8_t *p, uint64_t value) {
    write_u32(p, (uint32_t)value);
    write_u32(p + 4, (uint32_t)(value >> 32));
}

static int nonzero(const uint8_t *p, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) if (p[i] != 0) return 1;
    return 0;
}

static int range_less(struct cch_byte_range a, struct cch_byte_range b) {
    return a.begin < b.begin || (a.begin == b.begin && a.end < b.end);
}

static void range_sift(struct cch_byte_range *v, size_t root, size_t n) {
    for (;;) {
        size_t child = root * 2u + 1u;
        size_t largest = root;
        struct cch_byte_range tmp;
        if (child < n && range_less(v[largest], v[child])) largest = child;
        if (child + 1u < n && range_less(v[largest], v[child + 1u])) largest = child + 1u;
        if (largest == root) return;
        tmp = v[root]; v[root] = v[largest]; v[largest] = tmp;
        root = largest;
    }
}

static void sort_ranges(struct cch_byte_range *v, size_t n) {
    size_t i;
    struct cch_byte_range tmp;
    for (i = n / 2u; i > 0; --i) range_sift(v, i - 1u, n);
    for (i = n; i > 1u; --i) {
        tmp = v[0]; v[0] = v[i - 1u]; v[i - 1u] = tmp;
        range_sift(v, 0, i - 1u);
    }
}

static int ref_less(struct cch_package_ref a, struct cch_package_ref b) {
    if (a.package_id != b.package_id) return a.package_id < b.package_id;
    if (a.source != b.source) return a.source < b.source;
    return a.source_index < b.source_index;
}

static void ref_sift(struct cch_package_ref *v, size_t root, size_t n) {
    for (;;) {
        size_t child = root * 2u + 1u;
        size_t largest = root;
        struct cch_package_ref tmp;
        if (child < n && ref_less(v[largest], v[child])) largest = child;
        if (child + 1u < n && ref_less(v[largest], v[child + 1u])) largest = child + 1u;
        if (largest == root) return;
        tmp = v[root]; v[root] = v[largest]; v[largest] = tmp;
        root = largest;
    }
}

static void sort_refs(struct cch_package_ref *v, size_t n) {
    size_t i;
    struct cch_package_ref tmp;
    for (i = n / 2u; i > 0; --i) ref_sift(v, i - 1u, n);
    for (i = n; i > 1u; --i) {
        tmp = v[0]; v[0] = v[i - 1u]; v[i - 1u] = tmp;
        ref_sift(v, 0, i - 1u);
    }
}

static enum cch_status parse_header(const uint8_t *data, size_t bytes,
                                    unsigned source,
                                    struct cch_workspace *workspace,
                                    size_t *ref_count,
                                    struct cch_header_view *view) {
    uint32_t count, store_bytes;
    size_t ids_bytes, cursor, descriptor_bytes, range_count, total_items = 0;
    size_t i, covered;
    const uint8_t *store;

    if (!data || bytes > CCH_HEADER_BYTES_CAP || bytes < 20u) return CCH_INVALID;
    if (read_u32(data) != CCH_MAGIC || read_u32(data + 4) != CCH_VERSION)
        return CCH_UNSUPPORTED;
    count = read_u32(data + 16);
    if (count > CCH_MAX_PACKAGES || !mul_size((size_t)count, 8u, &ids_bytes) ||
        !span_ok(bytes, 20u, ids_bytes)) return CCH_INVALID;
    if ((size_t)count > workspace->range_capacity / 2u ||
        (size_t)count * 2u + 1u > workspace->range_capacity ||
        (size_t)count > workspace->package_ref_capacity - *ref_count)
        return CCH_INVALID;

    memset(view, 0, sizeof(*view));
    view->data = data;
    view->bytes = bytes;
    view->container_id = read_u64(data + 8);
    view->package_count = count;
    view->ids_offset = 20u;
    cursor = 20u + ids_bytes;
    if (!span_ok(bytes, cursor, 4u)) return CCH_INVALID;
    view->store_size_offset = cursor;
    store_bytes = read_u32(data + cursor);
    cursor += 4u;
    if (!span_ok(bytes, cursor, store_bytes)) return CCH_INVALID;
    view->store_offset = cursor;
    view->store_bytes = store_bytes;
    store = data + cursor;
    cursor += store_bytes;

    if (!mul_size((size_t)count, 16u, &descriptor_bytes) ||
        descriptor_bytes > store_bytes) return CCH_INVALID;
    range_count = 1u;
    workspace->ranges[0].begin = 0;
    workspace->ranges[0].end = (uint32_t)descriptor_bytes;

    for (i = 0; i < count; ++i) {
        size_t descriptor_offset = i * 16u;
        uint32_t imported_count, imported_relative, shader_count, shader_relative;
        size_t array_bytes, start, end;
        struct cch_package_ref *ref = &workspace->package_refs[*ref_count];
        ref->package_id = read_u64(data + view->ids_offset + i * 8u);
        ref->source_index = (uint32_t)i;
        ref->source = (uint8_t)source;
        ref->reserved[0] = ref->reserved[1] = ref->reserved[2] = 0;
        ++*ref_count;

        imported_count = read_u32(store + descriptor_offset);
        imported_relative = read_u32(store + descriptor_offset + 4u);
        shader_count = read_u32(store + descriptor_offset + 8u);
        shader_relative = read_u32(store + descriptor_offset + 12u);
        if (imported_count > CCH_ARRAY_ITEMS_CAP || shader_count > CCH_ARRAY_ITEMS_CAP ||
            total_items > CCH_ARRAY_ITEMS_CAP - (size_t)imported_count ||
            total_items + (size_t)imported_count > CCH_ARRAY_ITEMS_CAP - (size_t)shader_count)
            return CCH_INVALID;
        total_items += (size_t)imported_count + (size_t)shader_count;

        if (imported_count) {
            if (!mul_size((size_t)imported_count, 8u, &array_bytes) ||
                !add_size(descriptor_offset, (size_t)imported_relative, &start) ||
                start < descriptor_bytes || !span_ok(store_bytes, start, array_bytes) ||
                range_count >= workspace->range_capacity ||
                !add_size(start, array_bytes, &end) || end > UINT32_MAX)
                return CCH_INVALID;
            workspace->ranges[range_count].begin = (uint32_t)start;
            workspace->ranges[range_count].end = (uint32_t)end;
            ++range_count;
        }
        if (shader_count) {
            size_t view_offset;
            if (!add_size(descriptor_offset, 8u, &view_offset) ||
                !mul_size((size_t)shader_count, 20u, &array_bytes) ||
                !add_size(view_offset, (size_t)shader_relative, &start) ||
                start < descriptor_bytes || !span_ok(store_bytes, start, array_bytes) ||
                range_count >= workspace->range_capacity ||
                !add_size(start, array_bytes, &end) || end > UINT32_MAX)
                return CCH_INVALID;
            workspace->ranges[range_count].begin = (uint32_t)start;
            workspace->ranges[range_count].end = (uint32_t)end;
            ++range_count;
        }
    }

    sort_ranges(workspace->ranges, range_count);
    covered = 0;
    for (i = 0; i < range_count; ++i) {
        size_t begin = workspace->ranges[i].begin;
        size_t end = workspace->ranges[i].end;
        if (begin > covered && nonzero(store + covered, begin - covered))
            return CCH_UNSUPPORTED;
        if (end > covered) covered = end;
    }
    if (covered < store_bytes && nonzero(store + covered, store_bytes - covered))
        return CCH_UNSUPPORTED;

    for (i = 0; i < 5u; ++i) {
        if (!span_ok(bytes, cursor, 4u)) return CCH_INVALID;
        if (read_u32(data + cursor) != 0) return CCH_UNSUPPORTED;
        cursor += 4u;
    }
    if (!span_ok(bytes, cursor, 16u)) return CCH_INVALID;
    {
        uint64_t soft_offset = read_u64(data + cursor);
        uint64_t soft_size = read_u64(data + cursor + 8u);
        cursor += 16u;
        if (soft_size == 0) {
            if (soft_offset != 0 && soft_offset != (uint64_t)cursor)
                return CCH_UNSUPPORTED;
            view->soft_serial_absent = 1;
        } else {
            if (soft_size != 4u || soft_offset != (uint64_t)cursor)
                return CCH_UNSUPPORTED;
            if (!span_ok(bytes, cursor, 4u)) return CCH_INVALID;
            if (read_u32(data + cursor) != 0) return CCH_UNSUPPORTED;
            cursor += 4u;
            view->soft_serial_absent = 0;
        }
    }
    if (bytes - cursor > CCH_OPAQUE_SUFFIX_CAP || nonzero(data + cursor, bytes - cursor))
        return CCH_UNSUPPORTED;
    view->tail_offset = cursor;
    view->suffix_offset = cursor;
    view->suffix_bytes = (uint8_t)(bytes - cursor);
    return CCH_OK;
}

static int ranges_overlap(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t ap, bp;
    if (!a || !b || !an || !bn) return 0;
    ap = (uintptr_t)a; bp = (uintptr_t)b;
    if (an > UINTPTR_MAX - ap || bn > UINTPTR_MAX - bp) return 1;
    return ap < bp + bn && bp < ap + an;
}

static int safe_args(const uint8_t *base, size_t base_bytes,
                     const uint8_t *mod, size_t mod_bytes,
                     struct cch_workspace *ws, uint8_t *output,
                     size_t output_capacity, struct cch_result *result) {
    size_t refs_bytes, ranges_bytes;
    if (!base || !mod || !ws || !result || base_bytes > CCH_HEADER_BYTES_CAP ||
        mod_bytes > CCH_HEADER_BYTES_CAP || output_capacity > CCH_HEADER_BYTES_CAP)
        return 0;
    if ((!output && output_capacity != 0) ||
        !ws->package_refs || !ws->ranges ||
        ws->package_ref_capacity > CCH_MAX_REFS ||
        ws->range_capacity > CCH_MAX_RANGES ||
        !mul_size(ws->package_ref_capacity, sizeof(*ws->package_refs), &refs_bytes) ||
        !mul_size(ws->range_capacity, sizeof(*ws->ranges), &ranges_bytes)) return 0;
    if (ranges_overlap(result, sizeof(*result), base, base_bytes) ||
        ranges_overlap(result, sizeof(*result), mod, mod_bytes) ||
        ranges_overlap(result, sizeof(*result), output, output_capacity) ||
        ranges_overlap(result, sizeof(*result), ws->package_refs, refs_bytes) ||
        ranges_overlap(result, sizeof(*result), ws->ranges, ranges_bytes) ||
        ranges_overlap(base, base_bytes, output, output_capacity) ||
        ranges_overlap(mod, mod_bytes, output, output_capacity) ||
        ranges_overlap(base, base_bytes, ws->package_refs, refs_bytes) ||
        ranges_overlap(mod, mod_bytes, ws->package_refs, refs_bytes) ||
        ranges_overlap(base, base_bytes, ws->ranges, ranges_bytes) ||
        ranges_overlap(mod, mod_bytes, ws->ranges, ranges_bytes) ||
        ranges_overlap(ws->package_refs, refs_bytes, ws->ranges, ranges_bytes)) return 0;
    return 1;
}

static enum cch_status prepare(const uint8_t *base, size_t base_bytes,
                               const uint8_t *mod, size_t mod_bytes,
                               struct cch_workspace *ws,
                               struct cch_header_view views[2],
                               struct cch_result *result,
                               size_t *store_size) {
    size_t ref_count = 0, i, merged_count = 0, total_items = 0;
    size_t desc_bytes, arrays_bytes = 0, required = 0, soft_bytes;
    enum cch_status status;

    status = parse_header(base, base_bytes, 0, ws, &ref_count, &views[0]);
    if (status != CCH_OK) return status;
    result->base_package_count = views[0].package_count;
    result->base_container_id = views[0].container_id;
    status = parse_header(mod, mod_bytes, 1, ws, &ref_count, &views[1]);
    if (status != CCH_OK) return status;
    result->mod_package_count = views[1].package_count;
    result->mod_container_id = views[1].container_id;
    sort_refs(ws->package_refs, ref_count);

    for (i = 0; i < ref_count;) {
        size_t end = i + 1u, j;
        uint8_t saw_base = ws->package_refs[i].source == 0;
        uint8_t saw_mod = ws->package_refs[i].source == 1;
        while (end < ref_count && ws->package_refs[end].package_id == ws->package_refs[i].package_id) {
            if (ws->package_refs[end].source == ws->package_refs[end - 1u].source)
                return CCH_INVALID;
            if (ws->package_refs[end].source == 0) saw_base = 1;
            else saw_mod = 1;
            ++end;
        }
        j = end - 1u; /* sort order makes mod win exact-ID collisions */
        ws->package_refs[merged_count++] = ws->package_refs[j];
        if (saw_base && saw_mod) ++result->replaced_package_count;
        else if (saw_mod) ++result->added_package_count;
        i = end;
    }
    if (merged_count > CCH_MAX_PACKAGES) return CCH_INVALID;
    result->merged_package_count = (uint32_t)merged_count;
    if (!mul_size(merged_count, 16u, &desc_bytes)) return CCH_INVALID;
    for (i = 0; i < merged_count; ++i) {
        const struct cch_package_ref *ref = &ws->package_refs[i];
        const struct cch_header_view *view = &views[ref->source];
        const uint8_t *descriptor = view->data + view->store_offset + (size_t)ref->source_index * 16u;
        uint32_t imported_count = read_u32(descriptor);
        uint32_t shader_count = read_u32(descriptor + 8u);
        size_t imported_bytes, shader_bytes;
        if (imported_count > CCH_ARRAY_ITEMS_CAP || shader_count > CCH_ARRAY_ITEMS_CAP ||
            total_items > CCH_ARRAY_ITEMS_CAP - (size_t)imported_count ||
            total_items + (size_t)imported_count > CCH_ARRAY_ITEMS_CAP - (size_t)shader_count)
            return CCH_INVALID;
        total_items += (size_t)imported_count + (size_t)shader_count;
        if (!mul_size((size_t)imported_count, 8u, &imported_bytes) ||
            !mul_size((size_t)shader_count, 20u, &shader_bytes) ||
            !add_size(arrays_bytes, imported_bytes, &arrays_bytes) ||
            !add_size(arrays_bytes, shader_bytes, &arrays_bytes)) return CCH_INVALID;
    }
    if (!add_size(desc_bytes, arrays_bytes, store_size) ||
        *store_size > CCH_HEADER_BYTES_CAP || *store_size > UINT32_MAX) return CCH_INVALID;
    soft_bytes = views[0].soft_serial_absent ? 16u : 20u;
    if (!add_size(20u, merged_count * 8u, &required) ||
        !add_size(required, 4u, &required) || !add_size(required, *store_size, &required) ||
        !add_size(required, 20u, &required) || !add_size(required, soft_bytes, &required) ||
        !add_size(required, views[0].suffix_bytes, &required) || required > CCH_HEADER_BYTES_CAP)
        return CCH_INVALID;
    result->required_bytes = required;
    result->container_id_preserved = 1;
    return CCH_OK;
}

static void emit_output(const struct cch_header_view views[2],
                        const struct cch_workspace *ws,
                        const struct cch_result *result,
                        size_t store_size, uint8_t *output) {
    size_t pos = 0, store_pos, i;
    write_u32(output + pos, CCH_MAGIC); pos += 4u;
    write_u32(output + pos, CCH_VERSION); pos += 4u;
    write_u64(output + pos, views[0].container_id); pos += 8u;
    write_u32(output + pos, result->merged_package_count); pos += 4u;
    for (i = 0; i < result->merged_package_count; ++i) {
        write_u64(output + pos, ws->package_refs[i].package_id);
        pos += 8u;
    }
    write_u32(output + pos, (uint32_t)store_size); pos += 4u;
    store_pos = pos;
    memset(output + store_pos, 0, result->required_bytes - store_pos);
    pos += (size_t)result->merged_package_count * 16u;

    for (i = 0; i < result->merged_package_count; ++i) {
        const struct cch_package_ref *ref = &ws->package_refs[i];
        const struct cch_header_view *view = &views[ref->source];
        const uint8_t *descriptor = view->data + view->store_offset + (size_t)ref->source_index * 16u;
        const uint8_t *store = view->data + view->store_offset;
        uint32_t imported_count = read_u32(descriptor);
        uint32_t imported_relative = read_u32(descriptor + 4u);
        uint32_t shader_count = read_u32(descriptor + 8u);
        uint32_t shader_relative = read_u32(descriptor + 12u);
        size_t output_descriptor = store_pos + i * 16u;
        size_t source_start, array_bytes;
        if (imported_count) {
            source_start = (size_t)ref->source_index * 16u + imported_relative;
            array_bytes = (size_t)imported_count * 8u;
            write_u32(output + output_descriptor, imported_count);
            write_u32(output + output_descriptor + 4u, (uint32_t)(pos - store_pos - i * 16u));
            memcpy(output + pos, store + source_start, array_bytes);
            pos += array_bytes;
        }
        if (shader_count) {
            source_start = (size_t)ref->source_index * 16u + 8u + shader_relative;
            array_bytes = (size_t)shader_count * 20u;
            write_u32(output + output_descriptor + 8u, shader_count);
            write_u32(output + output_descriptor + 12u, (uint32_t)(pos - store_pos - i * 16u - 8u));
            memcpy(output + pos, store + source_start, array_bytes);
            pos += array_bytes;
        }
    }
    pos = store_pos + store_size;
    for (i = 0; i < 5u; ++i) { write_u32(output + pos, 0); pos += 4u; }
    if (views[0].soft_serial_absent) {
        write_u64(output + pos, 0); write_u64(output + pos + 8u, 0); pos += 16u;
    } else {
        write_u64(output + pos, (uint64_t)(pos + 16u));
        write_u64(output + pos + 8u, 4u);
        write_u32(output + pos + 16u, 0);
        pos += 20u;
    }
    memcpy(output + pos, views[0].data + views[0].suffix_offset, views[0].suffix_bytes);
}

enum cch_status cch_merge_v5(const uint8_t *base, size_t base_bytes,
                            const uint8_t *mod, size_t mod_bytes,
                            struct cch_workspace *workspace,
                            uint8_t *output, size_t output_capacity,
                            struct cch_result *result) {
    struct cch_header_view views[2];
    enum cch_status status;
    size_t store_size = 0;
    if (!result || !safe_args(base, base_bytes, mod, mod_bytes, workspace,
                              output, output_capacity, result)) return CCH_INVALID;
    memset(result, 0, sizeof(*result));
    status = prepare(base, base_bytes, mod, mod_bytes, workspace, views, result, &store_size);
    if (status != CCH_OK) {
        memset(result, 0, sizeof(*result));
        return status;
    }
    if (!output || output_capacity < result->required_bytes) return CCH_OUTPUT_TOO_SMALL;
    emit_output(views, workspace, result, store_size, output);
    result->output_bytes = result->required_bytes;
    return CCH_OK;
}
