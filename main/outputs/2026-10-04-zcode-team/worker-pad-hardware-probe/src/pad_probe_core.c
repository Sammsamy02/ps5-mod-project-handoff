// pad_probe_core.c — C port of the standalone probe runner
// (worker-pad-hardware-probe, team 2026-10-04). Derived-origin:
// worker-real-input-pad/pad_probe/PadProbe.cpp over the pad_source.c port
// of the retained provider. Ordering contract (prepared module):
// probe_start first; then source activation; then ONE keybind registration
// (OPTIONS->F7); then the bounded poll loop; probe_end / probe_failed last.
// All emitted names are compile-time constants and all numbers are
// integers, so the separator-correct v3 writer yields valid JSONL.
#include "pad_probe_core.h"
#include "ingame_libc.h"

#include <time.h>

typedef struct Recorder {
    IgSink *sink;
    const char *build_id;
    int *seq;
    int64_t t0_ms;  // monotonic start
} Recorder;

static int64_t now_mono_ms(int *ok)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        if (ok) *ok = 0;
        return -1;
    }
    if (ok) *ok = 1;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void emit(const Recorder *r, IgBuf *b, const char *exp, int64_t rc,
                 int64_t eno)
{
    ig_record_begin(b, (*r->seq)++, exp, rc, eno, r->build_id);
    ig_kv_int(b, "ts_ms", now_mono_ms(0) - r->t0_ms);
}

static void emit_probe_failed(const Recorder *r, IgBuf *b, const char *stage,
                              const char *detail, int64_t rc)
{
    emit(r, b, "probe_failed", rc, 0);
    ig_kv_str(b, "event", "probe_failed", 16);
    ig_kv_str(b, "stage", stage, 16);
    if (detail) ig_kv_str(b, "detail", detail, 40);
    ig_record_flush(b);
    if (ig_sink_write_record(r->sink, b) != 0) r->sink->failed = 1;
}

static void summary_append_u64(char *buf, size_t cap, uint64_t v)
{
    char tmp[24];
    size_t m = 0, L;
    if (v == 0) tmp[m++] = '0';
    while (v > 0 && m < sizeof tmp) { tmp[m++] = (char)('0' + (v % 10)); v /= 10; }
    while (m > 0 && (L = ig_strlen(buf)) + 1 < cap) {
        buf[L] = tmp[--m];
        buf[L + 1] = '\0';
    }
}

int pp_run_probe(const PP_ProbeConfig *cfg, const PP_ProbeDeps *deps,
                 PP_ProbeResult *out)
{
    IgBuf rec;
    PP_PadSource source;
    uint8_t subscribed[256];
    uint64_t polls = 0;
    uint64_t keys_fired = 0;
    uint64_t budget = cfg->poll_budget ? cfg->poll_budget : PP_DEFAULT_POLL_BUDGET;
    uint64_t fallback_budget = 3000000; // clock-unavailable bounded mode
    uint32_t window_ms = cfg->window_ms ? cfg->window_ms : PP_DEFAULT_WINDOW_MS;
    uint32_t grace_ms = cfg->grace_ms ? cfg->grace_ms : PP_DEFAULT_GRACE_MS;
    PP_PadPhase last_phase;
    int64_t window_end_ms = 0, first_fire_ms = -1, t0;
    int clock_ok = 0, rc_run = 0;

    t0 = now_mono_ms(&clock_ok);
    const Recorder recorder = {deps->sink, deps->build_id, &out->next_seq, t0};

    out->polls = 0;
    out->keys_fired = 0;
    out->clock_ok = clock_ok;
    out->final_status.phase = PP_PHASE_UNINITIALIZED;
    out->final_status.error = 0;
    out->final_status.user_id = -1;
    out->final_status.handle = -1;
    pp_memzero(&source, sizeof source);
    pp_memzero(subscribed, sizeof subscribed);

    /* probe_start — always first (prepared-module contract). */
    emit(&recorder, &rec, "probe_start", 0, 0);
    ig_kv_str(&rec, "event", "probe_start", 16);
    ig_kv_str(&rec, "binding", "OPTIONS->F7", 16);
    ig_kv_int(&rec, "requested_user", cfg->requested_user);
    ig_kv_int(&rec, "window_ms", window_ms);
    ig_kv_int(&rec, "grace_ms", grace_ms);
    ig_kv_int(&rec, "poll_budget", (int64_t)budget);
    ig_kv_int(&rec, "clock_ok", clock_ok);
    ig_record_flush(&rec);
    if (ig_sink_write_record(deps->sink, &rec) != 0) deps->sink->failed = 1;

    if (!deps->have_backend || !deps->backend.user_initialize ||
        !deps->backend.pad_initialize || !deps->backend.user_list ||
        !deps->backend.open || !deps->backend.read_state ||
        !deps->backend.close || !deps->backend.user_terminate) {
        emit_probe_failed(&recorder, &rec, "factory", "no source factory", 0);
        ig_strlcpy_bounded(out->summary, sizeof out->summary,
                           "{\"event\":\"probe_failed\",\"stage\":\"factory\"", 95);
        return 1;
    }

    /* The one first-hardware binding: OPTIONS rising edge -> Key::F7. */
    {
        const PP_PadBinding bindings[1] = {
            {PS5_PAD_BUTTON_OPTIONS, PP_KEY_F7}};
        pp_source_init(&source, bindings, 1, deps->backend,
                       cfg->requested_user);
    }
    if (!source.valid || !pp_source_activate(&source)) {
        /* Failure detail is captured BEFORE teardown (the prepared module
           records the live status, and its shared_ptr destructor only runs
           after run_probe returns). The port emits, then releases any
           partially-acquired lease exactly where the destructor would. */
        const PP_PadPhase fail_phase = source.status.phase;
        const int fail_error = source.status.error;
        emit_probe_failed(&recorder, &rec, "activate",
                          pp_phase_name(fail_phase), fail_error);
        ig_strlcpy_bounded(out->summary, sizeof out->summary,
                           "{\"event\":\"probe_failed\",\"stage\":\"activate\"", 95);
        pp_source_deactivate(&source);
        out->final_status = source.status;
        return 2;
    }
    emit(&recorder, &rec, "activate", 0, 0);
    ig_kv_str(&rec, "event", "activate", 16);
    ig_kv_str(&rec, "phase", pp_phase_name(source.status.phase), 20);
    ig_kv_int(&rec, "user_id", source.status.user_id);
    ig_kv_int(&rec, "handle", source.status.handle);
    ig_record_flush(&rec);
    if (ig_sink_write_record(deps->sink, &rec) != 0) deps->sink->failed = 1;

    /* Registers ONE keybind (prepared module: register_keydown_event(F7)). */
    subscribed[PP_KEY_F7] = 1;
    if (subscribed[PP_KEY_F7] != 1) {
        emit_probe_failed(&recorder, &rec, "register", "F7 not registered", 0);
        ig_strlcpy_bounded(out->summary, sizeof out->summary,
                           "{\"event\":\"probe_failed\",\"stage\":\"register\"", 95);
        pp_source_deactivate(&source);
        out->final_status = source.status;
        return 3;
    }
    emit(&recorder, &rec, "register", 0, 0);
    ig_kv_str(&rec, "event", "register", 16);
    ig_kv_str(&rec, "key", "F7", 8);
    ig_kv_str(&rec, "button", "OPTIONS", 12);
    ig_kv_int(&rec, "registered", 1);
    ig_record_flush(&rec);
    if (ig_sink_write_record(deps->sink, &rec) != 0) deps->sink->failed = 1;

    last_phase = source.status.phase;
    if (clock_ok) window_end_ms = t0 + (int64_t)window_ms;
    while (polls < budget) {
        size_t n, e;
        ++polls;
        /* One program-tick equivalent: poll the provider through the gate
           inputs (retained Handler::process_event -> PS5PadSource::poll). */
        n = pp_source_poll(&source, subscribed, 1);
        for (e = 0; e < n; ++e) {
            ++keys_fired;
            if (first_fire_ms < 0) first_fire_ms = now_mono_ms(0);
            emit(&recorder, &rec, "key_fired", 0, 0);
            ig_kv_str(&rec, "event", "key_fired", 16);
            ig_kv_str(&rec, "key", "F7", 8);
            ig_kv_str(&rec, "button", "OPTIONS", 12);
            ig_kv_int(&rec, "user_id", source.status.user_id);
            ig_kv_int(&rec, "handle", source.status.handle);
            ig_kv_int(&rec, "polls_so_far", (int64_t)polls);
            ig_record_flush(&rec);
            if (ig_sink_write_record(deps->sink, &rec) != 0)
                deps->sink->failed = 1;
        }
        if (source.status.phase != last_phase) {
            emit(&recorder, &rec, "phase_change", 0, 0);
            ig_kv_str(&rec, "event", "phase_change", 16);
            ig_kv_str(&rec, "from", pp_phase_name(last_phase), 20);
            ig_kv_str(&rec, "to", pp_phase_name(source.status.phase), 20);
            ig_kv_int(&rec, "error", source.status.error);
            ig_record_flush(&rec);
            if (ig_sink_write_record(deps->sink, &rec) != 0)
                deps->sink->failed = 1;
            last_phase = source.status.phase;
        }
        /* Faithful to the prepared module: keep polling through Read errors
           and Disconnected phases (each transition is recorded exactly
           once); the window/grace/budget bounds end the loop. */
        if (clock_ok) {
            int64_t now = now_mono_ms(&clock_ok);
            if (!clock_ok) continue;
            if (keys_fired > 0 && now >= first_fire_ms + (int64_t)grace_ms) {
                break; /* grace_elapsed */
            }
            if (now >= window_end_ms) break; /* window_elapsed */
        } else if (polls >= fallback_budget) {
            break; /* budget_fallback */
        }
    }

    /* probe_end — normal end record (prepared-module contract fields plus
       the end reason and clock health). */
    emit(&recorder, &rec, "probe_end", 0, 0);
    ig_kv_str(&rec, "event", "probe_end", 16);
    ig_kv_int(&rec, "polls", (int64_t)polls);
    ig_kv_int(&rec, "keys_fired", (int64_t)keys_fired);
    ig_kv_str(&rec, "final_phase", pp_phase_name(source.status.phase), 20);
    ig_kv_int(&rec, "user_id", source.status.user_id);
    ig_kv_int(&rec, "handle", source.status.handle);
    ig_kv_int(&rec, "elapsed_ms", now_mono_ms(0) - t0);
    ig_kv_str(&rec, "end_reason",
              !clock_ok ? "budget_fallback"
                        : (keys_fired > 0 ? "grace_elapsed" : "window_elapsed"),
              20);
    ig_record_flush(&rec);
    if (ig_sink_write_record(deps->sink, &rec) != 0) deps->sink->failed = 1;

    /* Teardown after the loop joins (Handler lifecycle contract): close the
       pad handle; terminate the user service ONLY when this probe's
       user_initialize call created the lease (retained deactivate port). */
    if (pp_source_deactivate(&source)) {
        emit(&recorder, &rec, "teardown", 0, 0);
        ig_kv_str(&rec, "event", "teardown", 16);
        ig_kv_str(&rec, "phase", pp_phase_name(source.status.phase), 20);
        ig_kv_int(&rec, "handle", source.status.handle);
        ig_kv_int(&rec, "owned_user_service_released", source.owns_user_service ? 0 : 1);
        ig_record_flush(&rec);
        if (ig_sink_write_record(deps->sink, &rec) != 0)
            deps->sink->failed = 1;
    }

    out->polls = polls;
    out->keys_fired = keys_fired;
    out->final_status = source.status;
    ig_strlcpy_bounded(out->summary, sizeof out->summary,
                       "{\"event\":\"probe_end\",\"keys_fired\":", 95);
    summary_append_u64(out->summary, sizeof out->summary, keys_fired);
    return rc_run;
}
