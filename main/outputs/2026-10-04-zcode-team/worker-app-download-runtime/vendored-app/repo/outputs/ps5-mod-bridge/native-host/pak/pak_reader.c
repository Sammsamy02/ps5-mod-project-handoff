#include "pak_reader.h"

#include <limits.h>
#include <string.h>

#include <mbedtls/sha1.h>

#define PAK_V11_MAGIC 0x5A6F12E1u
#define PAK_V11_VERSION 11u

typedef struct pak_reader_state {
    const pak_source_v1 *source;
    const pak_limits_v1 *limits;
    uint8_t *workspace;
    size_t workspace_bytes;
    pak_primary_result_v1 *result;
    uint64_t primary_position;
    mbedtls_sha1_context sha1;
    uint32_t sha1_started;
} pak_reader_state;

static pak_status fail(pak_reader_state *state, pak_status status, pak_reason reason)
{
    state->result->status = status;
    state->result->reason = reason;
    return status;
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const uint8_t *p)
{
    return (uint64_t)read_le32(p) | ((uint64_t)read_le32(p + 4) << 32);
}

static int64_t signed_le32(const uint8_t *p)
{
    uint32_t value = read_le32(p);
    if ((value & 0x80000000u) == 0) return (int64_t)value;
    return (int64_t)value - 0x100000000LL;
}

static pak_status provider_status(pak_reader_state *state, pak_provider_status status)
{
    switch (status) {
        case PAK_PROVIDER_IO_ERROR:
            return fail(state, PAK_STATUS_PROVIDER_ERROR, PAK_REASON_PROVIDER_IO);
        case PAK_PROVIDER_UNAVAILABLE:
            return fail(state, PAK_STATUS_PROVIDER_ERROR, PAK_REASON_PROVIDER_UNAVAILABLE);
        case PAK_PROVIDER_CHANGED:
            return fail(state, PAK_STATUS_PROVIDER_CHANGED, PAK_REASON_PROVIDER_CHANGED);
        case PAK_PROVIDER_CANCELLED:
            return fail(state, PAK_STATUS_CANCELLED, PAK_REASON_PROVIDER_CANCELLED);
        case PAK_PROVIDER_CONTRACT_ERROR:
            return fail(state, PAK_STATUS_PROVIDER_CONTRACT, PAK_REASON_PROVIDER_CONTRACT);
        case PAK_PROVIDER_OK:
        default:
            return fail(state, PAK_STATUS_PROVIDER_CONTRACT, PAK_REASON_PROVIDER_CONTRACT);
    }
}

static pak_status call_read(pak_reader_state *state, uint64_t offset,
                            uint8_t *destination, uint32_t requested)
{
    uint32_t actual = 0;
    pak_provider_status callback_status;
    pak_primary_result_v1 *result = state->result;

    if (requested == 0u || requested > state->limits->max_read_bytes ||
        requested > state->workspace_bytes || offset > state->source->logical_size ||
        (uint64_t)requested > state->source->logical_size - offset) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_INDEX_RANGE);
    }
    if (result->read_calls >= state->limits->max_read_calls) {
        return fail(state, PAK_STATUS_RESOURCE_LIMIT, PAK_REASON_READ_CALL_LIMIT);
    }
    if (result->requested_read_bytes > state->limits->max_total_read_bytes ||
        requested > state->limits->max_total_read_bytes - result->requested_read_bytes) {
        return fail(state, PAK_STATUS_RESOURCE_LIMIT, PAK_REASON_READ_LIMIT);
    }

    /* Reserve the full callback request before crossing the trusted provider boundary. */
    result->read_calls++;
    result->requested_read_bytes += requested;
    callback_status = state->source->read_at(state->source->context, offset,
                                              destination, requested, &actual);
    if (actual > requested) {
        return fail(state, PAK_STATUS_PROVIDER_CONTRACT, PAK_REASON_PROVIDER_CONTRACT);
    }
    if (callback_status != PAK_PROVIDER_OK) return provider_status(state, callback_status);
    if (actual != requested) {
        return fail(state, PAK_STATUS_PROVIDER_SHORT_READ, PAK_REASON_PROVIDER_SHORT_READ);
    }
    return PAK_STATUS_OK;
}

static pak_status read_footer(pak_reader_state *state, uint64_t offset, uint8_t *footer)
{
    uint32_t copied = 0;
    while (copied < PAK_V11_FOOTER_BYTES) {
        uint32_t take = PAK_V11_FOOTER_BYTES - copied;
        pak_status status;
        if (take > state->limits->max_read_bytes) take = state->limits->max_read_bytes;
        if (take > state->workspace_bytes) take = (uint32_t)state->workspace_bytes;
        status = call_read(state, offset + copied, state->workspace, take);
        if (status != PAK_STATUS_OK) return status;
        memcpy(footer + copied, state->workspace, take);
        copied += take;
    }
    return PAK_STATUS_OK;
}

static pak_status read_primary_bytes(pak_reader_state *state, uint8_t *destination, uint64_t count)
{
    uint64_t copied = 0;
    uint64_t remaining = state->result->primary_index_size - state->primary_position;
    if (count > remaining) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_INDEX_RANGE);
    }
    while (copied < count) {
        uint64_t left = count - copied;
        uint32_t take = left > state->limits->max_read_bytes ?
                        state->limits->max_read_bytes : (uint32_t)left;
        uint64_t absolute_offset;
        pak_status status;
        if (take > state->workspace_bytes) take = (uint32_t)state->workspace_bytes;
        if (state->result->primary_index_offset > UINT64_MAX - state->primary_position) {
            return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_INDEX_RANGE);
        }
        absolute_offset = state->result->primary_index_offset + state->primary_position;
        status = call_read(state, absolute_offset, state->workspace, take);
        if (status != PAK_STATUS_OK) return status;
        if (mbedtls_sha1_update(&state->sha1, state->workspace, take) != 0) {
            return fail(state, PAK_STATUS_INTERNAL_ERROR, PAK_REASON_SHA1_BACKEND);
        }
        if (destination != NULL && destination + copied != state->workspace) {
            memcpy(destination + copied, state->workspace, take);
        }
        state->primary_position += take;
        copied += take;
    }
    return PAK_STATUS_OK;
}

static pak_status read_primary_u32(pak_reader_state *state, uint32_t *value)
{
    uint8_t bytes[4];
    pak_status status = read_primary_bytes(state, bytes, sizeof(bytes));
    if (status == PAK_STATUS_OK) *value = read_le32(bytes);
    return status;
}

static pak_status read_primary_i32(pak_reader_state *state, int64_t *value)
{
    uint8_t bytes[4];
    pak_status status = read_primary_bytes(state, bytes, sizeof(bytes));
    if (status == PAK_STATUS_OK) *value = signed_le32(bytes);
    return status;
}

static pak_status read_primary_u64(pak_reader_state *state, uint64_t *value)
{
    uint8_t bytes[8];
    pak_status status = read_primary_bytes(state, bytes, sizeof(bytes));
    if (status == PAK_STATUS_OK) *value = read_le64(bytes);
    return status;
}

static pak_status read_primary_secondary(pak_reader_state *state, pak_secondary_index *entry)
{
    uint64_t offset, size;
    uint8_t hash[20];
    pak_status status = read_primary_u64(state, &offset);
    if (status != PAK_STATUS_OK) return status;
    status = read_primary_u64(state, &size);
    if (status != PAK_STATUS_OK) return status;
    status = read_primary_bytes(state, hash, sizeof(hash));
    if (status != PAK_STATUS_OK) return status;
    if ((offset & (1ULL << 63)) != 0 || (size & (1ULL << 63)) != 0) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_SECONDARY_RANGE);
    }
    entry->offset = offset;
    entry->size = size;
    memcpy(entry->sha1, hash, sizeof(hash));
    return PAK_STATUS_OK;
}

static int valid_range(uint64_t offset, uint64_t size, uint64_t bound)
{
    return offset <= bound && size <= bound - offset;
}

static int ranges_overlap(uint64_t left_offset, uint64_t left_size,
                          uint64_t right_offset, uint64_t right_size)
{
    if (left_size == 0 || right_size == 0) return 0;
    return left_offset < right_offset + right_size && right_offset < left_offset + left_size;
}

static int ascii_equal_fold(const uint8_t *input, uint32_t input_size, const char *known)
{
    uint32_t i;
    size_t known_size = strlen(known);
    if (known_size != input_size) return 0;
    for (i = 0; i < input_size; ++i) {
        uint8_t a = input[i];
        uint8_t b = (uint8_t)known[i];
        if (a >= 'A' && a <= 'Z') a = (uint8_t)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (uint8_t)(b + ('a' - 'A'));
        if (a != b) return 0;
    }
    return 1;
}

static int known_compression_name(const uint8_t *name, uint32_t length)
{
    static const char *const known[] = {
        "None", "Zlib", "Gzip", "Custom", "Oodle", "LZ4", "LZO",
        "Zstd", "XB1Zlib", "XboxOneGDKZlib", "Brotli"
    };
    size_t i;
    for (i = 0; i < sizeof(known) / sizeof(known[0]); ++i) {
        if (ascii_equal_fold(name, length, known[i])) return 1;
    }
    return 0;
}

static pak_status validate_compression_names(pak_reader_state *state)
{
    uint32_t slot;
    for (slot = 0; slot < PAK_READER_COMPRESSION_SLOTS; ++slot) {
        const uint8_t *name = state->result->compression_names[slot];
        uint32_t length = 0;
        uint32_t i;
        while (length < PAK_READER_COMPRESSION_NAME_BYTES && name[length] != 0) {
            if (name[length] < 0x21 || name[length] > 0x7e) {
                return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_COMPRESSION_NAME);
            }
            length++;
        }
        if (length < PAK_READER_COMPRESSION_NAME_BYTES) {
            for (i = length; i < PAK_READER_COMPRESSION_NAME_BYTES; ++i) {
                if (name[i] != 0) {
                    return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_COMPRESSION_NAME);
                }
            }
        }
        if (length != 0 && !known_compression_name(name, length)) {
            return fail(state, PAK_STATUS_UNSUPPORTED, PAK_REASON_UNSUPPORTED_COMPRESSION_NAME);
        }
    }
    return PAK_STATUS_OK;
}

static pak_status parse_footer(pak_reader_state *state, uint64_t footer_offset,
                               const uint8_t *footer)
{
    uint32_t magic, version;
    uint64_t primary_offset, primary_size;

    memcpy(state->result->encryption_key_guid, footer, 16);
    state->result->encrypted_index = footer[16] != 0;
    magic = read_le32(footer + 17);
    version = read_le32(footer + 21);
    state->result->pak_version = version;
    primary_offset = read_le64(footer + 25);
    primary_size = read_le64(footer + 33);
    memcpy(state->result->primary_index_sha1, footer + 41, 20);
    memcpy(state->result->compression_names, footer + 61,
           PAK_READER_COMPRESSION_SLOTS * PAK_READER_COMPRESSION_NAME_BYTES);

    if (magic != PAK_V11_MAGIC) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MAGIC);
    }
    if (version != PAK_V11_VERSION) {
        return fail(state, PAK_STATUS_UNSUPPORTED, PAK_REASON_UNSUPPORTED_VERSION);
    }
    if ((primary_offset & (1ULL << 63)) != 0 || (primary_size & (1ULL << 63)) != 0 ||
        primary_size == 0 || !valid_range(primary_offset, primary_size, footer_offset)) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_INDEX_RANGE);
    }
    if (primary_size > state->limits->max_primary_bytes) {
        return fail(state, PAK_STATUS_RESOURCE_LIMIT, PAK_REASON_INDEX_TOO_LARGE);
    }

    state->result->primary_index_offset = primary_offset;
    state->result->primary_index_size = primary_size;
    state->result->footer_parsed = 1u;

    if (state->result->encrypted_index != 0u) {
        return fail(state, PAK_STATUS_UNSUPPORTED, PAK_REASON_ENCRYPTED_INDEX);
    }
    return validate_compression_names(state);
}

static pak_status consume_remaining_primary(pak_reader_state *state)
{
    uint64_t remaining = state->result->primary_index_size - state->primary_position;
    return read_primary_bytes(state, NULL, remaining);
}

static pak_status finish_primary_sha1(pak_reader_state *state, uint32_t complete,
                                      pak_status unsupported_status, pak_reason unsupported_reason)
{
    uint8_t digest[20];
    int rc = mbedtls_sha1_finish(&state->sha1, digest);
    if (rc != 0) return fail(state, PAK_STATUS_INTERNAL_ERROR, PAK_REASON_SHA1_BACKEND);
    mbedtls_sha1_free(&state->sha1);
    state->sha1_started = 0;
    state->result->primary_sha1_checked = 1u;
    state->result->primary_sha1_matches = (uint32_t)(memcmp(digest, state->result->primary_index_sha1, 20) == 0);
    if (state->result->primary_sha1_matches == 0u) {
        return fail(state, PAK_STATUS_CHECKSUM_MISMATCH, PAK_REASON_PRIMARY_SHA1_MISMATCH);
    }
    if (unsupported_status != PAK_STATUS_OK) {
        return fail(state, unsupported_status, unsupported_reason);
    }
    state->result->primary_metadata_complete = complete;
    state->result->status = PAK_STATUS_OK;
    state->result->reason = PAK_REASON_NONE;
    return PAK_STATUS_OK;
}

static pak_status hash_unsupported_tail(pak_reader_state *state, pak_reason reason)
{
    pak_status status = consume_remaining_primary(state);
    if (status != PAK_STATUS_OK) return status;
    return finish_primary_sha1(state, 0u, PAK_STATUS_UNSUPPORTED, reason);
}

static pak_status parse_primary(pak_reader_state *state)
{
    int64_t serialized_string_length;
    uint64_t serialized_mount_bytes = 0;
    uint32_t file_count_u32, encoded_size_u32;
    uint64_t seed;
    uint32_t has_path, has_directory;
    int64_t signed_value;
    uint8_t mount_serialized[PAK_READER_MAX_MOUNT_BYTES + 2u];
    pak_status status;
    uint32_t i;

    mbedtls_sha1_init(&state->sha1);
    state->sha1_started = 1u;
    if (mbedtls_sha1_starts(&state->sha1) != 0) {
        return fail(state, PAK_STATUS_INTERNAL_ERROR, PAK_REASON_SHA1_BACKEND);
    }

    status = read_primary_i32(state, &serialized_string_length);
    if (status != PAK_STATUS_OK) return status;
    if (serialized_string_length == INT32_MIN) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MOUNT_STRING);
    }
    if (serialized_string_length > 0) {
        serialized_mount_bytes = (uint64_t)serialized_string_length;
        state->result->mount_encoding = PAK_MOUNT_ENCODING_ANSI_BYTES;
    } else if (serialized_string_length < 0) {
        uint64_t units = (uint64_t)(-serialized_string_length);
        if (units > UINT64_MAX / 2u) {
            return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MOUNT_STRING);
        }
        serialized_mount_bytes = units * 2u;
        state->result->mount_encoding = PAK_MOUNT_ENCODING_UTF16LE_CODE_UNITS;
    } else {
        state->result->mount_encoding = PAK_MOUNT_ENCODING_NONE;
    }

    if (serialized_mount_bytes != 0) {
        uint64_t terminator_bytes = state->result->mount_encoding == PAK_MOUNT_ENCODING_UTF16LE_CODE_UNITS ? 2u : 1u;
        uint64_t raw_bytes;
        if (serialized_mount_bytes < terminator_bytes) {
            return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MOUNT_STRING);
        }
        raw_bytes = serialized_mount_bytes - terminator_bytes;
        if (raw_bytes > state->limits->max_mount_bytes ||
            serialized_mount_bytes > sizeof(mount_serialized)) {
            return fail(state, PAK_STATUS_RESOURCE_LIMIT, PAK_REASON_MOUNT_TOO_LARGE);
        }
        status = read_primary_bytes(state, mount_serialized, serialized_mount_bytes);
        if (status != PAK_STATUS_OK) return status;
        if (state->result->mount_encoding == PAK_MOUNT_ENCODING_ANSI_BYTES) {
            if (mount_serialized[serialized_mount_bytes - 1u] != 0) {
                return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MOUNT_STRING);
            }
            for (i = 0; i < raw_bytes; ++i) {
                if (mount_serialized[i] == 0) {
                    return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MOUNT_STRING);
                }
            }
        } else {
            if (mount_serialized[serialized_mount_bytes - 1u] != 0 ||
                mount_serialized[serialized_mount_bytes - 2u] != 0) {
                return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MOUNT_STRING);
            }
            for (i = 0; i < raw_bytes; i += 2u) {
                if (mount_serialized[i] == 0 && mount_serialized[i + 1u] == 0) {
                    return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_MOUNT_STRING);
                }
            }
        }
        state->result->mount_bytes = (uint32_t)raw_bytes;
        memcpy(state->result->mount_raw, mount_serialized, (size_t)raw_bytes);
    }

    status = read_primary_i32(state, &signed_value);
    if (status != PAK_STATUS_OK) return status;
    if (signed_value < 0) return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_FILE_COUNT);
    if ((uint64_t)signed_value > state->limits->max_declared_files) {
        return fail(state, PAK_STATUS_RESOURCE_LIMIT, PAK_REASON_TOO_MANY_FILES);
    }
    file_count_u32 = (uint32_t)signed_value;
    state->result->declared_file_count = file_count_u32;

    status = read_primary_u64(state, &seed);
    if (status != PAK_STATUS_OK) return status;
    state->result->path_hash_seed = seed;

    status = read_primary_u32(state, &has_path);
    if (status != PAK_STATUS_OK) return status;
    if (has_path > 1u) return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_BOOLEAN);
    if (has_path == 0u) return hash_unsupported_tail(state, PAK_REASON_MISSING_SECONDARY_INDEX);
    status = read_primary_secondary(state, &state->result->path_hash_index);
    if (status != PAK_STATUS_OK) return status;

    status = read_primary_u32(state, &has_directory);
    if (status != PAK_STATUS_OK) return status;
    if (has_directory > 1u) return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_BOOLEAN);
    if (has_directory == 0u) return hash_unsupported_tail(state, PAK_REASON_MISSING_SECONDARY_INDEX);
    status = read_primary_secondary(state, &state->result->directory_index);
    if (status != PAK_STATUS_OK) return status;

    {
        uint64_t footer_offset = state->source->logical_size - PAK_V11_FOOTER_BYTES;
        if (!valid_range(state->result->path_hash_index.offset,
                         state->result->path_hash_index.size, footer_offset) ||
            !valid_range(state->result->directory_index.offset,
                         state->result->directory_index.size, footer_offset)) {
            return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_SECONDARY_RANGE);
        }
        if (ranges_overlap(state->result->primary_index_offset, state->result->primary_index_size,
                           state->result->path_hash_index.offset, state->result->path_hash_index.size) ||
            ranges_overlap(state->result->primary_index_offset, state->result->primary_index_size,
                           state->result->directory_index.offset, state->result->directory_index.size) ||
            ranges_overlap(state->result->path_hash_index.offset, state->result->path_hash_index.size,
                           state->result->directory_index.offset, state->result->directory_index.size)) {
            return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_OVERLAPPING_RANGES);
        }
    }

    status = read_primary_i32(state, &signed_value);
    if (status != PAK_STATUS_OK) return status;
    if (signed_value < 0 || (uint64_t)signed_value > UINT32_MAX) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_ENCODED_SIZE);
    }
    encoded_size_u32 = (uint32_t)signed_value;
    if (state->result->primary_index_size - state->primary_position < 4u ||
        (uint64_t)encoded_size_u32 >
            state->result->primary_index_size - state->primary_position - 4u) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_ENCODED_SIZE);
    }
    status = read_primary_bytes(state, NULL, encoded_size_u32);
    if (status != PAK_STATUS_OK) return status;

    status = read_primary_i32(state, &signed_value);
    if (status != PAK_STATUS_OK) return status;
    if (signed_value < 0 || (uint64_t)signed_value > file_count_u32) {
        return fail(state, PAK_STATUS_MALFORMED, PAK_REASON_BAD_NONENCODED_COUNT);
    }
    if (signed_value > 0) {
        return hash_unsupported_tail(state, PAK_REASON_NONENCODED_ENTRIES_UNSUPPORTED);
    }
    if (state->primary_position != state->result->primary_index_size) {
        return hash_unsupported_tail(state, PAK_REASON_TRAILING_PRIMARY_BYTES);
    }
    return finish_primary_sha1(state, 1u, PAK_STATUS_OK, PAK_REASON_NONE);
}

void pak_limits_v1_default(pak_limits_v1 *limits)
{
    if (limits == NULL) return;
    memset(limits, 0, sizeof(*limits));
    limits->struct_size = sizeof(*limits);
    limits->abi_version = PAK_READER_ABI_VERSION;
    limits->max_read_bytes = PAK_READER_MAX_READ_BYTES;
    limits->max_primary_bytes = PAK_READER_MAX_PRIMARY_BYTES;
    limits->max_total_read_bytes = PAK_READER_MAX_TOTAL_READ_BYTES;
    limits->max_read_calls = PAK_READER_MAX_READ_CALLS;
    limits->max_declared_files = PAK_READER_MAX_FILES;
    limits->max_mount_bytes = PAK_READER_MAX_MOUNT_BYTES;
}

pak_status pak_read_v11_primary(const pak_source_v1 *source,
                                const pak_limits_v1 *limits,
                                uint8_t *workspace,
                                size_t workspace_bytes,
                                pak_primary_result_v1 *result)
{
    pak_reader_state state;
    uint8_t footer[PAK_V11_FOOTER_BYTES];
    uint64_t footer_offset;
    pak_status status;

    if (result == NULL) return PAK_STATUS_BAD_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_READER_ABI_VERSION;

    if (source == NULL || limits == NULL || workspace == NULL ||
        source->struct_size < sizeof(*source) || source->abi_version != PAK_READER_ABI_VERSION ||
        source->reserved != 0u || source->read_at == NULL || source->snapshot_token == 0u ||
        source->snapshot_scope != PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE ||
        limits->struct_size < sizeof(*limits) || limits->abi_version != PAK_READER_ABI_VERSION ||
        limits->reserved != 0u || workspace_bytes < PAK_READER_WORKSPACE_BYTES ||
        limits->max_read_bytes == 0u || limits->max_read_bytes > PAK_READER_MAX_READ_BYTES ||
        limits->max_primary_bytes == 0u || limits->max_primary_bytes > PAK_READER_MAX_PRIMARY_BYTES ||
        limits->max_total_read_bytes == 0u || limits->max_total_read_bytes > PAK_READER_MAX_TOTAL_READ_BYTES ||
        limits->max_read_calls == 0u || limits->max_read_calls > PAK_READER_MAX_READ_CALLS ||
        limits->max_declared_files > PAK_READER_MAX_FILES ||
        limits->max_mount_bytes > PAK_READER_MAX_MOUNT_BYTES) {
        result->status = PAK_STATUS_BAD_ARGUMENT;
        result->reason = PAK_REASON_BAD_ARGUMENT;
        return result->status;
    }
    if (source->logical_size < PAK_V11_FOOTER_BYTES) {
        result->status = PAK_STATUS_MALFORMED;
        result->reason = PAK_REASON_TRAILER_TOO_SMALL;
        return result->status;
    }

    memset(&state, 0, sizeof(state));
    state.source = source;
    state.limits = limits;
    state.workspace = workspace;
    state.workspace_bytes = workspace_bytes;
    state.result = result;
    footer_offset = source->logical_size - PAK_V11_FOOTER_BYTES;
    status = read_footer(&state, footer_offset, footer);
    if (status != PAK_STATUS_OK) return status;
    status = parse_footer(&state, footer_offset, footer);
    if (status != PAK_STATUS_OK) return status;
    status = parse_primary(&state);
    if (state.sha1_started != 0u) {
        mbedtls_sha1_free(&state.sha1);
        state.sha1_started = 0u;
    }
    return status;
}
