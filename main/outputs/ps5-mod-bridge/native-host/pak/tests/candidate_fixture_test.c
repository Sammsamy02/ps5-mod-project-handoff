#include "pak_candidate.h"
#include <mbedtls/sha1.h>
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    printf("FAIL %u: %s\n",(unsigned)__LINE__,#x); } } while (0)

static uint8_t image[65536], original[16384];
static uint8_t reader[65536], primary[4096], secondary[4096], directory[4096], names[2048];
static uint8_t physical[1081], compressed[16384], arena[65536], output[32768+16], seen[8];
static pak_directory_record_v1 directories[8];
static pak_directory_entry_v1 files[8];
static uint32_t starts[8], sort_indices[8];
static pak_graph_extent_v1 extents[8];
static pak_entry_block_v1 decode_blocks[64], selected_blocks[64];
static pak_physical_block_v1 physical_blocks[64];
static bz_state codec_state;
static bz_budget codec_budget;
static pak_candidate_state_v1 state;
static pak_candidate_buffers_v1 buffers;
static pak_candidate_limits_v1 limits;
static pak_candidate_result_v1 result;
static pak_source_v1 source;
static size_t image_bytes, original_bytes;
static uint64_t observed_bytes;
static uint32_t observed_calls;
static uint64_t trace_offsets[256];
static uint32_t trace_requests[256];
static const char *fixture_root;
static const char *synthetic_root;
typedef enum { FAULT_NONE, FAULT_SHORT, FAULT_CHANGED, FAULT_CANCELLED,
               FAULT_OVERREPORT, FAULT_IO } fault_kind;
static fault_kind fault;
static uint64_t fault_offset;
static uint32_t fault_hits;
static size_t prefix_bytes;
static uint64_t prefix_source_offset;
static uint32_t prefix_observed;
static const uint8_t *prefix_expected;
static uint64_t prefix_work, prefix_steps;
static uint64_t virtual_tail_shift;
static size_t virtual_prefix_bytes;
static uint32_t virtual_hole_reads;

typedef struct fixture_view {
    size_t footer, primary_offset, primary_bytes, count_field;
    size_t path_offset, path_bytes, directory_offset, directory_bytes;
    size_t path_offset_field, directory_offset_field;
    size_t directory_digest_field, blob_offset, blob_bytes;
    size_t reference_fields[8], name_terminators[8];
    uint32_t file_count;
} fixture_view;

static int all_value(const void *pointer,size_t bytes,uint8_t value) {
    const uint8_t *p=pointer;
    for (size_t i=0;i<bytes;++i) if (p[i]!=value) return 0;
    return 1;
}

static int load_from(const char *root,const char *name,uint8_t *destination,size_t capacity,size_t *size) {
    char path[1024];
    if (snprintf(path,sizeof(path),"%s/%s",root,name)<0 || strlen(path)>=sizeof(path)-1) return 0;
    FILE *file=fopen(path,"rb");
    if (!file) return 0;
    if (fseek(file,0,SEEK_END)!=0) { fclose(file); return 0; }
    const long length=ftell(file);
    if (length<0 || (unsigned long)length>capacity || fseek(file,0,SEEK_SET)!=0) {
        fclose(file); return 0;
    }
    const int read_ok=fread(destination,1,(size_t)length,file)==(size_t)length;
    const int tail=fgetc(file);
    const int bad=ferror(file);
    const int closed=fclose(file);
    if (!read_ok || tail!=EOF || bad || closed) return 0;
    *size=(size_t)length;
    return 1;
}

static int load(const char *name,uint8_t *destination,size_t capacity,size_t *size) {
    return load_from(fixture_root,name,destination,capacity,size);
}

static uint32_t get32(size_t at) {
    CHECK(at<=image_bytes && 4<=image_bytes-at);
    if (at>image_bytes || 4>image_bytes-at) return 0;
    return (uint32_t)image[at] | (uint32_t)image[at+1]<<8 |
           (uint32_t)image[at+2]<<16 | (uint32_t)image[at+3]<<24;
}

static uint64_t get64(size_t at) {
    return (uint64_t)get32(at) | (uint64_t)get32(at+4)<<32;
}

static void put32(size_t at,uint32_t value) {
    CHECK(at<=image_bytes && 4<=image_bytes-at);
    if (at>image_bytes || 4>image_bytes-at) return;
    for (unsigned i=0;i<4;++i) image[at+i]=(uint8_t)(value>>(8*i));
}

static void put64(size_t at,uint64_t value) {
    put32(at,(uint32_t)value);
    put32(at+4,(uint32_t)(value>>32));
}

static void hash_into(size_t begin,size_t bytes,size_t destination) {
    CHECK(begin<=image_bytes && bytes<=image_bytes-begin);
    CHECK(destination<=image_bytes && 20<=image_bytes-destination);
    if (begin>image_bytes || bytes>image_bytes-begin ||
        destination>image_bytes || 20>image_bytes-destination) return;
    uint8_t digest[20];
    CHECK(mbedtls_sha1(image+begin,bytes,digest)==0);
    memcpy(image+destination,digest,sizeof(digest));
}

/* Fixture utility only: retained fixture strings are positive UTF-8 FStrings. */
static size_t skip_string(size_t at,size_t bound,size_t *terminator) {
    const uint32_t bytes=get32(at);
    CHECK(bytes && bytes<4096 && at<=bound && 4<=bound-at && bytes<=bound-at-4);
    if (!bytes || bytes>=4096 || at>bound || 4>bound-at || bytes>bound-at-4) return bound;
    if (terminator) *terminator=at+4+bytes-1;
    return at+4+bytes;
}

static fixture_view locate(void) {
    fixture_view v={0};
    CHECK(image_bytes>=221);
    if (image_bytes<221) return v;
    v.footer=image_bytes-221;
    v.primary_offset=(size_t)get64(v.footer+25);
    v.primary_bytes=(size_t)get64(v.footer+33);
    CHECK(v.primary_offset<=v.footer && v.primary_bytes<=v.footer-v.primary_offset);
    size_t at=skip_string(v.primary_offset,v.primary_offset+v.primary_bytes,NULL);
    v.count_field=at; at+=4+8;
    CHECK(get32(at)==1); at+=4;
    v.path_offset_field=at;
    v.path_offset=(size_t)get64(at); v.path_bytes=(size_t)get64(at+8); at+=36;
    CHECK(get32(at)==1); at+=4;
    v.directory_offset_field=at;
    v.directory_offset=(size_t)get64(at); v.directory_bytes=(size_t)get64(at+8);
    v.directory_digest_field=at+16; at+=36;
    v.blob_bytes=get32(at); v.blob_offset=at+4;
    CHECK(v.blob_offset<=v.footer && v.blob_bytes<=v.footer-v.blob_offset);
    at=v.directory_offset;
    const size_t end=at+v.directory_bytes;
    const uint32_t directory_count=get32(at); at+=4;
    CHECK(directory_count<=8 && end<=v.footer);
    for (uint32_t d=0;d<directory_count && d<8;++d) {
        at=skip_string(at,end,NULL);
        const uint32_t count=get32(at); at+=4;
        CHECK(count<=8-v.file_count);
        for (uint32_t f=0;f<count && v.file_count<8;++f) {
            at=skip_string(at,end,&v.name_terminators[v.file_count]);
            v.reference_fields[v.file_count++]=at; at+=4;
        }
    }
    CHECK(at==end);
    return v;
}

static void rehash_primary(const fixture_view *v) {
    hash_into(v->primary_offset,v->primary_bytes,v->footer+41);
}

static void rehash_directory(const fixture_view *v) {
    hash_into(v->directory_offset,v->directory_bytes,v->directory_digest_field);
    rehash_primary(v);
}

static uint64_t selected_origin(const fixture_view *v,uint32_t file) {
    CHECK(file<v->file_count);
    if (file>=v->file_count) return 0;
    const size_t at=v->blob_offset+get32(v->reference_fields[file]);
    const uint32_t flags=get32(at);
    const size_t offset_at=at+4+((flags&63)==63 ? 4 : 0);
    return flags&(UINT32_C(1)<<31) ? get32(offset_at) : get64(offset_at);
}

static pak_provider_status read_image(void *context,uint64_t offset,uint8_t *destination,
    uint32_t requested,uint32_t *actual) {
    (void)context;
    CHECK(observed_calls<limits.max_read_calls);
    CHECK(observed_bytes<=limits.max_requested_read_bytes &&
          requested<=limits.max_requested_read_bytes-observed_bytes);
    CHECK(state.ledger.remaining_calls==limits.max_read_calls-observed_calls-1);
    CHECK(state.ledger.remaining_requested_bytes==limits.max_requested_read_bytes-observed_bytes-requested);
    CHECK(requested<=limits.max_read_bytes);
    CHECK(observed_calls<256);
    if (observed_calls>=256) { *actual=0; return PAK_PROVIDER_CONTRACT_ERROR; }
    trace_offsets[observed_calls]=offset;
    trace_requests[observed_calls]=requested;
    ++observed_calls;
    observed_bytes+=requested;
    *actual=0;
    uint64_t backing_offset=offset;
    if (virtual_tail_shift) {
        const uint64_t tail=(uint64_t)virtual_prefix_bytes+virtual_tail_shift;
        if (offset<virtual_prefix_bytes && requested<=virtual_prefix_bytes-offset) {
            backing_offset=offset;
        } else if (offset>=tail && offset-tail<=image_bytes-virtual_prefix_bytes &&
                   requested<=image_bytes-virtual_prefix_bytes-(offset-tail)) {
            backing_offset=offset-virtual_tail_shift;
        } else {
            ++virtual_hole_reads;
            return PAK_PROVIDER_UNAVAILABLE;
        }
    }
    if (backing_offset>image_bytes || requested>image_bytes-backing_offset) return PAK_PROVIDER_CONTRACT_ERROR;
    if (prefix_bytes && offset==fault_offset) {
        CHECK(prefix_source_offset<=image_bytes && prefix_bytes<=image_bytes-prefix_source_offset);
        CHECK(memcmp(output,prefix_expected ? prefix_expected : image+(size_t)prefix_source_offset,prefix_bytes)==0);
        CHECK(!all_value(directories,sizeof(directories),0) && !all_value(files,sizeof(files),0));
        prefix_work=codec_budget.work_used; prefix_steps=codec_budget.steps_used;
        ++prefix_observed;
    }
    if (fault!=FAULT_NONE && offset==fault_offset) {
        ++fault_hits;
        if (fault==FAULT_SHORT) {
            memcpy(destination,image+(size_t)backing_offset,requested-1); *actual=requested-1;
            return PAK_PROVIDER_OK;
        }
        if (fault==FAULT_OVERREPORT) { *actual=requested+1; return PAK_PROVIDER_OK; }
        if (fault==FAULT_CHANGED) return PAK_PROVIDER_CHANGED;
        if (fault==FAULT_CANCELLED) return PAK_PROVIDER_CANCELLED;
        return PAK_PROVIDER_IO_ERROR;
    }
    memcpy(destination,image+(size_t)backing_offset,requested);
    *actual=requested;
    return PAK_PROVIDER_OK;
}

static void prepare(void) {
    memset(reader,0x5a,sizeof(reader)); memset(primary,0x5a,sizeof(primary));
    memset(secondary,0x5a,sizeof(secondary)); memset(directory,0x5a,sizeof(directory));
    memset(names,0x5a,sizeof(names)); memset(physical,0x5a,sizeof(physical));
    memset(compressed,0x5a,sizeof(compressed)); memset(arena,0x5a,sizeof(arena));
    memset(output,0xa5,sizeof(output)); memset(directories,0x5a,sizeof(directories));
    memset(files,0x5a,sizeof(files)); memset(starts,0x5a,sizeof(starts));
    memset(sort_indices,0x5a,sizeof(sort_indices)); memset(extents,0x5a,sizeof(extents));
    memset(seen,0x5a,sizeof(seen)); memset(decode_blocks,0x5a,sizeof(decode_blocks));
    memset(selected_blocks,0x5a,sizeof(selected_blocks));
    memset(physical_blocks,0x5a,sizeof(physical_blocks));
    memset(&codec_state,0x5a,sizeof(codec_state)); memset(&state,0x5a,sizeof(state));
    memset(&result,0x5a,sizeof(result));
    codec_budget=bz_default_budget();
    pak_candidate_limits_v1_default(&limits);
    observed_bytes=0; observed_calls=0;
    memset(trace_offsets,0,sizeof(trace_offsets)); memset(trace_requests,0,sizeof(trace_requests));
    fault=FAULT_NONE; fault_offset=0; fault_hits=0;
    prefix_bytes=0; prefix_source_offset=0; prefix_observed=0;
    prefix_expected=NULL; prefix_work=0; prefix_steps=0;
    virtual_tail_shift=0; virtual_prefix_bytes=0; virtual_hole_reads=0;
    source=(pak_source_v1){sizeof(source),PAK_READER_ABI_VERSION,image_bytes,
        UINT64_C(0x43414e4449444154),PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE,0,NULL,read_image};
    buffers=(pak_candidate_buffers_v1){0};
    buffers.struct_size=sizeof(buffers); buffers.abi_version=PAK_CANDIDATE_ABI_VERSION;
    buffers.state=&state;
    buffers.reader_workspace=reader; buffers.reader_workspace_bytes=sizeof(reader);
    buffers.primary_capture=primary; buffers.primary_capacity=sizeof(primary);
    buffers.secondary_workspace=secondary; buffers.secondary_workspace_bytes=sizeof(secondary);
    buffers.directory_capture=directory; buffers.directory_capture_capacity=sizeof(directory);
    buffers.directories=directories; buffers.directories_capacity=8;
    buffers.files=files; buffers.files_capacity=8;
    buffers.names=names; buffers.names_capacity=sizeof(names);
    buffers.graph=(pak_graph_workspace_v1){sizeof(buffers.graph),PAK_REFERENCE_GRAPH_ABI_VERSION,
        starts,8,extents,8,sort_indices,8,seen,sizeof(seen),decode_blocks,64,selected_blocks,64};
    buffers.physical_header=physical; buffers.physical_header_capacity=sizeof(physical);
    buffers.physical_blocks=physical_blocks; buffers.physical_blocks_capacity=64;
    buffers.payload=(pak_reconciled_payload_buffers_v1){compressed,sizeof(compressed),
        arena,sizeof(arena),&codec_state,output,32768};
}

static pak_status run(uint32_t want,uint32_t selected) {
    return pak_validate_v11_candidate(&source,&limits,want,selected,&buffers,&codec_budget,
                                     NULL,NULL,&result);
}

static void scratch_cleared(void) {
    CHECK(all_value(reader,sizeof(reader),0) && all_value(primary,sizeof(primary),0));
    CHECK(all_value(secondary,sizeof(secondary),0) && all_value(directory,sizeof(directory),0));
    CHECK(all_value(physical,sizeof(physical),0) && all_value(physical_blocks,sizeof(physical_blocks),0));
    CHECK(all_value(compressed,sizeof(compressed),0) && all_value(arena,sizeof(arena),0));
    CHECK(all_value(&codec_state,sizeof(codec_state),0) && all_value(&state,sizeof(state),0));
    CHECK(all_value(starts,sizeof(starts),0) && all_value(extents,sizeof(extents),0));
    CHECK(all_value(sort_indices,sizeof(sort_indices),0) && all_value(seen,sizeof(seen),0));
    CHECK(all_value(decode_blocks,sizeof(decode_blocks),0) && all_value(selected_blocks,sizeof(selected_blocks),0));
    CHECK(all_value(output+buffers.payload.output_capacity,16,0xa5));
}

static void positives(void) {
    const char *archives[]={"pack_v11.pak","pack_v11_compress.pak"};
    const char *originals[]={"test.png","test.txt","zeros.bin","nested.txt"};
    const uint64_t stored[2][4]={{10257,446,2048,596},{7746,446,23,596}};
    for (size_t a=0;a<2;++a) for (uint32_t file=0;file<4;++file) {
        CHECK(load(archives[a],image,sizeof(image),&image_bytes));
        CHECK(load(originals[file],original,sizeof(original),&original_bytes));
        prepare();
        const pak_status status=run(1,file);
        CHECK(status==PAK_STATUS_OK && result.status==PAK_STATUS_OK);
        CHECK(result.metadata_complete==1 && result.selected_payload_complete==1);
        CHECK(result.stage==PAK_CANDIDATE_STAGE_COMPLETE && result.directory_count==2 && result.file_count==4);
        CHECK(result.decoded_bytes==original_bytes && memcmp(output,original,original_bytes)==0);
        CHECK(!result.installation_supported && !result.authenticity_verified &&
              !result.game_acceptance_verified && !result.path_hash_semantics_verified);
        CHECK(result.read_calls==observed_calls && result.requested_read_bytes==observed_bytes);
        uint64_t trace_sum=0;
        printf("trace %s:%s",archives[a],originals[file]);
        for (uint32_t i=0;i<observed_calls;++i) {
            trace_sum+=trace_requests[i];
            printf(" %llu+%u",(unsigned long long)trace_offsets[i],trace_requests[i]);
        }
        printf(" calls=%u summed=%llu\n",observed_calls,(unsigned long long)trace_sum);
        CHECK(trace_sum==observed_bytes);
        const uint64_t metadata_bytes=221+(a ? 185 : 173)+56+104;
        const uint64_t header_bytes=a && (file==0 || file==2) ? 73 : 53;
        /* One footer, 15 primary fields, two secondaries, one header, one payload. */
        CHECK(observed_calls==1+15+2+1+1);
        CHECK(observed_bytes==metadata_bytes+header_bytes+stored[a][file]);
        CHECK(all_value(output+original_bytes,buffers.payload.output_capacity-original_bytes,0xa5));
        scratch_cleared();
        printf("positive %s:%s status=%d complete=%u bytes=%llu\n",archives[a],originals[file],
               status,result.selected_payload_complete,(unsigned long long)result.decoded_bytes);
    }
}

static void accounting(void) {
    uint64_t sum=0;
    for (uint32_t i=0;i<observed_calls;++i) sum+=trace_requests[i];
    CHECK(sum==observed_bytes);
    CHECK(result.requested_read_bytes==sum && result.read_calls==observed_calls);
}

static void rollback(void) {
    CHECK(!result.metadata_complete && !result.selected_payload_complete && !result.decoded_bytes);
    CHECK(!result.directory_count && !result.file_count && !result.name_bytes);
    CHECK(!result.installation_supported && !result.authenticity_verified &&
          !result.game_acceptance_verified && !result.path_hash_semantics_verified);
    CHECK(all_value(output,buffers.payload.output_capacity,0));
    CHECK(all_value(output+buffers.payload.output_capacity,sizeof(output)-buffers.payload.output_capacity,0xa5));
    CHECK(all_value(directories,sizeof(directories),0) && all_value(files,sizeof(files),0));
    CHECK(all_value(names,sizeof(names),0));
    scratch_cleared(); accounting();
}

static void reject(const char *name,pak_status want,pak_candidate_stage stage,uint32_t selected) {
    const pak_status status=run(1,selected);
    printf("reject %s status=%d stage=%d child=%u reason=%u calls=%u bytes=%llu\n",
        name,status,result.stage,result.child_status,result.child_reason,result.read_calls,
        (unsigned long long)result.requested_read_bytes);
    CHECK(status==want && result.status==want && result.stage==stage);
    rollback();
}

static fixture_view real_fixture(const char *archive) {
    CHECK(load(archive,image,sizeof(image),&image_bytes));
    const fixture_view v=locate();
    CHECK(v.file_count==4);
    prepare();
    return v;
}

/* Portable metadata-only regression: keep the original small prefix and
 * relocate its captured tail around a virtual >4 MiB None entry. No bytes
 * from the large physical header/payload or the intervening hole exist. */
static void virtual_large_unselected(void) {
    const fixture_view v=real_fixture("pack_v11.pak");
    const size_t big_record=v.blob_offset+get32(v.reference_fields[2]); /* zeros.bin */
    const uint64_t big_origin=UINT64_C(8388608);
    const uint32_t big_size=4194305u;
    const uint64_t tail_origin=big_origin+53u+big_size;
    const uint64_t shift=tail_origin-v.primary_offset;
    CHECK(get32(big_record)==UINT32_C(0xe0000000)); /* None, 32-bit offset/size. */
    put32(big_record+4,(uint32_t)big_origin);
    put32(big_record+8,big_size);
    put64(v.path_offset_field,v.path_offset+shift);
    put64(v.directory_offset_field,v.directory_offset+shift);
    put64(v.footer+25,v.primary_offset+shift);
    rehash_primary(&v);

    prepare();
    virtual_prefix_bytes=v.primary_offset; virtual_tail_shift=shift;
    source.logical_size=image_bytes+shift;
    pak_status status=run(0,0);
    printf("virtual-large metadata status=%d stage=%d child=%u reason=%u\n",
        status,result.stage,result.child_status,result.child_reason);
    CHECK(status==PAK_STATUS_OK && result.metadata_complete && !result.selected_payload_complete &&
          result.stage==PAK_CANDIDATE_STAGE_COMPLETE && result.file_count==4);
    CHECK(!virtual_hole_reads && !codec_budget.work_used);
    const uint32_t metadata_calls=observed_calls;
    const uint64_t metadata_bytes=observed_bytes;
    CHECK(metadata_bytes==221u+v.primary_bytes+v.path_bytes+v.directory_bytes);
    CHECK(all_value(output,buffers.payload.output_capacity,0));
    accounting(); scratch_cleared();

    CHECK(load("nested.txt",original,sizeof(original),&original_bytes));
    prepare();
    virtual_prefix_bytes=v.primary_offset; virtual_tail_shift=shift;
    source.logical_size=image_bytes+shift;
    status=run(1,3);
    printf("virtual-large small-selection status=%d stage=%d child=%u reason=%u\n",
        status,result.stage,result.child_status,result.child_reason);
    CHECK(status==PAK_STATUS_OK && result.metadata_complete && result.selected_payload_complete &&
          result.decoded_bytes==original_bytes && memcmp(output,original,original_bytes)==0);
    CHECK(!virtual_hole_reads && !result.installation_supported && !result.authenticity_verified &&
          !result.game_acceptance_verified && !result.path_hash_semantics_verified);
    accounting(); scratch_cleared();

    prepare();
    virtual_prefix_bytes=v.primary_offset; virtual_tail_shift=shift;
    source.logical_size=image_bytes+shift;
    reject("virtual oversized selection",PAK_STATUS_RESOURCE_LIMIT,PAK_CANDIDATE_STAGE_SELECTED_EXTENT,2);
    CHECK(!virtual_hole_reads && !codec_budget.work_used);
    CHECK(observed_calls==metadata_calls && observed_bytes==metadata_bytes);
    for(uint32_t i=0;i<observed_calls;++i)CHECK(trace_offsets[i]>=tail_origin);
}

static void mutations(void) {
    const char *archives[]={"pack_v11.pak","pack_v11_compress.pak"};
    for (size_t a=0;a<2;++a) {
        fixture_view v=real_fixture(archives[a]);
        image[v.primary_offset+4]^=1;
        reject("primary hash",PAK_STATUS_CHECKSUM_MISMATCH,PAK_CANDIDATE_STAGE_PRIMARY,3);

        v=real_fixture(archives[a]); image[v.path_offset]^=1;
        reject("path secondary hash",PAK_STATUS_CHECKSUM_MISMATCH,PAK_CANDIDATE_STAGE_PATH_SECONDARY,3);

        v=real_fixture(archives[a]); image[v.directory_offset+4]^=1;
        reject("directory secondary hash",PAK_STATUS_CHECKSUM_MISMATCH,PAK_CANDIDATE_STAGE_DIRECTORY_SECONDARY,3);

        v=real_fixture(archives[a]);
        const uint64_t origin=selected_origin(&v,3);
        image[(size_t)origin+28]^=1;
        reject("physical stored SHA1",PAK_STATUS_CHECKSUM_MISMATCH,PAK_CANDIDATE_STAGE_PAYLOAD,3);
        CHECK(codec_budget.work_used==596);

        v=real_fixture(archives[a]); image[(size_t)selected_origin(&v,3)+53+100]^=1;
        reject("raw None byte",PAK_STATUS_CHECKSUM_MISMATCH,PAK_CANDIDATE_STAGE_PAYLOAD,3);
        CHECK(codec_budget.work_used==596);

        v=real_fixture(archives[a]); put32(v.reference_fields[0],get32(v.reference_fields[0])+1);
        rehash_directory(&v);
        reject("middle reference",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_REFERENCE_GRAPH,3);
        CHECK(result.child_reason==PAK_GRAPH_REASON_REFERENCE_NOT_START);

        v=real_fixture(archives[a]); put32(v.reference_fields[1],get32(v.reference_fields[0]));
        rehash_directory(&v);
        reject("aliased reference",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_REFERENCE_GRAPH,3);
        CHECK(result.child_reason==PAK_GRAPH_REASON_REFERENCE_ALIAS);

        v=real_fixture(archives[a]); put32(v.count_field,5); rehash_primary(&v);
        reject("primary count mismatch",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_REFERENCE_GRAPH,3);
        CHECK(result.child_reason==PAK_GRAPH_REASON_COUNT_MISMATCH);

        v=real_fixture(archives[a]);
        const size_t late=v.blob_offset+get32(v.reference_fields[3]);
        put32(late,get32(late)|(UINT32_C(1)<<22)); rehash_primary(&v);
        reject("late encrypted record",PAK_STATUS_UNSUPPORTED,PAK_CANDIDATE_STAGE_REFERENCE_GRAPH,0);
        CHECK(result.child_reason==PAK_GRAPH_REASON_ENTRY_DECODER && result.graph_steps>=4);

        v=real_fixture(archives[a]);
        const size_t late_bad=v.blob_offset+get32(v.reference_fields[3]);
        put32(late_bad,(get32(late_bad)&~(UINT32_C(63)<<23))|(UINT32_C(5)<<23));
        rehash_primary(&v);
        reject("late invalid method",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_REFERENCE_GRAPH,0);
        CHECK(result.child_reason==PAK_GRAPH_REASON_ENTRY_DECODER && result.graph_steps>=4);

        v=real_fixture(archives[a]); image[v.name_terminators[3]]=1; rehash_directory(&v);
        reject("late bad name",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_DIRECTORY_PARSE,0);
        CHECK(result.child_reason==PAK_DIRECTORY_REASON_FSTRING_TERMINATOR);

        v=real_fixture(archives[a]); put32(v.reference_fields[3],UINT32_C(0x80000000)); rehash_directory(&v);
        reject("tombstone reference",PAK_STATUS_UNSUPPORTED,PAK_CANDIDATE_STAGE_REFERENCE_GRAPH,0);
        CHECK(result.child_reason==PAK_GRAPH_REASON_REFERENCE_UNSUPPORTED);

        v=real_fixture(archives[a]); put32(v.reference_fields[3],UINT32_MAX); rehash_directory(&v);
        reject("nonencoded reference",PAK_STATUS_UNSUPPORTED,PAK_CANDIDATE_STAGE_DIRECTORY_PARSE,0);
        CHECK(result.child_reason==PAK_DIRECTORY_REASON_LOCATION_UNSUPPORTED);

        v=real_fixture(archives[a]);
        const size_t physical_at=(size_t)selected_origin(&v,3);
        image[physical_at+16]^=1;
        reject("physical size conflict",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_RECONCILE,3);
    }

    fixture_view v=real_fixture("pack_v11_compress.pak");
    const size_t origin=(size_t)selected_origin(&v,0);
    CHECK(load("test.png",original,sizeof(original),&original_bytes));
    uint8_t decoded_hash[20]; CHECK(mbedtls_sha1(original,original_bytes,decoded_hash)==0);
    memcpy(image+origin+28,decoded_hash,sizeof(decoded_hash));
    reject("decoded hash substitution",PAK_STATUS_CHECKSUM_MISMATCH,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(codec_budget.steps_used>0 && codec_budget.allocations_used>0);

    v=real_fixture("pack_v11_compress.pak");
    const size_t compressed_origin=(size_t)selected_origin(&v,0);
    image[compressed_origin+73+(size_t)get64(compressed_origin+8)-1]^=1;
    reject("Zlib stored byte corruption",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(result.child_status==PAK_RECONCILED_PAYLOAD_CODEC_REJECTED);
}

static void limits_and_late_reads(void) {
    fixture_view v=real_fixture("pack_v11.pak");
    limits.max_read_bytes=4096;
    CHECK(run(1,0)==PAK_STATUS_OK);
    const uint64_t exact_bytes=observed_bytes;
    const uint32_t exact_calls=observed_calls;
    CHECK(exact_calls==22 && exact_bytes==10864);
    CHECK(codec_budget.work_used==10257 && codec_budget.steps_used==3);
    accounting(); scratch_cleared();

    (void)real_fixture("pack_v11.pak");
    limits.max_read_bytes=4096; limits.max_requested_read_bytes=exact_bytes; limits.max_read_calls=exact_calls;
    CHECK(run(1,0)==PAK_STATUS_OK && result.selected_payload_complete);
    CHECK(observed_bytes==exact_bytes && observed_calls==exact_calls);
    accounting(); scratch_cleared();

    v=real_fixture("pack_v11.pak");
    limits.max_read_bytes=4096; limits.max_requested_read_bytes=exact_bytes-1;
    prefix_source_offset=selected_origin(&v,0)+53; fault_offset=prefix_source_offset+8192; prefix_bytes=8192;
    reject("one-short aggregate bytes",PAK_STATUS_RESOURCE_LIMIT,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(result.io_cause==PAK_CANDIDATE_IO_BYTE_BUDGET);
    CHECK(observed_calls==exact_calls-1 && observed_bytes==exact_bytes-2065);
    CHECK(codec_budget.work_used==8192 && !prefix_observed);

    v=real_fixture("pack_v11.pak");
    limits.max_read_bytes=4096; limits.max_read_calls=exact_calls-1;
    reject("one-short aggregate calls",PAK_STATUS_RESOURCE_LIMIT,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(result.io_cause==PAK_CANDIDATE_IO_CALL_BUDGET);
    CHECK(observed_calls==exact_calls-1 && observed_bytes==exact_bytes-2065);
    CHECK(codec_budget.work_used==8192);

    const fault_kind kinds[]={FAULT_SHORT,FAULT_CHANGED,FAULT_CANCELLED,FAULT_OVERREPORT,FAULT_IO};
    const pak_status statuses[]={PAK_STATUS_PROVIDER_SHORT_READ,PAK_STATUS_PROVIDER_CHANGED,
        PAK_STATUS_CANCELLED,PAK_STATUS_PROVIDER_CONTRACT,PAK_STATUS_PROVIDER_ERROR};
    const char *names_[]={"late short","late Changed","late Cancelled","late overreport","late IO error"};
    for (size_t i=0;i<sizeof(kinds)/sizeof(kinds[0]);++i) {
        v=real_fixture("pack_v11.pak"); limits.max_read_bytes=4096;
        fault=kinds[i]; prefix_source_offset=selected_origin(&v,0)+53;
        fault_offset=prefix_source_offset+4096; prefix_bytes=4096;
        reject(names_[i],statuses[i],PAK_CANDIDATE_STAGE_PAYLOAD,0);
        CHECK(fault_hits==1 && prefix_observed==1);
        CHECK(observed_calls==exact_calls-1 && observed_bytes==607+8192);
        CHECK(codec_budget.work_used==4096 && codec_budget.steps_used==1);
        CHECK(result.last_requested==4096);
        CHECK(result.last_actual==(kinds[i]==FAULT_SHORT ? 4095u : kinds[i]==FAULT_OVERREPORT ? 4097u : 0u));
    }

    v=real_fixture("pack_v11.pak"); limits.max_read_bytes=4096;
    prefix_source_offset=selected_origin(&v,0)+53; fault_offset=prefix_source_offset+4096; prefix_bytes=4096;
    codec_budget.work_limit=4096;
    reject("late unreplenished codec budget",PAK_STATUS_RESOURCE_LIMIT,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(prefix_observed==1 && codec_budget.work_used==4096 && codec_budget.steps_used==1);
    CHECK(observed_calls==exact_calls-1 && observed_bytes==607+8192);

    (void)real_fixture("pack_v11_compress.pak"); limits.max_read_bytes=64;
    CHECK(run(1,2)==PAK_STATUS_OK && result.decoded_bytes==2048);
    CHECK(observed_bytes==662 && observed_calls>20);
    accounting(); scratch_cleared();
}

static void untouched(void) {
    CHECK(!observed_calls && !observed_bytes);
    CHECK(all_value(&result,sizeof(result),0x5a) && all_value(&state,sizeof(state),0x5a));
    CHECK(all_value(reader,sizeof(reader),0x5a) && all_value(primary,sizeof(primary),0x5a));
    CHECK(all_value(secondary,sizeof(secondary),0x5a) && all_value(directory,sizeof(directory),0x5a));
    CHECK(all_value(directories,sizeof(directories),0x5a) && all_value(files,sizeof(files),0x5a));
    CHECK(all_value(names,sizeof(names),0x5a) && all_value(physical,sizeof(physical),0x5a));
    CHECK(all_value(compressed,sizeof(compressed),0x5a) && all_value(arena,sizeof(arena),0x5a));
    CHECK(all_value(&codec_state,sizeof(codec_state),0x5a) && all_value(output,sizeof(output),0xa5));
    CHECK(all_value(starts,sizeof(starts),0x5a) && all_value(extents,sizeof(extents),0x5a));
    CHECK(all_value(sort_indices,sizeof(sort_indices),0x5a) && all_value(seen,sizeof(seen),0x5a));
    CHECK(all_value(decode_blocks,sizeof(decode_blocks),0x5a) && all_value(selected_blocks,sizeof(selected_blocks),0x5a));
    CHECK(all_value(physical_blocks,sizeof(physical_blocks),0x5a));
    const bz_budget fresh=bz_default_budget();
    CHECK(memcmp(&codec_budget,&fresh,sizeof(fresh))==0);
}

static int cancel_after_decode(void *context) {
    (void)context;
    return result.stage==PAK_CANDIDATE_STAGE_PAYLOAD && codec_budget.work_used!=0;
}

static void aliases_and_final_cancel(void) {
    (void)real_fixture("pack_v11.pak"); buffers.payload.compressed=output+1;
    CHECK(run(1,3)==PAK_STATUS_BAD_ARGUMENT); untouched();

    (void)real_fixture("pack_v11.pak"); buffers.graph.selected_blocks=decode_blocks;
    CHECK(run(1,3)==PAK_STATUS_BAD_ARGUMENT); untouched();

    (void)real_fixture("pack_v11.pak");
    CHECK(pak_validate_v11_candidate(&source,&limits,1,3,&buffers,&codec_budget,
        cancel_after_decode,NULL,&result)==PAK_STATUS_CANCELLED);
    CHECK(result.stage==PAK_CANDIDATE_STAGE_PAYLOAD && codec_budget.work_used==596);
    rollback();
}

static void synthetic_candidates(void) {
    CHECK(load_from(synthetic_root,"empty.pak",image,sizeof(image),&image_bytes));
    prepare();
    CHECK(run(0,0)==PAK_STATUS_OK && result.metadata_complete==1 && !result.selected_payload_complete);
    CHECK(result.stage==PAK_CANDIDATE_STAGE_COMPLETE && !result.file_count && !result.directory_count && !result.name_bytes);
    CHECK(!result.decoded_bytes && !codec_budget.work_used);
    CHECK(all_value(output,buffers.payload.output_capacity,0));
    accounting(); scratch_cleared();

    CHECK(load_from(synthetic_root,"two-block.pak",image,sizeof(image),&image_bytes));
    CHECK(load_from(synthetic_root,"two-block.bin",original,sizeof(original),&original_bytes));
    const uint64_t second_block=get64(68);
    prepare();
    fault_offset=second_block; prefix_bytes=64; prefix_expected=original;
    CHECK(run(1,0)==PAK_STATUS_OK && result.metadata_complete && result.selected_payload_complete);
    CHECK(result.file_count==1 && result.directory_count==1 && result.decoded_bytes==original_bytes);
    CHECK(memcmp(output,original,original_bytes)==0 && codec_budget.allocations_used>1);
    const uint64_t shared_work=codec_budget.work_used;
    const uint64_t shared_steps=codec_budget.steps_used;
    const uint64_t first_work=prefix_work, first_steps=prefix_steps;
    CHECK(prefix_observed==1 && first_steps==3 && first_work>0 && first_work<shared_work);
    accounting(); scratch_cleared();
    printf("synthetic two-block bytes=%zu work=%llu steps=%llu\n",original_bytes,
        (unsigned long long)shared_work,(unsigned long long)shared_steps);

    CHECK(load_from(synthetic_root,"two-block.pak",image,sizeof(image),&image_bytes));
    prepare();
    fault_offset=second_block; prefix_bytes=64; prefix_expected=original;
    image[(size_t)second_block+(size_t)(get64(76)-second_block)-1]^=1;
    reject("second Zlib block corrupt",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(result.child_status==PAK_RECONCILED_PAYLOAD_CODEC_REJECTED && codec_budget.allocations_used>1);
    CHECK(prefix_observed==1 && prefix_work==first_work && prefix_steps==first_steps);

    CHECK(load_from(synthetic_root,"two-block.pak",image,sizeof(image),&image_bytes));
    prepare(); codec_budget.step_limit=first_steps;
    fault_offset=second_block; prefix_bytes=64; prefix_expected=original;
    reject("two-block aggregate step budget",PAK_STATUS_RESOURCE_LIMIT,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(prefix_observed==1 && codec_budget.steps_used==first_steps && codec_budget.work_used==first_work);

    CHECK(load_from(synthetic_root,"two-block-truncated.pak",image,sizeof(image),&image_bytes));
    prepare(); fault_offset=second_block; prefix_bytes=64; prefix_expected=original;
    reject("second Zlib stream truncated",PAK_STATUS_MALFORMED,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(result.child_status==PAK_RECONCILED_PAYLOAD_CODEC_REJECTED && result.child_reason==BZ_TRUNCATED);
    CHECK(prefix_observed==1 && prefix_work==first_work && prefix_steps==first_steps);

    CHECK(load_from(synthetic_root,"two-block.pak",image,sizeof(image),&image_bytes));
    prepare(); fault=FAULT_CHANGED; fault_offset=second_block; prefix_bytes=64; prefix_expected=original;
    reject("second compressed block Changed",PAK_STATUS_PROVIDER_CHANGED,PAK_CANDIDATE_STAGE_PAYLOAD,0);
    CHECK(fault_hits==1 && prefix_observed==1 && codec_budget.work_used==first_work);
}

int main(int argc,char **argv) {
    if (argc!=3) return 2;
    fixture_root=argv[1];
    synthetic_root=argv[2];
    positives();
    mutations();
    limits_and_late_reads();
    aliases_and_final_cancel();
    synthetic_candidates();
    virtual_large_unselected();
    printf("composed checks=%u failures=%u\n",checks,failures);
    return failures ? 1 : 0;
}
