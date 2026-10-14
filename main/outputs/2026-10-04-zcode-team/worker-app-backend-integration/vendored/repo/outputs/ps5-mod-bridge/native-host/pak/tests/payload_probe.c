#include "pak_payload.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Development fixture program: immutable local input, no extraction path. */
#define CAP 65536u
static uint8_t image[CAP], reader[CAP], primary[CAP], compressed[CAP];
static uint8_t arena[CAP], output[CAP];
static pak_entry_block_v1 blocks[64];
static bz_state codec;

typedef struct { size_t bytes; } image_context;

static pak_provider_status read_image(void *context, uint64_t offset,
                                      uint8_t *destination, uint32_t requested,
                                      uint32_t *actual)
{
    image_context *input = context;
    if (input == NULL || destination == NULL || actual == NULL ||
        offset > input->bytes || requested > input->bytes - (size_t)offset) {
        if (actual != NULL) *actual = 0u;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    memcpy(destination, image + (size_t)offset, requested);
    *actual = requested;
    return PAK_PROVIDER_OK;
}

int main(int argc, char **argv)
{
    FILE *file;
    long length;
    unsigned long long selected;
    char *end;
    image_context input;
    pak_source_v1 source = {0};
    pak_limits_v1 reader_limits;
    pak_entry_limits_v1 entry_limits = {0};
    pak_payload_buffers buffers = {0};
    pak_payload_limits limits = pak_payload_default_limits();
    bz_budget budget = bz_default_budget();
    pak_payload_result result;

    if (argc != 3 || argv[2][0] < '0' || argv[2][0] > '9') return 2;
    errno = 0;
    selected = strtoull(argv[2], &end, 10);
    if (errno != 0 || *end != '\0' || selected > SIZE_MAX) return 2;
    file = fopen(argv[1], "rb");
    if (file == NULL) return 2;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
        (unsigned long)length > CAP || fseek(file, 0, SEEK_SET) != 0 ||
        fread(image, 1u, (size_t)length, file) != (size_t)length) {
        fclose(file);
        return 2;
    }
    if (fgetc(file) != EOF || ferror(file)) { fclose(file); return 2; }
    if (fclose(file) != 0) return 2;
    input.bytes = (size_t)length;
    source.struct_size = sizeof(source);
    source.abi_version = PAK_READER_ABI_VERSION;
    source.logical_size = input.bytes;
    source.snapshot_token = 1u;
    source.snapshot_scope = PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE;
    source.context = &input;
    source.read_at = read_image;
    pak_limits_v1_default(&reader_limits);
    reader_limits.max_primary_bytes = CAP;
    reader_limits.max_total_read_bytes = 4u * CAP;
    reader_limits.max_read_calls = 128u;
    reader_limits.max_read_bytes = 4096u;
    entry_limits.struct_size = sizeof(entry_limits);
    entry_limits.abi_version = PAK_ENTRY_DECODER_ABI_VERSION;
    entry_limits.max_blob_bytes = CAP;
    entry_limits.max_record_bytes = 8192u;
    entry_limits.max_blocks = 64u;
    entry_limits.max_block_size = CAP;
    entry_limits.max_compressed_size = CAP;
    entry_limits.max_uncompressed_size = CAP;
    entry_limits.max_total_block_bytes = 2u * CAP;
    buffers.reader_workspace = reader;
    buffers.reader_workspace_bytes = sizeof(reader);
    buffers.primary_scratch = primary;
    buffers.primary_scratch_bytes = sizeof(primary);
    buffers.blocks = blocks;
    buffers.blocks_capacity = 64u;
    buffers.compressed_scratch = compressed;
    buffers.compressed_scratch_bytes = sizeof(compressed);
    buffers.codec_arena = arena;
    buffers.codec_arena_bytes = sizeof(arena);
    buffers.codec_state = &codec;
    buffers.output = output;
    buffers.output_capacity = sizeof(output);
    limits.max_output_bytes = CAP;
    result = pak_read_v11_selected_payload(&source, &reader_limits, &entry_limits,
                                           (size_t)selected, buffers, limits, &budget);
    if (result.status != PAK_PAYLOAD_OK || !result.payload_complete ||
        result.installation_supported || result.decoded_bytes > sizeof(output)) {
        fprintf(stderr, "payload rejected: status=%d reader=%d/%d entry=%d/%d codec=%d\n",
                result.status, result.reader_status, result.reader_reason,
                result.entry_status, result.entry_reason, result.codec_status);
        return 2;
    }
    if (fwrite(output, 1u, result.decoded_bytes, stdout) != result.decoded_bytes ||
        fflush(stdout) != 0 || ferror(stdout)) return 2;
    return 0;
}
