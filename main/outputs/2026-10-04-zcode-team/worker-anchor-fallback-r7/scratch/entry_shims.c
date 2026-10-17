/* Link shims for the host micro-test TU (never executed on this path). */
#include "diagnostic.h"
#include "probe/probe_api.h"
#include "stubc/modforge_activation.h"
int diagnostic_ready(void) { return 1; }
int diagnostic_open_channel(void) { return 1; }
void diagnostic_close_channel(void) {}
void diagnostic_event(const char *t, uint32_t v) { (void)t; (void)v; }
int probe_lifecycle_start(struct probe_lifecycle *l, const struct probe_lifecycle_ops *o,
                          probe_callback a[], probe_callback b[], probe_callback c[], probe_callback d[],
                          unsigned char e[], unsigned char f[], unsigned char g[], unsigned char h[])
{ (void)l;(void)o;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; return -1; }
int probe_lifecycle_stop(struct probe_lifecycle *l, const struct probe_lifecycle_ops *o)
{ (void)l;(void)o; return 0; }
int probe_call_constructor_catching(probe_callback cb) { (void)cb; return 0; }
void probe_runtime_release_input(void) {}
int probe_runtime_validate_and_preload(void) { return 0; }
void probe_runtime_log_status(void) {}
int probe_worker_start(void) { return 0; }
void probe_worker_quiesce_join(void) {}
int probe_worker_is_current(void) { return 1; }
int mfa_activate(const struct mfa_config *c, struct mfa_activation **a, struct mfa_error *e)
{ (void)c;(void)a;(void)e; return 1; }
const char *mfa_result_name(int r) { (void)r; return "shim"; }
unsigned long long mfa_preloaded_bytes(struct mfa_activation *a) { (void)a; return 0; }
int mfa_status_open(struct mfa_status *s, const char *r, long p, unsigned long long n)
{ (void)s;(void)r;(void)p;(void)n; return -1; }
void mfa_status_write(struct mfa_status *s, const char *k, int c, const char *n)
{ (void)s;(void)k;(void)c;(void)n; }
void mfa_status_close(struct mfa_status *s) { (void)s; }
int sceKernelOpen(const char *p, int f, unsigned m) { (void)p;(void)f;(void)m; return -1; }
long long sceKernelWrite(int fd, const void *b, unsigned long n) { (void)fd;(void)b;(void)n; return -1; }
int sceKernelClose(int fd) { (void)fd; return 0; }
int modbridge_runtime_start_with_activation(const char *a, const char *b, const char *c,
                                            struct mfa_activation *d, char *e, unsigned f)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 1; }
int modbridge_runtime_stop(void) { return 0; }
