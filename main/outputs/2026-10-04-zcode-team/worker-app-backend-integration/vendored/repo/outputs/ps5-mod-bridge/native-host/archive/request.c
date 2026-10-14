#include "request.h"
#include <stdint.h>
#include <string.h>

typedef struct span_v1 { const void *ptr; size_t size; } span_v1;
typedef struct field_v1 { const uint8_t *ptr; size_t size; } field_v1;

static int overlaps(span_v1 a,span_v1 b) {
    uintptr_t x=(uintptr_t)a.ptr,y=(uintptr_t)b.ptr;
    if(!a.size||!b.size)return 0;
    return x<y+b.size&&y<x+a.size;
}
static int span_valid(span_v1 s) {
    uintptr_t p=(uintptr_t)s.ptr;
    return !s.size||(s.ptr&&p<=UINTPTR_MAX-s.size);
}
static int disjoint(const span_v1 *s,unsigned count) {
    unsigned i,j;for(i=0;i<count;i++)for(j=0;j<i;j++)if(overlaps(s[i],s[j]))return 0;return 1;
}
static int fail_request(mb_archive_request_v1 *out,int status) {
    memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);
    out->abi_version=MB_ARCHIVE_REQUEST_ABI_V1;out->status=(uint32_t)status;return status;
}
static int fail_ref(mb_archive_request_ref_v1 *out,int status) {
    memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);
    out->abi_version=MB_ARCHIVE_REQUEST_ABI_V1;out->status=(uint32_t)status;return status;
}
static int next_line(const uint8_t *bytes,size_t size,size_t *offset,field_v1 *line) {
    size_t start=*offset,i;
    if(start>=size)return 0;
    for(i=start;i<size&&bytes[i]!='\n';i++)if(bytes[i]<0x20||bytes[i]>0x7e)return 0;
    if(i==size)return 0;
    line->ptr=bytes+start;line->size=i-start;*offset=i+1;return 1;
}
static int line_is(field_v1 line,const char *expected) {
    size_t n=strlen(expected);return line.size==n&&memcmp(line.ptr,expected,n)==0;
}
static int next_value(const uint8_t *bytes,size_t size,size_t *offset,const char *key,field_v1 *value) {
    field_v1 line;size_t n=strlen(key);
    if(!next_line(bytes,size,offset,&line)||line.size<n||memcmp(line.ptr,key,n))return 0;
    value->ptr=line.ptr+n;value->size=line.size-n;return 1;
}
static int lowercase_hex(field_v1 value,size_t digits,uint8_t *decoded) {
    size_t i;uint8_t nibble;
    if(value.size!=digits)return 0;
    for(i=0;i<digits;i++) {
        unsigned char c=value.ptr[i];
        if(c>='0'&&c<='9')nibble=(uint8_t)(c-'0');
        else if(c>='a'&&c<='f')nibble=(uint8_t)(c-'a'+10);
        else return 0;
        if(decoded&&(i&1u)==0)decoded[i/2]=(uint8_t)(nibble<<4);
        else if(decoded)decoded[i/2]|=nibble;
    }
    return 1;
}
static int parse_decimal(field_v1 value,uint64_t *out) {
    size_t i;uint64_t n=0;
    if(!value.size||(value.size>1&&value.ptr[0]=='0'))return 0;
    for(i=0;i<value.size;i++) {
        unsigned char c=value.ptr[i];uint64_t d;
        if(c<'0'||c>'9')return 0;
        d=(uint64_t)(c-'0');if(n>(UINT64_MAX-d)/10)return 0;n=n*10+d;
    }
    if(!n||n>MB_ARCHIVE_MAX_RAW_BYTES)return 0;*out=n;return 1;
}
static int component(field_v1 value,char *out,size_t capacity) {
    size_t i;
    if(!value.size||value.size>=capacity||(value.size==1&&value.ptr[0]=='.')||
       (value.size==2&&value.ptr[0]=='.'&&value.ptr[1]=='.')||value.ptr[value.size-1]=='.')return 0;
    for(i=0;i<value.size;i++) {
        unsigned char c=value.ptr[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
        out[i]=(char)c;
    }
    out[value.size]=0;return 1;
}
static int profile_text(field_v1 value,char *out,size_t capacity,int allow_empty) {
    size_t i;
    if(value.size>=capacity||(!value.size&&!allow_empty))return 0;
    for(i=0;i<value.size;i++) {
        unsigned char c=value.ptr[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;
        out[i]=(char)c;
    }
    out[value.size]=0;return 1;
}
static int take_exact(const uint8_t *bytes,size_t size,size_t *offset,
    const char *key,size_t hex_digits,uint8_t *decoded);
static void derived(char *out,const char *prefix,const char *id,const char *suffix);
static int parse_ref(const uint8_t *bytes,size_t size,mb_archive_request_ref_v1 *out) {
    size_t pos=0;field_v1 line,value;char expected[48];
    if(!size||!next_line(bytes,size,&pos,&line)||!line_is(line,"MBARREQ1"))return MB_ARCHIVE_REQUEST_INVALID;
    if(!next_value(bytes,size,&pos,"id=",&value)||!lowercase_hex(value,32,NULL))return MB_ARCHIVE_REQUEST_INVALID;
    memcpy(out->request_id,value.ptr,32);out->request_id[32]=0;
    if(!next_value(bytes,size,&pos,"source=",&value)||!component(value,out->source_ref,sizeof(out->source_ref)))return MB_ARCHIVE_REQUEST_INVALID;
    if(!next_value(bytes,size,&pos,"context=",&value)||!component(value,out->context_ref,sizeof(out->context_ref)))return MB_ARCHIVE_REQUEST_INVALID;
    if(!take_exact(bytes,size,&pos,"context_sha256=",64,out->context_sha256))return MB_ARCHIVE_REQUEST_INVALID;
    if(!next_value(bytes,size,&pos,"quarantine=",&value)||!component(value,out->quarantine_ref,sizeof(out->quarantine_ref)))return MB_ARCHIVE_REQUEST_INVALID;
    derived(expected,"q-",out->request_id,"");if(strcmp(expected,out->quarantine_ref))return MB_ARCHIVE_REQUEST_INVALID;
    if(!next_value(bytes,size,&pos,"receipt=",&value)||!component(value,out->receipt_ref,sizeof(out->receipt_ref)))return MB_ARCHIVE_REQUEST_INVALID;
    derived(expected,"r-",out->request_id,".json");if(strcmp(expected,out->receipt_ref))return MB_ARCHIVE_REQUEST_INVALID;
    derived(out->claim_ref,"claim-",out->request_id,"");
    if(!strcmp(out->source_ref,out->context_ref)||!strcmp(out->source_ref,"request.v1")||
       !strcmp(out->context_ref,"request.v1")||!strcmp(out->source_ref,out->claim_ref)||
       !strcmp(out->source_ref,out->quarantine_ref)||!strcmp(out->source_ref,out->receipt_ref)||
       !strcmp(out->context_ref,out->claim_ref)||!strcmp(out->context_ref,out->quarantine_ref)||
       !strcmp(out->context_ref,out->receipt_ref))return MB_ARCHIVE_REQUEST_INVALID;
    if(!next_line(bytes,size,&pos,&line)||!line_is(line,"end")||pos!=size)return MB_ARCHIVE_REQUEST_INVALID;
    out->status=MB_ARCHIVE_REQUEST_OK;out->complete=1;return MB_ARCHIVE_REQUEST_OK;
}
static int take_exact(const uint8_t *bytes,size_t size,size_t *offset,
    const char *key,size_t hex_digits,uint8_t *decoded) {
    field_v1 value;return next_value(bytes,size,offset,key,&value)&&lowercase_hex(value,hex_digits,decoded);
}
static void derived(char *out,const char *prefix,const char *id,const char *suffix) {
    size_t p=strlen(prefix),i;memcpy(out,prefix,p);
    for(i=0;i<32;i++)out[p+i]=id[i];strcpy(out+p+32,suffix);
}

int mb_archive_request_ref_decode_v1(const uint8_t *request_bytes,size_t request_size,
    mb_archive_request_ref_v1 *out) {
    span_v1 s[2]={{request_bytes,request_size},{out,sizeof(*out)}};int rc;
    if(!out||!request_bytes)return MB_ARCHIVE_REQUEST_BAD_ARGUMENT;
    if(request_size>MB_ARCHIVE_REQUEST_MAX_BYTES)return MB_ARCHIVE_REQUEST_LIMIT;
    if(!span_valid(s[0])||!span_valid(s[1])||!disjoint(s,2)||
       (uintptr_t)out%_Alignof(mb_archive_request_ref_v1))return MB_ARCHIVE_REQUEST_BAD_ARGUMENT;
    memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=MB_ARCHIVE_REQUEST_ABI_V1;
    rc=parse_ref(request_bytes,request_size,out);if(rc)return fail_ref(out,rc);
    return MB_ARCHIVE_REQUEST_OK;
}

int mb_archive_request_decode_v1(const uint8_t *request_bytes,size_t request_size,
    const uint8_t *context_bytes,size_t context_size,const uint8_t observed_context_sha256[32],
    mb_archive_request_v1 *out) {
    span_v1 spans[4]={{request_bytes,request_size},{context_bytes,context_size},
        {observed_context_sha256,32},{out,sizeof(*out)}};
    size_t pos=0;field_v1 line,value;mb_archive_request_ref_v1 ref;int rc;
    if(!out||!request_bytes||!context_bytes||!observed_context_sha256)return MB_ARCHIVE_REQUEST_BAD_ARGUMENT;
    if(request_size>MB_ARCHIVE_REQUEST_MAX_BYTES||context_size>MB_ARCHIVE_REQUEST_MAX_BYTES)return MB_ARCHIVE_REQUEST_LIMIT;
    if(!span_valid(spans[0])||!span_valid(spans[1])||!span_valid(spans[2])||!span_valid(spans[3])||
       !disjoint(spans,4)||(uintptr_t)out%_Alignof(mb_archive_request_v1))return MB_ARCHIVE_REQUEST_BAD_ARGUMENT;
    memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=MB_ARCHIVE_REQUEST_ABI_V1;
    if(!context_size)return fail_request(out,MB_ARCHIVE_REQUEST_INVALID);
    memset(&ref,0,sizeof(ref));ref.struct_size=sizeof(ref);ref.abi_version=MB_ARCHIVE_REQUEST_ABI_V1;
    rc=parse_ref(request_bytes,request_size,&ref);if(rc)return fail_request(out,rc);
    if(memcmp(ref.context_sha256,observed_context_sha256,32))return fail_request(out,MB_ARCHIVE_REQUEST_CONTEXT_MISMATCH);
    out->status=MB_ARCHIVE_REQUEST_OK;
    memcpy(out->context_sha256,ref.context_sha256,32);
    memcpy(out->request_id,ref.request_id,sizeof(out->request_id));
    memcpy(out->source_ref,ref.source_ref,sizeof(out->source_ref));
    memcpy(out->context_ref,ref.context_ref,sizeof(out->context_ref));
    memcpy(out->quarantine_ref,ref.quarantine_ref,sizeof(out->quarantine_ref));
    memcpy(out->receipt_ref,ref.receipt_ref,sizeof(out->receipt_ref));
    memcpy(out->claim_ref,ref.claim_ref,sizeof(out->claim_ref));
    memcpy(out->nonce,ref.request_id,sizeof(out->nonce));
    if(!next_line(context_bytes,context_size,&pos,&line)||!line_is(line,"MBARCTX1"))return fail_request(out,MB_ARCHIVE_REQUEST_INVALID);
    if(!next_value(context_bytes,context_size,&pos,"source_bytes=",&value)||!parse_decimal(value,&out->source_bytes))
        return fail_request(out,MB_ARCHIVE_REQUEST_INVALID);
    if(!take_exact(context_bytes,context_size,&pos,"source_sha256=",64,out->source_sha256)||
       !take_exact(context_bytes,context_size,&pos,"target_token=",64,out->target_token))return fail_request(out,MB_ARCHIVE_REQUEST_INVALID);
    if(!next_value(context_bytes,context_size,&pos,"prefix=",&value)||!profile_text(value,out->prefix,sizeof(out->prefix),0))
        return fail_request(out,MB_ARCHIVE_REQUEST_INVALID);
    if(!next_value(context_bytes,context_size,&pos,"suffix=",&value)||!profile_text(value,out->suffix,sizeof(out->suffix),1))
        return fail_request(out,MB_ARCHIVE_REQUEST_INVALID);
    if(!next_line(context_bytes,context_size,&pos,&line)||!line_is(line,"end")||pos!=context_size)
        return fail_request(out,MB_ARCHIVE_REQUEST_INVALID);
    out->complete=1;return MB_ARCHIVE_REQUEST_OK;
}
