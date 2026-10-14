#include "none_range_reader.h"
#include <string.h>

typedef struct region {uintptr_t start,end;} region;
static int address(const void *p,size_t n,size_t alignment,region *r) {
 uintptr_t s=(uintptr_t)p;
 if((n&&!p)||(p&&s%alignment)||n>UINTPTR_MAX-s) return 0;
 *r=(region){s,s+n}; return 1;
}
static int overlaps(region a,region b) {
 return a.start<a.end && b.start<b.end && a.start<b.end && b.start<a.end;
}
static int zero(const uint8_t *p,size_t n) {
 for(size_t i=0;i<n;i++) if(p[i]) return 0;
 return 1;
}
static int apart(const region *control,size_t nc,const region *inputs,size_t ni) {
 for(size_t i=0;i<nc;i++) for(size_t j=i+1;j<nc;j++)
  if(overlaps(control[i],control[j])) return 0;
 for(size_t i=0;i<ni;i++) for(size_t j=1;j<nc;j++)
  if(overlaps(inputs[i],control[j])) return 0;
 return 1;
}
static mb_none_range_status_v1 publish(mb_none_range_result_v1 *r,
 mb_none_range_status_v1 s,mb_iostore_status inner,uint32_t why) {
 r->status=(uint32_t)s;r->inner_status=(uint32_t)inner;r->inner_reason=why;return s;
}
static void begin(mb_none_range_result_v1 *r,uint32_t row) {
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;r->toc_row=row;
 r->failed_partition=UINT32_MAX;r->failed_block=UINT32_MAX;
}
static int source_preflight(const mb_none_range_source_v1 *s,
 mb_none_range_plan_v1 *p,mb_none_range_result_v1 *r) {
 region controls[3],inputs[2];
 if(!s||!p||!r || !address(s,sizeof(*s),_Alignof(mb_none_range_source_v1),controls) ||
  !address(p,sizeof(*p),_Alignof(mb_none_range_plan_v1),controls+1) ||
  !address(r,sizeof(*r),_Alignof(mb_none_range_result_v1),controls+2)) return 0;
 if(s->struct_size!=sizeof(*s)||s->abi_version!=1||s->reserved||s->toc.offset||
  s->toc.bytes>MB_IOSTORE_MAX_TOC_BYTES||s->partition_count<1||s->partition_count>2||!s->read ||
  !address(s->toc.data,s->toc.bytes,1,inputs)||
  !address(s->partition_lengths,s->partition_count*sizeof(uint64_t),_Alignof(uint64_t),inputs+1)) return 0;
 return apart(controls,3,inputs,2);
}
static int range_preflight(const mb_none_range_plan_v1 *p,uint8_t *out,
 size_t cap,mb_none_range_result_v1 *r) {
 region controls[3],input;
 if(!p||!r||!address(p,sizeof(*p),_Alignof(mb_none_range_plan_v1),controls)||
  !address(r,sizeof(*r),_Alignof(mb_none_range_result_v1),controls+1)||
  !address(out,cap,1,controls+2)) return 0;
 if(p->struct_size!=sizeof(*p)||p->abi_version!=1||p->reserved||p->toc.offset||
  p->toc.bytes>MB_IOSTORE_MAX_TOC_BYTES||p->partition_count<1||p->partition_count>2||!p->read||
  cap>MB_NONE_RANGE_MAX_BYTES||!address(p->toc.data,p->toc.bytes,1,&input)) return 0;
 return apart(controls,3,&input,1);
}
static mb_iostore_span_v1 row_span(mb_iostore_span_v1 toc,uint64_t off,size_t n) {
 return (mb_iostore_span_v1){toc.data+(size_t)off,n,off};
}
static mb_iostore_view_v1 view_for(mb_iostore_span_v1 toc) {
 return (mb_iostore_view_v1){sizeof(mb_iostore_view_v1),1,
  {toc.data,toc.bytes<144?toc.bytes:144,0},toc.bytes};
}
/* All addressable spans come from validated constant-size TOC geometry. */
static mb_none_range_status_v1 decode(mb_iostore_span_v1 toc,uint32_t row,uint32_t parts,
 mb_iostore_header_result_v1 *h,mb_iostore_chunk_result_v1 *chunk,mb_none_range_result_v1 *r) {
 mb_iostore_view_v1 view=view_for(toc);mb_iostore_limits_v1 limits;
 mb_iostore_limits_v1_default(&limits);
 mb_iostore_status inner=mb_iostore_read_header_v1(&view,&limits,h);
 if(inner!=MB_IOSTORE_OK) return publish(r,inner==MB_IOSTORE_UNSUPPORTED?
  MB_NONE_RANGE_UNSUPPORTED:MB_NONE_RANGE_METADATA_REJECTED,inner,h->reason);
 const uint8_t *raw=toc.data;
 if(h->container_flags_raw!=8||h->compression_block_bytes!=65536||h->method_name_count||
  h->method_name_bytes!=32||h->partition_count!=parts||!zero(h->key_guid_raw,16)||
  !zero(raw+17,3)||!zero(raw+81,3)||!zero(raw+100,44)||h->declared_layout_end!=toc.bytes||
  (parts==1&&h->partition_bytes!=UINT64_MAX)||(parts==2&&h->partition_bytes==UINT64_MAX))
  return publish(r,MB_NONE_RANGE_UNSUPPORTED,MB_IOSTORE_UNSUPPORTED,0);
 if(row>=h->chunk_count) return publish(r,MB_NONE_RANGE_METADATA_REJECTED,MB_IOSTORE_OUT_OF_RANGE,MB_IOSTORE_REASON_INDEX);
 mb_iostore_span_v1 id=row_span(toc,h->chunk_ids.offset+(uint64_t)row*12,12);
 mb_iostore_span_v1 logical=row_span(toc,h->chunk_offset_lengths.offset+(uint64_t)row*10,10);
 inner=mb_iostore_read_chunk_v1(&view,&limits,row,&id,&logical,chunk);
 if(inner!=MB_IOSTORE_OK) return publish(r,MB_NONE_RANGE_METADATA_REJECTED,inner,chunk->reason);
 const uint8_t *meta=raw+(size_t)(h->entry_metadata.offset+(uint64_t)row*24);
 if(meta[20]||!zero(meta+21,3)||chunk->chunk_type_raw==4)
  return publish(r,MB_NONE_RANGE_UNSUPPORTED,MB_IOSTORE_UNSUPPORTED,0);
 if(!chunk->logical_bytes) return publish(r,MB_NONE_RANGE_METADATA_REJECTED,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_CHUNK_RANGE);
 if(chunk->logical_bytes>MB_NONE_RANGE_MAX_CHUNK_BYTES||chunk->block_count>MB_NONE_RANGE_MAX_CHUNK_BLOCKS)
  return publish(r,MB_NONE_RANGE_RESOURCE_LIMIT,MB_IOSTORE_RESOURCE_LIMIT,MB_IOSTORE_REASON_COUNT_LIMIT);
 r->chunk_bytes=chunk->logical_bytes;return MB_NONE_RANGE_OK;
}
static mb_none_range_status_v1 read_block(mb_iostore_span_v1 toc,uint64_t table,uint32_t index,
 const uint64_t *lengths,uint32_t parts,mb_iostore_block_result_v1 *block,mb_none_range_result_v1 *r) {
 mb_iostore_view_v1 view=view_for(toc);mb_iostore_limits_v1 limits;
 mb_iostore_limits_v1_default(&limits);
 mb_iostore_span_v1 span=row_span(toc,table+(uint64_t)index*12,12);
 mb_iostore_status inner=mb_iostore_read_block_v1(&view,&limits,index,&span,lengths,parts,block);
 if(inner!=MB_IOSTORE_OK) return publish(r,MB_NONE_RANGE_METADATA_REJECTED,inner,block->reason);
 if(!block->built_in_none) return publish(r,MB_NONE_RANGE_UNSUPPORTED,MB_IOSTORE_UNSUPPORTED,MB_IOSTORE_REASON_METHOD);
 r->selected_blocks_checked++;return MB_NONE_RANGE_OK;
}
mb_none_range_status_v1 mb_iostore_none_plan_v1(const mb_none_range_source_v1 *s,
 uint32_t row,mb_none_range_plan_v1 *plan,mb_none_range_result_v1 *r) {
 if(!source_preflight(s,plan,r)) return MB_NONE_RANGE_BAD_ARGUMENT;
 begin(r,row);mb_iostore_header_result_v1 h;mb_iostore_chunk_result_v1 chunk;
 mb_none_range_status_v1 status=decode(s->toc,row,s->partition_count,&h,&chunk,r);
 if(status!=MB_NONE_RANGE_OK) return status;
 uint64_t remaining=chunk.logical_bytes;
 for(uint32_t i=chunk.first_block;i<=chunk.last_block;i++) {
  mb_iostore_block_result_v1 b;
  status=read_block(s->toc,h.compression_blocks.offset,i,s->partition_lengths,s->partition_count,&b,r);
  if(status!=MB_NONE_RANGE_OK) return status;
  uint64_t skip=i==chunk.first_block?chunk.logical_offset%65536:0;
  uint64_t needed=65536-skip;if(needed>remaining) needed=remaining;
  if(skip>=b.decoded_bytes||needed>b.decoded_bytes-skip)
   return publish(r,MB_NONE_RANGE_METADATA_REJECTED,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_CHUNK_RANGE);
  remaining-=needed;
 }
 if(remaining) return publish(r,MB_NONE_RANGE_METADATA_REJECTED,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_CHUNK_RANGE);
 mb_none_range_plan_v1 p;memset(&p,0,sizeof(p));p.struct_size=sizeof(p);p.abi_version=1;
 p.toc=s->toc;p.partition_count=s->partition_count;p.toc_row=row;p.read=s->read;p.read_context=s->read_context;
 for(uint32_t i=0;i<s->partition_count;i++) p.partition_lengths[i]=s->partition_lengths[i];
 p.logical_offset=chunk.logical_offset;p.logical_bytes=chunk.logical_bytes;
 p.blocks_offset=h.compression_blocks.offset;p.partition_bytes=h.partition_bytes;
 p.first_block=chunk.first_block;p.last_block=chunk.last_block;p.block_count=chunk.block_count;p.total_blocks=h.block_count;
 memcpy(p.chunk_id,chunk.chunk_id_raw,12);*plan=p;r->full_chunk_coverage_scanned=1;
 return publish(r,MB_NONE_RANGE_OK,MB_IOSTORE_OK,0);
}
mb_none_range_status_v1 mb_iostore_none_range_v1(const mb_none_range_plan_v1 *p,
 uint64_t offset,uint32_t requested,uint8_t *out,size_t cap,mb_none_range_result_v1 *r) {
 if(!range_preflight(p,out,cap,r)) return MB_NONE_RANGE_BAD_ARGUMENT;
 begin(r,p->toc_row);r->range_offset=offset;r->range_bytes=requested;
 if(requested>MB_NONE_RANGE_MAX_BYTES) return publish(r,MB_NONE_RANGE_RESOURCE_LIMIT,MB_IOSTORE_RESOURCE_LIMIT,0);
 mb_iostore_header_result_v1 h;mb_iostore_chunk_result_v1 chunk;
 mb_none_range_status_v1 status=decode(p->toc,p->toc_row,p->partition_count,&h,&chunk,r);
 if(status!=MB_NONE_RANGE_OK) return status;
 if(p->logical_offset!=chunk.logical_offset||p->logical_bytes!=chunk.logical_bytes||
  p->first_block!=chunk.first_block||p->last_block!=chunk.last_block||p->block_count!=chunk.block_count||
  p->blocks_offset!=h.compression_blocks.offset||p->partition_bytes!=h.partition_bytes||
  p->total_blocks!=h.block_count||memcmp(p->chunk_id,chunk.chunk_id_raw,12))
  return publish(r,MB_NONE_RANGE_PLAN_MISMATCH,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_SPAN_BINDING);
 r->plan_geometry_rebound=1;
 if(offset>chunk.logical_bytes||requested>chunk.logical_bytes-offset)
  return publish(r,MB_NONE_RANGE_OUT_OF_RANGE,MB_IOSTORE_OUT_OF_RANGE,MB_IOSTORE_REASON_CHUNK_RANGE);
 if(requested>cap) return publish(r,MB_NONE_RANGE_BUFFER_TOO_SMALL,MB_IOSTORE_OK,0);
 if(!requested) {r->range_complete=1;return publish(r,MB_NONE_RANGE_OK,MB_IOSTORE_OK,0);}
 uint64_t logical=chunk.logical_offset+offset;
 uint32_t first=(uint32_t)(logical/65536),last=(uint32_t)((logical+requested-1)/65536);
 if(first<chunk.first_block||last>chunk.last_block||last-first>1)
  return publish(r,MB_NONE_RANGE_METADATA_REJECTED,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_CHUNK_RANGE);
 mb_iostore_block_result_v1 blocks[2];size_t skips[2],wanted[2],remaining=requested;
 uint32_t count=last-first+1;
 for(uint32_t k=0;k<count;k++) {
  status=read_block(p->toc,h.compression_blocks.offset,first+k,p->partition_lengths,p->partition_count,blocks+k,r);
  if(status!=MB_NONE_RANGE_OK) return status;
  skips[k]=k==0?(size_t)(logical%65536):0;
  wanted[k]=65536-skips[k];if(wanted[k]>remaining) wanted[k]=remaining;
  if(skips[k]>=blocks[k].decoded_bytes||wanted[k]>blocks[k].decoded_bytes-skips[k])
   return publish(r,MB_NONE_RANGE_METADATA_REJECTED,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_CHUNK_RANGE);
  remaining-=wanted[k];
 }
 if(remaining) return publish(r,MB_NONE_RANGE_METADATA_REJECTED,MB_IOSTORE_MALFORMED,MB_IOSTORE_REASON_CHUNK_RANGE);
 size_t done=0;
 for(uint32_t k=0;k<count;k++) {
  size_t received=0;r->read_calls++;
  int code=p->read(p->read_context,blocks[k].partition_index,blocks[k].partition_offset+skips[k],out+done,wanted[k],&received);
  if(code||received!=wanted[k]) {
   r->failed_partition=blocks[k].partition_index;r->failed_block=first+k;
   r->failed_physical_offset=blocks[k].partition_offset+skips[k];r->failed_received=received;r->provider_code=code;
   memset(out,0,requested);
   return publish(r,code?MB_NONE_RANGE_READ_ERROR:MB_NONE_RANGE_SHORT_READ,MB_IOSTORE_OK,0);
  }
  r->read_bytes+=received;done+=received;
 }
 r->range_complete=1;return publish(r,MB_NONE_RANGE_OK,MB_IOSTORE_OK,0);
}
