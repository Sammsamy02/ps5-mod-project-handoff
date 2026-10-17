#include "none_packer.h"
#include "blake3.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(x) do{checks++;if(!(x)){failures++;fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);}}while(0)
typedef struct control_io {uint8_t data[65600];uint64_t end;uint32_t reads,writes,fail_write;} control_io;
static uint8_t byte(uint32_t row,uint64_t offset){return (uint8_t)((offset+row*17)%251);}
static int read_chunk(void *v,uint32_t row,uint64_t at,uint8_t *dst,uint32_t n,uint32_t *actual) {
 control_io *io=v;io->reads++;for(uint32_t i=0;i<n;i++)dst[i]=byte(row,at+i);*actual=n;return 0;
}
static int write_ucas(void *v,uint64_t at,const uint8_t *src,uint32_t n,uint32_t *actual) {
 control_io *io=v;io->writes++;if(at>sizeof(io->data)||n>sizeof(io->data)-at)return -8;
 if(io->writes==io->fail_write){n/=2;memcpy(io->data+at,src,n);*actual=n;if(at+n>io->end)io->end=at+n;return -9;}
 memcpy(io->data+at,src,n);*actual=n;if(at+n>io->end)io->end=at+n;return 0;
}
static uint64_t little(const uint8_t *p,size_t n){uint64_t v=0;for(size_t i=n;i;i--)v=(v<<8)|p[i-1];return v;}
static uint64_t big(const uint8_t *p,size_t n){uint64_t v=0;for(size_t i=0;i<n;i++)v=(v<<8)|p[i];return v;}
static int filled(const uint8_t *p,size_t n,uint8_t value){for(size_t i=0;i<n;i++)if(p[i]!=value)return 0;return 1;}
int main(void) {
 static uint8_t toc[4096],transfer[65536],directory[]={1,2,3,4,5};static uint32_t sort[2];static control_io io;
 mb_iostore_none_chunk_v1 chunks[2]={{{9,0,0,0,0,0,0,0,0,0,0,1},65537},{{1,0,0,0,0,0,0,0,0,0,0,6},2}};
 mb_iostore_none_params_v1 p={0};p.struct_size=sizeof(p);p.abi_version=1;p.chunks=chunks;p.chunk_count=2;
 memcpy(p.container_id,"CIDBYTES",8);p.directory_index=directory;p.directory_bytes=sizeof(directory);p.io_context=&io;p.read_chunk=read_chunk;p.write_ucas=write_ucas;
 mb_iostore_none_workspace_v1 w={sizeof(w),1,toc,sizeof(toc),transfer,sizeof(transfer),sort,2};mb_iostore_none_result_v1 r={0};
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_OK&&r.complete&&r.stage==MB_IOSTORE_NONE_COMPLETE);
 CHECK(r.chunk_count==2&&r.block_count==3&&r.toc_bytes==289&&r.content_bytes==65539&&r.ucas_bytes==65553&&r.padding_bytes==14);
 CHECK(io.end==65553&&r.read_calls==3&&r.write_calls==4&&r.requested_read_bytes==65539&&r.requested_write_bytes==65553);
 CHECK(!memcmp(toc,"-==--==--==--==-",16)&&toc[16]==8&&little(toc+20,4)==144&&little(toc+24,4)==2&&little(toc+28,4)==3);
 CHECK(little(toc+32,4)==12&&little(toc+36,4)==0&&little(toc+40,4)==32&&little(toc+44,4)==65536&&little(toc+48,4)==5&&little(toc+52,4)==1);
 CHECK(!memcmp(toc+56,p.container_id,8)&&toc[80]==8&&little(toc+84,4)==1&&little(toc+88,8)==UINT64_MAX&&little(toc+96,4)==2);
 CHECK(filled(toc+17,3,0)&&filled(toc+64,16,0)&&filled(toc+81,3,0)&&filled(toc+100,44,0));
 CHECK(!memcmp(toc+144,chunks[0].raw_id,12)&&!memcmp(toc+156,chunks[1].raw_id,12)&&sort[0]==1&&sort[1]==0);
 CHECK(big(toc+168,5)==0&&big(toc+173,5)==65537&&big(toc+178,5)==131072&&big(toc+183,5)==2);
 CHECK((int32_t)little(toc+188,4)==-3&&little(toc+192,4)==0&&little(toc+196,4)==1);
 CHECK(little(toc+200,5)==0&&little(toc+205,3)==65536&&little(toc+208,3)==65536&&toc[211]==0);
 CHECK(little(toc+212,5)==65536&&little(toc+217,3)==1&&little(toc+220,3)==1&&toc[223]==0);
 CHECK(little(toc+224,5)==65537&&little(toc+229,3)==2&&little(toc+232,3)==2&&toc[235]==0&&!memcmp(toc+236,directory,5));
 for(uint32_t row=0;row<2;row++) {
  blake3_hasher hash;uint8_t expected[20],raw[65537];for(uint64_t i=0;i<chunks[row].bytes;i++)raw[i]=byte(row,i);
  blake3_hasher_init(&hash);blake3_hasher_update(&hash,raw,(size_t)chunks[row].bytes);blake3_hasher_finalize(&hash,expected,20);
  CHECK(!memcmp(toc+241+row*24,expected,20)&&filled(toc+261+row*24,4,0));
 }
 CHECK(io.data[65536]==byte(0,65536)&&io.data[65537]==byte(1,0)&&io.data[65538]==byte(1,1)&&filled(io.data+65539,14,0));
 mb_iostore_none_result_v1 success=r;uint8_t id[12];memcpy(id,chunks[1].raw_id,12);memcpy(chunks[1].raw_id,chunks[0].raw_id,12);
 memset(toc,0xa5,sizeof(toc));memset(&io,0,sizeof(io));
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_DUPLICATE_ID&&!r.complete&&!io.reads&&!io.writes&&filled(toc,sizeof(toc),0xa5));
 memcpy(chunks[1].raw_id,id,12);chunks[1].bytes=MB_IOSTORE_NONE_MAX_PAYLOAD_BYTES;
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_LIMIT&&!r.complete&&!io.reads&&!io.writes&&filled(toc,sizeof(toc),0xa5));
 chunks[1].bytes=2;w.toc_capacity=288;
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_BUFFER_TOO_SMALL&&!r.complete&&r.toc_bytes==success.toc_bytes&&!io.reads&&!io.writes&&filled(toc,sizeof(toc),0xa5));
 w.toc_capacity=sizeof(toc);chunks[1].raw_id[11]=4;
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_INVALID_CHUNK&&!r.complete&&!io.reads&&!io.writes&&filled(toc,sizeof(toc),0xa5));
 chunks[1].raw_id[11]=6;chunks[1].bytes=0;
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_INVALID_CHUNK&&!r.complete&&!io.reads&&!io.writes);chunks[1].bytes=2;
 p.chunk_count=0;CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_INVALID_CHUNK&&!r.complete&&!io.reads&&!io.writes);p.chunk_count=2;
 mb_iostore_none_result_v1 before=r;w.toc=(uint8_t *)chunks;w.toc_capacity=sizeof(chunks);mb_iostore_none_chunk_v1 snapshot[2];memcpy(snapshot,chunks,sizeof(chunks));
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_BAD_ARGUMENT&&!memcmp(&before,&r,sizeof(r))&&!memcmp(snapshot,chunks,sizeof(chunks))&&!io.reads&&!io.writes);
 w.toc=toc;w.toc_capacity=sizeof(toc);memset(&io,0,sizeof(io));io.fail_write=3;
 CHECK(mb_iostore_none_pack_v1(&p,&w,&r)==MB_IOSTORE_NONE_WRITE_FAILED&&!r.complete&&r.failed_row==1&&r.failed_block==2&&r.failed_offset==65537&&r.callback_actual==1&&r.callback_status==-9&&io.end==65538);
 CHECK(r.stage==MB_IOSTORE_NONE_PAYLOAD&&r.read_calls==3&&r.write_calls==3);
 printf("checks=%u failures=%u content_bytes=%llu ucas_bytes=%llu toc_bytes=%llu blocks=%u workspace_sort_bytes=%zu\n",checks,failures,(unsigned long long)success.content_bytes,(unsigned long long)success.ucas_bytes,(unsigned long long)success.toc_bytes,success.block_count,sizeof(sort));return failures?1:0;
}
