#include "pe_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char file[0x800], image[0x4000];
static mb_pe_info info; static mb_pe_error error; static mb_pe_report report;
static void w16(unsigned p,unsigned v){file[p]=(unsigned char)v;file[p+1]=(unsigned char)(v>>8);}
static void w32(unsigned p,uint32_t v){for(unsigned i=0;i<4;i++)file[p+i]=(unsigned char)(v>>(8*i));}
static void w64(unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)file[p+i]=(unsigned char)(v>>(8*i));}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void dir(unsigned i,unsigned rva,unsigned n){w32(0x108+i*8,rva);w32(0x10c+i*8,n);}
/* The fixture catches missing raw/header copying, mistaken RVA=file offsets,
 * tails copied from overlay bytes, wrong rebasing sign, and IAT-as-lookup reads. */
static void fixture(void){
 memset(file,0,sizeof(file));memset(image,0xcc,sizeof(image));
 w16(0,0x5a4d);w32(0x3c,0x80);w32(0x80,0x4550);w16(0x84,0x8664);
 w16(0x86,2);w16(0x94,240);w16(0x96,0x2022);w16(0x98,0x20b);
 w32(0xa8,0x1000);w64(0xb0,UINT64_C(0x180000000));
 w32(0xb8,0x1000);w32(0xbc,0x200);w32(0xd0,0x4000);w32(0xd4,0x200);w32(0x104,16);
 memcpy(file+0x188,".text",5);w32(0x190,0x300);w32(0x194,0x1000);w32(0x198,0x200);w32(0x19c,0x200);w32(0x1ac,0x60000020);
 memcpy(file+0x1b0,".data",5);w32(0x1b8,0x500);w32(0x1bc,0x2000);w32(0x1c0,0x400);w32(0x1c4,0x400);w32(0x1d4,0xc0000040);
 file[0x200]=0xc3;w64(0x208,UINT64_C(0x180001234));
 dir(5,0x2000,12);w32(0x400,0x1000);w32(0x404,12);w16(0x408,0xa008);w16(0x40a,0);
 dir(1,0x2040,40);w32(0x440,0x20a0);w32(0x44c,0x2080);w32(0x450,0x20c0);
 memcpy(file+0x480,"KERNEL32.dll",13);w64(0x4a0,0x20e0);w64(0x4a8,UINT64_C(0x8000000000000007));
 w64(0x4c0,0x20e0);w64(0x4c8,UINT64_C(0x8000000000000007));w16(0x4e0,9);memcpy(file+0x4e2,"CreateThing",12);
 dir(0,0x2100,0x90);w32(0x510,10);w32(0x514,3);w32(0x518,1);w32(0x51c,0x2140);w32(0x520,0x2150);w32(0x524,0x2160);
 w32(0x540,0x1000);w32(0x544,0x2170);w32(0x548,0);w32(0x550,0x2190);w16(0x560,0);memcpy(file+0x570,"OTHER.Forward",14);memcpy(file+0x590,"NamedExport",12);
}
static unsigned calls;
static mb_pe_bind_result binder(void*ctx,const mb_pe_import*i,uint64_t*v){
 (void)ctx;CHECK(i->library.size==12&&!memcmp(i->library.data,"KERNEL32.dll",12));
 if(calls++==0){CHECK(!i->by_ordinal&&i->hint==9&&i->iat_rva==0x20c0);CHECK(i->name.size==11&&!memcmp(i->name.data,"CreateThing",11));*v=UINT64_C(0x700000010);}
 else{CHECK(i->by_ordinal&&i->ordinal==7&&i->iat_rva==0x20c8);*v=UINT64_C(0x700000020);}
 return MB_PE_BIND_BOUND;
}
static mb_pe_status prepare(uint64_t base,mb_pe_binder b){calls=0;return mb_pe_prepare(file,sizeof(file),base,image,sizeof(image),b,NULL,&info,&report,&error);}
static unsigned exports;
static int visit(void*c,const mb_pe_export*e){(void)c;if(exports++==0){CHECK(e->ordinal==10&&e->rva==0x1000&&e->name.size==11&&e->forwarder.size==0);}else{CHECK(e->ordinal==11&&e->forwarder.size==13&&e->name.size==0);}return 0;}
static mb_pe_bind_result missing(void*c,const mb_pe_import*i,uint64_t*v){(void)c;(void)i;(void)v;calls++;return MB_PE_BIND_UNRESOLVED;}
static mb_pe_bind_result failing(void*c,const mb_pe_import*i,uint64_t*v){(void)c;(void)i;*v=77;return calls++ ? MB_PE_BIND_ERROR:MB_PE_BIND_BOUND;}
static int import_visit(void *context,const mb_pe_import *i){
 CHECK(context==&calls);CHECK(i->library.data>=((const char*)file)&&i->library.data<((const char*)file+sizeof(file)));
 CHECK(i->library.size==12&&!memcmp(i->library.data,"KERNEL32.dll",12));
 if(calls++==0){CHECK(!i->by_ordinal&&i->hint==9&&i->iat_rva==0x20c0&&i->name.size==11&&!memcmp(i->name.data,"CreateThing",11));}
 else{CHECK(i->by_ordinal&&i->ordinal==7&&i->iat_rva==0x20c8&&i->name.size==0);}
 return 0;
}
static int stop_import(void *context,const mb_pe_import *i){(void)context;CHECK(i->iat_rva==0x20c0);calls++;return 1;}
int main(void){
 fixture();CHECK(mb_pe_inspect(file,sizeof(file),&info,&error)==MB_PE_OK);CHECK(info.image_size==0x4000&&info.entry_rva==0x1000&&info.import_count==2&&info.relocation_count==1&&info.export_count==3);
 CHECK(prepare(UINT64_C(0x180100000),binder)==MB_PE_OK);CHECK(calls==2&&report.image_prepared&&report.imports_bound==2&&report.relocations_applied==1);
 CHECK(!memcmp(file,image,0x200)&&image[0x1000]==0xc3&&image[0x200]==0&&image[0x1200]==0&&image[0x2400]==0);CHECK(r64(image+0x1008)==UINT64_C(0x180101234));CHECK(r64(image+0x20c0)==UINT64_C(0x700000010)&&r64(image+0x20c8)==UINT64_C(0x700000020));
 fixture();CHECK(prepare(UINT64_C(0x17ff00000),binder)==MB_PE_OK);CHECK(r64(image+0x1008)==UINT64_C(0x17ff01234));
 fixture();CHECK(prepare(UINT64_C(0x180000000),NULL)==MB_PE_UNRESOLVED_IMPORTS);CHECK(report.image_prepared&&report.imports_unresolved==2&&r64(image+0x20c0)==0x20e0);
 fixture();CHECK(prepare(UINT64_C(0x180000000),missing)==MB_PE_UNRESOLVED_IMPORTS&&calls==2);
 fixture();w32(0x440,0);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_OK&&calls==2);
 fixture();exports=0;CHECK(mb_pe_visit_exports(file,sizeof(file),visit,NULL,&error)==MB_PE_OK&&exports==2);
 fixture();calls=0;unsigned char snapshot[sizeof(file)];memcpy(snapshot,file,sizeof(file));CHECK(mb_pe_visit_imports(file,sizeof(file),import_visit,&calls,&error)==MB_PE_OK&&calls==2&&!memcmp(snapshot,file,sizeof(file)));
 fixture();calls=0;CHECK(mb_pe_visit_imports(file,sizeof(file),stop_import,NULL,&error)==MB_PE_VISITOR_ERROR&&calls==1&&error.rva==0x20c0&&error.import.name.size==11);
 fixture();calls=0;w64(0x4a8,UINT64_C(0x8001000000000007));CHECK(mb_pe_visit_imports(file,sizeof(file),import_visit,&calls,&error)==MB_PE_BAD_FORMAT&&calls==0);
 fixture();calls=0;w16(0x560,3);CHECK(mb_pe_visit_imports(file,sizeof(file),import_visit,&calls,&error)==MB_PE_BAD_FORMAT&&calls==0);
 fixture();calls=0;dir(1,0,0);CHECK(mb_pe_visit_imports(file,sizeof(file),import_visit,&calls,&error)==MB_PE_OK&&calls==0);
 fixture();CHECK(mb_pe_visit_imports(file,sizeof(file),NULL,NULL,&error)==MB_PE_INVALID_ARGUMENT);
 fixture();calls=0;CHECK(mb_pe_visit_imports(file,sizeof(file),import_visit,&calls,(mb_pe_error*)file)==MB_PE_ALIAS&&calls==0&&file[0]=='M');
 fixture();calls=0;w32(0x440,0);CHECK(mb_pe_visit_imports(file,sizeof(file),import_visit,&calls,&error)==MB_PE_OK&&calls==2);
 fixture();CHECK(prepare(UINT64_C(0x180000000),failing)==MB_PE_BINDER_ERROR);CHECK(error.import.by_ordinal&&error.import.ordinal==7&&!report.image_prepared);for(unsigned n=0;n<sizeof(image);n++)CHECK(image[n]==0);
 fixture();w32(0x1bc,0x1000);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_OVERLAP&&calls==0);
 fixture();w32(0x1c4,0x200);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_OVERLAP);
 fixture();w16(0x408,0x3008);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_UNSUPPORTED&&calls==0);
 fixture();dir(5,0,0);CHECK(prepare(UINT64_C(0x180100000),binder)==MB_PE_RELOCATIONS_REQUIRED);
 fixture();w64(0x208,UINT64_MAX);CHECK(prepare(UINT64_C(0x180100000),binder)==MB_PE_RELOCATION_OVERFLOW&&calls==0);
 fixture();w64(0x208,1);CHECK(prepare(UINT64_C(0x17ff00000),binder)==MB_PE_RELOCATION_OVERFLOW);
 fixture();w32(0x404,7);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_BAD_FORMAT);
 fixture();w32(0x404,14);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_BAD_FORMAT);
 fixture();w32(0x400,0x3000);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_BAD_RVA);
 fixture();w64(0x4a0,UINT64_C(0x8001000000000001));CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_BAD_FORMAT);
 fixture();w64(0x4a0,0x2fff);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_BAD_RVA);
 fixture();dir(1,0x2040,20);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_BAD_FORMAT&&calls==0);
 fixture();w32(0x450,0x3ff8);CHECK(prepare(UINT64_C(0x180000000),binder)==MB_PE_BAD_RVA);
 fixture();w16(0x560,3);CHECK(mb_pe_visit_exports(file,sizeof(file),visit,NULL,&error)==MB_PE_BAD_FORMAT);
 fixture();w32(0x540,0x3000);CHECK(mb_pe_inspect(file,sizeof(file),&info,&error)==MB_PE_BAD_RVA);
 fixture();CHECK(mb_pe_prepare(file,sizeof(file),0,file,sizeof(file),NULL,NULL,&info,&report,&error)==MB_PE_ALIAS&&file[0]=='M');
 fixture();CHECK(mb_pe_prepare(file,sizeof(file),0,image,8,NULL,NULL,&info,&report,&error)==MB_PE_CAPACITY&&image[0]==0xcc);
 fixture();w32(0x3c,0xfffffff0);CHECK(mb_pe_inspect(file,sizeof(file),&info,&error)==MB_PE_TRUNCATED);
 fixture();w32(0xd0,0x1000);CHECK(mb_pe_inspect(file,sizeof(file),&info,&error)!=MB_PE_OK);
 fixture();dir(9,0x2200,40);dir(3,0x2230,12);dir(13,0x2240,32);dir(10,0x2260,64);CHECK(mb_pe_inspect(file,sizeof(file),&info,&error)==MB_PE_OK);CHECK((info.requirements&30)==30);
 /* These must refuse before a binder observes any partially conflicting plan. */
 fixture();dir(1,0x2040,60);w32(0x454,0x2200);w32(0x460,0x2080);w32(0x464,0x20c4);w64(0x600,UINT64_C(0x8000000000000063));w64(0x608,UINT64_C(0x8000000000000064));CHECK(prepare(UINT64_C(0x180000000),missing)==MB_PE_OVERLAP&&calls==0&&!report.image_prepared);for(unsigned n=0;n<sizeof(image);n++)CHECK(image[n]==0);
 fixture();w16(0x40a,0xa008);CHECK(prepare(UINT64_C(0x180100000),missing)==MB_PE_OVERLAP&&calls==0);
 fixture();w16(0x40a,0xa00c);CHECK(prepare(UINT64_C(0x180100000),missing)==MB_PE_OVERLAP&&calls==0);
 fixture();w32(0x400,0x2000);w16(0x408,0xa0c0);CHECK(prepare(UINT64_C(0x180100000),missing)==MB_PE_OVERLAP&&calls==0);
 fixture();dir(1,0x2040,60);w32(0x454,0x2200);w32(0x460,0x2080);w32(0x464,0x20d0);w64(0x600,UINT64_C(0x8000000000000063));CHECK(prepare(UINT64_C(0x180000000),missing)==MB_PE_OVERLAP&&calls==0);
 fixture();w64(0x4d0,1);CHECK(prepare(UINT64_C(0x180000000),missing)==MB_PE_BAD_FORMAT&&calls==0);
 puts("42 PE byte-semantic groups passed");return 0;
}
