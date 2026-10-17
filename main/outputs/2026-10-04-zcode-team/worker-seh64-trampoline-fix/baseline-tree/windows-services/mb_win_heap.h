#ifndef MB_WIN_HEAP_H
#define MB_WIN_HEAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MB_WIN_HEAP_ENOMEM INT32_C(12)
#define MB_WIN_HEAP_ALIGNMENT 16u

typedef enum mb_win_heap_status {
    MB_WIN_HEAP_OK = 0, MB_WIN_HEAP_INVALID_ARGUMENT, MB_WIN_HEAP_BAD_STATE,
    MB_WIN_HEAP_BUSY, MB_WIN_HEAP_LIMIT
} mb_win_heap_status;
typedef struct mb_win_heap_ops {
    void *context;
    void *(*allocate)(void *, size_t);
    void (*release)(void *, void *);
    int32_t *(*errno_slot)(void *);
} mb_win_heap_ops;
struct mb_win_heap;
typedef struct mb_win_heap_pin {
    struct mb_win_heap *owner;
    struct mb_win_heap_pin *next;
} mb_win_heap_pin;
typedef struct mb_win_heap {
    mb_win_heap_ops ops;
    mb_win_heap_pin *pins;
    size_t pin_count, live_count, live_bytes;
    uint8_t active;
} mb_win_heap;

/* Native C ABI only. Zero-initialize heap and pins before use. Do not copy,
 * move or mutate an active heap or acquired pin. Every operation, including
 * pin operations and errno access, is externally serialized. No callback may
 * reenter, mutate these objects, throw or longjmp. Ops are copied at open;
 * callback implementations/context stay alive until close succeeds.
 *
 * allocate returns NULL or a dedicated writable region of the requested size,
 * disjoint from all live allocations and owner/pin/context storage. No backend
 * alignment is assumed. release infallibly frees exactly a returned backend
 * pointer. The owner aligns user blocks to 16 bytes and stores private metadata.
 * errno_slot must return valid, prepared per-thread signed 32-bit Windows errno
 * storage on every active operation that requests it. It must not allocate or
 * return shared fallback storage. Success never clears errno; allocation size
 * overflow or backend failure writes Windows ENOMEM (12).
 *
 * malloc(0) and calloc with either zero factor request a valid freeable minimum
 * allocation. calloc checks multiplication before allocation and zeroes the
 * requested bytes. realloc(NULL,n) is malloc(n); realloc(nonNULL,0) frees and
 * returns NULL. Other resize failures leave the original allocation/data live;
 * success preserves min(old,new) bytes. Growth bytes are unspecified.
 * free(NULL) does nothing. Other free/realloc pointers must be live allocations
 * returned by this same heap; foreign, interior or freed pointers violate the
 * caller contract. Requested-byte accounting excludes private overhead and the
 * minimum storage for zero-size allocations. No memory can escape its lifetime.
 *
 * A Windows ABI adapter must acquire a stable pin before publishing any callable
 * heap import, retain it through every consumer/call, and release it only after
 * they retire. close returns BUSY while any pin or allocation remains. It never
 * frees live user blocks implicitly. There is no TLS, global installed heap,
 * locking implementation, new-handler, or full CRT runtime in this owner.
 */
mb_win_heap_status mb_win_heap_open(mb_win_heap *, const mb_win_heap_ops *);
mb_win_heap_status mb_win_heap_close(mb_win_heap *);
mb_win_heap_status mb_win_heap_acquire(mb_win_heap *, mb_win_heap_pin *);
mb_win_heap_status mb_win_heap_release(mb_win_heap_pin *);
void *mb_win_heap_malloc(mb_win_heap *, size_t);
void *mb_win_heap_calloc(mb_win_heap *, size_t count, size_t size);
void *mb_win_heap_realloc(mb_win_heap *, void *, size_t);
void mb_win_heap_free(mb_win_heap *, void *);
int32_t *mb_win_heap_errno(mb_win_heap *);

#ifdef __cplusplus
}
#endif
#endif
