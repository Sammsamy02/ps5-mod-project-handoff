#ifndef MB_PE_LOADER_H
#define MB_PE_LOADER_H
#include "pe_dependencies.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MB_PE_LOADER_MAX_MODULES 64u

typedef enum mb_pe_loader_status {
    MB_PE_LOADER_OK = 0, MB_PE_LOADER_INVALID_ARGUMENT, MB_PE_LOADER_BAD_STATE,
    MB_PE_LOADER_NOT_FOUND, MB_PE_LOADER_CYCLE, MB_PE_LOADER_LIMIT,
    MB_PE_LOADER_SOURCE_ERROR, MB_PE_LOADER_SERVICE_ERROR, MB_PE_LOADER_MODULE_ERROR,
    MB_PE_LOADER_BUSY, MB_PE_LOADER_CLEANUP_REQUIRED, MB_PE_LOADER_UNSUPPORTED
} mb_pe_loader_status;
typedef enum mb_pe_loader_record_state {
    MB_PE_LOADER_EMPTY = 0, MB_PE_LOADER_PREPARING, MB_PE_LOADER_ACTIVE, MB_PE_LOADER_CLEANUP
} mb_pe_loader_record_state;
typedef struct mb_pe_loader_catalog_entry {
    mb_pe_string library;
    void *source_context;
    const mb_pe_entry_ops *entry; /* NULL uses the no-entry module admission. */
    mb_pe_export_lease *import_pins;
    size_t import_pin_capacity;
    mb_pe_export_lease *procedure_pins;
    size_t procedure_pin_capacity;
} mb_pe_loader_catalog_entry;
typedef struct mb_pe_loader_source_ops {
    void *context;
    int (*acquire)(void *, const mb_pe_loader_catalog_entry *, const void **file,
                   size_t *size, void **token);
    int (*release)(void *, void *token);
} mb_pe_loader_source_ops;
typedef struct mb_pe_loader_service_ops {
    void *context;
    int (*begin)(void *, const mb_pe_loader_catalog_entry *, mb_pe_module *,
                 mb_pe_binder *binder, void **binder_context, void **token);
    int (*end)(void *, void *token);
} mb_pe_loader_service_ops;
typedef struct mb_pe_loader_record {
    mb_pe_link link;
    mb_pe_loader_record_state state;
    size_t explicit_refs, procedure_count;
    void *handle;
    const void *file;
    size_t file_size;
    void *source_token, *service_token;
    mb_pe_binder binder;
    void *binder_context;
    uint8_t source_active, service_active, cleanup_failed;
} mb_pe_loader_record;
typedef struct mb_pe_loader_config {
    const mb_pe_loader_catalog_entry *catalog;
    size_t catalog_count;
    mb_pe_loader_record *records;
    mb_pe_memory_ops memory;
    mb_pe_loader_source_ops source;
    mb_pe_loader_service_ops services; /* both callbacks NULL disables hooks */
    mb_pe_binder system_binder;
    void *system_context;
} mb_pe_loader_config;
struct mb_pe_loader;
typedef struct mb_pe_loader_pin {
    struct mb_pe_loader *owner;
    struct mb_pe_loader_pin *next;
} mb_pe_loader_pin;
typedef struct mb_pe_loader {
    mb_pe_loader_config config;
    mb_pe_loader_pin *pins;
    size_t pin_count;
    uint8_t active, operating;
} mb_pe_loader;
typedef struct mb_pe_loader_error {
    mb_pe_loader_status code, cause;
    size_t catalog_index;
    int callback_error;
    mb_pe_module_error module;
} mb_pe_loader_error;

/* Native C ABI. Zero-initialize loader, records, every import/procedure pin,
 * and loader pins. Catalog/records/pin storage and callback contexts are stable,
 * mutually disjoint, caller-owned, and survive successful loader close. Config
 * is copied; catalog/name storage remains borrowed. Returned module errors retain
 * scalar import/IAT diagnostics but clear borrowed library/name views, because
 * source bytes may have been released during rollback. All operations and callbacks
 * are externally serialized. Callbacks must return normally and may not reenter
 * load/find/proc/free/collect/close; designated acquire/release_pin calls from
 * service hooks are allowed. Existing PE entry restrictions remain unchanged.
 *
 * Catalog keys and queries are nonempty ASCII basenames, case-insensitive.
 * A key without a dot gains .dll; one trailing dot suppresses that addition.
 * Paths, drive prefixes, non-ASCII names and executable/NULL-name lookup are
 * unsupported. Normalized keys must be unique. No filesystem search occurs.
 * Catalog imports open recursively; a PREPARING revisit reports CYCLE. Source
 * bytes are acquired lazily and remain immutable through final cleanup.
 *
 * acquire/begin return zero on success; failures must retain no new resources.
 * Failed acquire leaves file/size/token empty; failed begin leaves its token
 * empty (binder outputs are ignored). Successful source acquisition
 * supplies nonempty bytes plus a nonnull token. release/end return zero only
 * after disposal; nonzero failure retains the entire token/resource for retry.
 * Service begin runs with an EMPTY module and starts from configured system
 * binder/context outputs; successful output replaces that fallback. end runs
 * only after link cleanup is complete and the module is EMPTY. Callbacks and
 * borrowed contexts stay alive through every cleanup retry.
 *
 * Load increments explicit_refs; recursive dependency loads do not. Real link
 * export leases keep ref-zero providers alive. Find adds no reference. Handles
 * are actual module image bases, compared against records before dereference.
 * Cleanup handles cannot be found/proc-resolved until collection finishes. New
 * loads are refused while any CLEANUP record remains, preventing address reuse
 * from aliasing an older retained cleanup handle.
 * Proc returns executable nonforwarded exports and caches a caller-owned lease;
 * repeat addresses share a cache entry. This does not gain a Load reference.
 * Caller holds a suitable Load reference/dependency through calls and retained
 * addresses. Cached proc leases retire before final module close.
 *
 * Free consumes one explicit reference once. Cleanup failure retains phase,
 * original handle, source/services/dependencies still owned, for free/collect
 * retry without consuming another reference. Ref-zero providers collect only
 * after dependent/external leases retire. Collect retries deferred cleanup and
 * sweeps eligible providers; close also collects, then refuses remaining records
 * or loader pins. No raw proc pointer may survive final reference/dependency.
 * No cycles, forwarded/data exports, automatic discovery, entry reentrancy,
 * main executable, PE static TLS, unwind or Windows loader lock are implemented.
 */
mb_pe_loader_status mb_pe_loader_open(mb_pe_loader *, const mb_pe_loader_config *, mb_pe_loader_error *);
mb_pe_loader_status mb_pe_loader_close(mb_pe_loader *, mb_pe_loader_error *);
mb_pe_loader_status mb_pe_loader_acquire(mb_pe_loader *, mb_pe_loader_pin *);
mb_pe_loader_status mb_pe_loader_release_pin(mb_pe_loader_pin *);
mb_pe_loader_status mb_pe_loader_load(mb_pe_loader *, mb_pe_string, void **handle, mb_pe_loader_error *);
mb_pe_loader_status mb_pe_loader_find(mb_pe_loader *, mb_pe_string, void **handle, mb_pe_loader_error *);
mb_pe_loader_status mb_pe_loader_proc_name(mb_pe_loader *, void *handle, mb_pe_string, void **address, mb_pe_loader_error *);
mb_pe_loader_status mb_pe_loader_proc_ordinal(mb_pe_loader *, void *handle, uint32_t, void **address, mb_pe_loader_error *);
mb_pe_loader_status mb_pe_loader_free(mb_pe_loader *, void *handle, mb_pe_loader_error *);
mb_pe_loader_status mb_pe_loader_collect(mb_pe_loader *, mb_pe_loader_error *);
#ifdef __cplusplus
}
#endif
#endif
