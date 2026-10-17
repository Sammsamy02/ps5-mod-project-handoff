// pad_probe_core.h — C port of the standalone probe runner for the pad
// probe PRX (worker-pad-hardware-probe, team 2026-10-04). Derived-origin:
// outputs/2026-10-04-zcode-team/worker-real-input-pad/pad_probe/PadProbe.{hpp,cpp}
// (PREPARED module, read-only) over the retained provider port in
// pad_source.h. Record vocabulary and ordering preserved exactly:
//   probe_start  — always first (binding, requested_user, window params)
//   probe_failed — factory/activate/register/run failure; final record
//   key_fired    — one per dispatch-gate delivery of the F7 key id
//   phase_change — provider PadPhase transitions while polling
//   probe_end    — normal end (polls, keys_fired, final status, end_reason)
// One hardcoded first-hardware binding: pad OPTIONS -> F7 (the UE4SS Key id
// 118 == Win32 VK_F7; the standalone probe has no UE4SS listening — the id
// is the dispatch-gate subscription label). The ini path is deliberately
// NOT exercised here (prepared-module contract; it belongs to the composed
// runtime after this probe passes).
//
// The single structural extension vs the prepared module: the poll window
// is WALL-CLOCK bounded (clock_gettime CLOCK_MONOTONIC — a verbatim frozen
// import) with a post-first-fire grace, because module_start runs
// synchronously and the human must have time to press OPTIONS. The
// iteration budget stays as the bounded backstop (prepared contract).
#ifndef PAD_PROBE_CORE_H
#define PAD_PROBE_CORE_H

#include "pad_source.h"
#include "record_sink.h"

#include <stdint.h>

#define PP_KEY_F7 118  // UE4SS Key::F7 == Win32 VK_F7 == 0x76

typedef struct PP_ProbeConfig {
    uint64_t poll_budget;  // 0 -> DEFAULT_POLL_BUDGET (hard backstop)
    uint32_t window_ms;    // 0 -> DEFAULT_WINDOW_MS (wall-clock window)
    uint32_t grace_ms;     // 0 -> DEFAULT_GRACE_MS (post-first-fire)
    int requested_user;    // -1 = first logged-in user (matches ini default)
} PP_ProbeConfig;

typedef struct PP_ProbeDeps {
    PP_PadBackend backend;  // console: the real scePad*/sceUserService* table
    int have_backend;       // 0 -> probe_failed stage=factory (null source)
    IgSink *sink;           // not owned; must outlive the run
    const char *build_id;   // envelope build id (generated config)
} PP_ProbeDeps;

typedef struct PP_ProbeResult {
    uint64_t polls;
    uint64_t keys_fired;
    int next_seq;          // record sequence after the run
    int clock_ok;          // 1 iff the first CLOCK_MONOTONIC read succeeded
    PP_PadStatus final_status;
    char summary[96];      // final record body convenience (NUL-terminated)
} PP_ProbeResult;

#define PP_DEFAULT_POLL_BUDGET 200000000ull
#define PP_DEFAULT_WINDOW_MS   30000u
#define PP_DEFAULT_GRACE_MS    5000u

// Return codes: 0 = ran to probe_end (keys_fired MAY still be 0);
// 1 = probe_failed stage=factory; 2 = stage=activate; 3 = stage=register;
// 4 = stage=run (exception port). Never writes outside the sink.
int pp_run_probe(const PP_ProbeConfig *cfg, const PP_ProbeDeps *deps,
                 PP_ProbeResult *out);

#endif  // PAD_PROBE_CORE_H
