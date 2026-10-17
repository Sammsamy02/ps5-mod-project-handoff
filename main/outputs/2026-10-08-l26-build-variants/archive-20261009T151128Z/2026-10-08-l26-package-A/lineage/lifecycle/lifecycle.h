#ifndef FULL_RUNTIME_LIFECYCLE_H
#define FULL_RUNTIME_LIFECYCLE_H

#include <stddef.h>

typedef void (*probe_callback)(void);

enum probe_lifecycle_result {
    PROBE_CTOR_EXCEPTION = -2
};

struct probe_lifecycle_ops {
    int (*validate)(void);
    void (*log_status)(void);
    int (*worker_start)(void);
    int (*worker_join)(void);
    int (*worker_is_current)(void);
    int (*invoke_constructor)(probe_callback);
    void (*finalize_dso)(void *);
    void *dso_handle;
};

enum probe_lifecycle_state { PROBE_NEW, PROBE_ACTIVE, PROBE_QUIESCING, PROBE_FINISHED };

struct probe_lifecycle {
    unsigned char guard;
    enum probe_lifecycle_state state;
    probe_callback *fini_begin;
    probe_callback *fini_end;
    size_t constructors_completed;
    int dso_finalized;
};

#define PROBE_LIFECYCLE_INIT { 0, PROBE_NEW, NULL, NULL, 0, 0 }

#ifdef __cplusplus
extern "C" {
int probe_lifecycle_start(struct probe_lifecycle *, const struct probe_lifecycle_ops *,
                          probe_callback *, probe_callback *, probe_callback *, probe_callback *,
                          const void *, const void *, const void *, const void *);
int probe_lifecycle_stop(struct probe_lifecycle *, const struct probe_lifecycle_ops *);
int probe_call_constructor_catching(probe_callback) noexcept;
}
#else
int probe_lifecycle_start(struct probe_lifecycle *, const struct probe_lifecycle_ops *,
                          probe_callback *, probe_callback *, probe_callback *, probe_callback *,
                          const void *, const void *, const void *, const void *);
int probe_lifecycle_stop(struct probe_lifecycle *, const struct probe_lifecycle_ops *);
int probe_call_constructor_catching(probe_callback);
#endif

#endif
