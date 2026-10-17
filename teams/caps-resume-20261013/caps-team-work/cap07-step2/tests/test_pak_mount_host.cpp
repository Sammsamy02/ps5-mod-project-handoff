// test_pak_mount_host.cpp - host unit tests for the generic data-only mount unit.
// Compiles the real PATCH/RuntimePakMount.cpp against a mock engine. No console.
#include "mock_engine.hpp"
#include "../PATCH/RuntimePakMount.hpp"
#include <cstdio>
#include <string>

#define CPP_PINS_INC "../PATCH/pak_mount_pins.inc"
#include <cstdio>
namespace { int g_checks = 0, g_fail = 0; }
#define CHECK(cond, name) do { ++g_checks; \
    if (cond) std::printf("  [PASS] %s\n", name); \
    else { ++g_fail; std::printf("  [FAIL] %s\n", name); } } while (0)

using namespace ModBridge::PakMount;

// pull the generated pin table in (exercises the generated .inc too)
#include <cstring>
#include "../PATCH/pak_mount_pins.inc"

static EnginePins good_pins()
{
    // Use the generated table; rebased expectations in the mock match BASE+off.
    return modbridge_pak_pins_ue561();
}

static MountRequest req()
{
    MountRequest r;
    r.pak_path_abs = "/data/modbridge/roots/PPSA33286/zz_mod_P.pak"; // supplied as DATA
    r.order = -1;
    return r;
}

int main()
{
    std::printf("cap07-step2 host mount tests\n");

    // 1. Happy path: pins ok, delegate bound, pak layer present, engine accepts.
    {
        mock::FakeImage img; img.wire();
        mock::MockBridge eng(img);
        auto o = mount(good_pins(), req(), eng);
        CHECK(o.verdict == Verdict::Mounted, "happy path mounts");
        CHECK(o.count_after == o.count_before + 1, "mounted count grew by one");
        CHECK(eng.execute_calls == 1, "engine Execute called exactly once");
        CHECK(eng.last_path == req().pak_path_abs, "path passed through verbatim (data-only)");
        CHECK(eng.fstrings_live == 0, "FString freed after the call (no leak)");
        CHECK(o.pak_layer == img.pak_layer, "census found the pak layer");
    }

    // 2. Census: vptr identity predicate.
    {
        CHECK(is_pak_layer(0x1234, 0x1234), "is_pak_layer accepts exact vptr match");
        CHECK(!is_pak_layer(0x1234, 0x9999), "is_pak_layer rejects mismatch");
        CHECK(!is_pak_layer(0, 0x1234), "is_pak_layer rejects null vptr");
    }

    // 3. Fail-closed: image sha mismatch -> refuse, no engine call.
    {
        mock::FakeImage img; img.wire(); img.sha = "deadbeef";
        mock::MockBridge eng(img);
        auto o = mount(good_pins(), req(), eng);
        CHECK(o.verdict == Verdict::RefusedPins, "sha mismatch refuses");
        CHECK(eng.execute_calls == 0, "no engine call on sha mismatch");
    }

    // 4. Fail-closed: P4 signature mismatch -> refuse before census/call.
    {
        mock::FakeImage img; img.wire();
        mock::MockBridge eng(img); eng.sig_ok = false;
        auto o = mount(good_pins(), req(), eng);
        CHECK(o.verdict == Verdict::RefusedPins, "signature mismatch refuses");
        CHECK(eng.execute_calls == 0, "no engine call on signature mismatch");
    }

    // 5. Delegate unbound (engine not ready) -> RefusedUnbound, no call.
    {
        mock::FakeImage img; img.wire(/*bind*/ false);
        mock::MockBridge eng(img);
        auto o = mount(good_pins(), req(), eng);
        CHECK(o.verdict == Verdict::RefusedUnbound, "unbound delegate refuses");
        CHECK(eng.execute_calls == 0, "no engine call when delegate unbound");
    }

    // 6. No FPakPlatformFile in chain -> RefusedNoPakLayer.
    {
        mock::FakeImage img; img.wire(/*bind*/ true, /*pak_present*/ false);
        mock::MockBridge eng(img);
        auto o = mount(good_pins(), req(), eng);
        CHECK(o.verdict == Verdict::RefusedNoPakLayer, "missing pak layer refuses");
        CHECK(eng.execute_calls == 0, "no engine call without a pak layer");
    }

    // 7. Not on game thread -> refuse, no census, no call.
    {
        mock::FakeImage img; img.wire();
        mock::MockBridge eng(img); eng.game_thread = false;
        auto o = mount(good_pins(), req(), eng);
        CHECK(o.verdict == Verdict::RefusedNotGameThread, "off game-thread refuses");
        CHECK(eng.execute_calls == 0, "no engine call off game thread");
    }

    // 8. Engine returns null / count flat -> RefusedCallFailed (vanilla), fstring freed.
    {
        mock::FakeImage img; img.wire();
        mock::MockBridge eng(img); eng.engine_accepts = false;
        auto o = mount(good_pins(), req(), eng);
        CHECK(o.verdict == Verdict::MountedUnverified, "engine null return leaves side effects unverified");
        CHECK(eng.fstrings_live == 0, "FString freed even on failed mount");
    }

    // 9. Empty request -> RefusedBadRequest.
    {
        mock::FakeImage img; img.wire();
        mock::MockBridge eng(img);
        MountRequest empty; empty.pak_path_abs.clear();
        auto o = mount(good_pins(), empty, eng);
        CHECK(o.verdict == Verdict::RefusedBadRequest, "empty path refuses");
    }

    // 10. Incomplete pins -> RefusedPins.
    {
        mock::FakeImage img; img.wire();
        mock::MockBridge eng(img);
        EnginePins p = good_pins(); p.mountpak_delegate = 0;
        auto o = mount(p, req(), eng);
        CHECK(o.verdict == Verdict::RefusedPins, "incomplete pins refuse");
    }

    std::printf("\nchecked %d assertions; %d PASS, %d FAIL\n",
                g_checks, g_checks - g_fail, g_fail);
    if (g_checks == 0) { std::printf("GATE FAIL: no assertions ran\n"); return 2; }
    if (g_fail) { std::printf("GATE FAIL\n"); return 1; }
    std::printf("GATE PASS\n");
    return 0;
}
