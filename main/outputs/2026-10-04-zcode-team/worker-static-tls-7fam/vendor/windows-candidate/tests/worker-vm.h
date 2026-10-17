#ifndef MB_OWNED_WORKER_VM_H
#define MB_OWNED_WORKER_VM_H
/* Combined-payload fixture hook (worker-combined-dll-allfam): the KERNEL32
 * virtual-memory family through actual PE imports. All five census names are
 * exercised (UE4SS.dll family "heap": VirtualAlloc/VirtualFree/
 * VirtualProtect/VirtualQuery/VirtualQueryEx). The native runner installs a
 * malloc/free backend, so pages are writable host memory with no hardware
 * protection: the fixture verifies the documented bookkeeping semantics
 * (granularity-rounded reservation, commit state/protection/type through
 * query, protection change with old-protection return, exact-base release,
 * free-gap query, pseudo-handle QueryEx, foreign-process refusal). Error
 * returns -110..-117 are unique across this tree's fixtures (moduleenum
 * -100..-109, crtstartup -90..-97, fileio -80..-89). */
typedef __UINT32_TYPE__ mb_vm_u32;
typedef __UINT64_TYPE__ mb_vm_u64;
typedef __INT32_TYPE__ mb_vm_i32;
__declspec(dllimport) void *VirtualAlloc(void *, mb_vm_u32, mb_vm_u32, mb_vm_u32);
__declspec(dllimport) mb_vm_i32 VirtualFree(void *, mb_vm_u32, mb_vm_u32);
__declspec(dllimport) mb_vm_i32 VirtualProtect(void *, mb_vm_u32, mb_vm_u32, mb_vm_u32 *);
__declspec(dllimport) mb_vm_u64 VirtualQuery(const void *, void *, mb_vm_u32);
__declspec(dllimport) mb_vm_u64 VirtualQueryEx(void *, const void *, void *, mb_vm_u32);
#define MB_VM_MEM_COMMIT 0x00001000u
#define MB_VM_MEM_RESERVE 0x00002000u
#define MB_VM_MEM_RELEASE 0x00008000u
#define MB_VM_MEM_FREE 0x00010000u
#define MB_VM_MEM_PRIVATE 0x00020000u
#define MB_VM_PAGE_NOACCESS 0x01u
#define MB_VM_PAGE_READONLY 0x02u
#define MB_VM_PAGE_READWRITE 0x04u
/* Exact x64 MEMORY_BASIC_INFORMATION: 48 bytes; base 0/8, allocation_base 8/8,
 * allocation_protect 16/4, partition_id 20/4, region_size 24/8, state 32/4,
 * protect 36/4, type 40/4. */
struct mb_vm_basic_info {
    void *base_address;
    void *allocation_base;
    mb_vm_u32 allocation_protect;
    mb_vm_u32 partition_id;
    mb_vm_u64 region_size;
    mb_vm_u32 state;
    mb_vm_u32 protect;
    mb_vm_u32 type;
};
_Static_assert(sizeof(struct mb_vm_basic_info) == 48, "Win64 MBI size");
static volatile unsigned char mb_vm_touch;
static mb_vm_i32 mb_worker_vm_exercise(void) {
    void *base = VirtualAlloc(0, 1, MB_VM_MEM_RESERVE | MB_VM_MEM_COMMIT, MB_VM_PAGE_READWRITE);
    if(!base)return -110; /* one-byte request rounds to a full granularity span */
    mb_vm_touch = 0x5Au; /* committed pages are writable host memory */
    struct mb_vm_basic_info info = {0, 0, 0, 0, 0, 0, 0, 0};
    if(VirtualQuery(base, &info, (mb_vm_u32)sizeof(info)) != sizeof(info))return -111;
    if(info.base_address != base || info.allocation_base != base ||
       info.state != MB_VM_MEM_COMMIT || info.protect != MB_VM_PAGE_READWRITE ||
       info.type != MB_VM_MEM_PRIVATE || info.region_size < 0x10000u)return -112;
    mb_vm_u32 old = 0;
    if(!VirtualProtect(base, 4096, MB_VM_PAGE_READONLY, &old) ||
       old != MB_VM_PAGE_READWRITE)return -113;
    if(mb_vm_touch != 0x5Au)return -114; /* data survives the protection change */
    if(!VirtualFree(base, 0, MB_VM_MEM_RELEASE))return -115;
    if(VirtualFree(base, 0, MB_VM_MEM_RELEASE))return -116; /* double release refused */
    if(VirtualQuery(base, &info, (mb_vm_u32)sizeof(info)) != sizeof(info) ||
       info.state != MB_VM_MEM_FREE || info.protect != MB_VM_PAGE_NOACCESS)return -117;
    /* Pseudo-handle (-1) delegates; a foreign process handle is refused (0). */
    if(VirtualQueryEx((void *)(mb_vm_u64)-1, base, &info,
                      (mb_vm_u32)sizeof(info)) != sizeof(info))return -117;
    if(VirtualQueryEx((void *)(mb_vm_u64)0x1234, base, &info,
                      (mb_vm_u32)sizeof(info)) != 0)return -117;
    return 1;
}
#endif
