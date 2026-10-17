#ifndef MB_WIN_VM_H
#define MB_WIN_VM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Virtual-memory service family for the retained DLL imports.
 * Census (outputs/2026-10-04-zcode-team/worker-dll-mod-retention, both
 * baseline-dll-census.json and ue4ss-zip-census.json agree): UE4SS.dll imports
 * KERNEL32.dll VirtualAlloc, VirtualFree, VirtualProtect, VirtualQuery and
 * VirtualQueryEx (family "heap"); dwmapi.dll imports none of them. */

#define MB_WIN_VM_CURRENT_PROCESS ((void *)(uintptr_t)INT64_C(-1))

/* Windows dwAllocationGranularity defaults; owner config may override. */
#define MB_WIN_VM_PAGE_SIZE 4096u
#define MB_WIN_VM_GRANULARITY 65536u

/* Canonical x64 user address-space top used to bound free-region query. */
#define MB_WIN_VM_ADDRESS_LIMIT ((uintptr_t)INT64_C(0x0000800000000000))

/* MEM_* (winnt.h). Only the modeled subset is exposed. */
#define MB_WIN_VM_MEM_COMMIT UINT32_C(0x00001000)
#define MB_WIN_VM_MEM_RESERVE UINT32_C(0x00002000)
#define MB_WIN_VM_MEM_DECOMMIT UINT32_C(0x00004000)
#define MB_WIN_VM_MEM_RELEASE UINT32_C(0x00008000)
#define MB_WIN_VM_MEM_FREE UINT32_C(0x00010000)
#define MB_WIN_VM_MEM_PRIVATE UINT32_C(0x00020000)
#define MB_WIN_VM_MEM_MAPPED UINT32_C(0x00040000)
#define MB_WIN_VM_MEM_TOP_DOWN UINT32_C(0x00100000)

/* PAGE_* (winnt.h) protection set accepted by this owner. */
#define MB_WIN_VM_PAGE_NOACCESS UINT32_C(0x01)
#define MB_WIN_VM_PAGE_READONLY UINT32_C(0x02)
#define MB_WIN_VM_PAGE_READWRITE UINT32_C(0x04)
#define MB_WIN_VM_PAGE_WRITECOPY UINT32_C(0x08)
#define MB_WIN_VM_PAGE_EXECUTE UINT32_C(0x10)
#define MB_WIN_VM_PAGE_EXECUTE_READ UINT32_C(0x20)
#define MB_WIN_VM_PAGE_EXECUTE_READWRITE UINT32_C(0x40)
#define MB_WIN_VM_PAGE_EXECUTE_WRITECOPY UINT32_C(0x80)
#define MB_WIN_VM_PAGE_GUARD UINT32_C(0x100)
#define MB_WIN_VM_PAGE_NOCACHE UINT32_C(0x200)
#define MB_WIN_VM_PAGE_WRITECOMBINE UINT32_C(0x400)
#define MB_WIN_VM_PAGE_TARGETS_INVALID UINT32_C(0x40000000)
#define MB_WIN_VM_PAGE_TARGETS_NO_UPDATE UINT32_C(0x40000000)

/* Win32 error codes recorded through ops.error (kernelbase winerror.h). */
#define MB_WIN_VM_ERROR_INVALID_PARAMETER UINT32_C(87)
#define MB_WIN_VM_ERROR_INVALID_ADDRESS UINT32_C(487)
#define MB_WIN_VM_ERROR_NOT_COMMITTED UINT32_C(487)
#define MB_WIN_VM_ERROR_COMMITMENT_LIMIT UINT32_C(1455)

typedef enum mb_win_vm_status {
    MB_WIN_VM_OK = 0, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_BAD_STATE,
    MB_WIN_VM_BUSY, MB_WIN_VM_LIMIT, MB_WIN_VM_NO_MEMORY, MB_WIN_VM_CONFLICT,
    MB_WIN_VM_BACKEND_ERROR
} mb_win_vm_status;

/* Backend contract: allocate returns NULL or a dedicated writable region of
 * exactly `bytes` at an arbitrary host alignment; `bytes` is never zero.
 * release infallibly frees exactly a returned backend pointer/size pair.
 * protect is optional (may be NULL): receives a page-aligned range and the
 * win32 PAGE_* word for host-enforced protection; nonzero return means the
 * backend refused and the owner reports MB_WIN_VM_BACKEND_ERROR. error is
 * optional (may be NULL): records a win32 error code for the bridge to
 * publish through GetLastError; success never calls it. No callback may
 * reenter, mutate the owner, throw or longjmp; callbacks and context stay
 * alive until close succeeds. */
typedef struct mb_win_vm_ops {
    void *context;
    void *(*allocate)(void *, size_t);
    void (*release)(void *, void *, size_t);
    int (*protect)(void *, void *, size_t, uint32_t);
    void (*error)(void *, uint32_t);
} mb_win_vm_ops;

typedef struct mb_win_vm_config {
    uint32_t page_size;      /* 0 selects MB_WIN_VM_PAGE_SIZE */
    uint32_t granularity;    /* 0 selects MB_WIN_VM_GRANULARITY */
} mb_win_vm_config;

typedef struct mb_win_vm_region mb_win_vm_region;

typedef struct mb_win_vm {
    mb_win_vm_ops ops;
    mb_win_vm_config config;
    mb_win_vm_region *regions;   /* newest first, disjoint spans */
    size_t region_count, live_bytes;
    uint8_t active;
} mb_win_vm;

/* Win64 MEMORY_BASIC_INFORMATION (48 bytes, 8-aligned). */
typedef struct mb_win_vm_basic_info {
    void *base_address;
    void *allocation_base;
    uint32_t allocation_protect;
    uint32_t partition_id;
    size_t region_size;
    uint32_t state;
    uint32_t protect;
    uint32_t type;
} mb_win_vm_basic_info;
#define MB_WIN_VM_BASIC_INFO_SIZE ((uintptr_t)48)

/* Native C ABI only. Zero-initialize owner and every region before use. Do
 * not copy, move or mutate an active owner. Every operation is externally
 * serialized; there is no internal locking, TLS or CRT in this owner.
 *
 * open copies ops and config; zero config fields select the Windows x64
 * defaults. page_size and granularity must be powers of two, page_size <=
 * granularity and granularity % page_size == 0. close refuses with BUSY
 * while any region remains; it never releases backend memory implicitly.
 *
 * VirtualAlloc semantics (mb_win_vm_alloc):
 * - For MEM_RESERVE and MEM_RESERVE|MEM_COMMIT, lpAddress must be NULL:
 *   caller-chosen reservation bases are unsupported by this emulation
 *   because the backend picks host addresses. Non-NULL is
 *   INVALID_ARGUMENT (win32 error 87).
 * - For MEM_COMMIT alone, lpAddress must point inside exactly one live
 *   region of this owner; interior addresses round to page boundaries.
 * - flAllocationType must be MEM_RESERVE, MEM_RESERVE|MEM_COMMIT or
 *   MEM_COMMIT; other combinations are INVALID_ARGUMENT.
 * - MEM_RESERVE: size is rounded up to granularity; the backend span is
 *   aligned up to granularity; the returned base is the aligned base.
 * - MEM_COMMIT without MEM_RESERVE: lpAddress range is rounded down to
 *   page start and up to page end and must lie inside exactly one live
 *   region owned by this owner; otherwise MB_WIN_VM_CONFLICT (error 487).
 *   Committed pages are zero-filled on the reserve->commit transition
 *   (win32 guarantee); re-commit of live pages only updates protection.
 * - flProtect must be one plain PAGE_* base protection (guard/nocache/
 *   writecombine/targets modifiers are rejected as INVALID_ARGUMENT).
 * - NO_MEMORY reports backend allocation failure (win32 error 1455);
 *   LIMIT reports backend bookkeeping exhaustion.
 *
 * VirtualFree semantics (mb_win_vm_free):
 * - MEM_RELEASE requires lpAddress == region base and dwSize == 0 and
 *   releases the whole span; anything else is MB_WIN_VM_CONFLICT (87/487).
 * - MEM_DECOMMIT rounds the range to page boundaries; decommitting pages
 *   that are not committed does not fail (documented win32 behavior); a
 *   range outside every live region is MB_WIN_VM_CONFLICT (487).
 *   Decommit keeps pages reserved; later commit re-zeroes them.
 * - Combining both flags or passing NULL is INVALID_ARGUMENT.
 *
 * VirtualProtect semantics (mb_win_vm_protect):
 * - The range rounds to page boundaries and must be fully committed
 *   inside one live region; MB_WIN_VM_CONFLICT (487) otherwise.
 * - *old_protect receives the protection of the first page in range, as
 *   documented; the new protection applies to every page in range.
 * - A non-NULL ops.protect that refuses the change yields
 *   MB_WIN_VM_BACKEND_ERROR without changing bookkeeping.
 *
 * VirtualQuery semantics (mb_win_vm_query / mb_win_vm_query_ex):
 * - Ex validates process == MB_WIN_VM_CURRENT_PROCESS (87) and delegates.
 * - Addresses inside a region describe the maximal run of pages with the
 *   same committed state and protection starting at the queried page:
 *   base_address = run start, allocation_base = region base,
 *   allocation_protect = region's allocation protection, state = MEM_COMMIT
 *   or MEM_RESERVE, protect = page protection (committed) or
 *   PAGE_NOACCESS (reserved), type = MEM_PRIVATE, region_size = run bytes.
 * - Addresses outside every region describe the free gap: base_address is
 *   the queried address rounded down to the granularity-aligned gap start
 *   (region ends are granularity multiples), region_size runs to the next
 *   region base or ADDRESS_LIMIT, state = MEM_FREE, protect =
 *   PAGE_NOACCESS, allocation_base/protect/type = 0.
 * - Returns MB_WIN_VM_BASIC_INFO_SIZE on success; 0 with error 87 when
 *   the buffer is smaller than MB_WIN_VM_BASIC_INFO_SIZE or the output
 *   pointer is NULL. Addresses at or above ADDRESS_LIMIT are outside
 *   this emulation and fail with 87.
 *
 * Caller-supplied addresses are host pointers returned by this owner.
 * Foreign, interior-freed or stale pointers violate the caller contract.
 * system_info publishes page_size/granularity for a GetSystemInfo bridge.
 */
mb_win_vm_status mb_win_vm_open(mb_win_vm *, const mb_win_vm_ops *, const mb_win_vm_config *);
mb_win_vm_status mb_win_vm_close(mb_win_vm *);
void *mb_win_vm_alloc(mb_win_vm *, void *address, size_t size, uint32_t allocation_type, uint32_t protect);
mb_win_vm_status mb_win_vm_free(mb_win_vm *, void *address, size_t size, uint32_t free_type);
mb_win_vm_status mb_win_vm_protect(mb_win_vm *, void *address, size_t size, uint32_t new_protect, uint32_t *old_protect);
size_t mb_win_vm_query(mb_win_vm *, const void *address, mb_win_vm_basic_info *, size_t length);
size_t mb_win_vm_query_ex(mb_win_vm *, void *process, const void *address, mb_win_vm_basic_info *, size_t length);
void mb_win_vm_system_info(const mb_win_vm *, uint32_t *page_size, uint32_t *allocation_granularity);

#ifdef __cplusplus
}
#endif
#endif
