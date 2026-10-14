#include "pe_dependencies.h"

/* Counted ranges are validated before any caller-owned record is changed. */
typedef struct region { const void *data; size_t size; } region;
static int valid(region a) {
    return !a.size || (a.data && a.size <= UINTPTR_MAX - (uintptr_t)a.data);
}
static int overlaps(region a, region b) {
    uintptr_t x = (uintptr_t)a.data, y = (uintptr_t)b.data;
    return a.size && b.size && (x <= y ? y - x < a.size : x - y < b.size);
}
static int aligned(const void *p, size_t alignment) {
    return !((uintptr_t)p % alignment);
}
static int writable_overlap(const region writes[3], region input) {
    for (unsigned i = 0; i < 3; ++i)
        if (overlaps(writes[i], input)) return 1;
    return 0;
}
static unsigned char fold(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}
static int same_library(mb_pe_string a, mb_pe_string b) {
    if (a.size != b.size) return 0;
    for (size_t i = 0; i < a.size; ++i)
        if (fold((unsigned char)a.data[i]) != fold((unsigned char)b.data[i])) return 0;
    return 1;
}
static mb_pe_module_status result(mb_pe_module_error *out, mb_pe_module_status code) {
    if (out) *out = (mb_pe_module_error){.code = code, .cause = code};
    return code;
}
static mb_pe_module_status copy_result(mb_pe_module_error *out,
                                       const mb_pe_module_error *error,
                                       mb_pe_module_status status) {
    if (out) *out = *error;
    return status;
}

static int open_arguments(mb_pe_link *link, const void *file, size_t file_size,
    const mb_pe_memory_ops *memory, const mb_pe_entry_ops *entry,
    const mb_pe_dependency *registry, size_t registry_count,
    mb_pe_export_lease *pins, size_t pin_capacity, mb_pe_module_error *error) {
    if (!link || !file_size || !memory || registry_count > MB_PE_LINK_MAX_DEPENDENCIES ||
        pin_capacity > MB_PE_MAX_ITEMS ||
        !aligned(link, _Alignof(mb_pe_link)) ||
        !aligned(memory, _Alignof(mb_pe_memory_ops)) ||
        (entry && !aligned(entry, _Alignof(mb_pe_entry_ops))) ||
        (registry_count && !aligned(registry, _Alignof(mb_pe_dependency))) ||
        (pin_capacity && !aligned(pins, _Alignof(mb_pe_export_lease))) ||
        (error && !aligned(error, _Alignof(mb_pe_module_error)))) return 0;
    region writes[3] = {{link, sizeof(*link)}, {pins, pin_capacity * sizeof(*pins)},
                        {error, error ? sizeof(*error) : 0}};
    region inputs[4] = {{file, file_size}, {memory, sizeof(*memory)},
                       {entry, entry ? sizeof(*entry) : 0},
                       {registry, registry_count * sizeof(*registry)}};
    for (unsigned i = 0; i < 3; ++i) {
        if (!valid(writes[i])) return 0;
        for (unsigned j = 0; j < i; ++j)
            if (overlaps(writes[i], writes[j])) return 0;
    }
    for (unsigned i = 0; i < 4; ++i)
        if (!valid(inputs[i]) || writable_overlap(writes, inputs[i])) return 0;
    /* Match the underlying module's independent file/memory/entry contract. */
    if (overlaps(inputs[0], inputs[1]) || overlaps(inputs[0], inputs[2]) ||
        overlaps(inputs[1], inputs[2])) return 0;
    /* Validate every registry record before any error output or lease mutation. */
    for (size_t i = 0; i < registry_count; ++i) {
        mb_pe_string name = registry[i].library;
        mb_pe_module *provider = registry[i].module;
        region key = {name.data, name.size}, object = {provider, sizeof(*provider)};
        if (!name.size || name.size > MB_PE_MAX_STRING || !valid(key) ||
            !valid(object) || !aligned(provider, _Alignof(mb_pe_module)) ||
            writable_overlap(writes, key) || writable_overlap(writes, object)) return 0;
        for (unsigned j = 0; j < 4; ++j)
            if (overlaps(object, inputs[j])) return 0;
        for (size_t j = 0; j < name.size; ++j) if (!name.data[j]) return 0;
        region image = {provider->image, provider->allocation_size};
        region source = {provider->file, provider->file_size};
        if (!valid(image) || !valid(source) || writable_overlap(writes, image) ||
            writable_overlap(writes, source)) return 0;
    }
    for (size_t i = 0; i < registry_count; ++i) {
        region object = {registry[i].module, sizeof(mb_pe_module)};
        for (size_t j = 0; j < registry_count; ++j) {
            region key = {registry[j].library.data, registry[j].library.size};
            region other = {registry[j].module, sizeof(mb_pe_module)};
            if (overlaps(object, key) ||
                (registry[i].module != registry[j].module && overlaps(object, other))) return 0;
            if (j < i && same_library(registry[i].library, registry[j].library)) return 0;
        }
    }
    return 1;
}

typedef struct binding_context {
    mb_pe_link *link;
    const mb_pe_dependency *registry;
    size_t registry_count;
    mb_pe_binder system_binder;
    void *system_context;
} binding_context;

static mb_pe_bind_result bind_dependency(void *opaque, const mb_pe_import *import,
                                         uint64_t *address) {
    binding_context *context = opaque;
    for (size_t i = 0; i < context->registry_count; ++i) {
        const mb_pe_dependency *dependency = &context->registry[i];
        if (!same_library(import->library, dependency->library)) continue;
        mb_pe_link *link = context->link;
        if (link->pin_count == link->pin_capacity) return MB_PE_BIND_ERROR;
        mb_pe_export_lease *pin = &link->pins[link->pin_count];
        mb_pe_module_status status = import->by_ordinal
            ? mb_pe_module_acquire_ordinal(dependency->module, import->ordinal, pin, NULL)
            : mb_pe_module_acquire_export(dependency->module, import->name, pin, NULL);
        if (status == MB_PE_MODULE_OK) {
            ++link->pin_count; /* Own the lease before publishing its address. */
            *address = (uint64_t)(uintptr_t)pin->address;
            return MB_PE_BIND_BOUND;
        }
        if (status == MB_PE_MODULE_NOT_FOUND || status == MB_PE_MODULE_FORWARDER ||
            status == MB_PE_MODULE_NOT_EXECUTABLE) return MB_PE_BIND_UNRESOLVED;
        return MB_PE_BIND_ERROR;
    }
    return context->system_binder
        ? context->system_binder(context->system_context, import, address)
        : MB_PE_BIND_UNRESOLVED;
}

/* Only an EMPTY consumer may drop dependencies, including on failed open. */
static mb_pe_module_status retire(mb_pe_link *link, mb_pe_module_error *error) {
    while (link->pin_count) {
        size_t index = link->pin_count - 1;
        mb_pe_module_status status = mb_pe_module_release(&link->pins[index]);
        if (status != MB_PE_MODULE_OK) {
            if (error) *error = (mb_pe_module_error){
                .code = MB_PE_MODULE_CLEANUP_REQUIRED, .cause = status, .detail = (uint32_t)index};
            return MB_PE_MODULE_CLEANUP_REQUIRED;
        }
        --link->pin_count;
    }
    *link = (mb_pe_link){0};
    return MB_PE_MODULE_OK;
}

mb_pe_module_status mb_pe_link_open(mb_pe_link *link, const void *file, size_t size,
    const mb_pe_memory_ops *memory, const mb_pe_entry_ops *entry,
    const mb_pe_dependency *registry, size_t registry_count,
    mb_pe_export_lease *pins, size_t pin_capacity,
    mb_pe_binder system_binder, void *system_context, mb_pe_module_error *error) {
    if (!open_arguments(link, file, size, memory, entry, registry, registry_count,
                        pins, pin_capacity, error)) return MB_PE_MODULE_INVALID_ARGUMENT;
    if (link->owned || link->module.state != MB_PE_MODULE_EMPTY || link->pins ||
        link->pin_capacity || link->pin_count) return result(error, MB_PE_MODULE_BAD_STATE);
    for (size_t i = 0; i < pin_capacity; ++i)
        if (pins[i].address || pins[i].owner || pins[i].next)
            return result(error, MB_PE_MODULE_BAD_STATE);
    for (size_t i = 0; i < registry_count; ++i)
        if (registry[i].module->state != MB_PE_MODULE_ACTIVE)
            return result(error, MB_PE_MODULE_BAD_STATE);
    mb_pe_info info;
    mb_pe_module_error local = {0};
    if (mb_pe_inspect(file, size, &info, &local.image) != MB_PE_OK) {
        local.code = local.cause = MB_PE_MODULE_IMAGE_ERROR;
        return copy_result(error, &local, local.code);
    }
    if (pin_capacity < info.import_count) return result(error, MB_PE_MODULE_LIMIT);
    link->owned = 1;
    link->pins = pins;
    link->pin_capacity = pin_capacity;
    binding_context context = {link, registry, registry_count, system_binder, system_context};
    mb_pe_module_status status = entry
        ? mb_pe_module_open_with_entry(&link->module, file, size, memory, entry,
                                       bind_dependency, &context, &local)
        : mb_pe_module_open(&link->module, file, size, memory, bind_dependency, &context, &local);
    if (status != MB_PE_MODULE_OK && link->module.state == MB_PE_MODULE_EMPTY) {
        mb_pe_module_status cleanup = retire(link, error);
        if (cleanup != MB_PE_MODULE_OK) return cleanup;
    }
    return copy_result(error, &local, status);
}

static int close_arguments(mb_pe_link *link, mb_pe_module_error *error) {
    region object = {link, sizeof(*link)}, output = {error, error ? sizeof(*error) : 0};
    if (!valid(object) || !aligned(link, _Alignof(mb_pe_link)) ||
        !valid(output) || (error && !aligned(error, _Alignof(mb_pe_module_error))) ||
        overlaps(object, output)) return 0;
    if (link->pin_capacity > MB_PE_MAX_ITEMS || link->pin_count > link->pin_capacity) return 0;
    region pins = {link->pins, link->pin_capacity * sizeof(*link->pins)};
    if (!valid(pins) || (pins.size && !aligned(pins.data, _Alignof(mb_pe_export_lease))) ||
        overlaps(object, pins) || overlaps(pins, output)) return 0;
    region writes[3] = {object, pins, output};
    region file = {link->module.file, link->module.file_size};
    region image = {link->module.image, link->module.allocation_size};
    if (!valid(file) || !valid(image) || writable_overlap(writes, file) ||
        writable_overlap(writes, image)) return 0;
    for (size_t i = 0; i < link->pin_count; ++i) {
        mb_pe_module *provider = link->pins[i].owner;
        region provider_object = {provider, sizeof(*provider)};
        if (!valid(provider_object) || !aligned(provider, _Alignof(mb_pe_module)) ||
            writable_overlap(writes, provider_object)) return 0;
        region provider_file = {provider->file, provider->file_size};
        region provider_image = {provider->image, provider->allocation_size};
        if (!valid(provider_file) || !valid(provider_image) ||
            writable_overlap(writes, provider_file) || writable_overlap(writes, provider_image)) return 0;
    }
    return 1;
}

mb_pe_module_status mb_pe_link_close(mb_pe_link *link, mb_pe_module_error *error) {
    if (!close_arguments(link, error)) return MB_PE_MODULE_INVALID_ARGUMENT;
    if (!link->owned) {
        if (link->module.state != MB_PE_MODULE_EMPTY || link->pins || link->pin_count ||
            link->pin_capacity) return result(error, MB_PE_MODULE_BAD_STATE);
        return result(error, MB_PE_MODULE_OK);
    }
    if (link->module.state != MB_PE_MODULE_EMPTY) {
        mb_pe_module_error local = {0};
        mb_pe_module_status status = mb_pe_module_close(&link->module, &local);
        if (status != MB_PE_MODULE_OK) return copy_result(error, &local, status);
    }
    mb_pe_module_status status = retire(link, error);
    if (status != MB_PE_MODULE_OK) return status;
    return result(error, MB_PE_MODULE_OK);
}
