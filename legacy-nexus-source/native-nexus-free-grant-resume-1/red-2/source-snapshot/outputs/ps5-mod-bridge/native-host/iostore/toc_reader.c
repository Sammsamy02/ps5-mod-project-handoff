#include "toc_reader.h"
#include <string.h>

typedef struct address_span { uintptr_t begin, end; } address_span;

static int span_valid(const void *p,size_t n,size_t alignment,address_span *s) {
    const uintptr_t start=(uintptr_t)p;
    if ((n && !p) || (n && start%alignment) || n>UINTPTR_MAX-start) return 0;
    s->begin=start; s->end=start+n; return 1;
}
static int overlaps(address_span a,address_span b) {
    return a.begin!=a.end && b.begin!=b.end && a.begin<b.end && b.begin<a.end;
}
static int input_disjoint(const void *p,size_t n,size_t alignment,address_span output) {
    address_span input;
    return span_valid(p,n,alignment,&input) && !overlaps(input,output);
}
static int limits_valid(const mb_iostore_limits_v1 *l) {
    return l->struct_size>=sizeof(*l) && l->abi_version==1 && !l->reserved &&
        l->max_chunks<=MB_IOSTORE_MAX_CHUNKS && l->max_blocks<=MB_IOSTORE_MAX_BLOCKS &&
        l->max_hash_seeds<=MB_IOSTORE_MAX_CHUNKS && l->max_overflow_chunks<=MB_IOSTORE_MAX_CHUNKS &&
        l->max_partitions<=MB_IOSTORE_MAX_PARTITIONS && l->max_methods<=MB_IOSTORE_MAX_METHODS &&
        l->max_method_name_bytes<=MB_IOSTORE_MAX_METHOD_NAME_BYTES && l->max_toc_bytes<=MB_IOSTORE_MAX_TOC_BYTES;
}
static int preflight(const mb_iostore_view_v1 *v,const mb_iostore_limits_v1 *l,
    const mb_iostore_span_v1 *a,const mb_iostore_span_v1 *b,
    const uint64_t *partitions,uint32_t partition_count,void *r,size_t size,size_t alignment) {
    address_span output;
    if (!r || !v || !l || !span_valid(r,size,alignment,&output) ||
        !input_disjoint(v,sizeof(*v),_Alignof(mb_iostore_view_v1),output) ||
        !input_disjoint(l,sizeof(*l),_Alignof(mb_iostore_limits_v1),output)) return 0;
    if (v->struct_size<sizeof(*v) || v->abi_version!=1 || v->header.offset || !limits_valid(l) ||
        !input_disjoint(v->header.data,v->header.bytes,1,output)) return 0;
    const mb_iostore_span_v1 *rows[2]={a,b};
    for (unsigned i=0;i<2;++i) if (rows[i] &&
        (!input_disjoint(rows[i],sizeof(*rows[i]),_Alignof(mb_iostore_span_v1),output) ||
         !input_disjoint(rows[i]->data,rows[i]->bytes,1,output))) return 0;
    return partition_count<=MB_IOSTORE_MAX_PARTITIONS &&
        input_disjoint(partitions,(size_t)partition_count*sizeof(*partitions),_Alignof(uint64_t),output);
}
static uint64_t little(const uint8_t *p,unsigned bytes) {
    uint64_t n=0; for(unsigned i=0;i<bytes;++i) n|=(uint64_t)p[i]<<(8*i); return n;
}
static uint64_t big40(const uint8_t *p) {
    uint64_t n=0; for(unsigned i=0;i<5;++i) n=(n<<8)|p[i]; return n;
}

void mb_iostore_limits_v1_default(mb_iostore_limits_v1 *l) {
    if (!l) return;
    *l=(mb_iostore_limits_v1){sizeof(*l),1,MB_IOSTORE_MAX_CHUNKS,MB_IOSTORE_MAX_BLOCKS,
        MB_IOSTORE_MAX_CHUNKS,MB_IOSTORE_MAX_CHUNKS,MB_IOSTORE_MAX_PARTITIONS,MB_IOSTORE_MAX_METHODS,
        MB_IOSTORE_MAX_METHOD_NAME_BYTES,0,MB_IOSTORE_MAX_TOC_BYTES};
}
static mb_iostore_status header_fail(mb_iostore_header_result_v1 *r,mb_iostore_status s,mb_iostore_reason why) {
    memset(r,0,sizeof(*r)); r->struct_size=sizeof(*r); r->abi_version=1; r->status=s; r->reason=why; return s;
}
static mb_iostore_status chunk_fail(mb_iostore_chunk_result_v1 *r,mb_iostore_status s,mb_iostore_reason why) {
    memset(r,0,sizeof(*r)); r->struct_size=sizeof(*r); r->abi_version=1; r->status=s; r->reason=why; return s;
}
static mb_iostore_status block_fail(mb_iostore_block_result_v1 *r,mb_iostore_status s,mb_iostore_reason why) {
    memset(r,0,sizeof(*r)); r->struct_size=sizeof(*r); r->abi_version=1; r->status=s; r->reason=why; return s;
}
static int next_range(uint64_t *position,uint32_t count,uint32_t width,uint64_t total,mb_iostore_range_v1 *r) {
    const uint64_t bytes=(uint64_t)count*width;
    if (*position>total || bytes>total-*position) return 0;
    *r=(mb_iostore_range_v1){*position,bytes}; *position+=bytes; return 1;
}

static mb_iostore_status parse_header(const mb_iostore_view_v1 *v,const mb_iostore_limits_v1 *l,
    mb_iostore_header_result_v1 *r) {
    static const uint8_t magic[]="-==--==--==--==-";
    header_fail(r,MB_IOSTORE_OK,MB_IOSTORE_REASON_NONE);
    if (v->header.bytes<144) return header_fail(r,MB_IOSTORE_TRUNCATED,MB_IOSTORE_REASON_CAPTURE);
    const uint8_t *p=v->header.data;
    if (memcmp(p,magic,16)) return header_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_MAGIC);
    if (p[16]!=8) return header_fail(r,MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_VERSION);
    if (little(p+20,4)!=144) return header_fail(r,MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_HEADER_WIDTH);
    if (p[80]&~9u) return header_fail(r,MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_FLAGS);
    r->version=8; r->container_flags_raw=p[80];
    r->chunk_count=(uint32_t)little(p+24,4); r->block_count=(uint32_t)little(p+28,4);
    r->block_entry_bytes=(uint32_t)little(p+32,4);
    r->method_name_count=(uint32_t)little(p+36,4); r->method_name_bytes=(uint32_t)little(p+40,4);
    r->compression_block_bytes=(uint32_t)little(p+44,4); r->partition_count=(uint32_t)little(p+52,4);
    r->container_id=little(p+56,8); memcpy(r->key_guid_raw,p+64,16);
    r->hash_seed_count=(uint32_t)little(p+84,4); r->partition_bytes=little(p+88,8);
    r->overflow_chunk_count=(uint32_t)little(p+96,4); r->metadata_entry_bytes=24;
    const uint32_t directory_bytes=(uint32_t)little(p+48,4);
    if (r->block_entry_bytes!=12) return header_fail(r,MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_TABLE_WIDTH);
    if (r->method_name_count && !r->method_name_bytes) return header_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_TABLE_WIDTH);
    if (r->chunk_count>l->max_chunks || r->block_count>l->max_blocks || r->hash_seed_count>l->max_hash_seeds ||
        r->overflow_chunk_count>l->max_overflow_chunks || r->partition_count>l->max_partitions ||
        r->method_name_count>l->max_methods || r->method_name_bytes>l->max_method_name_bytes ||
        v->toc_logical_bytes>l->max_toc_bytes) return header_fail(r,MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
    if (!r->partition_count || !r->partition_bytes || (r->partition_bytes==UINT64_MAX && r->partition_count!=1))
        return header_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_PARTITION);
    if (!r->compression_block_bytes) return header_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_BLOCK_SIZE);
    if (!(p[80]&8u) && directory_bytes) return header_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_DIRECTORY_FLAG);
    uint64_t at=144;
    if (!next_range(&at,r->chunk_count,12,v->toc_logical_bytes,&r->chunk_ids) ||
        !next_range(&at,r->chunk_count,10,v->toc_logical_bytes,&r->chunk_offset_lengths) ||
        !next_range(&at,r->hash_seed_count,4,v->toc_logical_bytes,&r->hash_seeds) ||
        !next_range(&at,r->overflow_chunk_count,4,v->toc_logical_bytes,&r->overflow_chunks) ||
        !next_range(&at,r->block_count,12,v->toc_logical_bytes,&r->compression_blocks) ||
        !next_range(&at,r->method_name_count,r->method_name_bytes,v->toc_logical_bytes,&r->compression_methods) ||
        !next_range(&at,directory_bytes,1,v->toc_logical_bytes,&r->directory_index) ||
        !next_range(&at,r->chunk_count,24,v->toc_logical_bytes,&r->entry_metadata))
        return header_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_TOC_RANGE);
    r->logical_block_capacity=(uint64_t)r->block_count*r->compression_block_bytes;
    r->declared_layout_end=at; r->header_parsed=1; r->table_geometry_valid=1;
    return MB_IOSTORE_OK;
}

mb_iostore_status mb_iostore_read_header_v1(const mb_iostore_view_v1 *v,
    const mb_iostore_limits_v1 *l,mb_iostore_header_result_v1 *r) {
    if (!preflight(v,l,NULL,NULL,NULL,0,r,sizeof(*r),_Alignof(mb_iostore_header_result_v1))) return MB_IOSTORE_BAD_ARGUMENT;
    return parse_header(v,l,r);
}
static mb_iostore_status row_binding(const mb_iostore_span_v1 *row,uint64_t offset,size_t width) {
    if (row->bytes<width) return MB_IOSTORE_TRUNCATED;
    if (row->bytes!=width || row->offset!=offset) return MB_IOSTORE_MALFORMED;
    return MB_IOSTORE_OK;
}
mb_iostore_status mb_iostore_read_chunk_v1(const mb_iostore_view_v1 *v,const mb_iostore_limits_v1 *l,
    uint32_t index,const mb_iostore_span_v1 *id,const mb_iostore_span_v1 *logical,
    mb_iostore_chunk_result_v1 *r) {
    if (!id || !logical || !preflight(v,l,id,logical,NULL,0,r,sizeof(*r),_Alignof(mb_iostore_chunk_result_v1))) return MB_IOSTORE_BAD_ARGUMENT;
    chunk_fail(r,MB_IOSTORE_OK,MB_IOSTORE_REASON_NONE);
    mb_iostore_header_result_v1 h; mb_iostore_status s=parse_header(v,l,&h);
    if (s!=MB_IOSTORE_OK) return chunk_fail(r,s,(mb_iostore_reason)h.reason);
    if (index>=h.chunk_count) return chunk_fail(r,MB_IOSTORE_OUT_OF_RANGE,MB_IOSTORE_REASON_INDEX);
    s=row_binding(id,h.chunk_ids.offset+(uint64_t)index*12,12);
    if (s==MB_IOSTORE_OK) s=row_binding(logical,h.chunk_offset_lengths.offset+(uint64_t)index*10,10);
    if (s!=MB_IOSTORE_OK) return chunk_fail(r,s,s==MB_IOSTORE_TRUNCATED ? MB_IOSTORE_REASON_CAPTURE : MB_IOSTORE_REASON_SPAN_BINDING);
    const uint64_t offset=big40(logical->data),bytes=big40(logical->data+5);
    if (offset>h.logical_block_capacity || bytes>h.logical_block_capacity-offset)
        return chunk_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_CHUNK_RANGE);
    r->logical_offset=offset; r->logical_bytes=bytes; r->logical_end=offset+bytes;
    if (bytes) {
        r->first_block=(uint32_t)(offset/h.compression_block_bytes);
        r->last_block=(uint32_t)((r->logical_end-1)/h.compression_block_bytes);
        r->block_count=r->last_block-r->first_block+1;
    }
    memcpy(r->chunk_id_raw,id->data,12); r->chunk_id=little(id->data,8);
    r->chunk_index=((uint32_t)id->data[8]<<8)|id->data[9]; r->padding_raw=id->data[10]; r->chunk_type_raw=id->data[11];
    r->toc_index=index; r->descriptor_valid=1; return MB_IOSTORE_OK;
}
mb_iostore_status mb_iostore_read_block_v1(const mb_iostore_view_v1 *v,const mb_iostore_limits_v1 *l,
    uint32_t index,const mb_iostore_span_v1 *row,const uint64_t *paired,uint32_t paired_count,
    mb_iostore_block_result_v1 *r) {
    if (!row || !preflight(v,l,row,NULL,paired,paired_count,r,sizeof(*r),_Alignof(mb_iostore_block_result_v1))) return MB_IOSTORE_BAD_ARGUMENT;
    block_fail(r,MB_IOSTORE_OK,MB_IOSTORE_REASON_NONE);
    mb_iostore_header_result_v1 h; mb_iostore_status s=parse_header(v,l,&h);
    if (s!=MB_IOSTORE_OK) return block_fail(r,s,(mb_iostore_reason)h.reason);
    if (paired_count!=h.partition_count) return block_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_PARTITION);
    if (index>=h.block_count) return block_fail(r,MB_IOSTORE_OUT_OF_RANGE,MB_IOSTORE_REASON_INDEX);
    s=row_binding(row,h.compression_blocks.offset+(uint64_t)index*12,12);
    if (s!=MB_IOSTORE_OK) return block_fail(r,s,s==MB_IOSTORE_TRUNCATED ? MB_IOSTORE_REASON_CAPTURE : MB_IOSTORE_REASON_SPAN_BINDING);
    const uint64_t offset=little(row->data,5);
    const uint32_t stored=(uint32_t)little(row->data+5,3),decoded=(uint32_t)little(row->data+8,3),method=row->data[11];
    if (method>h.method_name_count) return block_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_METHOD);
    if (!stored || !decoded || decoded>h.compression_block_bytes) return block_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_BLOCK_SIZE);
    if (!method && stored!=decoded) return block_fail(r,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_NONE_SIZE);
    const uint64_t partition=offset/h.partition_bytes,local=offset%h.partition_bytes;
    if (partition>=h.partition_count || stored>h.partition_bytes-local ||
        local>paired[partition] || stored>paired[partition]-local)
        return block_fail(r,MB_IOSTORE_OUT_OF_RANGE,MB_IOSTORE_REASON_UCAS_RANGE);
    r->toc_index=index; r->partition_index=(uint32_t)partition; r->method_index=method;
    r->stored_bytes=stored; r->decoded_bytes=decoded; r->built_in_none=method==0;
    r->physical_offset=offset; r->partition_offset=local; r->partition_end=local+stored;
    r->descriptor_valid=1; r->paired_extent_checked=1; return MB_IOSTORE_OK;
}
