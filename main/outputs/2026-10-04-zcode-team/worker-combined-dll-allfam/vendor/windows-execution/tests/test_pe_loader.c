#include "../pe_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c); exit(1); } } while (0)
static unsigned char files[3][0x800];
static _Alignas(16384) unsigned char images[3][0x8000];
static void w16(unsigned char *f,unsigned p,unsigned v){f[p]=(unsigned char)v;f[p+1]=(unsigned char)(v>>8);}
static void w32(unsigned char *f,unsigned p,uint32_t v){for(unsigned i=0;i<4;i++)f[p+i]=(unsigned char)(v>>(8*i));}
static void w64(unsigned char *f,unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)f[p+i]=(unsigned char)(v>>(8*i));}
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
typedef struct source { const void *file; unsigned acquired,released,held; int fail_acquire,fail_release; } source;
typedef struct service { mb_pe_loader_pin pin; unsigned begun,ended; int fail_begin,fail_end; } service;
static mb_pe_loader loader;
static mb_pe_loader_record records[2];
static mb_pe_loader_catalog_entry catalog[2];
static mb_pe_export_lease imports[2][4],procedures[2][2];
static source sources[2];static service services[2];
static unsigned allocations,releases,mapped[3],fallback_calls;
static int fail_memory_release;
static mb_pe_loader_error error;
static void *allocate(void *context,size_t size,size_t alignment){(void)context;CHECK(size==0x4000&&alignment<=16384);for(unsigned i=0;i<3;i++)if(!mapped[i]){mapped[i]=1;++allocations;return images[i];}CHECK(0);return NULL;}
static int protect(void *context,void *base,size_t offset,size_t size,unsigned rights){(void)context;(void)base;CHECK(offset+size<=0x4000&&(rights&6)!=6);return 0;}
static int flush(void *context,void *base,size_t size){(void)context;(void)base;CHECK(size==0x4000);return 0;}
static int release_memory(void *context,void *base,size_t size){(void)context;CHECK(size==0x4000);++releases;if(fail_memory_release)return 42;for(unsigned i=0;i<3;i++)if(base==images[i]){CHECK(mapped[i]);mapped[i]=0;return 0;}CHECK(0);return -1;}
static int acquire_source(void *context,const mb_pe_loader_catalog_entry *entry,const void **file,size_t *size,void **token){(void)context;source *s=entry->source_context;++s->acquired;CHECK(!s->held);if(s->fail_acquire)return 31;s->held=1;*file=s->file;*size=0x800;*token=s;return 0;}
static int release_source(void *context,void *token){(void)context;source *s=token;++s->released;CHECK(s->held);if(s->fail_release)return 32;s->held=0;return 0;}
static mb_pe_bind_result fallback(void *context,const mb_pe_import *item,uint64_t *value){(void)context;(void)item;(void)value;++fallback_calls;return MB_PE_BIND_UNRESOLVED;}
static int begin_service(void *context,const mb_pe_loader_catalog_entry *entry,mb_pe_module *module,mb_pe_binder *binder,void **binder_context,void **token){(void)context;size_t i=(size_t)(entry-catalog);CHECK(i<2&&module==&records[i].link.module&&module->state==MB_PE_MODULE_EMPTY);service *s=&services[i];++s->begun;if(s->fail_begin)return 41;CHECK(mb_pe_loader_acquire(&loader,&s->pin)==MB_PE_LOADER_OK);*binder=fallback;*binder_context=NULL;*token=s;return 0;}
static int end_service(void *context,void *token){(void)context;service *s=token;size_t i=(size_t)(s-services);CHECK(i<2&&records[i].link.module.state==MB_PE_MODULE_EMPTY);++s->ended;if(s->fail_end)return 43;CHECK(mb_pe_loader_release_pin(&s->pin)==MB_PE_LOADER_OK);return 0;}
static mb_pe_loader_config config;
static mb_pe_string str(const char *s){return (mb_pe_string){s,strlen(s)};}
static void reset(void){
 memset(&loader,0,sizeof(loader));memset(records,0,sizeof(records));memset(imports,0,sizeof(imports));memset(procedures,0,sizeof(procedures));memset(sources,0,sizeof(sources));memset(services,0,sizeof(services));memset(mapped,0,sizeof(mapped));allocations=releases=fallback_calls=0;fail_memory_release=0;
 fixture(files[0],0);fixture(files[2],1);sources[0].file=files[0];sources[1].file=files[2];
 catalog[0]=(mb_pe_loader_catalog_entry){str("Provider.dll"),&sources[0],NULL,imports[0],4,procedures[0],2};catalog[1]=(mb_pe_loader_catalog_entry){str("Consumer.dll"),&sources[1],NULL,imports[1],4,procedures[1],2};
 config=(mb_pe_loader_config){catalog,2,records,{NULL,4096,allocate,protect,flush,release_memory},{NULL,acquire_source,release_source},{NULL,begin_service,end_service},fallback,NULL};
 CHECK(mb_pe_loader_open(&loader,&config,&error)==MB_PE_LOADER_OK);
}
static void finish(void){CHECK(mb_pe_loader_close(&loader,&error)==MB_PE_LOADER_OK&&!loader.active&&!sources[0].held&&!sources[1].held&&!mapped[0]&&!mapped[1]&&!mapped[2]);}
static void references_and_procedures(void){
 reset();CHECK(!sources[0].acquired&&!sources[1].acquired);void *consumer=NULL,*provider=NULL,*address=NULL,*again=NULL;
 CHECK(mb_pe_loader_load(&loader,str("consumer"),&consumer,&error)==MB_PE_LOADER_OK);CHECK(consumer==records[1].link.module.image&&records[1].explicit_refs==1&&records[0].explicit_refs==0&&records[0].link.module.lease_count==2&&loader.pin_count==2);
 CHECK(mb_pe_loader_find(&loader,str("PROVIDER.DLL."),&provider,&error)==MB_PE_LOADER_OK&&provider==records[0].handle&&records[0].explicit_refs==0);
 CHECK(mb_pe_loader_load(&loader,str("provider.dll"),&again,&error)==MB_PE_LOADER_OK&&again==provider&&records[0].explicit_refs==1);
 CHECK(mb_pe_loader_load(&loader,str("consumer.dll"),&again,&error)==MB_PE_LOADER_OK&&again==consumer&&records[1].explicit_refs==2&&allocations==2);
 CHECK(mb_pe_loader_proc_name(&loader,consumer,str("Entry"),&address,&error)==MB_PE_LOADER_OK);CHECK(address==(unsigned char*)consumer+0x1000&&records[1].procedure_count==1);
 CHECK(mb_pe_loader_proc_ordinal(&loader,consumer,10,&again,&error)==MB_PE_LOADER_OK&&again==address&&records[1].procedure_count==1);
 CHECK(mb_pe_loader_proc_name(&loader,(void*)(uintptr_t)0x123,str("Entry"),&again,&error)==MB_PE_LOADER_NOT_FOUND);
 CHECK(mb_pe_loader_proc_name(&loader,consumer,str("Missing"),&again,&error)==MB_PE_LOADER_NOT_FOUND);
 CHECK(mb_pe_loader_proc_ordinal(&loader,consumer,11,&again,&error)==MB_PE_LOADER_UNSUPPORTED);
 CHECK(mb_pe_loader_proc_ordinal(&loader,consumer,12,&again,&error)==MB_PE_LOADER_UNSUPPORTED);
 CHECK(mb_pe_loader_free(&loader,consumer,&error)==MB_PE_LOADER_OK&&records[1].explicit_refs==1&&!releases);
 CHECK(mb_pe_loader_free(&loader,provider,&error)==MB_PE_LOADER_OK&&records[0].state==MB_PE_LOADER_ACTIVE&&!records[0].explicit_refs);
 CHECK(mb_pe_loader_free(&loader,provider,&error)==MB_PE_LOADER_BAD_STATE);
 CHECK(mb_pe_loader_free(&loader,consumer,&error)==MB_PE_LOADER_OK&&releases==2&&loader.pin_count==0&&sources[0].released==1&&sources[1].released==1);
 CHECK(mb_pe_loader_find(&loader,str("provider"),&again,&error)==MB_PE_LOADER_NOT_FOUND);finish();
}
static void cycles_and_rollback(void){
 reset();fixture(files[0],1);memcpy(files[0]+0x480,"Consumer.dll",13);void *handle=NULL;
 CHECK(mb_pe_loader_load(&loader,str("consumer"),&handle,&error)==MB_PE_LOADER_CYCLE&&allocations==0&&!handle&&sources[0].released==1&&sources[1].released==1);finish();
 reset();second_library("System.dll","Missing");CHECK(mb_pe_loader_load(&loader,str("consumer"),&handle,&error)==MB_PE_LOADER_MODULE_ERROR&&fallback_calls==1&&records[0].state==MB_PE_LOADER_EMPTY&&sources[0].released==1&&sources[1].released==1);CHECK(error.module.image.import.library.data==NULL&&error.module.image.import.library.size==0&&error.module.image.import.name.data==NULL&&error.module.image.import.name.size==0);finish();
 reset();memcpy(files[2]+0x4e2,"Missing",8);CHECK(mb_pe_loader_load(&loader,str("consumer"),&handle,&error)==MB_PE_LOADER_MODULE_ERROR&&fallback_calls==0&&!sources[0].held&&!sources[1].held);finish();
 reset();sources[0].fail_acquire=1;CHECK(mb_pe_loader_load(&loader,str("consumer"),&handle,&error)==MB_PE_LOADER_SOURCE_ERROR&&allocations==0&&sources[1].released==1&&sources[0].released==0);finish();
 reset();services[1].fail_begin=1;CHECK(mb_pe_loader_load(&loader,str("consumer"),&handle,&error)==MB_PE_LOADER_SERVICE_ERROR&&!sources[0].held&&!sources[1].held&&services[1].ended==0);finish();
}
static void retained_cleanup(void){
 reset();second_library("System.dll","Missing");void *failed_handle=NULL;fail_memory_release=1;CHECK(mb_pe_loader_load(&loader,str("consumer"),&failed_handle,&error)==MB_PE_LOADER_CLEANUP_REQUIRED&&!failed_handle&&records[1].state==MB_PE_LOADER_CLEANUP&&records[1].link.module.state==MB_PE_MODULE_CLEANUP&&sources[1].held&&services[1].ended==0&&records[0].link.module.lease_count==2);fail_memory_release=0;CHECK(mb_pe_loader_collect(&loader,&error)==MB_PE_LOADER_OK&&records[0].state==MB_PE_LOADER_EMPTY&&records[1].state==MB_PE_LOADER_EMPTY);finish();
 reset();void *handle=NULL,*ignored=NULL;CHECK(mb_pe_loader_load(&loader,str("consumer"),&handle,&error)==MB_PE_LOADER_OK);fail_memory_release=1;
 CHECK(mb_pe_loader_free(&loader,handle,&error)==MB_PE_LOADER_CLEANUP_REQUIRED&&records[1].explicit_refs==0&&records[1].state==MB_PE_LOADER_CLEANUP&&sources[1].held&&services[1].ended==0&&records[0].link.module.lease_count==2);
 CHECK(mb_pe_loader_find(&loader,str("consumer"),&ignored,&error)==MB_PE_LOADER_NOT_FOUND);CHECK(mb_pe_loader_load(&loader,str("consumer"),&ignored,&error)==MB_PE_LOADER_BAD_STATE&&records[1].explicit_refs==0);
 fail_memory_release=0;CHECK(mb_pe_loader_free(&loader,handle,&error)==MB_PE_LOADER_OK&&sources[1].released==1&&services[1].ended==1);finish();
 reset();CHECK(mb_pe_loader_load(&loader,str("provider"),&handle,&error)==MB_PE_LOADER_OK);services[0].fail_end=1;CHECK(mb_pe_loader_free(&loader,handle,&error)==MB_PE_LOADER_CLEANUP_REQUIRED&&records[0].link.module.state==MB_PE_MODULE_EMPTY&&sources[0].held&&sources[0].released==0);
 services[0].fail_end=0;CHECK(mb_pe_loader_collect(&loader,&error)==MB_PE_LOADER_OK&&services[0].ended==2&&releases==1);finish();
 reset();CHECK(mb_pe_loader_load(&loader,str("provider"),&handle,&error)==MB_PE_LOADER_OK);sources[0].fail_release=1;CHECK(mb_pe_loader_free(&loader,handle,&error)==MB_PE_LOADER_CLEANUP_REQUIRED&&records[0].handle==handle&&services[0].ended==1&&sources[0].held);
 sources[0].fail_release=0;CHECK(mb_pe_loader_free(&loader,handle,&error)==MB_PE_LOADER_OK&&services[0].ended==1&&releases==1&&sources[0].released==2);finish();
}
static void pins_capacity_and_validation(void){
 reset();mb_pe_loader_pin pin={0};CHECK(mb_pe_loader_acquire(&loader,&pin)==MB_PE_LOADER_OK);CHECK(mb_pe_loader_close(&loader,&error)==MB_PE_LOADER_BUSY);mb_pe_loader_pin copied=pin;CHECK(mb_pe_loader_release_pin(&copied)==MB_PE_LOADER_BAD_STATE);CHECK(mb_pe_loader_release_pin(&pin)==MB_PE_LOADER_OK);
 void *handle=NULL,*address=NULL;CHECK(mb_pe_loader_load(&loader,str("../provider.dll"),&handle,&error)==MB_PE_LOADER_UNSUPPORTED);CHECK(mb_pe_loader_load(&loader,str("provider."),&handle,&error)==MB_PE_LOADER_NOT_FOUND);finish();
 reset();w32(files[0],0x544,0x1001);CHECK(mb_pe_loader_close(&loader,&error)==MB_PE_LOADER_OK);catalog[0].procedure_pin_capacity=1;CHECK(mb_pe_loader_open(&loader,&config,&error)==MB_PE_LOADER_OK);CHECK(mb_pe_loader_load(&loader,str("provider"),&handle,&error)==MB_PE_LOADER_OK);CHECK(mb_pe_loader_proc_name(&loader,handle,str("Entry"),&address,&error)==MB_PE_LOADER_OK);CHECK(mb_pe_loader_proc_ordinal(&loader,handle,11,&address,&error)==MB_PE_LOADER_LIMIT);CHECK(mb_pe_loader_proc_ordinal(&loader,handle,10,&address,&error)==MB_PE_LOADER_OK&&records[0].procedure_count==1);
 mb_pe_export_lease external={0};CHECK(mb_pe_module_acquire_ordinal(&records[0].link.module,10,&external,NULL)==MB_PE_MODULE_OK);CHECK(mb_pe_loader_free(&loader,handle,&error)==MB_PE_LOADER_OK&&records[0].state==MB_PE_LOADER_ACTIVE&&records[0].procedure_count==0);CHECK(mb_pe_loader_close(&loader,&error)==MB_PE_LOADER_BUSY);CHECK(mb_pe_module_release(&external)==MB_PE_MODULE_OK);CHECK(mb_pe_loader_collect(&loader,&error)==MB_PE_LOADER_OK);finish();
 reset();finish();catalog[0].library=str("My Provider.dll");memcpy(files[2]+0x480,"My Provider.dll",16);CHECK(mb_pe_loader_open(&loader,&config,&error)==MB_PE_LOADER_OK);CHECK(mb_pe_loader_load(&loader,str("consumer"),&handle,&error)==MB_PE_LOADER_OK);CHECK(mb_pe_loader_find(&loader,str("my provider"),&address,&error)==MB_PE_LOADER_OK&&address==records[0].handle);CHECK(mb_pe_loader_free(&loader,handle,&error)==MB_PE_LOADER_OK);finish();
 reset();finish();catalog[1].library=str("PROVIDER");CHECK(mb_pe_loader_open(&loader,&config,&error)==MB_PE_LOADER_INVALID_ARGUMENT);
}
int main(void){references_and_procedures();cycles_and_rollback();retained_cleanup();pins_capacity_and_validation();puts("4 loader-owner groups passed: lazy dependency refs/procs, cycle/failure rollback, retained cleanup retries, pins/capacity/admission");return 0;}
