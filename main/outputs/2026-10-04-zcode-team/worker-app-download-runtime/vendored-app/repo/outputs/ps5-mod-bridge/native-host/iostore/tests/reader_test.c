#include "toc_reader.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only this fixture caller uses host stdio. Captured corpus paths/lengths arrive
 * as arguments, with no title/mod identifiers or captured assets in source. */
static unsigned groups, checks, failures;
static uint8_t header[144], id[12], logical[10], block[12];
static mb_iostore_view_v1 view;
static mb_iostore_limits_v1 limits;
static mb_iostore_header_result_v1 h;
static mb_iostore_chunk_result_v1 c;
static mb_iostore_block_result_v1 b;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL line=%u %s\n",(unsigned)__LINE__,#x); } } while(0)
#define REQUIRE(x) do { CHECK(x); if (!(x)) return; } while(0)

static void le(uint8_t *p, uint64_t n, unsigned bytes) {
    for (unsigned i=0;i<bytes;++i) { p[i]=(uint8_t)n; n>>=8; }
}
static void be40(uint8_t *p, uint64_t n) {
    for (unsigned i=0;i<5;++i) p[4-i]=(uint8_t)(n>>(8*i));
}
static int filled(const void *p, size_t bytes, uint8_t value) {
    const uint8_t *s=p; for(size_t i=0;i<bytes;++i) if(s[i]!=value) return 0; return 1;
}
static void base(void) {
    static const uint8_t magic[]="-==--==--==--==-";
    memset(header,0,sizeof(header)); memcpy(header,magic,16); header[16]=8;
    le(header+20,144,4); le(header+24,2,4); le(header+28,3,4); le(header+32,12,4);
    le(header+36,1,4); le(header+40,32,4); le(header+44,64,4); le(header+48,16,4);
    le(header+52,1,4); le(header+56,UINT64_C(0x1122334455667788),8);
    header[80]=8; le(header+84,1,4); le(header+88,UINT64_MAX,8); le(header+96,1,4);
    view=(mb_iostore_view_v1){sizeof(view),1,{header,sizeof(header),0},328};
    mb_iostore_limits_v1_default(&limits);
}
static void no_header_facts(mb_iostore_status rc) {
    CHECK(h.status==(uint32_t)rc && h.struct_size==sizeof(h) && h.abi_version==1);
    CHECK(!h.header_parsed && !h.table_geometry_valid && !h.chunk_count && !h.block_count);
    CHECK(!h.chunk_ids.offset && !h.compression_blocks.bytes && !h.directory_index.bytes && !h.declared_layout_end);
    CHECK(!h.full_toc_validated && !h.logical_block_coverage_verified && !h.payload_verified &&
        !h.authenticity_verified && !h.runtime_supported && !h.installation_supported);
}
static void header_reject(mb_iostore_status expected, mb_iostore_reason reason) {
    memset(&h,0xa5,sizeof(h)); CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==expected);
    no_header_facts(expected); CHECK(h.reason==(uint32_t)reason);
}

static void header_checks(void) {
    ++groups; base();
    CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_OK);
    CHECK(h.header_parsed && h.table_geometry_valid && h.version==8 && h.container_flags_raw==8);
    CHECK(h.chunk_count==2 && h.block_count==3 && h.metadata_entry_bytes==24 && h.partition_count==1);
    CHECK(h.chunk_ids.offset==144 && h.chunk_ids.bytes==24 && h.chunk_offset_lengths.offset==168 && h.chunk_offset_lengths.bytes==20);
    CHECK(h.hash_seeds.offset==188 && h.hash_seeds.bytes==4 && h.overflow_chunks.offset==192 && h.overflow_chunks.bytes==4);
    CHECK(h.compression_blocks.offset==196 && h.compression_blocks.bytes==36 && h.compression_methods.offset==232 && h.compression_methods.bytes==32);
    CHECK(h.directory_index.offset==264 && h.directory_index.bytes==16 && h.entry_metadata.offset==280 && h.entry_metadata.bytes==48 && h.declared_layout_end==328);
    CHECK(h.logical_block_capacity==192 && h.container_id==UINT64_C(0x1122334455667788) && h.partition_bytes==UINT64_MAX);
    CHECK(!h.full_toc_validated && !h.logical_block_coverage_verified && !h.payload_verified && !h.authenticity_verified && !h.runtime_supported && !h.installation_supported);
    view.toc_logical_bytes=400; CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_OK && h.declared_layout_end==328);
    base(); view.header.bytes=143; header_reject(MB_IOSTORE_TRUNCATED,MB_IOSTORE_REASON_CAPTURE);
    base(); view.header.data=NULL; view.header.bytes=0; header_reject(MB_IOSTORE_TRUNCATED,MB_IOSTORE_REASON_CAPTURE);
    base(); header[0]^=1; header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_MAGIC);
    base(); header[16]=7; header_reject(MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_VERSION);
    base(); header[16]=9; header_reject(MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_VERSION);
    base(); le(header+20,160,4); header_reject(MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_HEADER_WIDTH);
    base(); le(header+32,16,4); header_reject(MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_TABLE_WIDTH);
    const uint8_t rejected_flags[]={2,4,16,128};
    for(unsigned i=0;i<sizeof(rejected_flags);++i) { base(); header[80]=rejected_flags[i]; header_reject(MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_FLAGS); }
    base(); header[80]=0; header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_DIRECTORY_FLAG);
    base(); le(header+40,0,4); header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_TABLE_WIDTH);
    base(); le(header+36,17,4); header_reject(MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
    base(); le(header+40,65,4); header_reject(MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
    const unsigned capped[]={24,28,84,96};
    const uint32_t cap[]={MB_IOSTORE_MAX_CHUNKS,MB_IOSTORE_MAX_BLOCKS,MB_IOSTORE_MAX_CHUNKS,MB_IOSTORE_MAX_CHUNKS};
    for(unsigned i=0;i<4;++i) { base(); le(header+capped[i],(uint64_t)cap[i]+1,4); header_reject(MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT); }
    base(); le(header+52,0,4); header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_PARTITION);
    base(); le(header+52,17,4); header_reject(MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
    base(); le(header+88,0,8); header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_PARTITION);
    base(); le(header+52,2,4); header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_PARTITION);
    base(); le(header+44,0,4); header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_BLOCK_SIZE);
    base(); view.toc_logical_bytes=327; header_reject(MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_TOC_RANGE);
    base(); view.toc_logical_bytes=MB_IOSTORE_MAX_TOC_BYTES+1; header_reject(MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
    base(); limits.max_chunks=1; header_reject(MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
    base(); limits.max_toc_bytes=327; header_reject(MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
    base(); le(header+24,0,4); le(header+28,0,4); le(header+36,0,4); le(header+48,0,4); le(header+84,0,4); le(header+96,0,4); header[80]=0;
    view.toc_logical_bytes=144;
    CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_OK && h.declared_layout_end==144 && !h.chunk_count && !h.block_count);
}

static void no_chunk_facts(mb_iostore_status status) {
    CHECK(c.status==(uint32_t)status && !c.descriptor_valid && !c.chunk_id && !c.logical_offset && !c.logical_bytes && !c.block_count);
    CHECK(filled(c.chunk_id_raw,sizeof(c.chunk_id_raw),0));
    CHECK(!c.logical_block_coverage_verified && !c.paired_ranges_verified && !c.payload_verified && !c.authenticity_verified && !c.runtime_supported && !c.installation_supported);
}
static void chunk_checks(void) {
    ++groups; base();
    memset(id,0,sizeof(id)); le(id,UINT64_C(0xfedcba9876543210),8); id[8]=0x12; id[9]=0x34; id[10]=0xa7; id[11]=255;
    be40(logical,60); be40(logical+5,80);
    mb_iostore_span_v1 ids={id,12,156}, lengths={logical,10,178};
    CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_OK);
    CHECK(c.descriptor_valid && c.toc_index==1 && c.chunk_id==UINT64_C(0xfedcba9876543210) && c.chunk_index==0x1234 && c.chunk_type_raw==255 && c.padding_raw==0xa7);
    CHECK(c.logical_offset==60 && c.logical_bytes==80 && c.logical_end==140 && c.first_block==0 && c.last_block==2 && c.block_count==3);
    CHECK(!c.logical_block_coverage_verified && !c.paired_ranges_verified && !c.payload_verified && !c.runtime_supported && !c.installation_supported);
    lengths.offset=177; memset(&c,0xa5,sizeof(c)); CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_MALFORMED); no_chunk_facts(MB_IOSTORE_MALFORMED);
    lengths.offset=178; ids.bytes=11; CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_TRUNCATED); no_chunk_facts(MB_IOSTORE_TRUNCATED);
    ids.bytes=12; lengths.bytes=9; CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_TRUNCATED); no_chunk_facts(MB_IOSTORE_TRUNCATED);
    lengths.bytes=10; CHECK(mb_iostore_read_chunk_v1(&view,&limits,2,&ids,&lengths,&c)==MB_IOSTORE_OUT_OF_RANGE); no_chunk_facts(MB_IOSTORE_OUT_OF_RANGE);
    be40(logical,191); be40(logical+5,2); CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_MALFORMED); no_chunk_facts(MB_IOSTORE_MALFORMED);
    be40(logical,192); be40(logical+5,0); CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_OK && !c.block_count && c.logical_end==192);
    /* Descriptor metadata beyond a payload buffer ceiling stays admissible. */
    base(); le(header+28,100000,4); le(header+44,65536,4); view.toc_logical_bytes=UINT64_C(2000000);
    be40(logical,0); be40(logical+5,UINT64_C(5242881));
    CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_OK && c.logical_bytes==UINT64_C(5242881));
    be40(logical,UINT64_C(0xffffffffff)); be40(logical+5,UINT64_C(0xffffffffff));
    CHECK(mb_iostore_read_chunk_v1(&view,&limits,1,&ids,&lengths,&c)==MB_IOSTORE_MALFORMED); no_chunk_facts(MB_IOSTORE_MALFORMED);
}

static void no_block_facts(mb_iostore_status status) {
    CHECK(b.status==(uint32_t)status && !b.descriptor_valid && !b.stored_bytes && !b.decoded_bytes && !b.physical_offset && !b.partition_end && !b.paired_extent_checked);
    CHECK(!b.full_toc_validated && !b.logical_block_coverage_verified && !b.payload_verified && !b.authenticity_verified && !b.runtime_supported && !b.installation_supported);
}
static void make_block(uint64_t offset,uint32_t stored,uint32_t decoded,uint8_t method) {
    le(block,offset,5); le(block+5,stored,3); le(block+8,decoded,3); block[11]=method;
}
static void block_checks(void) {
    ++groups; base(); le(header+52,2,4); le(header+88,100,8);
    uint64_t paired[]={100,90}; mb_iostore_span_v1 row={block,12,208};
    make_block(120,30,40,1);
    CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_OK);
    CHECK(b.descriptor_valid && b.toc_index==1 && b.partition_index==1 && b.physical_offset==120 && b.partition_offset==20 && b.partition_end==50 && b.paired_extent_checked);
    CHECK(b.method_index==1 && b.stored_bytes==30 && b.decoded_bytes==40 && !b.built_in_none);
    CHECK(!b.full_toc_validated && !b.logical_block_coverage_verified && !b.payload_verified && !b.runtime_supported && !b.installation_supported);
    make_block(80,10,10,0); CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_OK && b.built_in_none && b.partition_index==0);
    make_block(80,10,11,0); CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_MALFORMED); no_block_facts(MB_IOSTORE_MALFORMED); CHECK(b.reason==MB_IOSTORE_REASON_NONE_SIZE);
    make_block(80,10,10,2); CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_MALFORMED); no_block_facts(MB_IOSTORE_MALFORMED);
    make_block(80,10,65,1); CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_MALFORMED); no_block_facts(MB_IOSTORE_MALFORMED);
    make_block(90,11,11,0); CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_OUT_OF_RANGE); no_block_facts(MB_IOSTORE_OUT_OF_RANGE);
    make_block(185,6,6,0); CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_OUT_OF_RANGE); no_block_facts(MB_IOSTORE_OUT_OF_RANGE);
    make_block(200,1,1,0); CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_OUT_OF_RANGE); no_block_facts(MB_IOSTORE_OUT_OF_RANGE);
    make_block(80,10,10,0); row.bytes=11; CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_TRUNCATED); no_block_facts(MB_IOSTORE_TRUNCATED);
    row.bytes=12; row.offset=207; CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,2,&b)==MB_IOSTORE_MALFORMED); no_block_facts(MB_IOSTORE_MALFORMED);
    row.offset=208; CHECK(mb_iostore_read_block_v1(&view,&limits,3,&row,paired,2,&b)==MB_IOSTORE_OUT_OF_RANGE); no_block_facts(MB_IOSTORE_OUT_OF_RANGE);
    CHECK(mb_iostore_read_block_v1(&view,&limits,1,&row,paired,1,&b)==MB_IOSTORE_MALFORMED); no_block_facts(MB_IOSTORE_MALFORMED);
    /* A declared paired 128GB file is geometry input, never copied/read here. */
    base(); uint64_t large=UINT64_C(128240377200); row.offset=196;
    make_block(UINT64_C(128240377199),1,1,0);
    CHECK(mb_iostore_read_block_v1(&view,&limits,0,&row,&large,1,&b)==MB_IOSTORE_OK && b.partition_end==large);
    --large; CHECK(mb_iostore_read_block_v1(&view,&limits,0,&row,&large,1,&b)==MB_IOSTORE_OUT_OF_RANGE); no_block_facts(MB_IOSTORE_OUT_OF_RANGE);
    make_block(UINT64_C(0xffffffffff),0xffffff,0xffffff,1); le(header+44,0xffffff,4); large=UINT64_MAX;
    CHECK(mb_iostore_read_block_v1(&view,&limits,0,&row,&large,1,&b)==MB_IOSTORE_OK && b.physical_offset==UINT64_C(0xffffffffff) && b.stored_bytes==0xffffff);
}

static void preflight_checks(void) {
    ++groups; base(); memset(&h,0xa5,sizeof(h)); view.abi_version=2;
    CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_BAD_ARGUMENT && filled(&h,sizeof(h),0xa5));
    base(); limits.max_chunks=MB_IOSTORE_MAX_CHUNKS+1;
    CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_BAD_ARGUMENT && filled(&h,sizeof(h),0xa5));
    base(); view.header.offset=1;
    CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_BAD_ARGUMENT && filled(&h,sizeof(h),0xa5));
    base(); view.header.data=NULL;
    CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_BAD_ARGUMENT && filled(&h,sizeof(h),0xa5));
    base(); view.header.data=(const uint8_t *)&h; view.header.bytes=144;
    CHECK(mb_iostore_read_header_v1(&view,&limits,&h)==MB_IOSTORE_BAD_ARGUMENT && filled(&h,sizeof(h),0xa5));
    base();
    CHECK(mb_iostore_read_header_v1(&view,&limits,(mb_iostore_header_result_v1 *)((uint8_t *)&h+1))==MB_IOSTORE_BAD_ARGUMENT && filled(&h,sizeof(h),0xa5));
    CHECK(mb_iostore_read_header_v1(NULL,&limits,&h)==MB_IOSTORE_BAD_ARGUMENT && filled(&h,sizeof(h),0xa5));
    mb_iostore_span_v1 span={block,12,196}; uint64_t paired=100;
    memset(&b,0xa5,sizeof(b)); span.data=(const uint8_t *)&b;
    CHECK(mb_iostore_read_block_v1(&view,&limits,0,&span,&paired,1,&b)==MB_IOSTORE_BAD_ARGUMENT && filled(&b,sizeof(b),0xa5));
    span.data=block;
    CHECK(mb_iostore_read_block_v1(&view,&limits,0,&span,&paired,17,&b)==MB_IOSTORE_BAD_ARGUMENT && filled(&b,sizeof(b),0xa5));
    CHECK(mb_iostore_read_block_v1(&view,&limits,0,&span,(const uint64_t *)((const uint8_t *)&paired+1),1,&b)==MB_IOSTORE_BAD_ARGUMENT && filled(&b,sizeof(b),0xa5));
    mb_iostore_span_v1 ids={id,12,144}, lengths={logical,10,168}; memset(&c,0xa5,sizeof(c)); lengths.data=(const uint8_t *)&c;
    CHECK(mb_iostore_read_chunk_v1(&view,&limits,0,&ids,&lengths,&c)==MB_IOSTORE_BAD_ARGUMENT && filled(&c,sizeof(c),0xa5));
}

static int capture(FILE *f,uint64_t offset,uint8_t *to,size_t bytes) {
    return offset<=(uint64_t)LONG_MAX && fseek(f,(long)offset,SEEK_SET)==0 && fread(to,1,bytes,f)==bytes;
}
static uint32_t selected_index(uint32_t count,unsigned i,unsigned all) {
    return all || count<=3 ? i : i==0 ? 0 : i==1 ? count/2 : count-1;
}
static void actual_fixture(unsigned ordinal,const char *path,const char *paired_text,const char *selected_text) {
    ++groups; char *end=NULL; const uint64_t paired=strtoull(paired_text,&end,10);
    REQUIRE(end && end!=paired_text && !*end && paired_text[0]!='-');
    const unsigned long selected=strtoul(selected_text,&end,10);
    REQUIRE(end && end!=selected_text && !*end && selected_text[0]!='-' && selected<=UINT32_MAX);
    FILE *f=fopen(path,"rb"); REQUIRE(f!=NULL);
    if(fseek(f,0,SEEK_END) || ftell(f)<0) { CHECK(0); fclose(f); return; }
    const uint64_t size=(uint64_t)ftell(f);
    if(!capture(f,0,header,sizeof(header))) { CHECK(0); fclose(f); return; }
    view=(mb_iostore_view_v1){sizeof(view),1,{header,sizeof(header),0},size}; mb_iostore_limits_v1_default(&limits);
    if(mb_iostore_read_header_v1(&view,&limits,&h)!=MB_IOSTORE_OK) { CHECK(0); fclose(f); return; }
    CHECK(h.header_parsed && h.table_geometry_valid && h.partition_count==1);
    const unsigned all=size<=65536;
    const uint32_t chunks=all || h.chunk_count<=3 ? h.chunk_count : 3;
    const uint32_t blocks=all || h.block_count<=3 ? h.block_count : 3;
    uint32_t checked_chunks=chunks;
    uint64_t stored=0,maximum_chunk=0;
    mb_iostore_chunk_result_v1 chosen={0};
    for(uint32_t i=0;i<chunks;++i) {
        const uint32_t index=selected_index(h.chunk_count,i,all);
        mb_iostore_span_v1 ids={id,sizeof(id),h.chunk_ids.offset+(uint64_t)index*12};
        mb_iostore_span_v1 lengths={logical,sizeof(logical),h.chunk_offset_lengths.offset+(uint64_t)index*10};
        CHECK(capture(f,ids.offset,id,sizeof(id)) && capture(f,lengths.offset,logical,sizeof(logical)));
        CHECK(mb_iostore_read_chunk_v1(&view,&limits,index,&ids,&lengths,&c)==MB_IOSTORE_OK && c.descriptor_valid);
        CHECK(!c.logical_block_coverage_verified && !c.paired_ranges_verified && !c.payload_verified && !c.runtime_supported && !c.installation_supported);
        if(c.logical_bytes>maximum_chunk) maximum_chunk=c.logical_bytes;
        if(index==selected) chosen=c;
    }
    if(!chosen.descriptor_valid) {
        mb_iostore_span_v1 ids={id,sizeof(id),h.chunk_ids.offset+(uint64_t)selected*12};
        mb_iostore_span_v1 lengths={logical,sizeof(logical),h.chunk_offset_lengths.offset+(uint64_t)selected*10};
        CHECK(capture(f,ids.offset,id,sizeof(id)) && capture(f,lengths.offset,logical,sizeof(logical)));
        CHECK(mb_iostore_read_chunk_v1(&view,&limits,(uint32_t)selected,&ids,&lengths,&chosen)==MB_IOSTORE_OK && chosen.descriptor_valid);
        ++checked_chunks;
    }
    if(chosen.logical_bytes>maximum_chunk) maximum_chunk=chosen.logical_bytes;
    for(uint32_t i=0;i<blocks;++i) {
        const uint32_t index=selected_index(h.block_count,i,all);
        mb_iostore_span_v1 row={block,sizeof(block),h.compression_blocks.offset+(uint64_t)index*12};
        CHECK(capture(f,row.offset,block,sizeof(block)));
        CHECK(mb_iostore_read_block_v1(&view,&limits,index,&row,&paired,1,&b)==MB_IOSTORE_OK && b.descriptor_valid && b.paired_extent_checked);
        CHECK(!b.payload_verified && !b.logical_block_coverage_verified && !b.full_toc_validated && !b.runtime_supported && !b.installation_supported);
        stored+=b.stored_bytes;
    }
    CHECK(fclose(f)==0);
    printf("{\"fixture\":%u,\"chunks\":%u,\"blocks\":%u,\"seeds\":%u,\"overflow\":%u,\"block_table\":%" PRIu64
           ",\"method_table\":%" PRIu64 ",\"directory\":%" PRIu64 ",\"entry_metadata\":%" PRIu64 ",\"layout_end\":%" PRIu64
           ",\"chunk_rows_checked\":%u,\"block_rows_checked\":%u,\"all_rows_checked\":%u,\"stored_bytes_checked\":%" PRIu64
           ",\"maximum_chunk_bytes_checked\":%" PRIu64 ",\"selected_chunk\":%lu,\"selected_offset\":%" PRIu64
           ",\"selected_bytes\":%" PRIu64 ",\"selected_first_block\":%u,\"selected_last_block\":%u}\n",ordinal,h.chunk_count,h.block_count,h.hash_seed_count,h.overflow_chunk_count,
           h.compression_blocks.offset,h.compression_methods.offset,h.directory_index.offset,h.entry_metadata.offset,h.declared_layout_end,
           checked_chunks,blocks,all,stored,maximum_chunk,selected,chosen.logical_offset,chosen.logical_bytes,chosen.first_block,chosen.last_block);
}

int main(int argc,char **argv) {
    if((argc-1)%3) return 2;
    header_checks(); chunk_checks(); block_checks(); preflight_checks();
    for(int i=1;i<argc;i+=3) actual_fixture((unsigned)(i-1)/3,argv[i],argv[i+1],argv[i+2]);
    printf("iostore-reader groups=%u checks=%u failures=%u full_toc_validated=false payload_verified=false runtime_supported=false installation_supported=false\n",groups,checks,failures);
    return failures ? 1 : 0;
}
