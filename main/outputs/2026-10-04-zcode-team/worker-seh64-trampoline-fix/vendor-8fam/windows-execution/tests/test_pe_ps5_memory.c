#include "../pe_ps5_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c); exit(1); } } while (0)
static int page_size=16384, fail_map, fail_protect, fail_unmap;
static unsigned maps, protects, unmaps, native_live;
static void *last_map, *last_protect, *last_unmap;
static size_t last_map_size, last_protect_size, last_unmap_size;
static int last_rights;
int mb_pe_ps5_test_pagesize(void){return page_size;}
void *mb_pe_ps5_test_map(void *hint,size_t size,int rights,int flags,int fd,int64_t offset){
 CHECK(!hint&&rights==3&&flags==0x1002&&fd==-1&&offset==0);++maps;
 if(fail_map){errno=ENOMEM;return (void*)(intptr_t)-1;}
 void *p=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
 CHECK(p!=MAP_FAILED);last_map=p;last_map_size=size;++native_live;return p;
}
int mb_pe_ps5_test_protect(const void *p,size_t size,int rights){
 CHECK((rights&6)!=6);++protects;last_protect=(void*)p;last_protect_size=size;last_rights=rights;
 if(fail_protect){errno=EINVAL;return (int32_t)UINT32_C(0x8002000d);}
 int native=0;if(rights&1)native|=PROT_READ;if(rights&2)native|=PROT_WRITE;if(rights&4)native|=PROT_EXEC;
 return mprotect((void*)p,size,native);
}
int mb_pe_ps5_test_unmap(void *p,size_t size){
 ++unmaps;last_unmap=p;last_unmap_size=size;if(fail_unmap){errno=EIO;return -1;}
 CHECK(native_live);int status=munmap(p,size);if(!status)--native_live;return status;
}
static mb_pe_ps5_memory owner;
static mb_pe_ps5_memory_slot slots[2];
static mb_pe_memory_ops ops;
static void reset(void){
 CHECK(!native_live);memset(&owner,0,sizeof(owner));memset(slots,0,sizeof(slots));memset(&ops,0,sizeof(ops));
 maps=protects=unmaps=0;fail_map=fail_protect=fail_unmap=0;page_size=16384;
}
static void open_owner(void){CHECK(mb_pe_ps5_memory_open(&owner,slots,2,&ops)==MB_PE_PS5_MEMORY_OK);CHECK(ops.context==&owner&&ops.protection_granule==16384);}
static void admission(void){
 reset();page_size=4096;CHECK(mb_pe_ps5_memory_open(&owner,slots,2,&ops)==MB_PE_PS5_MEMORY_UNSUPPORTED&&!owner.active&&!maps&&!ops.allocate_rw);
 page_size=16384;CHECK(mb_pe_ps5_memory_open(&owner,slots,0,&ops)==MB_PE_PS5_MEMORY_INVALID_ARGUMENT);
 slots[0].image_size=1;CHECK(mb_pe_ps5_memory_open(&owner,slots,2,&ops)==MB_PE_PS5_MEMORY_BAD_STATE);slots[0].image_size=0;
 open_owner();CHECK(mb_pe_ps5_memory_open(&owner,slots,2,&ops)==MB_PE_PS5_MEMORY_BAD_STATE);
 CHECK(!ops.allocate_rw(&owner,0,16384)&&!ops.allocate_rw(&owner,16384,8192)&&!ops.allocate_rw(&owner,16384,24576));
 CHECK(!ops.allocate_rw(&owner,SIZE_MAX-16383,16384)&&!maps);
 CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);
 CHECK(!ops.allocate_rw(&owner,16384,16384));CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);
}
static void allocations_and_protection(void){
 reset();open_owner();fail_map=1;CHECK(!ops.allocate_rw(&owner,16384,16384)&&owner.live_allocations==0);fail_map=0;
 unsigned char *a=ops.allocate_rw(&owner,3*16384,65536);CHECK(a&&((uintptr_t)a%65536)==0&&owner.live_allocations==1);
 CHECK(slots[0].mapping==last_map&&slots[0].mapping_size==last_map_size&&last_map_size==3*16384+65536);
 memset(a,0x5a,3*16384);void *b=ops.allocate_rw(&owner,16384,16384);CHECK(b&&b!=a&&owner.live_allocations==2);
 unsigned before=maps;CHECK(!ops.allocate_rw(&owner,16384,16384)&&maps==before);
 CHECK(ops.protect(&owner,a,16384,16384,5)==0&&last_protect==a+16384&&last_protect_size==16384&&last_rights==5);
 CHECK(ops.protect(&owner,a,0,16384,1)==0&&last_rights==1&&a[0]==0x5a);
 CHECK(ops.protect(&owner,a,2*16384,16384,0)==0&&last_rights==0);
 before=protects;CHECK(ops.protect(&owner,a,0,16384,7)!=0&&ops.protect(&owner,a,0,16384,8)!=0);
 CHECK(ops.protect(&owner,a,1,16384,1)!=0&&ops.protect(&owner,a,0,1,1)!=0&&ops.protect(&owner,a,0,0,1)!=0);
 CHECK(ops.protect(&owner,a,3*16384,16384,1)!=0&&ops.protect(&owner,(void*)1,0,16384,1)!=0&&protects==before);
 CHECK(ops.flush_instruction_cache(&owner,a,3*16384)==0&&ops.flush_instruction_cache(&owner,a,16384)!=0);
 CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_BUSY);
 CHECK(ops.release(&owner,a,3*16384)==0&&ops.release(&owner,b,16384)==0&&!native_live&&!owner.live_allocations);
 CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);
}
static void retained_failure(void){
 reset();open_owner();unsigned char *p=ops.allocate_rw(&owner,16384,65536);CHECK(p);p[0]=0xc7;
 mb_pe_ps5_memory_slot saved=slots[0];fail_protect=1;CHECK(ops.protect(&owner,p,0,16384,5)==(int32_t)UINT32_C(0x8002000d)&&p[0]==0xc7&&owner.live_allocations==1);
 fail_unmap=1;CHECK(ops.release(&owner,p,16384)!=0&&p[0]==0xc7&&native_live==1);
 CHECK(!memcmp(&slots[0],&saved,sizeof(saved))&&last_unmap==saved.mapping&&last_unmap_size==saved.mapping_size);
 CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_BUSY&&owner.active);
 CHECK(ops.release(&owner,p,8192)!=0&&unmaps==1);fail_unmap=0;
 CHECK(ops.release(&owner,p,16384)==0&&unmaps==2&&!native_live&&!owner.live_allocations);
 CHECK(ops.release(&owner,p,16384)!=0&&unmaps==2);CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);
}
static unsigned char file[0x600];
static void w16(unsigned p,unsigned v){file[p]=(unsigned char)v;file[p+1]=(unsigned char)(v>>8);}
static void w32(unsigned p,uint32_t v){for(unsigned i=0;i<4;i++)file[p+i]=(unsigned char)(v>>(8*i));}
static void w64(unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)file[p+i]=(unsigned char)(v>>(8*i));}
static void fixture(void){
 memset(file,0,sizeof(file));w16(0,0x5a4d);w32(0x3c,0x80);w32(0x80,0x4550);w16(0x84,0x8664);w16(0x86,2);w16(0x94,240);w16(0x96,0x2022);w16(0x98,0x20b);
 w64(0xb0,UINT64_C(0x180000000));w32(0xb8,0x4000);w32(0xbc,0x200);w32(0xd0,0xc000);w32(0xd4,0x200);w32(0x104,16);
 w32(0x190,0x200);w32(0x194,0x4000);w32(0x198,0x200);w32(0x19c,0x200);w32(0x1ac,0x60000020);
 w32(0x1b8,0x200);w32(0x1bc,0x8000);w32(0x1c0,0x200);w32(0x1c4,0x400);w32(0x1d4,0xc0000040);
 file[0x200]=0xc3;w64(0x208,UINT64_C(0x180004123));w32(0x130,0x8000);w32(0x134,12);w32(0x400,0x4000);w32(0x404,12);w16(0x408,0xa008);
}
static void module_cleanup(void){
 reset();open_owner();fixture();mb_pe_module module={0};mb_pe_module_error error={0};
 CHECK(mb_pe_module_open(&module,file,sizeof(file),&ops,NULL,NULL,&error)==MB_PE_MODULE_OK);
 CHECK(module.image==slots[0].image&&module.allocation_size==0xc000&&protects==3&&module.report.relocations_applied==1);
 uint64_t relocated;memcpy(&relocated,(unsigned char*)module.image+0x4008,8);CHECK(relocated==(uintptr_t)module.image+0x4123);
 fail_unmap=1;void *image=module.image;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&module.image==image&&owner.live_allocations==1);
 fail_unmap=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&!owner.live_allocations);
 fail_protect=fail_unmap=1;CHECK(mb_pe_module_open(&module,file,sizeof(file),&ops,NULL,NULL,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&error.cause==MB_PE_MODULE_MEMORY_ERROR&&module.state==MB_PE_MODULE_CLEANUP&&owner.live_allocations==1);
 fail_unmap=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&!native_live);CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);
}
static mb_pe_ps5_memory_diagnostics snapshot(void){
 mb_pe_ps5_memory_diagnostics d={0};CHECK(mb_pe_ps5_memory_get_diagnostics(&owner,&d)==MB_PE_PS5_MEMORY_OK);return d;
}
static void diagnostic_lifecycle(void){
 reset();mb_pe_ps5_memory_diagnostics d=snapshot();CHECK(d.last.operation==MB_PE_PS5_MEMORY_OP_NONE&&d.first_failure.operation==MB_PE_PS5_MEMORY_OP_NONE);
 CHECK(mb_pe_ps5_memory_get_diagnostics(NULL,&d)==MB_PE_PS5_MEMORY_INVALID_ARGUMENT&&mb_pe_ps5_memory_get_diagnostics(&owner,NULL)==MB_PE_PS5_MEMORY_INVALID_ARGUMENT);
 open_owner();unsigned char *p=ops.allocate_rw(&owner,3*16384,65536);CHECK(p);d=snapshot();
 CHECK(d.last.operation==MB_PE_PS5_MEMORY_OP_ALLOCATE&&d.last.native_result==0&&d.last.native_errno==0&&d.last.length==3*16384+65536&&d.last.offset==0&&d.last.rights==3&&d.first_failure.operation==MB_PE_PS5_MEMORY_OP_NONE);
 CHECK(ops.protect(&owner,p,0,16384,7)!=0);CHECK(snapshot().first_failure.operation==MB_PE_PS5_MEMORY_OP_NONE);
 fail_protect=1;CHECK(ops.protect(&owner,p,16384,16384,5)==(int32_t)UINT32_C(0x8002000d));d=snapshot();
 CHECK(d.last.operation==MB_PE_PS5_MEMORY_OP_PROTECT&&d.last.native_result==(int32_t)UINT32_C(0x8002000d)&&d.last.native_errno==0&&d.last.offset==16384&&d.last.length==16384&&d.last.rights==5);
 CHECK(d.first_failure.operation==d.last.operation&&d.first_failure.native_result==d.last.native_result&&d.first_failure.offset==16384);
 fail_protect=0;CHECK(ops.protect(&owner,p,16384,16384,1)==0);d=snapshot();CHECK(d.last.operation==MB_PE_PS5_MEMORY_OP_PROTECT&&d.last.native_result==0&&d.last.native_errno==0&&d.last.rights==1&&d.first_failure.native_result==(int32_t)UINT32_C(0x8002000d)&&d.first_failure.rights==5);
 fail_unmap=1;CHECK(ops.release(&owner,p,3*16384)==-1);d=snapshot();CHECK(d.last.operation==MB_PE_PS5_MEMORY_OP_RELEASE&&d.last.native_result==-1&&d.last.native_errno==EIO&&d.last.length==3*16384+65536&&d.last.offset==0&&d.last.rights==0&&d.first_failure.operation==MB_PE_PS5_MEMORY_OP_PROTECT);
 CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_BUSY&&snapshot().last.native_errno==EIO);
 fail_unmap=0;CHECK(ops.release(&owner,p,3*16384)==0);d=snapshot();CHECK(d.last.native_result==0&&d.last.native_errno==0&&d.first_failure.native_result==(int32_t)UINT32_C(0x8002000d));
 CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);d=snapshot();CHECK(d.last.operation==MB_PE_PS5_MEMORY_OP_RELEASE&&d.last.native_result==0&&d.first_failure.operation==MB_PE_PS5_MEMORY_OP_PROTECT&&d.first_failure.offset==16384);
 open_owner();d=snapshot();CHECK(d.last.operation==MB_PE_PS5_MEMORY_OP_NONE&&d.first_failure.operation==MB_PE_PS5_MEMORY_OP_NONE);CHECK(mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);
 reset();open_owner();fail_map=1;CHECK(!ops.allocate_rw(&owner,16384,65536));d=snapshot();CHECK(d.first_failure.operation==MB_PE_PS5_MEMORY_OP_ALLOCATE&&d.first_failure.native_result==-1&&d.first_failure.native_errno==ENOMEM&&d.first_failure.length==16384+65536);
 fail_map=0;p=ops.allocate_rw(&owner,16384,65536);CHECK(p);d=snapshot();CHECK(d.last.native_result==0&&d.last.native_errno==0&&d.first_failure.native_errno==ENOMEM);CHECK(ops.release(&owner,p,16384)==0&&mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);
 reset();open_owner();p=ops.allocate_rw(&owner,16384,16384);CHECK(p);fail_unmap=1;CHECK(ops.release(&owner,p,16384)==-1);d=snapshot();CHECK(d.first_failure.operation==MB_PE_PS5_MEMORY_OP_RELEASE&&d.first_failure.native_errno==EIO&&d.first_failure.length==32768);
 fail_unmap=0;CHECK(ops.release(&owner,p,16384)==0&&mb_pe_ps5_memory_close(&owner)==MB_PE_PS5_MEMORY_OK);CHECK(snapshot().first_failure.native_errno==EIO);
}
int main(void){admission();allocations_and_protection();retained_failure();module_cleanup();diagnostic_lifecycle();puts("PS5 memory adapter: 5 host fault/lifetime/diagnostic groups passed; host mappings only, no target or PE instructions executed");return 0;}
