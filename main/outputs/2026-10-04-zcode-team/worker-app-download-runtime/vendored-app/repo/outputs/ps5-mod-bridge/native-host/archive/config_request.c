#include "config_request.h"
#include <string.h>

typedef struct span { const void *ptr; size_t size; } span;
typedef struct field { const uint8_t *ptr; size_t size; } field;
static int valid_span(span s) {
 uintptr_t p=(uintptr_t)s.ptr;
 return !s.size||(s.ptr&&p<=UINTPTR_MAX-s.size);
}
static int disjoint(const span *s,size_t count) {
 for(size_t i=0;i<count;i++) {
  if(!valid_span(s[i]))return 0;
  for(size_t j=0;j<i;j++) {
   uintptr_t a=(uintptr_t)s[i].ptr,b=(uintptr_t)s[j].ptr;
   if(s[i].size&&s[j].size&&a<b+s[j].size&&b<a+s[i].size)return 0;
  }
 }
 return 1;
}
static int fail_ref(mb_config_request_ref_v1 *out,int status) {
 memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;out->status=(uint32_t)status;return status;
}
static int fail_full(mb_config_request_v1 *out,int status) {
 memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;out->status=(uint32_t)status;return status;
}
static int line(const uint8_t *p,size_t size,size_t *at,field *value) {
 size_t start=*at,i;
 if(start>=size)return 0;
 for(i=start;i<size&&p[i]!='\n';i++)if(p[i]<0x20||p[i]>0x7e)return 0;
 if(i==size)return 0;
 value->ptr=p+start;value->size=i-start;*at=i+1;return 1;
}
static int equal(field value,const char *s) {
 size_t n=strlen(s);return value.size==n&&memcmp(value.ptr,s,n)==0;
}
static int value(const uint8_t *p,size_t size,size_t *at,const char *key,field *out) {
 field f;size_t n=strlen(key);
 if(!line(p,size,at,&f)||f.size<n||memcmp(f.ptr,key,n))return 0;
 out->ptr=f.ptr+n;out->size=f.size-n;return 1;
}
static int lower_hex(field v,size_t digits,uint8_t *out) {
 if(v.size!=digits)return 0;
 for(size_t i=0;i<digits;i++) {
  uint8_t c=v.ptr[i],n;
  if(c>='0'&&c<='9')n=(uint8_t)(c-'0');
  else if(c>='a'&&c<='f')n=(uint8_t)(c-'a'+10);else return 0;
  if(out){if(!(i&1u))out[i/2]=(uint8_t)(n<<4);else out[i/2]|=n;}
 }
 return 1;
}
static int hex_line(const uint8_t *p,size_t size,size_t *at,const char *key,uint8_t out[32]) {
 field v;return value(p,size,at,key,&v)&&lower_hex(v,64,out);
}
static int number(field v,uint64_t minimum,uint64_t maximum,uint64_t *out) {
 uint64_t n=0;
 if(!v.size||(v.size>1&&v.ptr[0]=='0'))return 0;
 for(size_t i=0;i<v.size;i++) {
  uint64_t d;if(v.ptr[i]<'0'||v.ptr[i]>'9')return 0;d=v.ptr[i]-'0';
  if(n>(UINT64_MAX-d)/10)return 0;n=n*10+d;
 }
 if(n<minimum||n>maximum)return 0;*out=n;return 1;
}
static int number_line(const uint8_t *p,size_t size,size_t *at,const char *key,
 uint64_t minimum,uint64_t maximum,uint64_t *out) {
 field v;return value(p,size,at,key,&v)&&number(v,minimum,maximum,out);
}
static int ascii_alnum(uint8_t c) {
 return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9');
}
static uint8_t fold(uint8_t c) { return c>='A'&&c<='Z'?(uint8_t)(c+('a'-'A')):c; }
static int case_equal(const char *a,const char *b) {
 size_t i=0;
 while(a[i]&&b[i]){if(fold((uint8_t)a[i])!=fold((uint8_t)b[i]))return 0;i++;}
 return a[i]==b[i];
}
static int component(field v,char *out,size_t capacity) {
 if(!v.size||v.size>=capacity||v.ptr[v.size-1]=='.'||
 (v.size==1&&v.ptr[0]=='.')||(v.size==2&&v.ptr[0]=='.'&&v.ptr[1]=='.'))return 0;
 for(size_t i=0;i<v.size;i++)if(!ascii_alnum(v.ptr[i])&&v.ptr[i]!='_'&&v.ptr[i]!='-'&&v.ptr[i]!='.')return 0;
 memcpy(out,v.ptr,v.size);out[v.size]=0;return 1;
}
static void derived(char *out,const char *prefix,const char *id,const char *suffix) {
 size_t n=strlen(prefix);memcpy(out,prefix,n);memcpy(out+n,id,32);strcpy(out+n+32,suffix);
}
static int parse_ref(const uint8_t *p,size_t size,mb_config_request_ref_v1 *out) {
 size_t at=0;field f;char expected[48];
 if(!line(p,size,&at,&f)||!equal(f,"MBCFGREQ1"))return MB_CONFIG_REQUEST_INVALID;
 if(!value(p,size,&at,"id=",&f)||!lower_hex(f,32,NULL))return MB_CONFIG_REQUEST_INVALID;
 memcpy(out->request_id,f.ptr,32);out->request_id[32]=0;
 if(!value(p,size,&at,"source=",&f)||!component(f,out->source_ref,sizeof(out->source_ref))||
 !value(p,size,&at,"base=",&f)||!component(f,out->base_ref,sizeof(out->base_ref))||
 !value(p,size,&at,"context=",&f)||!component(f,out->context_ref,sizeof(out->context_ref))||
 !hex_line(p,size,&at,"context_sha256=",out->context_sha256))return MB_CONFIG_REQUEST_INVALID;
 if(!value(p,size,&at,"quarantine=",&f)||!component(f,out->quarantine_ref,sizeof(out->quarantine_ref)))return MB_CONFIG_REQUEST_INVALID;
 derived(expected,"q-",out->request_id,"");if(strcmp(expected,out->quarantine_ref))return MB_CONFIG_REQUEST_INVALID;
 if(!value(p,size,&at,"output=",&f)||!component(f,out->output_ref,sizeof(out->output_ref)))return MB_CONFIG_REQUEST_INVALID;
 derived(expected,"p-",out->request_id,".pak");if(strcmp(expected,out->output_ref))return MB_CONFIG_REQUEST_INVALID;
 if(!value(p,size,&at,"receipt=",&f)||!component(f,out->receipt_ref,sizeof(out->receipt_ref)))return MB_CONFIG_REQUEST_INVALID;
 derived(expected,"r-",out->request_id,".json");if(strcmp(expected,out->receipt_ref))return MB_CONFIG_REQUEST_INVALID;
 derived(out->claim_ref,"claim-",out->request_id,"");
 const char *inputs[]={out->source_ref,out->base_ref,out->context_ref};
 const char *controls[]={out->quarantine_ref,out->output_ref,out->receipt_ref,out->claim_ref,"request.v1","request.config.v1"};
 for(size_t i=0;i<3;i++) {
  for(size_t j=0;j<i;j++)if(case_equal(inputs[i],inputs[j]))return MB_CONFIG_REQUEST_INVALID;
  for(size_t j=0;j<sizeof(controls)/sizeof(controls[0]);j++)if(case_equal(inputs[i],controls[j]))return MB_CONFIG_REQUEST_INVALID;
 }
 if(!line(p,size,&at,&f)||!equal(f,"end")||at!=size)return MB_CONFIG_REQUEST_INVALID;
 out->status=MB_CONFIG_REQUEST_OK;out->complete=1;return MB_CONFIG_REQUEST_OK;
}
static int path(field v,int mount,char *out,uint32_t *bytes) {
 size_t start=0;int ordinary=0;
 if(!v.size||v.size>MB_CONFIG_MAX_PATH_BYTES)return 0;
 if(mount&&v.size==1&&v.ptr[0]=='/')goto copy;
 if(v.ptr[0]=='/'||(mount&&v.ptr[v.size-1]!='/')||(!mount&&v.ptr[v.size-1]=='/'))return 0;
 for(size_t i=0;i<v.size;i++) {
  uint8_t c=v.ptr[i];
  if(c<0x20||c>=0x7f||c=='\\'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')return 0;
  if(c!='/'&&i!=v.size-1)continue;
  size_t end=c=='/'?i:i+1,n=end-start;
  if(!n||n>255||v.ptr[start]==' '||v.ptr[end-1]==' '||(n==1&&v.ptr[start]=='.'))return 0;
  if(n==2&&v.ptr[start]=='.'&&v.ptr[start+1]=='.'){if(!mount||ordinary)return 0;}else ordinary=1;
  start=i+1;
 }
copy:
 memcpy(out,v.ptr,v.size);out[v.size]=0;*bytes=(uint32_t)v.size;return 1;
}
static int section(field v,mb_config_request_v1 *out,uint32_t index) {
 if(!v.size||v.size>MB_CONFIG_MAX_SECTION_BYTES)return 0;
 for(size_t i=0;i<v.size;i++) {
  uint8_t c=v.ptr[i];if(!ascii_alnum(c)&&c!='_'&&c!='.'&&c!='/'&&c!=':'&&c!='-')return 0;
 }
 for(uint32_t i=0;i<index;i++)if(out->section_bytes[i]==v.size) {
  size_t j;for(j=0;j<v.size;j++)if(fold(v.ptr[j])!=fold((uint8_t)out->sections[i][j]))break;
  if(j==v.size)return 0;
 }
 memcpy(out->sections[index],v.ptr,v.size);out->sections[index][v.size]=0;out->section_bytes[index]=(uint32_t)v.size;return 1;
}
static void bind_ref(const mb_config_request_ref_v1 *ref,mb_config_request_v1 *out) {
 memcpy(out->context_sha256,ref->context_sha256,32);
 memcpy(out->request_id,ref->request_id,sizeof(out->request_id));
 memcpy(out->source_ref,ref->source_ref,sizeof(out->source_ref));memcpy(out->base_ref,ref->base_ref,sizeof(out->base_ref));
 memcpy(out->context_ref,ref->context_ref,sizeof(out->context_ref));memcpy(out->quarantine_ref,ref->quarantine_ref,sizeof(out->quarantine_ref));
 memcpy(out->output_ref,ref->output_ref,sizeof(out->output_ref));memcpy(out->receipt_ref,ref->receipt_ref,sizeof(out->receipt_ref));
 memcpy(out->claim_ref,ref->claim_ref,sizeof(out->claim_ref));
}
int mb_config_request_ref_decode_v1(const uint8_t *request,size_t size,mb_config_request_ref_v1 *out) {
 span s[]={{request,size},{out,sizeof(*out)}};
 if(!request||!out)return MB_CONFIG_REQUEST_BAD_ARGUMENT;
 if(size>MB_CONFIG_REQUEST_MAX_BYTES)return MB_CONFIG_REQUEST_LIMIT;
 if(!disjoint(s,2)||(uintptr_t)out%_Alignof(mb_config_request_ref_v1))return MB_CONFIG_REQUEST_BAD_ARGUMENT;
 fail_ref(out,MB_CONFIG_REQUEST_OK);int rc=parse_ref(request,size,out);return rc?fail_ref(out,rc):rc;
}
int mb_config_request_decode_v1(const uint8_t *request,size_t request_size,const uint8_t *context,size_t context_size,
 const uint8_t observed_context_sha256[32],mb_config_request_v1 *out) {
 span s[]={{request,request_size},{context,context_size},{observed_context_sha256,32},{out,sizeof(*out)}};
 size_t at=0;field f;uint64_t count;mb_config_request_ref_v1 ref={0};
 if(!request||!context||!observed_context_sha256||!out)return MB_CONFIG_REQUEST_BAD_ARGUMENT;
 if(request_size>MB_CONFIG_REQUEST_MAX_BYTES||context_size>MB_CONFIG_CONTEXT_MAX_BYTES)return MB_CONFIG_REQUEST_LIMIT;
 if(!disjoint(s,4)||(uintptr_t)out%_Alignof(mb_config_request_v1))return MB_CONFIG_REQUEST_BAD_ARGUMENT;
 fail_full(out,MB_CONFIG_REQUEST_OK);
 int rc=parse_ref(request,request_size,&ref);if(rc)return fail_full(out,rc);
 if(memcmp(ref.context_sha256,observed_context_sha256,32))return fail_full(out,MB_CONFIG_REQUEST_CONTEXT_MISMATCH);
 bind_ref(&ref,out);
 if(!line(context,context_size,&at,&f)||!equal(f,"MBCFGCTX1")||
 !number_line(context,context_size,&at,"source_bytes=",1,MB_CONFIG_MAX_SOURCE_BYTES,&out->source_bytes)||
 !hex_line(context,context_size,&at,"source_sha256=",out->source_sha256)||
 !number_line(context,context_size,&at,"base_bytes=",1,MB_CONFIG_MAX_BASE_BYTES,&out->base_bytes)||
 !hex_line(context,context_size,&at,"base_sha256=",out->base_sha256)||
 !hex_line(context,context_size,&at,"target_token=",out->target_token)||
 !value(context,context_size,&at,"role=",&f)||!equal(f,"scalar_ini"))return fail_full(out,MB_CONFIG_REQUEST_INVALID);
 out->role=MB_CONFIG_ROLE_SCALAR_INI;
 if(!value(context,context_size,&at,"basename=",&f)||!component(f,out->basename,sizeof(out->basename)))return fail_full(out,MB_CONFIG_REQUEST_INVALID);
 out->basename_bytes=(uint32_t)f.size;
 if(!number_line(context,context_size,&at,"max_member_bytes=",1,MB_CONFIG_MAX_MEMBER_BYTES,&out->max_member_bytes)||
 !value(context,context_size,&at,"target_entry=",&f)||!path(f,0,out->target_entry,&out->target_entry_bytes)||
 !value(context,context_size,&at,"mount=",&f)||!path(f,1,out->mount,&out->mount_bytes)||
 !number_line(context,context_size,&at,"path_hash_seed=",0,UINT64_MAX,&out->path_hash_seed)||
 !number_line(context,context_size,&at,"section_count=",1,MB_CONFIG_MAX_SECTIONS,&count))return fail_full(out,MB_CONFIG_REQUEST_INVALID);
 out->section_count=(uint32_t)count;
 for(uint32_t i=0;i<out->section_count;i++)if(!value(context,context_size,&at,"section=",&f)||!section(f,out,i))return fail_full(out,MB_CONFIG_REQUEST_INVALID);
 if(!line(context,context_size,&at,&f)||!equal(f,"end")||at!=context_size)return fail_full(out,MB_CONFIG_REQUEST_INVALID);
 out->status=MB_CONFIG_REQUEST_OK;out->complete=1;return MB_CONFIG_REQUEST_OK;
}

static int fail_v2(mb_config_request_v2 *out,int status) {
 memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=2;out->status=(uint32_t)status;
 out->common.struct_size=sizeof(out->common);out->common.abi_version=1;out->common.status=(uint32_t)status;return status;
}
static int platform_v2(field v,char *out,uint32_t *bytes) {
 if(!v.size||v.size>MB_CONFIG_MAX_PLATFORM_BYTES)return 0;
 for(size_t i=0;i<v.size;i++)if(!ascii_alnum(v.ptr[i])&&v.ptr[i]!='_'&&v.ptr[i]!='-')return 0;
 memcpy(out,v.ptr,v.size);out[v.size]=0;*bytes=(uint32_t)v.size;return 1;
}
int mb_config_request_decode_v2(const uint8_t *request,size_t request_size,const uint8_t *context,size_t context_size,
 const uint8_t observed_context_sha256[32],mb_config_request_v2 *out) {
 span s[]={{request,request_size},{context,context_size},{observed_context_sha256,32},{out,sizeof(*out)}};
 size_t at=0;field f;uint64_t count;mb_config_request_ref_v1 ref={0};
 if(!request||!context||!observed_context_sha256||!out)return MB_CONFIG_REQUEST_BAD_ARGUMENT;
 if(request_size>MB_CONFIG_REQUEST_MAX_BYTES||context_size>MB_CONFIG_CONTEXT_MAX_BYTES)return MB_CONFIG_REQUEST_LIMIT;
 if(!disjoint(s,4)||(uintptr_t)out%_Alignof(mb_config_request_v2))return MB_CONFIG_REQUEST_BAD_ARGUMENT;
 fail_v2(out,MB_CONFIG_REQUEST_OK);
 int rc=parse_ref(request,request_size,&ref);if(rc)return fail_v2(out,rc);
 if(memcmp(ref.context_sha256,observed_context_sha256,32))return fail_v2(out,MB_CONFIG_REQUEST_CONTEXT_MISMATCH);
 mb_config_request_v1 *c=&out->common;bind_ref(&ref,c);
 if(!line(context,context_size,&at,&f)||!equal(f,"MBCFGCTX2")||
 !number_line(context,context_size,&at,"source_bytes=",1,MB_CONFIG_MAX_SOURCE_BYTES,&c->source_bytes)||
 !hex_line(context,context_size,&at,"source_sha256=",c->source_sha256)||
 !number_line(context,context_size,&at,"base_bytes=",1,MB_CONFIG_MAX_BASE_BYTES,&c->base_bytes)||
 !hex_line(context,context_size,&at,"base_sha256=",c->base_sha256)||
 !hex_line(context,context_size,&at,"target_token=",c->target_token)||
 !value(context,context_size,&at,"role=",&f)||!equal(f,"scalar_ini"))return fail_v2(out,MB_CONFIG_REQUEST_INVALID);
 c->role=MB_CONFIG_ROLE_SCALAR_INI;
 if(!value(context,context_size,&at,"basename=",&f)||!component(f,c->basename,sizeof(c->basename)))return fail_v2(out,MB_CONFIG_REQUEST_INVALID);
 c->basename_bytes=(uint32_t)f.size;
 if(!number_line(context,context_size,&at,"max_member_bytes=",1,MB_CONFIG_MAX_MEMBER_BYTES,&c->max_member_bytes)||
 !value(context,context_size,&at,"target_selection=",&f)||!equal(f,"unreal_project_platform")||
 !value(context,context_size,&at,"platform=",&f)||!platform_v2(f,out->platform,&out->platform_bytes)||
 !number_line(context,context_size,&at,"section_count=",1,MB_CONFIG_MAX_SECTIONS,&count))return fail_v2(out,MB_CONFIG_REQUEST_INVALID);
 out->target_selection=MB_CONFIG_TARGET_UNREAL_PROJECT_PLATFORM;c->section_count=(uint32_t)count;
 for(uint32_t i=0;i<c->section_count;i++)if(!value(context,context_size,&at,"section=",&f)||!section(f,c,i))return fail_v2(out,MB_CONFIG_REQUEST_INVALID);
 if(!line(context,context_size,&at,&f)||!equal(f,"end")||at!=context_size)return fail_v2(out,MB_CONFIG_REQUEST_INVALID);
 c->status=MB_CONFIG_REQUEST_OK;c->complete=1;out->status=MB_CONFIG_REQUEST_OK;out->complete=1;return MB_CONFIG_REQUEST_OK;
}
