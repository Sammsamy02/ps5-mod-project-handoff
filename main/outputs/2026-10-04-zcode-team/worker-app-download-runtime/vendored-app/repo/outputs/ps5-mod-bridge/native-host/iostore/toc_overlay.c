#include "toc_overlay.h"
#include <string.h>

#define MAX_U40 UINT64_C(1099511627775)
typedef struct region { uintptr_t begin,end; } region;
typedef struct parsed_toc {
 const uint8_t *bytes;
 mb_iostore_view_v1 view;
 mb_iostore_header_result_v1 h;
 uint64_t physical_end,aligned_end;
} parsed_toc;

static int address(const void *p,size_t n,size_t alignment,region *r) {
 uintptr_t begin=(uintptr_t)p;
 if((n&&!p)||(p&&begin%alignment)||n>UINTPTR_MAX-begin)return 0;
 *r=(region){begin,begin+n};return 1;
}
static int overlap(region a,region b) {
 return a.begin<a.end&&b.begin<b.end&&a.begin<b.end&&b.begin<a.end;
}
static int preflight(const mb_toc_overlay_params_v1 *p,
 const mb_toc_overlay_workspace_v1 *w,uint8_t *out,size_t cap,
 mb_toc_overlay_result_v1 *r) {
 region writable[7],inputs[3];
 if(!p||!w||!r||
  !address(p,sizeof(*p),_Alignof(mb_toc_overlay_params_v1),&writable[0])||
  !address(w,sizeof(*w),_Alignof(mb_toc_overlay_workspace_v1),&writable[1])||
  !address(r,sizeof(*r),_Alignof(mb_toc_overlay_result_v1),&writable[2])||
  !address(out,cap,1,&writable[3]))return 0;
 if(p->struct_size!=sizeof(*p)||p->abi_version!=1||
  w->struct_size!=sizeof(*w)||w->abi_version!=1||
  p->base_toc.offset||p->small_toc.offset||p->merged_directory.offset||
  w->base_capacity>MB_IOSTORE_MAX_CHUNKS||w->small_capacity>MB_IOSTORE_MAX_CHUNKS||
  w->scratch_capacity>(size_t)MB_IOSTORE_MAX_BLOCKS+1||
  cap>MB_IOSTORE_MAX_TOC_BYTES||
  !address(w->base_rows,(size_t)w->base_capacity*sizeof(*w->base_rows),
    _Alignof(mb_toc_overlay_id_row_v1),&writable[4])||
  !address(w->small_rows,(size_t)w->small_capacity*sizeof(*w->small_rows),
    _Alignof(mb_toc_overlay_id_row_v1),&writable[5])||
  !address(w->scratch,w->scratch_capacity*sizeof(*w->scratch),_Alignof(uint32_t),&writable[6])||
  !address(p->base_toc.data,p->base_toc.bytes,1,&inputs[0])||
  !address(p->small_toc.data,p->small_toc.bytes,1,&inputs[1])||
  !address(p->merged_directory.data,p->merged_directory.bytes,1,&inputs[2]))return 0;
 for(unsigned i=0;i<7;++i)for(unsigned j=i+1;j<7;++j)
  if(overlap(writable[i],writable[j]))return 0;
 for(unsigned i=0;i<3;++i)for(unsigned j=0;j<7;++j)
  if(overlap(inputs[i],writable[j]))return 0;
 return 1;
}
static uint64_t le(const uint8_t *p,unsigned n) {
 uint64_t v=0;for(unsigned i=0;i<n;++i)v|=(uint64_t)p[i]<<(8*i);return v;
}
static uint64_t be40(const uint8_t *p) {
 uint64_t v=0;for(unsigned i=0;i<5;++i)v=(v<<8)|p[i];return v;
}
static void put_le(uint8_t *p,uint64_t v,unsigned n) {
 for(unsigned i=0;i<n;++i){p[i]=(uint8_t)v;v>>=8;}
}
static void put_be40(uint8_t *p,uint64_t v) {
 for(unsigned i=0;i<5;++i){p[4-i]=(uint8_t)v;v>>=8;}
}
static int zero(const uint8_t *p,size_t n) {
 for(size_t i=0;i<n;++i)if(p[i])return 0;
 return 1;
}
static mb_toc_overlay_status_v1 refused(mb_toc_overlay_result_v1 *r,
 mb_toc_overlay_status_v1 s,mb_toc_overlay_reason_v1 why,uint32_t source,
 uint32_t inner,uint32_t inner_reason) {
 r->status=s;r->reason=why;r->source_index=source;
 r->inner_status=inner;r->inner_reason=inner_reason;return s;
}
static void swap_id(mb_toc_overlay_id_row_v1 *a,mb_toc_overlay_id_row_v1 *b) {
 mb_toc_overlay_id_row_v1 tmp=*a;*a=*b;*b=tmp;
}
static void sift_id(mb_toc_overlay_id_row_v1 *a,size_t root,size_t count) {
 while(root<count/2){
  size_t child=root*2+1;
  if(child+1<count&&memcmp(a[child].id,a[child+1].id,12)<0)++child;
  if(memcmp(a[root].id,a[child].id,12)>=0)return;
  swap_id(a+root,a+child);root=child;
 }
}
static void sort_ids(mb_toc_overlay_id_row_v1 *a,size_t n) {
 for(size_t i=n/2;i>0;--i)sift_id(a,i-1,n);
 for(size_t end=n;end>1;--end){swap_id(a,a+end-1);sift_id(a,0,end-1);}
}
static void sift_u32(uint32_t *a,size_t root,size_t n) {
 while(root<n/2){
  size_t child=root*2+1;
  if(child+1<n&&a[child]<a[child+1])++child;
  if(a[root]>=a[child])return;
  uint32_t tmp=a[root];a[root]=a[child];a[child]=tmp;root=child;
 }
}
static void sort_u32(uint32_t *a,size_t n) {
 for(size_t i=n/2;i>0;--i)sift_u32(a,i-1,n);
 for(size_t end=n;end>1;--end){
  uint32_t tmp=a[0];a[0]=a[end-1];a[end-1]=tmp;sift_u32(a,0,end-1);
 }
}
static const mb_toc_overlay_id_row_v1 *find_id(
 const mb_toc_overlay_id_row_v1 *a,size_t n,const uint8_t id[12]) {
 size_t lo=0,hi=n;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;int cmp=memcmp(a[mid].id,id,12);
  if(cmp<0)lo=mid+1;else if(cmp>0)hi=mid;else return a+mid;
 }
 return NULL;
}
static const uint8_t *id_at(const parsed_toc *t,uint32_t row) {
 return t->bytes+(size_t)t->h.chunk_ids.offset+(size_t)row*12;
}
static const uint8_t *logical_at(const parsed_toc *t,uint32_t row) {
 return t->bytes+(size_t)t->h.chunk_offset_lengths.offset+(size_t)row*10;
}
static const uint8_t *meta_at(const parsed_toc *t,uint32_t row) {
 return t->bytes+(size_t)t->h.entry_metadata.offset+(size_t)row*24;
}
static const uint8_t *block_at(const parsed_toc *t,uint32_t index) {
 return t->bytes+(size_t)t->h.compression_blocks.offset+(size_t)index*12;
}
static mb_toc_overlay_status_v1 parse(const mb_iostore_span_v1 *span,
 const mb_iostore_limits_v1 *limits,parsed_toc *t,mb_toc_overlay_result_v1 *r,
 uint32_t source) {
 t->bytes=span->data;t->physical_end=0;t->aligned_end=0;
 t->view=(mb_iostore_view_v1){sizeof(t->view),1,
  {span->data,span->bytes<144?span->bytes:144,0},span->bytes};
 mb_iostore_status s=mb_iostore_read_header_v1(&t->view,limits,&t->h);
 if(s!=MB_IOSTORE_OK)return refused(r,s==MB_IOSTORE_RESOURCE_LIMIT?
  MB_TOC_OVERLAY_RESOURCE_LIMIT:(s==MB_IOSTORE_UNSUPPORTED?
  MB_TOC_OVERLAY_UNSUPPORTED:MB_TOC_OVERLAY_METADATA_REJECTED),
  MB_TOC_OVERLAY_REASON_HEADER,source,s,t->h.reason);
 const uint8_t *b=t->bytes;const mb_iostore_header_result_v1 *h=&t->h;
 if(h->container_flags_raw!=8||!zero(h->key_guid_raw,16)||
  h->method_name_count||h->method_name_bytes!=32||h->compression_block_bytes!=65536||
  !zero(b+17,3)||!zero(b+81,3)||!zero(b+100,44)||
  h->partition_count!=1||h->partition_bytes!=UINT64_MAX)
  return refused(r,MB_TOC_OVERLAY_UNSUPPORTED,MB_TOC_OVERLAY_REASON_POLICY,source,0,0);
 if(!h->chunk_count||!h->block_count||h->hash_seed_count>h->chunk_count||
  h->overflow_chunk_count>h->chunk_count||h->directory_index.bytes>MB_TOC_OVERLAY_MAX_DIRECTORY_BYTES||
  h->declared_layout_end!=span->bytes)
  return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_HEADER,source,0,0);
 return MB_TOC_OVERLAY_OK;
}
static mb_toc_overlay_status_v1 validate(parsed_toc *t,uint64_t file_bytes,
 mb_toc_overlay_id_row_v1 *rows,uint32_t *scratch,
 const mb_iostore_limits_v1 *limits,mb_toc_overlay_result_v1 *r,uint32_t source) {
 const mb_iostore_header_result_v1 *h=&t->h;
 for(uint32_t i=0;i<h->chunk_count;++i){
  memcpy(rows[i].id,id_at(t,i),12);rows[i].row=i;
  const uint8_t *m=meta_at(t,i);
  if((m[20]!=0&&m[20]!=2)||!zero(m+21,3))
   return refused(r,MB_TOC_OVERLAY_UNSUPPORTED,MB_TOC_OVERLAY_REASON_META,source,0,0);
 }
 sort_ids(rows,h->chunk_count);
 for(uint32_t i=1;i<h->chunk_count;++i)if(!memcmp(rows[i-1].id,rows[i].id,12))
  return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_ID,source,0,0);
 for(uint32_t i=0;i<h->overflow_chunk_count;++i){
  scratch[i]=(uint32_t)le(t->bytes+(size_t)h->overflow_chunks.offset+(size_t)i*4,4);
  if(scratch[i]>=h->chunk_count)
   return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_OVERFLOW,source,0,0);
 }
 sort_u32(scratch,h->overflow_chunk_count);
 for(uint32_t i=1;i<h->overflow_chunk_count;++i)if(scratch[i-1]==scratch[i])
  return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_OVERFLOW,source,0,0);
 scratch[0]=0;
 for(uint32_t i=0;i<h->block_count;++i){
  uint64_t offset=h->compression_blocks.offset+(uint64_t)i*12;
  mb_iostore_span_v1 span={block_at(t,i),12,offset};mb_iostore_block_result_v1 block;
  mb_iostore_status s=mb_iostore_read_block_v1(&t->view,limits,i,&span,&file_bytes,1,&block);
  if(s!=MB_IOSTORE_OK)return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,
   MB_TOC_OVERLAY_REASON_BLOCK,source,s,block.reason);
  if(!block.built_in_none)return refused(r,MB_TOC_OVERLAY_UNSUPPORTED,
   MB_TOC_OVERLAY_REASON_BLOCK,source,s,MB_IOSTORE_REASON_METHOD);
  uint64_t aligned=block.physical_offset+((block.stored_bytes+UINT64_C(15))&~UINT64_C(15));
  if(aligned>file_bytes)return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,
   MB_TOC_OVERLAY_REASON_PARTITION,source,0,0);
  if(block.partition_end>t->physical_end)t->physical_end=block.partition_end;
  if(aligned>t->aligned_end)t->aligned_end=aligned;
  scratch[i+1]=scratch[i]+(block.decoded_bytes!=h->compression_block_bytes);
 }
 for(uint32_t i=0;i<h->chunk_count;++i){
  mb_iostore_span_v1 id={id_at(t,i),12,h->chunk_ids.offset+(uint64_t)i*12};
  mb_iostore_span_v1 logical={logical_at(t,i),10,h->chunk_offset_lengths.offset+(uint64_t)i*10};
  mb_iostore_chunk_result_v1 c;
  mb_iostore_status s=mb_iostore_read_chunk_v1(&t->view,limits,i,&id,&logical,&c);
  if(s!=MB_IOSTORE_OK)return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,
   MB_TOC_OVERLAY_REASON_CHUNK,source,s,c.reason);
  if(!c.logical_bytes||c.logical_bytes>MAX_U40||c.logical_end>MAX_U40||
   c.logical_offset%h->compression_block_bytes>=le(block_at(t,c.first_block)+8,3)||
   (c.logical_end-1)%h->compression_block_bytes>=le(block_at(t,c.last_block)+8,3)||
   scratch[c.last_block]!=scratch[c.first_block])
   return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_CHUNK,source,0,0);
 }
 return MB_TOC_OVERLAY_OK;
}

mb_toc_overlay_status_v1 mb_iostore_toc_overlay_v1(
 const mb_toc_overlay_params_v1 *p,mb_toc_overlay_workspace_v1 *w,
 uint8_t *output,size_t capacity,mb_toc_overlay_result_v1 *r) {
 if(!preflight(p,w,output,capacity,r))return MB_TOC_OVERLAY_BAD_ARGUMENT;
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;
 if(!p->base_ucas_bytes||p->base_ucas_bytes>MAX_U40||p->base_ucas_bytes%16||
  !p->small_ucas_bytes||p->small_ucas_bytes>MB_TOC_OVERLAY_MAX_SIDECAR_BYTES||
  p->small_ucas_bytes>=p->base_ucas_bytes||!p->merged_directory.bytes||
  p->merged_directory.bytes>MB_TOC_OVERLAY_MAX_DIRECTORY_BYTES)
  return refused(r,MB_TOC_OVERLAY_RESOURCE_LIMIT,MB_TOC_OVERLAY_REASON_PARTITION,0,0,0);
 mb_iostore_limits_v1 limits;mb_iostore_limits_v1_default(&limits);
 parsed_toc base,small;mb_toc_overlay_status_v1 s=parse(&p->base_toc,&limits,&base,r,0);
 if(s!=MB_TOC_OVERLAY_OK)return s;
 s=parse(&p->small_toc,&limits,&small,r,1);if(s!=MB_TOC_OVERLAY_OK)return s;
 if(base.h.container_id!=small.h.container_id)
  return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_HEADER_CHUNK,1,0,0);
 r->required_base_rows=base.h.chunk_count;r->required_small_rows=small.h.chunk_count;
 uint64_t scratch=(uint64_t)(base.h.block_count>small.h.block_count?base.h.block_count:small.h.block_count)+1;
 if(scratch<base.h.overflow_chunk_count)scratch=base.h.overflow_chunk_count;
 if(scratch<small.h.overflow_chunk_count)scratch=small.h.overflow_chunk_count;
 r->required_scratch_u32=scratch;
 if(w->base_capacity<r->required_base_rows||w->small_capacity<r->required_small_rows||w->scratch_capacity<scratch)
  return refused(r,MB_TOC_OVERLAY_WORKSPACE_TOO_SMALL,MB_TOC_OVERLAY_REASON_NONE,0,0,0);
 s=validate(&base,p->base_ucas_bytes,w->base_rows,w->scratch,&limits,r,0);
 if(s!=MB_TOC_OVERLAY_OK)return s;
 s=validate(&small,p->small_ucas_bytes,w->small_rows,w->scratch,&limits,r,1);
 if(s!=MB_TOC_OVERLAY_OK)return s;
 if(p->small_ucas_bytes-small.physical_end>15)
  return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_PARTITION,1,0,0);
 uint8_t header_id[12]={0};put_le(header_id,base.h.container_id,8);header_id[11]=6;
 if(!find_id(w->base_rows,base.h.chunk_count,header_id)||
  !find_id(w->small_rows,small.h.chunk_count,header_id))
  return refused(r,MB_TOC_OVERLAY_METADATA_REJECTED,MB_TOC_OVERLAY_REASON_HEADER_CHUNK,1,0,0);
 uint32_t replaced=0,added=0;uint64_t logical_base=base.h.logical_block_capacity;
 for(uint32_t i=0;i<small.h.chunk_count;++i){
  const uint8_t *id=id_at(&small,i),*m=meta_at(&small,i);
  if(m[20]||id[11]==4||(id[11]==6&&memcmp(id,header_id,12)))
   return refused(r,MB_TOC_OVERLAY_UNSUPPORTED,MB_TOC_OVERLAY_REASON_META,1,0,0);
  const mb_toc_overlay_id_row_v1 *old=find_id(w->base_rows,base.h.chunk_count,id);
  if(old){
   if(meta_at(&base,old->row)[20])return refused(r,MB_TOC_OVERLAY_UNSUPPORTED,
    MB_TOC_OVERLAY_REASON_META,0,0,0);
   ++replaced;
  }else ++added;
  uint64_t offset=be40(logical_at(&small,i)),length=be40(logical_at(&small,i)+5);
  if(logical_base>MAX_U40-offset||length>MAX_U40-(logical_base+offset))
   return refused(r,MB_TOC_OVERLAY_RESOURCE_LIMIT,MB_TOC_OVERLAY_REASON_CHUNK,1,0,0);
 }
 uint64_t count=(uint64_t)base.h.chunk_count+added;
 uint64_t blocks=(uint64_t)base.h.block_count+small.h.block_count;
 if(count>MB_IOSTORE_MAX_CHUNKS||blocks>MB_IOSTORE_MAX_BLOCKS)
  return refused(r,MB_TOC_OVERLAY_RESOURCE_LIMIT,MB_TOC_OVERLAY_REASON_OUTPUT,0,0,0);
 for(uint32_t i=0;i<small.h.block_count;++i){
  const uint8_t *b=block_at(&small,i);uint64_t offset=le(b,5),stored=le(b+5,3);
  if(p->base_ucas_bytes>MAX_U40-offset||stored>MAX_U40-(p->base_ucas_bytes+offset))
   return refused(r,MB_TOC_OVERLAY_RESOURCE_LIMIT,MB_TOC_OVERLAY_REASON_BLOCK,1,0,0);
 }
 uint64_t needed=144+count*50+4+blocks*12+p->merged_directory.bytes;
 if(needed>MB_IOSTORE_MAX_TOC_BYTES||needed>SIZE_MAX)
  return refused(r,MB_TOC_OVERLAY_RESOURCE_LIMIT,MB_TOC_OVERLAY_REASON_OUTPUT,0,0,0);
 r->required_output_bytes=needed;
 if(capacity<needed)return refused(r,MB_TOC_OVERLAY_OUTPUT_TOO_SMALL,MB_TOC_OVERLAY_REASON_OUTPUT,0,0,0);
 /* All refusal paths precede the first output write. Immutable caller backing
  * is required for this callback-free validated serialization phase. */
 memcpy(output,base.bytes,144);
 put_le(output+24,count,4);put_le(output+28,blocks,4);
 put_le(output+48,p->merged_directory.bytes,4);put_le(output+52,2,4);
 put_le(output+84,1,4);put_le(output+88,p->base_ucas_bytes,8);put_le(output+96,count,4);
 uint8_t *ids=output+144,*logicals=ids+(size_t)count*12;
 uint8_t *seeds=logicals+(size_t)count*10,*overflows=seeds+4;
 uint8_t *block_bytes=overflows+(size_t)count*4;
 uint8_t *directory=block_bytes+(size_t)blocks*12,*metas=directory+p->merged_directory.bytes;
 memcpy(ids,base.bytes+(size_t)base.h.chunk_ids.offset,(size_t)base.h.chunk_count*12);
 memcpy(logicals,base.bytes+(size_t)base.h.chunk_offset_lengths.offset,(size_t)base.h.chunk_count*10);
 memcpy(metas,base.bytes+(size_t)base.h.entry_metadata.offset,(size_t)base.h.chunk_count*24);
 uint32_t added_row=base.h.chunk_count;
 for(uint32_t i=0;i<small.h.chunk_count;++i){
  const mb_toc_overlay_id_row_v1 *source=w->small_rows+i;
  const mb_toc_overlay_id_row_v1 *old=find_id(w->base_rows,base.h.chunk_count,source->id);
  uint32_t target=old?old->row:added_row++;
  memcpy(ids+(size_t)target*12,source->id,12);
  const uint8_t *l=logical_at(&small,source->row);
  put_be40(logicals+(size_t)target*10,logical_base+be40(l));
  memcpy(logicals+(size_t)target*10+5,l+5,5);
  memcpy(metas+(size_t)target*24,meta_at(&small,source->row),24);
 }
 put_le(seeds,(uint32_t)(-(int32_t)(count+1)),4);
 for(uint32_t i=0;i<(uint32_t)count;++i)put_le(overflows+(size_t)i*4,i,4);
 memcpy(block_bytes,base.bytes+(size_t)base.h.compression_blocks.offset,(size_t)base.h.block_count*12);
 for(uint32_t i=0;i<small.h.block_count;++i){
  const uint8_t *b=block_at(&small,i);uint8_t *out=block_bytes+((size_t)base.h.block_count+i)*12;
  put_le(out,p->base_ucas_bytes+le(b,5),5);memcpy(out+5,b+5,7);
 }
 memcpy(directory,p->merged_directory.data,p->merged_directory.bytes);
 r->original_chunks=base.h.chunk_count;r->small_chunks=small.h.chunk_count;r->final_chunks=(uint32_t)count;
 r->replaced_chunks=replaced;r->added_chunks=added;r->unaffected_chunks=base.h.chunk_count-replaced;
 r->original_blocks=base.h.block_count;r->small_blocks=small.h.block_count;r->final_blocks=(uint32_t)blocks;
 r->container_id=base.h.container_id;r->partition_bytes=p->base_ucas_bytes;r->partition_count=2;
 r->base_physical_end=base.physical_end;r->base_aligned_end=base.aligned_end;
 r->small_physical_end=small.physical_end;r->small_aligned_end=small.aligned_end;
 r->output_bytes=needed;r->metadata_complete=1;r->original_blocks_preserved=1;r->unaffected_mappings_preserved=1;
 r->status=MB_TOC_OVERLAY_OK;return MB_TOC_OVERLAY_OK;
}
