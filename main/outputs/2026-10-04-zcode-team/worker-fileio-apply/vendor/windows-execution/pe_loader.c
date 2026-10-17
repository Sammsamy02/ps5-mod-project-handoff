#include "pe_loader.h"

/* Catalog identity is deliberately narrower than filesystem path resolution. */
typedef struct key { mb_pe_string text; size_t length; int suffix; } key;
static unsigned char fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c; }
static int key_for(mb_pe_string text, key *out) {
    if (!text.data || !text.size || text.size > MB_PE_MAX_STRING) return 0;
    int dot = 0, trailing = text.data[text.size - 1] == '.';
    for (size_t i = 0; i < text.size; ++i) {
        unsigned char c = (unsigned char)text.data[i];
        if (c < 32 || c > 126 || c == '/' || c == '\\' || c == ':') return 0;
        if (c == '.') dot = 1;
    }
    if (trailing) --text.size;
    if (!text.size) return 0;
    out->text = text;
    out->suffix = !dot;
    out->length = text.size + (out->suffix ? 4u : 0u);
    return out->length <= MB_PE_MAX_STRING;
}
static unsigned char key_byte(key k, size_t i) {
    return fold((unsigned char)(i < k.text.size ? k.text.data[i] : ".dll"[i - k.text.size]));
}
static int same_key(key a, key b) {
    if (a.length != b.length) return 0;
    for (size_t i = 0; i < a.length; ++i) if (key_byte(a, i) != key_byte(b, i)) return 0;
    return 1;
}
static int same_import(mb_pe_string a, mb_pe_string b) {
    if (a.size != b.size) return 0;
    for (size_t i = 0; i < a.size; ++i)
        if (fold((unsigned char)a.data[i]) != fold((unsigned char)b.data[i])) return 0;
    return 1;
}
static size_t catalog_find(mb_pe_loader *loader, key query) {
    for (size_t i = 0; i < loader->config.catalog_count; ++i) {
        key candidate;
        if (key_for(loader->config.catalog[i].library, &candidate) && same_key(query, candidate)) return i;
    }
    return SIZE_MAX;
}
static size_t handle_find(mb_pe_loader *loader, void *handle) {
    if (handle)
        for (size_t i = 0; i < loader->config.catalog_count; ++i)
            if (loader->config.records[i].state != MB_PE_LOADER_EMPTY &&
                loader->config.records[i].handle == handle) return i;
    return SIZE_MAX;
}
static mb_pe_loader_status result(mb_pe_loader_error *error, mb_pe_loader_status code,
                                  size_t index, int callback, const mb_pe_module_error *module) {
    if (error) {
        *error = (mb_pe_loader_error){.code = code, .cause = code,
                                     .catalog_index = index, .callback_error = callback};
        if (module) {
            error->module = *module;
            /* Source tokens may be disposed before this error reaches callers. */
            error->module.image.import.library = (mb_pe_string){0};
            error->module.image.import.name = (mb_pe_string){0};
        }
    }
    return code;
}
static int ready(mb_pe_loader *loader) { return loader && loader->active && !loader->operating; }
static int pins_empty(mb_pe_export_lease *pins, size_t count) {
    if (count > MB_PE_MAX_ITEMS || (count && !pins)) return 0;
    for (size_t i = 0; i < count; ++i) if (pins[i].owner || pins[i].address || pins[i].next) return 0;
    return 1;
}

mb_pe_loader_status mb_pe_loader_open(mb_pe_loader *loader, const mb_pe_loader_config *config,
                                     mb_pe_loader_error *error) {
    if (!loader || !config || !config->catalog || !config->records ||
        !config->catalog_count || config->catalog_count > MB_PE_LOADER_MAX_MODULES ||
        !config->source.acquire || !config->source.release ||
        !config->memory.allocate_rw || !config->memory.protect ||
        !config->memory.flush_instruction_cache || !config->memory.release ||
        (!!config->services.begin != !!config->services.end))
        return result(error, MB_PE_LOADER_INVALID_ARGUMENT, SIZE_MAX, 0, NULL);
    if (loader->active || loader->operating || loader->pins || loader->pin_count)
        return result(error, MB_PE_LOADER_BAD_STATE, SIZE_MAX, 0, NULL);
    for (size_t i = 0; i < config->catalog_count; ++i) {
        key name;
        const mb_pe_loader_catalog_entry *entry = &config->catalog[i];
        mb_pe_loader_record *record = &config->records[i];
        if (!key_for(entry->library, &name))
            return result(error, MB_PE_LOADER_INVALID_ARGUMENT, i, 0, NULL);
        for (size_t j = 0; j < i; ++j) {
            key earlier;
            (void)key_for(config->catalog[j].library, &earlier);
            if (same_key(name, earlier)) return result(error, MB_PE_LOADER_INVALID_ARGUMENT, i, 0, NULL);
        }
        if (record->state || record->link.owned || record->explicit_refs || record->procedure_count ||
            record->source_active || record->service_active || record->handle ||
            !pins_empty(entry->import_pins, entry->import_pin_capacity) ||
            !pins_empty(entry->procedure_pins, entry->procedure_pin_capacity))
            return result(error, MB_PE_LOADER_BAD_STATE, i, 0, NULL);
    }
    loader->config = *config;
    loader->active = 1;
    return result(error, MB_PE_LOADER_OK, SIZE_MAX, 0, NULL);
}
mb_pe_loader_status mb_pe_loader_acquire(mb_pe_loader *loader, mb_pe_loader_pin *pin) {
    if (!loader || !pin) return MB_PE_LOADER_INVALID_ARGUMENT;
    if (!loader->active || pin->owner || pin->next) return MB_PE_LOADER_BAD_STATE;
    if (loader->pin_count == SIZE_MAX) return MB_PE_LOADER_LIMIT;
    pin->owner = loader;
    pin->next = loader->pins;
    loader->pins = pin;
    ++loader->pin_count;
    return MB_PE_LOADER_OK;
}
mb_pe_loader_status mb_pe_loader_release_pin(mb_pe_loader_pin *pin) {
    if (!pin) return MB_PE_LOADER_INVALID_ARGUMENT;
    mb_pe_loader *loader = pin->owner;
    if (!loader || !loader->active || !loader->pin_count) return MB_PE_LOADER_BAD_STATE;
    mb_pe_loader_pin **cursor = &loader->pins;
    while (*cursor && *cursor != pin) cursor = &(*cursor)->next;
    if (!*cursor) return MB_PE_LOADER_BAD_STATE;
    *cursor = pin->next;
    --loader->pin_count;
    *pin = (mb_pe_loader_pin){0};
    return MB_PE_LOADER_OK;
}

static mb_pe_loader_status cleanup_record(mb_pe_loader *loader, size_t index,
                                          mb_pe_loader_error *error) {
    mb_pe_loader_record *record = &loader->config.records[index];
    const mb_pe_loader_catalog_entry *entry = &loader->config.catalog[index];
    while (record->procedure_count) {
        mb_pe_module_status status = mb_pe_module_release(&entry->procedure_pins[record->procedure_count - 1]);
        if (status != MB_PE_MODULE_OK) {
            mb_pe_module_error module = {.code = status, .cause = status};
            record->state = MB_PE_LOADER_CLEANUP;
            record->cleanup_failed = 1;
            return result(error, MB_PE_LOADER_CLEANUP_REQUIRED, index, 0, &module);
        }
        --record->procedure_count;
    }
    /* Dependency/external leases are not explicit LoadLibrary references. */
    if (record->link.module.lease_count) return MB_PE_LOADER_BUSY;
    record->state = MB_PE_LOADER_CLEANUP;
    if (record->link.owned) {
        mb_pe_module_error module = {0};
        mb_pe_module_status status = mb_pe_link_close(&record->link, &module);
        if (status != MB_PE_MODULE_OK) {
            record->cleanup_failed = 1;
            return result(error, MB_PE_LOADER_CLEANUP_REQUIRED, index, 0, &module);
        }
    }
    if (record->service_active) {
        int rc = loader->config.services.end(loader->config.services.context, record->service_token);
        if (rc) {
            record->cleanup_failed = 1;
            result(error, MB_PE_LOADER_CLEANUP_REQUIRED, index, rc, NULL);
            if (error) error->cause = MB_PE_LOADER_SERVICE_ERROR;
            return MB_PE_LOADER_CLEANUP_REQUIRED;
        }
        record->service_active = 0;
        record->service_token = NULL;
    }
    if (record->source_active) {
        int rc = loader->config.source.release(loader->config.source.context, record->source_token);
        if (rc) {
            record->cleanup_failed = 1;
            result(error, MB_PE_LOADER_CLEANUP_REQUIRED, index, rc, NULL);
            if (error) error->cause = MB_PE_LOADER_SOURCE_ERROR;
            return MB_PE_LOADER_CLEANUP_REQUIRED;
        }
    }
    *record = (mb_pe_loader_record){0};
    return MB_PE_LOADER_OK;
}
static mb_pe_loader_status collect(mb_pe_loader *loader, int retry_failed, mb_pe_loader_error *error) {
    uint64_t failed = 0;
    mb_pe_loader_error first = {0};
    int progress;
    do {
        progress = 0;
        for (size_t i = 0; i < loader->config.catalog_count; ++i) {
            mb_pe_loader_record *record = &loader->config.records[i];
            if (record->state == MB_PE_LOADER_EMPTY || record->explicit_refs ||
                (failed & (UINT64_C(1) << i)) || (!retry_failed && record->cleanup_failed)) continue;
            mb_pe_loader_error current = {0};
            mb_pe_loader_status status = cleanup_record(loader, i, &current);
            if (status == MB_PE_LOADER_OK) progress = 1;
            else if (status != MB_PE_LOADER_BUSY) {
                if (!failed) first = current;
                failed |= UINT64_C(1) << i; /* One failed phase attempt per public call. */
            }
        }
    } while (progress);
    if (failed) { if (error) *error = first; return MB_PE_LOADER_CLEANUP_REQUIRED; }
    return result(error, MB_PE_LOADER_OK, SIZE_MAX, 0, NULL);
}

static mb_pe_loader_status ensure(mb_pe_loader *, size_t, mb_pe_loader_error *);
typedef struct dependency_visit {
    mb_pe_loader *loader;
    mb_pe_loader_error *error;
    mb_pe_loader_status failure;
} dependency_visit;
static int ensure_import(void *opaque, const mb_pe_import *import) {
    dependency_visit *visit = opaque;
    key name;
    if (!key_for(import->library, &name)) return 0;
    size_t index = catalog_find(visit->loader, name);
    if (index == SIZE_MAX) return 0;
    visit->failure = ensure(visit->loader, index, visit->error);
    return visit->failure != MB_PE_LOADER_OK;
}
typedef struct registry_visit {
    mb_pe_loader *loader;
    mb_pe_dependency entries[MB_PE_LINK_MAX_DEPENDENCIES];
    size_t count;
} registry_visit;
static int register_import(void *opaque, const mb_pe_import *import) {
    registry_visit *visit = opaque;
    key name;
    if (!key_for(import->library, &name)) return 0;
    size_t index = catalog_find(visit->loader, name);
    if (index == SIZE_MAX) return 0;
    for (size_t i = 0; i < visit->count; ++i)
        if (same_import(import->library, visit->entries[i].library)) return 0;
    if (visit->count == MB_PE_LINK_MAX_DEPENDENCIES) return 1;
    visit->entries[visit->count++] = (mb_pe_dependency){import->library,
        &visit->loader->config.records[index].link.module};
    return 0;
}
/* This larger registry frame is created only after recursive preparation ends. */
static mb_pe_module_status open_link(mb_pe_loader *loader, size_t index, mb_pe_module_error *error) {
    mb_pe_loader_record *record = &loader->config.records[index];
    const mb_pe_loader_catalog_entry *entry = &loader->config.catalog[index];
    registry_visit registry = {.loader = loader};
    mb_pe_status status = mb_pe_visit_imports(record->file, record->file_size,
                                            register_import, &registry, &error->image);
    if (status != MB_PE_OK) {
        error->code = error->cause = MB_PE_MODULE_IMAGE_ERROR;
        return error->code;
    }
    return mb_pe_link_open(&record->link, record->file, record->file_size,
        &loader->config.memory, entry->entry, registry.entries, registry.count,
        entry->import_pins, entry->import_pin_capacity, record->binder, record->binder_context, error);
}
static mb_pe_loader_status ensure(mb_pe_loader *loader, size_t index, mb_pe_loader_error *error) {
    mb_pe_loader_record *record = &loader->config.records[index];
    const mb_pe_loader_catalog_entry *entry = &loader->config.catalog[index];
    if (record->state == MB_PE_LOADER_ACTIVE) return MB_PE_LOADER_OK;
    if (record->state == MB_PE_LOADER_PREPARING) return result(error, MB_PE_LOADER_CYCLE, index, 0, NULL);
    if (record->state != MB_PE_LOADER_EMPTY) return result(error, MB_PE_LOADER_BAD_STATE, index, 0, NULL);
    record->state = MB_PE_LOADER_PREPARING;
    int rc = loader->config.source.acquire(loader->config.source.context, entry,
        &record->file, &record->file_size, &record->source_token);
    if (rc) {
        record->state = MB_PE_LOADER_CLEANUP;
        return result(error, MB_PE_LOADER_SOURCE_ERROR, index, rc, NULL);
    }
    record->source_active = 1;
    if (!record->file || !record->file_size || !record->source_token) {
        record->state = MB_PE_LOADER_CLEANUP;
        return result(error, MB_PE_LOADER_SOURCE_ERROR, index, 0, NULL);
    }
    dependency_visit visit = {loader, error, MB_PE_LOADER_OK};
    mb_pe_module_error module = {0};
    mb_pe_status inspected = mb_pe_visit_imports(record->file, record->file_size,
                                                ensure_import, &visit, &module.image);
    if (inspected != MB_PE_OK) {
        record->state = MB_PE_LOADER_CLEANUP;
        if (visit.failure != MB_PE_LOADER_OK) return visit.failure;
        module.code = module.cause = MB_PE_MODULE_IMAGE_ERROR;
        return result(error, MB_PE_LOADER_MODULE_ERROR, index, 0, &module);
    }
    record->binder = loader->config.system_binder;
    record->binder_context = loader->config.system_context;
    if (loader->config.services.begin) {
        rc = loader->config.services.begin(loader->config.services.context, entry, &record->link.module,
                                           &record->binder, &record->binder_context, &record->service_token);
        if (rc) {
            record->state = MB_PE_LOADER_CLEANUP;
            return result(error, MB_PE_LOADER_SERVICE_ERROR, index, rc, NULL);
        }
        record->service_active = 1;
    }
    mb_pe_module_status status = open_link(loader, index, &module);
    if (status != MB_PE_MODULE_OK) {
        record->state = MB_PE_LOADER_CLEANUP;
        record->cleanup_failed = status == MB_PE_MODULE_CLEANUP_REQUIRED;
        return result(error, record->cleanup_failed ? MB_PE_LOADER_CLEANUP_REQUIRED : MB_PE_LOADER_MODULE_ERROR,
                      index, 0, &module);
    }
    record->handle = record->link.module.image;
    record->state = MB_PE_LOADER_ACTIVE;
    return MB_PE_LOADER_OK;
}

mb_pe_loader_status mb_pe_loader_load(mb_pe_loader *loader, mb_pe_string library,
                                     void **handle, mb_pe_loader_error *error) {
    if (!handle || !loader) return result(error, MB_PE_LOADER_INVALID_ARGUMENT, SIZE_MAX, 0, NULL);
    if (!ready(loader)) return result(error, MB_PE_LOADER_BAD_STATE, SIZE_MAX, 0, NULL);
    key name;
    if (!key_for(library, &name)) return result(error, MB_PE_LOADER_UNSUPPORTED, SIZE_MAX, 0, NULL);
    size_t index = catalog_find(loader, name);
    if (index == SIZE_MAX) return result(error, MB_PE_LOADER_NOT_FOUND, index, 0, NULL);
    /* An old cleanup handle may name an unmapped address: do not recycle it. */
    for (size_t i = 0; i < loader->config.catalog_count; ++i)
        if (loader->config.records[i].state == MB_PE_LOADER_CLEANUP)
            return result(error, MB_PE_LOADER_BAD_STATE, i, 0, NULL);
    mb_pe_loader_record *record = &loader->config.records[index];
    if (record->explicit_refs == SIZE_MAX) return result(error, MB_PE_LOADER_LIMIT, index, 0, NULL);
    loader->operating = 1;
    mb_pe_loader_error primary = {0};
    mb_pe_loader_status status = ensure(loader, index, &primary);
    if (status == MB_PE_LOADER_OK) {
        ++record->explicit_refs;
        *handle = record->handle;
        result(&primary, status, index, 0, NULL);
    } else {
        mb_pe_loader_error cleanup = {0};
        if (collect(loader, 0, &cleanup) != MB_PE_LOADER_OK) {
            cleanup.cause = status;
            primary = cleanup;
            status = MB_PE_LOADER_CLEANUP_REQUIRED;
        }
    }
    loader->operating = 0;
    if (error) *error = primary;
    return status;
}
mb_pe_loader_status mb_pe_loader_find(mb_pe_loader *loader, mb_pe_string library,
                                     void **handle, mb_pe_loader_error *error) {
    if (!handle || !loader) return result(error, MB_PE_LOADER_INVALID_ARGUMENT, SIZE_MAX, 0, NULL);
    if (!ready(loader)) return result(error, MB_PE_LOADER_BAD_STATE, SIZE_MAX, 0, NULL);
    key name;
    if (!key_for(library, &name)) return result(error, MB_PE_LOADER_UNSUPPORTED, SIZE_MAX, 0, NULL);
    size_t index = catalog_find(loader, name);
    if (index == SIZE_MAX || loader->config.records[index].state != MB_PE_LOADER_ACTIVE)
        return result(error, MB_PE_LOADER_NOT_FOUND, index, 0, NULL);
    *handle = loader->config.records[index].handle;
    return result(error, MB_PE_LOADER_OK, index, 0, NULL);
}
static mb_pe_loader_status procedure(mb_pe_loader *loader, void *handle, mb_pe_string name,
    uint32_t ordinal, int by_ordinal, void **address, mb_pe_loader_error *error) {
    if (!loader || !address) return result(error, MB_PE_LOADER_INVALID_ARGUMENT, SIZE_MAX, 0, NULL);
    if (!ready(loader)) return result(error, MB_PE_LOADER_BAD_STATE, SIZE_MAX, 0, NULL);
    size_t index = handle_find(loader, handle);
    if (index == SIZE_MAX || loader->config.records[index].state != MB_PE_LOADER_ACTIVE)
        return result(error, MB_PE_LOADER_NOT_FOUND, index, 0, NULL);
    mb_pe_loader_record *record = &loader->config.records[index];
    const mb_pe_loader_catalog_entry *entry = &loader->config.catalog[index];
    mb_pe_export_lease temporary = {0};
    int full = record->procedure_count == entry->procedure_pin_capacity;
    mb_pe_export_lease *lease = full ? &temporary : &entry->procedure_pins[record->procedure_count];
    mb_pe_module_error module = {0};
    mb_pe_module_status status = by_ordinal
        ? mb_pe_module_acquire_ordinal(&record->link.module, ordinal, lease, &module)
        : mb_pe_module_acquire_export(&record->link.module, name, lease, &module);
    if (status != MB_PE_MODULE_OK) {
        mb_pe_loader_status mapped = status == MB_PE_MODULE_NOT_FOUND ? MB_PE_LOADER_NOT_FOUND
            : status == MB_PE_MODULE_FORWARDER || status == MB_PE_MODULE_NOT_EXECUTABLE ? MB_PE_LOADER_UNSUPPORTED
            : status == MB_PE_MODULE_LIMIT ? MB_PE_LOADER_LIMIT : MB_PE_LOADER_MODULE_ERROR;
        return result(error, mapped, index, 0, &module);
    }
    void *found = lease->address;
    for (size_t i = 0; i < record->procedure_count; ++i) {
        if (entry->procedure_pins[i].address == found) {
            (void)mb_pe_module_release(lease);
            *address = found;
            return result(error, MB_PE_LOADER_OK, index, 0, NULL);
        }
    }
    if (full) {
        (void)mb_pe_module_release(lease);
        return result(error, MB_PE_LOADER_LIMIT, index, 0, NULL);
    }
    ++record->procedure_count;
    *address = found;
    return result(error, MB_PE_LOADER_OK, index, 0, NULL);
}
mb_pe_loader_status mb_pe_loader_proc_name(mb_pe_loader *loader, void *handle, mb_pe_string name,
                                           void **address, mb_pe_loader_error *error) {
    return procedure(loader, handle, name, 0, 0, address, error);
}
mb_pe_loader_status mb_pe_loader_proc_ordinal(mb_pe_loader *loader, void *handle, uint32_t ordinal,
                                              void **address, mb_pe_loader_error *error) {
    if (!ordinal || ordinal > UINT16_MAX) return result(error, MB_PE_LOADER_INVALID_ARGUMENT, SIZE_MAX, 0, NULL);
    return procedure(loader, handle, (mb_pe_string){0}, ordinal, 1, address, error);
}
mb_pe_loader_status mb_pe_loader_free(mb_pe_loader *loader, void *handle, mb_pe_loader_error *error) {
    if (!ready(loader)) return result(error, MB_PE_LOADER_BAD_STATE, SIZE_MAX, 0, NULL);
    size_t index = handle_find(loader, handle);
    if (index == SIZE_MAX) return result(error, MB_PE_LOADER_NOT_FOUND, index, 0, NULL);
    mb_pe_loader_record *record = &loader->config.records[index];
    if (record->explicit_refs) --record->explicit_refs;
    else if (record->state != MB_PE_LOADER_CLEANUP) return result(error, MB_PE_LOADER_BAD_STATE, index, 0, NULL);
    loader->operating = 1;
    mb_pe_loader_status status = collect(loader, 1, error);
    loader->operating = 0;
    return status;
}
mb_pe_loader_status mb_pe_loader_collect(mb_pe_loader *loader, mb_pe_loader_error *error) {
    if (!ready(loader)) return result(error, MB_PE_LOADER_BAD_STATE, SIZE_MAX, 0, NULL);
    loader->operating = 1;
    mb_pe_loader_status status = collect(loader, 1, error);
    loader->operating = 0;
    return status;
}
mb_pe_loader_status mb_pe_loader_close(mb_pe_loader *loader, mb_pe_loader_error *error) {
    if (!loader) return result(error, MB_PE_LOADER_INVALID_ARGUMENT, SIZE_MAX, 0, NULL);
    if (!loader->active) return result(error, MB_PE_LOADER_OK, SIZE_MAX, 0, NULL);
    if (!ready(loader)) return result(error, MB_PE_LOADER_BAD_STATE, SIZE_MAX, 0, NULL);
    loader->operating = 1;
    mb_pe_loader_status status = collect(loader, 1, error);
    loader->operating = 0;
    if (status != MB_PE_LOADER_OK) return status;
    for (size_t i = 0; i < loader->config.catalog_count; ++i)
        if (loader->config.records[i].state != MB_PE_LOADER_EMPTY)
            return result(error, MB_PE_LOADER_BUSY, i, 0, NULL);
    if (loader->pin_count) return result(error, MB_PE_LOADER_BUSY, SIZE_MAX, 0, NULL);
    *loader = (mb_pe_loader){0};
    return result(error, MB_PE_LOADER_OK, SIZE_MAX, 0, NULL);
}
