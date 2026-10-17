#pragma once
#include <stddef.h>
typedef int (*probe_callback)(void);
enum { PROBE_NEW = 0, PROBE_ACTIVE = 1, PROBE_FINISHED = 2 };
struct probe_lifecycle { int state; };
#define PROBE_LIFECYCLE_INIT { PROBE_NEW }
struct probe_lifecycle_ops {
  int (*validate_and_preload)(void);
  void (*log_status)(void);
  int (*worker_start)(void);
  void (*worker_quiesce_join)(void);
  int (*worker_is_current)(void);
  int (*invoke_constructor)(probe_callback);
  void (*finalize_dso)(void *);
  void *dso_handle;
};
int probe_runtime_validate_and_preload(void);
void probe_runtime_log_status(void);
int probe_worker_start(void);
void probe_worker_quiesce_join(void);
int probe_worker_is_current(void);
int probe_lifecycle_start(struct probe_lifecycle *, const struct probe_lifecycle_ops *,
                          probe_callback[], probe_callback[], probe_callback[], probe_callback[],
                          unsigned char[], unsigned char[], unsigned char[], unsigned char[]);
int probe_lifecycle_stop(struct probe_lifecycle *, const struct probe_lifecycle_ops *);
int probe_call_constructor_catching(probe_callback);
void probe_runtime_release_input(void);
