// Host tests for the standalone pad probe (worker-real-input-pad, 2026-10-04).
// Link the RETAINED PS5PadSource.cpp + Handler.cpp read-only (the same objects
// the composed runtime links) and drive the provider through its explicit
// owned-test seam, PadBackend (PS5PadSource.hpp:14-15). Console ordering
// (init before register) is mirrored because __PROSPERO__ flips
// Handler::m_source_injected_mode to true — the probe code does that already;
// these tests verify the behavior end to end.
#include "PadProbe.hpp"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using RC::Input::PadBackend;
using RC::Input::PadBinding;
using RC::Input::PadPhase;
using RC::Input::PS5PadSource;
using modbridge::padprobe::Config;
using modbridge::padprobe::Dependencies;
using modbridge::padprobe::RecordSink;

struct Sample
{
    uint32_t buttons{};
    bool connected{true};
    uint64_t timestamp{};
};

// Scripted hardware: user service ok, pad open returns handle 7 (or -1 when
// fail_open), read_state replays queued samples then repeats the last one
// (mirroring cached scePadReadState).
struct ScriptedPad
{
    std::vector<Sample> samples;
    size_t cursor{};
    int open_result{7};
    bool user_list_ok{true};

    static int user_initialize(void*) { return 0; }
    static int pad_initialize() { return 0; }
    static int user_list(int users[4])
    {
        users[0] = 1;
        users[1] = users[2] = users[3] = -1;
        return script->user_list_ok ? 0 : -1;
    }
    static int open(int, int, int, void*) { return script->open_result; }
    static int read_state(int, PS5_PadData* data)
    {
        if (script->cursor < script->samples.size()) ++script->cursor;
        const Sample& sample = script->samples[script->cursor ? script->cursor - 1 : 0];
        data->buttons = sample.buttons;
        data->connected = sample.connected;
        data->timestamp = sample.timestamp;
        return 0;
    }
    static int close(int) { return 0; }
    static int user_terminate() { return 0; }

    static ScriptedPad* script;
};
ScriptedPad* ScriptedPad::script = nullptr;

PadBackend scripted_backend(ScriptedPad& pad)
{
    ScriptedPad::script = &pad;
    return {ScriptedPad::user_initialize, ScriptedPad::pad_initialize, ScriptedPad::user_list,
            ScriptedPad::open,            ScriptedPad::read_state,      ScriptedPad::close,
            ScriptedPad::user_terminate};
}

struct CapturedSink final : RecordSink
{
    std::vector<std::string> lines;
    void write_line(const std::string& line) override { lines.push_back(line); }
    size_t count(const char* event) const
    {
        size_t n = 0;
        for (const auto& line : lines)
            if (line.find("\"event\":\"" + std::string(event) + "\"") != std::string::npos) ++n;
        return n;
    }
    bool has(const std::string& needle) const
    {
        for (const auto& line : lines)
            if (line.find(needle) != std::string::npos) return true;
        return false;
    }
};

Dependencies make_deps(ScriptedPad& pad, CapturedSink& sink)
{
    PadBackend backend = scripted_backend(pad);
    return {[backend](std::vector<PadBinding> bindings, int user) {
                auto source = std::make_shared<PS5PadSource>(std::move(bindings), backend, user);
                source->activate();
                return source;
            },
            &sink};
}

int failures = 0;
void check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}
constexpr uint32_t OPTIONS = PS5_PAD_BUTTON_OPTIONS;
constexpr uint32_t CROSS = PS5_PAD_BUTTON_CROSS;
} // namespace

int main()
{
    // 1. No press: probe_start + probe_end only, zero key_fired.
    {
        ScriptedPad pad;
        pad.samples = {{}, {0, true, 1}, {0, true, 2}};
        CapturedSink sink;
        Config config;
        config.poll_budget = 4;
        modbridge::padprobe::run_probe(config, make_deps(pad, sink));
        check(sink.count("probe_start") == 1 && sink.count("probe_end") == 1 && sink.count("key_fired") == 0,
              "no press -> no key_fired");
        check(sink.has("\"binding\":\"OPTIONS->F7\""), "probe_start records the OPTIONS->F7 binding");
        check(sink.has("\"polls\":4"), "probe_end records the bounded poll count");
    }

    // 2. One press (with a held duplicate and a cached timestamp) -> exactly one key_fired.
    {
        ScriptedPad pad;
        pad.samples = {{0, true, 100}, {OPTIONS, true, 200}, {OPTIONS, true, 200}, {OPTIONS, true, 201}};
        CapturedSink sink;
        Config config;
        config.poll_budget = 4;
        modbridge::padprobe::run_probe(config, make_deps(pad, sink));
        check(sink.count("key_fired") == 1, "single press + hold + cached read -> exactly one key_fired");
        check(sink.has("\"key\":\"F7\",\"button\":\"OPTIONS\""), "key_fired names F7 <- OPTIONS");
        check(sink.has("\"user_id\":1,\"handle\":7"), "key_fired carries the opened user/handle");
    }

    // 3. Two physical presses (down/up/down with new timestamps) -> two key_fired.
    {
        ScriptedPad pad;
        pad.samples = {{0, true, 10}, {OPTIONS, true, 11}, {0, true, 12}, {OPTIONS, true, 13}};
        CapturedSink sink;
        Config config;
        config.poll_budget = 4;
        modbridge::padprobe::run_probe(config, make_deps(pad, sink));
        check(sink.count("key_fired") == 2, "press/release/press -> two key_fired (rising edge per press)");
    }

    // 4. Unmapped button never fires; only the bound chord does.
    {
        ScriptedPad pad;
        pad.samples = {{0, true, 10}, {CROSS, true, 11}, {OPTIONS | CROSS, true, 12}};
        CapturedSink sink;
        Config config;
        config.poll_budget = 3;
        modbridge::padprobe::run_probe(config, make_deps(pad, sink));
        check(sink.count("key_fired") == 1, "CROSS alone silent; OPTIONS+CROSS fires once (exact-chord match)");
    }

    // 5. Disconnect -> one phase_change; reconnect sample is baseline (silent);
    //    the next press after reconnect fires exactly once.
    {
        ScriptedPad pad;
        pad.samples = {{0, true, 9},
                       {OPTIONS, true, 10}, // pre-disconnect press fires
                       {0, false, 11},
                       {0, true, 12}, // baseline swallow after reattach
                       {OPTIONS, true, 13}};
        CapturedSink sink;
        Config config;
        config.poll_budget = 5;
        modbridge::padprobe::run_probe(config, make_deps(pad, sink));
        check(sink.count("phase_change") == 2, "disconnect + reconnect phase transitions recorded");
        check(sink.has("\"from\":\"Active\",\"to\":\"Disconnected\""), "Active->Disconnected transition recorded");
        check(sink.has("\"from\":\"Disconnected\",\"to\":\"Active\""), "Disconnected->Active transition recorded");
        check(sink.count("key_fired") == 2, "pre-disconnect press + post-reconnect press each fired once");
    }

    // 6. Factory cannot open the pad -> probe_failed with the retained phase name.
    {
        ScriptedPad pad;
        pad.open_result = -1;
        pad.samples = {{}};
        CapturedSink sink;
        Config config;
        config.poll_budget = 4;
        modbridge::padprobe::run_probe(config, make_deps(pad, sink));
        check(sink.count("probe_failed") == 1 && sink.has("\"stage\":\"activate\"") && sink.has("\"phase\":\"Open\""),
              "failed scePadOpen -> probe_failed stage=activate phase=Open");
        check(sink.count("probe_end") == 0, "no probe_end after a failed activation");
    }

    // 7. Null factory -> probe_failed stage=factory.
    {
        CapturedSink sink;
        Config config;
        config.poll_budget = 2;
        modbridge::padprobe::run_probe(config, {nullptr, &sink});
        check(sink.has("\"stage\":\"factory\"") && sink.has("\"detail\":\"no source factory\""),
              "null factory -> probe_failed stage=factory");
    }

    // 8. Stability emits no phase_change records (initial Active is the baseline).
    {
        ScriptedPad pad;
        pad.samples = {{0, true, 1}, {0, true, 2}, {0, true, 3}};
        CapturedSink sink;
        Config config;
        config.poll_budget = 3;
        modbridge::padprobe::run_probe(config, make_deps(pad, sink));
        check(sink.count("phase_change") == 0, "stable Active session -> zero phase_change records");
    }

    // 9. run_probe never throws even when the factory throws.
    {
        CapturedSink sink;
        Config config;
        Dependencies deps{[](std::vector<PadBinding>, int) -> std::shared_ptr<PS5PadSource> {
                              throw std::runtime_error("simulated factory failure");
                          },
                          &sink};
        modbridge::padprobe::run_probe(config, deps);
        check(sink.count("probe_failed") >= 1, "throwing factory contained (probe_failed recorded, no escape)");
    }

    std::printf(failures ? "HOST TESTS: %d FAILURE(S)\n" : "HOST TESTS: ALL PASS (%d failures)\n", failures);
    return failures ? 1 : 0;
}
