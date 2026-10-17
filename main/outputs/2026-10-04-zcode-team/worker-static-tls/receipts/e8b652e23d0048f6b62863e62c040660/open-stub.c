/* VENDORED ADAPTED COPY (origin: new work; structure follows the
 * windows-services family owner pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once.c and worker-crt-startup-implement;
 * those files are unchanged). Freestanding C11: no OS calls, no libc; the
 * only allocations go through the pinned domain's
 * mb_win_threads_allocate/release. */
#include "mb_win_static_tls.h"

typedef struct mb_win_static_tls_native {
    mb_win_thread_domain *domain;
    mb_win_thread_pin pin;
    mb_win_static_tls_module *modules;
    mb_win_static_tls_pin *pins;
    uint8_t open;
} mb_win_static_tls_native;

static mb_win_static_tls_native *native_of(mb_win_static_tls *o) { return o ? o->state : 0; }

mb_win_static_tls_status mb_win_static_tls_open(mb_win_static_tls *owner,
    const mb_win_static_tls_config *config, int *native_error)
{(void)owner;(void)config;(void)native_error;return MB_WIN_STATIC_TLS_BAD_STATE;}

mb_win_static_tls_status mb_win_static_tls_close(mb_win_static_tls *owner, int *native_error)
{
    if (native_error) *native_error = 0;
    mb_win_static_tls_native *n = native_of(owner);
    if (!owner || !n || !n->open) return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    mb_win_threads_lock(n->domain);
    if (n->pins || n->modules) {
        mb_win_threads_unlock(n->domain);
        return MB_WIN_STATIC_TLS_BUSY; /* retains ownership, like every family close */
    }
    n->open = 0;
    mb_win_threads_unlock(n->domain);
    mb_win_thread_status status = mb_win_threads_release_pin(&n->pin);
    if (status != MB_WIN_THREADS_OK) {
        if (native_error) *native_error = (int)status;
        mb_win_threads_lock(n->domain);
        n->open = 1; /* retain ownership on native cleanup failure */
        mb_win_threads_unlock(n->domain);
        return MB_WIN_STATIC_TLS_NATIVE_ERROR;
    }
    owner->state = 0;
    mb_win_threads_release(n->domain, n);
    return MB_WIN_STATIC_TLS_OK;
}

mb_win_static_tls_status mb_win_static_tls_acquire_pin(mb_win_static_tls *owner, mb_win_static_tls_pin *pin)
{
    mb_win_static_tls_native *n = native_of(owner);
    if (!n || !n->open || !pin) return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    if (pin->owner || pin->next) return MB_WIN_STATIC_TLS_BAD_STATE;
    mb_win_threads_lock(n->domain);
    if (!n->open) { mb_win_threads_unlock(n->domain); return MB_WIN_STATIC_TLS_BAD_STATE; }
    pin->owner = owner;
    pin->next = n->pins;
    n->pins = pin;
    mb_win_threads_unlock(n->domain);
    return MB_WIN_STATIC_TLS_OK;
}

mb_win_static_tls_status mb_win_static_tls_release_pin(mb_win_static_tls_pin *pin)
{
    if (!pin || !pin->owner) return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    mb_win_static_tls_native *n = native_of(pin->owner);
    if (!n) return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    mb_win_static_tls_pin **cursor = &n->pins;
    mb_win_threads_lock(n->domain);
    while (*cursor && *cursor != pin) cursor = &(*cursor)->next;
    if (!*cursor) { mb_win_threads_unlock(n->domain); return MB_WIN_STATIC_TLS_BAD_STATE; }
    *cursor = pin->next;
    mb_win_threads_unlock(n->domain);
    *pin = (mb_win_static_tls_pin){0};
    return MB_WIN_STATIC_TLS_OK;
}

mb_win_thread_domain *mb_win_static_tls_domain(mb_win_static_tls *owner)
{
    mb_win_static_tls_native *n = native_of(owner);
    return n ? n->domain : 0;
}

/* Little-endian field read; x64 directories only (documented). */
static uint64_t read_le64(const uint8_t *p)
{
    uint64_t v = 0;
    for (unsigned i = 8; i-- > 0;) v = (v << 8) | p[i];
    return v;
}
static uint32_t read_le32(const uint8_t *p)
{
    uint32_t v = 0;
    for (unsigned i = 4; i-- > 0;) v = (v << 8) | p[i];
    return v;
}

int mb_win_static_tls_parse(const void *directory64, mb_win_static_tls_values *out)
{
    if (!directory64 || !out) return -1;
    const uint8_t *p = directory64;
    uint64_t start = read_le64(p + 0), end = read_le64(p + 8);
    if (end < start) return -1;
    out->start_address_of_raw_data = start;
    out->end_address_of_raw_data = end;
    out->address_of_index = read_le64(p + 16);
    out->address_of_callbacks = read_le64(p + 24);
    out->size_of_zero_fill = read_le32(p + 32);
    out->characteristics = read_le32(p + 36);
    return 0;
}

static mb_win_static_tls_module *find_module(mb_win_static_tls_native *n, mb_win_static_tls_module *m)
{
    mb_win_static_tls_module **cursor = &n->modules;
    while (*cursor && *cursor != m) cursor = &(*cursor)->next;
    return *cursor; /* NULL when absent */
}

mb_win_static_tls_module *mb_win_static_tls_module_for(mb_win_static_tls *owner, const void *host_base)
{
    mb_win_static_tls_native *n = native_of(owner);
    if (!n || !n->open || !host_base) return 0;
    mb_win_threads_lock(n->domain);
    mb_win_static_tls_module *found = 0;
    for (mb_win_static_tls_module *m = n->modules; m; m = m->next)
        if (m->host_base == host_base) { found = m; break; }
    mb_win_threads_unlock(n->domain);
    return found;
}

mb_win_static_tls_status mb_win_static_tls_register(mb_win_static_tls *owner,
    mb_win_static_tls_module *module, const mb_win_static_tls_values *values,
    void *host_base, uint64_t preferred_base)
{
    mb_win_static_tls_native *n = native_of(owner);
    if (!n || !n->open || !module || !values || !host_base || !preferred_base)
        return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    const uint8_t *probe;
    mb_win_static_tls_module probe_copy = *module;
    if (probe_copy.tls_index == MB_WIN_STATIC_TLS_NO_INDEX)
        probe_copy.tls_index = 0; /* a zeroed or unregistered record is fresh */
    probe = (const uint8_t *)&probe_copy;
    for (size_t i = 0; i < sizeof probe_copy; ++i)
        if (probe[i]) return MB_WIN_STATIC_TLS_BAD_STATE; /* non-zeroed record */
    if (values->end_address_of_raw_data < values->start_address_of_raw_data ||
        values->start_address_of_raw_data < preferred_base ||
        !values->address_of_index || !values->address_of_callbacks ||
        values->address_of_index < preferred_base ||
        values->address_of_callbacks < preferred_base)
        return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    size_t raw = (size_t)(values->end_address_of_raw_data - values->start_address_of_raw_data);
    if (raw + (size_t)values->size_of_zero_fill == 0)
        return MB_WIN_STATIC_TLS_INVALID_ARGUMENT; /* adapter policy: empty TLS refused */

    mb_win_threads_lock(n->domain);
    if (!n->open || find_module(n, module)) { mb_win_threads_unlock(n->domain); return MB_WIN_STATIC_TLS_BAD_STATE; }
    module->next = n->modules;
    n->modules = module;
    mb_win_threads_unlock(n->domain);

    /* Same dynamic pool as TlsAlloc; requires an admitted current thread. */
    uint32_t index = mb_win_threads_tls_alloc(n->domain);
    if (index == MB_WIN_TLS_OUT_OF_INDEXES) {
        mb_win_threads_lock(n->domain);
        mb_win_static_tls_module **cursor = &n->modules;
        while (*cursor && *cursor != module) cursor = &(*cursor)->next;
        if (*cursor) *cursor = module->next;
        mb_win_threads_unlock(n->domain);
        return MB_WIN_STATIC_TLS_NATIVE_ERROR;
    }
    module->values = *values;
    module->host_base = host_base;
    module->preferred_base = preferred_base;
    module->raw_data = (uint8_t *)host_base + (size_t)(values->start_address_of_raw_data - preferred_base);
    module->raw_bytes = raw;
    module->index_slot = (uint32_t *)((uint8_t *)host_base + (size_t)(values->address_of_index - preferred_base));
    module->callbacks = (uintptr_t *)((uint8_t *)host_base + (size_t)(values->address_of_callbacks - preferred_base));
    module->owner = owner;
    module->tls_index = index;
    module->live_blocks = 0;
    *module->index_slot = index; /* load-time write, before any attach */
    return MB_WIN_STATIC_TLS_OK;
}

mb_win_static_tls_status mb_win_static_tls_unregister(mb_win_static_tls *owner, mb_win_static_tls_module *module)
{
    mb_win_static_tls_native *n = native_of(owner);
    if (!n || !n->open || !module) return MB_WIN_STATIC_TLS_INVALID_ARGUMENT;
    mb_win_threads_lock(n->domain);
    if (!find_module(n, module)) {
        mb_win_threads_unlock(n->domain);
        return MB_WIN_STATIC_TLS_BAD_STATE;
    }
    if (module->live_blocks) {
        mb_win_threads_unlock(n->domain);
        return MB_WIN_STATIC_TLS_BUSY;
    }
    mb_win_threads_unlock(n->domain);
    /* Frees the pool index and NULLs that slot on all live threads; zero
     * live blocks above means no block pointer can leak. Requires an
     * admitted current thread (documented domain contract). BOOL: 0 fails. */
    if (mb_win_threads_tls_free(n->domain, module->tls_index) == 0)
        return MB_WIN_STATIC_TLS_NATIVE_ERROR;
    mb_win_threads_lock(n->domain);
    mb_win_static_tls_module **cursor = &n->modules;
    while (*cursor && *cursor != module) cursor = &(*cursor)->next;
    if (*cursor) *cursor = module->next;
    mb_win_threads_unlock(n->domain);
    *module = (mb_win_static_tls_module){0};
    module->tls_index = MB_WIN_STATIC_TLS_NO_INDEX; /* unregistered, fresh */
    return MB_WIN_STATIC_TLS_OK;
}

void *mb_win_static_tls_block(mb_win_static_tls_module *module)
{
    if (!module || !module->owner || module->tls_index == MB_WIN_STATIC_TLS_NO_INDEX) return 0;
    mb_win_thread_domain *domain = mb_win_static_tls_domain(module->owner);
    void *existing = mb_win_threads_tls_get(domain, module->tls_index);
    if (existing) return existing;
    size_t size = module->raw_bytes + module->values.size_of_zero_fill;
    uint8_t *block = mb_win_threads_allocate(domain, size);
    if (!block) return 0;
    const uint8_t *raw = module->raw_data;
    for (size_t i = 0; i < module->raw_bytes; ++i) block[i] = raw[i];
    for (size_t i = module->raw_bytes; i < size; ++i) block[i] = 0;
    if (mb_win_threads_tls_set(domain, module->tls_index, block) == 0) { /* BOOL: 0 fails */
        mb_win_threads_release(domain, block);
        return 0;
    }
    mb_win_threads_lock(domain);
    ++module->live_blocks;
    mb_win_threads_unlock(domain);
    return block;
}

/* Dispatches through plain native-C function pointers read live from the
 * mapped (relocation-updated) array; win64cc dispatch wiring is the entry
 * adapter's documented composition point (INTEGRATION.md). */
static void dispatch(mb_win_static_tls_module *module, uint32_t reason, int ascending)
{
    uintptr_t *callbacks = module->callbacks;
    size_t count = 0;
    while (callbacks[count]) ++count;
    if (ascending) {
        for (size_t i = 0; i < count; ++i)
            ((void (*)(void *, uint32_t, void *))callbacks[i])(module->host_base, reason, 0);
    } else {
        for (size_t i = count; i-- > 0;)
            ((void (*)(void *, uint32_t, void *))callbacks[i])(module->host_base, reason, 0);
    }
}

static int attach(mb_win_static_tls_module *module, uint32_t reason)
{
    if (!module || !module->owner || module->tls_index == MB_WIN_STATIC_TLS_NO_INDEX) return -1;
    if (!mb_win_static_tls_block(module)) return -1;
    dispatch(module, reason, 1);
    return 0;
}

static void detach(mb_win_static_tls_module *module, uint32_t reason)
{
    if (!module || !module->owner || module->tls_index == MB_WIN_STATIC_TLS_NO_INDEX) return;
    dispatch(module, reason, 0);
    mb_win_thread_domain *domain = mb_win_static_tls_domain(module->owner);
    void *block = mb_win_threads_tls_get(domain, module->tls_index);
    if (!block) return;
    if (mb_win_threads_tls_set(domain, module->tls_index, 0) == 0) return; /* BOOL: 0 fails */
    mb_win_threads_release(domain, block);
    mb_win_threads_lock(domain);
    --module->live_blocks;
    mb_win_threads_unlock(domain);
}

int mb_win_static_tls_process_attach(mb_win_static_tls_module *module)
{
    return attach(module, MB_WIN_STATIC_TLS_DLL_PROCESS_ATTACH);
}
int mb_win_static_tls_thread_attach(mb_win_static_tls_module *module)
{
    return attach(module, MB_WIN_STATIC_TLS_DLL_THREAD_ATTACH);
}
void mb_win_static_tls_thread_detach(mb_win_static_tls_module *module)
{
    detach(module, MB_WIN_STATIC_TLS_DLL_THREAD_DETACH);
}
void mb_win_static_tls_process_detach(mb_win_static_tls_module *module)
{
    detach(module, MB_WIN_STATIC_TLS_DLL_PROCESS_DETACH);
}
