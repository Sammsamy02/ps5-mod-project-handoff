#ifndef MB_PE_MODULE_H
#define MB_PE_MODULE_H
#include "../windows-image/pe_image.h"
#ifdef __cplusplus
extern "C" {
#endif

enum mb_pe_memory_rights { MB_PE_MEMORY_READ = 1u, MB_PE_MEMORY_WRITE = 2u, MB_PE_MEMORY_EXECUTE = 4u };
typedef enum mb_pe_module_status {
    MB_PE_MODULE_OK = 0, MB_PE_MODULE_INVALID_ARGUMENT, MB_PE_MODULE_BAD_STATE,
    MB_PE_MODULE_IMAGE_ERROR, MB_PE_MODULE_UNSUPPORTED, MB_PE_MODULE_UNRESOLVED_IMPORTS,
    MB_PE_MODULE_MEMORY_ERROR, MB_PE_MODULE_CLEANUP_REQUIRED, MB_PE_MODULE_BUSY,
    MB_PE_MODULE_NOT_FOUND, MB_PE_MODULE_FORWARDER, MB_PE_MODULE_NOT_EXECUTABLE,
    MB_PE_MODULE_LIMIT, MB_PE_MODULE_ENTRY_REJECTED
} mb_pe_module_status;
typedef enum mb_pe_module_state {
    MB_PE_MODULE_EMPTY = 0, MB_PE_MODULE_LOADING, MB_PE_MODULE_ACTIVE, MB_PE_MODULE_CLEANUP,
    MB_PE_MODULE_INITIALIZING, MB_PE_MODULE_STOPPING
} mb_pe_module_state;
typedef struct mb_pe_memory_ops {
    void *context;
    size_t protection_granule;
    void *(*allocate_rw)(void *, size_t size, size_t alignment);
    int (*protect)(void *, void *base, size_t offset, size_t length, unsigned rights);
    int (*flush_instruction_cache)(void *, void *base, size_t length);
    int (*release)(void *, void *base, size_t length);
} mb_pe_memory_ops;
typedef struct mb_pe_entry_ops {
    void *context;
    /* Native C callback; it must call entry_address using the Microsoft x64
     * BOOL(HINSTANCE, DWORD, LPVOID) ABI and return that signed 32-bit BOOL. */
    int32_t (*invoke)(void *, void *entry_address, void *image_base, uint32_t reason, void *reserved);
} mb_pe_entry_ops;
typedef struct mb_pe_module_error {
    mb_pe_module_status code, cause; /* cause preserves the failure if cleanup fails */
    mb_pe_error image;
    uint32_t detail; /* unsupported directory index/flags, or memory stage 1/2/3/4 */
} mb_pe_module_error;
struct mb_pe_module;
typedef struct mb_pe_export_lease {
    void *address;
    struct mb_pe_module *owner;
    struct mb_pe_export_lease *next;
} mb_pe_export_lease;
typedef struct mb_pe_module {
    mb_pe_module_state state;
    void *image;
    size_t allocation_size;
    const void *file;
    size_t file_size;
    mb_pe_memory_ops memory;
    mb_pe_info info;
    mb_pe_report report;
    mb_pe_module_error failure;
    mb_pe_export_lease *leases;
    uint32_t lease_count;
    mb_pe_entry_ops entry;
    uint8_t entry_attached;
    /* worker-static-tls-7fam admission ticket (vendored adaptation): set by
     * the composition before open while the module is EMPTY (e.g. from a
     * loader service_begin). When non-NULL, admission additionally accepts
     * MB_PE_REQUIRES_TLS and data directory 9 so a static-TLS session can
     * parse and own the directory. Opaque: never dereferenced or retained;
     * open_impl reads it before zeroing the module. Default (NULL) keeps the
     * exact legacy closed-allowlist refusal of TLS-bearing images. */
    const void *tls_admission;
} mb_pe_module;

/* Zero-initialize module and leases before first use. They must not be moved,
 * copied, or mutated while owned by this API. Access is single-threaded or
 * externally synchronized. File bytes stay immutable/alive until close succeeds;
 * imported implementations, entry callback/context, and their dependencies stay
 * alive for the same time. Entry ops are copied, so their source struct need not.
 * The module, leases, file, ops, and error outputs must be disjoint objects.
 *
 * allocate_rw returns a dedicated, writable, non-executable region of requested
 * size/alignment, or NULL. Integer MEMORY callbacks return 0 on success. protect
 * acts on exact granule-aligned subranges; rights zero means inaccessible. A
 * failed release MUST retain the whole allocation for a later release retry.
 * Callbacks may not reenter/mutate these objects or throw/longjmp. The dedicated
 * image allocation and its established protections must not be changed outside
 * this owner while it is ACTIVE or leased.
 *
 * Only AMD64 images without runtime directories beyond exports/imports/resource/
 * security/reloc/debug/IAT are admitted. Compiled with MB_PE_SEH64, the
 * exception directory (unwind/.pdata) is additionally admitted when it lies
 * inside the image: the SEH64 family (windows-services/mb_win_seh64*) then
 * registers it with its dynamic unwind registry for the module's lifetime
 * and provides the exception dispatch services (design §7 amendment).
 * Without that switch images with unwind data stay unadmittable.
 * open requires /noentry. open_with_entry
 * explicitly permits an entry RVA inside an executable section. It requires a
 * nonnull entry invoke callback, even for zero-entry images, which invoke nothing.
 * No TLS/unwind/CFG/CLR support or separate CRT initialization is provided.
 * All imports must bind to nonzero callable/data
 * addresses supplied by the caller, with the proper ABI. Header checks do not
 * prove that arbitrary instructions can run without other Windows services.
 *
 * RW preparation is followed by W^X protection and cache synchronization.
 * open_with_entry then invokes process attach (reason 1, image base, NULL) while
 * INITIALIZING. Its native-C invoke callback must return the actual Windows
 * BOOL: any nonzero int32_t publishes ACTIVE, zero invokes process detach
 * (reason 0, image base, NULL) once and fails with ENTRY_REJECTED. Detach's return
 * value is ignored. Successful close invokes detach once while STOPPING before
 * releasing memory; failed release retries do not invoke detach again.
 * The caller serializes all entries across its loader domain; no Windows loader
 * lock, thread notifications, static-load or process-exit semantics are supplied.
 * Entry callbacks and image entry code must return synchronously and normally,
 * and may not reenter the loader, start asynchronous image work, or leave escaping
 * callbacks/image pointers. No exceptions/longjmp may cross this boundary.
 * Failure releases the region;
 * a release failure retains CLEANUP ownership, which close can retry.
 * acquire_export/acquire_ordinal only return non-forwarded exports within
 * executable sections. acquire_code accepts an internal address without an
 * export entry, but only inside an ACTIVE module's mapped executable section;
 * headers, data sections, gaps, rounded-page padding and out-of-image addresses
 * are refused. This relies on successful memory callbacks, not host probing.
 * All three acquisitions use the same stationary mb_pe_export_lease token and
 * release function. Code admission establishes permissions/lifetime, not an
 * instruction boundary, ThreadProc signature, engine identity or callable ABI.
 * The
 * caller supplies the exact Microsoft x64 function prototype; an address alone
 * does not describe its ABI. Keep a lease through the entire call and any retained
 * callbacks/work. Acquire a worker's own lease before making it runnable, and
 * release only after its final image instruction has returned. Closing a worker
 * handle cannot retire this lease. DllMain asynchronous work remains unsupported.
 * close refuses while either code or export leases remain. No raw address may be used
 * after its lease is released; no exception/longjmp may cross the call boundary.
 */
mb_pe_module_status mb_pe_module_open(mb_pe_module *, const void *, size_t,
    const mb_pe_memory_ops *, mb_pe_binder, void *, mb_pe_module_error *);
mb_pe_module_status mb_pe_module_open_with_entry(mb_pe_module *, const void *, size_t,
    const mb_pe_memory_ops *, const mb_pe_entry_ops *, mb_pe_binder, void *, mb_pe_module_error *);
mb_pe_module_status mb_pe_module_acquire_export(mb_pe_module *, mb_pe_string,
    mb_pe_export_lease *, mb_pe_module_error *);
mb_pe_module_status mb_pe_module_acquire_ordinal(mb_pe_module *, uint32_t,
    mb_pe_export_lease *, mb_pe_module_error *);
mb_pe_module_status mb_pe_module_acquire_code(mb_pe_module *, void *address,
    mb_pe_export_lease *, mb_pe_module_error *);
mb_pe_module_status mb_pe_module_release(mb_pe_export_lease *);
mb_pe_module_status mb_pe_module_close(mb_pe_module *, mb_pe_module_error *);
const char *mb_pe_module_status_name(mb_pe_module_status);
#ifdef __cplusplus
}
#endif
#endif
