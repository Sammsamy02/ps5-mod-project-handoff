#include "mb_win_vm.h"

/* Virtual-memory family owner. See mb_win_vm.h for the full contract. */

_Static_assert(sizeof(size_t) == 8 && sizeof(void *) == 8,
               "Virtual-memory owner requires 64-bit sizes and pointers");
_Static_assert(sizeof(mb_win_vm_basic_info) == MB_WIN_VM_BASIC_INFO_SIZE,
               "MEMORY_BASIC_INFORMATION must match the Win64 48-byte layout");
_Static_assert(offsetof(mb_win_vm_basic_info, base_address) == 0 &&
               offsetof(mb_win_vm_basic_info, allocation_base) == 8 &&
               offsetof(mb_win_vm_basic_info, allocation_protect) == 16 &&
               offsetof(mb_win_vm_basic_info, partition_id) == 20 &&
               offsetof(mb_win_vm_basic_info, region_size) == 24 &&
               offsetof(mb_win_vm_basic_info, state) == 32 &&
               offsetof(mb_win_vm_basic_info, protect) == 36 &&
               offsetof(mb_win_vm_basic_info, type) == 40,
               "MEMORY_BASIC_INFORMATION field offsets must match Win64");

struct mb_win_vm_region {
    mb_win_vm_region *next;
    void *span_raw;             /* backend pointer backing the aligned span */
    void *book_raw;             /* backend pointer backing this node + arrays */
    void *base;                 /* granularity-aligned span base */
    size_t span;                /* span bytes, multiple of granularity */
    size_t pages;               /* span / page_size */
    uint32_t allocation_protect;
    /* committed[pages] (uint8_t) and protection[pages] (uint32_t) follow. */
};

static uint8_t *committed_of(mb_win_vm_region *region) {
    return (uint8_t *)(region + 1);
}
static uint32_t *protection_of(mb_win_vm_region *region) {
    uintptr_t slot = (uintptr_t)(region + 1) + region->pages;
    return (uint32_t *)((slot + 3u) & ~(uintptr_t)3u);
}

static int is_power_of_two(uint32_t value) {
    return value && !(value & (value - 1u));
}
static int is_plain_protection(uint32_t protect) {
    switch (protect) {
    case MB_WIN_VM_PAGE_NOACCESS: case MB_WIN_VM_PAGE_READONLY:
    case MB_WIN_VM_PAGE_READWRITE: case MB_WIN_VM_PAGE_WRITECOPY:
    case MB_WIN_VM_PAGE_EXECUTE: case MB_WIN_VM_PAGE_EXECUTE_READ:
    case MB_WIN_VM_PAGE_EXECUTE_READWRITE: case MB_WIN_VM_PAGE_EXECUTE_WRITECOPY:
        return 1;
    default:
        return 0;
    }
}
static mb_win_vm_status fail(mb_win_vm *vm, mb_win_vm_status status, uint32_t code) {
    if (vm->ops.error) vm->ops.error(vm->ops.context, code);
    return status;
}
static void *span_round_up(void *address, uintptr_t alignment) {
    return (void *)(((uintptr_t)address + alignment - 1) & ~(alignment - 1));
}

mb_win_vm_status mb_win_vm_open(mb_win_vm *vm, const mb_win_vm_ops *ops,
                                const mb_win_vm_config *config) {
    if (!vm || !ops || !ops->allocate || !ops->release)
        return MB_WIN_VM_INVALID_ARGUMENT;
    if (vm->active || vm->regions || vm->region_count || vm->live_bytes)
        return MB_WIN_VM_BAD_STATE;
    mb_win_vm_config resolved = {MB_WIN_VM_PAGE_SIZE, MB_WIN_VM_GRANULARITY};
    if (config) {
        if (config->page_size) resolved.page_size = config->page_size;
        if (config->granularity) resolved.granularity = config->granularity;
    }
    if (!is_power_of_two(resolved.page_size) || !is_power_of_two(resolved.granularity) ||
        resolved.page_size > resolved.granularity ||
        resolved.granularity % resolved.page_size)
        return MB_WIN_VM_INVALID_ARGUMENT;
    vm->ops = *ops;
    vm->config = resolved;
    vm->active = 1;
    return MB_WIN_VM_OK;
}

mb_win_vm_status mb_win_vm_close(mb_win_vm *vm) {
    if (!vm) return MB_WIN_VM_INVALID_ARGUMENT;
    if (vm->regions || vm->region_count || vm->live_bytes) return MB_WIN_VM_BUSY;
    if (!vm->active) return MB_WIN_VM_BAD_STATE;
    *vm = (mb_win_vm){0};
    return MB_WIN_VM_OK;
}

void mb_win_vm_system_info(const mb_win_vm *vm, uint32_t *page_size,
                           uint32_t *allocation_granularity) {
    if (!vm || !vm->active) return;
    if (page_size) *page_size = vm->config.page_size;
    if (allocation_granularity) *allocation_granularity = vm->config.granularity;
}

/* Returns the unique region whose span contains [start, end), or NULL. */
static mb_win_vm_region *region_containing(mb_win_vm *vm, const void *start, size_t bytes) {
    const uintptr_t lo = (uintptr_t)start;
    const uintptr_t hi = lo + bytes;
    for (mb_win_vm_region *cursor = vm->regions; cursor; cursor = cursor->next) {
        const uintptr_t base = (uintptr_t)cursor->base;
        if (lo >= base && hi <= base + cursor->span) return cursor;
    }
    return NULL;
}

static mb_win_vm_status commit_pages(mb_win_vm *vm, mb_win_vm_region *region,
                                     void *start, size_t bytes, uint32_t protect) {
    uint8_t *committed = committed_of(region);
    uint32_t *protection = protection_of(region);
    const size_t first = ((uintptr_t)start - (uintptr_t)region->base) / vm->config.page_size;
    const size_t count = bytes / vm->config.page_size;
    if (vm->ops.protect &&
        vm->ops.protect(vm->ops.context, start, bytes, protect))
        return MB_WIN_VM_BACKEND_ERROR;
    for (size_t i = 0; i < count; ++i) {
        unsigned char *page = (unsigned char *)start + i * vm->config.page_size;
        if (!committed[first + i]) {
            for (size_t b = 0; b < vm->config.page_size; ++b) page[b] = 0;
            committed[first + i] = 1;
        }
        protection[first + i] = protect;
    }
    return MB_WIN_VM_OK;
}

void *mb_win_vm_alloc(mb_win_vm *vm, void *address, size_t size,
                      uint32_t allocation_type, uint32_t protect) {
    if (!vm || !vm->active) return NULL;
    const uint32_t commit = allocation_type & MB_WIN_VM_MEM_COMMIT;
    const uint32_t reserve = allocation_type & MB_WIN_VM_MEM_RESERVE;
    if (allocation_type & ~(MB_WIN_VM_MEM_COMMIT | MB_WIN_VM_MEM_RESERVE) ||
        !is_plain_protection(protect) || !size) {
        fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (reserve) {
        /* Fresh reservation: backend picks the address. */
        if (address) {
            fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
            return NULL;
        }
        if (size > (size_t)MB_WIN_VM_ADDRESS_LIMIT ||
            size > SIZE_MAX - vm->config.granularity - (vm->config.granularity - 1)) {
            fail(vm, MB_WIN_VM_NO_MEMORY, MB_WIN_VM_ERROR_COMMITMENT_LIMIT);
            return NULL;
        }
        const size_t span = (size + vm->config.granularity - 1) & ~(size_t)(vm->config.granularity - 1);
        if (span > MB_WIN_VM_ADDRESS_LIMIT) {
            fail(vm, MB_WIN_VM_NO_MEMORY, MB_WIN_VM_ERROR_COMMITMENT_LIMIT);
            return NULL;
        }
        void *raw = vm->ops.allocate(vm->ops.context, span + vm->config.granularity - 1);
        if (!raw) {
            fail(vm, MB_WIN_VM_NO_MEMORY, MB_WIN_VM_ERROR_COMMITMENT_LIMIT);
            return NULL;
        }
        void *base = span_round_up(raw, vm->config.granularity);
        const size_t pages = span / vm->config.page_size;
        const size_t protect_off = ((uintptr_t)sizeof(mb_win_vm_region) + pages + 3u) & ~(uintptr_t)3u;
        void *book = vm->ops.allocate(vm->ops.context, protect_off + pages * sizeof(uint32_t));
        if (!book) {
            vm->ops.release(vm->ops.context, raw, span + vm->config.granularity - 1);
            fail(vm, MB_WIN_VM_NO_MEMORY, MB_WIN_VM_ERROR_COMMITMENT_LIMIT);
            return NULL;
        }
        mb_win_vm_region *region = book;
        *region = (mb_win_vm_region){
            vm->regions, raw, book, base, span, pages, protect};
        for (size_t i = 0; i < pages; ++i) committed_of(region)[i] = 0;
        for (size_t i = 0; i < pages; ++i) protection_of(region)[i] = 0;
        vm->regions = region;
        ++vm->region_count;
        vm->live_bytes += span;
        if (commit &&
            commit_pages(vm, region, base, span, protect) != MB_WIN_VM_OK) {
            /* Backend refused enforcement; unwind the fresh reservation. */
            vm->regions = region->next;
            --vm->region_count;
            vm->live_bytes -= span;
            vm->ops.release(vm->ops.context, book, protect_off + pages * sizeof(uint32_t));
            vm->ops.release(vm->ops.context, raw, span + vm->config.granularity - 1);
            fail(vm, MB_WIN_VM_BACKEND_ERROR, MB_WIN_VM_ERROR_INVALID_PARAMETER);
            return NULL;
        }
        return base;
    }

    /* Commit-only: interior pages of one live region. */
    if (!commit || !address) {
        fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
        return NULL;
    }
    const uintptr_t lo = (uintptr_t)address & ~(uintptr_t)(vm->config.page_size - 1);
    if (size > MB_WIN_VM_ADDRESS_LIMIT || lo + size < lo ||
        lo + size > (uintptr_t)MB_WIN_VM_ADDRESS_LIMIT) {
        fail(vm, MB_WIN_VM_CONFLICT, MB_WIN_VM_ERROR_INVALID_ADDRESS);
        return NULL;
    }
    const size_t bytes = ((uintptr_t)address + size - lo + vm->config.page_size - 1) &
                         ~(size_t)(vm->config.page_size - 1);
    mb_win_vm_region *region = region_containing(vm, (const void *)lo, bytes);
    if (!region) {
        fail(vm, MB_WIN_VM_CONFLICT, MB_WIN_VM_ERROR_INVALID_ADDRESS);
        return NULL;
    }
    if (commit_pages(vm, region, (void *)lo, bytes, protect) != MB_WIN_VM_OK) {
        fail(vm, MB_WIN_VM_BACKEND_ERROR, MB_WIN_VM_ERROR_INVALID_PARAMETER);
        return NULL;
    }
    return (void *)lo;
}

mb_win_vm_status mb_win_vm_free(mb_win_vm *vm, void *address, size_t size, uint32_t free_type) {
    if (!vm || !vm->active) return MB_WIN_VM_BAD_STATE;
    const uint32_t decommit = free_type & MB_WIN_VM_MEM_DECOMMIT;
    const uint32_t release = free_type & MB_WIN_VM_MEM_RELEASE;
    if (free_type & ~(MB_WIN_VM_MEM_DECOMMIT | MB_WIN_VM_MEM_RELEASE) ||
        (decommit && release) || !address)
        return fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);

    if (release) {
        if (size)
            return fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
        mb_win_vm_region **cursor = &vm->regions;
        while (*cursor && (*cursor)->base != address) cursor = &(*cursor)->next;
        mb_win_vm_region *region = *cursor;
        if (!region)
            return fail(vm, MB_WIN_VM_CONFLICT, MB_WIN_VM_ERROR_INVALID_ADDRESS);
        *cursor = region->next;
        --vm->region_count;
        vm->live_bytes -= region->span;
        /* Snapshot before the first release invalidates the node. */
        void *span_raw = region->span_raw;
        const size_t span_request = region->span + vm->config.granularity - 1;
        const size_t pages = region->pages;
        const size_t protect_off = ((uintptr_t)sizeof(mb_win_vm_region) + pages + 3u) & ~(uintptr_t)3u;
        vm->ops.release(vm->ops.context, region->book_raw, protect_off + pages * sizeof(uint32_t));
        vm->ops.release(vm->ops.context, span_raw, span_request);
        return MB_WIN_VM_OK;
    }

    /* Decommit: never fails on pages that are not committed. */
    const uintptr_t lo = (uintptr_t)address & ~(uintptr_t)(vm->config.page_size - 1);
    if (!size || size > MB_WIN_VM_ADDRESS_LIMIT || lo + size < lo ||
        lo + size > (uintptr_t)MB_WIN_VM_ADDRESS_LIMIT)
        return fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
    const size_t bytes = ((uintptr_t)address + size - lo + vm->config.page_size - 1) &
                         ~(size_t)(vm->config.page_size - 1);
    mb_win_vm_region *region = region_containing(vm, (const void *)lo, bytes);
    if (!region)
        return fail(vm, MB_WIN_VM_CONFLICT, MB_WIN_VM_ERROR_INVALID_ADDRESS);
    if (vm->ops.protect &&
        vm->ops.protect(vm->ops.context, (void *)lo, bytes, MB_WIN_VM_PAGE_NOACCESS))
        return MB_WIN_VM_BACKEND_ERROR;
    const size_t first = (lo - (uintptr_t)region->base) / vm->config.page_size;
    const size_t count = bytes / vm->config.page_size;
    uint8_t *committed = committed_of(region);
    uint32_t *protection = protection_of(region);
    for (size_t i = 0; i < count; ++i) {
        committed[first + i] = 0;
        protection[first + i] = 0;
    }
    return MB_WIN_VM_OK;
}

mb_win_vm_status mb_win_vm_protect(mb_win_vm *vm, void *address, size_t size,
                                   uint32_t new_protect, uint32_t *old_protect) {
    if (!vm || !vm->active) return MB_WIN_VM_BAD_STATE;
    if (!is_plain_protection(new_protect) || !old_protect || !address || !size ||
        size > MB_WIN_VM_ADDRESS_LIMIT)
        return fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
    const uintptr_t lo = (uintptr_t)address & ~(uintptr_t)(vm->config.page_size - 1);
    if (lo + size < lo || lo + size > (uintptr_t)MB_WIN_VM_ADDRESS_LIMIT)
        return fail(vm, MB_WIN_VM_CONFLICT, MB_WIN_VM_ERROR_INVALID_ADDRESS);
    const size_t bytes = ((uintptr_t)address + size - lo + vm->config.page_size - 1) &
                         ~(size_t)(vm->config.page_size - 1);
    mb_win_vm_region *region = region_containing(vm, (const void *)lo, bytes);
    if (!region)
        return fail(vm, MB_WIN_VM_CONFLICT, MB_WIN_VM_ERROR_INVALID_ADDRESS);
    const size_t first = (lo - (uintptr_t)region->base) / vm->config.page_size;
    const size_t count = bytes / vm->config.page_size;
    const uint8_t *committed = committed_of(region);
    const uint32_t *protection = protection_of(region);
    for (size_t i = 0; i < count; ++i)
        if (!committed[first + i])
            return fail(vm, MB_WIN_VM_CONFLICT, MB_WIN_VM_ERROR_INVALID_ADDRESS);
    if (vm->ops.protect &&
        vm->ops.protect(vm->ops.context, (void *)lo, bytes, new_protect))
        return MB_WIN_VM_BACKEND_ERROR;
    *old_protect = protection[first];
    uint32_t *writable = protection_of(region);
    for (size_t i = 0; i < count; ++i) writable[first + i] = new_protect;
    return MB_WIN_VM_OK;
}

size_t mb_win_vm_query_ex(mb_win_vm *vm, void *process, const void *address,
                          mb_win_vm_basic_info *info, size_t length) {
    if (!vm || !vm->active) return 0;
    if (process != MB_WIN_VM_CURRENT_PROCESS) {
        fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
        return 0;
    }
    return mb_win_vm_query(vm, address, info, length);
}

size_t mb_win_vm_query(mb_win_vm *vm, const void *address,
                       mb_win_vm_basic_info *info, size_t length) {
    if (!vm || !vm->active) return 0;
    if (!info || length < MB_WIN_VM_BASIC_INFO_SIZE) {
        fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
        return 0;
    }
    const uintptr_t page = vm->config.page_size;
    const uintptr_t point = (uintptr_t)address;
    if (point >= MB_WIN_VM_ADDRESS_LIMIT) {
        fail(vm, MB_WIN_VM_INVALID_ARGUMENT, MB_WIN_VM_ERROR_INVALID_PARAMETER);
        return 0;
    }
    for (mb_win_vm_region *region = vm->regions; region; region = region->next) {
        const uintptr_t base = (uintptr_t)region->base;
        if (point < base || point >= base + region->span) continue;
        const size_t index = (point - base) / page;
        const uint8_t *committed = committed_of(region);
        const uint32_t *protection = protection_of(region);
        const int state = committed[index];
        const uint32_t protect = state ? protection[index] : MB_WIN_VM_PAGE_NOACCESS;
        /* The described region is the maximal run of pages sharing this
         * state and protection, extending both ways from the query. */
        size_t first = index;
        size_t last = index;
        while (first > 0 && committed[first - 1] == state &&
               protection[first - 1] == protection[index])
            --first;
        while (last + 1 < region->pages && committed[last + 1] == state &&
               protection[last + 1] == protection[index])
            ++last;
        *info = (mb_win_vm_basic_info){
            (void *)(base + first * page), region->base, region->allocation_protect,
            0, (last - first + 1) * page,
            state ? MB_WIN_VM_MEM_COMMIT : MB_WIN_VM_MEM_RESERVE,
            protect, MB_WIN_VM_MEM_PRIVATE};
        return MB_WIN_VM_BASIC_INFO_SIZE;
    }
    /* Free gap: from the containing granularity-aligned boundary through
     * the next region or the address-space limit. */
    uintptr_t free_base = point & ~(uintptr_t)(vm->config.granularity - 1);
    uintptr_t boundary = MB_WIN_VM_ADDRESS_LIMIT;
    for (mb_win_vm_region *region = vm->regions; region; region = region->next) {
        const uintptr_t base = (uintptr_t)region->base;
        if (base > free_base && base < boundary) boundary = base;
    }
    *info = (mb_win_vm_basic_info){
        (void *)free_base, NULL, 0, 0, boundary - free_base,
        MB_WIN_VM_MEM_FREE, MB_WIN_VM_PAGE_NOACCESS, 0};
    return MB_WIN_VM_BASIC_INFO_SIZE;
}
