#include "pe_module.h"
#include <string.h>

#define SECTION_EXECUTE UINT32_C(0x20000000)
#define SECTION_READ UINT32_C(0x40000000)
#define SECTION_WRITE UINT32_C(0x80000000)
#define MAX_BOUNDARIES (2u * MB_PE_MAX_SECTIONS + 3u)
typedef struct protection_span { size_t offset, length; unsigned rights; } protection_span;
typedef struct protection_plan { protection_span spans[MAX_BOUNDARIES]; unsigned count; size_t size, alignment; } protection_plan;

static int valid(const void *p, size_t n) { return p && n <= UINTPTR_MAX - (uintptr_t)p; }
static int overlap(const void *a, size_t n, const void *b, size_t m) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return n && m && (x <= y ? y - x < n : x - y < m);
}
static int independent_error(const mb_pe_module *m, const mb_pe_module_error *e) {
    return !e || (valid(e, sizeof(*e)) && !overlap(m, sizeof(*m), e, sizeof(*e)));
}
static mb_pe_module_status result(mb_pe_module_error *e, mb_pe_module_status code, uint32_t detail) {
    if (e) { memset(e, 0, sizeof(*e)); e->code = e->cause = code; e->detail = detail; }
    return code;
}
static unsigned section_rights(uint32_t flags) {
    unsigned rights = 0;
    if (flags & SECTION_READ) rights |= MB_PE_MEMORY_READ;
    if (flags & SECTION_WRITE) rights |= MB_PE_MEMORY_WRITE;
    if (flags & SECTION_EXECUTE) rights |= MB_PE_MEMORY_EXECUTE;
    return rights;
}
static int executable_rva(const mb_pe_info *v, uint32_t rva) {
    unsigned i;
    for (i = 0; i < v->section_count; ++i) {
        const mb_pe_section *s = &v->sections[i];
        if (rva >= s->rva && rva - s->rva < s->mapped_size && (s->characteristics & SECTION_EXECUTE)) return 1;
    }
    return 0;
}
static size_t round_up(size_t n, size_t granule) { return (n + granule - 1) & ~(granule - 1); }
static mb_pe_module_status admission(const mb_pe_module *m, const mb_pe_info *v, int with_entry, mb_pe_module_error *e) {
    /* A closed directory allowlist prevents omitted requirement bits from
     * accidentally becoming execution support. Resource/security/debug stay
     * inert data; their presence does not implement Windows APIs or trust. */
    /* worker-static-tls-7fam (vendored adaptation): an explicit, opaque
     * admission ticket on the module opts this single open in to data
     * directory 9 / MB_PE_REQUIRES_TLS so a bound static-TLS session can
     * parse and own the directory. Default (no ticket) is unchanged. */
    const unsigned allowed = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 4) |
        (1u << 5) | (1u << 6) | (1u << 12) | (m->tls_admission ? (1u << 9) : 0u);
    unsigned i;
    const unsigned requirements = MB_PE_REQUIRES_IMPORTS | (with_entry ? MB_PE_REQUIRES_ENTRY : 0u) |
        (m->tls_admission ? MB_PE_REQUIRES_TLS : 0u);
    if (v->requirements & ~requirements)
        return result(e, MB_PE_MODULE_UNSUPPORTED, v->requirements);
    for (i = 0; i < MB_PE_MAX_DIRECTORIES; ++i)
        if (!(allowed & (1u << i)) && (v->directories[i].rva || v->directories[i].size))
            return result(e, MB_PE_MODULE_UNSUPPORTED, i);
    if (v->characteristics & (0x1000u | 0x0100u | 0x4000u | 0x0080u | 0x8000u))
        return result(e, MB_PE_MODULE_UNSUPPORTED, v->characteristics);
    /* ASLR/NX/no-SEH/no-bind/terminal-aware do not require loader work here. */
    if (v->dll_characteristics & ~(0x0020u | 0x0040u | 0x0100u | 0x0200u | 0x0400u | 0x0800u | 0x8000u))
        return result(e, MB_PE_MODULE_UNSUPPORTED, v->dll_characteristics);
    for (i = 0; i < v->section_count; ++i)
        if (v->sections[i].characteristics & UINT32_C(0x1c000000))
            return result(e, MB_PE_MODULE_UNSUPPORTED, i); /* shared/cache/paging semantics */
    if (v->entry_rva && !executable_rva(v, v->entry_rva))
        return result(e, MB_PE_MODULE_NOT_EXECUTABLE, v->entry_rva);
    return MB_PE_MODULE_OK;
}
static mb_pe_module_status plan_protections(const mb_pe_info *v, size_t granule,
                                           protection_plan *plan, mb_pe_module_error *e) {
    size_t boundaries[MAX_BOUNDARIES];
    unsigned count = 0, i, j;
    if ((size_t)v->image_size > SIZE_MAX - (granule - 1))
        return result(e, MB_PE_MODULE_LIMIT, 0);
    plan->size = round_up(v->image_size, granule);
    plan->alignment = granule > v->section_alignment ? granule : v->section_alignment;
    plan->count = 0;
    boundaries[count++] = 0;
    boundaries[count++] = plan->size;
    boundaries[count++] = round_up(v->headers_size, granule);
    for (i = 0; i < v->section_count; ++i) {
        const mb_pe_section *s = &v->sections[i];
        if (!s->mapped_size) continue;
        boundaries[count++] = (size_t)s->rva & ~(granule - 1);
        boundaries[count++] = round_up((size_t)s->rva + s->mapped_size, granule);
    }
    for (i = 1; i < count; ++i) {
        size_t value = boundaries[i];
        for (j = i; j && boundaries[j - 1] > value; --j) boundaries[j] = boundaries[j - 1];
        boundaries[j] = value;
    }
    for (i = 1; i < count; ++i) {
        size_t start = boundaries[i - 1], end = boundaries[i];
        unsigned rights = 0;
        protection_span *span;
        if (end == start) continue;
        if (start < v->headers_size) rights = MB_PE_MEMORY_READ;
        for (j = 0; j < v->section_count; ++j) {
            const mb_pe_section *s = &v->sections[j];
            if (s->mapped_size && start < (size_t)s->rva + s->mapped_size && (size_t)s->rva < end)
                rights |= section_rights(s->characteristics);
        }
        if ((rights & (MB_PE_MEMORY_WRITE | MB_PE_MEMORY_EXECUTE)) == (MB_PE_MEMORY_WRITE | MB_PE_MEMORY_EXECUTE))
            return result(e, MB_PE_MODULE_UNSUPPORTED, (uint32_t)start);
        if (plan->count && plan->spans[plan->count - 1].rights == rights) {
            plan->spans[plan->count - 1].length += end - start;
            continue;
        }
        span = &plan->spans[plan->count++];
        span->offset = start; span->length = end - start; span->rights = rights;
    }
    return MB_PE_MODULE_OK;
}
static mb_pe_module_status cleanup(mb_pe_module *m, mb_pe_module_error failure, mb_pe_module_error *e) {
    m->state = MB_PE_MODULE_CLEANUP;
    if (m->memory.release(m->memory.context, m->image, m->allocation_size)) {
        failure.cause = failure.code;
        failure.code = MB_PE_MODULE_CLEANUP_REQUIRED;
        m->failure = failure;
    } else memset(m, 0, sizeof(*m));
    if (e) *e = failure;
    return failure.code;
}
typedef struct binding_context { mb_pe_binder binder; void *context; } binding_context;
static mb_pe_bind_result bind_nonzero(void *ctx, const mb_pe_import *import, uint64_t *value) {
    binding_context *b = ctx;
    mb_pe_bind_result status = b->binder ? b->binder(b->context, import, value) : MB_PE_BIND_UNRESOLVED;
    return status == MB_PE_BIND_BOUND && !*value ? MB_PE_BIND_UNRESOLVED : status;
}
static mb_pe_module_status open_impl(mb_pe_module *m, const void *file, size_t size,
    const mb_pe_memory_ops *ops, const mb_pe_entry_ops *entry, int with_entry,
    mb_pe_binder binder, void *ctx, mb_pe_module_error *e) {
    mb_pe_info info;
    mb_pe_module_error failure = {0};
    mb_pe_module_status status;
    mb_pe_status pe_status;
    protection_plan plan;
    binding_context binding = {binder, ctx};
    unsigned i;
    if (!valid(m, sizeof(*m)) || !independent_error(m, e)) return MB_PE_MODULE_INVALID_ARGUMENT;
    if (!valid(file, size) || !size || !valid(ops, sizeof(*ops)) ||
        overlap(m, sizeof(*m), file, size) || overlap(m, sizeof(*m), ops, sizeof(*ops)) ||
        overlap(file, size, ops, sizeof(*ops)) ||
        (e && (overlap(e, sizeof(*e), file, size) || overlap(e, sizeof(*e), ops, sizeof(*ops)))))
        return MB_PE_MODULE_INVALID_ARGUMENT;
    if (with_entry && (!valid(entry, sizeof(*entry)) ||
        overlap(entry, sizeof(*entry), m, sizeof(*m)) || overlap(entry, sizeof(*entry), file, size) ||
        overlap(entry, sizeof(*entry), ops, sizeof(*ops)) || (e && overlap(entry, sizeof(*entry), e, sizeof(*e)))))
        return MB_PE_MODULE_INVALID_ARGUMENT;
    if (m->state != MB_PE_MODULE_EMPTY) return result(e, MB_PE_MODULE_BAD_STATE, 0);
    if (!ops->protection_granule || (ops->protection_granule & (ops->protection_granule - 1)) ||
        !ops->allocate_rw || !ops->protect || !ops->flush_instruction_cache || !ops->release ||
        (with_entry && !entry->invoke))
        return result(e, MB_PE_MODULE_INVALID_ARGUMENT, 0);
    pe_status = mb_pe_inspect(file, size, &info, &failure.image);
    if (pe_status != MB_PE_OK) {
        failure.code = failure.cause = MB_PE_MODULE_IMAGE_ERROR;
        if (e) *e = failure;
        return failure.code;
    }
    status = admission(m, &info, with_entry, e);
    if (status != MB_PE_MODULE_OK) return status;
    status = plan_protections(&info, ops->protection_granule, &plan, e);
    if (status != MB_PE_MODULE_OK) return status;
    memset(m, 0, sizeof(*m));
    m->state = MB_PE_MODULE_LOADING; m->memory = *ops; m->file = file; m->file_size = size;
    if (with_entry) m->entry = *entry;
    m->allocation_size = plan.size;
    m->image = ops->allocate_rw(ops->context, plan.size, plan.alignment);
    if (!m->image) { memset(m, 0, sizeof(*m)); return result(e, MB_PE_MODULE_MEMORY_ERROR, 1); }
    failure.code = failure.cause = MB_PE_MODULE_MEMORY_ERROR; failure.detail = 1;
    if (!valid(m->image, plan.size) || (uintptr_t)m->image % plan.alignment ||
        overlap(m->image, plan.size, file, size) || overlap(m->image, plan.size, m, sizeof(*m)) ||
        overlap(m->image, plan.size, ops, sizeof(*ops)) ||
        (with_entry && overlap(m->image, plan.size, entry, sizeof(*entry))) ||
        (e && overlap(m->image, plan.size, e, sizeof(*e))))
        return cleanup(m, failure, e);
    pe_status = mb_pe_prepare(file, size, (uint64_t)(uintptr_t)m->image, m->image, plan.size,
        bind_nonzero, &binding, &m->info, &m->report, &failure.image);
    if (pe_status != MB_PE_OK) {
        failure.code = failure.cause = pe_status == MB_PE_UNRESOLVED_IMPORTS ?
            MB_PE_MODULE_UNRESOLVED_IMPORTS : MB_PE_MODULE_IMAGE_ERROR;
        failure.detail = 0;
        return cleanup(m, failure, e);
    }
    failure.code = failure.cause = MB_PE_MODULE_MEMORY_ERROR; failure.detail = 2;
    for (i = 0; i < plan.count; ++i) {
        protection_span *span = &plan.spans[i];
        if (ops->protect(ops->context, m->image, span->offset, span->length, span->rights))
            return cleanup(m, failure, e);
    }
    failure.detail = 3;
    if (ops->flush_instruction_cache(ops->context, m->image, plan.size)) return cleanup(m, failure, e);
    if (m->info.entry_rva) {
        void *address = (unsigned char *)m->image + m->info.entry_rva;
        m->state = MB_PE_MODULE_INITIALIZING;
        if (!m->entry.invoke(m->entry.context, address, m->image, 1u, NULL)) {
            /* Dynamic-load attach rejection still receives process detach.
             * Mark shutdown before invoking it; cleanup retries cannot repeat it. */
            m->state = MB_PE_MODULE_STOPPING;
            (void)m->entry.invoke(m->entry.context, address, m->image, 0u, NULL);
            memset(&failure, 0, sizeof(failure));
            failure.code = failure.cause = MB_PE_MODULE_ENTRY_REJECTED;
            return cleanup(m, failure, e);
        }
        m->entry_attached = 1;
    }
    m->state = MB_PE_MODULE_ACTIVE;
    return result(e, MB_PE_MODULE_OK, 0);
}
mb_pe_module_status mb_pe_module_open(mb_pe_module *m, const void *file, size_t size,
    const mb_pe_memory_ops *ops, mb_pe_binder binder, void *ctx, mb_pe_module_error *e) {
    return open_impl(m, file, size, ops, NULL, 0, binder, ctx, e);
}
mb_pe_module_status mb_pe_module_open_with_entry(mb_pe_module *m, const void *file, size_t size,
    const mb_pe_memory_ops *ops, const mb_pe_entry_ops *entry, mb_pe_binder binder, void *ctx, mb_pe_module_error *e) {
    return open_impl(m, file, size, ops, entry, 1, binder, ctx, e);
}
typedef struct export_lookup {
    mb_pe_string name;
    uint32_t ordinal, rva;
    int by_ordinal, found, forwarded;
} export_lookup;
static int find_export(void *ctx, const mb_pe_export *entry) {
    export_lookup *q = ctx;
    if (!q->found && (q->by_ordinal ? q->ordinal == entry->ordinal :
        q->name.size == entry->name.size && entry->name.data && !memcmp(q->name.data, entry->name.data, q->name.size))) {
        q->found = 1; q->rva = entry->rva; q->forwarded = entry->forwarder.size != 0;
    }
    return 0;
}
static mb_pe_module_status acquire(mb_pe_module *m, export_lookup *q, mb_pe_export_lease *lease, mb_pe_module_error *e) {
    mb_pe_error pe_error;
    if (!valid(m, sizeof(*m)) || !independent_error(m, e) || !valid(lease, sizeof(*lease)) ||
        overlap(m, sizeof(*m), lease, sizeof(*lease)) ||
        (e && overlap(e, sizeof(*e), lease, sizeof(*lease)))) return MB_PE_MODULE_INVALID_ARGUMENT;
    if (m->state != MB_PE_MODULE_ACTIVE) return result(e, MB_PE_MODULE_BAD_STATE, 0);
    if (overlap(lease, sizeof(*lease), m->file, m->file_size) || overlap(lease, sizeof(*lease), m->image, m->allocation_size) ||
        (e && (overlap(e, sizeof(*e), m->file, m->file_size) || overlap(e, sizeof(*e), m->image, m->allocation_size))))
        return MB_PE_MODULE_INVALID_ARGUMENT;
    if (lease->owner || lease->address || lease->next) return result(e, MB_PE_MODULE_BAD_STATE, 0);
    if (!q->by_ordinal && (!q->name.size || q->name.size > MB_PE_MAX_STRING || !valid(q->name.data, q->name.size)))
        return result(e, MB_PE_MODULE_INVALID_ARGUMENT, 0);
    if (m->lease_count == MB_PE_MAX_ITEMS) return result(e, MB_PE_MODULE_LIMIT, 0);
    if (mb_pe_visit_exports(m->file, m->file_size, find_export, q, &pe_error) != MB_PE_OK) {
        result(e, MB_PE_MODULE_IMAGE_ERROR, 0);
        if (e) e->image = pe_error;
        return MB_PE_MODULE_IMAGE_ERROR;
    }
    if (!q->found) return result(e, MB_PE_MODULE_NOT_FOUND, 0);
    if (q->forwarded) return result(e, MB_PE_MODULE_FORWARDER, 0);
    if (!executable_rva(&m->info, q->rva)) return result(e, MB_PE_MODULE_NOT_EXECUTABLE, q->rva);
    lease->address = (unsigned char *)m->image + q->rva;
    lease->owner = m; lease->next = m->leases; m->leases = lease; ++m->lease_count;
    return result(e, MB_PE_MODULE_OK, 0);
}
mb_pe_module_status mb_pe_module_acquire_export(mb_pe_module *m, mb_pe_string name, mb_pe_export_lease *lease, mb_pe_module_error *e) {
    export_lookup q = {0}; q.name = name;
    return acquire(m, &q, lease, e);
}
mb_pe_module_status mb_pe_module_acquire_ordinal(mb_pe_module *m, uint32_t ordinal, mb_pe_export_lease *lease, mb_pe_module_error *e) {
    export_lookup q = {0}; q.ordinal = ordinal; q.by_ordinal = 1;
    return acquire(m, &q, lease, e);
}
mb_pe_module_status mb_pe_module_acquire_code(mb_pe_module *m, void *address, mb_pe_export_lease *lease, mb_pe_module_error *e) {
    uintptr_t base, value, offset;
    if (!valid(m, sizeof(*m)) || !independent_error(m, e) || !valid(lease, sizeof(*lease)) ||
        overlap(m, sizeof(*m), lease, sizeof(*lease)) ||
        (e && overlap(e, sizeof(*e), lease, sizeof(*lease)))) return MB_PE_MODULE_INVALID_ARGUMENT;
    if (m->state != MB_PE_MODULE_ACTIVE) return result(e, MB_PE_MODULE_BAD_STATE, 0);
    if (overlap(lease, sizeof(*lease), m->file, m->file_size) || overlap(lease, sizeof(*lease), m->image, m->allocation_size) ||
        (e && (overlap(e, sizeof(*e), m->file, m->file_size) || overlap(e, sizeof(*e), m->image, m->allocation_size))))
        return MB_PE_MODULE_INVALID_ARGUMENT;
    if (lease->owner || lease->address || lease->next) return result(e, MB_PE_MODULE_BAD_STATE, 0);
    if (!address) return result(e, MB_PE_MODULE_INVALID_ARGUMENT, 0);
    base = (uintptr_t)m->image; value = (uintptr_t)address;
    if (!valid(m->image, m->allocation_size) || value < base)
        return result(e, MB_PE_MODULE_NOT_EXECUTABLE, 0);
    offset = value - base;
    if (offset >= m->allocation_size || offset >= m->info.image_size || offset > UINT32_MAX)
        return result(e, MB_PE_MODULE_NOT_EXECUTABLE, 0);
    /* ACTIVE means every protection span and the cache flush succeeded. Require
     * a committed executable PE section as well: a data section or gap sharing
     * its rounded RX protection granule does not become an admitted code address. */
    if (!executable_rva(&m->info, (uint32_t)offset))
        return result(e, MB_PE_MODULE_NOT_EXECUTABLE, (uint32_t)offset);
    if (m->lease_count == MB_PE_MAX_ITEMS) return result(e, MB_PE_MODULE_LIMIT, 0);
    lease->address = (unsigned char *)m->image + offset;
    lease->owner = m; lease->next = m->leases; m->leases = lease; ++m->lease_count;
    return result(e, MB_PE_MODULE_OK, 0);
}
mb_pe_module_status mb_pe_module_release(mb_pe_export_lease *lease) {
    mb_pe_module *m;
    mb_pe_export_lease **link;
    if (!valid(lease, sizeof(*lease))) return MB_PE_MODULE_INVALID_ARGUMENT;
    m = lease->owner;
    if (!m || m->state != MB_PE_MODULE_ACTIVE || !m->lease_count) return MB_PE_MODULE_BAD_STATE;
    for (link = &m->leases; *link && *link != lease; link = &(*link)->next) {}
    if (!*link) return MB_PE_MODULE_BAD_STATE; /* A copied token is not a lease. */
    *link = lease->next; --m->lease_count; memset(lease, 0, sizeof(*lease));
    return MB_PE_MODULE_OK;
}
mb_pe_module_status mb_pe_module_close(mb_pe_module *m, mb_pe_module_error *e) {
    mb_pe_module_error failure = {0};
    if (!valid(m, sizeof(*m)) || !independent_error(m, e)) return MB_PE_MODULE_INVALID_ARGUMENT;
    if (m->state == MB_PE_MODULE_EMPTY) return result(e, MB_PE_MODULE_OK, 0);
    if (e && (overlap(e, sizeof(*e), m->file, m->file_size) || overlap(e, sizeof(*e), m->image, m->allocation_size)))
        return MB_PE_MODULE_INVALID_ARGUMENT;
    if (m->state != MB_PE_MODULE_ACTIVE && m->state != MB_PE_MODULE_CLEANUP) return result(e, MB_PE_MODULE_BAD_STATE, 0);
    if (m->lease_count) return result(e, MB_PE_MODULE_BUSY, 0);
    if (m->state == MB_PE_MODULE_CLEANUP) failure = m->failure;
    else { failure.code = failure.cause = MB_PE_MODULE_MEMORY_ERROR; failure.detail = 4; }
    if (m->entry_attached) {
        void *address = (unsigned char *)m->image + m->info.entry_rva;
        m->state = MB_PE_MODULE_STOPPING;
        m->entry_attached = 0;
        (void)m->entry.invoke(m->entry.context, address, m->image, 0u, NULL);
    }
    m->state = MB_PE_MODULE_CLEANUP;
    if (m->memory.release(m->memory.context, m->image, m->allocation_size)) {
        failure.code = MB_PE_MODULE_CLEANUP_REQUIRED; m->failure = failure;
        if (e) *e = failure;
        return failure.code;
    }
    memset(m, 0, sizeof(*m));
    return result(e, MB_PE_MODULE_OK, 0);
}
const char *mb_pe_module_status_name(mb_pe_module_status status) {
    static const char *const names[] = {"ok", "invalid_argument", "bad_state", "image_error", "unsupported",
        "unresolved_imports", "memory_error", "cleanup_required", "busy", "not_found", "forwarder", "not_executable", "limit", "entry_rejected"};
    return (unsigned)status < sizeof(names) / sizeof(names[0]) ? names[status] : "unknown";
}
