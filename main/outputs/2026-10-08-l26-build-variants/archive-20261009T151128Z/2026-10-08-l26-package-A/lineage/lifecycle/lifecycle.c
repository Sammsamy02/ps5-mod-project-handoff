#include "lifecycle.h"
#include <stdint.h>

static int enter(struct probe_lifecycle *s) {
    return __atomic_test_and_set(&s->guard, __ATOMIC_ACQUIRE) ? -1 : 0;
}

static int leave(struct probe_lifecycle *s, int result) {
    __atomic_clear(&s->guard, __ATOMIC_RELEASE);
    return result;
}

static int valid_ops(const struct probe_lifecycle_ops *o) {
    return o && o->validate && o->log_status && o->worker_start && o->worker_join &&
           o->worker_is_current && o->invoke_constructor && o->finalize_dso;
}

static int span(probe_callback *begin, probe_callback *end, size_t *count) {
    uintptr_t b = (uintptr_t)begin, e = (uintptr_t)end;
    if (e < b || b % _Alignof(probe_callback) || e % _Alignof(probe_callback) ||
        (e - b) % sizeof(probe_callback)) return -1;
    *count = (e - b) / sizeof(probe_callback);
    return 0;
}

static int valid_frame_bounds(const void *frame_begin, const void *frame_end,
                              const void *hdr_begin, const void *hdr_end) {
    uintptr_t fb = (uintptr_t)frame_begin, fe = (uintptr_t)frame_end;
    uintptr_t hb = (uintptr_t)hdr_begin, he = (uintptr_t)hdr_end;
    return frame_begin && frame_end && hdr_begin && hdr_end && fe > fb && he > hb;
}

static void finalize_dso_once(struct probe_lifecycle *s, const struct probe_lifecycle_ops *o) {
    if (!s->dso_finalized) {
        o->finalize_dso(o->dso_handle);
        s->dso_finalized = 1;
    }
}

/* Normal shutdown order follows ELF fini-array then DT_FINI (the DSO atexit owner). */
static void finish(struct probe_lifecycle *s, const struct probe_lifecycle_ops *o) {
    size_t n = ((uintptr_t)s->fini_end - (uintptr_t)s->fini_begin) / sizeof(probe_callback);
    while (n) {
        probe_callback f = s->fini_begin[--n];
        if (f) f();
    }
    finalize_dso_once(s, o);
    s->state = PROBE_FINISHED;
}

/* On a throwing global constructor, unrelated fini-array callbacks have not been paired
   with a successful init sequence. Let the DSO registry clean only constructed globals. */
static void finish_partial_constructor(struct probe_lifecycle *s,
                                       const struct probe_lifecycle_ops *o) {
    finalize_dso_once(s, o);
    s->state = PROBE_FINISHED;
}

int probe_lifecycle_start(struct probe_lifecycle *s, const struct probe_lifecycle_ops *o,
                          probe_callback *ib, probe_callback *ie,
                          probe_callback *fb, probe_callback *fe,
                          const void *eh_begin, const void *eh_end,
                          const void *hdr_begin, const void *hdr_end) {
    size_t ni, nf;
    if (!s || !valid_ops(o) || enter(s)) return -1;
    if (s->state == PROBE_ACTIVE) return leave(s, 0);
    if (s->state != PROBE_NEW) return leave(s, -1);
    if (span(ib, ie, &ni) || span(fb, fe, &nf) ||
        !valid_frame_bounds(eh_begin, eh_end, hdr_begin, hdr_end)) {
        s->state = PROBE_FINISHED;
        return leave(s, -1);
    }
    (void)nf;

    /* SDK libunwind resolves against these linker-owned bounds. Their validity
       is required before the C++ boundary can catch constructor exceptions. */
    int rc = o->validate();
    if (rc) {
        s->state = PROBE_FINISHED;
        return leave(s, rc);
    }
    s->fini_begin = fb;
    s->fini_end = fe;

    for (size_t i = 0; i < ni; ++i) {
        if (!ib[i]) continue;
        rc = o->invoke_constructor(ib[i]);
        if (rc) {
            finish_partial_constructor(s, o);
            return leave(s, PROBE_CTOR_EXCEPTION);
        }
        ++s->constructors_completed;
    }

    o->log_status();
    rc = o->worker_start();
    /* worker_start guarantees nonzero means no thread/resource was created. */
    if (rc) {
        finish(s, o);
        return leave(s, rc);
    }
    s->state = PROBE_ACTIVE;
    return leave(s, 0);
}

int probe_lifecycle_stop(struct probe_lifecycle *s, const struct probe_lifecycle_ops *o) {
    if (!s || !valid_ops(o) || enter(s)) return -1;
    if (s->state == PROBE_FINISHED) return leave(s, 0);
    if (s->state == PROBE_NEW) {
        s->state = PROBE_FINISHED;
        return leave(s, 0);
    }
    s->state = PROBE_QUIESCING;
    if (o->worker_is_current()) return leave(s, -1);
    int rc = o->worker_join();
    if (rc) return leave(s, rc);
    finish(s, o);
    return leave(s, 0);
}
