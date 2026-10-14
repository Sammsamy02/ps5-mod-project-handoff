#include "none_range_reader.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,failures;
static void check(int yes,const char *label) {checks++;if(!yes){failures++;fprintf(stderr,"FAIL %s\n",label);}}
static void le(uint8_t *p,uint64_t v,unsigned n) {for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(i*8));}
static void be40(uint8_t *p,uint64_t v) {for(unsigned i=0;i<5;i++)p[4-i]=(uint8_t)(v>>(i*8));}
static int filled(const uint8_t *p,size_t n,uint8_t v) {for(size_t i=0;i<n;i++)if(p[i]!=v)return 0;return 1;}
static void make_toc(uint8_t *p) {
 memset(p,0,214);memcpy(p,"-==--==--==--==-",16);p[16]=8;
 le(p+20,144,4);le(p+24,1,4);le(p+28,2,4);le(p+32,12,4);
 le(p+40,32,4);le(p+44,65536,4);le(p+52,2,4);p[80]=8;le(p+88,65536,8);
 p[144]=9;p[155]=1;be40(p+156,65530);be40(p+161,12);
 le(p+166,0,5);le(p+171,65536,3);le(p+174,65536,3);
 le(p+178,65536,5);le(p+183,16,3);le(p+186,16,3);
}
typedef struct memory {uint8_t *p[2];size_t bytes[2];unsigned calls,failed_call;int failure_code;} memory;
static int memory_read(void *opaque,uint32_t part,uint64_t off,uint8_t *out,size_t n,size_t *actual) {
 memory *m=opaque;m->calls++;
 if(part>1||off>m->bytes[part]||n>m->bytes[part]-(size_t)off)return -1;
 *actual=m->calls==m->failed_call?n-1:n;memcpy(out,m->p[part]+(size_t)off,*actual);
 return m->calls==m->failed_call?m->failure_code:0;
}
static void synthetic(void) {
 _Alignas(mb_none_range_result_v1) uint8_t toc[214],bad[214];
 uint8_t part0[65536],part1[16],out[20],expected[12],snapshot[214];
 uint64_t lengths[2]={65536,16};make_toc(toc);
 for(size_t i=0;i<sizeof(part0);i++)part0[i]=(uint8_t)(i%251);
 for(size_t i=0;i<sizeof(part1);i++)part1[i]=(uint8_t)(224+i);
 memcpy(expected,part0+65530,6);memcpy(expected+6,part1,6);
 memory m={{part0,part1},{sizeof(part0),sizeof(part1)},0,0,0};
 mb_none_range_source_v1 s={sizeof(s),1,{toc,sizeof(toc),0},lengths,2,0,memory_read,&m};
 mb_none_range_plan_v1 plan,saved;mb_none_range_result_v1 r;mb_none_range_status_v1 st;
 memset(&plan,0xa5,sizeof(plan));
 st=mb_iostore_none_plan_v1(&s,0,&plan,&r);
 check(st==MB_NONE_RANGE_OK&&r.full_chunk_coverage_scanned&&r.selected_blocks_checked==2&&!r.read_calls&&!m.calls,"full chunk plan covers two blocks without provider");
 saved=plan;memset(out,0xaa,sizeof(out));
 st=mb_iostore_none_range_v1(&plan,2,8,out,sizeof(out),&r);
 check(st==MB_NONE_RANGE_OK&&r.range_complete&&r.read_calls==2&&r.read_bytes==8&&!memcmp(out,expected+2,8),"bounded cross-block two-partition slice");
 check(r.selected_blocks_checked==2&&r.plan_geometry_rebound&&!r.full_chunk_coverage_scanned,"range validates only requested blocks and rebinds geometry");
 check(filled(out+8,12,0xaa),"success output tail unchanged");
 check(!r.full_toc_validated&&!r.payload_hash_verified&&!r.authenticity_verified&&!r.runtime_supported&&!r.installation_supported,"authority flags clear");
 unsigned before=m.calls;st=mb_iostore_none_range_v1(&plan,12,0,NULL,0,&r);
 check(st==MB_NONE_RANGE_OK&&r.range_complete&&!r.read_calls&&!r.selected_blocks_checked&&m.calls==before,"zero-byte EOF slice without read");
 memset(out,0xaa,sizeof(out));before=m.calls;
 st=mb_iostore_none_range_v1(&plan,13,0,out,sizeof(out),&r);
 check(st==MB_NONE_RANGE_OUT_OF_RANGE&&!r.range_complete&&m.calls==before&&filled(out,sizeof(out),0xaa),"offset beyond EOF refusal before provider");
 st=mb_iostore_none_range_v1(&plan,0,12,out,11,&r);
 check(st==MB_NONE_RANGE_BUFFER_TOO_SMALL&&!r.range_complete&&m.calls==before&&filled(out,sizeof(out),0xaa),"short capacity refusal before provider");
 st=mb_iostore_none_range_v1(&plan,0,65537,out,sizeof(out),&r);
 check(st==MB_NONE_RANGE_RESOURCE_LIMIT&&!r.range_complete&&m.calls==before&&filled(out,sizeof(out),0xaa),"range cap refusal before provider");
 m.calls=0;m.failed_call=2;m.failure_code=0;memset(out,0xaa,sizeof(out));
 st=mb_iostore_none_range_v1(&plan,0,12,out,sizeof(out),&r);
 check(st==MB_NONE_RANGE_SHORT_READ&&!r.range_complete&&r.read_calls==2&&r.read_bytes==6&&r.failed_partition==1&&r.failed_block==1&&r.failed_received==5,"second callback short read recorded");
 check(filled(out,12,0)&&filled(out+12,8,0xaa),"short read wipes entire requested span and preserves tail");
 m.calls=0;m.failure_code=-7;memset(out,0xaa,sizeof(out));
 st=mb_iostore_none_range_v1(&plan,0,12,out,sizeof(out),&r);
 check(st==MB_NONE_RANGE_READ_ERROR&&!r.range_complete&&r.provider_code==-7&&r.failed_received==5,"second callback provider failure recorded");
 check(filled(out,12,0)&&filled(out+12,8,0xaa),"provider failure wipes requested span and preserves tail");
 m.calls=0;m.failed_call=0;m.failure_code=0;
 memcpy(bad,toc,sizeof(bad));le(bad+183,5,3);le(bad+186,5,3);s.toc.data=bad;
 st=mb_iostore_none_plan_v1(&s,0,&plan,&r);
 check(st==MB_NONE_RANGE_METADATA_REJECTED&&!r.full_chunk_coverage_scanned&&!m.calls&&!memcmp(&plan,&saved,sizeof(plan)),"inadequate full-chunk coverage refuses plan without modifying previous plan");
 memcpy(bad,toc,sizeof(bad));bad[155]=4;
 st=mb_iostore_none_plan_v1(&s,0,&plan,&r);
 check(st==MB_NONE_RANGE_UNSUPPORTED&&!m.calls&&!memcmp(&plan,&saved,sizeof(plan)),"memorymapped selected chunk refused");
 s.toc.data=toc;memcpy(snapshot,toc,sizeof(toc));
 st=mb_iostore_none_plan_v1(&s,0,&plan,(mb_none_range_result_v1 *)toc);
 check(st==MB_NONE_RANGE_BAD_ARGUMENT&&!memcmp(snapshot,toc,sizeof(toc))&&!memcmp(&plan,&saved,sizeof(plan))&&!m.calls,"planning result alias immutable TOC no-write");
 st=mb_iostore_none_range_v1(&plan,0,12,out,sizeof(out),(mb_none_range_result_v1 *)&plan);
 check(st==MB_NONE_RANGE_BAD_ARGUMENT&&!memcmp(&plan,&saved,sizeof(plan))&&!m.calls,"range result alias plan no-write");
 st=mb_iostore_none_range_v1(&plan,0,12,toc,20,&r);
 check(st==MB_NONE_RANGE_BAD_ARGUMENT&&!memcmp(snapshot,toc,sizeof(toc))&&!m.calls,"range output alias TOC no-write");
 mb_none_range_plan_v1 mismatch=plan;mismatch.logical_bytes++;
 st=mb_iostore_none_range_v1(&mismatch,0,12,out,sizeof(out),&r);
 check(st==MB_NONE_RANGE_PLAN_MISMATCH&&!r.range_complete&&!m.calls,"modified plan decoded geometry refused without cookie");
 printf("{\"synthetic_checks\":%u,\"failures\":%u,\"plan_bytes\":%zu,\"cross_block_partitions\":true,\"failure_wiping\":true,\"aliases_no_write\":true}\n",checks,failures,sizeof(plan));
}
typedef struct file_provider {FILE *stream;uint32_t calls;uint64_t bytes;} file_provider;
static int file_read(void *opaque,uint32_t part,uint64_t off,uint8_t *out,size_t n,size_t *actual) {
 file_provider *f=opaque;if(part||off>INT64_MAX||fseek(f->stream,(long)off,SEEK_SET))return -1;
 f->calls++;*actual=fread(out,1,n,f->stream);f->bytes+=*actual;return ferror(f->stream)?-1:0;
}
static uint8_t *load(const char *path,size_t cap,size_t *bytes) {
 FILE *f=fopen(path,"rb");long n;if(!f)return NULL;
 if(fseek(f,0,SEEK_END)||(n=ftell(f))<1||(uint64_t)n>cap||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
 uint8_t *p=malloc((size_t)n);if(!p||fread(p,1,(size_t)n,f)!=(size_t)n||fclose(f)){free(p);return NULL;}
 *bytes=(size_t)n;return p;
}
static int actual(int argc,char **argv) {
 if(argc!=6)return 2;
 char *end;uint64_t len=strtoull(argv[3],&end,10);if(!len||*end)return 2;
 unsigned long parsed=strtoul(argv[4],&end,10);if(*end||parsed>UINT32_MAX)return 2;
 size_t tn=0,en=0;uint8_t *toc=load(argv[1],MB_IOSTORE_MAX_TOC_BYTES,&tn);
 uint8_t *expected=load(argv[5],MB_NONE_RANGE_MAX_CHUNK_BYTES,&en);FILE *stream=fopen(argv[2],"rb");
 if(!toc||!expected||!stream)return 2;
 if(fseek(stream,0,SEEK_END)||ftell(stream)<0||(uint64_t)ftell(stream)!=len)return 2;
 file_provider provider={stream,0,0};mb_none_range_source_v1 s={sizeof(s),1,{toc,tn,0},&len,1,0,file_read,&provider};
 mb_none_range_plan_v1 plan;mb_none_range_result_v1 r;
 mb_none_range_status_v1 st=mb_iostore_none_plan_v1(&s,(uint32_t)parsed,&plan,&r);
 uint32_t planned_blocks=r.selected_blocks_checked;int planned=st==MB_NONE_RANGE_OK&&r.full_chunk_coverage_scanned&&!provider.calls;
 if(!planned||plan.logical_bytes!=en)return 1;
 uint8_t output[MB_NONE_RANGE_MAX_BYTES];uint64_t offset=0;uint32_t ranges=0,checked_blocks=0;int exact=1;
 while(offset<plan.logical_bytes) {
  uint32_t n=(uint32_t)(plan.logical_bytes-offset);if(n>65535)n=65535;
  memset(output,0xa5,sizeof(output));
  st=mb_iostore_none_range_v1(&plan,offset,n,output,sizeof(output),&r);ranges++;checked_blocks+=r.selected_blocks_checked;
  if(st!=MB_NONE_RANGE_OK||!r.range_complete||r.full_chunk_coverage_scanned||r.read_bytes!=n||
   memcmp(output,expected+(size_t)offset,n)||!filled(output+n,sizeof(output)-n,0xa5)) {exact=0;break;}
  offset+=n;
 }
 printf("{\"actual_status\":%u,\"selected_row\":%lu,\"chunk_bytes\":%llu,\"plan_full_chunk_coverage_scanned\":%s,"
        "\"planned_blocks\":%u,\"range_calls\":%u,\"range_blocks_checked\":%u,\"read_calls\":%u,"
        "\"ucas_bytes_read\":%llu,\"raw_reference_byte_equal\":%s,\"no_full_scan_per_range\":true,"
        "\"full_toc_validated\":0,\"payload_hash_verified\":0,\"runtime_supported\":0,\"installation_supported\":0}\n",
        (unsigned)st,parsed,(unsigned long long)plan.logical_bytes,planned?"true":"false",planned_blocks,ranges,
        checked_blocks,provider.calls,(unsigned long long)provider.bytes,exact&&offset==en?"true":"false");
 free(toc);free(expected);fclose(stream);return exact&&offset==en?0:1;
}
int main(int argc,char **argv) {
 if(argc==2&&!strcmp(argv[1],"--synthetic")){synthetic();return failures?1:0;}
 return actual(argc,argv);
}
