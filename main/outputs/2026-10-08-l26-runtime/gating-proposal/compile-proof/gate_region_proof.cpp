// Host-compile + behavior proof for the l26 copyin-initializer gating proposal.
// Agent Z15, 2026-10-08. This is a PROPOSAL packet: no build consumes this file.
//
// PROVENANCE OF EVERY PIECE (verbatim vs stand-in):
//   VERBATIM  compile-proof/CopyinStartup.hpp
//             — l13_sink::InitializerOwner, byte-identical copy of the gate
//             class the bridge TU includes
//             (outputs/2026-10-08-l24-package/lineage/CopyinStartup.hpp,
//             sha256 b300cb59f41c7b552c66672439521285edbd6b0c1cbbb5ac04065cb114af7900;
//             identical to outputs/2026-10-07-l19-r3-full-build/units/CopyinStartup.hpp).
//   VERBATIM  compile-proof/extracted_gate_functions.inc
//             — bridge_copyin_initializer_enter / (PATCHED)
//             bridge_copyin_initializer_complete / bridge_copyin_initializer_leave,
//             sed-extracted lines 1071-1149 of the patched primary candidate
//             (gating-patch.diff applied to copyin-probe candidate f510c014...).
//             The gating lines are exactly:
//               ::diagnostic_event("copyin_probe_gate_l26_early_release", 1u);
//               g_copyin_initializer_owner.leave();
//   VERBATIM  probe_gate_check() below — the first check of
//             bridge_kernel_write_copyin_probe_until ("if (!g_copyin_initializer_
//             owner.allows()) return -1;"); only its tail is a stand-in.
//   STAND-IN  diagnostic_event, bridge_copyin_directory_release, and the
//             statics the state note reads (g_copyin_handles / parent_fd /
//             probe counters). Shapes match the bridge TU's declarations
//             (diagnostic.h:4, bridge :980-1061 region); behavior is counted,
//             not replicated. The real bridge_copyin_directory_release is
//             idempotent (std::exchange(fd,-1) loop is a no-op once retired),
//             which is the property the idempotence assertions rely on.
//
// What this proves:
//   1. The patched region COMPILES standalone (host clang++, C++20).
//   2. THE GATING PROPERTY: after complete(), the owner gate is disarmed —
//      the init thread itself now fails the probe gate check (-1).
//   3. IDEMPOTENCE (task requirement): a second complete() AND the shutdown-
//      time bridge_copyin_initializer_leave() are both no-ops afterwards.
//   4. The l25 observation events still fire with their pre-gating values
//      (state=3, gate_armed=1, bank_retired=1) BEFORE the gate closes.
//   5. Foreign threads were never allowed and stay refused.
//   6. Re-arming remains possible by EXPLICIT enter() (the l26+ "future
//      feature must re-arm explicitly" property), and the unpatched shutdown
//      leave() path still works on a re-armed gate.

#include "CopyinStartup.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

// ---- STAND-IN: diagnostic event sink (counts tags, remembers last value) ----
std::unordered_map<std::string, int> g_event_count;
std::unordered_map<std::string, std::uint32_t> g_event_last;
extern "C" void diagnostic_event(const char *tag, std::uint32_t value)
{
    g_event_count[tag] += 1;
    g_event_last[tag] = value;
}
static int events(const char *tag) { return g_event_count[tag]; }

// ---- STAND-IN statics referenced by the verbatim extracted functions ----
static l13_sink::InitializerOwner g_copyin_initializer_owner;
struct CopyinHandleStub { int fd = -1; const char *path = ""; };
static CopyinHandleStub g_copyin_handles[64];   // retired state: every fd < 0
static int g_copyin_parent_fd = -1;             // retired state: closed
static std::uint32_t g_copyin_probe_nonce = 0;
static std::uint32_t g_copyin_probe_attempts = 0;
static std::uint32_t g_copyin_probe_attempts_late = 0;
static bool g_copyin_retirement_logged = false;
static int g_release_calls = 0;
static void bridge_copyin_directory_release()   // STAND-IN: counts, no fds
{
    ++g_release_calls;
}

// ---- VERBATIM patched region (see banner) ----
#include "extracted_gate_functions.inc"

// ---- VERBATIM first check of bridge_kernel_write_copyin_probe_until ----
static int probe_gate_check(void)
{
    if (!g_copyin_initializer_owner.allows()) return -1;
    return 0; // STAND-IN tail: proof extracts only the owner-gate check
}

static int g_failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++g_failures; } \
} while (0)

int main()
{
    // A. Pre-admission: gate closed for EVERYONE (fail-closed baseline).
    CHECK(probe_gate_check() == -1);

    // B. enter() arms the gate for the init (main) thread only.
    CHECK(bridge_copyin_initializer_enter() == 1);
    CHECK(probe_gate_check() == 0);                    // owner allowed
    {
        std::thread foreign([] {
            if (probe_gate_check() != -1) std::_Exit(42); // foreign never allowed
        });
        foreign.join();
    }
    CHECK(g_release_calls == 0);                       // nothing released yet

    // D. complete(): l25 observation fires with PRE-gating truth, then the
    //    l26 gating lines run (event + leave), bank released exactly once.
    bridge_copyin_initializer_complete();
    CHECK(g_release_calls == 1);
    CHECK(events("copyin_probe_state") == 1);
    CHECK(g_event_last["copyin_probe_state"] == 3u);   // armed | retired
    CHECK(g_event_last["copyin_probe_gate_armed"] == 1u);
    CHECK(g_event_last["copyin_probe_bank_retired"] == 1u);
    CHECK(g_event_last["copyin_probe_sinks_open"] == 0u);
    CHECK(g_event_last["copyin_probe_parent_fd_open"] == 0u);
    CHECK(events("copyin_probe_gate_l26_early_release") == 1);   // THE new event
    CHECK(g_event_last["copyin_probe_gate_l26_early_release"] == 1u);

    // E. THE GATING PROPERTY: even the init thread now fails the gate check.
    CHECK(probe_gate_check() == -1);

    // F. IDEMPOTENCE: second complete() is a no-op (allows() guard returns).
    bridge_copyin_initializer_complete();
    CHECK(g_release_calls == 1);
    CHECK(events("copyin_probe_state") == 1);          // no duplicate state note
    CHECK(events("copyin_probe_gate_l26_early_release") == 1);

    // F2. IDEMPOTENCE: the shutdown fallback (CopyinOwnerScope destructor
    //     calls bridge_copyin_initializer_leave() at initialize() return) is
    //     a no-op in a gated session: no second release, no shutdown events.
    bridge_copyin_initializer_leave();
    bridge_copyin_initializer_leave();                 // and again, still quiet
    CHECK(g_release_calls == 1);
    CHECK(events("copyin_probe_gate_released") == 0);  // shutdown proof silent
    CHECK(events("copyin_probe_attempts_late_total") == 0);

    // G. Foreign threads stay refused after gating.
    {
        std::thread foreign([] {
            if (probe_gate_check() != -1) std::_Exit(43);
        });
        foreign.join();
    }

    // H. Explicit re-arm still works (next controller decision can re-arm;
    //    a new process does this implicitly via module_start -> enter()).
    CHECK(bridge_copyin_initializer_enter() == 1);
    CHECK(probe_gate_check() == 0);
    // H2. The UNPATCHED shutdown path (leave while armed) still functions.
    bridge_copyin_initializer_leave();
    CHECK(g_release_calls == 2);
    CHECK(events("copyin_probe_gate_released") == 1);
    CHECK(events("copyin_probe_attempts_late_total") == 1);
    // H3. leave() is idempotent too (second call no-op).
    bridge_copyin_initializer_leave();
    CHECK(g_release_calls == 2);
    CHECK(events("copyin_probe_gate_released") == 1);

    if (g_failures == 0) {
        std::printf("ALL CHECKS PASSED: gating property holds, release idempotent\n");
        return 0;
    }
    std::printf("%d FAILURES\n", g_failures);
    return 1;
}
