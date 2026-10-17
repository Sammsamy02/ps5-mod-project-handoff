#include "none_packer.h"
#include "blake3.h"
#include <string.h>
typedef struct span { const void *pointer;size_t bytes; } span;
static int separate(const span *s,size_t n) {
 for(size_t i=0;i<n;i++) {
  uintptr_t a=(uintptr_t)s[i].pointer;
  if(s[i].bytes&&(!s[i].pointer||s[i].bytes>UINTPTR_MAX-a))return 0;
  for(size_t j=0;j<i;j++) {
   uintptr_t b=(uintptr_t)s[j].pointer;
   if(s[i].bytes&&s[j].bytes&&a<b+s[j].bytes&&b<a+s[i].bytes)return 0;
  }
 }
 return 1;
}
static void little(uint8_t *dst,uint64_t value,size_t bytes) {
 for(size_t i=0;i<bytes;i++){dst[i]=(uint8_t)value;value>>=8;}
}
static void big40(uint8_t *dst,uint64_t value) {
 for(size_t i=0;i<5;i++){dst[4-i]=(uint8_t)value;value>>=8;}
}
static int compare(const mb_iostore_none_chunk_v1 *chunks,uint32_t a,uint32_t b) {
 return memcmp(chunks[a].raw_id,chunks[b].raw_id,12);
}
static void sift(const mb_iostore_none_chunk_v1 *chunks,uint32_t *rows,uint32_t root,uint32_t count) {
 for(;;) {
  uint32_t child=root*2+1;if(child>=count)return;
  if(child+1<count&&compare(chunks,rows[child],rows[child+1])<0)child++;
  if(compare(chunks,rows[root],rows[child])>=0)return;
  uint32_t temp=rows[root];rows[root]=rows[child];rows[child]=temp;root=child;
 }
}
static void sort(const mb_iostore_none_chunk_v1 *chunks,uint32_t *rows,uint32_t count) {
 for(uint32_t i=0;i<count;i++)rows[i]=i;
 uint32_t root=count/2;while(root){--root;sift(chunks,rows,root,count);}
 uint32_t end=count;while(end>1){--end;uint32_t temp=rows[0];rows[0]=rows[end];rows[end]=temp;sift(chunks,rows,0,end);}
}
static mb_iostore_none_status_v1 finish(mb_iostore_none_result_v1 *r,mb_iostore_none_status_v1 status) {
 r->status=(uint32_t)status;return status;
}
mb_iostore_none_status_v1 mb_iostore_none_pack_v1(const mb_iostore_none_params_v1 *p,
 mb_iostore_none_workspace_v1 *w,mb_iostore_none_result_v1 *r) {
 if(!p||!w||!r||(uintptr_t)p%_Alignof(mb_iostore_none_params_v1)||
 (uintptr_t)w%_Alignof(mb_iostore_none_workspace_v1)||(uintptr_t)r%_Alignof(mb_iostore_none_result_v1))return MB_IOSTORE_NONE_BAD_ARGUMENT;
 if(p->chunk_count>MB_IOSTORE_NONE_MAX_CHUNKS||p->directory_bytes>MB_IOSTORE_NONE_MAX_TOC_BYTES||
 w->toc_capacity>MB_IOSTORE_NONE_MAX_TOC_BYTES||w->sort_capacity>MB_IOSTORE_NONE_MAX_CHUNKS)return MB_IOSTORE_NONE_LIMIT;
 if(w->transfer_bytes!=MB_IOSTORE_NONE_BLOCK_BYTES||w->sort_capacity<p->chunk_count||
 (uintptr_t)p->chunks%_Alignof(mb_iostore_none_chunk_v1)||(uintptr_t)w->sort_rows%_Alignof(uint32_t))return MB_IOSTORE_NONE_BAD_ARGUMENT;
 span spans[]={{p,sizeof(*p)},{w,sizeof(*w)},{r,sizeof(*r)},
 {p->chunks,(size_t)p->chunk_count*sizeof(*p->chunks)},{p->directory_index,p->directory_bytes},
 {w->toc,w->toc_capacity},{w->transfer,w->transfer_bytes},{w->sort_rows,w->sort_capacity*sizeof(*w->sort_rows)}};
 if(!separate(spans,sizeof(spans)/sizeof(spans[0])))return MB_IOSTORE_NONE_BAD_ARGUMENT;
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;
 r->failed_row=UINT32_MAX;r->failed_block=UINT32_MAX;
 if(p->struct_size!=sizeof(*p)||p->abi_version!=1||p->reserved||w->struct_size!=sizeof(*w)||w->abi_version!=1||
 !p->read_chunk||!p->write_ucas||!p->directory_bytes)return finish(r,MB_IOSTORE_NONE_BAD_ARGUMENT);
 if(!p->chunk_count)return finish(r,MB_IOSTORE_NONE_INVALID_CHUNK);
 uint64_t content=0,padded=0;uint32_t blocks=0;
 for(uint32_t row=0;row<p->chunk_count;row++) {
  const mb_iostore_none_chunk_v1 *c=&p->chunks[row];
  if(!c->bytes||c->raw_id[11]==4)return finish(r,MB_IOSTORE_NONE_INVALID_CHUNK);
  if(c->bytes>MB_IOSTORE_NONE_MAX_PAYLOAD_BYTES-content)return finish(r,MB_IOSTORE_NONE_LIMIT);
  uint64_t block_count=(c->bytes+MB_IOSTORE_NONE_BLOCK_BYTES-1)/MB_IOSTORE_NONE_BLOCK_BYTES;
  if(block_count>MB_IOSTORE_NONE_MAX_BLOCKS-blocks)return finish(r,MB_IOSTORE_NONE_LIMIT);
  blocks+=(uint32_t)block_count;content+=c->bytes;
  uint64_t extent=content+((16-(c->bytes%16))%16);if(extent>padded)padded=extent;
 }
 uint64_t toc_bytes=UINT64_C(148)+(uint64_t)p->chunk_count*50+(uint64_t)blocks*12+p->directory_bytes;
 if(toc_bytes>MB_IOSTORE_NONE_MAX_TOC_BYTES)return finish(r,MB_IOSTORE_NONE_LIMIT);
 sort(p->chunks,w->sort_rows,p->chunk_count);
 for(uint32_t i=1;i<p->chunk_count;i++)if(!compare(p->chunks,w->sort_rows[i-1],w->sort_rows[i]))return finish(r,MB_IOSTORE_NONE_DUPLICATE_ID);
 r->chunk_count=p->chunk_count;r->block_count=blocks;r->toc_bytes=toc_bytes;r->content_bytes=content;
 r->ucas_bytes=padded;r->padding_bytes=padded-content;
 if(w->toc_capacity<toc_bytes)return finish(r,MB_IOSTORE_NONE_BUFFER_TOO_SMALL);
 r->stage=MB_IOSTORE_NONE_TOC;memset(w->toc,0,(size_t)toc_bytes);
 uint8_t *toc=w->toc;memcpy(toc,"-==--==--==--==-",16);toc[16]=8;
 little(toc+20,144,4);little(toc+24,p->chunk_count,4);little(toc+28,blocks,4);little(toc+32,12,4);
 little(toc+40,32,4);little(toc+44,MB_IOSTORE_NONE_BLOCK_BYTES,4);little(toc+48,p->directory_bytes,4);little(toc+52,1,4);
 memcpy(toc+56,p->container_id,8);toc[80]=8;little(toc+84,1,4);little(toc+88,UINT64_MAX,8);little(toc+96,p->chunk_count,4);
 size_t ids=144,offsets=ids+(size_t)p->chunk_count*12,seed=offsets+(size_t)p->chunk_count*10;
 size_t overflow=seed+4,block_table=overflow+(size_t)p->chunk_count*4;
 size_t directory=block_table+(size_t)blocks*12,metas=directory+p->directory_bytes;
 little(toc+seed,(uint32_t)(-(int32_t)(p->chunk_count+1)),4);
 memcpy(toc+directory,p->directory_index,p->directory_bytes);
 uint64_t physical=0;uint32_t block=0;
 for(uint32_t row=0;row<p->chunk_count;row++) {
  memcpy(toc+ids+(size_t)row*12,p->chunks[row].raw_id,12);
  big40(toc+offsets+(size_t)row*10,(uint64_t)block*MB_IOSTORE_NONE_BLOCK_BYTES);
  big40(toc+offsets+(size_t)row*10+5,p->chunks[row].bytes);little(toc+overflow+(size_t)row*4,row,4);
  uint64_t left=p->chunks[row].bytes;
  while(left) {
   uint32_t n=(uint32_t)(left>MB_IOSTORE_NONE_BLOCK_BYTES?MB_IOSTORE_NONE_BLOCK_BYTES:left);
   uint8_t *entry=toc+block_table+(size_t)block*12;little(entry,physical,5);little(entry+5,n,3);little(entry+8,n,3);
   physical+=n;left-=n;++block;
  }
 }
 r->stage=MB_IOSTORE_NONE_PAYLOAD;physical=0;block=0;
 for(uint32_t row=0;row<p->chunk_count;row++) {
  blake3_hasher hash;blake3_hasher_init(&hash);uint64_t at=0;
  while(at<p->chunks[row].bytes) {
   uint64_t left=p->chunks[row].bytes-at;uint32_t n=(uint32_t)(left>MB_IOSTORE_NONE_BLOCK_BYTES?MB_IOSTORE_NONE_BLOCK_BYTES:left),actual=0;
   ++r->read_calls;r->requested_read_bytes+=n;
   int code=p->read_chunk(p->io_context,row,at,w->transfer,n,&actual);
   if(code||actual!=n){r->failed_row=row;r->failed_block=block;r->failed_offset=at;r->callback_actual=actual;r->callback_status=code;return finish(r,MB_IOSTORE_NONE_READ_FAILED);}
   blake3_hasher_update(&hash,w->transfer,n);actual=0;++r->write_calls;r->requested_write_bytes+=n;
   code=p->write_ucas(p->io_context,physical,w->transfer,n,&actual);
   if(code||actual!=n){r->failed_row=row;r->failed_block=block;r->failed_offset=physical;r->callback_actual=actual;r->callback_status=code;return finish(r,MB_IOSTORE_NONE_WRITE_FAILED);}
   at+=n;physical+=n;++block;
  }
  blake3_hasher_finalize(&hash,toc+metas+(size_t)row*24,20);
 }
 if(padded>content) {
  r->stage=MB_IOSTORE_NONE_PADDING;uint32_t n=(uint32_t)(padded-content),actual=0;memset(w->transfer,0,n);
  ++r->write_calls;r->requested_write_bytes+=n;
  int code=p->write_ucas(p->io_context,content,w->transfer,n,&actual);
  if(code||actual!=n){r->failed_offset=content;r->callback_actual=actual;r->callback_status=code;return finish(r,MB_IOSTORE_NONE_WRITE_FAILED);}
 }
 r->complete=1;r->stage=MB_IOSTORE_NONE_COMPLETE;return finish(r,MB_IOSTORE_NONE_OK);
}
