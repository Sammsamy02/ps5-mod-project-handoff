#include "PadProbe.hpp"
#include <chrono>
namespace modbridge::padprobe
{
namespace
{
using RC::Input::Handler;
using RC::Input::InputEvent;
using RC::Input::Key;
using RC::Input::PadBinding;
using RC::Input::PadPhase;
using RC::Input::PadStatus;
using RC::Input::PS5PadSource;

// Names mirror the retained PadPhase enum order (PS5PadSource.hpp:10-12).
auto phase_name(PadPhase phase) -> const char*
{
    switch (phase)
    {
        case PadPhase::Uninitialized: return "Uninitialized";
        case PadPhase::InvalidBindings: return "InvalidBindings";
        case PadPhase::UserInitialize: return "UserInitialize";
        case PadPhase::PadInitialize: return "PadInitialize";
        case PadPhase::UserList: return "UserList";
        case PadPhase::NoUser: return "NoUser";
        case PadPhase::Open: return "Open";
        case PadPhase::Active: return "Active";
        case PadPhase::Disconnected: return "Disconnected";
        case PadPhase::Read: return "Read";
        case PadPhase::Close: return "Close";
        case PadPhase::UserTerminate: return "UserTerminate";
        case PadPhase::Inactive: return "Inactive";
    }
    return "Unknown";
}

struct Stamp
{
    std::chrono::steady_clock::time_point start;
    auto ms() const -> std::string
    {
        return std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
                .count());
    }
};

// All emitted names are compile-time constants and all numbers are integers,
// so plain concatenation is valid JSONL; no user-controlled strings exist.
struct Recorder
{
    RecordSink* sink{};
    Stamp stamp;
    auto emit(const std::string& body) const
    {
        if (sink) sink->write_line("{\"ts_ms\":" + stamp.ms() + "," + body + "}");
    }
};
} // namespace

auto run_probe(const Config& config, const Dependencies& deps) -> std::string
{
    const Recorder recorder{deps.sink, Stamp{std::chrono::steady_clock::now()}};
    uint64_t polls = 0;
    uint64_t keys_fired = 0;
    try
    {
        recorder.emit("\"event\":\"probe_start\",\"binding\":\"OPTIONS->F7\",\"requested_user\":" +
                      std::to_string(config.requested_user));

        // The one first-hardware binding: OPTIONS rising edge -> Key::F7.
        const std::vector<PadBinding> bindings{PadBinding{PS5_PAD_BUTTON_OPTIONS, Key::F7, {}}};

        std::shared_ptr<PS5PadSource> source;
        if (!deps.make_source)
        {
            recorder.emit("\"event\":\"probe_failed\",\"stage\":\"factory\",\"detail\":\"no source factory\"");
            return "\"event\":\"probe_failed\",\"stage\":\"factory\"";
        }
        source = deps.make_source(bindings, config.requested_user);
        if (!source)
        {
            recorder.emit("\"event\":\"probe_failed\",\"stage\":\"factory\",\"detail\":\"factory returned null\"");
            return "\"event\":\"probe_failed\",\"stage\":\"factory\"";
        }
        if (!source->is_available())
        {
            const PadStatus& status = source->status();
            recorder.emit("\"event\":\"probe_failed\",\"stage\":\"activate\",\"phase\":\"" +
                          std::string(phase_name(status.phase)) +
                          "\",\"error\":" + std::to_string(status.error));
            return "\"event\":\"probe_failed\",\"stage\":\"activate\"";
        }

        // Real retained dispatch gate. Console ordering is mandatory:
        // __PROSPERO__ defaults Handler::m_source_injected_mode to true, and
        // register_keydown_event throws without an available active source
        // (Handler.hpp:61-64, Handler.cpp:109) — so init() runs first.
        Handler handler;
        handler.init(source);
        handler.register_keydown_event(Key::F7, [&] {
            ++keys_fired;
            const PadStatus& status = source->status();
            recorder.emit("\"event\":\"key_fired\",\"key\":\"F7\",\"button\":\"OPTIONS\",\"user_id\":" +
                          std::to_string(status.user_id) + ",\"handle\":" + std::to_string(status.handle) +
                          ",\"polls_so_far\":" + std::to_string(polls));
        });
        if (!handler.is_keydown_event_registered(Key::F7))
        {
            recorder.emit("\"event\":\"probe_failed\",\"stage\":\"register\",\"detail\":\"F7 not registered\"");
            return "\"event\":\"probe_failed\",\"stage\":\"register\"";
        }

        PadPhase last_phase = source->status().phase;
        for (; polls < config.poll_budget; ++polls)
        {
            // One program-tick equivalent: poll the provider through the real
            // Handler (Handler.cpp:75) and deliver matching callbacks.
            handler.process_event();
            const PadStatus& status = source->status();
            if (status.phase != last_phase)
            {
                recorder.emit("\"event\":\"phase_change\",\"from\":\"" + std::string(phase_name(last_phase)) +
                              "\",\"to\":\"" + std::string(phase_name(status.phase)) +
                              "\",\"error\":" + std::to_string(status.error));
                last_phase = status.phase;
            }
        }

        const PadStatus& status = source->status();
        recorder.emit("\"event\":\"probe_end\",\"polls\":" + std::to_string(polls) +
                      ",\"keys_fired\":" + std::to_string(keys_fired) + ",\"final_phase\":\"" +
                      std::string(phase_name(status.phase)) + "\",\"user_id\":" + std::to_string(status.user_id) +
                      ",\"handle\":" + std::to_string(status.handle));
        return "\"event\":\"probe_end\",\"keys_fired\":" + std::to_string(keys_fired);
    }
    catch (const std::exception&)
    {
        recorder.emit("\"event\":\"probe_failed\",\"stage\":\"run\",\"detail\":\"exception\"");
        return "\"event\":\"probe_failed\",\"stage\":\"run\"";
    }
    catch (...)
    {
        recorder.emit("\"event\":\"probe_failed\",\"stage\":\"run\",\"detail\":\"unknown\"");
        return "\"event\":\"probe_failed\",\"stage\":\"run\"";
    }
}
}
