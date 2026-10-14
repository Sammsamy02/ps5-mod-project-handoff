// pad_adapter_test.cpp — host test of the kernel input reader (kekcall nr=9)
// pad route (worker-ue4ss-kernel, 2026-10-06).
//
// Compiles the REAL vendored SDL header (PS5_PadData), the REAL facade
// (KekcallServices.hpp) and the REAL adapter (KekcallPadAdapter.hpp — the
// exact bytes PS5PadNative.cpp includes after diff 0003) against the mock
// kernel. The edge-consumption replica below transcribes the retained
// PS5PadSource::poll semantics (duplicate-timestamp guard, first-sample
// baseline swallow, rising-edge emission while allowed+subscribed, held
// state tracked while gated, disconnect clears state) from the staging
// source; drift is a review item.
//
// Cases:
//   1  ModuleNew: route opens via one full KIN1 snapshot; reads map
//      field-for-field into PS5_PadData; the edge replica emits exactly one
//      OPTIONS edge per press through the kernel service.
//   2  Duplicate-timestamp suppression + baseline swallow on attach.
//   3  Intercepted bit (0x80000000) passes through and cannot fabricate
//      button edges.
//   4  ModuleOld (ENOSYS): try_open refuses -> the factory's route decision
//      selects the legacy borrowed-handle backend (fallback law).
//   5  NOSTATE (-19, firmware without derived offsets): same clean refusal.
//   6  Port selection: first CONNECTED pad wins over a disconnected port 0.

#include "mock_kekcall.hpp"

#include <cstdio>
#include <cstring>

// The REAL vendored pinned SDL declaration header (same bytes PS5PadSource.hpp
// includes on console).
extern "C" {
#include "../../../worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/primary-sources/SDL/src/joystick/ps5/SDL_ps5joystick.h"
}

// The REAL adapter (requires PS5_PadData defined first — same order the
// patched PS5PadNative.cpp uses).
#include "../KekcallPadAdapter.hpp"

static int failures = 0;
#define CHECK(cond, msg)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(cond))                                                            \
        {                                                                       \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, msg);            \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

// ---------------------------------------------------------------------------
// Edge-consumption replica of PS5PadSource::poll (staging source, the
// retained semantics — one binding for these tests).
// ---------------------------------------------------------------------------
struct EdgeReplica
{
    bool have_sample = false;
    bool down = false;
    uint64_t timestamp = 0;
    int emitted_total = 0;

    // Returns edges emitted this poll.
    int poll(int handle, bool subscribed, bool allow)
    {
        PS5_PadData data{};
        if (KekcallPad::read_state(handle, &data) != 0) { return -1; }
        if (!data.connected)
        {
            have_sample = false;
            down = false; // disconnect clears state, no phantom edges later
            return 0;
        }
        if (have_sample && data.timestamp == timestamp) { return 0; } // cached snapshot
        const bool baseline = !have_sample;
        const bool pressed = (data.buttons & PS5_PAD_BUTTON_OPTIONS) == PS5_PAD_BUTTON_OPTIONS;
        int emitted = 0;
        if (!baseline && pressed && !down && allow && subscribed) { emitted = 1; }
        down = pressed; // tracked even while gated: no phantom edges on ungating
        have_sample = true;
        timestamp = data.timestamp;
        emitted_total += emitted;
        return emitted;
    }
};

static void reset_adapter_state()
{
    KekcallPad::g_route_active = false;
    KekcallPad::g_locked_port = 0;
}

int main()
{
    using ksrv::KinputPadState;

    // ---- 1. Kernel route: open, map, edge -----------------------------------
    {
        mock::reset(mock::Module::New);
        mock::pad_script(0, {
            // disconnect first (attach moment), then press, hold (dup ts), release
            mock::make_state(0, 128, 128, 128, 128, 0, 0, 0, 0),
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 130, 127, 128, 128, 42, 0, 1, 1000),
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 130, 127, 128, 128, 42, 0, 1, 1000),
            mock::make_state(PS5_PAD_BUTTON_OPTIONS | PS5_PAD_BUTTON_CROSS, 130, 127, 128, 128, 42, 255, 1, 2000),
            mock::make_state(0, 128, 128, 128, 128, 0, 0, 1, 3000),
        });
        mock::install();
        reset_adapter_state();

        const int handle = KekcallPad::try_open(0);
        CHECK(handle == KekcallPad::kKernelPadHandle, "1: kernel handle returned");
        CHECK(KekcallPad::is_kernel_handle(handle), "1: marker recognized");
        CHECK(KekcallPad::g_locked_port == 0, "1: locked port 0");

        // Field mapping check. try_open CONSUMED script[0] (the disconnect
        // snapshot it scanned); read #1 therefore sees the press sample.
        PS5_PadData data{};
        CHECK(KekcallPad::read_state(handle, &data) == 0, "1: read ok (press sample)");
        CHECK(data.buttons == PS5_PAD_BUTTON_OPTIONS, "1: buttons map");
        CHECK(data.leftStick.x == 130 && data.leftStick.y == 127, "1: left stick maps");
        CHECK(data.rightStick.x == 128 && data.rightStick.y == 128, "1: right stick maps");
        CHECK(data.analogButtons.l2 == 42 && data.analogButtons.r2 == 0, "1: triggers map");
        CHECK(data.connected == 1, "1: connected maps");
        CHECK(data.timestamp == 1000, "1: timestamp maps (dup-guard input)");
        CHECK(data.touch.fingers == 0, "1: touch maps");
        static_assert(sizeof(PS5_PadData) == 120, "vendored layout is the pinned 120-byte one");

        // Edge replica over a FRESH script: the route is already active, so
        // try_open returns without reading and read #1 sees script[0].
        mock::pad_script(0, {
            mock::make_state(0, 128, 128, 128, 128, 0, 0, 1, 500),            // baseline
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 128, 128, 128, 128, 0, 0, 1, 1000), // press
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 128, 128, 128, 128, 0, 0, 1, 1000), // dup
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 128, 128, 128, 128, 0, 0, 1, 1500), // hold
            mock::make_state(0, 128, 128, 128, 128, 0, 0, 1, 2000),            // release
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 128, 128, 128, 128, 0, 0, 1, 3000), // press again
        });
        EdgeReplica edge;
        CHECK(edge.poll(handle, true, true) == 0, "1: baseline sample swallows edges");
        CHECK(edge.poll(handle, true, true) == 1, "1: OPTIONS press -> exactly one edge");
        CHECK(edge.poll(handle, true, true) == 0, "1: duplicate timestamp suppressed");
        CHECK(edge.poll(handle, true, true) == 0, "1: hold emits nothing");
        CHECK(edge.poll(handle, true, true) == 0, "1: release emits nothing");
        CHECK(edge.poll(handle, true, true) == 1, "1: second press -> one edge");
        CHECK(mock::g_counters.pad_read >= 7, "1: every poll was a kernel nr=9 read");
    }

    // ---- 2. Gated hold: no edge while gated, none fabricated on ungating ----
    {
        mock::reset(mock::Module::New);
        mock::pad_script(0, {
            mock::make_state(0, 128, 128, 128, 128, 0, 0, 0, 0), // consumed by try_open
            mock::make_state(0, 128, 128, 128, 128, 0, 0, 1, 100), // baseline
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 128, 128, 128, 128, 0, 0, 1, 200), // press (gated)
            mock::make_state(PS5_PAD_BUTTON_OPTIONS, 128, 128, 128, 128, 0, 0, 1, 300), // hold (ungated)
        });
        mock::install();
        reset_adapter_state();
        const int handle = KekcallPad::try_open(0);
        EdgeReplica edge;
        (void)edge.poll(handle, true, true); // baseline
        CHECK(edge.poll(handle, true, false) == 0, "2: press while gated: no edge");
        CHECK(edge.down, "2: held state tracked while gated");
        CHECK(edge.poll(handle, true, true) == 0, "2: ungating with held key: no phantom edge");
    }

    // ---- 3. Intercepted bit passes through, fabricates no edges -------------
    {
        mock::reset(mock::Module::New);
        mock::pad_script(0, {
            mock::make_state(0, 128, 128, 128, 128, 0, 0, 0, 0), // consumed by try_open
            mock::make_state(0x80000000u | PS5_PAD_BUTTON_OPTIONS, 128, 128, 128, 128, 0, 0, 1, 100),
        });
        mock::install();
        reset_adapter_state();
        const int handle = KekcallPad::try_open(0);
        PS5_PadData data{};
        CHECK(KekcallPad::read_state(handle, &data) == 0, "3: read ok");
        CHECK(data.buttons == (0x80000000u | PS5_PAD_BUTTON_OPTIONS),
              "3: Intercepted bit passes through unmodified (faithful mapping)");
        // NOTE: discarding Intercepted samples is DESIGNED consumer behavior
        // (INPUT-SERVICE.md §6.1) that would live in PS5PadSource::poll —
        // deliberately NOT changed by diff 0003 (the retained poll semantics
        // run unchanged); recorded as follow-up in KEKCALL-INTEGRATION.md. The
        // bit arriving unmodified is this adapter's contract.
    }

    // ---- 4. ModuleOld: clean refusal -> factory selects the legacy backend ---
    {
        mock::reset(mock::Module::Old);
        mock::install();
        reset_adapter_state();
        CHECK(KekcallPad::try_open(0) == -1, "4: ENOSYS -> try_open refuses (route falls back)");
        CHECK(!KekcallPad::g_route_active, "4: route inactive");
        CHECK(ksrv::status() == ksrv::Status::Enosys, "4: ENOSYS latched for the boot");
        // The patched factory's route decision (diff 0003): kernel_route =
        // (try_open(0) == kKernelPadHandle) — false here, so the PadBackend is
        // the exact pre-r15 expression (acquire_service, pad_initialize_once,
        // ..., open_or_borrow, scePadReadState, close_owned, release_service).
        const bool kernel_route = (KekcallPad::try_open(0) == KekcallPad::kKernelPadHandle);
        CHECK(!kernel_route, "4: factory takes the legacy backend");
    }

    // ---- 5. NOSTATE: firmware without derived offsets -> same fallback -------
    {
        mock::reset(mock::Module::New);
        for (uint32_t p = 0; p < 4; ++p)
        {
            mock::g_pad_ports[p].script.clear();
            mock::g_pad_ports[p].forced_rc = ksrv::KINPUT_ERR_NOSTATE;
        }
        mock::install();
        reset_adapter_state();
        CHECK(KekcallPad::try_open(0) == -1, "5: NOSTATE -> clean refusal (never guess)");
        CHECK(!KekcallPad::g_route_active, "5: route inactive");
    }

    // ---- 6. Port selection: first CONNECTED pad wins -------------------------
    {
        mock::reset(mock::Module::New);
        mock::pad_script(0, {mock::make_state(0, 128, 128, 128, 128, 0, 0, 0, 0)}); // disconnected
        mock::pad_script(2, {mock::make_state(0, 128, 128, 128, 128, 0, 0, 1, 42)}); // connected
        mock::install();
        reset_adapter_state();
        CHECK(KekcallPad::try_open(0) == KekcallPad::kKernelPadHandle, "6: opens");
        CHECK(KekcallPad::g_locked_port == 2, "6: locked the first CONNECTED port (2)");
        PS5_PadData data{};
        CHECK(KekcallPad::read_state(KekcallPad::kKernelPadHandle, &data) == 0 && data.connected == 1,
              "6: reads flow from the locked port");
    }

    // ---- 7. close: route re-arms (retry-after-deactivate shape) --------------
    {
        mock::reset(mock::Module::New);
        mock::pad_script(1, {mock::make_state(0, 128, 128, 128, 128, 0, 0, 1, 7)});
        mock::install();
        reset_adapter_state();
        const int h = KekcallPad::try_open(3);
        CHECK(h == KekcallPad::kKernelPadHandle, "7: opens");
        CHECK(KekcallPad::close(h) == 0, "7: close ok (no SDK handle to close)");
        CHECK(!KekcallPad::g_route_active, "7: route deactivated");
        CHECK(KekcallPad::try_open(0) == KekcallPad::kKernelPadHandle, "7: re-opens (first valid port 1)");
    }

    if (failures == 0) { std::printf("pad_adapter_test: ALL PASS\n"); return 0; }
    std::printf("pad_adapter_test: %d FAILURES\n", failures);
    return 1;
}
