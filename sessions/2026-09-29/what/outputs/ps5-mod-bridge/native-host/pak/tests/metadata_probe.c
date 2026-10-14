#include "pak_reader.h"
#include "pak_entry_pipeline.h"
#include "pak_directory_parser.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_CAP (64u * 1024u)
#define PRIMARY_CAP (64u * 1024u)
#define DIR_INDEX_CAP (32u * 1024u)
#define ENTRY_BLOCK_CAP 64u
#define DIR_CAP 8u
#define ENTRY_CAP 8u
#define ARENA_CAP 1024u

struct memory_image {
    const uint8_t *bytes;
    size_t size;
};

static pak_provider_status read_memory(void *context, uint64_t offset,
                                       uint8_t *destination, uint32_t requested,
                                       uint32_t *actual)
{
    const struct memory_image *image = (const struct memory_image *)context;
    if (image == NULL || destination == NULL || actual == NULL ||
        offset > image->size || (uint64_t)requested > image->size - (size_t)offset) {
        if (actual != NULL) *actual = 0u;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    memcpy(destination, image->bytes + (size_t)offset, requested);
    *actual = requested;
    return PAK_PROVIDER_OK;
}

static int load_image(const char *path, uint8_t *bytes, size_t capacity, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long end;
    size_t size;
    int trailing;
    if (file == NULL) return 0;
    if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    size = (size_t)end;
    if ((long)size != end || size == 0u || size > capacity ||
        fread(bytes, 1u, size, file) != size) {
        fclose(file);
        return 0;
    }
    trailing = fgetc(file);
    if (trailing != EOF || ferror(file) || fclose(file) != 0) return 0;
    *size_out = size;
    return 1;
}

static int range_in_image(uint64_t offset, uint64_t size, size_t image_size)
{
    return offset <= image_size && size <= (uint64_t)image_size - offset;
}

static void json_string(const uint8_t *bytes, size_t length)
{
    size_t i;
    putchar('"');
    for (i = 0; i < length; ++i) {
        uint8_t ch = bytes[i];
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar((int)ch);
        } else if (ch >= 0x20u) {
            putchar((int)ch);
        } else {
            printf("\\u%04x", (unsigned)ch);
        }
    }
    putchar('"');
}

static void print_hex20(const uint8_t digest[20])
{
    size_t i;
    for (i = 0; i < 20u; ++i) printf("%02x", digest[i]);
}

static int fail_json(const char *stage, int status, int reason)
{
    printf("{\"stage\":\"%s\",\"status\":%d,\"reason\":%d}\n",
           stage, status, reason);
    return 2;
}

int main(int argc, char **argv)
{
    static uint8_t image_bytes[IMAGE_CAP];
    static uint8_t reader_workspace[PAK_READER_WORKSPACE_BYTES];
    static uint8_t primary_scratch[PRIMARY_CAP];
    static uint8_t directory_scratch[DIR_INDEX_CAP];
    static pak_directory_record_v1 directories[DIR_CAP];
    static pak_directory_entry_v1 entries[ENTRY_CAP];
    static uint8_t string_arena[ARENA_CAP];
    static pak_entry_block_v1 blocks[ENTRY_BLOCK_CAP];
    struct memory_image image;
    pak_source_v1 source;
    pak_limits_v1 reader_limits;
    pak_primary_result_v1 primary;
    pak_entry_limits_v1 entry_limits;
    pak_entry_pipeline_result_v1 bootstrap;
    pak_directory_limits_v1 directory_limits;
    pak_directory_result_v1 directory_result;
    size_t image_size, i;

    if (argc != 2) return fail_json("arguments", -1, -1);
    if (!load_image(argv[1], image_bytes, sizeof(image_bytes), &image_size)) {
        return fail_json("bounded_input_read", -1, -1);
    }
    image.bytes = image_bytes;
    image.size = image_size;
    memset(&source, 0, sizeof(source));
    source.struct_size = sizeof(source);
    source.abi_version = PAK_READER_ABI_VERSION;
    source.logical_size = image_size;
    source.snapshot_token = 1u;
    source.snapshot_scope = PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE;
    source.context = &image;
    source.read_at = read_memory;

    pak_limits_v1_default(&reader_limits);
    reader_limits.max_read_bytes = 4096u;
    reader_limits.max_primary_bytes = PRIMARY_CAP;
    reader_limits.max_total_read_bytes = 128u * 1024u;
    reader_limits.max_read_calls = 64u;
    reader_limits.max_declared_files = ENTRY_CAP;
    if (pak_read_v11_primary(&source, &reader_limits, reader_workspace,
                             sizeof(reader_workspace), &primary) != PAK_STATUS_OK ||
        primary.primary_metadata_complete != 1u || primary.primary_sha1_checked != 1u ||
        primary.primary_sha1_matches != 1u || primary.encrypted_index != 0u ||
        !range_in_image(primary.directory_index.offset, primary.directory_index.size, image_size) ||
        primary.directory_index.size > sizeof(directory_scratch)) {
        return fail_json("primary", primary.status, primary.reason);
    }

    memset(&entry_limits, 0, sizeof(entry_limits));
    entry_limits.struct_size = sizeof(entry_limits);
    entry_limits.abi_version = PAK_ENTRY_DECODER_ABI_VERSION;
    entry_limits.max_blob_bytes = IMAGE_CAP;
    entry_limits.max_record_bytes = 8192u;
    entry_limits.max_blocks = 16u;
    entry_limits.max_block_size = IMAGE_CAP;
    entry_limits.max_compressed_size = IMAGE_CAP;
    entry_limits.max_uncompressed_size = IMAGE_CAP;
    entry_limits.max_total_block_bytes = 2u * IMAGE_CAP;
    if (pak_read_v11_one_entry(&source, &reader_limits, reader_workspace,
                               sizeof(reader_workspace), primary_scratch,
                               sizeof(primary_scratch), 0u, &entry_limits,
                               blocks, ENTRY_BLOCK_CAP, &bootstrap) != PAK_ENTRY_PIPELINE_OK ||
        bootstrap.primary_validated != 1u || bootstrap.primary_capture_complete != 1u ||
        bootstrap.primary.primary_index_offset != primary.primary_index_offset ||
        bootstrap.primary.primary_index_size != primary.primary_index_size ||
        bootstrap.encoded_span_bytes > IMAGE_CAP ||
        bootstrap.entry_complete != 1u) {
        return fail_json("entry_pipeline_offset_zero", bootstrap.status, bootstrap.reason);
    }

    pak_directory_limits_v1_default(&directory_limits);
    directory_limits.max_index_bytes = sizeof(directory_scratch);
    directory_limits.max_encoded_blob_bytes = IMAGE_CAP;
    directory_limits.max_directories = DIR_CAP;
    directory_limits.max_entries = ENTRY_CAP;
    directory_limits.max_component_utf8_bytes = 128u;
    directory_limits.max_total_name_bytes = ARENA_CAP;
    if (pak_directory_read_classic_v11(
            image_bytes + (size_t)primary.directory_index.offset,
            (size_t)primary.directory_index.size,
            bootstrap.encoded_span_bytes, &directory_limits,
            directories, DIR_CAP, entries, ENTRY_CAP,
            string_arena, sizeof(string_arena), &directory_result) != PAK_DIRECTORY_STATUS_OK ||
        directory_result.metadata_complete != 1u) {
        return fail_json("directory", directory_result.status, directory_result.reason);
    }

    printf("{\"stage\":\"complete\",\"image_bytes\":%zu,", image_size);
    printf("\"primary_sha1_checked\":true,\"primary_sha1_matches\":true,");
    printf("\"primary_offset\":\"%" PRIu64 "\",\"primary_size\":\"%" PRIu64 "\",",
           primary.primary_index_offset, primary.primary_index_size);
    printf("\"file_count\":%u,\"mount_encoding\":%u,\"mount_bytes\":%u,",
           primary.declared_file_count, primary.mount_encoding, primary.mount_bytes);
    printf("\"mount_raw\":");
    json_string(primary.mount_raw, primary.mount_bytes);
    printf(",\"path_hash\":{\"offset\":\"%" PRIu64 "\",\"size\":\"%" PRIu64 "\",\"sha1\":\"",
           primary.path_hash_index.offset, primary.path_hash_index.size);
    print_hex20(primary.path_hash_index.sha1);
    printf("\"},\"directory_index\":{\"offset\":\"%" PRIu64 "\",\"size\":\"%" PRIu64 "\",\"sha1\":\"",
           primary.directory_index.offset, primary.directory_index.size);
    print_hex20(primary.directory_index.sha1);
    printf("\"},\"encoded_blob_bytes\":%u,\"pipeline_probe_offset\":0,",
           bootstrap.encoded_span_bytes);
    printf("\"footer_compression_names\":[");
    for (i = 0; i < PAK_READER_COMPRESSION_SLOTS; ++i) {
        size_t n = 0u;
        while (n < PAK_READER_COMPRESSION_NAME_BYTES && primary.compression_names[i][n] != 0u) ++n;
        if (i != 0u) putchar(',');
        json_string(primary.compression_names[i], n);
    }
    printf("],\"directories\":[");
    for (i = 0; i < directory_result.directory_count; ++i) {
        const pak_directory_record_v1 *directory = &directories[i];
        if (i != 0u) putchar(',');
        printf("{\"name\":");
        json_string(string_arena + directory->name_offset, directory->name_length);
        printf(",\"encoding\":%u,\"first_entry\":%u,\"entry_count\":%u}",
               directory->input_encoding, directory->first_entry, directory->entry_count);
    }
    printf("],\"entries\":[");
    for (i = 0; i < directory_result.entry_count; ++i) {
        const pak_directory_entry_v1 *directory_entry = &entries[i];
        const uint8_t *name = string_arena + directory_entry->name_offset;
        const pak_directory_record_v1 *parent = NULL;
        pak_entry_pipeline_result_v1 decoded;
        size_t d;
        if (directory_entry->flags & PAK_DIRECTORY_ENTRY_TOMBSTONE) {
            return fail_json("unexpected_fixture_tombstone", directory_entry->location, 0);
        }
        for (d = 0; d < directory_result.directory_count; ++d) {
            if (i >= directories[d].first_entry &&
                i < (size_t)directories[d].first_entry + directories[d].entry_count) {
                parent = &directories[d];
                break;
            }
        }
        if (parent == NULL || directory_entry->location < 0) {
            return fail_json("directory_entry_parent_or_location", -1, (int)directory_entry->location);
        }
        if (pak_read_v11_one_entry(&source, &reader_limits, reader_workspace,
                                   sizeof(reader_workspace), primary_scratch,
                                   sizeof(primary_scratch), (size_t)directory_entry->location,
                                   &entry_limits, blocks, ENTRY_BLOCK_CAP, &decoded) !=
                PAK_ENTRY_PIPELINE_OK ||
            decoded.entry_complete != 1u || decoded.primary.primary_index_offset != primary.primary_index_offset ||
            decoded.primary.primary_index_size != primary.primary_index_size ||
            decoded.encoded_span_bytes != bootstrap.encoded_span_bytes) {
            return fail_json("directory_referenced_entry", decoded.status, decoded.entry_reason);
        }
        if (i != 0u) putchar(',');
        printf("{\"directory\":");
        json_string(string_arena + parent->name_offset, parent->name_length);
        printf(",\"name\":");
        json_string(name, directory_entry->name_length);
        printf(",\"location\":%d,\"record_bytes\":%u,\"header_bytes\":%u,",
               directory_entry->location, decoded.entry.bytes_consumed, decoded.entry.header_bytes);
        printf("\"method_index\":%u,\"method\":", decoded.entry.method_index);
        json_string(decoded.entry.method_name, decoded.entry.method_name_bytes);
        printf(",\"compressed_size\":\"%" PRIu64 "\",\"uncompressed_size\":\"%" PRIu64 "\",",
               decoded.entry.compressed_size, decoded.entry.uncompressed_size);
        printf("\"data_offset\":\"%" PRIu64 "\",\"block_count\":%u,\"blocks\":[",
               decoded.entry.data_offset, decoded.entry.block_count);
        for (d = 0; d < decoded.entry.block_count; ++d) {
            if (d != 0u) putchar(',');
            printf("{\"compressed_offset\":\"%" PRIu64 "\",\"compressed_size\":\"%" PRIu64 "\",",
                   blocks[d].compressed_offset, blocks[d].compressed_size);
            printf("\"uncompressed_offset\":\"%" PRIu64 "\",\"uncompressed_size\":\"%" PRIu64 "\"}",
                   blocks[d].uncompressed_offset, blocks[d].uncompressed_size);
        }
        printf("]}");
    }
    printf("],\"directory_parse_complete\":true}\n");
    return 0;
}
