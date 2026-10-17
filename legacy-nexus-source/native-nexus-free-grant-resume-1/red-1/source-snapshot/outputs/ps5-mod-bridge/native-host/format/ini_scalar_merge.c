#include "ini_scalar_merge.h"
#include <string.h>

_Static_assert(sizeof(pm_ini_workspace) <= PM_INI_WORKSPACE_CEILING,
               "workspace exceeds explicit ceiling");
_Static_assert(PM_INI_MAX_WINNERS < UINT16_MAX, "hash index width");

typedef struct { uintptr_t begin, end; } memory_range;
typedef struct {
    uint8_t *output;
    size_t position;
    uint32_t lf_count;
    uint8_t has_bytes, last_lf;
} writer;

static uint8_t lower_ascii(uint8_t c) {
    return c >= 'A' && c <= 'Z' ? (uint8_t)(c + ('a' - 'A')) : c;
}

static int space(uint8_t c) { return c == ' ' || c == '\t'; }
static int alpha(uint8_t c) {
    c = lower_ascii(c);
    return c >= 'a' && c <= 'z';
}
static int digit(uint8_t c) { return c >= '0' && c <= '9'; }

static int range_of(const void *p, size_t n, memory_range *r) {
    uintptr_t begin = (uintptr_t)p;
    if ((n && !p) || n > UINTPTR_MAX - begin) return 0;
    r->begin = begin; r->end = begin + n;
    return 1;
}

static int overlap(memory_range a, memory_range b) {
    return a.begin < a.end && b.begin < b.end &&
           a.begin < b.end && b.begin < a.end;
}

static int clashes(const void *p, size_t n, const memory_range writable[3]) {
    memory_range r;
    size_t i;
    if (!range_of(p, n, &r)) return -1;
    for (i = 0; i < 3; ++i) if (overlap(r, writable[i])) return 1;
    return 0;
}

static pm_ini_status finish(pm_ini_result *result, pm_ini_result *value,
                            pm_ini_status status) {
    value->status = status;
    *result = *value;
    return status;
}

static pm_ini_status utf8(pm_ini_bytes view) {
    size_t i = 0;
    while (i < view.size) {
        uint8_t c = view.data[i++];
        size_t more, j;
        uint8_t first_min = 0x80, first_max = 0xbf;
        if (!c) return PM_INI_NUL;
        if (c < 0x80) continue;
        if (c >= 0xc2 && c <= 0xdf) more = 1;
        else if (c >= 0xe0 && c <= 0xef) {
            more = 2;
            if (c == 0xe0) first_min = 0xa0;
            if (c == 0xed) first_max = 0x9f;
        } else if (c >= 0xf0 && c <= 0xf4) {
            more = 3;
            if (c == 0xf0) first_min = 0x90;
            if (c == 0xf4) first_max = 0x8f;
        } else return PM_INI_INVALID_UTF8;
        if (more > view.size - i) return PM_INI_INVALID_UTF8;
        for (j = 0; j < more; ++j) {
            uint8_t next = view.data[i++];
            if (next < (j ? 0x80 : first_min) || next > (j ? 0xbf : first_max))
                return PM_INI_INVALID_UTF8;
        }
    }
    return PM_INI_OK;
}

static int equal_ascii(const uint8_t *a, size_t an, const uint8_t *b, size_t bn) {
    size_t i;
    if (an != bn) return 0;
    for (i = 0; i < an; ++i)
        if (lower_ascii(a[i]) != lower_ascii(b[i])) return 0;
    return 1;
}

static int section_index(const uint8_t *name, size_t size,
                         const pm_ini_bytes *sections, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i)
        if (equal_ascii(name, size, sections[i].data, sections[i].size)) return (int)i;
    return -1;
}

static pm_ini_status selected_names(const pm_ini_bytes *sections, size_t count) {
    size_t i, j, k;
    for (i = 0; i < count; ++i) {
        if (!sections[i].size || sections[i].size > PM_INI_MAX_SECTION_BYTES)
            return PM_INI_SECTION_LIMIT;
        for (j = 0; j < sections[i].size; ++j) {
            uint8_t c = sections[i].data[j];
            if (!alpha(c) && !digit(c) && c != '_' && c != '.' &&
                c != '/' && c != ':' && c != '-') return PM_INI_HEADER_SYNTAX;
        }
        for (k = 0; k < i; ++k)
            if (equal_ascii(sections[i].data, sections[i].size,
                            sections[k].data, sections[k].size)) return PM_INI_SECTION_LIMIT;
    }
    return PM_INI_OK;
}

static pm_ini_status scalar(const uint8_t *value, size_t size) {
    size_t begin = 0, end = size, i;
    while (begin < end && space(value[begin])) ++begin;
    while (end > begin && space(value[end - 1])) --end;
    if (end > begin && value[end - 1] == '\\') return PM_INI_SCALAR_SYNTAX;
    if (end > begin && value[begin] == '"') {
        i = begin + 1;
        while (i < end) {
            if (value[i] == '\\') { i += 2; continue; }
            if (value[i] == '"') return i == end - 1 ? PM_INI_OK : PM_INI_SCALAR_SYNTAX;
            ++i;
        }
        return PM_INI_SCALAR_SYNTAX;
    }
    for (i = begin; i < end; ++i) {
        uint8_t c = value[i];
        if (c == '"' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}') return PM_INI_SCALAR_SYNTAX;
    }
    return PM_INI_OK;
}

static uint32_t key_hash(uint8_t section, const uint8_t *key, size_t size) {
    uint32_t hash = 2166136261u ^ section;
    size_t i;
    for (i = 0; i < size; ++i) hash = (hash ^ lower_ascii(key[i])) * 16777619u;
    return hash;
}

/* Returns a winner index, or -1 for an empty slot. No pointers are cached. */
static int find_winner(pm_ini_workspace *work, const pm_ini_bytes *sources,
                       uint8_t section, const uint8_t *key, size_t size,
                       uint32_t *empty_slot) {
    uint32_t slot = key_hash(section, key, size) & (PM_INI_HASH_SLOTS - 1u);
    uint32_t probes;
    for (probes = 0; probes < PM_INI_HASH_SLOTS; ++probes) {
        uint16_t id = work->hash_slots[slot];
        if (!id) { if (empty_slot) *empty_slot = slot; return -1; }
        {
            const pm_ini_scratch_winner *w = &work->winners[id - 1u];
            if (w->section == section && equal_ascii(key, size,
                    sources[w->source].data + w->key_offset, w->key_size)) return (int)id - 1;
        }
        slot = (slot + 1u) & (PM_INI_HASH_SLOTS - 1u);
    }
    return -2;
}

static pm_ini_status add_winner(pm_ini_workspace *work, const pm_ini_bytes *sources,
                                size_t source, pm_ini_scratch_line line,
                                pm_ini_result *result) {
    uint32_t slot = 0;
    int id = find_winner(work, sources, (uint8_t)line.section,
                         sources[source].data + line.key_offset, line.key_size, &slot);
    pm_ini_scratch_winner *w;
    if (id == -2) return PM_INI_WINNER_LIMIT;
    if (id < 0) {
        if (work->winner_count == PM_INI_MAX_WINNERS) return PM_INI_WINNER_LIMIT;
        id = (int)work->winner_count++;
        work->hash_slots[slot] = (uint16_t)(id + 1);
    } else ++result->source_duplicates;
    w = &work->winners[id];
    w->body_offset = line.offset; w->body_size = line.body_size;
    w->key_offset = line.key_offset; w->key_size = line.key_size;
    w->value_offset = line.value_offset; w->value_size = line.value_size;
    w->section = (uint8_t)line.section; w->source = (uint8_t)source;
    w->ordinal = work->ordinal++; w->base_last = -1;
    ++result->source_assignments;
    return PM_INI_OK;
}

static pm_ini_status parse_document(pm_ini_workspace *work, pm_ini_bytes doc,
                                    int is_base, size_t source,
                                    const pm_ini_bytes *sources,
                                    const pm_ini_bytes *sections, size_t section_count,
                                    pm_ini_result *result) {
    size_t pos = 0;
    uint32_t lines = 0;
    int current_section = -1, saw_header = 0;
    pm_ini_status status = utf8(doc);
    if (status != PM_INI_OK) return status;
    if (doc.size >= 3 && doc.data[0] == 0xef && doc.data[1] == 0xbb && doc.data[2] == 0xbf) {
        pos = 3;
        if (is_base) work->bom_size = 3;
    }
    while (pos < doc.size) {
        size_t end = pos, raw_end, begin, trim_end, i;
        pm_ini_scratch_line line = {0};
        while (end < doc.size && doc.data[end] != '\r' && doc.data[end] != '\n') ++end;
        raw_end = end;
        if (raw_end < doc.size) {
            if (doc.data[raw_end] == '\r') {
                if (raw_end + 1 >= doc.size || doc.data[raw_end + 1] != '\n') return PM_INI_LINE_SYNTAX;
                raw_end += 2; line.newline_size = 2;
            } else { ++raw_end; line.newline_size = 1; }
        }
        if (++lines > PM_INI_MAX_LINES || raw_end - pos > PM_INI_MAX_LINE_BYTES)
            return PM_INI_INPUT_LIMIT;
        for (i = pos; i < end; ++i)
            if ((doc.data[i] < 32 && doc.data[i] != '\t') || doc.data[i] == 127)
                return PM_INI_LINE_SYNTAX;
        line.offset = (uint32_t)pos; line.raw_size = (uint32_t)(raw_end - pos);
        line.body_size = (uint32_t)(end - pos);
        begin = pos; trim_end = end;
        while (begin < trim_end && space(doc.data[begin])) ++begin;
        while (trim_end > begin && space(doc.data[trim_end - 1])) --trim_end;
        if (begin < trim_end && doc.data[begin] == '[') {
            size_t name_begin = begin + 1, name_end = trim_end - 1;
            if (doc.data[trim_end - 1] != ']' || name_begin == name_end ||
                space(doc.data[name_begin]) || space(doc.data[name_end - 1])) return PM_INI_HEADER_SYNTAX;
            /* ASCII header identities are an explicit conservative port subset.
             * Values and opaque non-header lines may contain valid UTF-8. */
            for (i = name_begin; i < name_end; ++i)
                if (doc.data[i] >= 127 || doc.data[i] == '[' || doc.data[i] == ']' ||
                    doc.data[i] == '=') return PM_INI_HEADER_SYNTAX;
            current_section = section_index(doc.data + name_begin, name_end - name_begin,
                                            sections, section_count);
            saw_header = 1;
        } else if (begin < trim_end && doc.data[begin] != ';') {
            if (current_section < 0) {
                if (!is_base && !saw_header) return PM_INI_SCALAR_SYNTAX;
            } else {
                size_t equals = pos, key_begin, key_end;
                uint8_t first = doc.data[begin];
                if (first == '+' || first == '-' || first == '.' || first == '!') return PM_INI_ARRAY_UNSUPPORTED;
                while (equals < end && doc.data[equals] != '=') ++equals;
                if (equals == end) return PM_INI_SCALAR_SYNTAX;
                key_begin = pos; key_end = equals;
                while (key_begin < key_end && space(doc.data[key_begin])) ++key_begin;
                while (key_end > key_begin && space(doc.data[key_end - 1])) --key_end;
                if (key_begin == key_end || (!alpha(doc.data[key_begin]) && doc.data[key_begin] != '_'))
                    return PM_INI_SCALAR_SYNTAX;
                for (i = key_begin + 1; i < key_end; ++i) {
                    uint8_t c = doc.data[i];
                    if (!alpha(c) && !digit(c) && c != '_' && c != '.' &&
                        c != ':' && c != '/' && c != '-') return PM_INI_SCALAR_SYNTAX;
                }
                status = scalar(doc.data + equals + 1, end - equals - 1);
                if (status != PM_INI_OK) return status;
                line.left_size = (uint16_t)(equals - pos);
                line.key_offset = (uint32_t)key_begin; line.key_size = (uint16_t)(key_end - key_begin);
                line.value_offset = (uint32_t)(equals + 1); line.value_size = (uint16_t)(end - equals - 1);
            }
        }
        line.section = (int16_t)current_section;
        if (is_base) {
            work->lines[work->line_count] = line;
            if (current_section >= 0) work->section_last[current_section] = (int32_t)work->line_count;
            ++work->line_count;
        } else if (line.key_size) {
            status = add_winner(work, sources, source, line, result);
            if (status != PM_INI_OK) return status;
        }
        pos = raw_end;
    }
    return PM_INI_OK;
}

static void sort_winners(pm_ini_workspace *work) {
    uint32_t i, width;
    for (i = 0; i < work->winner_count; ++i) work->order[i] = (uint16_t)i;
    for (width = 1; width < work->winner_count; width *= 2) {
        uint32_t start;
        for (start = 0; start < work->winner_count; start += width * 2) {
            uint32_t a = start, middle = start + width, b, end, to = start;
            if (middle > work->winner_count) middle = work->winner_count;
            b = middle; end = start + 2 * width;
            if (end > work->winner_count) end = work->winner_count;
            while (a < middle || b < end) {
                if (b == end || (a < middle && work->winners[work->order[a]].ordinal <
                                              work->winners[work->order[b]].ordinal))
                    work->sort_temp[to++] = work->order[a++];
                else work->sort_temp[to++] = work->order[b++];
            }
        }
        memcpy(work->order, work->sort_temp, work->winner_count * sizeof(work->order[0]));
    }
}

static pm_ini_status append(writer *out, const uint8_t *bytes, size_t size) {
    size_t i;
    if (size > PM_INI_MAX_OUTPUT_BYTES - out->position) return PM_INI_OUTPUT_LIMIT;
    if (out->output && size) memcpy(out->output + out->position, bytes, size);
    for (i = 0; i < size; ++i) if (bytes[i] == '\n') ++out->lf_count;
    if (size) { out->last_lf = bytes[size - 1] == '\n'; out->has_bytes = 1; }
    out->position += size;
    return out->lf_count > PM_INI_MAX_LINES ? PM_INI_OUTPUT_LIMIT : PM_INI_OK;
}

static pm_ini_status append_additions(writer *out, pm_ini_workspace *work,
                                      const pm_ini_bytes *sources, size_t section,
                                      const uint8_t *newline, size_t newline_size) {
    uint32_t i;
    int found = 0;
    pm_ini_status status;
    for (i = 0; i < work->winner_count; ++i) {
        pm_ini_scratch_winner *w = &work->winners[work->order[i]];
        if (w->section != section || w->base_count || w->emitted) continue;
        if (!found && out->has_bytes && !out->last_lf) {
            status = append(out, newline, newline_size);
            if (status != PM_INI_OK) return status;
        }
        found = 1;
        status = append(out, sources[w->source].data + w->body_offset, w->body_size);
        if (status != PM_INI_OK) return status;
        status = append(out, newline, newline_size);
        if (status != PM_INI_OK) return status;
        w->emitted = 1;
    }
    return PM_INI_OK;
}

static pm_ini_status render(pm_ini_workspace *work, pm_ini_bytes base,
                            const pm_ini_bytes *sources, const pm_ini_bytes *sections,
                            size_t section_count, writer *out) {
    static const uint8_t crlf[] = {'\r', '\n'}, lf[] = {'\n'}, equals[] = {'='};
    static const uint8_t open[] = {'['}, close[] = {']'};
    const uint8_t *newline = work->preferred_crlf ? crlf : lf;
    size_t newline_size = work->preferred_crlf ? 2 : 1, section;
    uint32_t i;
    pm_ini_status status;
    for (i = 0; i < work->winner_count; ++i) work->winners[i].emitted = 0;
    status = append(out, base.data, work->bom_size);
    if (status != PM_INI_OK) return status;
    for (i = 0; i < work->line_count; ++i) {
        const pm_ini_scratch_line *line = &work->lines[i];
        int id = -1;
        if (line->key_size) id = find_winner(work, sources, (uint8_t)line->section,
                                            base.data + line->key_offset, line->key_size, NULL);
        if (id >= 0) {
            const pm_ini_scratch_winner *w = &work->winners[id];
            if (w->base_last == (int32_t)i) {
                status = append(out, base.data + line->offset, line->left_size);
                if (status != PM_INI_OK) return status;
                status = append(out, equals, 1);
                if (status != PM_INI_OK) return status;
                status = append(out, sources[w->source].data + w->value_offset, w->value_size);
                if (status != PM_INI_OK) return status;
                if (line->newline_size) status = append(out, base.data + line->offset + line->body_size, line->newline_size);
                else status = append(out, newline, newline_size);
                if (status != PM_INI_OK) return status;
            }
        } else {
            status = append(out, base.data + line->offset, line->raw_size);
            if (status != PM_INI_OK) return status;
        }
        if (line->section >= 0 && work->section_last[line->section] == (int32_t)i) {
            status = append_additions(out, work, sources, (size_t)line->section, newline, newline_size);
            if (status != PM_INI_OK) return status;
        }
    }
    for (section = 0; section < section_count; ++section) {
        int has_addition = 0;
        for (i = 0; i < work->winner_count; ++i)
            if (work->winners[i].section == section && !work->winners[i].base_count &&
                !work->winners[i].emitted) { has_addition = 1; break; }
        if (!has_addition) continue;
        if (out->has_bytes && !out->last_lf) return PM_INI_NEW_SECTION_BOUNDARY;
        status = append(out, open, 1);
        if (status != PM_INI_OK) return status;
        status = append(out, sections[section].data, sections[section].size);
        if (status != PM_INI_OK) return status;
        status = append(out, close, 1);
        if (status != PM_INI_OK) return status;
        status = append(out, newline, newline_size);
        if (status != PM_INI_OK) return status;
        status = append_additions(out, work, sources, section, newline, newline_size);
        if (status != PM_INI_OK) return status;
    }
    if (out->lf_count + (out->has_bytes && !out->last_lf) > PM_INI_MAX_LINES)
        return PM_INI_OUTPUT_LIMIT;
    return PM_INI_OK;
}

pm_ini_status pm_ini_merge(const pm_ini_bytes *base,
                          const pm_ini_bytes *sources, size_t source_count,
                          const pm_ini_bytes *sections, size_t section_count,
                          pm_ini_workspace *workspace,
                          uint8_t *output, size_t output_capacity,
                          pm_ini_result *result) {
    memory_range writable[3];
    pm_ini_result r = {0};
    pm_ini_status status;
    size_t i, j;
    uint32_t crlf = 0, lf = 0;
    writer measured = {0}, written = {0};
    if (!base || !sources || !sections || !workspace || !result ||
        !source_count || source_count > PM_INI_MAX_SOURCES ||
        !section_count || section_count > PM_INI_MAX_SECTIONS ||
        output_capacity > PM_INI_MAX_OUTPUT_BYTES || (output_capacity && !output) ||
        (uintptr_t)workspace % _Alignof(pm_ini_workspace)) return PM_INI_INVALID_ARGUMENT;
    if (!range_of(workspace, sizeof(*workspace), &writable[0]) ||
        !range_of(output, output_capacity, &writable[1]) ||
        !range_of(result, sizeof(*result), &writable[2])) return PM_INI_INVALID_ARGUMENT;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < i; ++j)
            if (overlap(writable[i], writable[j])) return PM_INI_OVERLAP;
    /* Check immutable metadata before reading through it. */
    {
        int a = clashes(base, sizeof(*base), writable);
        int b = clashes(sources, source_count * sizeof(*sources), writable);
        int c = clashes(sections, section_count * sizeof(*sections), writable);
        if (a < 0 || b < 0 || c < 0) return PM_INI_INVALID_ARGUMENT;
        if (a || b || c) return PM_INI_OVERLAP;
    }
    {
        int collision = clashes(base->data, base->size, writable);
        if (collision < 0) return PM_INI_INVALID_ARGUMENT;
        if (collision) return PM_INI_OVERLAP;
    }
    for (i = 0; i < source_count; ++i) {
        int collision = clashes(sources[i].data, sources[i].size, writable);
        if (collision < 0) return PM_INI_INVALID_ARGUMENT;
        if (collision) return PM_INI_OVERLAP;
    }
    for (i = 0; i < section_count; ++i) {
        int collision = clashes(sections[i].data, sections[i].size, writable);
        if (collision < 0) return PM_INI_INVALID_ARGUMENT;
        if (collision) return PM_INI_OVERLAP;
    }
    if (base->size > PM_INI_MAX_INPUT_BYTES) return finish(result, &r, PM_INI_INPUT_LIMIT);
    for (i = 0; i < source_count; ++i)
        if (sources[i].size > PM_INI_MAX_INPUT_BYTES) return finish(result, &r, PM_INI_INPUT_LIMIT);
    status = selected_names(sections, section_count);
    if (status != PM_INI_OK) return finish(result, &r, status);
    memset(workspace, 0, sizeof(*workspace));
    for (i = 0; i < section_count; ++i) workspace->section_last[i] = -1;
    status = parse_document(workspace, *base, 1, 0, sources, sections, section_count, &r);
    if (status != PM_INI_OK) return finish(result, &r, status);
    for (i = 0; i < workspace->line_count; ++i) {
        if (workspace->lines[i].newline_size == 2) ++crlf;
        else if (workspace->lines[i].newline_size == 1) ++lf;
    }
    workspace->preferred_crlf = (uint8_t)(crlf && crlf >= lf);
    for (i = 0; i < source_count; ++i) {
        uint32_t before = r.source_assignments;
        status = parse_document(workspace, sources[i], 0, i, sources, sections, section_count, &r);
        if (status != PM_INI_OK) return finish(result, &r, status);
        if (r.source_assignments == before) return finish(result, &r, PM_INI_SOURCE_EMPTY);
    }
    for (i = 0; i < workspace->line_count; ++i) {
        const pm_ini_scratch_line *line = &workspace->lines[i];
        if (line->key_size) {
            int id = find_winner(workspace, sources, (uint8_t)line->section,
                                 base->data + line->key_offset, line->key_size, NULL);
            if (id >= 0) {
                workspace->winners[id].base_last = (int32_t)i;
                ++workspace->winners[id].base_count;
            }
        }
    }
    for (i = 0; i < workspace->winner_count; ++i) {
        if (workspace->winners[i].base_count) {
            ++r.replacements;
            r.base_duplicates_removed += workspace->winners[i].base_count - 1u;
        } else ++r.additions;
    }
    r.scalar_winners = workspace->winner_count;
    r.bom_preserved = 1; r.preferred_crlf = workspace->preferred_crlf;
    sort_winners(workspace);
    status = render(workspace, *base, sources, sections, section_count, &measured);
    if (status != PM_INI_OK) return finish(result, &r, status);
    r.required_output_bytes = measured.position;
    r.output_lines = measured.lf_count + (measured.has_bytes && !measured.last_lf);
    if (output_capacity < measured.position) return finish(result, &r, PM_INI_OUTPUT_LIMIT);
    written.output = output;
    status = render(workspace, *base, sources, sections, section_count, &written);
    if (status != PM_INI_OK || written.position != measured.position)
        return finish(result, &r, status == PM_INI_OK ? PM_INI_OUTPUT_LIMIT : status);
    r.output_bytes = written.position;
    return finish(result, &r, PM_INI_OK);
}

const char *pm_ini_status_name(pm_ini_status status) {
    switch (status) {
    case PM_INI_OK: return "ok";
    case PM_INI_INVALID_ARGUMENT: return "invalid_argument";
    case PM_INI_INPUT_LIMIT: return "input_limit";
    case PM_INI_SECTION_LIMIT: return "section_limit";
    case PM_INI_OVERLAP: return "overlap";
    case PM_INI_INVALID_UTF8: return "invalid_utf8";
    case PM_INI_NUL: return "nul";
    case PM_INI_LINE_SYNTAX: return "line_syntax";
    case PM_INI_HEADER_SYNTAX: return "header_syntax";
    case PM_INI_SCALAR_SYNTAX: return "scalar_syntax";
    case PM_INI_ARRAY_UNSUPPORTED: return "array_unsupported";
    case PM_INI_SOURCE_EMPTY: return "source_empty";
    case PM_INI_WINNER_LIMIT: return "winner_limit";
    case PM_INI_OUTPUT_LIMIT: return "output_limit";
    case PM_INI_NEW_SECTION_BOUNDARY: return "new_section_boundary";
    default: return "unknown_status";
    }
}
