/* Exercises the real PE parser, module owner and dependency leases. Memory and
 * entry callbacks are deterministic; no PE instructions are executed here. */
#include "../pe_dependencies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c); exit(1); } } while (0)

static unsigned char files[3][0x800];
static _Alignas(16384) unsigned char images[3][0x8000];
typedef struct memory_state { unsigned index, allocations, releases; int fail_release, fail_protect; } memory_state;
static memory_state memory[3];
static mb_pe_memory_ops ops[3];
static mb_pe_module providers[2];
static mb_pe_link consumer;
static mb_pe_export_lease pins[4];
static mb_pe_module_error error;
static unsigned fallback_calls, attaches, detaches;
static int fallback_ok;
static int32_t attach_result;

static void w16(unsigned char *f,unsigned p,unsigned v){f[p]=(unsigned char)v;f[p+1]=(unsigned char)(v>>8);}
static void w32(unsigned char *f,unsigned p,uint32_t v){for(unsigned i=0;i<4;i++)f[p+i]=(unsigned char)(v>>(8*i));}
static void w64(unsigned char *f,unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)f[p+i]=(unsigned char)(v>>(8*i));}
static uint64_t r64(const unsigned char *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void dir(unsigned char *f,unsigned i,unsigned rva,unsigned n){w32(f,0x108+i*8,rva);w32(f,0x10c+i*8,n);}
static void fixture(unsigned char *f,int imports){
 memset(f,0,0x800);
 w16(f,0,0x5a4d);w32(f,0x3c,0x80);w32(f,0x80,0x4550);w16(f,0x84,0x8664);w16(f,0x86,2);w16(f,0x94,240);w16(f,0x96,0x2022);w16(f,0x98,0x20b);
 w64(f,0xb0,UINT64_C(0x180000000));w32(f,0xb8,0x1000);w32(f,0xbc,0x200);w32(f,0xd0,0x4000);w32(f,0xd4,0x200);w32(f,0x104,16);
 w32(f,0x190,0x300);w32(f,0x194,0x1000);w32(f,0x198,0x200);w32(f,0x19c,0x200);w32(f,0x1ac,0x60000020);
 w32(f,0x1b8,0x500);w32(f,0x1bc,0x2000);w32(f,0x1c0,0x400);w32(f,0x1c4,0x400);w32(f,0x1d4,0xc0000040);
 f[0x200]=0xc3;w64(f,0x208,UINT64_C(0x180001234));
 dir(f,5,0x2000,12);w32(f,0x400,0x1000);w32(f,0x404,12);w16(f,0x408,0xa008);
 dir(f,0,0x2100,0x90);w32(f,0x510,10);w32(f,0x514,3);w32(f,0x518,1);w32(f,0x51c,0x2140);w32(f,0x520,0x2150);w32(f,0x524,0x2160);
 w32(f,0x540,0x1000);w32(f,0x544,0x2170);w32(f,0x548,0x2200);w32(f,0x550,0x2190);memcpy(f+0x570,"Other.Forward",14);memcpy(f+0x590,"Entry",6);
 if(imports){dir(f,1,0x2040,40);w32(f,0x440,0x20a0);w32(f,0x44c,0x2080);w32(f,0x450,0x20c0);memcpy(f+0x480,"PrOvIdEr.DlL",13);w64(f,0x4a0,0x20e0);w64(f,0x4a8,UINT64_C(0x800000000000000a));w64(f,0x4c0,0x20e0);w64(f,0x4c8,UINT64_C(0x800000000000000a));memcpy(f+0x4e2,"Entry",6);}
}
static void second_library(const char *library,const char *name){
 unsigned char *f=files[2];dir(f,1,0x2040,60);w32(f,0x454,0x2260);w32(f,0x460,0x2240);w32(f,0x464,0x2280);
 strcpy((char*)f+0x640,library);w64(f,0x660,0x22a0);w64(f,0x680,0x22a0);strcpy((char*)f+0x6a2,name);
}
static void *allocate(void *context,size_t size,size_t alignment){memory_state *m=context;++m->allocations;CHECK(size==0x4000&&alignment<=16384);return images[m->index];}
static int protect(void *context,void *base,size_t off,size_t size,unsigned rights){memory_state *m=context;CHECK(base==images[m->index]&&off+size<=0x4000&&(rights&6)!=6);return m->fail_protect?-1:0;}
static int flush(void *context,void *base,size_t size){memory_state *m=context;CHECK(base==images[m->index]&&size==0x4000);return 0;}
static int release(void *context,void *base,size_t size){memory_state *m=context;CHECK(base==images[m->index]&&size==0x4000);++m->releases;return m->fail_release?-1:0;}
static mb_pe_bind_result fallback(void *context,const mb_pe_import *import,uint64_t *value){CHECK(context==&fallback_ok);(void)import;++fallback_calls;CHECK(providers[0].lease_count==2);if(!fallback_ok)return MB_PE_BIND_UNRESOLVED;*value=UINT64_C(0x12345000);return MB_PE_BIND_BOUND;}
static int32_t entry(void *context,void *address,void *base,uint32_t reason,void *reserved){CHECK(context==&attach_result&&address==images[2]+0x1000&&base==images[2]&&reserved==NULL);CHECK(providers[0].lease_count==2&&consumer.pin_count==2);if(reason==1){++attaches;CHECK(consumer.module.state==MB_PE_MODULE_INITIALIZING);return attach_result;}CHECK(reason==0&&consumer.module.state==MB_PE_MODULE_STOPPING);++detaches;return 0;}
static mb_pe_entry_ops entry_ops={&attach_result,entry};
static mb_pe_dependency registry[2];
static void reset(void){
 memset(providers,0,sizeof(providers));memset(&consumer,0,sizeof(consumer));memset(pins,0,sizeof(pins));memset(memory,0,sizeof(memory));memset(&error,0,sizeof(error));fallback_calls=attaches=detaches=0;fallback_ok=1;attach_result=-7;
 for(unsigned i=0;i<3;i++){fixture(files[i],i==2);memory[i].index=i;ops[i]=(mb_pe_memory_ops){&memory[i],4096,allocate,protect,flush,release};}
 for(unsigned i=0;i<2;i++)CHECK(mb_pe_module_open(&providers[i],files[i],sizeof(files[i]),&ops[i],NULL,NULL,&error)==MB_PE_MODULE_OK);
 registry[0]=(mb_pe_dependency){{"provider.dll",12},&providers[0]};registry[1]=(mb_pe_dependency){{"other.dll",9},&providers[1]};
}
static mb_pe_module_status open_link(const mb_pe_entry_ops *entry_ops_arg,size_t count){return mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],entry_ops_arg,registry,count,pins,4,fallback,&fallback_ok,&error);}
static void close_providers(void){CHECK(mb_pe_module_close(&providers[0],&error)==MB_PE_MODULE_OK);CHECK(mb_pe_module_close(&providers[1],&error)==MB_PE_MODULE_OK);}

static void pins_and_busy(void){
 reset();CHECK(open_link(NULL,1)==MB_PE_MODULE_OK);CHECK(consumer.owned&&consumer.pin_count==2&&providers[0].lease_count==2&&fallback_calls==0);
 CHECK(r64(images[2]+0x20c0)==(uint64_t)(uintptr_t)(images[0]+0x1000));CHECK(r64(images[2]+0x20c8)==r64(images[2]+0x20c0));
 CHECK(mb_pe_module_close(&providers[0],&error)==MB_PE_MODULE_BUSY);
 mb_pe_export_lease lease={0};CHECK(mb_pe_module_acquire_ordinal(&consumer.module,10,&lease,&error)==MB_PE_MODULE_OK);
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_BUSY&&consumer.pin_count==2&&providers[0].lease_count==2&&memory[2].releases==0);
 CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_OK);CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK);
 CHECK(!consumer.owned&&!consumer.pins&&!consumer.pin_count&&providers[0].lease_count==0);CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK);close_providers();
}
static void registry_and_fallback(void){
 reset();second_library("System.dll","Fallback");CHECK(open_link(NULL,1)==MB_PE_MODULE_OK&&fallback_calls==1&&consumer.pin_count==2);CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK);close_providers();
 /* A declared owner cannot silently fall through for missing, forward or data. */
 for(unsigned ordinal=11;ordinal<=13;ordinal++){reset();w64(files[2],0x4a0,UINT64_C(0x8000000000000000)|ordinal);CHECK(open_link(NULL,1)==MB_PE_MODULE_UNRESOLVED_IMPORTS&&fallback_calls==0&&providers[0].lease_count==0&&!consumer.owned);close_providers();}
 reset();memcpy(files[2]+0x4e2,"Missing",8);CHECK(open_link(NULL,1)==MB_PE_MODULE_UNRESOLVED_IMPORTS&&fallback_calls==0&&providers[0].lease_count==0);close_providers();
}
static void validate_arguments(void){
 reset();registry[1].library=(mb_pe_string){"PROVIDER.DLL",12};CHECK(open_link(NULL,2)==MB_PE_MODULE_INVALID_ARGUMENT&&memory[2].allocations==0);close_providers();
 reset();providers[1].state=MB_PE_MODULE_CLEANUP;CHECK(open_link(NULL,2)==MB_PE_MODULE_BAD_STATE&&memory[2].allocations==0);providers[1].state=MB_PE_MODULE_ACTIVE;close_providers();
 reset();CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],NULL,registry,1,pins,1,NULL,NULL,&error)==MB_PE_MODULE_LIMIT&&memory[2].allocations==0);
 pins[3].address=(void*)(uintptr_t)1;CHECK(open_link(NULL,1)==MB_PE_MODULE_BAD_STATE&&memory[2].allocations==0);pins[3].address=NULL;
 CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],NULL,registry,1,(mb_pe_export_lease*)files[2],4,NULL,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_link_open(&consumer,&consumer,sizeof(consumer),&ops[2],NULL,registry,1,pins,4,NULL,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],NULL,registry,1,pins,4,NULL,NULL,(mb_pe_module_error*)files[0])==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],NULL,registry,1,(mb_pe_export_lease*)&providers[0],4,NULL,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],NULL,(mb_pe_dependency*)pins,1,pins,4,NULL,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 /* Underlying module alias rejection must preserve independent error output. */
 error=(mb_pe_module_error){.code=MB_PE_MODULE_BUSY,.cause=MB_PE_MODULE_BUSY,.detail=99};
 mb_pe_module_error saved_error=error;
 CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),(mb_pe_memory_ops*)(files[2]+0x600),NULL,registry,1,pins,4,NULL,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(memcmp(&error,&saved_error,sizeof(error))==0&&!consumer.owned);
 CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],(mb_pe_entry_ops*)(files[2]+0x600),registry,1,pins,4,NULL,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(memcmp(&error,&saved_error,sizeof(error))==0&&!consumer.owned);
 CHECK(mb_pe_link_open(&consumer,files[2],sizeof(files[2]),&ops[2],(mb_pe_entry_ops*)&ops[2],registry,1,pins,4,NULL,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(memcmp(&error,&saved_error,sizeof(error))==0&&!consumer.owned);
 CHECK(memory[2].allocations==0);close_providers();
}
static void cleanup_retries(void){
 reset();second_library("System.dll","Fallback");fallback_ok=0;memory[2].fail_release=1;
 CHECK(open_link(NULL,1)==MB_PE_MODULE_CLEANUP_REQUIRED&&error.cause==MB_PE_MODULE_UNRESOLVED_IMPORTS&&consumer.module.state==MB_PE_MODULE_CLEANUP&&consumer.pin_count==2&&providers[0].lease_count==2);
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&consumer.pin_count==2&&providers[0].lease_count==2);memory[2].fail_release=0;
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK&&!consumer.owned&&providers[0].lease_count==0);close_providers();
 reset();memory[2].fail_protect=memory[2].fail_release=1;CHECK(open_link(NULL,1)==MB_PE_MODULE_CLEANUP_REQUIRED&&providers[0].lease_count==2);memory[2].fail_release=0;CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK&&providers[0].lease_count==0);close_providers();
}
static void entries_keep_pins(void){
 reset();w32(files[2],0xa8,0x1000);CHECK(open_link(&entry_ops,1)==MB_PE_MODULE_OK&&attaches==1&&detaches==0);memory[2].fail_release=1;
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&detaches==1&&providers[0].lease_count==2);
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&detaches==1);memory[2].fail_release=0;
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK&&detaches==1&&providers[0].lease_count==0);close_providers();
 reset();w32(files[2],0xa8,0x1000);attach_result=0;memory[2].fail_release=1;
 CHECK(open_link(&entry_ops,1)==MB_PE_MODULE_CLEANUP_REQUIRED&&attaches==1&&detaches==1&&providers[0].lease_count==2);memory[2].fail_release=0;
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK&&detaches==1&&providers[0].lease_count==0);close_providers();
}
static void partial_retirement(void){
 reset();second_library("other.dll","Entry");CHECK(open_link(NULL,2)==MB_PE_MODULE_OK&&consumer.pin_count==3&&providers[1].lease_count==1);
 /* Deliberately inject a provider-state fault to exercise failed retirement.
  * Normal callers must not mutate a provider while leases exist. */
 providers[0].state=MB_PE_MODULE_CLEANUP;
 CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&error.cause==MB_PE_MODULE_BAD_STATE&&error.detail==1);
 CHECK(consumer.owned&&consumer.module.state==MB_PE_MODULE_EMPTY&&consumer.pin_count==2&&providers[0].lease_count==2&&providers[1].lease_count==0);
 providers[0].state=MB_PE_MODULE_ACTIVE;CHECK(mb_pe_link_close(&consumer,&error)==MB_PE_MODULE_OK&&!consumer.owned&&providers[0].lease_count==0);close_providers();
}
int main(void){pins_and_busy();registry_and_fallback();validate_arguments();cleanup_retries();entries_keep_pins();partial_retirement();puts("6 dependency-owner groups passed: names/ordinals, fallback boundaries, validation, cleanup retries, entry pins, partial retirement");return 0;}
