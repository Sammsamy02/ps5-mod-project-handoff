#include "toc_overlay.h"
#include <stdio.h>
#include <string.h>

static void le(uint8_t *p,uint64_t n,unsigned width) {
 for(unsigned i=0;i<width;++i){p[i]=(uint8_t)n;n>>=8;}
}
static void be40(uint8_t *p,uint64_t n) {
 for(unsigned i=0;i<5;++i){p[4-i]=(uint8_t)n;n>>=8;}
}
static uint64_t little(const uint8_t *p,unsigned width) {
 uint64_t n=0;for(unsigned i=0;i<width;++i)n|=(uint64_t)p[i]<<(8*i);return n;
}
static void header(uint8_t *p,uint32_t count,uint32_t blocks,uint32_t directory) {
 memcpy(p,"-==--==--==--==-",16);p[16]=8;
 le(p+20,144,4);le(p+24,count,4);le(p+28,blocks,4);le(p+32,12,4);
 le(p+40,32,4);le(p+44,65536,4);le(p+48,directory,4);le(p+52,1,4);
 le(p+56,123,8);p[80]=8;le(p+88,UINT64_MAX,8);
}
static void id(uint8_t *p,uint64_t n,uint8_t type) {le(p,n,8);p[11]=type;}
static void chunk(uint8_t *p,uint64_t off,uint64_t n) {be40(p,off);be40(p+5,n);}
static void block(uint8_t *p,uint64_t off,uint32_t n) {le(p,off,5);le(p+5,n,3);le(p+8,n,3);}
static int all(const uint8_t *p,size_t n,uint8_t value) {
 for(size_t i=0;i<n;++i)if(p[i]!=value)return 0;
 return 1;
}
#define CHECK(x) do{++assertions;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
 uint32_t assertions=0;uint8_t base[310]={0},small[298]={0},directory[4]={0};
 header(base,3,2,4);header(small,3,1,4);
 id(base+144,123,6);id(base+156,10,1);id(base+168,20,1);
 chunk(base+180,0,8);chunk(base+190,65536,8);chunk(base+200,65544,8);
 block(base+210,0,65536);block(base+222,65536,65536);
 for(unsigned i=0;i<3;++i)memset(base+238+i*24,(int)(10+i),20);
 base[238+2*24+20]=2; /* unaffected memory-mapped metadata is preserved */
 id(small+144,30,1);id(small+156,123,6);id(small+168,10,1);
 chunk(small+180,0,8);chunk(small+190,8,8);chunk(small+200,16,8);
 block(small+210,0,24);
 for(unsigned i=0;i<3;++i)memset(small+226+i*24,(int)(20+i),20);
 mb_toc_overlay_params_v1 p={sizeof(p),1,{base,sizeof(base),0},{small,sizeof(small),0},
  {directory,sizeof(directory),0},131072,32};
 mb_toc_overlay_id_row_v1 baserows[3],smallrows[3];uint32_t scratch[3];
 mb_toc_overlay_workspace_v1 w={sizeof(w),1,baserows,smallrows,3,3,scratch,3};
 mb_toc_overlay_result_v1 r;uint8_t out[404];memset(out,0xa5,sizeof(out));
 CHECK(mb_iostore_toc_overlay_v1(&p,&w,NULL,0,&r)==MB_TOC_OVERLAY_OUTPUT_TOO_SMALL);
 CHECK(r.required_output_bytes==388&&!r.metadata_complete&&!r.output_bytes);
 CHECK(r.required_base_rows==3&&r.required_small_rows==3&&r.required_scratch_u32==3);
 CHECK(mb_iostore_toc_overlay_v1(&p,&w,out,387,&r)==MB_TOC_OVERLAY_OUTPUT_TOO_SMALL);
 CHECK(all(out,sizeof(out),0xa5));
 CHECK(mb_iostore_toc_overlay_v1(&p,&w,out,sizeof(out),&r)==MB_TOC_OVERLAY_OK);
 CHECK(r.metadata_complete&&r.output_bytes==388&&r.final_chunks==4&&r.final_blocks==3);
 CHECK(r.replaced_chunks==2&&r.added_chunks==1&&r.unaffected_chunks==1);
 CHECK(r.original_blocks_preserved&&r.unaffected_mappings_preserved);
 CHECK(!r.payload_hash_verified&&!r.authenticity_verified&&!r.runtime_supported&&!r.installation_supported);
 CHECK(!memcmp(out+144,base+144,36)&&!memcmp(out+180,small+144,12));
 CHECK(little(out+24,4)==4&&little(out+28,4)==3&&little(out+52,4)==2);
 CHECK(little(out+84,4)==1&&little(out+88,8)==131072&&little(out+96,4)==4);
 CHECK(little(out+232,4)==UINT32_C(4294967291)); /* -5 */
 for(unsigned i=0;i<4;++i)CHECK(little(out+236+i*4,4)==i);
 CHECK(!memcmp(out+252,base+210,24));
 CHECK(little(out+276,5)==131072&&little(out+281,3)==24);
 CHECK(!memcmp(out+288,directory,4));
 CHECK(!memcmp(out+292+2*24,base+238+2*24,24));
 CHECK(all(out+388,sizeof(out)-388,0xa5));
 uint8_t workspace_before[sizeof(baserows)+sizeof(smallrows)+sizeof(scratch)];
 memcpy(workspace_before,baserows,sizeof(baserows));
 memcpy(workspace_before+sizeof(baserows),smallrows,sizeof(smallrows));
 memcpy(workspace_before+sizeof(baserows)+sizeof(smallrows),scratch,sizeof(scratch));
 memset(&r,0xa5,sizeof(r));mb_toc_overlay_result_v1 old_result=r;
 uint8_t base_before[sizeof(base)];memcpy(base_before,base,sizeof(base));
 CHECK(mb_iostore_toc_overlay_v1(&p,&w,base,sizeof(base),&r)==MB_TOC_OVERLAY_BAD_ARGUMENT);
 CHECK(!memcmp(&r,&old_result,sizeof(r))&&!memcmp(base,base_before,sizeof(base)));
 CHECK(!memcmp(workspace_before,baserows,sizeof(baserows)));
 CHECK(!memcmp(workspace_before+sizeof(baserows),smallrows,sizeof(smallrows)));
 CHECK(!memcmp(workspace_before+sizeof(baserows)+sizeof(smallrows),scratch,sizeof(scratch)));
 /* Valid physical None blocks but a chunk crosses a short intermediate block. */
 block(base+210,0,32);chunk(base+180,0,65540);memset(out,0xa5,sizeof(out));
 CHECK(mb_iostore_toc_overlay_v1(&p,&w,out,sizeof(out),&r)==MB_TOC_OVERLAY_METADATA_REJECTED);
 CHECK(r.reason==MB_TOC_OVERLAY_REASON_CHUNK&&!r.metadata_complete&&!r.output_bytes);
 CHECK(all(out,sizeof(out),0xa5));
 printf("{\"phase\":\"passed\",\"assertions\":%u,\"controls\":[\"size_query\",\"short_output\",\"valid_overlay\",\"input_output_alias\",\"short_intermediate_block\"],\"payload_verified\":false}\n",assertions);
 return 0;
}
