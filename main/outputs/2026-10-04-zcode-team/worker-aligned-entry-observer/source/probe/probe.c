/* Observer revision of the aligned-entry runtime worker.
   Same startup contract as the frozen build (imports validation, DSO
   finalizer binding, constructor invocation via lifecycle.c) but the single
   worker thread now OBSERVES UE4SS startup evidence instead of consuming
   the Lua startup input. Read-only: stat /data/plugins/UE4SS.log every ~2 s
   for up to ~60 s, on growth capture one bounded 512-byte tail with pread
   (read() drain fallback), and stat the staged mod activation root for
   existence. This module never writes UE4SS.log, never attaches to UE4SS
   memory, and creates no thread beyond the one proven worker. */
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <probe_config.h>
#include "probe_api.h"
#include "observer.h"
#include "../diagnostic.h"
#include "../import_table.h"

extern void __cxa_finalize(void *);

/* Same direct libkernel bindings the frozen diagnostic channel uses; these
   are resolved inside the composed object, not new import rows. */
extern int64_t sceKernelWrite(int, const void *, size_t);

static pthread_t worker;
static int worker_created;

void probe_event(const char *tag)
{
    diagnostic_event(tag, 0);
}

/* Split 64-bit values across two existing uint32 events (same convention as
   the frozen ffield_value_low/high events). */
static void event64(const char *tag, int64_t value)
{
    uint64_t bits = (uint64_t)value;
    diagnostic_event(tag, (uint32_t)(bits & 0xffffffffu));
    diagnostic_event(tag, (uint32_t)(value < 0 ? 0 : (bits >> 32)));
}

int probe_runtime_validate_and_preload(void)
{
    int result = probe_imports_validate();
    if (result != 0)
        return result;
    void (*volatile finalizer)(void *) = __cxa_finalize;
    if (finalizer == NULL) {
        diagnostic_event("dso_finalizer_binding_missing", 0);
        return -10;
    }
    /* No startup input is loaded by the observer revision. */
    diagnostic_event("observer_revision_no_input", 0);
    return 0;
}

void probe_runtime_log_status(void)
{
    diagnostic_event("full_runtime_constructors_complete", 0);
}

/* Bounded single-line text emitter onto the retained O_APPEND channel, the
   same write pattern as diagnostic.c's write_bounded. */
static void emit_text_line(const char *tag, const char *text)
{
    int fd = diagnostic_channel_fd();
    if (fd < 0 || !diagnostic_ready())
        return;
    char line[1024];
    size_t used = 0;
    const char *parts[4] = {"[" PROBE_TITLE "] " PROBE_MARKER " ", tag, "=", text};
    for (unsigned i = 0; i < 4; ++i) {
        const char *cursor = parts[i];
        if (cursor == NULL)
            continue;
        while (*cursor && used < sizeof(line) - 1)
            line[used++] = *cursor++;
    }
    line[used++] = '\n';
    size_t offset = 0;
    unsigned attempts = 0;
    while (offset < used && attempts < 64u) {
        ++attempts;
        int64_t written = sceKernelWrite(fd, line + offset, used - offset);
        if (written <= 0 || (uint64_t)written > used - offset)
            return;
        offset += (size_t)written;
    }
}

static void poll_once(int64_t *baseline, unsigned *growth_events,
                      int *root_ever, unsigned *tail_failures)
{
    int64_t size = 0;
    int is_dir = 0;
    if (observer_stat_path(OBSERVER_UE4SS_LOG_PATH, &size, &is_dir) == 0) {
        event64("ue4ss_log_size", size);
        if (is_dir)
            diagnostic_event("ue4ss_log_not_regular", 1);
        enum observer_growth growth = observer_classify(*baseline, size);
        if (*baseline < 0) {
            event64("ue4ss_log_baseline", size);
            *baseline = size;
        } else if (growth == OBSERVER_GREW) {
            diagnostic_event("ue4ss_log_grew", (uint32_t)(size - *baseline));
            *growth_events += 1;
            unsigned char tail[OBSERVER_TAIL_BYTES];
            size_t tail_len = 0;
            int tail_result = observer_read_tail(OBSERVER_UE4SS_LOG_PATH, size,
                                                 tail, sizeof(tail), &tail_len,
                                                 OBSERVER_TAIL_DRAIN_LIMIT);
            if (tail_result == OBSERVER_TAIL_OK) {
                char text[OBSERVER_TAIL_TEXT_MAX];
                int truncated = 0;
                int window_at_start = size <= (int64_t)OBSERVER_TAIL_BYTES;
                size_t text_len = observer_last_line(tail, tail_len,
                                                     window_at_start, text,
                                                     sizeof(text), &truncated);
                diagnostic_event("ue4ss_tail_bytes", (uint32_t)tail_len);
                diagnostic_event("ue4ss_tail_text_len", (uint32_t)text_len);
                if (truncated)
                    diagnostic_event("ue4ss_tail_truncated", 1);
                if (text_len > 0)
                    emit_text_line("ue4ss_tail", text);
            } else if (tail_result == OBSERVER_TAIL_DRAIN_CAPPED) {
                diagnostic_event("ue4ss_tail_drain_capped", 1);
            } else {
                diagnostic_event("ue4ss_tail_read_failed", errno);
                *tail_failures += 1;
            }
            *baseline = size;
        } else if (growth == OBSERVER_SHRANK) {
            diagnostic_event("ue4ss_log_shrank", (uint32_t)*baseline);
            event64("ue4ss_log_new_size", size);
            *baseline = size; /* rotation/truncation: re-baseline, no tail */
        }
    } else {
        diagnostic_event("ue4ss_log_stat_failed", errno);
    }
    int root_dir = 0;
    int root_state = observer_check_exists(OBSERVER_MOD_ROOT_PATH, &root_dir);
    if (root_state == 1) {
        if (root_ever != NULL && !*root_ever)
            *root_ever = 1;
        diagnostic_event("mod_root_present", (uint32_t)(root_dir ? 2 : 1));
    } else if (root_state == 0) {
        diagnostic_event("mod_root_absent", 0);
    } else {
        diagnostic_event("mod_root_stat_failed", errno);
    }
    int variant_dir = 0;
    int variant_state = observer_check_exists(OBSERVER_MOD_ROOT_UE4SS_PATH,
                                              &variant_dir);
    if (variant_state == 1)
        diagnostic_event("mod_root_ue4ss_present", (uint32_t)(variant_dir ? 2 : 1));
    else if (variant_state == 0)
        diagnostic_event("mod_root_ue4ss_absent", 0);
    else
        diagnostic_event("mod_root_ue4ss_stat_failed", errno);
}

static void *worker_main(void *unused)
{
    (void)unused;
    diagnostic_event("observer_worker_entry", 0);
    int64_t baseline = -1;
    unsigned growth_events = 0;
    unsigned tail_failures = 0;
    int root_ever = 0;
    unsigned polls = 0;
    for (unsigned poll = 0; poll < OBSERVER_POLL_COUNT; ++poll) {
        struct timespec remaining = {OBSERVER_POLL_SECONDS, 0};
        unsigned retries = 0;
        for (;;) {
            int result = nanosleep(&remaining, &remaining);
            int error_number = result != 0 ? errno : 0;
            if (result == 0)
                break;
            if (error_number != EINTR || retries++ >= 8u) {
                diagnostic_event("observer_sleep_failed", (uint32_t)error_number);
                goto summary;
            }
        }
        polls = poll + 1;
        poll_once(&baseline, &growth_events, &root_ever, &tail_failures);
    }
summary:
    diagnostic_event("observer_polls_completed", polls);
    diagnostic_event("observer_growth_events", growth_events);
    diagnostic_event("observer_tail_failures", tail_failures);
    diagnostic_event("observer_root_ever_present", (uint32_t)root_ever);
    event64("observer_final_size", baseline < 0 ? 0 : baseline);
    diagnostic_event("observer_complete", 0);
    return NULL;
}

int probe_worker_start(void)
{
    if (worker_created)
        return 0;
    int result = pthread_create(&worker, NULL, worker_main, NULL);
    diagnostic_event("worker_create_result", (uint32_t)result);
    if (result == 0)
        worker_created = 1;
    return result;
}

int probe_worker_is_current(void)
{
    return worker_created && pthread_self() == worker;
}

int probe_worker_quiesce_join(void)
{
    if (!worker_created)
        return 0;
    if (probe_worker_is_current())
        return EDEADLK;
    int result = pthread_join(worker, NULL);
    diagnostic_event("worker_join_result", (uint32_t)result);
    if (result == 0)
        worker_created = 0;
    return result;
}

void probe_runtime_release_input(void)
{
    /* The observer revision loads no startup input. */
}
