#include "mb_win_threads.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct native_thread native_thread;
typedef struct native_domain {
    pthread_mutex_t mutex;
    pthread_key_t key;
    size_t registrations, pin_count;
    uint32_t tls_count;
    uint64_t allocated[MB_WIN_TLS_CAPACITY / 64];
    native_thread *threads;
    mb_win_thread_pin *pins;
    int closing, key_live;
} native_domain;
struct native_thread {
    native_domain *owner;
    native_thread *previous, *next;
    int32_t windows_errno;
    uint32_t last_error;
    void *values[MB_WIN_TLS_CAPACITY];
};
_Static_assert(sizeof(int32_t) == 4 && sizeof(uint32_t) == 4, "Windows thread state is 32 bits");
_Static_assert(MB_WIN_TLS_CAPACITY % 64 == 0, "TLS bitmap covers complete words");

static mb_win_thread_status native_result(int *out, int rc) {
    if (out) *out = rc;
    return rc ? MB_WIN_THREADS_NATIVE_ERROR : MB_WIN_THREADS_OK;
}
static void lock(native_domain *d) { if (!d || pthread_mutex_lock(&d->mutex)) abort(); }
static void unlock(native_domain *d) { if (pthread_mutex_unlock(&d->mutex)) abort(); }
static void unlink_thread(native_domain *d, native_thread *t) {
    if (!d->registrations || t->owner != d) abort();
    if (t->previous) t->previous->next = t->next;
    else d->threads = t->next;
    if (t->next) t->next->previous = t->previous;
    --d->registrations;
}
static void thread_exited(void *value) {
    native_thread *t = value;
    native_domain *d = t->owner;
    lock(d);
    unlink_thread(d, t);
    unlock(d);
    free(t);
}

mb_win_thread_status mb_win_threads_open(mb_win_thread_domain *domain, int *error) {
    if (!domain) return MB_WIN_THREADS_INVALID_ARGUMENT;
    if (domain->state) return MB_WIN_THREADS_BAD_STATE;
    native_domain *d = calloc(1, sizeof(*d));
    if (!d) return native_result(error, ENOMEM);
    int rc = pthread_mutex_init(&d->mutex, NULL);
    if (rc) { free(d); return native_result(error, rc); }
    rc = pthread_key_create(&d->key, thread_exited);
    if (rc) {
        /* Preserve a mutex that unexpectedly refuses destruction for close retry. */
        int cleanup = pthread_mutex_destroy(&d->mutex);
        if (cleanup) { d->closing = 1; domain->state = d; }
        else free(d);
        return native_result(error, rc);
    }
    d->key_live = 1;
    domain->state = d;
    return native_result(error, 0);
}

mb_win_thread_status mb_win_threads_enter(mb_win_thread_domain *domain, int *error) {
    if (!domain) return MB_WIN_THREADS_INVALID_ARGUMENT;
    native_domain *d = domain->state;
    if (!d) return MB_WIN_THREADS_BAD_STATE;
    lock(d);
    if (d->closing || !d->key_live) { unlock(d); return MB_WIN_THREADS_BAD_STATE; }
    if (pthread_getspecific(d->key)) { unlock(d); return native_result(error, 0); }
    if (d->registrations == SIZE_MAX) { unlock(d); return native_result(error, ENOMEM); }
    native_thread *t = calloc(1, sizeof(*t));
    if (!t) { unlock(d); return native_result(error, ENOMEM); }
    t->owner = d;
    int rc = pthread_setspecific(d->key, t);
    if (rc) { free(t); unlock(d); return native_result(error, rc); }
    t->next = d->threads;
    if (d->threads) d->threads->previous = t;
    d->threads = t;
    ++d->registrations;
    unlock(d);
    return native_result(error, 0);
}

mb_win_thread_status mb_win_threads_leave(mb_win_thread_domain *domain, int *error) {
    if (!domain) return MB_WIN_THREADS_INVALID_ARGUMENT;
    native_domain *d = domain->state;
    if (!d) return MB_WIN_THREADS_BAD_STATE;
    lock(d);
    if (d->closing || !d->key_live) { unlock(d); return MB_WIN_THREADS_BAD_STATE; }
    native_thread *t = pthread_getspecific(d->key);
    if (!t) { unlock(d); return native_result(error, 0); }
    int rc = pthread_setspecific(d->key, NULL);
    if (rc) { unlock(d); return native_result(error, rc); }
    unlink_thread(d, t);
    unlock(d);
    free(t);
    return native_result(error, 0);
}

mb_win_thread_status mb_win_threads_close(mb_win_thread_domain *domain, int *error) {
    if (!domain) return MB_WIN_THREADS_INVALID_ARGUMENT;
    native_domain *d = domain->state;
    if (!d) return native_result(error, 0);
    lock(d);
    if (d->registrations || d->pin_count || d->tls_count) { unlock(d); return MB_WIN_THREADS_BUSY; }
    d->closing = 1;
    unlock(d);
    if (d->key_live) {
        int rc = pthread_key_delete(d->key);
        if (rc) return native_result(error, rc);
        d->key_live = 0;
    }
    int rc = pthread_mutex_destroy(&d->mutex);
    if (rc) return native_result(error, rc);
    domain->state = NULL;
    free(d);
    return native_result(error, 0);
}

int32_t *mb_win_threads_errno(void *opaque) {
    mb_win_thread_domain *domain = opaque;
    if (!domain || !domain->state) return NULL;
    native_domain *d = domain->state;
    /* A registered current thread keeps the key/domain alive until leave/exit. */
    if (!d->key_live || d->closing) return NULL;
    native_thread *t = pthread_getspecific(d->key);
    return t ? &t->windows_errno : NULL;
}
void mb_win_threads_lock(void *opaque) {
    mb_win_thread_domain *domain = opaque;
    if (!domain) abort();
    lock(domain->state);
}
void mb_win_threads_unlock(void *opaque) {
    mb_win_thread_domain *domain = opaque;
    if (!domain || !domain->state) abort();
    unlock(domain->state);
}
void *mb_win_threads_allocate(void *domain, size_t size) { (void)domain; return malloc(size); }
void mb_win_threads_release(void *domain, void *allocation) { (void)domain; free(allocation); }


mb_win_thread_status mb_win_threads_acquire(mb_win_thread_domain *domain, mb_win_thread_pin *pin) {
    if (!domain || !pin) return MB_WIN_THREADS_INVALID_ARGUMENT;
    native_domain *d = domain->state;
    if (!d) return MB_WIN_THREADS_BAD_STATE;
    lock(d);
    if (d->closing || !d->key_live || pin->owner || pin->next) {
        unlock(d); return MB_WIN_THREADS_BAD_STATE;
    }
    if (d->pin_count == SIZE_MAX) { unlock(d); return MB_WIN_THREADS_NATIVE_ERROR; }
    pin->owner = domain;
    pin->next = d->pins;
    d->pins = pin;
    ++d->pin_count;
    unlock(d);
    return MB_WIN_THREADS_OK;
}
mb_win_thread_status mb_win_threads_release_pin(mb_win_thread_pin *pin) {
    if (!pin) return MB_WIN_THREADS_INVALID_ARGUMENT;
    if (!pin->owner || !pin->owner->state) return MB_WIN_THREADS_BAD_STATE;
    native_domain *d = pin->owner->state;
    lock(d);
    mb_win_thread_pin **cursor = &d->pins;
    while (*cursor && *cursor != pin) cursor = &(*cursor)->next;
    if (!*cursor || !d->pin_count) { unlock(d); return MB_WIN_THREADS_BAD_STATE; }
    *cursor = pin->next;
    --d->pin_count;
    pin->owner = NULL;
    pin->next = NULL;
    unlock(d);
    return MB_WIN_THREADS_OK;
}

static native_thread *required_thread(void *opaque) {
    mb_win_thread_domain *domain = opaque;
    if (!domain || !domain->state) abort();
    native_domain *d = domain->state;
    if (!d->key_live || d->closing) abort();
    native_thread *t = pthread_getspecific(d->key);
    if (!t || t->owner != d) abort();
    return t;
}
uint32_t *mb_win_threads_last_error(void *domain) {
    return &required_thread(domain)->last_error;
}
static int index_allocated(native_domain *d, uint32_t index) {
    return index < MB_WIN_TLS_CAPACITY &&
        (d->allocated[index / 64] & (UINT64_C(1) << (index % 64))) != 0;
}
static int index_accessible(native_domain *d, uint32_t index) {
    return index < MB_WIN_TLS_MINIMUM_AVAILABLE || index_allocated(d, index);
}
uint32_t mb_win_threads_tls_alloc(void *domain) {
    native_thread *t = required_thread(domain);
    native_domain *d = t->owner;
    lock(d);
    for (uint32_t index = 0; index < MB_WIN_TLS_CAPACITY; ++index) {
        if (index_allocated(d, index)) continue;
        d->allocated[index / 64] |= UINT64_C(1) << (index % 64);
        ++d->tls_count;
        /* Includes earlier occupants of a reused slot and threads that wrote
         * an unallocated minimum-range index. Pointees remain caller-owned. */
        for (native_thread *current = d->threads; current; current = current->next)
            current->values[index] = NULL;
        unlock(d);
        return index;
    }
    t->last_error = MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
    unlock(d);
    return MB_WIN_TLS_OUT_OF_INDEXES;
}
int32_t mb_win_threads_tls_free(void *domain, uint32_t index) {
    native_thread *t = required_thread(domain);
    native_domain *d = t->owner;
    lock(d);
    if (!index_allocated(d, index)) {
        t->last_error = MB_WIN_ERROR_INVALID_PARAMETER;
        unlock(d); return 0;
    }
    d->allocated[index / 64] &= ~(UINT64_C(1) << (index % 64));
    --d->tls_count;
    unlock(d);
    return 1;
}
void *mb_win_threads_tls_get(void *domain, uint32_t index) {
    native_thread *t = required_thread(domain);
    native_domain *d = t->owner;
    lock(d);
    if (!index_accessible(d, index)) {
        t->last_error = MB_WIN_ERROR_INVALID_PARAMETER;
        unlock(d); return NULL;
    }
    void *value = t->values[index];
    t->last_error = MB_WIN_ERROR_SUCCESS;
    unlock(d);
    return value;
}
int32_t mb_win_threads_tls_set(void *domain, uint32_t index, void *value) {
    native_thread *t = required_thread(domain);
    native_domain *d = t->owner;
    lock(d);
    if (!index_accessible(d, index)) {
        t->last_error = MB_WIN_ERROR_INVALID_PARAMETER;
        unlock(d); return 0;
    }
    t->values[index] = value;
    unlock(d);
    return 1;
}
