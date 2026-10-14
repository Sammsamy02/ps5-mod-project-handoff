/* Real parser/preparer and adapter with deterministic caller memory operations.
 * Catches missed admission gates, W+X page unions, callable publication on
 * failure, lost allocation ownership, and premature unload during a call. */
#include "../pe_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned char file[0x800];
static _Alignas(16384) unsigned char storage[0x8000];
static unsigned allocation_calls, release_calls, protect_calls, cache_calls, bind_calls;
static int fail_protect, fail_cache, fail_release, bad_alignment;
static unsigned rights[8];
static mb_pe_module module;
static mb_pe_module_error error;
static void w16(unsigned p,unsigned v){file[p]=(unsigned char)v;file[p+1]=(unsigned char)(v>>8);}
static void w32(unsigned p,uint32_t v){for(unsigned i=0;i<4;i++)file[p+i]=(unsigned char)(v>>(8*i));}
static void w64(unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)file[p+i]=(unsigned char)(v>>(8*i));}
static void dir(unsigned i,unsigned rva,unsigned n){w32(0x108+i*8,rva);w32(0x10c+i*8,n);}
static void fixture(void){
 memset(file,0,sizeof(file));memset(storage,0xcc,sizeof(storage));memset(&module,0,sizeof(module));
 allocation_calls=release_calls=protect_calls=cache_calls=bind_calls=0;fail_protect=fail_cache=fail_release=bad_alignment=0;memset(rights,0xff,sizeof(rights));
 w16(0,0x5a4d);w32(0x3c,0x80);w32(0x80,0x4550);w16(0x84,0x8664);w16(0x86,2);w16(0x94,240);w16(0x96,0x2022);w16(0x98,0x20b);
 w64(0xb0,UINT64_C(0x180000000));w32(0xb8,0x1000);w32(0xbc,0x200);w32(0xd0,0x4000);w32(0xd4,0x200);w32(0x104,16);
 w32(0x190,0x300);w32(0x194,0x1000);w32(0x198,0x200);w32(0x19c,0x200);w32(0x1ac,0x60000020);
 w32(0x1b8,0x500);w32(0x1bc,0x2000);w32(0x1c0,0x400);w32(0x1c4,0x400);w32(0x1d4,0xc0000040);
 file[0x200]=0xc3;w64(0x208,UINT64_C(0x180001234));
 dir(5,0x2000,12);w32(0x400,0x1000);w32(0x404,12);w16(0x408,0xa008);
 dir(1,0x2040,40);w32(0x440,0x20a0);w32(0x44c,0x2080);w32(0x450,0x20c0);memcpy(file+0x480,"Test.dll",9);w64(0x4a0,0x20e0);w64(0x4c0,0x20e0);memcpy(file+0x4e2,"Imported",9);
 dir(0,0x2100,0x90);w32(0x510,10);w32(0x514,3);w32(0x518,1);w32(0x51c,0x2140);w32(0x520,0x2150);w32(0x524,0x2160);
 w32(0x540,0x1000);w32(0x544,0x2170);w32(0x548,0x2200);w32(0x550,0x2190);memcpy(file+0x570,"Other.Forward",14);memcpy(file+0x590,"Entry",6);
}
static void *allocate(void*c,size_t n,size_t alignment){(void)c;++allocation_calls;CHECK(n<=sizeof(storage));CHECK(alignment<=16384);return storage+bad_alignment;}
static int protect(void*c,void*b,size_t p,size_t n,unsigned r){(void)c;CHECK(b==storage);CHECK(p%4096==0&&n%4096==0&&p+n<=sizeof(storage));CHECK((r&6)!=6);++protect_calls;if(fail_protect)return -1;for(size_t i=p/4096;i<(p+n)/4096;i++)rights[i]=r;return 0;}
static int flush(void*c,void*b,size_t n){(void)c;CHECK(b==storage&&n==0x4000);++cache_calls;return fail_cache?-1:0;}
static int release(void*c,void*b,size_t n){(void)c;CHECK(b==storage+bad_alignment&&n==0x4000);++release_calls;return fail_release?-1:0;}
static mb_pe_bind_result bind(void*c,const mb_pe_import*i,uint64_t*v){(void)c;CHECK(i->name.size==8&&!memcmp(i->name.data,"Imported",8));++bind_calls;*v=UINT64_C(0x12345000);return MB_PE_BIND_BOUND;}
static mb_pe_bind_result zero_bind(void*c,const mb_pe_import*i,uint64_t*v){(void)c;(void)i;*v=0;return MB_PE_BIND_BOUND;}
static mb_pe_memory_ops ops={NULL,4096,allocate,protect,flush,release};
static mb_pe_module_status open_module(mb_pe_binder b){return mb_pe_module_open(&module,file,sizeof(file),&ops,b,NULL,&error);}
static unsigned attaches, detaches;
static int entry_context;
static int32_t attach_result;
static int32_t invoke_entry(void *context,void *entry,void *base,uint32_t reason,void *reserved){
 CHECK(context==&entry_context&&entry==storage+0x1000&&base==storage&&reserved==NULL);
 CHECK(cache_calls==1&&rights[1]==5&&module.report.imports_bound==1&&release_calls==0);
 if(reason==1){CHECK(module.state==MB_PE_MODULE_INITIALIZING&&attaches==0&&detaches==0);++attaches;return attach_result;}
 CHECK(reason==0&&module.state==MB_PE_MODULE_STOPPING&&attaches==1&&detaches==0);++detaches;return 0;
}
static const mb_pe_entry_ops entry_ops={&entry_context,invoke_entry};
static void entry_fixture(void){fixture();w32(0xa8,0x1000);attaches=detaches=0;attach_result=-7;}
static mb_pe_module_status open_entry(mb_pe_binder binder){return mb_pe_module_open_with_entry(&module,file,sizeof(file),&ops,&entry_ops,binder,NULL,&error);}
static void test_entry_lifecycle(void){
 /* Startup must happen once after preparation, and any nonzero BOOL succeeds. */
 entry_fixture();CHECK(open_entry(bind)==MB_PE_MODULE_OK&&attaches==1&&detaches==0&&module.state==MB_PE_MODULE_ACTIVE);
 mb_pe_export_lease lease={0};CHECK(mb_pe_module_acquire_ordinal(&module,10,&lease,&error)==MB_PE_MODULE_OK);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY&&detaches==0);CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_OK);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&detaches==1&&release_calls==1);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&detaches==1);
 /* Rejected attach receives one detach before cleanup, preserving rejection
  * through every failed release retry without rerunning the entry point. */
 entry_fixture();attach_result=0;CHECK(open_entry(bind)==MB_PE_MODULE_ENTRY_REJECTED&&attaches==1&&detaches==1&&release_calls==1&&module.state==MB_PE_MODULE_EMPTY);
 entry_fixture();attach_result=0;fail_release=1;CHECK(open_entry(bind)==MB_PE_MODULE_CLEANUP_REQUIRED&&error.cause==MB_PE_MODULE_ENTRY_REJECTED&&detaches==1);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&error.cause==MB_PE_MODULE_ENTRY_REJECTED&&detaches==1);fail_release=0;
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&detaches==1&&release_calls==3);
 /* Successful attachment also detaches once even if deallocation needs retry. */
 entry_fixture();CHECK(open_entry(bind)==MB_PE_MODULE_OK);fail_release=1;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&detaches==1);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&detaches==1);fail_release=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&detaches==1);
 /* The opt-in does not grant unsupported runtime features or callable data. */
 entry_fixture();CHECK(open_module(bind)==MB_PE_MODULE_UNSUPPORTED&&attaches==0&&allocation_calls==0);
 entry_fixture();w32(0xa8,0x2200);CHECK(open_entry(bind)==MB_PE_MODULE_NOT_EXECUTABLE&&attaches==0&&allocation_calls==0);
 entry_fixture();dir(9,0x2200,8);CHECK(open_entry(bind)==MB_PE_MODULE_UNSUPPORTED&&attaches==0&&allocation_calls==0);
 entry_fixture();CHECK(mb_pe_module_open_with_entry(&module,file,sizeof(file),&ops,NULL,bind,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT&&attaches==0&&allocation_calls==0);
 entry_fixture();mb_pe_entry_ops invalid_entry={0};CHECK(mb_pe_module_open_with_entry(&module,file,sizeof(file),&ops,&invalid_entry,bind,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT&&allocation_calls==0);
 entry_fixture();CHECK(open_entry(NULL)==MB_PE_MODULE_UNRESOLVED_IMPORTS&&attaches==0&&detaches==0);
 entry_fixture();fail_protect=1;CHECK(open_entry(bind)==MB_PE_MODULE_MEMORY_ERROR&&attaches==0&&detaches==0);
 entry_fixture();fail_cache=1;CHECK(open_entry(bind)==MB_PE_MODULE_MEMORY_ERROR&&attaches==0&&detaches==0);
 entry_fixture();w32(0xa8,0);CHECK(open_entry(bind)==MB_PE_MODULE_OK&&attaches==0);CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&detaches==0);
}
int main(void){
 fixture();CHECK(open_module(bind)==MB_PE_MODULE_OK);CHECK(module.state==MB_PE_MODULE_ACTIVE&&bind_calls==1&&cache_calls==1);CHECK(rights[0]==1&&rights[1]==5&&rights[2]==3&&rights[3]==0);
 mb_pe_export_lease lease={0};CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"Entry",5},&lease,&error)==MB_PE_MODULE_OK);CHECK(lease.address==storage+0x1000&&module.lease_count==1);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY&&release_calls==0);mb_pe_export_lease copied=lease;CHECK(mb_pe_module_release(&copied)==MB_PE_MODULE_BAD_STATE&&module.lease_count==1);
 CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_OK&&lease.address==NULL);CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_BAD_STATE);
 CHECK(mb_pe_module_acquire_ordinal(&module,11,&lease,&error)==MB_PE_MODULE_FORWARDER);CHECK(mb_pe_module_acquire_ordinal(&module,12,&lease,&error)==MB_PE_MODULE_NOT_EXECUTABLE);CHECK(mb_pe_module_acquire_ordinal(&module,99,&lease,&error)==MB_PE_MODULE_NOT_FOUND);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&release_calls==1&&module.state==MB_PE_MODULE_EMPTY);CHECK(mb_pe_module_acquire_ordinal(&module,10,&lease,&error)==MB_PE_MODULE_BAD_STATE);
 fixture();w32(0xa8,0x1000);CHECK(open_module(bind)==MB_PE_MODULE_UNSUPPORTED&&allocation_calls==0);
 const unsigned forbidden[]={3,7,8,9,10,11,13,14,15};for(unsigned i=0;i<sizeof(forbidden)/sizeof(*forbidden);i++){fixture();dir(forbidden[i],0x2200,8);CHECK(open_module(bind)==MB_PE_MODULE_UNSUPPORTED&&allocation_calls==0);}
 fixture();w16(0xde,0x4000);CHECK(open_module(bind)==MB_PE_MODULE_UNSUPPORTED&&allocation_calls==0);
 fixture();ops.protection_granule=16384;CHECK(open_module(bind)==MB_PE_MODULE_UNSUPPORTED&&allocation_calls==0);ops.protection_granule=4096;
 fixture();CHECK(open_module(NULL)==MB_PE_MODULE_UNRESOLVED_IMPORTS&&release_calls==1&&protect_calls==0&&module.state==MB_PE_MODULE_EMPTY);
 fixture();CHECK(open_module(zero_bind)==MB_PE_MODULE_UNRESOLVED_IMPORTS&&release_calls==1&&protect_calls==0);
 fixture();fail_release=1;CHECK(open_module(NULL)==MB_PE_MODULE_CLEANUP_REQUIRED&&error.cause==MB_PE_MODULE_UNRESOLVED_IMPORTS&&error.image.import.name.size==8);CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&error.cause==MB_PE_MODULE_UNRESOLVED_IMPORTS&&error.image.code==MB_PE_UNRESOLVED_IMPORTS&&error.image.import.name.size==8);fail_release=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&release_calls==3);
 fixture();fail_protect=1;CHECK(open_module(bind)==MB_PE_MODULE_MEMORY_ERROR&&release_calls==1&&module.state==MB_PE_MODULE_EMPTY);
 fixture();fail_protect=fail_release=1;CHECK(open_module(bind)==MB_PE_MODULE_CLEANUP_REQUIRED&&module.state==MB_PE_MODULE_CLEANUP&&module.image==storage&&error.cause==MB_PE_MODULE_MEMORY_ERROR);CHECK(mb_pe_module_acquire_ordinal(&module,10,&lease,&error)==MB_PE_MODULE_BAD_STATE);fail_release=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&release_calls==2);
 fixture();fail_cache=1;CHECK(open_module(bind)==MB_PE_MODULE_MEMORY_ERROR&&release_calls==1&&module.state==MB_PE_MODULE_EMPTY);
 fixture();CHECK(open_module(bind)==MB_PE_MODULE_OK);fail_release=1;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&module.image==storage);fail_release=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&module.image==NULL);
 fixture();bad_alignment=1;CHECK(open_module(bind)==MB_PE_MODULE_MEMORY_ERROR&&release_calls==1&&bind_calls==0);
 fixture();ops.protection_granule=3000;CHECK(open_module(bind)==MB_PE_MODULE_INVALID_ARGUMENT&&allocation_calls==0);ops.protection_granule=4096;
 test_entry_lifecycle();
 puts("PE execution adapter admission, protections, entry lifecycle, failure ownership and lease tests passed; no PE instructions executed");return 0;
}
