#ifndef NATIVE_PAK_DIRECTORY_PARSER_EXPERIMENT_H
#define NATIVE_PAK_DIRECTORY_PARSER_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_DIRECTORY_PARSER_ABI_VERSION 1u
#define PAK_DIRECTORY_MAX_INDEX_BYTES (4u * 1024u * 1024u)
#define PAK_DIRECTORY_MAX_ENCODED_BLOB_BYTES (4u * 1024u * 1024u)
#define PAK_DIRECTORY_MAX_DIRECTORIES 65536u
#define PAK_DIRECTORY_MAX_ENTRIES 65536u
#define PAK_DIRECTORY_MAX_COMPONENT_UTF8_BYTES 4096u
#define PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES (4u * 1024u * 1024u)

typedef enum pak_directory_status {
    PAK_DIRECTORY_STATUS_OK = 0,
    PAK_DIRECTORY_STATUS_UNSUPPORTED = 1,
    PAK_DIRECTORY_STATUS_MALFORMED = 2,
    PAK_DIRECTORY_STATUS_RESOURCE_LIMIT = 3,
    PAK_DIRECTORY_STATUS_BAD_ARGUMENT = 4
} pak_directory_status;

typedef enum pak_directory_reason {
    PAK_DIRECTORY_REASON_NONE = 0,
    PAK_DIRECTORY_REASON_BAD_ARGUMENT,
    PAK_DIRECTORY_REASON_BAD_LIMITS,
    PAK_DIRECTORY_REASON_INDEX_TOO_LARGE,
    PAK_DIRECTORY_REASON_ENCODED_BLOB_TOO_LARGE,
    PAK_DIRECTORY_REASON_TRUNCATED,
    PAK_DIRECTORY_REASON_BAD_COUNT,
    PAK_DIRECTORY_REASON_DIRECTORY_LIMIT,
    PAK_DIRECTORY_REASON_ENTRY_LIMIT,
    PAK_DIRECTORY_REASON_OUTPUT_CAPACITY,
    PAK_DIRECTORY_REASON_BAD_FSTRING_LENGTH,
    PAK_DIRECTORY_REASON_FSTRING_TERMINATOR,
    PAK_DIRECTORY_REASON_INVALID_UTF8,
    PAK_DIRECTORY_REASON_INVALID_UTF16LE,
    PAK_DIRECTORY_REASON_COMPONENT_LIMIT,
    PAK_DIRECTORY_REASON_NAME_ARENA_LIMIT,
    PAK_DIRECTORY_REASON_LOCATION_UNSUPPORTED,
    PAK_DIRECTORY_REASON_LOCATION_OUT_OF_RANGE,
    PAK_DIRECTORY_REASON_TRAILING_BYTES
} pak_directory_reason;

typedef enum pak_directory_string_encoding {
    PAK_DIRECTORY_STRING_UTF8 = 1,
    PAK_DIRECTORY_STRING_UTF16LE = 2
} pak_directory_string_encoding;

enum {
    PAK_DIRECTORY_ENTRY_TOMBSTONE = 1u
};

typedef struct pak_directory_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_index_bytes;
    uint32_t max_encoded_blob_bytes;
    uint32_t max_directories;
    uint32_t max_entries;
    uint32_t max_component_utf8_bytes;
    uint32_t max_total_name_bytes; /* Includes each stored NUL terminator. */
    uint32_t reserved[2];
} pak_directory_limits_v1;

typedef struct pak_directory_record_v1 {
    uint32_t name_offset;
    uint32_t name_length; /* UTF-8 bytes, excluding the stored NUL. */
    uint32_t input_encoding;
    uint32_t first_entry;
    uint32_t entry_count; /* Includes tombstone rows. */
    uint32_t reserved;
} pak_directory_record_v1;

typedef struct pak_directory_entry_v1 {
    uint32_t name_offset;
    uint32_t name_length; /* UTF-8 bytes, excluding the stored NUL. */
    uint32_t input_encoding;
    int32_t location;
    uint32_t flags;
    uint32_t reserved;
} pak_directory_entry_v1;

typedef struct pak_directory_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_directory_status status;
    pak_directory_reason reason;
    uint32_t metadata_complete;
    uint32_t directory_count;
    uint32_t entry_count;
    uint32_t tombstone_count;
    uint32_t string_bytes;
    uint32_t reserved[3];
} pak_directory_result_v1;

void pak_directory_limits_v1_default(pak_directory_limits_v1 *limits);

/*
 * Parse only the classic, unencrypted PAK v11 full-directory byte layout.
 * The caller selects this operation only after validating the enclosing
 * archive/version and supplies the primary-verified encoded-blob length.
 * Directory and filename components are decoded to UTF-8 in string_arena
 * but remain separate engine metadata; no mount concatenation, path
 * normalization, duplicate policy, path-hash parsing, digest verification,
 * record-start validation, extraction, or filesystem mapping is performed.
 *
 * All buffers are caller-owned, immutable inputs and disjoint output spans.
 * No heap is allocated and no pointer is retained. On success all directory
 * rows (including empty directories) and file rows (including tombstones)
 * are complete. On any failure, result counts/complete are zeroed and every
 * output prefix written by this call is cleared. `string_bytes` includes NUL
 * terminators; per-name lengths exclude them. Nonnegative locations are
 * checked only as offsets strictly inside the supplied encoded-blob span.
 */
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
    pak_directory_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
