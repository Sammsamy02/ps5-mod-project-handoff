// Host test for the RUNTIME-ACK emission points added by runtime-ack.diff.
// Compiles the PATCHED bootstrap (ue4ss_backend.cpp, runtime_owner.cpp) plus
// the REAL activated_mod_source.cpp and the REAL vendored modforge_activation.c
// against stub port headers. Verifies the acknowledgment sequence, the
// running==mods-started rule, failure paths, and open/write-failure tolerance.
#include "ue4ss_backend.hpp"
#include <UE4SSProgram.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std::chrono_literals;
using namespace modbridge::native;

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::_Exit(1); } } while (false)

namespace
{
const std::string kTitle = "TestTitle";
constexpr long kPid = 4242;
constexpr std::uint64_t kNow = 1700000000123ull;

std::filesystem::path make_temp_dir(const char* tag)
{
    static int counter = 0;
    auto dir = std::filesystem::temp_directory_path() /
        ("runtime-ack-" + std::string(tag) + "-" + std::to_string(::getpid()) + "-" + std::to_string(++counter));
    std::filesystem::create_directories(dir / "ue4ss/Mods/TestMod/Scripts");
    return dir;
}

// Stage a real modforge mod root: loadlist.txt + one preloaded lua_mod file.
void stage_mod_root(const std::filesystem::path& root)
{
    const std::string lua = "-- modbridge ack test\n";
    std::filesystem::create_directories(root / "ue4ss/Mods/TestMod/Scripts");
    std::filesystem::path lua_path = root / "ue4ss/Mods/TestMod/Scripts/main.lua";
    std::ofstream(lua_path, std::ios::binary) << lua;
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
    RC::Platform::NativeProgramContext context;
    std::ofstream(base / "module.prx", std::ios::binary) << "prx";
    std::ofstream(base / "game.elf", std::ios::binary) << "elf";
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

// RUNTIME-ACK.md rule 2: running iff the last complete state line is
// mods-started.
bool running(const std::filesystem::path& root)
{
    const auto states = state_lines(root);
    return !states.empty() && states.back() == "mods-started";
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

void wait_for_running(const std::filesystem::path& root)
{
    const auto end = std::chrono::steady_clock::now() + 2s;
    while (!running(root) && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(1ms);
    CHECK(running(root));
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
    // T1: no ack configured -> identical lifecycle, no status file at all.
    {
        reset_knobs();
        const auto base = make_temp_dir("t1");
        stage_mod_root(base / "mods");
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"), kTitle);
        CHECK(owner->start());
        wait_state(*owner, RuntimeState::EngineInitialized);
        CHECK(owner->stop());
        CHECK(owner->status().state == RuntimeState::Stopped);
        CHECK(!std::filesystem::exists(base / "mods/runtime-status.txt"));
        std::puts("T1 PASS no-ack guard: lifecycle unchanged, no runtime-status.txt");
    }
    // T2: full success -> activated, engine-ready, mods-started (running=1),
    // then stopped after stop() joins (running=0). Header/session recorded.
    {
        reset_knobs();
        const auto base = make_temp_dir("t2");
        stage_mod_root(base / "mods");
        RuntimeAckConfig ack_config;
        ack_config.status_root = base / "mods";
        ack_config.pid = kPid;
        ack_config.now_ms = kNow;
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, ack_config);
        CHECK(owner->start());
        wait_for_running(base / "mods"); // engine-ready AND mods-started observed
        CHECK(owner->stop());
        CHECK(owner->status().state == RuntimeState::Stopped);
        CHECK(owner->stop()); // repeated stop (as the destructor performs)
        const auto lines = status_lines(base / "mods");
        CHECK(lines.size() == 6); // header + session + 4 states, no duplicate stopped
        CHECK(lines[0] == "modforge-runtime-status\t1");
        CHECK(lines[1] == "session\t4242\t1700000000123");
        CHECK(join(state_lines(base / "mods")) == "activated,engine-ready,mods-started,stopped");
        CHECK(!running(base / "mods"));
        std::puts("T2 PASS success sequence: activated,engine-ready,mods-started(running),stopped");
    }
    // T3: program construction rejected -> prepare throws, single failed line.
    {
        reset_knobs();
        RC::UE4SSProgram::reject_construction = true;
        const auto base = make_temp_dir("t3");
        stage_mod_root(base / "mods");
        RuntimeAckConfig ack_config;
        ack_config.status_root = base / "mods";
        ack_config.pid = kPid;
        ack_config.now_ms = kNow;
        bool threw = false;
        try
        {
            PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"), kTitle, {}, ack_config);
        }
        catch (const std::runtime_error& error)
        {
            threw = std::string(error.what()).find("Native UE4SS construction rejected") != std::string::npos;
        }
        CHECK(threw);
        CHECK(join(state_lines(base / "mods")) == "failed");
        const auto lines = status_lines(base / "mods");
        CHECK(lines.size() == 3); // header + session + failed
        CHECK(lines[2].find("stub construction error") != std::string::npos);
        CHECK(lines[2].find("\t1\t") != std::string::npos); // nonzero code
        CHECK(!running(base / "mods"));
        std::puts("T3 PASS construction failure: failed(code!=0,detail), running=0");
    }
    // T4: deferred mod startup inventory rejected -> failed, engine never runs.
    {
        reset_knobs();
        RC::UE4SSProgram::reject_inventory = true;
        const auto base = make_temp_dir("t4");
        stage_mod_root(base / "mods");
        RuntimeAckConfig ack_config;
        ack_config.status_root = base / "mods";
        ack_config.pid = kPid;
        ack_config.now_ms = kNow;
        bool threw = false;
        try
        {
            PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"), kTitle, {}, ack_config);
        }
        catch (const std::runtime_error& error)
        {
            threw = std::string(error.what()).find("Native UE4SS inventory rejected") != std::string::npos;
        }
        CHECK(threw);
        CHECK(join(state_lines(base / "mods")) == "failed");
        CHECK(!running(base / "mods"));
        std::puts("T4 PASS inventory failure: failed before activated, running=0");
    }
    // T5: engine failure AFTER the callback -> failed is the last line even
    // though engine-ready and mods-started were honestly recorded earlier.
    {
        reset_knobs();
        RC::UE4SSProgram::fail_after_engine_ready = true;
        const auto base = make_temp_dir("t5");
        stage_mod_root(base / "mods");
        RuntimeAckConfig ack_config;
        ack_config.status_root = base / "mods";
        ack_config.pid = kPid;
        ack_config.now_ms = kNow;
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, ack_config);
        CHECK(owner->start());
        wait_state(*owner, RuntimeState::Failed);
        CHECK(owner->status().error == "stub engine startup deadline exceeded");
        CHECK(owner->stop());
        CHECK(owner->status().state == RuntimeState::Failed); // failed stays terminal
        CHECK(join(state_lines(base / "mods")) == "activated,engine-ready,mods-started,failed");
        CHECK(!running(base / "mods"));
        std::puts("T5 PASS post-callback engine failure: failed last, running=0");
    }
    // T6: status open failure (root is a regular file) -> inert sink, the
    // engine still starts and stops cleanly.
    {
        reset_knobs();
        const auto base = make_temp_dir("t6");
        stage_mod_root(base / "mods");
        std::ofstream(base / "not-a-dir") << "file";
        RuntimeAckConfig ack_config;
        ack_config.status_root = base / "not-a-dir"; // open -> ENOTDIR
        ack_config.pid = kPid;
        ack_config.now_ms = kNow;
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, ack_config);
        CHECK(owner->start());
        wait_state(*owner, RuntimeState::EngineInitialized);
        CHECK(owner->stop());
        CHECK(owner->status().state == RuntimeState::Stopped);
        std::puts("T6 PASS open-failure tolerance: startup never blocked by ack");
    }
    // T7: write failure mid-session (RLIMIT_FSIZE clamped to the current file
    // size, SIGXFSZ ignored) -> emissions record the failure and continue.
    {
        reset_knobs();
        const auto base = make_temp_dir("t7");
        std::filesystem::create_directories(base / "mods");
        RuntimeAckConfig ack_config;
        ack_config.status_root = base / "mods";
        ack_config.pid = kPid;
        ack_config.now_ms = kNow;
        auto sink = RuntimeAckSink::Open(ack_config);
        CHECK(sink && sink->last_error().empty());
        sink->activated();
        CHECK(join(state_lines(base / "mods")) == "activated");
        std::signal(SIGXFSZ, SIG_IGN);
        rlimit limits{};
        CHECK(getrlimit(RLIMIT_FSIZE, &limits) == 0);
        rlimit clamped{static_cast<rlim_t>(std::filesystem::file_size(base / "mods/runtime-status.txt")),
                       limits.rlim_max};
        CHECK(setrlimit(RLIMIT_FSIZE, &clamped) == 0);
        sink->engine_ready();   // must fail internally...
        sink->mods_started();   // ...and must not throw or abort
        sink->failed(1, "recorded anyway");
        CHECK(setrlimit(RLIMIT_FSIZE, &limits) == 0);
        CHECK(join(state_lines(base / "mods")) == "activated"); // no new line landed
        CHECK(!sink->last_error().empty());
        std::printf("T7 PASS write-failure tolerance: last_error=\"%s\"\n", sink->last_error().c_str());
        sink->close();
    }
    // T8: open-time write failure (RLIMIT_FSIZE 0 before Prepare) -> the sink
    // opens inert and a full activated runtime lifecycle still completes.
    {
        reset_knobs();
        const auto base = make_temp_dir("t8");
        stage_mod_root(base / "mods");
        std::signal(SIGXFSZ, SIG_IGN);
        rlimit limits{};
        CHECK(getrlimit(RLIMIT_FSIZE, &limits) == 0);
        rlimit zero{0, limits.rlim_max};
        CHECK(setrlimit(RLIMIT_FSIZE, &zero) == 0);
        RuntimeAckConfig ack_config;
        ack_config.status_root = base / "mods";
        ack_config.pid = kPid;
        ack_config.now_ms = kNow;
        auto owner = PrepareActivatedUE4SSRuntime(make_context(base), activate(base / "mods"),
                                                  kTitle, {}, ack_config);
        CHECK(owner->start());
        wait_state(*owner, RuntimeState::EngineInitialized);
        CHECK(owner->stop());
        CHECK(setrlimit(RLIMIT_FSIZE, &limits) == 0);
        CHECK(owner->status().state == RuntimeState::Stopped);
        CHECK(state_lines(base / "mods").empty()); // header write itself failed
        std::puts("T8 PASS open-write failure: full lifecycle with zero ack lines");
    }
    std::puts("PASS runtime-ack emission sequence, failure and tolerance (host stubs)");
    return 0;
}
