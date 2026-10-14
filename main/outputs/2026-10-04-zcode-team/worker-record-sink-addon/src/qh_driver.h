// qh_driver.h — worker-record-sink-addon (qdrv1, team 2026-10-04). NEW FILE.
//
// Host-testable seam contract for the query-harness record driver.
//   Target (default): qh_driver_start() spawns the ONE bounded driver thread
//   (observer-lineage pattern) that runs run_driver_loop() against the real
//   target ops (ig_mapscan_run/ig_map_cover + direct dereference +
//   nanosleep pacing).
//   Host (compile this TU and the tests with -DQH_DRIVER_HOST_TEST and
//   -DIG_BUILD_ID=... -DIG_RT_GUOBJECT_ARRAY=...): qh_driver_start() runs the
//   SAME loop synchronously with caller-installed ops (qd_install_ops), so
//   the emitted JSONL is produced by the REAL vendored harness + REAL
//   record_sink and can be replayed through rung_check.py.
#ifndef QH_DRIVER_H
#define QH_DRIVER_H

// record_sink.h carries no extern "C" guard (byte-identical vendored lineage
// header, not edited); wrap the include so C++ TUs reference the plain symbols.
#ifdef __cplusplus
extern "C" {
#endif
#include "record_sink.h"
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Target + host entry: spawn (target) or run synchronously (host). */
int qh_driver_start(IgSink *sink);

/* Host only: install the injectable ops BEFORE qh_driver_start(). */
#ifdef QH_DRIVER_HOST_TEST
typedef struct QdOps {
    int (*mapscan_run)(void);  /* fresh per-invocation KERN_PROC_VMMAP walk   */
    int (*cover)(unsigned long long addr, unsigned long long len, void *out);
    void (*deref)(void *dst, unsigned long long src, unsigned n);
    unsigned long (*self)(void);
    int (*sleep_sec)(long sec);
} QdOps;
void qd_install_ops(const QdOps *ops);
/* Number of polls the last loop ran (for test assertions). */
unsigned qd_last_poll_count(void);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* QH_DRIVER_H */
