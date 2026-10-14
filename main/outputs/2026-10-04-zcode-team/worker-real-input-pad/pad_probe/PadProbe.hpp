#pragma once
// Standalone first-hardware pad probe (worker-real-input-pad, 2026-10-04).
// Proves exactly one thing on real hardware: a pad button reaches the RETAINED
// dispatch gate (RC::Input::Handler) and fires a registered keybind callback.
// It intentionally contains no Lua, no engine, and no game-thread dependency —
// the blocked ExecuteInGameThread seam (worker-3) cannot mask this signal.
//
// Retained sources used verbatim (read-only, linked from the same files the
// composed runtime links): PS5PadSource.cpp (provider + rising-edge detection)
// and Handler.cpp (dispatch gate). See ../PAD-API-AND-INTEGRATION.md.
#include <Input/Handler.hpp>
#include <PS5PadSource.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace modbridge::padprobe
{
// JSONL destination. write_line receives one complete record without the
// trailing newline; implementers must serialize (console sink: append+write).
struct RecordSink
{
    virtual ~RecordSink() = default;
    virtual void write_line(const std::string& line) = 0;
};

struct Config
{
    uint64_t poll_budget{60000}; // bounded: max Handler::process_event iterations
    int requested_user{-1};      // -1 = first logged-in user (matches ini default)
};

// Console wires RC::Input::make_native_ps5_pad_source (real scePad* backend);
// host tests wire a scripted factory over the retained PadBackend test seam
// (PS5PadSource.hpp:14-15, explicit owned tests). The factory must ACTIVATE
// the source (the console factory does eagerly) or return an unavailable one —
// the probe records that as probe_failed rather than treating it as a crash.
using SourceFactory = std::function<std::shared_ptr<RC::Input::PS5PadSource>(
    std::vector<RC::Input::PadBinding>, int user_id)>;

struct Dependencies
{
    SourceFactory make_source;
    RecordSink* sink{}; // not owned; must outlive run_probe
};

// The single hardcoded first-hardware binding: pad OPTIONS -> UE4SS Key::F7.
// The ini path (ue4ss/PS5Pad.ini via parse_ps5_pad_config) is deliberately NOT
// exercised here; it belongs to the composed-runtime step after this probe
// passes (see ../PS5Pad.ini and HARDWARE-TEST-PLAN.md).
constexpr const char* probe_button_name = "OPTIONS";
constexpr const char* probe_key_name = "F7";

// Runs the bounded probe synchronously on the calling thread (console: call
// from the payload's probe path; host: the test). Records written:
//   probe_start  — always first (binding, requested_user)
//   probe_failed — factory/init/registration failure; final record
//   key_fired    — one per dispatch-gate delivery of Key::F7
//   phase_change — provider PadPhase transitions (e.g. Disconnected/reconnect)
//   probe_end    — normal end at poll_budget (polls, keys_fired, final status)
// Never throws; returns the final record body as a convenience for the caller.
std::string run_probe(const Config& config, const Dependencies& deps);
}
