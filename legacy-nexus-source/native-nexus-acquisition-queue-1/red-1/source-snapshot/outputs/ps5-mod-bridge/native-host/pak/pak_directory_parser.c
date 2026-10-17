#include "pak_directory_parser.h"

#include <limits.h>
#include <string.h>

typedef struct directory_cursor {
    const uint8_t *bytes;
    size_t size;
    size_t position;
} directory_cursor;

typedef struct directory_state {
    const pak_directory_limits_v1 *limits;
    pak_directory_record_v1 *directories;
    size_t directories_capacity;
    pak_directory_entry_v1 *entries;
    size_t entries_capacity;
    uint8_t *arena;
    size_t arena_capacity;
    size_t arena_used;
    uint32_t directories_written;
    uint32_t entries_written;
    uint32_t tombstones_written;
} directory_state;

typedef struct fstring_span {
    const uint8_t *bytes;
    size_t length;
    uint32_t input_encoding;
} fstring_span;

static uint32_t load_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint16_t load_le16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static int32_t signed_i32_from_bits(uint32_t bits)
{
    uint32_t magnitude;
    if (bits <= (uint32_t)INT32_MAX) return (int32_t)bits;
    magnitude = (~bits) + 1u;
    if (magnitude == UINT32_C(0x80000000)) return INT32_MIN;
    return -(int32_t)magnitude;
}

static void result_init(pak_directory_result_v1 *result,
                        pak_directory_status status,
                        pak_directory_reason reason)
{
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_DIRECTORY_PARSER_ABI_VERSION;
    result->status = status;
    result->reason = reason;
}

static int valid_limits(const pak_directory_limits_v1 *limits)
{
    if (limits == NULL || limits->struct_size != sizeof(*limits) ||
        limits->abi_version != PAK_DIRECTORY_PARSER_ABI_VERSION ||
        limits->max_index_bytes > PAK_DIRECTORY_MAX_INDEX_BYTES ||
        limits->max_encoded_blob_bytes > PAK_DIRECTORY_MAX_ENCODED_BLOB_BYTES ||
        limits->max_directories > PAK_DIRECTORY_MAX_DIRECTORIES ||
        limits->max_entries > PAK_DIRECTORY_MAX_ENTRIES ||
        limits->max_component_utf8_bytes > PAK_DIRECTORY_MAX_COMPONENT_UTF8_BYTES ||
        limits->max_total_name_bytes > PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES ||
        limits->reserved[0] != 0u || limits->reserved[1] != 0u) {
        return 0;
    }
    return 1;
}

static pak_directory_status read_bytes(directory_cursor *cursor, size_t count,
                                       const uint8_t **bytes,
                                       pak_directory_reason *reason)
{
    if (cursor->position > cursor->size || count > cursor->size - cursor->position) {
        *reason = PAK_DIRECTORY_REASON_TRUNCATED;
        return PAK_DIRECTORY_STATUS_MALFORMED;
    }
    if (bytes != NULL) *bytes = cursor->bytes + cursor->position;
    cursor->position += count;
    return PAK_DIRECTORY_STATUS_OK;
}

static pak_directory_status read_i32(directory_cursor *cursor, int32_t *value,
                                     pak_directory_reason *reason)
{
    const uint8_t *bytes;
    pak_directory_status status = read_bytes(cursor, 4u, &bytes, reason);
    if (status == PAK_DIRECTORY_STATUS_OK) {
        *value = signed_i32_from_bits(load_le32(bytes));
    }
    return status;
}

static pak_directory_status read_fstring(directory_cursor *cursor,
                                         fstring_span *string,
                                         pak_directory_reason *reason)
{
    int32_t signed_length;
    const uint8_t *bytes;
    size_t payload_bytes;
    pak_directory_status status = read_i32(cursor, &signed_length, reason);
    if (status != PAK_DIRECTORY_STATUS_OK) return status;

    if (signed_length == 0) {
        string->bytes = NULL;
        string->length = 0u;
        string->input_encoding = PAK_DIRECTORY_STRING_UTF8;
        return PAK_DIRECTORY_STATUS_OK;
    }
    if (signed_length == INT32_MIN) {
        *reason = PAK_DIRECTORY_REASON_BAD_FSTRING_LENGTH;
        return PAK_DIRECTORY_STATUS_MALFORMED;
    }

    if (signed_length > 0) {
        payload_bytes = (size_t)(uint32_t)signed_length;
        status = read_bytes(cursor, payload_bytes, &bytes, reason);
        if (status != PAK_DIRECTORY_STATUS_OK) return status;
        if (bytes[payload_bytes - 1u] != 0u) {
            *reason = PAK_DIRECTORY_REASON_FSTRING_TERMINATOR;
            return PAK_DIRECTORY_STATUS_MALFORMED;
        }
        string->bytes = bytes;
        string->length = payload_bytes - 1u;
        string->input_encoding = PAK_DIRECTORY_STRING_UTF8;
        return PAK_DIRECTORY_STATUS_OK;
    }

    {
        uint32_t units = (uint32_t)(-(int64_t)signed_length);
        if (units == 0u || (uint64_t)units * 2u > (uint64_t)SIZE_MAX) {
            *reason = PAK_DIRECTORY_REASON_BAD_FSTRING_LENGTH;
            return PAK_DIRECTORY_STATUS_MALFORMED;
        }
        payload_bytes = (size_t)units * 2u;
    }
    status = read_bytes(cursor, payload_bytes, &bytes, reason);
    if (status != PAK_DIRECTORY_STATUS_OK) return status;
    if (bytes[payload_bytes - 2u] != 0u || bytes[payload_bytes - 1u] != 0u) {
        *reason = PAK_DIRECTORY_REASON_FSTRING_TERMINATOR;
        return PAK_DIRECTORY_STATUS_MALFORMED;
    }
    string->bytes = bytes;
    string->length = payload_bytes - 2u;
    string->input_encoding = PAK_DIRECTORY_STRING_UTF16LE;
    return PAK_DIRECTORY_STATUS_OK;
}

static pak_directory_reason validate_utf8(const uint8_t *bytes, size_t length)
{
    size_t i = 0u;
    while (i < length) {
        uint8_t first = bytes[i];
        size_t count;
        uint8_t second_min = 0x80u;
        uint8_t second_max = 0xBFu;
        size_t j;

        if (first == 0u) return PAK_DIRECTORY_REASON_INVALID_UTF8;
        if (first <= 0x7Fu) {
            i++;
            continue;
        } else if (first >= 0xC2u && first <= 0xDFu) {
            count = 2u;
        } else if (first >= 0xE0u && first <= 0xEFu) {
            count = 3u;
            if (first == 0xE0u) second_min = 0xA0u;
            if (first == 0xEDu) second_max = 0x9Fu;
        } else if (first >= 0xF0u && first <= 0xF4u) {
            count = 4u;
            if (first == 0xF0u) second_min = 0x90u;
            if (first == 0xF4u) second_max = 0x8Fu;
        } else {
            return PAK_DIRECTORY_REASON_INVALID_UTF8;
        }
        if (count > length - i) return PAK_DIRECTORY_REASON_INVALID_UTF8;
        if (bytes[i + 1u] < second_min || bytes[i + 1u] > second_max) {
            return PAK_DIRECTORY_REASON_INVALID_UTF8;
        }
        for (j = 2u; j < count; ++j) {
            if (bytes[i + j] < 0x80u || bytes[i + j] > 0xBFu) {
                return PAK_DIRECTORY_REASON_INVALID_UTF8;
            }
        }
        i += count;
    }
    return PAK_DIRECTORY_REASON_NONE;
}

static int utf8_bytes_for_codepoint(uint32_t codepoint)
{
    if (codepoint <= 0x7Fu) return 1;
    if (codepoint <= 0x7FFu) return 2;
    if (codepoint <= 0xFFFFu) return 3;
    return 4;
}

static pak_directory_reason measure_utf16le(const uint8_t *bytes,
                                            size_t byte_length,
                                            size_t *utf8_length)
{
    size_t i = 0u;
    size_t total = 0u;
    if ((byte_length & 1u) != 0u) return PAK_DIRECTORY_REASON_INVALID_UTF16LE;
    while (i < byte_length) {
        uint16_t first = load_le16(bytes + i);
        uint32_t codepoint;
        i += 2u;
        if (first == 0u) return PAK_DIRECTORY_REASON_INVALID_UTF16LE;
        if (first >= 0xD800u && first <= 0xDBFFu) {
            uint16_t second;
            if (byte_length - i < 2u) return PAK_DIRECTORY_REASON_INVALID_UTF16LE;
            second = load_le16(bytes + i);
            if (second < 0xDC00u || second > 0xDFFFu) {
                return PAK_DIRECTORY_REASON_INVALID_UTF16LE;
            }
            i += 2u;
            codepoint = UINT32_C(0x10000) +
                        (((uint32_t)first - UINT32_C(0xD800)) << 10) +
                        ((uint32_t)second - UINT32_C(0xDC00));
        } else if (first >= 0xDC00u && first <= 0xDFFFu) {
            return PAK_DIRECTORY_REASON_INVALID_UTF16LE;
        } else {
            codepoint = first;
        }
        if ((size_t)utf8_bytes_for_codepoint(codepoint) > SIZE_MAX - total) {
            return PAK_DIRECTORY_REASON_INVALID_UTF16LE;
        }
        total += (size_t)utf8_bytes_for_codepoint(codepoint);
    }
    *utf8_length = total;
    return PAK_DIRECTORY_REASON_NONE;
}

static size_t encode_codepoint_utf8(uint32_t codepoint, uint8_t *destination)
{
    if (codepoint <= 0x7Fu) {
        destination[0] = (uint8_t)codepoint;
        return 1u;
    }
    if (codepoint <= 0x7FFu) {
        destination[0] = (uint8_t)(0xC0u | (codepoint >> 6));
        destination[1] = (uint8_t)(0x80u | (codepoint & 0x3Fu));
        return 2u;
    }
    if (codepoint <= 0xFFFFu) {
        destination[0] = (uint8_t)(0xE0u | (codepoint >> 12));
        destination[1] = (uint8_t)(0x80u | ((codepoint >> 6) & 0x3Fu));
        destination[2] = (uint8_t)(0x80u | (codepoint & 0x3Fu));
        return 3u;
    }
    destination[0] = (uint8_t)(0xF0u | (codepoint >> 18));
    destination[1] = (uint8_t)(0x80u | ((codepoint >> 12) & 0x3Fu));
    destination[2] = (uint8_t)(0x80u | ((codepoint >> 6) & 0x3Fu));
    destination[3] = (uint8_t)(0x80u | (codepoint & 0x3Fu));
    return 4u;
}

static size_t encode_utf16le(const uint8_t *bytes, size_t byte_length,
                             uint8_t *destination)
{
    size_t i = 0u;
    size_t out = 0u;
    while (i < byte_length) {
        uint16_t first = load_le16(bytes + i);
        uint32_t codepoint;
        i += 2u;
        if (first >= 0xD800u && first <= 0xDBFFu) {
            uint16_t second = load_le16(bytes + i);
            i += 2u;
            codepoint = UINT32_C(0x10000) +
                        (((uint32_t)first - UINT32_C(0xD800)) << 10) +
                        ((uint32_t)second - UINT32_C(0xDC00));
        } else {
            codepoint = first;
        }
        out += encode_codepoint_utf8(codepoint, destination + out);
    }
    return out;
}

static pak_directory_status append_string(directory_state *state,
                                          const fstring_span *string,
                                          uint32_t *name_offset,
                                          uint32_t *name_length,
                                          pak_directory_reason *reason)
{
    size_t utf8_length = string->length;
    size_t stored_bytes;
    size_t start;

    if (string->input_encoding == PAK_DIRECTORY_STRING_UTF8) {
        *reason = validate_utf8(string->bytes, string->length);
        if (*reason != PAK_DIRECTORY_REASON_NONE) return PAK_DIRECTORY_STATUS_MALFORMED;
    } else {
        *reason = measure_utf16le(string->bytes, string->length, &utf8_length);
        if (*reason != PAK_DIRECTORY_REASON_NONE) return PAK_DIRECTORY_STATUS_MALFORMED;
    }
    if (utf8_length > state->limits->max_component_utf8_bytes || utf8_length > UINT32_MAX) {
        *reason = PAK_DIRECTORY_REASON_COMPONENT_LIMIT;
        return PAK_DIRECTORY_STATUS_RESOURCE_LIMIT;
    }
    if (utf8_length == SIZE_MAX) {
        *reason = PAK_DIRECTORY_REASON_NAME_ARENA_LIMIT;
        return PAK_DIRECTORY_STATUS_RESOURCE_LIMIT;
    }
    stored_bytes = utf8_length + 1u;
    if (state->arena_used > state->limits->max_total_name_bytes ||
        stored_bytes > state->limits->max_total_name_bytes - state->arena_used ||
        state->arena_used > state->arena_capacity ||
        stored_bytes > state->arena_capacity - state->arena_used) {
        *reason = PAK_DIRECTORY_REASON_NAME_ARENA_LIMIT;
        return PAK_DIRECTORY_STATUS_RESOURCE_LIMIT;
    }

    start = state->arena_used;
    state->arena_used += stored_bytes;
    if (string->input_encoding == PAK_DIRECTORY_STRING_UTF8) {
        if (string->length != 0u) {
            memcpy(state->arena + start, string->bytes, string->length);
        }
    } else if (encode_utf16le(string->bytes, string->length,
                              state->arena + start) != utf8_length) {
        *reason = PAK_DIRECTORY_REASON_INVALID_UTF16LE;
        return PAK_DIRECTORY_STATUS_MALFORMED;
    }
    state->arena[start + utf8_length] = 0u;
    *name_offset = (uint32_t)start;
    *name_length = (uint32_t)utf8_length;
    return PAK_DIRECTORY_STATUS_OK;
}

static pak_directory_status fail_parse(directory_state *state,
                                       pak_directory_result_v1 *result,
                                       pak_directory_status status,
                                       pak_directory_reason reason)
{
    if (state != NULL) {
        if (state->directories != NULL && state->directories_written != 0u) {
            memset(state->directories, 0,
                   (size_t)state->directories_written * sizeof(*state->directories));
        }
        if (state->entries != NULL && state->entries_written != 0u) {
            memset(state->entries, 0,
                   (size_t)state->entries_written * sizeof(*state->entries));
        }
        if (state->arena != NULL && state->arena_used != 0u) {
            memset(state->arena, 0, state->arena_used);
        }
    }
    result_init(result, status, reason);
    return status;
}

void pak_directory_limits_v1_default(pak_directory_limits_v1 *limits)
{
    if (limits == NULL) return;
    memset(limits, 0, sizeof(*limits));
    limits->struct_size = sizeof(*limits);
    limits->abi_version = PAK_DIRECTORY_PARSER_ABI_VERSION;
    limits->max_index_bytes = PAK_DIRECTORY_MAX_INDEX_BYTES;
    limits->max_encoded_blob_bytes = PAK_DIRECTORY_MAX_ENCODED_BLOB_BYTES;
    limits->max_directories = PAK_DIRECTORY_MAX_DIRECTORIES;
    limits->max_entries = PAK_DIRECTORY_MAX_ENTRIES;
    limits->max_component_utf8_bytes = PAK_DIRECTORY_MAX_COMPONENT_UTF8_BYTES;
    limits->max_total_name_bytes = PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES;
}

pak_directory_status pak_directory_read_classic_v11(
    const uint8_t *directory_index,
    size_t directory_index_bytes,
    uint64_t encoded_blob_bytes,
    const pak_directory_limits_v1 *limits,
    pak_directory_record_v1 *directories_out,
    size_t directories_capacity,
    pak_directory_entry_v1 *entries_out,
    size_t entries_capacity,
    uint8_t *string_arena,
    size_t string_arena_capacity,
    pak_directory_result_v1 *result)
{
    directory_cursor cursor;
    directory_state state;
    pak_directory_status status;
    pak_directory_reason reason = PAK_DIRECTORY_REASON_NONE;
    int32_t directory_count_signed;
    uint32_t directory_count;
    uint32_t directory_row;

    if (result == NULL) return PAK_DIRECTORY_STATUS_BAD_ARGUMENT;
    result_init(result, PAK_DIRECTORY_STATUS_BAD_ARGUMENT,
                PAK_DIRECTORY_REASON_BAD_ARGUMENT);
    if (!valid_limits(limits)) {
        result->reason = PAK_DIRECTORY_REASON_BAD_LIMITS;
        return PAK_DIRECTORY_STATUS_BAD_ARGUMENT;
    }
    if ((directory_index == NULL && directory_index_bytes != 0u) ||
        (directories_out == NULL && directories_capacity != 0u) ||
        (entries_out == NULL && entries_capacity != 0u) ||
        (string_arena == NULL && string_arena_capacity != 0u) ||
        directories_capacity > SIZE_MAX / sizeof(*directories_out) ||
        entries_capacity > SIZE_MAX / sizeof(*entries_out)) {
        return PAK_DIRECTORY_STATUS_BAD_ARGUMENT;
    }

    memset(&state, 0, sizeof(state));
    state.limits = limits;
    state.directories = directories_out;
    state.directories_capacity = directories_capacity;
    state.entries = entries_out;
    state.entries_capacity = entries_capacity;
    state.arena = string_arena;
    state.arena_capacity = string_arena_capacity;

    if (directory_index_bytes > limits->max_index_bytes) {
        return fail_parse(&state, result, PAK_DIRECTORY_STATUS_RESOURCE_LIMIT,
                          PAK_DIRECTORY_REASON_INDEX_TOO_LARGE);
    }
    if (encoded_blob_bytes > limits->max_encoded_blob_bytes) {
        return fail_parse(&state, result, PAK_DIRECTORY_STATUS_RESOURCE_LIMIT,
                          PAK_DIRECTORY_REASON_ENCODED_BLOB_TOO_LARGE);
    }
    if (directory_index_bytes == 0u) {
        return fail_parse(&state, result, PAK_DIRECTORY_STATUS_MALFORMED,
                          PAK_DIRECTORY_REASON_TRUNCATED);
    }

    cursor.bytes = directory_index;
    cursor.size = directory_index_bytes;
    cursor.position = 0u;
    status = read_i32(&cursor, &directory_count_signed, &reason);
    if (status != PAK_DIRECTORY_STATUS_OK) {
        return fail_parse(&state, result, status, reason);
    }
    if (directory_count_signed < 0) {
        return fail_parse(&state, result, PAK_DIRECTORY_STATUS_MALFORMED,
                          PAK_DIRECTORY_REASON_BAD_COUNT);
    }
    directory_count = (uint32_t)directory_count_signed;
    if (directory_count > limits->max_directories) {
        return fail_parse(&state, result, PAK_DIRECTORY_STATUS_RESOURCE_LIMIT,
                          PAK_DIRECTORY_REASON_DIRECTORY_LIMIT);
    }
    if (directory_count > directories_capacity) {
        return fail_parse(&state, result, PAK_DIRECTORY_STATUS_RESOURCE_LIMIT,
                          PAK_DIRECTORY_REASON_OUTPUT_CAPACITY);
    }

    for (directory_row = 0u; directory_row < directory_count; ++directory_row) {
        fstring_span directory_name;
        uint32_t name_offset, name_length;
        int32_t file_count_signed;
        uint32_t file_count;
        uint32_t first_entry = state.entries_written;
        uint32_t file_index;

        status = read_fstring(&cursor, &directory_name, &reason);
        if (status != PAK_DIRECTORY_STATUS_OK) {
            return fail_parse(&state, result, status, reason);
        }
        status = append_string(&state, &directory_name, &name_offset,
                               &name_length, &reason);
        if (status != PAK_DIRECTORY_STATUS_OK) {
            return fail_parse(&state, result, status, reason);
        }
        directories_out[directory_row].name_offset = name_offset;
        directories_out[directory_row].name_length = name_length;
        directories_out[directory_row].input_encoding = directory_name.input_encoding;
        directories_out[directory_row].first_entry = first_entry;
        directories_out[directory_row].entry_count = 0u;
        directories_out[directory_row].reserved = 0u;
        state.directories_written++;

        status = read_i32(&cursor, &file_count_signed, &reason);
        if (status != PAK_DIRECTORY_STATUS_OK) {
            return fail_parse(&state, result, status, reason);
        }
        if (file_count_signed < 0) {
            return fail_parse(&state, result, PAK_DIRECTORY_STATUS_MALFORMED,
                              PAK_DIRECTORY_REASON_BAD_COUNT);
        }
        file_count = (uint32_t)file_count_signed;
        if (state.entries_written > limits->max_entries ||
            file_count > limits->max_entries - state.entries_written) {
            return fail_parse(&state, result, PAK_DIRECTORY_STATUS_RESOURCE_LIMIT,
                              PAK_DIRECTORY_REASON_ENTRY_LIMIT);
        }
        if (state.entries_written > entries_capacity ||
            file_count > entries_capacity - state.entries_written) {
            return fail_parse(&state, result, PAK_DIRECTORY_STATUS_RESOURCE_LIMIT,
                              PAK_DIRECTORY_REASON_OUTPUT_CAPACITY);
        }

        for (file_index = 0u; file_index < file_count; ++file_index) {
            fstring_span filename;
            uint32_t file_name_offset, file_name_length;
            int32_t location;
            uint32_t flags = 0u;

            status = read_fstring(&cursor, &filename, &reason);
            if (status != PAK_DIRECTORY_STATUS_OK) {
                return fail_parse(&state, result, status, reason);
            }
            status = read_i32(&cursor, &location, &reason);
            if (status != PAK_DIRECTORY_STATUS_OK) {
                return fail_parse(&state, result, status, reason);
            }
            if (location == INT32_MIN) {
                flags = PAK_DIRECTORY_ENTRY_TOMBSTONE;
            } else if (location < 0) {
                return fail_parse(&state, result, PAK_DIRECTORY_STATUS_UNSUPPORTED,
                                  PAK_DIRECTORY_REASON_LOCATION_UNSUPPORTED);
            } else if ((uint64_t)(uint32_t)location >= encoded_blob_bytes) {
                return fail_parse(&state, result, PAK_DIRECTORY_STATUS_MALFORMED,
                                  PAK_DIRECTORY_REASON_LOCATION_OUT_OF_RANGE);
            }

            status = append_string(&state, &filename, &file_name_offset,
                                   &file_name_length, &reason);
            if (status != PAK_DIRECTORY_STATUS_OK) {
                return fail_parse(&state, result, status, reason);
            }
            entries_out[state.entries_written].name_offset = file_name_offset;
            entries_out[state.entries_written].name_length = file_name_length;
            entries_out[state.entries_written].input_encoding = filename.input_encoding;
            entries_out[state.entries_written].location = location;
            entries_out[state.entries_written].flags = flags;
            entries_out[state.entries_written].reserved = 0u;
            state.entries_written++;
            if (flags != 0u) state.tombstones_written++;
        }
        directories_out[directory_row].entry_count = state.entries_written - first_entry;
    }

    if (cursor.position != cursor.size) {
        return fail_parse(&state, result, PAK_DIRECTORY_STATUS_MALFORMED,
                          PAK_DIRECTORY_REASON_TRAILING_BYTES);
    }

    result_init(result, PAK_DIRECTORY_STATUS_OK, PAK_DIRECTORY_REASON_NONE);
    result->metadata_complete = 1u;
    result->directory_count = state.directories_written;
    result->entry_count = state.entries_written;
    result->tombstone_count = state.tombstones_written;
    result->string_bytes = (uint32_t)state.arena_used;
    return PAK_DIRECTORY_STATUS_OK;
}
