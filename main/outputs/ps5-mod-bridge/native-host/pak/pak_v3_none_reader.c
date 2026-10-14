#include "pak_v3_none_reader.h"
#include <mbedtls/sha1.h>
#include <string.h>

typedef struct reader_job {
 const pak_source_v1 *source;const mb_pak_v3_limits_v1 *limits;
 const mb_pak_v3_buffers_v1 *buffers;mb_pak_v3_result_v1 *result;
 mb_pak_v3_status compare_status;
} reader_job;
typedef struct span {uintptr_t begin,end;} span;
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t le64(const uint8_t *p){return le32(p)|((uint64_t)le32(p+4)<<32);}
static uint16_t le16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));}
static int make_span(const void *p,size_t count,size_t width,size_t alignment,span *out){
 if(!width||count>SIZE_MAX/width)return 0;
 size_t bytes=count*width;uintptr_t start=(uintptr_t)p;
 if((bytes&&!p)||(p&&start%alignment)||bytes>UINTPTR_MAX-start)return 0;
 *out=(span){start,start+bytes};return 1;
}
static int overlap(span a,span b){return a.begin<a.end&&b.begin<b.end&&a.begin<b.end&&b.begin<a.end;}
static int preflight(const pak_source_v1 *s,const mb_pak_v3_limits_v1 *l,uint32_t want,uint32_t row,
 const mb_pak_v3_buffers_v1 *b,mb_pak_v3_result_v1 *r){
 span spans[10];
 if(!make_span(s,1,sizeof(*s),_Alignof(pak_source_v1),&spans[0])||!s||
 !make_span(l,1,sizeof(*l),_Alignof(mb_pak_v3_limits_v1),&spans[1])||!l||
 !make_span(b,1,sizeof(*b),_Alignof(mb_pak_v3_buffers_v1),&spans[2])||!b||
 !make_span(r,1,sizeof(*r),_Alignof(mb_pak_v3_result_v1),&spans[3])||!r)return 0;
 for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)if(overlap(spans[i],spans[j]))return 0;
 if(s->struct_size!=sizeof(*s)||s->abi_version!=1||s->reserved||!s->read_at||!s->snapshot_token||
 s->snapshot_scope!=PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE||
 l->struct_size!=sizeof(*l)||l->abi_version!=1||b->struct_size!=sizeof(*b)||b->abi_version!=1||want>1||(!want&&row)||
 !l->max_archive_bytes||l->max_archive_bytes>MB_PAK_V3_MAX_ARCHIVE_BYTES||
 !l->max_index_bytes||l->max_index_bytes>MB_PAK_V3_MAX_INDEX_BYTES||!l->max_files||l->max_files>MB_PAK_V3_MAX_FILES||
 !l->max_name_bytes||l->max_name_bytes>MB_PAK_V3_MAX_NAME_BYTES||!l->max_read_bytes||l->max_read_bytes>MB_PAK_V3_TRANSFER_BYTES||
 !l->max_read_calls||l->max_read_calls>MB_PAK_V3_MAX_READ_CALLS||!l->max_requested_bytes||l->max_requested_bytes>MB_PAK_V3_MAX_REQUESTED_BYTES||
 !l->max_selected_bytes||l->max_selected_bytes>MB_PAK_V3_MAX_SELECTED_BYTES||
 !l->max_comparison_bytes||l->max_comparison_bytes>MB_PAK_V3_MAX_COMPARISON_BYTES||
 !b->index_capacity||b->index_capacity>MB_PAK_V3_MAX_INDEX_BYTES||!b->entries_capacity||b->entries_capacity>MB_PAK_V3_MAX_FILES||
 !b->names_capacity||b->names_capacity>MB_PAK_V3_MAX_NAME_BYTES||!b->sort_capacity||b->sort_capacity>MB_PAK_V3_MAX_FILES||
 b->transfer_bytes!=MB_PAK_V3_TRANSFER_BYTES||b->selected_capacity>MB_PAK_V3_MAX_SELECTED_BYTES||
 (want&&!b->selected_capacity))return 0;
 if(!make_span(b->index,b->index_capacity,1,1,&spans[4])||
 !make_span(b->entries,b->entries_capacity,sizeof(*b->entries),_Alignof(mb_pak_v3_entry_v1),&spans[5])||
 !make_span(b->names,b->names_capacity,1,1,&spans[6])||
 !make_span(b->sort_indices,b->sort_capacity,sizeof(*b->sort_indices),_Alignof(uint32_t),&spans[7])||
 !make_span(b->transfer,b->transfer_bytes,1,1,&spans[8])||
 !make_span(b->selected,b->selected_capacity,1,1,&spans[9]))return 0;
 for(unsigned i=0;i<10;++i)for(unsigned j=i+1;j<10;++j)if(overlap(spans[i],spans[j]))return 0;
 return 1;
}
static void clear_buffers(const mb_pak_v3_buffers_v1 *b,int published){
 memset(b->index,0,b->index_capacity);memset(b->transfer,0,b->transfer_bytes);
 memset(b->sort_indices,0,(size_t)b->sort_capacity*sizeof(*b->sort_indices));
 if(!published){memset(b->entries,0,(size_t)b->entries_capacity*sizeof(*b->entries));memset(b->names,0,b->names_capacity);
  if(b->selected_capacity)memset(b->selected,0,b->selected_capacity);}
}
static mb_pak_v3_status read_exact(reader_job *j,uint64_t offset,uint8_t *out,uint32_t n){
 mb_pak_v3_result_v1 *r=j->result;const mb_pak_v3_limits_v1 *l=j->limits;
 if(!n||n>l->max_read_bytes||offset>j->source->logical_size||n>j->source->logical_size-offset)return MB_PAK_V3_MALFORMED;
 if(r->read_calls>=l->max_read_calls||r->requested_bytes>l->max_requested_bytes||n>l->max_requested_bytes-r->requested_bytes)return MB_PAK_V3_RESOURCE_LIMIT;
 ++r->read_calls;r->requested_bytes+=n;r->last_requested=n;r->last_actual=0;
 uint32_t actual=0;pak_provider_status status=j->source->read_at(j->source->context,offset,out,n,&actual);
 r->last_provider_status=(uint32_t)status;r->last_actual=actual;
 if(actual>n)return MB_PAK_V3_PROVIDER_CONTRACT;
 switch(status){case PAK_PROVIDER_OK:return actual==n?MB_PAK_V3_OK:MB_PAK_V3_SHORT_READ;
 case PAK_PROVIDER_CHANGED:return MB_PAK_V3_SOURCE_CHANGED;
 case PAK_PROVIDER_CANCELLED:return MB_PAK_V3_CANCELLED;
 case PAK_PROVIDER_CONTRACT_ERROR:return MB_PAK_V3_PROVIDER_CONTRACT;
 case PAK_PROVIDER_IO_ERROR:case PAK_PROVIDER_UNAVAILABLE:return MB_PAK_V3_READ_ERROR;
 default:return MB_PAK_V3_PROVIDER_CONTRACT;}
}
static mb_pak_v3_status read_span(reader_job *j,uint64_t offset,uint8_t *out,size_t n){
 size_t at=0;while(at<n){uint32_t part=(uint32_t)(n-at>j->limits->max_read_bytes?j->limits->max_read_bytes:n-at);
  mb_pak_v3_status status=read_exact(j,offset+at,out+at,part);if(status)return status;at+=part;}return MB_PAK_V3_OK;
}
/* Strict UTF8, including overlong/surrogate/non-scalar rejection. */
static int valid_utf8(const uint8_t *p,size_t n){
 for(size_t at=0;at<n;){uint32_t cp;unsigned continuation;uint8_t c=p[at++];
  if(!c)return 0;if(c<0x80)continue;
  if(c>=0xc2&&c<=0xdf){cp=c&31u;continuation=1;}
  else if(c>=0xe0&&c<=0xef){cp=c&15u;continuation=2;}
  else if(c>=0xf0&&c<=0xf4){cp=c&7u;continuation=3;}else return 0;
  if(continuation>n-at)return 0;unsigned length=continuation;
  for(unsigned k=0;k<length;++k){c=p[at++];if((c&0xc0)!=0x80)return 0;cp=(cp<<6)|(c&63u);}
  if((length==2&&cp<0x800)||(length==3&&cp<0x10000)||(cp>=0xd800&&cp<=0xdfff)||cp>0x10ffff)return 0;
 }return 1;
}
static int append_cp(uint32_t cp,uint8_t *out,size_t cap,size_t *used){
 unsigned n=cp<0x80?1:cp<0x800?2:cp<0x10000?3:4;if(n>cap-*used)return 0;
 if(n==1)out[(*used)++]=(uint8_t)cp;
 else {unsigned shift=6*(n-1);out[(*used)++]=(uint8_t)((n==2?0xc0:n==3?0xe0:0xf0)|(cp>>shift));
  while(shift){shift-=6;out[(*used)++]=(uint8_t)(0x80|((cp>>shift)&63u));}}
 return 1;
}
static mb_pak_v3_status fstring(const uint8_t *index,size_t size,size_t *at,uint8_t *out,size_t cap,size_t *bytes){
 if(*at>size||size-*at<4)return MB_PAK_V3_MALFORMED;
 uint32_t raw=le32(index+*at);*at+=4;size_t used=0;
 if(!raw||raw==UINT32_C(0x80000000))return MB_PAK_V3_MALFORMED;
 if(raw<UINT32_C(0x80000000)){
  size_t n=raw;if(n>size-*at||index[*at+n-1]||!valid_utf8(index+*at,n-1))return MB_PAK_V3_MALFORMED;
  if(n-1>MB_PAK_V3_MAX_PATH_BYTES||n>cap)return MB_PAK_V3_RESOURCE_LIMIT;
  memcpy(out,index+*at,n);used=n-1;*at+=n;
 }else{
  uint32_t units=~raw+1u;if(units>(size-*at)/2||le16(index+*at+(size_t)(units-1)*2))return MB_PAK_V3_MALFORMED;
  size_t end=*at+(size_t)units*2;
  for(uint32_t i=0;i+1<units;++i){uint32_t cp=le16(index+*at+(size_t)i*2);if(!cp)return MB_PAK_V3_MALFORMED;
   if(cp>=0xd800&&cp<=0xdbff){if(i+2>=units)return MB_PAK_V3_MALFORMED;
    uint32_t low=le16(index+*at+(size_t)(++i)*2);if(low<0xdc00||low>0xdfff)return MB_PAK_V3_MALFORMED;cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);}
   else if(cp>=0xdc00&&cp<=0xdfff)return MB_PAK_V3_MALFORMED;
   if(!cap||!append_cp(cp,out,cap-1,&used)||used>MB_PAK_V3_MAX_PATH_BYTES)return MB_PAK_V3_RESOURCE_LIMIT;
  }
  if(!cap)return MB_PAK_V3_RESOURCE_LIMIT;out[used]=0;*at=end;
 }*bytes=used;return MB_PAK_V3_OK;
}
static int compare(reader_job *j,uint32_t a,uint32_t b,int names){
 const mb_pak_v3_entry_v1 *x=&j->buffers->entries[a],*y=&j->buffers->entries[b];
 if(!names)return x->archive_offset<y->archive_offset?-1:x->archive_offset>y->archive_offset?1:0;
 if(j->compare_status)return 0;
 size_t n=x->path_bytes<y->path_bytes?x->path_bytes:y->path_bytes;
 if(n>j->limits->max_comparison_bytes-j->result->comparison_bytes){j->compare_status=MB_PAK_V3_RESOURCE_LIMIT;return 0;}
 j->result->comparison_bytes+=n;
 int c=memcmp(j->buffers->names+x->path_offset,j->buffers->names+y->path_offset,n);
 return c?c:x->path_bytes<y->path_bytes?-1:x->path_bytes>y->path_bytes?1:0;
}
static void sift(reader_job *j,uint32_t root,uint32_t count,int names){
 uint32_t *order=j->buffers->sort_indices;
 while(root<count/2){uint32_t child=root*2+1;
  if(child+1<count&&compare(j,order[child],order[child+1],names)<0)++child;
  if(compare(j,order[root],order[child],names)>=0)return;
  uint32_t t=order[root];order[root]=order[child];order[child]=t;root=child;}
}
static void sort_rows(reader_job *j,uint32_t count,int names){
 uint32_t *order=j->buffers->sort_indices;for(uint32_t i=0;i<count;++i)order[i]=i;
 for(uint32_t start=count/2;start;--start)sift(j,start-1,count,names);
 for(uint32_t end=count;end>1;--end){uint32_t t=order[0];order[0]=order[end-1];order[end-1]=t;sift(j,0,end-1,names);}
}
void mb_pak_v3_limits_v1_default(mb_pak_v3_limits_v1 *l){
 if(l)*l=(mb_pak_v3_limits_v1){sizeof(*l),1,MB_PAK_V3_MAX_FILES,MB_PAK_V3_MAX_INDEX_BYTES,MB_PAK_V3_MAX_NAME_BYTES,
 MB_PAK_V3_TRANSFER_BYTES,MB_PAK_V3_MAX_READ_CALLS,MB_PAK_V3_MAX_SELECTED_BYTES,MB_PAK_V3_MAX_ARCHIVE_BYTES,
 MB_PAK_V3_MAX_REQUESTED_BYTES,MB_PAK_V3_MAX_COMPARISON_BYTES};
}
mb_pak_v3_status mb_pak_v3_read_none_v1(const pak_source_v1 *s,const mb_pak_v3_limits_v1 *l,
 uint32_t want,uint32_t selected,const mb_pak_v3_buffers_v1 *b,mb_pak_v3_result_v1 *r){
 if(!preflight(s,l,want,selected,b,r))return MB_PAK_V3_BAD_ARGUMENT;
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;r->selected_row=want?selected:UINT32_MAX;
 clear_buffers(b,0);reader_job j={s,l,b,r,MB_PAK_V3_OK};mb_pak_v3_status status=MB_PAK_V3_OK;
 mbedtls_sha1_context hash;mbedtls_sha1_init(&hash);
#define STOP(code) do {status=(code);goto cleanup;} while(0)
 if(s->logical_size<MB_PAK_V3_FOOTER_BYTES)STOP(MB_PAK_V3_MALFORMED);
 if(s->logical_size>l->max_archive_bytes)STOP(MB_PAK_V3_RESOURCE_LIMIT);
 r->stage=MB_PAK_V3_FOOTER;uint8_t footer[44];
 status=read_span(&j,s->logical_size-44,footer,sizeof(footer));if(status)goto cleanup;
 if(le32(footer)!=UINT32_C(0x5a6f12e1)||le32(footer+4)!=3)STOP(MB_PAK_V3_UNSUPPORTED);
 r->index_offset=le64(footer+8);r->index_bytes=le64(footer+16);
 if(!r->index_bytes||r->index_offset>s->logical_size-44||r->index_bytes!=s->logical_size-44-r->index_offset)STOP(MB_PAK_V3_MALFORMED);
 if(r->index_bytes>l->max_index_bytes)STOP(MB_PAK_V3_RESOURCE_LIMIT);
 if(r->index_bytes>b->index_capacity)STOP(MB_PAK_V3_BUFFER_TOO_SMALL);
 r->stage=MB_PAK_V3_INDEX;status=read_span(&j,r->index_offset,b->index,(size_t)r->index_bytes);if(status)goto cleanup;
 uint8_t digest[20];if(mbedtls_sha1(b->index,(size_t)r->index_bytes,digest))STOP(MB_PAK_V3_HASH_ERROR);
 if(memcmp(digest,footer+24,20))STOP(MB_PAK_V3_DIGEST_MISMATCH);
 memcpy(r->index_sha1,digest,20);size_t at=0,mount_bytes=0;
 status=fstring(b->index,(size_t)r->index_bytes,&at,r->mount,sizeof(r->mount),&mount_bytes);if(status)goto cleanup;
 r->mount_bytes=(uint32_t)mount_bytes;
 if(r->index_bytes-at<4)STOP(MB_PAK_V3_MALFORMED);
 uint32_t count=le32(b->index+at);at+=4;
 if(count>l->max_files)STOP(MB_PAK_V3_RESOURCE_LIMIT);
 if(count>b->entries_capacity||count>b->sort_capacity)STOP(MB_PAK_V3_BUFFER_TOO_SMALL);
 if(want&&selected>=count)STOP(MB_PAK_V3_MALFORMED);
 r->stage=MB_PAK_V3_ENTRIES;size_t name_used=0;
 for(uint32_t i=0;i<count;++i){mb_pak_v3_entry_v1 *e=&b->entries[i];size_t path_bytes=0;
  if(name_used>=b->names_capacity)STOP(MB_PAK_V3_BUFFER_TOO_SMALL);
  if(name_used>=l->max_name_bytes)STOP(MB_PAK_V3_RESOURCE_LIMIT);
  size_t cap=b->names_capacity-name_used;if(cap>l->max_name_bytes-name_used)cap=l->max_name_bytes-name_used;
  status=fstring(b->index,(size_t)r->index_bytes,&at,b->names+name_used,cap,&path_bytes);if(status)goto cleanup;
  if(!path_bytes)STOP(MB_PAK_V3_MALFORMED);
  e->path_offset=(uint32_t)name_used;e->path_bytes=(uint32_t)path_bytes;name_used+=path_bytes+1;
  if(r->index_bytes-at<53)STOP(MB_PAK_V3_MALFORMED);const uint8_t *p=b->index+at;
  if(le32(p+24)||p[48]||le32(p+49))STOP(MB_PAK_V3_UNSUPPORTED);
  e->archive_offset=le64(p);e->payload_bytes=le64(p+8);e->data_offset=e->archive_offset+53;
  if(e->archive_offset>r->index_offset||r->index_offset-e->archive_offset<53||
   e->payload_bytes!=le64(p+16)||e->payload_bytes>r->index_offset-e->data_offset)STOP(MB_PAK_V3_MALFORMED);
  memcpy(e->sha1,p+28,20);at+=53;
 }
 if(at!=r->index_bytes)STOP(MB_PAK_V3_MALFORMED);
 sort_rows(&j,count,1);if(j.compare_status)STOP(j.compare_status);
 for(uint32_t i=1;i<count;++i){if(!compare(&j,b->sort_indices[i-1],b->sort_indices[i],1))STOP(j.compare_status?j.compare_status:MB_PAK_V3_MALFORMED);}
 if(j.compare_status)STOP(j.compare_status);
 sort_rows(&j,count,0);uint64_t end=0;
 for(uint32_t i=0;i<count;++i){const mb_pak_v3_entry_v1 *e=&b->entries[b->sort_indices[i]];
  if(e->archive_offset<end)STOP(MB_PAK_V3_MALFORMED);end=e->data_offset+e->payload_bytes;}
 r->stage=MB_PAK_V3_PHYSICAL_HEADERS;
 for(uint32_t i=0;i<count;++i){const mb_pak_v3_entry_v1 *e=&b->entries[i];uint8_t physical[53];
  status=read_span(&j,e->archive_offset,physical,sizeof(physical));if(status)goto cleanup;
  if(le32(physical+24)||physical[48]||le32(physical+49))STOP(MB_PAK_V3_UNSUPPORTED);
  uint64_t offset=le64(physical);
  if((offset&&offset!=e->archive_offset)||le64(physical+8)!=e->payload_bytes||le64(physical+16)!=e->payload_bytes||memcmp(physical+28,e->sha1,20))STOP(MB_PAK_V3_MALFORMED);
 }
 if(want){const mb_pak_v3_entry_v1 *e=&b->entries[selected];r->stage=MB_PAK_V3_PAYLOAD;
  if(e->payload_bytes>l->max_selected_bytes)STOP(MB_PAK_V3_RESOURCE_LIMIT);
  if(e->payload_bytes>b->selected_capacity)STOP(MB_PAK_V3_BUFFER_TOO_SMALL);
  if(mbedtls_sha1_starts(&hash))STOP(MB_PAK_V3_HASH_ERROR);
  uint64_t pos=0;while(pos<e->payload_bytes){uint32_t n=(uint32_t)(e->payload_bytes-pos>l->max_read_bytes?l->max_read_bytes:e->payload_bytes-pos);
   status=read_exact(&j,e->data_offset+pos,b->transfer,n);if(status)goto cleanup;
   if(mbedtls_sha1_update(&hash,b->transfer,n))STOP(MB_PAK_V3_HASH_ERROR);
   memcpy(b->selected+(size_t)pos,b->transfer,n);pos+=n;}
  if(mbedtls_sha1_finish(&hash,digest))STOP(MB_PAK_V3_HASH_ERROR);
  if(memcmp(digest,e->sha1,20))STOP(MB_PAK_V3_DIGEST_MISMATCH);
  memcpy(r->selected_sha1,digest,20);r->selected_bytes=e->payload_bytes;r->selected_complete=1;
 }
 r->file_count=count;r->name_bytes=(uint32_t)name_used;r->metadata_complete=1;r->snapshot_token=s->snapshot_token;r->stage=MB_PAK_V3_COMPLETE;
cleanup:
 mbedtls_sha1_free(&hash);clear_buffers(b,status==MB_PAK_V3_OK);
 if(status){mb_pak_v3_result_v1 spent=*r;memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;
  r->stage=spent.stage;r->selected_row=want?selected:UINT32_MAX;r->read_calls=spent.read_calls;r->requested_bytes=spent.requested_bytes;
  r->comparison_bytes=spent.comparison_bytes;r->last_provider_status=spent.last_provider_status;r->last_requested=spent.last_requested;r->last_actual=spent.last_actual;}
 else if(!want&&b->selected_capacity)memset(b->selected,0,b->selected_capacity);
 r->status=status;return status;
#undef STOP
}
