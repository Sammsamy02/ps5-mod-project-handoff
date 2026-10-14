#include "mb_win_heap.h"

_Static_assert(sizeof(size_t) == 8 && sizeof(void *) == 8,
               "Windows heap owner requires 64-bit sizes and pointers");
_Static_assert(sizeof(int32_t) == 4, "Windows errno requires 32-bit storage");

typedef struct allocation_header {
    void *raw;
    mb_win_heap *owner;
    size_t requested;
} allocation_header;
_Static_assert(_Alignof(allocation_header) <= MB_WIN_HEAP_ALIGNMENT &&
               sizeof(allocation_header) % _Alignof(allocation_header) == 0,
               "Aligned payload must also align its preceding private header");

static allocation_header *header_for(void *pointer) {
    return (allocation_header *)((unsigned char *)pointer - sizeof(allocation_header));
}
static void no_memory(mb_win_heap *heap) {
    /* The prepared per-thread slot is a caller contract, never host errno. */
    *heap->ops.errno_slot(heap->ops.context) = MB_WIN_HEAP_ENOMEM;
}

mb_win_heap_status mb_win_heap_open(mb_win_heap *heap, const mb_win_heap_ops *ops) {
    if (!heap || !ops || !ops->allocate || !ops->release || !ops->errno_slot)
        return MB_WIN_HEAP_INVALID_ARGUMENT;
    if (heap->active || heap->pins || heap->pin_count || heap->live_count || heap->live_bytes)
        return MB_WIN_HEAP_BAD_STATE;
    heap->ops = *ops;
    heap->active = 1;
    return MB_WIN_HEAP_OK;
}
mb_win_heap_status mb_win_heap_close(mb_win_heap *heap) {
    if (!heap) return MB_WIN_HEAP_INVALID_ARGUMENT;
    if (heap->pins || heap->pin_count || heap->live_count || heap->live_bytes)
        return MB_WIN_HEAP_BUSY;
    *heap = (mb_win_heap){0};
    return MB_WIN_HEAP_OK;
}
mb_win_heap_status mb_win_heap_acquire(mb_win_heap *heap, mb_win_heap_pin *pin) {
    if (!heap || !pin) return MB_WIN_HEAP_INVALID_ARGUMENT;
    if (!heap->active || pin->owner || pin->next) return MB_WIN_HEAP_BAD_STATE;
    if (heap->pin_count == SIZE_MAX) return MB_WIN_HEAP_LIMIT;
    pin->owner = heap;
    pin->next = heap->pins;
    heap->pins = pin;
    ++heap->pin_count;
    return MB_WIN_HEAP_OK;
}
mb_win_heap_status mb_win_heap_release(mb_win_heap_pin *pin) {
    if (!pin) return MB_WIN_HEAP_INVALID_ARGUMENT;
    mb_win_heap *heap = pin->owner;
    if (!heap || !heap->active || !heap->pin_count) return MB_WIN_HEAP_BAD_STATE;
    mb_win_heap_pin **cursor = &heap->pins;
    while (*cursor && *cursor != pin) cursor = &(*cursor)->next;
    if (!*cursor) return MB_WIN_HEAP_BAD_STATE;
    *cursor = pin->next;
    --heap->pin_count;
    *pin = (mb_win_heap_pin){0};
    return MB_WIN_HEAP_OK;
}
int32_t *mb_win_heap_errno(mb_win_heap *heap) {
    return heap && heap->active ? heap->ops.errno_slot(heap->ops.context) : NULL;
}

void *mb_win_heap_malloc(mb_win_heap *heap, size_t size) {
    if (!heap || !heap->active) return NULL;
    const size_t overhead = sizeof(allocation_header) + MB_WIN_HEAP_ALIGNMENT - 1;
    size_t payload = size ? size : 1;
    if (payload > SIZE_MAX - overhead || heap->live_count == SIZE_MAX ||
        size > SIZE_MAX - heap->live_bytes) {
        no_memory(heap);
        return NULL;
    }
    void *raw = heap->ops.allocate(heap->ops.context, payload + overhead);
    if (!raw) {
        no_memory(heap);
        return NULL;
    }
    /* Modulo arithmetic avoids uintptr_t overflow while computing padding.
     * The backend's dedicated valid allocation guarantees the pointer extent. */
    size_t remainder = ((uintptr_t)raw % MB_WIN_HEAP_ALIGNMENT +
                        sizeof(allocation_header) % MB_WIN_HEAP_ALIGNMENT) % MB_WIN_HEAP_ALIGNMENT;
    size_t padding = remainder ? MB_WIN_HEAP_ALIGNMENT - remainder : 0;
    unsigned char *pointer = (unsigned char *)raw + sizeof(allocation_header) + padding;
    allocation_header *header = header_for(pointer);
    *header = (allocation_header){raw, heap, size};
    ++heap->live_count;
    heap->live_bytes += size;
    return pointer;
}
void *mb_win_heap_calloc(mb_win_heap *heap, size_t count, size_t size) {
    if (!heap || !heap->active) return NULL;
    if (count && size > SIZE_MAX / count) {
        no_memory(heap);
        return NULL;
    }
    size_t total = count * size;
    unsigned char *pointer = mb_win_heap_malloc(heap, total);
    if (pointer)
        for (size_t i = 0; i < total; ++i) pointer[i] = 0;
    return pointer;
}
void mb_win_heap_free(mb_win_heap *heap, void *pointer) {
    if (!pointer) return;
    /* Non-NULL pointers must be live allocations from this active owner. */
    allocation_header *header = header_for(pointer);
    void *raw = header->raw;
    --heap->live_count;
    heap->live_bytes -= header->requested;
    heap->ops.release(heap->ops.context, raw);
}
void *mb_win_heap_realloc(mb_win_heap *heap, void *pointer, size_t size) {
    if (!pointer) return mb_win_heap_malloc(heap, size);
    if (!size) {
        mb_win_heap_free(heap, pointer);
        return NULL;
    }
    allocation_header *old = header_for(pointer);
    if (size == old->requested) return pointer;
    unsigned char *replacement = mb_win_heap_malloc(heap, size);
    if (!replacement) return NULL;
    size_t preserve = old->requested < size ? old->requested : size;
    const unsigned char *source = pointer;
    for (size_t i = 0; i < preserve; ++i) replacement[i] = source[i];
    mb_win_heap_free(heap, pointer);
    return replacement;
}
