#ifndef MB_PE_DEPENDENCIES_H
#define MB_PE_DEPENDENCIES_H
#include "pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MB_PE_LINK_MAX_DEPENDENCIES 256u

typedef struct mb_pe_dependency {
    mb_pe_string library;
    mb_pe_module *module;
} mb_pe_dependency;

typedef struct mb_pe_link {
    mb_pe_module module;
    mb_pe_export_lease *pins;
    size_t pin_capacity, pin_count;
    uint8_t owned;
} mb_pe_link;

/* Zero-initialize link and every pin slot before opening; do not move/copy or
 * mutate them while owned. All access is externally serialized. Registered
 * modules must be ACTIVE and stable; their immutable files, objects and image
 * allocations stay alive while referenced. DLL keys are nonempty counted byte
 * strings (no embedded NUL), ASCII-case-insensitive and unique. Symbols remain
 * case-sensitive. Registry/name storage is needed only during synchronous open.
 *
 * Pin capacity must cover the inspected total import count and be at most
 * MB_PE_MAX_ITEMS. Each provider import acquires its own executable export
 * lease before its IAT address is published. A registered library's missing,
 * forwarded or data export stays unresolved, without system fallback. Only
 * unregistered libraries reach system_binder; that binder's implementations and
 * dependencies retain the lifetime/ABI obligations of mb_pe_module_open.
 *
 * NULL entry chooses mb_pe_module_open; otherwise open_with_entry is used.
 * Existing module entry, callback, memory and no-escaping-work contracts apply.
 * The allocator supplies dedicated storage disjoint from all input, registry,
 * provider, link and pin objects. Link/pins/error must be disjoint from inputs
 * and provider objects/files/images; provider objects must also be disjoint
 * from input records. Error output is optional, independent and never written
 * for invalid pointer/alias arguments. Opaque callback context storage remains
 * the caller's responsibility; callbacks must not mutate or reenter this owner.
 *
 * Consumer export leases are acquired on &link->module. Close BUSY retains all
 * pins. Consumer CLEANUP, failed attach and failed deallocation keep pins until
 * the consumer is EMPTY. Only then are pins retired in reverse acquisition order.
 * A failed retirement returns CLEANUP_REQUIRED with cause = lease-release status
 * and detail = zero-based pin index, retaining the wrapper and remaining pins
 * for close retry. Do not free/overwrite link or pin storage until owned == 0.
 * Successful cleanup clears link. Do not close link->module directly.
 *
 * This owner does no file discovery, dependency opening, cycle resolution,
 * forwarded/data export resolution or Windows loader API emulation.
 */
mb_pe_module_status mb_pe_link_open(mb_pe_link *, const void *file, size_t file_size,
    const mb_pe_memory_ops *, const mb_pe_entry_ops *entry,
    const mb_pe_dependency *registry, size_t registry_count,
    mb_pe_export_lease *pins, size_t pin_capacity,
    mb_pe_binder system_binder, void *system_context, mb_pe_module_error *);
mb_pe_module_status mb_pe_link_close(mb_pe_link *, mb_pe_module_error *);

#ifdef __cplusplus
}
#endif
#endif
