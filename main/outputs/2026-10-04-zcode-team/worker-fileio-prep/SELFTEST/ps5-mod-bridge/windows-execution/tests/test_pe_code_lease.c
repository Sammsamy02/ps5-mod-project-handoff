/* Reuse the existing deterministic owned PE builder/memory callbacks. Its test
 * entrypoint is renamed and never run here; this suite tests only code leases. */
#define main mb_pe_module_existing_controls_not_run
#include "test_pe_module.c"
#undef main

static void test_internal_code_and_lifetime(void) {
 fixture();CHECK(open_module(bind)==MB_PE_MODULE_OK);
 mb_pe_export_lease exported={0},worker={0},second={0};
 CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"Entry",5},&exported,&error)==MB_PE_MODULE_OK);
 /* This internal address has no export table entry. Acquire before scheduling
  * the worker, and retire the calling export independently. */
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&worker,&error)==MB_PE_MODULE_OK);
 CHECK(worker.owner==&module&&worker.address==storage+0x1020&&module.lease_count==2);
 CHECK(mb_pe_module_release(&exported)==MB_PE_MODULE_OK&&module.lease_count==1);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY&&release_calls==0&&module.state==MB_PE_MODULE_ACTIVE);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x12ff,&second,&error)==MB_PE_MODULE_OK); /* committed zero-fill tail */
 mb_pe_export_lease copy=worker;
 CHECK(mb_pe_module_release(&copy)==MB_PE_MODULE_BAD_STATE&&module.lease_count==2);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&worker,&error)==MB_PE_MODULE_BAD_STATE);
 CHECK(mb_pe_module_release(&worker)==MB_PE_MODULE_OK&&module.lease_count==1);
 CHECK(!worker.address&&!worker.owner&&!worker.next);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY&&release_calls==0);
 CHECK(mb_pe_module_release(&second)==MB_PE_MODULE_OK);
 CHECK(mb_pe_module_release(&worker)==MB_PE_MODULE_BAD_STATE);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&release_calls==1);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&worker,&error)==MB_PE_MODULE_BAD_STATE);
}
static void test_bounds_and_section_commit(void) {
 fixture();CHECK(open_module(bind)==MB_PE_MODULE_OK);
 mb_pe_export_lease lease={0};
 const uintptr_t rejected[]={ (uintptr_t)storage, (uintptr_t)storage+0x200,
  (uintptr_t)storage+0x1300, /* executable OS page padding is not a PE code section */
  (uintptr_t)storage+0x2000, /* committed writable data */
  (uintptr_t)storage+0x3000, /* inaccessible image gap */
  (uintptr_t)storage+0x4000, /* image end */
  (uintptr_t)storage-1, UINTPTR_MAX };
 CHECK(mb_pe_module_acquire_code(&module,NULL,&lease,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 for(size_t i=0;i<sizeof(rejected)/sizeof(*rejected);++i){
  CHECK(mb_pe_module_acquire_code(&module,(void*)rejected[i],&lease,&error)==MB_PE_MODULE_NOT_EXECUTABLE);
  CHECK(!lease.owner&&!lease.address&&!lease.next&&!module.lease_count);
 }
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK);
 /* A data section can share an RX protection granule; its own non-executable
  * characteristics still prevent admission. No W+X union is requested. */
 fixture();w32(0x1d4,0x40000040);ops.protection_granule=16384;
 CHECK(open_module(bind)==MB_PE_MODULE_OK&&rights[0]==5&&rights[2]==5);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x2000,&lease,&error)==MB_PE_MODULE_NOT_EXECUTABLE);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&lease,&error)==MB_PE_MODULE_OK);
 CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_OK);CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK);ops.protection_granule=4096;
}
static void test_nonactive_and_failed_cleanup(void) {
 mb_pe_export_lease lease={0};mb_pe_module inactive={0};
 const mb_pe_module_state states[]={MB_PE_MODULE_EMPTY,MB_PE_MODULE_LOADING,MB_PE_MODULE_INITIALIZING,MB_PE_MODULE_STOPPING,MB_PE_MODULE_CLEANUP};
 for(size_t i=0;i<sizeof(states)/sizeof(*states);++i){inactive.state=states[i];CHECK(mb_pe_module_acquire_code(&inactive,storage+0x1020,&lease,&error)==MB_PE_MODULE_BAD_STATE);CHECK(!lease.owner);}
 fixture();fail_protect=fail_release=1;
 CHECK(open_module(bind)==MB_PE_MODULE_CLEANUP_REQUIRED);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&lease,&error)==MB_PE_MODULE_BAD_STATE);
 fail_release=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK);
 entry_fixture();CHECK(open_entry(bind)==MB_PE_MODULE_OK);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&lease,&error)==MB_PE_MODULE_OK);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY&&detaches==0&&release_calls==0);
 CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_OK);fail_release=1;
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&detaches==1);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&lease,&error)==MB_PE_MODULE_BAD_STATE);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED&&detaches==1);
 fail_release=0;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK&&detaches==1);
}
static void test_argument_ownership(void) {
 fixture();CHECK(open_module(bind)==MB_PE_MODULE_OK);mb_pe_export_lease lease={0};
 CHECK(mb_pe_module_acquire_code(NULL,storage+0x1020,&lease,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,NULL,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,(mb_pe_export_lease*)&module,&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,(mb_pe_export_lease*)(file+0x600),&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,(mb_pe_export_lease*)(storage+0x2200),&error)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&lease,(mb_pe_module_error*)&lease)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&lease,(mb_pe_module_error*)file)==MB_PE_MODULE_INVALID_ARGUMENT);
 CHECK(!lease.owner&&!module.lease_count);
 CHECK(mb_pe_module_acquire_code(&module,storage+0x1020,&lease,NULL)==MB_PE_MODULE_OK);
 CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_OK);CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK);
}
int main(void){test_internal_code_and_lifetime();test_bounds_and_section_commit();test_nonactive_and_failed_cleanup();test_argument_ownership();puts("PE internal code lease: 4 focused groups passed; no PE instructions executed");return 0;}
