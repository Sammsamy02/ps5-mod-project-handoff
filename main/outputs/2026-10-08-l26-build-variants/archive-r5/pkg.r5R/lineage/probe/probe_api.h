#ifndef BUFFERED_RUNTIME_PROBE_API_H
#define BUFFERED_RUNTIME_PROBE_API_H
void probe_event(const char *tag);
int probe_runtime_validate_and_preload(void);
void probe_runtime_log_status(void);
int probe_worker_start(void);
int probe_worker_quiesce_join(void);
int probe_worker_is_current(void);
void probe_runtime_release_input(void);
#endif
