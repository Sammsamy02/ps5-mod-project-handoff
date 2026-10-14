#include "pak_candidate.h"
#include <stdio.h>
#include <string.h>

/* Only host fixture I/O lives here; production writer has no file/provider API.
 * Canonical candidate independently verifies declared metadata and stored bytes. */
static unsigned checks, failures, outside;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    printf("FAIL candidate line=%u %s\n", (unsigned)__LINE__, #x); } } while (0)
static uint8_t image[16384], reader[65536], primary[4096], secondary[4096], directory[4096], names[4096];
static uint8_t physical[1081], compressed[4096], arena[65536], output[4096], seen[1];
static pak_directory_record_v1 directories[16];
static pak_directory_entry_v1 files[8];
static uint32_t starts[8], order[8];
static pak_graph_extent_v1 extents[8];
static pak_entry_block_v1 decode_blocks[64], selected_blocks[64];
static pak_physical_block_v1 physical_blocks[64];
static bz_state codec_state;
static bz_budget codec_budget;
static pak_candidate_state_v1 state;
static pak_candidate_result_v1 result;
static size_t image_bytes;
static const uint8_t icon[] = {0,1,2,255,128,0,42,17,99};
static const uint8_t text[] = "mode=original\r\n", readme[] = "bounded synthetic bytes\n";
typedef struct expected { const char *name; const uint8_t *bytes; size_t size; } expected;
static const expected entries[] = {
    {"Engine/Content/UI/Icon.bin",icon,sizeof(icon)},
    {"Example/Config/base.ini",text,sizeof(text)-1},
    {"empty.bin",NULL,0}, {"readme.txt",readme,sizeof(readme)-1}
};

static pak_provider_status read_at(void *context, uint64_t offset, uint8_t *to,
                                 uint32_t count, uint32_t *actual) {
    (void)context; *actual = 0;
    if (offset > image_bytes || count > image_bytes-offset) { ++outside; return PAK_PROVIDER_UNAVAILABLE; }
    memcpy(to,image+(size_t)offset,count); *actual = count; return PAK_PROVIDER_OK;
}

static pak_status validate(unsigned want, unsigned selected) {
    pak_candidate_limits_v1 limits; pak_candidate_limits_v1_default(&limits);
    const pak_source_v1 source = {sizeof(source),1,image_bytes,UINT64_C(0x575249544552),
        PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE,0,NULL,read_at};
    pak_candidate_buffers_v1 b = {0}; b.struct_size = sizeof(b); b.abi_version = 1;
    b.state = &state; b.reader_workspace = reader; b.reader_workspace_bytes = sizeof(reader);
    b.primary_capture = primary; b.primary_capacity = sizeof(primary);
    b.secondary_workspace = secondary; b.secondary_workspace_bytes = sizeof(secondary);
    b.directory_capture = directory; b.directory_capture_capacity = sizeof(directory);
    b.directories = directories; b.directories_capacity = 16; b.files = files; b.files_capacity = 8;
    b.names = names; b.names_capacity = sizeof(names);
    b.graph = (pak_graph_workspace_v1){sizeof(b.graph),1,starts,8,extents,8,order,8,seen,1,
        decode_blocks,64,selected_blocks,64};
    b.physical_header = physical; b.physical_header_capacity = sizeof(physical);
    b.physical_blocks = physical_blocks; b.physical_blocks_capacity = 64;
    b.payload = (pak_reconciled_payload_buffers_v1){compressed,sizeof(compressed),arena,sizeof(arena),
        &codec_state,output,sizeof(output)};
    codec_budget = bz_default_budget();
    return pak_validate_v11_candidate(&source,&limits,want,selected,&b,&codec_budget,NULL,NULL,&result);
}

static int load(const char *directory_path, const char *name) {
    char path[4096]; const int n = snprintf(path,sizeof(path),"%s/%s",directory_path,name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    FILE *f = fopen(path,"rb"); if (!f) return 0;
    image_bytes = fread(image,1,sizeof(image),f);
    const int good = !ferror(f) && fgetc(f) == EOF; return fclose(f) == 0 && good;
}

static int selected_match(unsigned row) {
    if (row >= result.file_count) return -1;
    for (uint32_t d = 0; d < result.directory_count; ++d) {
        const pak_directory_record_v1 *dir = &directories[d];
        if (row < dir->first_entry || row-dir->first_entry >= dir->entry_count) continue;
        const uint8_t *prefix = names+dir->name_offset;
        const uint32_t prefix_bytes = dir->name_length == 1 && prefix[0] == '/' ? 0 : dir->name_length;
        const pak_directory_entry_v1 *entry = &files[row];
        for (unsigned i = 0; i < 4; ++i) {
            const size_t bytes = strlen(entries[i].name);
            if (bytes == prefix_bytes+entry->name_length &&
                memcmp(entries[i].name,prefix,prefix_bytes) == 0 &&
                memcmp(entries[i].name+prefix_bytes,names+entry->name_offset,entry->name_length) == 0)
                return (int)i;
        }
    }
    return -1;
}

static void positive(const char *directory_path, const char *name, unsigned count) {
    CHECK(load(directory_path,name));
    CHECK(validate(0,0) == PAK_STATUS_OK);
    CHECK(result.metadata_complete && !result.selected_payload_complete && result.file_count == count);
    unsigned mask = 0;
    for (unsigned row = 0; row < count; ++row) {
        const int expected_index = selected_match(row);
        CHECK(expected_index >= 0);
        if (expected_index < 0) continue;
        CHECK(validate(1,row) == PAK_STATUS_OK);
        CHECK(result.metadata_complete && result.selected_payload_complete &&
              result.stage == PAK_CANDIDATE_STAGE_COMPLETE);
        const expected *e = &entries[expected_index];
        CHECK(result.decoded_bytes == e->size && (!e->size || memcmp(output,e->bytes,e->size) == 0));
        mask |= 1u << (unsigned)expected_index;
    }
    CHECK(mask == (count ? 15u : 0u));
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    positive(argv[1],"prepared.pak",4); positive(argv[1],"custom.pak",4); positive(argv[1],"empty.pak",0);
    CHECK(load(argv[1],"payload-corrupt.pak"));
    CHECK(validate(0,0) == PAK_STATUS_OK && result.metadata_complete);
    int selected = -1;
    for (unsigned row = 0; row < 4; ++row) if (selected_match(row) == 0) selected = (int)row;
    CHECK(selected >= 0);
    if (selected >= 0) {
        memset(output,0xa5,sizeof(output));
        CHECK(validate(1,(unsigned)selected) == PAK_STATUS_CHECKSUM_MISMATCH);
        CHECK(!result.selected_payload_complete && !result.decoded_bytes);
        for (size_t i = 0; i < sizeof(output); ++i) if (output[i]) { CHECK(0); break; }
    }
    CHECK(!outside);
    printf("writer-candidate archives=3 selected_positive=8 corrupt_negative=1 checks=%u failures=%u outside_reads=%u\n",
           checks,failures,outside);
    return failures ? 1 : 0;
}
