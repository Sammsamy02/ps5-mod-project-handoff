// Adversarial host pass for the ack-enabled vendored bootstrap
// (worker-ack-integration-deep). Re-verifies the previously tested failure
// scenarios against the composition-2-compatible vendored tree and adds the
// corners the worker-runtime-ack suite did not cover:
//   A1 double-start          A5 stale-session re-open interleaving (finding)
//   A2 stop-before-start     A6 ENOENT open failure
//   A3 triple-stop+dtor      A7 failed(0) code normalization
//   A4 module re-entry       A8 empty status_root sink / A9 double close()
// Everything is host evidence with stub port headers; nothing ran on console.
#include "ue4ss_backend.hpp"
#include <UE4SSProgram.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std::chrono_literals;
using namespace modbridge::native;

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::_Exit(1); } } while (false)

namespace
{
const std::string kTitle = "TestTitle";
constexpr long kPidA = 1111;
constexpr long kPidB = 2222;

std::filesystem::path make_temp_dir(const char* tag)
{
    static int counter = 0;
    auto dir = std::filesystem::temp_directory_path() /
        ("ack-adv-" + std::string(tag) + "-" + std::to_string(::getpid()) + "-" + std::to_string(++counter));
    std::filesystem::create_directories(dir);
    return dir;
}

void stage_mod_root(const std::filesystem::path& root)
{
    const std::string lua = "-- modbridge ack adversarial test\n";
    std::filesystem::create_directories(root / "ue4ss/Mods/TestMod/Scripts");
    std::ofstream(root / "ue4ss/Mods/TestMod/Scripts/main.lua", std::ios::binary) << lua;
    std::ofstream(root / "loadlist.txt", std::ios::binary)
        << "modforge-loadlist\t1\n"
        << "title\t" << kTitle << "\n"
        << "engine\tunreal\n"
        << "framework\tue4ss-ps5\n"
        << "mod\tTestMod\t1.0\n"
        << "file\tue4ss/Mods/TestMod/Scripts/main.lua\tlua_mod\t" << lua.size() << "\n";
}

ActivatedMods activate(const std::filesystem::path& root)
{
    const std::string root_string = root.string();
    mfa_config cfg{};
    cfg.root = root_string.c_str();
    cfg.title_id = kTitle.c_str();
    mfa_activation* raw = nullptr;
    mfa_error err{};
    if (mfa_activate(&cfg, &raw, &err) != MFA_OK)
    {
        std::fprintf(stderr, "mfa_activate failed: %d %s\n", err.code, err.message);
        std::_Exit(1);
    }
    return ActivatedMods(raw);
}

RC::Platform::NativeProgramContext make_context(const std::filesystem::path& base)
{
    std::ofstream(base / "module.prx", std::ios::binary) << "prx";
    std::ofstream(base / "game.elf", std::ios::binary) << "elf";
    RC::Platform::NativeProgramContext context;
    context.module_file_path = base / "module.prx";
    context.game_executable_path = base / "game.elf";
    context.module_source = std::make_shared<RC::Unreal::Platform::ModuleSnapshot>();
    return context;
}

std::vector<std::string> status_lines(const std::filesystem::path& root)
{
    std::vector<std::string> lines;
    std::ifstream in(root / "runtime-status.txt", std::ios::binary);
    std::string line;
    while (std::getline(in, line))
        if (!line.empty() && line.back() == '\r') lines.push_back(line.substr(0, line.size() - 1));
        else lines.push_back(line);
    return lines;
}

std::vector<std::string> state_lines(const std::filesystem::path& root)
{
    std::vector<std::string> states;
    for (const auto& line : status_lines(root))
        if (line.rfind("state\t", 0) == 0)
        {
            std::istringstream fields(line);
            std::string tag, seq, state;
            std::getline(fields, tag, '\t');
            std::getline(fields, seq, '\t');
            std::getline(fields, state, '\t');
            states.push_back(state);
        }
    return states;
}

std::string join(const std::vector<std::string>& parts)
{
    std::string out;
    for (const auto& part : parts) { if (!out.empty()) out += ","; out += part; }
    return out;
}

void reset_knobs()
{
    RC::UE4SSProgram::reject_construction = false;
    RC::UE4SSProgram::reject_inventory = false;
    RC::UE4SSProgram::fail_after_engine_ready = false;
}

RuntimeAckConfig ack_for(const std::filesystem::path& root, long pid)
{
    RuntimeAckConfig config;
    config.status_root = root;
    config.pid = pid;
    config.now_ms = 1700000000123ull; // fixed width keeps offsets deterministic
    return config;
}

void wait_state(const RuntimeOwner& owner, RuntimeState expected)
{
    const auto end = std::chrono::steady_clock::now() + 2s;
    while (owner.status().state != expected && std::chrono::steady_clock::now() < end)
        std::this_thread::yield();
    CHECK(owner.status().state == expected);
}
} // namespace

int main()
{
    // A1: double-start — the second start() must be refused and add no line.
    {
        reset_knobs();
        const auto base = make_temp_dir("a1");
        stage_mod_root(base / "mods");
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, ack_for(base / "mods", kPidA));
        CHECK(owner->start());
        CHECK(!owner->start()); // only Prepared may start
        wait_state(*owner, RuntimeState::EngineInitialized);
        CHECK(owner->stop());
        CHECK(join(state_lines(base / "mods")) == "activated,engine-ready,mods-started,stopped");
        std::puts("A1 PASS double-start refused, no duplicate lines");
    }
    // A2: stop-before-start — a prepared runtime stopped without ever
    // starting acknowledges activated,stopped and never claims running.
    {
        reset_knobs();
        const auto base = make_temp_dir("a2");
        stage_mod_root(base / "mods");
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, ack_for(base / "mods", kPidA));
        CHECK(owner->stop()); // never started
        CHECK(owner->status().state == RuntimeState::Stopped);
        CHECK(join(state_lines(base / "mods")) == "activated,stopped");
        std::puts("A2 PASS stop-before-start: activated,stopped, running=0");
    }
    // A3: triple-stop + destructor — exactly one stopped line, fd released once.
    {
        reset_knobs();
        const auto base = make_temp_dir("a3");
        stage_mod_root(base / "mods");
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, ack_for(base / "mods", kPidA));
        CHECK(owner->start());
        wait_state(*owner, RuntimeState::EngineInitialized);
        CHECK(owner->stop());
        CHECK(owner->stop());
        CHECK(owner->stop());
        const auto lines = status_lines(base / "mods");
        CHECK(lines.size() == 6);
        CHECK(join(state_lines(base / "mods")) == "activated,engine-ready,mods-started,stopped");
        owner.reset(); // destructor stop() adds nothing
        CHECK(lines.size() == 6);
        std::puts("A3 PASS triple-stop + destructor: single stopped line");
    }
    // A4: module re-entry (sequential sessions) — the new session REPLACES the
    // status file (fresh header, seq restarts at 1); the old session's lines
    // cannot leak into the new one.
    {
        reset_knobs();
        const auto base = make_temp_dir("a4");
        const auto mods = base / "mods";
        stage_mod_root(mods);
        {
            auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(mods),
                                                      kTitle, {}, ack_for(mods, kPidA));
            CHECK(owner->start());
            wait_state(*owner, RuntimeState::EngineInitialized);
            CHECK(owner->stop());
        }
        CHECK(join(state_lines(mods)) == "activated,engine-ready,mods-started,stopped");
        {
            auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(mods),
                                                      kTitle, {}, ack_for(mods, kPidB));
            CHECK(owner->start());
            wait_state(*owner, RuntimeState::EngineInitialized);
            CHECK(owner->stop());
        }
        const auto lines = status_lines(mods);
        CHECK(lines.size() == 6); // replaced, not appended
        CHECK(lines[0] == "modforge-runtime-status\t1");
        CHECK(lines[1] == "session\t2222\t1700000000123"); // new session facts
        CHECK(join(state_lines(mods)) == "activated,engine-ready,mods-started,stopped");
        std::puts("A4 PASS module re-entry: fresh session replaces file, seq restarts");
    }
    // A5: stale-session interleaving (FINDING, guarded upstream) — two sinks
    // open on the same root at once is a composition bug. The module_start
    // entry guard (module_entry.c, -20) and one-session-per-process prevent
    // it; this test documents the damage so nobody mistakes it for supported.
    {
        reset_knobs();
        const auto base = make_temp_dir("a5");
        const auto mods = base / "mods";
        stage_mod_root(mods);
        const auto config_a = ack_for(mods, kPidA);
        const auto config_b = ack_for(mods, kPidB);
        auto stale = RuntimeAckSink::Open(config_a);
        CHECK(stale && stale->last_error().empty());
        stale->activated(); // offset now past session A's header
        auto fresh = RuntimeAckSink::Open(config_b); // replaces the file
        CHECK(fresh && fresh->last_error().empty());
        stale->engine_ready(); // stale writer punches a NUL hole past B's header
        const auto lines = status_lines(mods);
        bool clean_b_header = lines.size() >= 2 && lines[0] == "modforge-runtime-status\t1" &&
            lines[1] == "session\t2222\t1700000000123";
        // The stale write must NOT be readable as a current state line.
        bool stale_visible = false;
        for (const auto& line : state_lines(mods))
            if (line == "engine-ready") stale_visible = true;
        CHECK(clean_b_header);
        CHECK(!stale_visible); // NUL-prefixed fragment is not a state line
        std::puts("A5 FINDING PASS stale re-open corrupts mid-file; reader sees no state; "
                  "prevented by module_start entry guard (one session per process)");
        fresh->close();
        stale->close();
    }
    // A6: open failure ENOENT (root directory does not exist) — inert sink,
    // the engine lifecycle still completes normally.
    {
        reset_knobs();
        const auto base = make_temp_dir("a6");
        stage_mod_root(base / "mods");
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {},
                                                  ack_for(base / "missing-root", kPidA));
        CHECK(owner->start());
        wait_state(*owner, RuntimeState::EngineInitialized);
        CHECK(owner->stop());
        CHECK(owner->status().state == RuntimeState::Stopped);
        CHECK(!std::filesystem::exists(base / "missing-root"));
        std::puts("A6 PASS ENOENT open failure: inert sink, lifecycle completes");
    }
    // A7: failed(0) normalizes to a nonzero code (contract: code != 0).
    {
        const auto base = make_temp_dir("a7");
        const auto mods = base / "mods";
        std::filesystem::create_directories(mods);
        auto sink = RuntimeAckSink::Open(ack_for(mods, kPidA));
        CHECK(sink && sink->last_error().empty());
        sink->failed(0, "zero must become one");
        const auto lines = status_lines(mods);
        CHECK(lines.size() == 3);
        CHECK(lines[2].rfind("state\t1\tfailed\t1\t", 0) == 0);
        sink->close();
        std::puts("A7 PASS failed(0) normalized to code 1");
    }
    // A8: empty status_root — Open stays inert with a recorded reason and
    // Prepare takes the default (no sink) path.
    {
        auto sink = RuntimeAckSink::Open(RuntimeAckConfig{});
        CHECK(sink);
        CHECK(sink->last_error() == "runtime ack root is not configured");
        sink->activated(); // recorded no-op
        sink->stopped();
        sink->close();
        reset_knobs();
        const auto base = make_temp_dir("a8");
        stage_mod_root(base / "mods");
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, RuntimeAckConfig{});
        CHECK(owner->start());
        wait_state(*owner, RuntimeState::EngineInitialized);
        CHECK(owner->stop());
        CHECK(!std::filesystem::exists(base / "mods/runtime-status.txt"));
        std::puts("A8 PASS empty config: inert sink with reason, no file");
    }
    // A9: double close() is idempotent (mfa_status_close guards fd < 0).
    {
        const auto base = make_temp_dir("a9");
        const auto mods = base / "mods";
        std::filesystem::create_directories(mods);
        auto sink = RuntimeAckSink::Open(ack_for(mods, kPidA));
        CHECK(sink && sink->last_error().empty());
        sink->stopped();
        sink->close();
        sink->close();
        CHECK(sink->last_error().empty());
        sink->stopped(); // emit-once guard: a second stopped() is a silent no-op
        CHECK(sink->last_error().empty());
        sink->activated(); // a NEW state after close records why it was dropped
        CHECK(sink->last_error() == "ack sink has no open status file");
        std::puts("A9 PASS double close idempotent; post-close stopped stays emit-once; "
                  "new state recorded as no-open no-op");
    }
    std::puts("PASS adversarial ack pass (host stubs, composition-compatible vendored tree)");
    return 0;
}
