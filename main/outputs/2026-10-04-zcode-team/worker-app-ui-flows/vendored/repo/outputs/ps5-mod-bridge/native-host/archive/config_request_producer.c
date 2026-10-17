#include "config_request_producer.h"
#include <mbedtls/sha256.h>
#include <string.h>
typedef struct span { const void *pointer;size_t bytes; } span;
typedef struct buffer { uint8_t *data;size_t size,capacity; } buffer;
static int separate(const span *s,size_t count) {
 for(size_t i=0;i<count;i++) {
  uintptr_t a=(uintptr_t)s[i].pointer;
  if(s[i].bytes&&(!s[i].pointer||a>UINTPTR_MAX-s[i].bytes))return 0;
  for(size_t j=0;j<i;j++) {
   uintptr_t b=(uintptr_t)s[j].pointer;
   if(s[i].bytes&&s[j].bytes&&a<b+s[j].bytes&&b<a+s[i].bytes)return 0;
  }
 }
 return 1;
}
static int put(buffer *b,const void *data,size_t n) {
 if(n>b->capacity-b->size)return 0;
 if(n)memcpy(b->data+b->size,data,n);b->size+=n;return 1;
}
static int text(buffer *b,const char *s){return put(b,s,strlen(s));}
static int field(buffer *b,const char *key,const void *data,size_t n) {
 return text(b,key)&&put(b,data,n)&&text(b,"\n");
}
static int decimal(buffer *b,const char *key,uint64_t value) {
 char digits[20];size_t n=0;
 do{digits[n++]=(char)('0'+value%10);value/=10;}while(value);
 if(!text(b,key))return 0;
 while(n)if(!put(b,&digits[--n],1))return 0;
 return text(b,"\n");
}
static int hex(buffer *b,const char *key,const uint8_t *data,size_t n) {
 static const char digits[]="0123456789abcdef";
 if(!text(b,key))return 0;
 for(size_t i=0;i<n;i++) {
  char pair[2]={digits[data[i]>>4],digits[data[i]&15]};
  if(!put(b,pair,2))return 0;
 }
 return text(b,"\n");
}
static int bounded_string(const char *p,size_t capacity,size_t *n) {
 const char *end=memchr(p,0,capacity);if(!end)return 0;*n=(size_t)(end-p);return *n!=0;
}
static int identity(const char *id) {
 if(id[32])return 0;
 for(size_t i=0;i<32;i++)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return 0;
 return 1;
}
static int derived_field(buffer *b,const char *key,const char *prefix,const char *id,const char *suffix) {
 return text(b,key)&&text(b,prefix)&&put(b,id,32)&&text(b,suffix)&&text(b,"\n");
}
static int finish(mb_config_request_producer_result_v1 *out,int status) {
 out->status=(uint32_t)status;return status;
}
int mb_config_request_encode_v1(const mb_config_request_producer_params_v1 *p,
 uint8_t *request,size_t request_capacity,uint8_t *context,size_t context_capacity,
 mb_config_request_producer_result_v1 *out) {
 span s[]={{p,sizeof(*p)},{request,request_capacity},{context,context_capacity},{out,sizeof(*out)}};
 if(!p||!out)return MB_CONFIG_PRODUCER_BAD_ARGUMENT;
 if(request_capacity>MB_CONFIG_REQUEST_MAX_BYTES||context_capacity>MB_CONFIG_CONTEXT_MAX_BYTES)return MB_CONFIG_PRODUCER_LIMIT;
 if((uintptr_t)p%_Alignof(mb_config_request_producer_params_v1)||
 (uintptr_t)out%_Alignof(mb_config_request_producer_result_v1)||!separate(s,4))return MB_CONFIG_PRODUCER_BAD_ARGUMENT;
 memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;
 if(p->struct_size!=sizeof(*p)||p->abi_version!=1)return finish(out,MB_CONFIG_PRODUCER_BAD_ARGUMENT);
 if(p->source_bytes>MB_CONFIG_MAX_SOURCE_BYTES||p->base_bytes>MB_CONFIG_MAX_BASE_BYTES||
 p->max_member_bytes>MB_CONFIG_MAX_MEMBER_BYTES||p->section_count>MB_CONFIG_MAX_SECTIONS||
 p->basename_bytes>MB_CONFIG_MAX_BASENAME_BYTES||p->target_entry_bytes>MB_CONFIG_MAX_PATH_BYTES||
 p->mount_bytes>MB_CONFIG_MAX_PATH_BYTES)return finish(out,MB_CONFIG_PRODUCER_LIMIT);
 if(p->download_complete!=1||p->role!=MB_CONFIG_ROLE_SCALAR_INI||!identity(p->request_id)||
 !p->source_bytes||!p->base_bytes||!p->max_member_bytes||!p->section_count||
 !p->basename_bytes||!p->target_entry_bytes||!p->mount_bytes)return finish(out,MB_CONFIG_PRODUCER_INVALID);
 for(uint32_t i=0;i<p->section_count;i++) {
  if(p->section_bytes[i]>MB_CONFIG_MAX_SECTION_BYTES)return finish(out,MB_CONFIG_PRODUCER_LIMIT);
  if(!p->section_bytes[i])return finish(out,MB_CONFIG_PRODUCER_INVALID);
 }
 size_t source_n,base_n,context_n;
 if(!bounded_string(p->source_ref,sizeof(p->source_ref),&source_n)||
 !bounded_string(p->base_ref,sizeof(p->base_ref),&base_n)||
 !bounded_string(p->context_ref,sizeof(p->context_ref),&context_n))return finish(out,MB_CONFIG_PRODUCER_INVALID);
 uint8_t context_scratch[MB_CONFIG_CONTEXT_MAX_BYTES],request_scratch[MB_CONFIG_REQUEST_MAX_BYTES],digest[32];
 buffer c={context_scratch,0,sizeof(context_scratch)},r={request_scratch,0,sizeof(request_scratch)};
 if(!text(&c,"MBCFGCTX1\n")||!decimal(&c,"source_bytes=",p->source_bytes)||
 !hex(&c,"source_sha256=",p->source_sha256,32)||!decimal(&c,"base_bytes=",p->base_bytes)||
 !hex(&c,"base_sha256=",p->base_sha256,32)||!hex(&c,"target_token=",p->target_token,32)||
 !text(&c,"role=scalar_ini\n")||!field(&c,"basename=",p->basename,p->basename_bytes)||
 !decimal(&c,"max_member_bytes=",p->max_member_bytes)||!field(&c,"target_entry=",p->target_entry,p->target_entry_bytes)||
 !field(&c,"mount=",p->mount,p->mount_bytes)||!decimal(&c,"path_hash_seed=",p->path_hash_seed)||
 !decimal(&c,"section_count=",p->section_count))return finish(out,MB_CONFIG_PRODUCER_LIMIT);
 for(uint32_t i=0;i<p->section_count;i++)if(!field(&c,"section=",p->sections[i],p->section_bytes[i]))return finish(out,MB_CONFIG_PRODUCER_LIMIT);
 if(!text(&c,"end\n"))return finish(out,MB_CONFIG_PRODUCER_LIMIT);
 if(mbedtls_sha256(context_scratch,c.size,digest,0))return finish(out,MB_CONFIG_PRODUCER_HASH_FAILED);
 if(!text(&r,"MBCFGREQ1\n")||!field(&r,"id=",p->request_id,32)||!field(&r,"source=",p->source_ref,source_n)||
 !field(&r,"base=",p->base_ref,base_n)||!field(&r,"context=",p->context_ref,context_n)||
 !hex(&r,"context_sha256=",digest,32)||!derived_field(&r,"quarantine=","q-",p->request_id,"")||
 !derived_field(&r,"output=","p-",p->request_id,".pak")||
 !derived_field(&r,"receipt=","r-",p->request_id,".json")||!text(&r,"end\n"))return finish(out,MB_CONFIG_PRODUCER_LIMIT);
 mb_config_request_v1 decoded;
 int status=mb_config_request_decode_v1(request_scratch,r.size,context_scratch,c.size,digest,&decoded);
 if(status!=MB_CONFIG_REQUEST_OK||!decoded.complete)return finish(out,MB_CONFIG_PRODUCER_INVALID);
 out->request_bytes=(uint32_t)r.size;out->context_bytes=(uint32_t)c.size;memcpy(out->context_sha256,digest,32);
 if(request_capacity<r.size||context_capacity<c.size)return finish(out,MB_CONFIG_PRODUCER_BUFFER_TOO_SMALL);
 memcpy(request,request_scratch,r.size);memcpy(context,context_scratch,c.size);
 out->complete=1;return finish(out,MB_CONFIG_PRODUCER_OK);
}

static int finish_v2(mb_config_request_producer_result_v2 *out,int status) {
 out->status=(uint32_t)status;out->common.status=(uint32_t)status;return status;
}
static int all_zero_v2(const void *data,size_t n) {
 const uint8_t *p=data;for(size_t i=0;i<n;i++)if(p[i])return 0;return 1;
}
int mb_config_request_encode_v2(const mb_config_request_producer_params_v2 *p2,
 uint8_t *request,size_t request_capacity,uint8_t *context,size_t context_capacity,
 mb_config_request_producer_result_v2 *out) {
 span s[]={{p2,sizeof(*p2)},{request,request_capacity},{context,context_capacity},{out,sizeof(*out)}};
 if(!p2||!out)return MB_CONFIG_PRODUCER_BAD_ARGUMENT;
 if(request_capacity>MB_CONFIG_REQUEST_MAX_BYTES||context_capacity>MB_CONFIG_CONTEXT_MAX_BYTES)return MB_CONFIG_PRODUCER_LIMIT;
 if((uintptr_t)p2%_Alignof(mb_config_request_producer_params_v2)||
 (uintptr_t)out%_Alignof(mb_config_request_producer_result_v2)||!separate(s,4))return MB_CONFIG_PRODUCER_BAD_ARGUMENT;
 memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=2;
 out->common.struct_size=sizeof(out->common);out->common.abi_version=1;
 const mb_config_request_producer_params_v1 *p=&p2->common;
 if(p2->struct_size!=sizeof(*p2)||p2->abi_version!=2||p->struct_size!=sizeof(*p)||p->abi_version!=1)return finish_v2(out,MB_CONFIG_PRODUCER_BAD_ARGUMENT);
 if(p->source_bytes>MB_CONFIG_MAX_SOURCE_BYTES||p->base_bytes>MB_CONFIG_MAX_BASE_BYTES||
 p->max_member_bytes>MB_CONFIG_MAX_MEMBER_BYTES||p->section_count>MB_CONFIG_MAX_SECTIONS||
 p->basename_bytes>MB_CONFIG_MAX_BASENAME_BYTES||p2->platform_bytes>MB_CONFIG_MAX_PLATFORM_BYTES)return finish_v2(out,MB_CONFIG_PRODUCER_LIMIT);
 if(p->download_complete!=1||p->role!=MB_CONFIG_ROLE_SCALAR_INI||!identity(p->request_id)||
 !p->source_bytes||!p->base_bytes||!p->max_member_bytes||!p->section_count||!p->basename_bytes||
 p2->target_selection!=MB_CONFIG_TARGET_UNREAL_PROJECT_PLATFORM||!p2->platform_bytes||
 p->target_entry_bytes||p->mount_bytes||p->path_hash_seed||
 !all_zero_v2(p->target_entry,sizeof(p->target_entry))||!all_zero_v2(p->mount,sizeof(p->mount)))return finish_v2(out,MB_CONFIG_PRODUCER_INVALID);
 for(uint32_t i=0;i<p->section_count;i++) {
  if(p->section_bytes[i]>MB_CONFIG_MAX_SECTION_BYTES)return finish_v2(out,MB_CONFIG_PRODUCER_LIMIT);
  if(!p->section_bytes[i])return finish_v2(out,MB_CONFIG_PRODUCER_INVALID);
 }
 size_t source_n,base_n,context_n;
 if(!bounded_string(p->source_ref,sizeof(p->source_ref),&source_n)||
 !bounded_string(p->base_ref,sizeof(p->base_ref),&base_n)||
 !bounded_string(p->context_ref,sizeof(p->context_ref),&context_n))return finish_v2(out,MB_CONFIG_PRODUCER_INVALID);
 uint8_t context_scratch[MB_CONFIG_CONTEXT_MAX_BYTES],request_scratch[MB_CONFIG_REQUEST_MAX_BYTES],digest[32];
 buffer c={context_scratch,0,sizeof(context_scratch)},r={request_scratch,0,sizeof(request_scratch)};
 if(!text(&c,"MBCFGCTX2\n")||!decimal(&c,"source_bytes=",p->source_bytes)||
 !hex(&c,"source_sha256=",p->source_sha256,32)||!decimal(&c,"base_bytes=",p->base_bytes)||
 !hex(&c,"base_sha256=",p->base_sha256,32)||!hex(&c,"target_token=",p->target_token,32)||
 !text(&c,"role=scalar_ini\n")||!field(&c,"basename=",p->basename,p->basename_bytes)||
 !decimal(&c,"max_member_bytes=",p->max_member_bytes)||!text(&c,"target_selection=unreal_project_platform\n")||
 !field(&c,"platform=",p2->platform,p2->platform_bytes)||!decimal(&c,"section_count=",p->section_count))return finish_v2(out,MB_CONFIG_PRODUCER_LIMIT);
 for(uint32_t i=0;i<p->section_count;i++)if(!field(&c,"section=",p->sections[i],p->section_bytes[i]))return finish_v2(out,MB_CONFIG_PRODUCER_LIMIT);
 if(!text(&c,"end\n"))return finish_v2(out,MB_CONFIG_PRODUCER_LIMIT);
 if(mbedtls_sha256(context_scratch,c.size,digest,0))return finish_v2(out,MB_CONFIG_PRODUCER_HASH_FAILED);
 if(!text(&r,"MBCFGREQ1\n")||!field(&r,"id=",p->request_id,32)||!field(&r,"source=",p->source_ref,source_n)||
 !field(&r,"base=",p->base_ref,base_n)||!field(&r,"context=",p->context_ref,context_n)||
 !hex(&r,"context_sha256=",digest,32)||!derived_field(&r,"quarantine=","q-",p->request_id,"")||
 !derived_field(&r,"output=","p-",p->request_id,".pak")||
 !derived_field(&r,"receipt=","r-",p->request_id,".json")||!text(&r,"end\n"))return finish_v2(out,MB_CONFIG_PRODUCER_LIMIT);
 mb_config_request_v2 decoded;
 int status=mb_config_request_decode_v2(request_scratch,r.size,context_scratch,c.size,digest,&decoded);
 if(status!=MB_CONFIG_REQUEST_OK||!decoded.complete)return finish_v2(out,MB_CONFIG_PRODUCER_INVALID);
 out->common.request_bytes=(uint32_t)r.size;out->common.context_bytes=(uint32_t)c.size;memcpy(out->common.context_sha256,digest,32);
 out->target_selection=decoded.target_selection;out->platform_bytes=decoded.platform_bytes;memcpy(out->platform,decoded.platform,sizeof(out->platform));
 if(request_capacity<r.size||context_capacity<c.size)return finish_v2(out,MB_CONFIG_PRODUCER_BUFFER_TOO_SMALL);
 memcpy(request,request_scratch,r.size);memcpy(context,context_scratch,c.size);
 out->complete=1;out->common.complete=1;return finish_v2(out,MB_CONFIG_PRODUCER_OK);
}
