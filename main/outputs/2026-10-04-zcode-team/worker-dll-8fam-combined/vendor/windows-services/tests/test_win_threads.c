#include "../mb_win_threads.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); abort(); } } while (0)
typedef struct barrier {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int ready, release;
} barrier;
typedef struct worker {
    mb_win_thread_domain *domain;
    barrier *gate;
    int32_t value;
    int explicit_leave;
    uintptr_t slot_address;
} worker;
static void *run(void *opaque) {
    worker *w = opaque;
    CHECK(mb_win_threads_errno(w->domain) == NULL);
    int rc = -1;
    CHECK(mb_win_threads_enter(w->domain, &rc) == MB_WIN_THREADS_OK && rc == 0);
    int32_t *slot = mb_win_threads_errno(w->domain);
    CHECK(slot && *slot == 0);
    w->slot_address = (uintptr_t)slot;
    *slot = w->value;
    errno = EDOM;
    CHECK(mb_win_threads_enter(w->domain, &rc) == MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_errno(w->domain) == slot && *slot == w->value && errno == EDOM);
    CHECK(pthread_mutex_lock(&w->gate->mutex) == 0);
    ++w->gate->ready;
    CHECK(pthread_cond_broadcast(&w->gate->cond) == 0);
    while (!w->gate->release) CHECK(pthread_cond_wait(&w->gate->cond, &w->gate->mutex) == 0);
    CHECK(pthread_mutex_unlock(&w->gate->mutex) == 0);
    CHECK(*slot == w->value && errno == EDOM);
    if (w->explicit_leave) {
        CHECK(mb_win_threads_leave(w->domain, &rc) == MB_WIN_THREADS_OK);
        CHECK(mb_win_threads_errno(w->domain) == NULL);
        CHECK(mb_win_threads_leave(w->domain, &rc) == MB_WIN_THREADS_OK);
    }
    return NULL; /* The other worker exercises the actual pthread destructor. */
}
int main(void) {
    mb_win_thread_domain domain = {0}; int rc = -1;
    CHECK(mb_win_threads_open(&domain, &rc) == MB_WIN_THREADS_OK && rc == 0);
    CHECK(mb_win_threads_open(&domain, &rc) == MB_WIN_THREADS_BAD_STATE);
    CHECK(mb_win_threads_errno(&domain) == NULL);
    CHECK(mb_win_threads_enter(&domain, &rc) == MB_WIN_THREADS_OK);
    int32_t *main_slot = mb_win_threads_errno(&domain);
    CHECK(main_slot && *main_slot == 0); *main_slot = 17; errno = ERANGE;
    CHECK(mb_win_threads_close(&domain, &rc) == MB_WIN_THREADS_BUSY);
    CHECK(mb_win_threads_errno(&domain) == main_slot && *main_slot == 17 && errno == ERANGE);
    barrier gate = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0};
    worker a = {&domain, &gate, 41, 1, 0}, b = {&domain, &gate, 73, 0, 0};
    pthread_t ta, tb;
    CHECK(pthread_create(&ta, NULL, run, &a) == 0 && pthread_create(&tb, NULL, run, &b) == 0);
    CHECK(pthread_mutex_lock(&gate.mutex) == 0);
    while (gate.ready != 2) CHECK(pthread_cond_wait(&gate.cond, &gate.mutex) == 0);
    CHECK(a.slot_address != b.slot_address && a.slot_address != (uintptr_t)main_slot && b.slot_address != (uintptr_t)main_slot);
    CHECK(*main_slot == 17);
    CHECK(mb_win_threads_close(&domain, &rc) == MB_WIN_THREADS_BUSY);
    gate.release = 1; CHECK(pthread_cond_broadcast(&gate.cond) == 0);
    CHECK(pthread_mutex_unlock(&gate.mutex) == 0);
    CHECK(pthread_join(ta, NULL) == 0 && pthread_join(tb, NULL) == 0);
    CHECK(mb_win_threads_errno(&domain) == main_slot && *main_slot == 17);
    CHECK(mb_win_threads_leave(&domain, &rc) == MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_close(&domain, &rc) == MB_WIN_THREADS_OK && domain.state == NULL);
    CHECK(mb_win_threads_close(&domain, &rc) == MB_WIN_THREADS_OK);
    CHECK(pthread_cond_destroy(&gate.cond) == 0 && pthread_mutex_destroy(&gate.mutex) == 0);
    puts("3 actual native threads: isolated stable Windows errno, explicit detach, exit destructor, busy close passed");
    return 0;
}
