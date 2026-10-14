// KekcallPadAdapter.hpp — kernel input reader (kekcall nr=9) adapter for the
// PS5 pad provider, staged by worker-ue4ss-kernel (2026-10-06).
// Host-compiled and host-tested (host-test/pad_adapter_test.cpp compiles
// THESE bytes against the vendored SDL header). No console contact, no game
// claims.
//
// Scope (assignment item 2c): replaces the BORROWED PAD HANDLE route
// (scePadGetHandle on a helper thread, PS5PadNative.cpp rev 2/3) with the
// kernel input reader when the rebuilt kstuff module (nr=9) is resident:
//   - no scePadOpen / scePadGetHandle / handle exclusivity at all (the
//     ALREADY_OPENED failure class does not exist on this route —
//     worker-kernel-input INPUT-SERVICE.md §6.1);
//   - no PadServiceLease / sceUserService acquire (the user-service half is
//     naturally inert in-process: the game already initialized it);
//   - FALLBACK (assignment item 4): ENOSYS (-78, old module), NOSTATE (-19,
//     firmware without derived offsets) or any first-read refusal leaves the
//     borrowed-handle route byte-for-byte intact — this adapter only ever
//     returns kKernelPadHandle after one FULL successful KIN1 snapshot.
//
// Requirements at inclusion: KekcallServices.hpp included AND PS5_PadData
// defined (vendored SDL_ps5joystick.h via PS5PadSource.hpp). The mapping is
// pure and unit-tested; the routing half is the exact code PS5PadNative.cpp
// calls (open_route / read_route / close_route below).

#pragma once

#include "KekcallServices.hpp"
#include <cstdint>

namespace RC::Input::KekcallPad
{
    // Synthetic handle value: positive (PS5PadSource::is_available needs
    // handle >= 0), ASCII 'KIN1', and far outside the SDK's small handle
    // domain; scePadOpen/scePadGetHandle can never mint it.
    constexpr int kKernelPadHandle = 0x4B494E31;

    inline bool is_kernel_handle(int handle) { return handle == kKernelPadHandle; }

    // ---- pure mapping: KIN1 snapshot -> PS5_PadData -------------------------
    // Field-for-field from INPUT-SERVICE.md §6.1: fill a PS5_PadData-shaped
    // local from kinput_pad_state; connected==0 or the Intercepted bit pass
    // through unchanged (the retained PS5PadSource::poll edge semantics
    // consume them exactly as they consume SDK samples). quat/vel/acell have
    // no KIN1 source (v1) and stay zero — no runtime binding consumes them.
    // Touch coordinates are u16 in BOTH structs (KIN1 touch_x/touch_y,
    // PS5_PadTouch x/y): the mapping is lossless.
    inline void map_state(const ksrv::KinputPadState &in, PS5_PadData &out)
    {
        out.buttons = in.buttons;           // 0x80000000 Intercepted passes through
        out.leftStick.x = in.lx;
        out.leftStick.y = in.ly;
        out.rightStick.x = in.rx;
        out.rightStick.y = in.ry;
        out.analogButtons.l2 = in.l2;
        out.analogButtons.r2 = in.r2;
        out.connected = in.connected;
        out.timestamp = in.timestamp_us;    // duplicate-timestamp guard input
        out.touch.fingers = in.touch_fingers;
        out.touch.touch[0].x = in.touch_x[0];
        out.touch.touch[0].y = in.touch_y[0];
        out.touch.touch[0].finger = in.touch_id[0];
        out.touch.touch[1].x = in.touch_x[1];
        out.touch.touch[1].y = in.touch_y[1];
        out.touch.touch[1].finger = in.touch_id[1];
    }

    // ---- routing half (the exact functions PS5PadNative.cpp calls) ----------

    // Port lock state (sole processing thread per the PadBackend lifecycle
    // contract; plain statics suffice under that serialization).
    inline uint32_t g_locked_port = 0;
    inline bool g_route_active = false;

    // One-time route bring-up. Returns:
    //   kKernelPadHandle — route ACTIVE (one full KIN1 snapshot proven)
    //   -1               — kernel route unavailable/refused: caller falls
    //                      back to the borrowed-handle route unchanged.
    // Port selection mirrors the native provider's first-connected-user
    // selection (INPUT-SERVICE.md §6.1): scan ports 0..3, lock the first
    // connected pad; if none reports connected, lock port 0 and let the
    // first successful read carry it (a not-yet-connected pad is a normal
    // boot state, not a refusal).
    inline int try_open(int requested_port)
    {
        if (g_route_active) { return kKernelPadHandle; }
        if (!ksrv::maybe_available()) { return -1; }
        ksrv::KinputPadState st{};
        uint32_t port = requested_port <= 3 ? static_cast<uint32_t>(requested_port) : 0;
        uint32_t first_valid = 0xFFFFFFFFu;
        bool locked = false;
        for (uint32_t p = 0; p < 4; ++p)
        {
            const int32_t rc = ksrv::pad_read(p, &st);
            if (rc >= static_cast<int32_t>(sizeof(ksrv::KinputPadState)) &&
                st.magic == ksrv::KINPUT_MAGIC_KIN1)
            {
                if (st.connected != 0)
                {
                    port = p; // first CONNECTED pad wins (native-provider selection mirror)
                    locked = true;
                    break;
                }
                if (first_valid == 0xFFFFFFFFu) { first_valid = p; }
            }
            else if (rc == ksrv::KINPUT_ERR_NOSTATE || rc == ksrv::KSR_ERR_ENOSYS ||
                     rc == ksrv::KSR_ERR_NOCHANNEL)
            {
                return -1; // clean refusal: firmware without offsets / old module / no channel
            }
            else if (rc == ksrv::KSR_ERR_STRUCT)
            {
                return -1; // locator broken this boot: refuse the route, fall back
            }
            // BADARGS/PORT/TORN-class on one port: keep scanning the rest
        }
        if (!locked)
        {
            if (first_valid != 0xFFFFFFFFu) { port = first_valid; } // first responsive pad
            // else: no port answered with a snapshot — lock port 0 and let
            // the first read_state carry the verdict (a not-yet-connected
            // pad is a normal boot state, not a route refusal).
        }
        g_locked_port = port;
        g_route_active = true;
        return kKernelPadHandle;
    }

    // read_state replacement. Returns 0 and fills `out` on success (SDK
    // convention), or the negative KINPUT/KSR error (the caller's
    // PadPhase::Read failure path consumes it exactly like an SDK error).
    inline int read_state(int /*handle*/, PS5_PadData *out)
    {
        if (out == nullptr) { return ksrv::KINPUT_ERR_BADARGS; }
        ksrv::KinputPadState st{};
        const int32_t rc = ksrv::pad_read(g_locked_port, &st);
        if (rc < 0) { return rc; }
        if (st.magic != ksrv::KINPUT_MAGIC_KIN1) { return ksrv::KINPUT_ERR_STRUCT; }
        map_state(st, *out);
        return 0;
    }

    // close replacement: the kernel route holds no handle; nothing to close.
    inline int close(int /*handle*/)
    {
        g_route_active = false;
        return 0;
    }
}
