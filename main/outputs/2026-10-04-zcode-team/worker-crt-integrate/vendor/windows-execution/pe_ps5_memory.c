#include "pe_ps5_memory.h"
#include <errno.h>
#if defined(MB_PE_PS5_MEMORY_TEST)
/* Only the host fault tests replace calls. Production has no callback fallback. */
#include "tests/ps5_memory_backend.h"
#else
#if !defined(__PROSPERO__) || !defined(__x86_64__)
#error This memory adapter requires the native x86_64 Prospero SDK
#endif
#include <sys/mman.h>
#include <unistd.h>
/* Retained prosperodev-libprospero 3cdd09cc libkernel.h:150. The saved native
 * wrapper forwards addr/length/prot to SYS_mprotect; do not substitute libc's
 * mprotect, whose EXEC branch uses the SDK's coarse VM-entry provider. */
extern int sceKernelMprotect(const void *, size_t, int);
_Static_assert(sizeof(off_t)==8, "PS5 mmap requires 64-bit off_t");
_Static_assert(PROT_NONE==0 && PROT_READ==1 && PROT_WRITE==2 && PROT_EXEC==4,
    "Pinned PS5 protection constants changed");
_Static_assert(MAP_PRIVATE==2 && MAP_ANON==0x1000, "Pinned PS5 mapping constants changed");
#define MB_NATIVE_PROT_READ PROT_READ
#define MB_NATIVE_PROT_WRITE PROT_WRITE
#define MB_NATIVE_PROT_EXEC PROT_EXEC
#define MB_NATIVE_MAP_PRIVATE MAP_PRIVATE
#define MB_NATIVE_MAP_ANON MAP_ANON
#define MB_NATIVE_MAP_FAILED MAP_FAILED
#define MB_NATIVE_PAGESIZE getpagesize
#define MB_NATIVE_MAP mmap
#define MB_NATIVE_PROTECT sceKernelMprotect
#define MB_NATIVE_UNMAP munmap
#endif
_Static_assert(sizeof(void*)==8 && sizeof(size_t)==8 && sizeof(int)==4,
    "This adapter requires the PS5 native data model");

static void record(mb_pe_ps5_memory *owner, mb_pe_ps5_memory_operation operation,
    int status, int native_errno, size_t offset, size_t length, unsigned rights) {
    mb_pe_ps5_memory_operation_result current={operation,status,native_errno,offset,length,rights};
    owner->diagnostics.last=current;
    if (status && owner->diagnostics.first_failure.operation==MB_PE_PS5_MEMORY_OP_NONE)
        owner->diagnostics.first_failure=current;
}
static mb_pe_ps5_memory_slot *find(mb_pe_ps5_memory *owner, void *image) {
    size_t i;
    if (!owner || !owner->active || !image) return NULL;
    for (i=0; i<owner->capacity; ++i)
        if (owner->slots[i].mapping_size && owner->slots[i].image==image) return &owner->slots[i];
    return NULL;
}
static void *allocate_rw(void *context, size_t size, size_t alignment) {
    mb_pe_ps5_memory *owner=context;
    mb_pe_ps5_memory_slot *slot=NULL;
    size_t i, total, offset;
    void *mapping;
    if (!owner || !owner->active || !size || size%MB_PE_PS5_MEMORY_GRANULE ||
        alignment<MB_PE_PS5_MEMORY_GRANULE || (alignment&(alignment-1)) ||
        alignment>(size_t)PTRDIFF_MAX || size>(size_t)PTRDIFF_MAX-alignment) return NULL;
    for (i=0; i<owner->capacity; ++i) if (!owner->slots[i].mapping_size) {slot=&owner->slots[i];break;}
    if (!slot) return NULL;
    total=size+alignment;
    mapping=MB_NATIVE_MAP(NULL,total,MB_NATIVE_PROT_READ|MB_NATIVE_PROT_WRITE,
        MB_NATIVE_MAP_PRIVATE|MB_NATIVE_MAP_ANON,-1,0);
    record(owner,MB_PE_PS5_MEMORY_OP_ALLOCATE,mapping==MB_NATIVE_MAP_FAILED?-1:0,
        mapping==MB_NATIVE_MAP_FAILED?errno:0,0,total,MB_PE_MEMORY_READ|MB_PE_MEMORY_WRITE);
    if (mapping==MB_NATIVE_MAP_FAILED) return NULL;
    /* Native mmap owns this complete valid range. No fallible operation follows
     * allocation: extra alignment space also handles a mapping at address zero. */
    offset=((uintptr_t)0-(uintptr_t)mapping)&(alignment-1);
    if (!mapping) offset=alignment;
    slot->mapping=mapping; slot->mapping_size=total;
    slot->image=(void*)((uintptr_t)mapping+offset); slot->image_size=size;
    ++owner->live_allocations;
    return slot->image;
}
static int protect(void *context, void *image, size_t offset, size_t length, unsigned rights) {
    mb_pe_ps5_memory *owner=context;
    mb_pe_ps5_memory_slot *slot=find(owner,image);
    int native=0, status;
    if (!slot || !length || offset%MB_PE_PS5_MEMORY_GRANULE || length%MB_PE_PS5_MEMORY_GRANULE ||
        offset>slot->image_size || length>slot->image_size-offset || (rights&~7u) || (rights&6u)==6u) return -1;
    if (rights&MB_PE_MEMORY_READ) native|=MB_NATIVE_PROT_READ;
    if (rights&MB_PE_MEMORY_WRITE) native|=MB_NATIVE_PROT_WRITE;
    if (rights&MB_PE_MEMORY_EXECUTE) native|=MB_NATIVE_PROT_EXEC;
    status=MB_NATIVE_PROTECT((unsigned char*)image+offset,length,native);
    record(owner,MB_PE_PS5_MEMORY_OP_PROTECT,status,0,offset,length,rights);
    return status;
}
static int flush_instruction_cache(void *context, void *image, size_t length) {
    mb_pe_ps5_memory_slot *slot=find(context,image);
    if (!slot || length!=slot->image_size) return -1;
#if !defined(MB_PE_PS5_MEMORY_TEST)
    /* x86_64 has coherent instruction/data caches (retained AsmJit contract).
     * Clang lowers this builtin to no cache-maintenance instructions on x86. */
    __builtin___clear_cache((char*)image,(char*)image+length);
#endif
    __asm__ __volatile__("" ::: "memory");
    return 0;
}
static int release(void *context, void *image, size_t length) {
    mb_pe_ps5_memory *owner=context;
    mb_pe_ps5_memory_slot *slot=find(owner,image);
    int status;
    if (!slot || length!=slot->image_size) return -1;
    status=MB_NATIVE_UNMAP(slot->mapping,slot->mapping_size);
    record(owner,MB_PE_PS5_MEMORY_OP_RELEASE,status,status?errno:0,0,slot->mapping_size,0);
    if (status) return status;
    *slot=(mb_pe_ps5_memory_slot){0};
    --owner->live_allocations;
    return 0;
}
mb_pe_ps5_memory_status mb_pe_ps5_memory_open(mb_pe_ps5_memory *owner,
    mb_pe_ps5_memory_slot *slots, size_t capacity, mb_pe_memory_ops *ops) {
    size_t i;
    if (!owner || !slots || !capacity || capacity>SIZE_MAX/sizeof(*slots) || !ops)
        return MB_PE_PS5_MEMORY_INVALID_ARGUMENT;
    if (owner->active || owner->slots || owner->capacity || owner->live_allocations)
        return MB_PE_PS5_MEMORY_BAD_STATE;
    for (i=0; i<capacity; ++i)
        if (slots[i].mapping || slots[i].image || slots[i].mapping_size || slots[i].image_size)
            return MB_PE_PS5_MEMORY_BAD_STATE;
    if (MB_NATIVE_PAGESIZE()!=16384)
        return MB_PE_PS5_MEMORY_UNSUPPORTED;
    owner->diagnostics=(mb_pe_ps5_memory_diagnostics){0};
    owner->slots=slots; owner->capacity=capacity; owner->active=1;
    *ops=(mb_pe_memory_ops){owner,MB_PE_PS5_MEMORY_GRANULE,allocate_rw,protect,flush_instruction_cache,release};
    return MB_PE_PS5_MEMORY_OK;
}
mb_pe_ps5_memory_status mb_pe_ps5_memory_close(mb_pe_ps5_memory *owner) {
    if (!owner) return MB_PE_PS5_MEMORY_INVALID_ARGUMENT;
    if (owner->live_allocations) return MB_PE_PS5_MEMORY_BUSY;
    if (!owner->active && (owner->slots || owner->capacity)) return MB_PE_PS5_MEMORY_BAD_STATE;
    owner->slots=NULL; owner->capacity=0; owner->live_allocations=0; owner->active=0;
    return MB_PE_PS5_MEMORY_OK;
}
mb_pe_ps5_memory_status mb_pe_ps5_memory_get_diagnostics(const mb_pe_ps5_memory *owner,
    mb_pe_ps5_memory_diagnostics *out) {
    if (!owner || !out) return MB_PE_PS5_MEMORY_INVALID_ARGUMENT;
    *out=owner->diagnostics;
    return MB_PE_PS5_MEMORY_OK;
}
