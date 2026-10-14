#include "pak_v3_none_reader.h"
#include <mbedtls/sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t image[65536],baseline[65536],index_data[65536],names[32768],transfer[65536],payload[65536];
static mb_pak_v3_entry_v1 entries[32];static uint32_t order[32];
static size_t image_bytes;static unsigned checks,failed;static int short_payload;
#define CHECK(x) do {++checks;if(!(x)){++failed;fprintf(stderr,"FAIL line%u: %s\n",(unsigned)__LINE__,#x);}} while(0)
static uint64_t le64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=(uint64_t)p[i]<<(8*i);return v;}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(v>>(8*i));}
static pak_provider_status read_memory(void *ctx,uint64_t offset,uint8_t *dst,uint32_t n,uint32_t *actual){
 (void)ctx;*actual=0;if(offset>image_bytes||n>image_bytes-offset)return PAK_PROVIDER_CONTRACT_ERROR;
 if(short_payload&&offset==53){if(n>1)memcpy(dst,image+offset,n-1);*actual=n-1;return PAK_PROVIDER_OK;}
 memcpy(dst,image+offset,n);*actual=n;return PAK_PROVIDER_OK;
}
static int zero(const void *p,size_t n){const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i])return 0;return 1;}
static void restore(void){memcpy(image,baseline,image_bytes);short_payload=0;}
static void rehash_index(void){uint8_t *footer=image+image_bytes-44;uint64_t at=le64(footer+8),n=le64(footer+16);
 CHECK(at<=image_bytes&&n<=image_bytes-at);CHECK(!mbedtls_sha1(image+(size_t)at,(size_t)n,footer+24));}
static mb_pak_v3_status run(uint32_t want,mb_pak_v3_result_v1 *r){
 pak_source_v1 s={sizeof(s),1,image_bytes,77,PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE,0,NULL,read_memory};
 mb_pak_v3_limits_v1 l;mb_pak_v3_limits_v1_default(&l);
 mb_pak_v3_buffers_v1 b={sizeof(b),1,index_data,sizeof(index_data),entries,32,names,sizeof(names),order,32,transfer,sizeof(transfer),payload,sizeof(payload)};
 return mb_pak_v3_read_none_v1(&s,&l,want,0,&b,r);
}
static void rejected(mb_pak_v3_status expected,mb_pak_v3_stage stage){
 mb_pak_v3_result_v1 r;memset(&r,0xa5,sizeof(r));
 CHECK(run(1,&r)==expected);CHECK(r.status==expected&&r.stage==stage);
 CHECK(!r.metadata_complete&&!r.selected_complete&&!r.selected_bytes&&!r.installation_supported);
 CHECK(zero(entries,sizeof(entries))&&zero(names,sizeof(names))&&zero(payload,sizeof(payload)));
 CHECK(zero(index_data,sizeof(index_data))&&zero(transfer,sizeof(transfer))&&zero(order,sizeof(order)));
 CHECK(r.read_calls>0&&r.requested_bytes>0);
}
static void hex(const uint8_t *d){for(unsigned i=0;i<20;++i)printf("%02x",d[i]);}
int main(int argc,char **argv){
 if(argc!=2)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 2;
 if(fseek(f,0,SEEK_END))return 2;long n=ftell(f);if(n<44||(unsigned long)n>sizeof(image)||fseek(f,0,SEEK_SET))return 2;
 image_bytes=(size_t)n;if(fread(image,1,image_bytes,f)!=image_bytes||fgetc(f)!=EOF||ferror(f)||fclose(f))return 2;
 memcpy(baseline,image,image_bytes);mb_pak_v3_result_v1 actual;
 CHECK(run(1,&actual)==MB_PAK_V3_OK);CHECK(actual.metadata_complete&&actual.selected_complete&&actual.file_count==1);
 CHECK(!actual.installation_supported&&actual.snapshot_token==77&&actual.selected_bytes==entries[0].payload_bytes);
 CHECK(entries[0].data_offset==entries[0].archive_offset+53&&actual.selected_bytes==5643);
 CHECK(!memcmp(payload,baseline+entries[0].data_offset,(size_t)actual.selected_bytes));
 CHECK(!memcmp(actual.selected_sha1,entries[0].sha1,20));
 CHECK(zero(index_data,sizeof(index_data))&&zero(transfer,sizeof(transfer))&&zero(order,sizeof(order)));
 uint64_t index_offset=actual.index_offset;size_t p=(size_t)index_offset;
 uint32_t mount_length=le32(image+p);p+=4+mount_length;p+=4;
 uint32_t path_length=le32(image+p);p+=4+path_length;size_t entry_offset=p;
 restore();put64(image+image_bytes-44+8,index_offset+1);rejected(MB_PAK_V3_MALFORMED,MB_PAK_V3_FOOTER);
 restore();image[image_bytes-1]^=1;rejected(MB_PAK_V3_DIGEST_MISMATCH,MB_PAK_V3_INDEX);
 restore();put64(image+entry_offset,index_offset);rehash_index();rejected(MB_PAK_V3_MALFORMED,MB_PAK_V3_ENTRIES);
 restore();put32(image+entry_offset+24,1);rehash_index();rejected(MB_PAK_V3_UNSUPPORTED,MB_PAK_V3_ENTRIES);
 restore();image[53]^=1;rejected(MB_PAK_V3_DIGEST_MISMATCH,MB_PAK_V3_PAYLOAD);
 restore();image[28]^=1;rejected(MB_PAK_V3_MALFORMED,MB_PAK_V3_PHYSICAL_HEADERS);
 restore();short_payload=1;rejected(MB_PAK_V3_SHORT_READ,MB_PAK_V3_PAYLOAD);
 restore();mb_pak_v3_result_v1 metadata;
 CHECK(run(0,&metadata)==MB_PAK_V3_OK&&metadata.metadata_complete&&!metadata.selected_complete&&zero(payload,sizeof(payload)));
 /* Known output/input alias must not mutate archive/provider or result. */
 pak_source_v1 s={sizeof(s),1,image_bytes,77,1,0,NULL,read_memory};mb_pak_v3_limits_v1 l;mb_pak_v3_limits_v1_default(&l);
 mb_pak_v3_buffers_v1 b={sizeof(b),1,index_data,sizeof(index_data),entries,32,names,sizeof(names),order,32,transfer,sizeof(transfer),payload,sizeof(payload)};
 b.selected=(uint8_t *)&l;b.selected_capacity=sizeof(l);mb_pak_v3_limits_v1 copy=l;memset(&metadata,0xa5,sizeof(metadata));
 mb_pak_v3_result_v1 unchanged=metadata;CHECK(mb_pak_v3_read_none_v1(&s,&l,1,0,&b,&metadata)==MB_PAK_V3_BAD_ARGUMENT);
 CHECK(!memcmp(&metadata,&unchanged,sizeof(metadata))&&!memcmp(&l,&copy,sizeof(l))&&!memcmp(image,baseline,image_bytes));
 printf("{\"result\":\"%s\",\"checks\":%u,\"failures\":%u,\"archive_bytes\":%zu,\"file_count\":%u,\"selected_bytes\":%llu,\"read_calls\":%u,\"requested_bytes\":%llu,\"index_sha1\":\"",
 failed?"FAIL":"PASS",checks,failed,image_bytes,actual.file_count,(unsigned long long)actual.selected_bytes,actual.read_calls,(unsigned long long)actual.requested_bytes);
 hex(actual.index_sha1);printf("\",\"payload_sha1\":\"");hex(actual.selected_sha1);puts("\",\"installation_supported\":false}");
 return failed?1:0;
}
