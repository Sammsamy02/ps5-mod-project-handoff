#include "pak_selection.h"
#include <stdint.h>
#include <string.h>

typedef struct selection_span { uintptr_t begin, end; } selection_span;

static int region(const void *pointer, size_t count, size_t width,
                  size_t alignment, selection_span *out) {
    uintptr_t begin = (uintptr_t)pointer;
    size_t bytes;
    if (width && count > SIZE_MAX / width) return 0;
    bytes = count * width;
    if ((bytes && !pointer) || (pointer && begin % alignment) ||
        bytes > UINTPTR_MAX - begin) return 0;
    out->begin = begin;
    out->end = begin + bytes;
    return 1;
}

static int disjoint(const selection_span *spans, size_t count) {
    size_t i, j;
    for (i = 0; i < count; ++i) for (j = 0; j < i; ++j)
        if (spans[i].begin < spans[i].end && spans[j].begin < spans[j].end &&
            spans[i].begin < spans[j].end && spans[j].begin < spans[i].end)
            return 0;
    return 1;
}

static int preflight(const mb_archive_prepared_view_v1 *view,
                     mb_pak_archive_policy_v1 policy,
                     const mb_archive_prepared_workspace_v1 *work,
                     uint32_t *rows, uint32_t row_capacity,
                     mb_pak_archive_member_v1 *members, uint32_t member_capacity,
                     mb_pak_archive_selection_v1 *result) {
    selection_span spans[13];
    size_t n = 0;
    if (!view || !work || !result ||
        (uintptr_t)view % _Alignof(mb_archive_prepared_view_v1) ||
        (uintptr_t)work % _Alignof(mb_archive_prepared_workspace_v1) ||
        (uintptr_t)result % _Alignof(mb_pak_archive_selection_v1) ||
        !view->inventory ||
        (uintptr_t)view->inventory % _Alignof(mb_archive_inventory_v1)) return 0;
    if ((policy != MB_PAK_ARCHIVE_POLICY_DEFAULT &&
         policy != MB_PAK_ARCHIVE_POLICY_PAK_COMPONENT_ONLY) ||
        view->row_capacity > MB_ARCHIVE_MAX_ENTRIES ||
        view->name_capacity > MB_ARCHIVE_MAX_NAME_BYTES ||
        row_capacity > MB_ARCHIVE_MAX_ENTRIES ||
        member_capacity > MB_ARCHIVE_MAX_ENTRIES ||
        work->sort_capacity > MB_ARCHIVE_MAX_ENTRIES ||
        work->group_capacity > MB_ARCHIVE_GROUPS_MAX_GROUPS ||
        work->member_capacity > MB_ARCHIVE_GROUPS_MAX_MEMBERS ||
        work->key_capacity > MB_ARCHIVE_GROUPS_MAX_KEY_BYTES ||
        view->inventory->struct_size != sizeof(*view->inventory) ||
        view->inventory->abi_version != MB_ARCHIVE_ABI_V1) return 0;
#define SP(P, C, T) do { \
    if (!region((P), (C), sizeof(T), _Alignof(T), spans + n)) return 0; \
    ++n; \
} while (0)
    SP(view, 1, mb_archive_prepared_view_v1);
    SP(work, 1, mb_archive_prepared_workspace_v1);
    SP(view->inventory, 1, mb_archive_inventory_v1);
    SP(view->rows, view->row_capacity, mb_archive_row_v1);
    SP(view->names, view->name_capacity, char);
    SP(work->sort_indices, work->sort_capacity, uint32_t);
    SP(work->groups, work->group_capacity, mb_archive_group_v1);
    SP(work->members, work->member_capacity, mb_archive_group_member_v1);
    SP(work->keys, work->key_capacity, char);
    SP(rows, row_capacity, uint32_t);
    SP(members, member_capacity, mb_pak_archive_member_v1);
    SP(result, 1, mb_pak_archive_selection_v1);
#undef SP
    return disjoint(spans, n);
}

static int valid_utf8(const char *data, uint32_t bytes) {
    uint32_t i = 0;
    while (i < bytes) {
        uint32_t cp, minimum;
        unsigned more;
        uint8_t lead = (uint8_t)data[i++];
        if (!lead) return 0;
        if (lead < 0x80) continue;
        if (lead >= 0xc2 && lead <= 0xdf) {
            cp = lead & 31u; more = 1; minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            cp = lead & 15u; more = 2; minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            cp = lead & 7u; more = 3; minimum = 0x10000;
        } else return 0;
        if (more > bytes - i) return 0;
        while (more--) {
            uint8_t next = (uint8_t)data[i++];
            if ((next & 0xc0) != 0x80) return 0;
            cp = (cp << 6) | (next & 63u);
        }
        if (cp < minimum || cp > 0x10ffff ||
            (cp >= 0xd800 && cp <= 0xdfff)) return 0;
    }
    return 1;
}

static int stored_name(const mb_archive_prepared_view_v1 *view,
                       uint32_t offset, uint32_t bytes) {
    uint32_t capacity = view->inventory->name_bytes;
    if (!bytes || bytes > MB_ARCHIVE_MAX_PATH_BYTES || offset >= capacity ||
        bytes >= capacity - offset || view->names[offset + bytes] != '\0')
        return 0;
    return valid_utf8(view->names + offset, bytes);
}

static int normalized_path(const char *path, uint32_t bytes) {
    uint32_t i, start = 0;
    if (!bytes || path[0] == '/' || path[0] == '\\') return 0;
    for (i = 0; i <= bytes; ++i) {
        if (i < bytes) {
            unsigned char c = (unsigned char)path[i];
            if (!c || c < 32 || c == 127 || c == '\\' || c == ':') return 0;
            if (c != '/') continue;
        }
        if (i == start || (i - start == 1 && path[start] == '.') ||
            (i - start == 2 && path[start] == '.' && path[start + 1] == '.') ||
            path[i - 1] == '.' || path[i - 1] == ' ') return 0;
        start = i + 1;
    }
    return 1;
}

static int inventory_rows_valid(const mb_archive_prepared_view_v1 *view,
                                uint32_t *regular_count, uint32_t *problem_row) {
    const mb_archive_inventory_v1 *inv = view->inventory;
    uint64_t expanded = 0;
    uint32_t i;
    *regular_count = 0;
    for (i = 0; i < inv->row_count; ++i) {
        const mb_archive_row_v1 *row = view->rows + i;
        if (row->reserved ||
            (row->kind != MB_ARCHIVE_REGULAR && row->kind != MB_ARCHIVE_DIRECTORY) ||
            (row->kind == MB_ARCHIVE_DIRECTORY && row->size) ||
            row->size > MB_ARCHIVE_MAX_MEMBER_BYTES ||
            row->size > MB_ARCHIVE_MAX_EXPANDED_BYTES - expanded ||
            !stored_name(view, row->original_offset, row->original_length) ||
            !stored_name(view, row->path_offset, row->path_length) ||
            !normalized_path(view->names + row->path_offset, row->path_length)) {
            *problem_row = i;
            return 0;
        }
        expanded += row->size;
        if (row->kind == MB_ARCHIVE_REGULAR) ++*regular_count;
    }
    return expanded == inv->expanded_bytes;
}

static void clear_backing(uint32_t *rows, uint32_t row_capacity,
                          mb_pak_archive_member_v1 *members, uint32_t member_capacity) {
    if (row_capacity) memset(rows, 0, (size_t)row_capacity * sizeof(*rows));
    if (member_capacity) memset(members, 0, (size_t)member_capacity * sizeof(*members));
}

static mb_pak_archive_status_v1 finish(mb_pak_archive_selection_v1 *result,
                                      mb_pak_archive_status_v1 status) {
    result->status = (uint32_t)status;
    return status;
}

mb_pak_archive_status_v1 mb_pak_archive_select_v1(
    const mb_archive_prepared_view_v1 *view, mb_pak_archive_policy_v1 policy,
    const mb_archive_prepared_workspace_v1 *work,
    uint32_t *materialize_rows, uint32_t row_capacity,
    mb_pak_archive_member_v1 *members, uint32_t member_capacity,
    mb_pak_archive_selection_v1 *result) {
    const mb_archive_inventory_v1 *inv;
    mb_archive_groups_result_v1 grouped;
    uint32_t regular_count, chosen = UINT32_MAX, i, n = 0;
    int inner;
    if (!preflight(view, policy, work, materialize_rows, row_capacity,
                   members, member_capacity, result)) return MB_PAK_ARCHIVE_BAD_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = MB_PAK_ARCHIVE_SELECTION_ABI_V1;
    result->policy = (uint32_t)policy;
    result->pak_row = result->problem_row = UINT32_MAX;
    clear_backing(materialize_rows, row_capacity, members, member_capacity);
    inv = view->inventory;
    if (inv->status != MB_ARCHIVE_OK || inv->complete != 1 || inv->reserved ||
        inv->installation_supported || inv->row_count > view->row_capacity ||
        inv->row_count > MB_ARCHIVE_MAX_ENTRIES || inv->name_bytes > view->name_capacity ||
        inv->name_bytes > MB_ARCHIVE_MAX_NAME_BYTES ||
        inv->source_size > MB_ARCHIVE_MAX_RAW_BYTES ||
        inv->expanded_bytes > MB_ARCHIVE_MAX_EXPANDED_BYTES)
        return finish(result, MB_PAK_ARCHIVE_INCOMPLETE);
    if (!inventory_rows_valid(view, &regular_count, &result->problem_row))
        return finish(result, MB_PAK_ARCHIVE_INVALID_INVENTORY);
    if (regular_count > row_capacity || regular_count > member_capacity ||
        inv->row_count > work->sort_capacity)
        return finish(result, MB_PAK_ARCHIVE_LIMIT);
    memset(&grouped, 0, sizeof(grouped));
    inner = mb_archive_groups_v1(inv, view->rows, view->row_capacity,
        view->names, view->name_capacity, work->sort_indices, work->sort_capacity,
        work->groups, work->group_capacity, work->members, work->member_capacity,
        work->keys, work->key_capacity, &grouped);
    result->group_inner_status = (uint32_t)inner;
    if (inner != MB_ARCHIVE_GROUPS_OK)
        return finish(result, inner == MB_ARCHIVE_GROUPS_LIMIT ?
                      MB_PAK_ARCHIVE_LIMIT : MB_PAK_ARCHIVE_GROUP_REJECTED);
    result->group_count = grouped.group_count;
    result->path_work_used = grouped.path_work_used;
    result->source_bytes = inv->source_size;
    memcpy(result->source_sha256, inv->source_sha256, 32);
    memcpy(result->inventory_sha256, inv->inventory_sha256, 32);
    memcpy(result->target_token, inv->target_token, 32);
    for (i = 0; i < grouped.group_count; ++i) {
        const mb_archive_group_v1 *g = work->groups + i;
        if (g->status != MB_ARCHIVE_GROUP_STATUS_CANDIDATE ||
            g->members_complete != 1 || g->reason_flags) {
            ++result->unresolved_group_count;
            continue;
        }
        if (g->kind == MB_ARCHIVE_GROUP_PAK) {
            if (g->member_count != 1 || g->member_begin >= grouped.member_count ||
                work->members[g->member_begin].role != MB_ARCHIVE_GROUP_ROLE_PAK ||
                work->members[g->member_begin].row_index >= inv->row_count) {
                return finish(result, MB_PAK_ARCHIVE_GROUP_REJECTED);
            }
            chosen = work->members[g->member_begin].row_index;
            ++result->candidate_count;
        }
    }
    if (result->candidate_count != 1 || result->unresolved_group_count)
        chosen = UINT32_MAX;
    for (i = 0; i < inv->row_count; ++i) {
        const mb_archive_row_v1 *row = view->rows + i;
        mb_pak_archive_member_v1 *member;
        if (row->kind != MB_ARCHIVE_REGULAR) continue;
        materialize_rows[n] = i;
        member = members + n++;
        member->row_index = i;
        member->role = i == chosen ? MB_PAK_ARCHIVE_ROLE_PAK :
                                   MB_PAK_ARCHIVE_ROLE_RETAINED_EXTRA;
        member->retained_unhandled = i != chosen;
        member->bytes = row->size;
        memcpy(member->sha256, row->sha256, 32);
    }
    result->member_count = n;
    result->member_list_complete = 1;
    result->retained_extra_count = n - (chosen != UINT32_MAX ? 1u : 0u);
    if (result->unresolved_group_count) return finish(result, MB_PAK_ARCHIVE_UNRESOLVED);
    if (!result->candidate_count) return finish(result, MB_PAK_ARCHIVE_NO_CANDIDATE);
    if (result->candidate_count > 1) return finish(result, MB_PAK_ARCHIVE_SELECTION_REQUIRED);
    result->pak_row = chosen;
    result->selection_complete = 1;
    result->whole_bundle_handled = result->retained_extra_count == 0;
    if (result->retained_extra_count && policy == MB_PAK_ARCHIVE_POLICY_DEFAULT)
        return finish(result, MB_PAK_ARCHIVE_ADDITIONAL_ACTION_REQUIRED);
    result->preparation_ready = 1;
    return finish(result, MB_PAK_ARCHIVE_OK);
}
