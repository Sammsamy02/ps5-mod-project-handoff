#ifndef MB_PE_PS5_MEMORY_H
#define MB_PE_PS5_MEMORY_H
#include "pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MB_PE_PS5_MEMORY_GRANULE ((size_t)16384)
typedef enum mb_pe_ps5_memory_status {
    MB_PE_PS5_MEMORY_OK = 0, MB_PE_PS5_MEMORY_INVALID_ARGUMENT,
    MB_PE_PS5_MEMORY_BAD_STATE, MB_PE_PS5_MEMORY_UNSUPPORTED,
    MB_PE_PS5_MEMORY_BUSY
} mb_pe_ps5_memory_status;
typedef enum mb_pe_ps5_memory_operation {
    MB_PE_PS5_MEMORY_OP_NONE = 0, MB_PE_PS5_MEMORY_OP_ALLOCATE,
    MB_PE_PS5_MEMORY_OP_PROTECT, MB_PE_PS5_MEMORY_OP_RELEASE
} mb_pe_ps5_memory_operation;
typedef struct mb_pe_ps5_memory_operation_result {
    mb_pe_ps5_memory_operation operation;
    int32_t native_result, native_errno;
    size_t offset, length;
    unsigned rights;
} mb_pe_ps5_memory_operation_result;
typedef struct mb_pe_ps5_memory_diagnostics {
    mb_pe_ps5_memory_operation_result last, first_failure;
} mb_pe_ps5_memory_diagnostics;
typedef struct mb_pe_ps5_memory_slot {
    void *mapping, *image;
    size_t mapping_size, image_size;
} mb_pe_ps5_memory_slot;
typedef struct mb_pe_ps5_memory {
    mb_pe_ps5_memory_slot *slots;
    size_t capacity, live_allocations;
    unsigned active;
    mb_pe_ps5_memory_diagnostics diagnostics;
} mb_pe_ps5_memory;

/* Native PS5 SDK payload adapter. Zero-initialize owner and caller-owned slots;
 * keep them stable, disjoint and alive through every module cleanup. Serialize
 * all callbacks/controls externally and close all modules before closing owner.
 * Slots and owner fields are private while active. Output ops is a separate
 * object, copied by mb_pe_module; old copies must not be used after owner close.
 *
 * Open requires x86_64 Prospero and getpagesize()==16384. Protection calls the
 * native sceKernelMprotect range syscall wrapper directly. Its declaration and
 * saved native wrapper were reviewed; target executable permission is untested.
 * Nonzero native errors (including execution denial) propagate to module cleanup.
 * This deliberately avoids SDK libc mprotect's executable kernel_mprotect hook:
 * that hook adjusts whole VM entries and cannot promise exact subrange changes.
 * The saved AsmJit payload success used that different route; it does not prove
 * this adapter executes on target. No provider is installed or modified here.
 *
 * Each image owns one anonymous private RW, non-executable mapping. Extra space
 * supplies requested power-of-two alignment without fixed-address placement or
 * partial unmapping. The PE image keeps the SAME base for writes/relocations and
 * execution; no writable/executable aliases exist. Protection rejects W+X and
 * acts only on exact 16 KiB ranges inside the image. Failed protection retains
 * mapping ownership. A failed whole-mapping munmap leaves its slot untouched;
 * the module's CLEANUP retry remains the sole owner of its release obligation.
 * Owner close is BUSY while any mapping remains; it never frees live modules.
 *
 * Cache publication uses the x86 coherent instruction-cache contract also used
 * by the retained AsmJit port, plus a compiler memory barrier. Images must finish
 * preparation before publication; concurrent code patching is unsupported.
 * No console execution is implied by compiling/linking this adapter.
 */
mb_pe_ps5_memory_status mb_pe_ps5_memory_open(mb_pe_ps5_memory *,
    mb_pe_ps5_memory_slot *, size_t capacity, mb_pe_memory_ops *);
mb_pe_ps5_memory_status mb_pe_ps5_memory_close(mb_pe_ps5_memory *);
/* Address-free snapshots of actual native calls, valid on a zero-initialized,
 * active or closed owner. Output is a disjoint caller-owned object. NONE means
 * no operation/failure has been recorded in this owner lifecycle. Validation or
 * capacity refusals and cache publication do not perform native calls and leave
 * these snapshots unchanged; they are not evidence of native denial.
 *
 * Every native call replaces last. first_failure captures the first failing
 * native call only; later successes or failures and successful close retain it.
 * Both snapshots reset on the next successful open. Thus first_failure describes
 * the owner lifecycle, NOT necessarily the most recent callback's result.
 *
 * PROTECT stores the exact signed 32-bit sceKernelMprotect result and requested
 * image-relative offset/length/rights; native_errno is zero. ALLOCATE records
 * native_result 0 for success or -1 for MAP_FAILED. RELEASE stores munmap's
 * actual result. Their length is the whole native mapping size (including
 * alignment slack), offset is zero, rights are RW for ALLOCATE or zero for
 * RELEASE; their native_errno is sampled immediately on failure, else zero.
 * No image, mapping or function addresses appear in either snapshot.
 */
mb_pe_ps5_memory_status mb_pe_ps5_memory_get_diagnostics(const mb_pe_ps5_memory *,
    mb_pe_ps5_memory_diagnostics *);

#ifdef __cplusplus
}
#endif
#endif
