#include "pe_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned named,ordinal;static uint32_t named_iat,ordinal_iat,entry,anchor;
static uint64_t read64(const unsigned char*p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static mb_pe_bind_result bind(void*c,const mb_pe_import*i,uint64_t*v){
 (void)c;CHECK(i->library.size==19&&!memcmp(i->library.data,"FixtureProvider.dll",19));
 if(i->by_ordinal){CHECK(i->ordinal==17);++ordinal;ordinal_iat=i->iat_rva;*v=UINT64_C(0x5555666677778888);}
 else{CHECK(i->name.size==5&&!memcmp(i->name.data,"Named",5));++named;named_iat=i->iat_rva;*v=UINT64_C(0x1111222233334444);}
 return MB_PE_BIND_BOUND;
}
static int visit(void*c,const mb_pe_export*e){(void)c;if(e->name.size==5&&!memcmp(e->name.data,"Entry",5))entry=e->rva;if(e->name.size==6&&!memcmp(e->name.data,"Anchor",6))anchor=e->rva;CHECK(!e->forwarder.size);return 0;}
int main(int argc,char**argv){
 CHECK(argc==2);FILE*f=fopen(argv[1],"rb");CHECK(f);CHECK(fseek(f,0,SEEK_END)==0);long n=ftell(f);CHECK(n>0);rewind(f);
 unsigned char*file=malloc((size_t)n);CHECK(file&&fread(file,1,(size_t)n,f)==(size_t)n);CHECK(fclose(f)==0);
 mb_pe_info info;mb_pe_error error;mb_pe_report report;CHECK(mb_pe_inspect(file,(size_t)n,&info,&error)==MB_PE_OK);
 CHECK(info.import_count==2&&info.relocation_count==1&&info.entry_rva==0&&info.preferred_base==UINT64_C(0x180000000));
 unsigned char*image=malloc(info.image_size);CHECK(image);CHECK(mb_pe_visit_exports(file,(size_t)n,visit,NULL,&error)==MB_PE_OK&&entry&&anchor);
 CHECK(mb_pe_prepare(file,(size_t)n,UINT64_C(0x180100000),image,info.image_size,bind,NULL,&info,&report,&error)==MB_PE_OK);
 CHECK(named==1&&ordinal==1&&report.image_prepared&&report.imports_bound==2&&report.relocations_applied==1);
 CHECK(read64(image+anchor)==UINT64_C(0x180100000)+entry);
 CHECK(read64(image+named_iat)==UINT64_C(0x1111222233334444)&&read64(image+ordinal_iat)==UINT64_C(0x5555666677778888));
 free(image);free(file);puts("compiler-built PE byte preparation passed; no PE code executed");return 0;
}
