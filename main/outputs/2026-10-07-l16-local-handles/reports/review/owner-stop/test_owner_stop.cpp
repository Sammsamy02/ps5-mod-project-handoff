// Four independent cleanup cases using the actual selected RuntimeOwner source.
// Kernel adapters and bridge/supplier bodies are generated from their inputs
// by run_owner_stop.py; production sources and the main fixture stay untouched.
#include "kernel_adapters.inc"
#include "runtime_owner.hpp"

// A null acknowledgement sink is used. These link shims are never its I/O path.
extern "C" int mfa_status_open(mfa_status*, const char*, long, uint64_t) { return -1; }
extern "C" int mfa_status_write(mfa_status*, const char*, int, const char*) { return -1; }
extern "C" void mfa_status_close(mfa_status*) {}

struct WorkerState {
    std::atomic_bool stopping{false};
    std::atomic_bool done{false};
};

struct ReviewBackend final : modbridge::native::RuntimeBackend {
    std::shared_ptr<WorkerState> state;
    explicit ReviewBackend(std::shared_ptr<WorkerState> value) : state(std::move(value)) {}
    void initialize(std::function<void()> ready) override {
        ready();
        while (!state->stopping.load()) std::this_thread::yield();
        state->done.store(true);
    }
    void request_stop() noexcept override { state->stopping.store(true); }
};

namespace modbridge::native {
#include "supplier_stop.inc"
static int stop_mode = 0;
static bool StopRuntimeWithAck(std::unique_ptr<RuntimeOwner>& owner) {
    if (stop_mode == 1) throw 7; // actual destructor must stop/join in bridge catch
    const bool stopped = RealStopRuntimeWithAck(owner);
    return stop_mode == 2 ? false : stopped;
}
}

static std::unique_ptr<modbridge::native::RuntimeOwner>& owner_slot() {
    static std::unique_ptr<modbridge::native::RuntimeOwner> owner;
    return owner;
}
static constexpr int MRS_ERR_STOP_EXN = 5;
#include "stop_extract.inc"

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    dir = argv[1];
    std::filesystem::create_directory(dir);
    for (int mode = 0; mode < 3; ++mode) {
        review_done = nullptr;
        reset();
        auto state = std::make_shared<WorkerState>();
        review_done = &state->done;
        close_before_join = 0;
        owner_slot() = std::make_unique<modbridge::native::RuntimeOwner>(
            std::make_unique<ReviewBackend>(state));
        const bool started = owner_slot()->start();
        modbridge::native::stop_mode = mode;
        const int result = modbridge_runtime_stop();
        check(started && state->done.load() && !owner_slot() && live_fds.empty() &&
                  close_before_join == 0 && result == (mode == 0 ? 0 : mode == 1 ? 5 : 1),
              mode == 0 ? "actual selected owner normal stop joins before releasing bank" :
              mode == 1 ? "actual selected owner destruction after injected stop exception joins before releasing bank" :
                          "actual selected owner destroyed despite negative stop releases bank after join");
        review_done = nullptr;
    }
    reset();
    check(modbridge_runtime_stop() == 0 && live_fds.empty(),
          "empty-owner actual stop path releases bank");
    std::filesystem::remove_all(dir);
    std::printf("failures=%d\n", failures);
    return failures ? 1 : 0;
}
