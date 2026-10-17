// seam_host_test.cpp — host tests for the seam adapters AND the existing
// lifetime/cancellation/affinity guards, reproduced unmodified on host:
//   * outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp
//     is compiled as-is (read-only use of immutable history) so the real
//     RuntimeOwner self-join refusal, late-ready protection and failure
//     honesty are exercised, not a reimplementation.
//   * .superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/
//     Unreal/Platform/NativeGameThreadIdentity.hpp is included as-is for the
//     real game-thread identity affinity guards.
// Host evidence only: nothing here counts as a Townfall game-thread callback.
#define MODBRIDGE_SEAM_HOST_TESTS 1

#include "gamethread_seams.hpp"

#include <Unreal/Platform/NativeGameThreadIdentity.hpp>

#include "runtime_owner.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

using namespace modbridge::seams;
using modbridge::native::RuntimeBackend;
using modbridge::native::RuntimeOwner;
using modbridge::native::RuntimeState;
using namespace std::chrono_literals;

#define CHECK(x)                                                      \
    do                                                                \
    {                                                                 \
        if (!(x))                                                     \
        {                                                             \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
            std::_Exit(1);                                            \
        }                                                             \
    } while (false)

static auto contains(const std::string& haystack, const char* needle) -> bool
{
    return haystack.find(needle) != std::string::npos;
}

// ===========================================================================
// SEAM S1 adapter: refusal paths
// ===========================================================================
static auto base_facts() -> LiveModuleFacts
{
    LiveModuleFacts facts;
    facts.lifetime_owner = std::make_shared<int>(42); // any retaining owner
    facts.main_exe.base = reinterpret_cast<const uint8_t*>(0x1000);
    facts.main_exe.size = 0x2000;
    facts.main_exe.has_readable_ranges = true;
    facts.module_file_path = "/app0/MODBRIDGE/modbridge.prx";
    facts.game_executable_path = "/app0/sile/bin/townfall.elf";
    facts.mapped.elf64_sysv = true;
    facts.mapped.text_base = 0x1000;
    facts.mapped.image_base = 0x1000;
    facts.correspondence = LiveMappingProof::MintForHostFixture();
    return facts;
}

static void test_loader_refusals()
{
    // 1. Supplier absent (worker-1 in flight).
    auto r = ResolveLiveModuleFacts(std::nullopt);
    CHECK(!r.ok && r.refusal == LoaderRefusal::SupplierAbsent);

    // 2. Pretender: paths/base present, no genuine live proof.
    auto pretender = base_facts();
    pretender.correspondence = {};
    r = ResolveLiveModuleFacts(pretender);
    CHECK(!r.ok && r.refusal == LoaderRefusal::EvidencePretender);
    CHECK(contains(r.message, "not a supplier"));

    // 3. Facts with no raw materials and no proof (proof absent, not pretender).
    auto empty = LiveModuleFacts{};
    r = ResolveLiveModuleFacts(empty);
    CHECK(!r.ok && r.refusal == LoaderRefusal::ProofAbsent);

    // 4. Lifetime owner absent.
    auto no_owner = base_facts();
    no_owner.lifetime_owner = nullptr;
    r = ResolveLiveModuleFacts(no_owner);
    CHECK(!r.ok && r.refusal == LoaderRefusal::LifetimeAbsent);

    // 5. MainExe bytes absent.
    auto no_exe = base_facts();
    no_exe.main_exe.base = nullptr;
    r = ResolveLiveModuleFacts(no_exe);
    CHECK(!r.ok && r.refusal == LoaderRefusal::MainExeAbsent);

    // 6. Readable ranges absent.
    auto no_ranges = base_facts();
    no_ranges.main_exe.has_readable_ranges = false;
    r = ResolveLiveModuleFacts(no_ranges);
    CHECK(!r.ok && r.refusal == LoaderRefusal::RangesAbsent);

    // 7. Format not ELF64/SysV.
    auto wrong_format = base_facts();
    wrong_format.mapped.elf64_sysv = false;
    r = ResolveLiveModuleFacts(wrong_format);
    CHECK(!r.ok && r.refusal == LoaderRefusal::FormatNotElf64);

    // 8. Serialized ELF witness offered without EH extents.
    auto no_eh = base_facts();
    no_eh.serialized_original_elf = std::vector<uint8_t>(64, 0x7f);
    no_eh.mapped.eh_frame.present = false;
    r = ResolveLiveModuleFacts(no_eh);
    CHECK(!r.ok && r.refusal == LoaderRefusal::WitnessWithoutEh);
    auto no_hdr = base_facts();
    no_hdr.serialized_original_elf = std::vector<uint8_t>(64, 0x7f);
    no_hdr.mapped.eh_frame_hdr.present = false;
    r = ResolveLiveModuleFacts(no_hdr);
    CHECK(!r.ok && r.refusal == LoaderRefusal::WitnessWithoutEh);

    // 9. Path rules mirrored from ValidateNativeProgramContext.
    auto rel = base_facts();
    rel.module_file_path = "relative/modbridge.prx";
    r = ResolveLiveModuleFacts(rel);
    CHECK(!r.ok && r.refusal == LoaderRefusal::PathInvalid);
    auto dotdot = base_facts();
    dotdot.game_executable_path = "/app0/../app0/game.elf";
    r = ResolveLiveModuleFacts(dotdot);
    CHECK(!r.ok && r.refusal == LoaderRefusal::PathInvalid);

    // 10. Accept: complete genuine facts resolve, inputs preserved verbatim.
    auto full = base_facts();
    full.monolithic = true;
    full.serialized_original_elf = std::vector<uint8_t>(64, 0x7f);
    full.mapped.eh_frame = {true, 0x10, 0x1010, 0x100};
    full.mapped.eh_frame_hdr = {true, 0x20, 0x1020, 0x30};
    r = ResolveLiveModuleFacts(full);
    CHECK(r.ok);
    CHECK(r.inputs.lifetime_owner == full.lifetime_owner);
    CHECK(r.inputs.main_exe.base == full.main_exe.base);
    CHECK(r.inputs.main_exe.size == full.main_exe.size);
    CHECK(r.inputs.monolithic);
    CHECK(r.inputs.has_serialized_witness);
    CHECK(r.inputs.module_file_path == full.module_file_path);
    CHECK(r.inputs.game_executable_path == full.game_executable_path);
    CHECK(r.inputs.mapped.image_base == full.mapped.image_base);
}

// ===========================================================================
// SEAM S2 adapter: refusal paths
// ===========================================================================
static auto base_profile() -> LayoutProfileFacts
{
    LayoutProfileFacts profile;
    profile.pair_staged = true;
    profile.engine_major = 5;
    profile.engine_minor = 6;
    profile.non_shipping = LayoutProfileFacts::Axis::Yes;
    profile.stats = LayoutProfileFacts::Axis::No;
    profile.validation = LayoutValidationProof::MintForHostFixture();
    for (const auto& key : RequiredLayoutEntries())
        profile.entries.push_back(
            {key, LayoutEntryFact::Origin::ValidatedProfile, 8, 100});
    return profile;
}

static void test_layout_refusals()
{
    // 1. Profile absent (worker-2 in flight).
    auto r = ResolveLayoutProfile(std::nullopt);
    CHECK(!r.ok && r.refusal == LayoutRefusal::ProfileAbsent);

    // 2. Unstaged pair.
    auto unstaged = base_profile();
    unstaged.pair_staged = false;
    r = ResolveLayoutProfile(unstaged);
    CHECK(!r.ok && r.refusal == LayoutRefusal::PairNotStaged);

    // 3. Version absent.
    auto no_version = base_profile();
    no_version.engine_minor.reset();
    r = ResolveLayoutProfile(no_version);
    CHECK(!r.ok && r.refusal == LayoutRefusal::VersionAbsent);

    // 4. Axes unknown — defaults never establish target facts.
    auto axes = base_profile();
    axes.stats = LayoutProfileFacts::Axis::Unknown;
    r = ResolveLayoutProfile(axes);
    CHECK(!r.ok && r.refusal == LayoutRefusal::AxesUnknown);
    axes = base_profile();
    axes.non_shipping = LayoutProfileFacts::Axis::Unknown;
    r = ResolveLayoutProfile(axes);
    CHECK(!r.ok && r.refusal == LayoutRefusal::AxesUnknown);

    // 5. No validated-profile proof.
    auto unproven = base_profile();
    unproven.validation = {};
    r = ResolveLayoutProfile(unproven);
    CHECK(!r.ok && r.refusal == LayoutRefusal::ProofAbsent);

    // 6. Required entry missing.
    auto missing = base_profile();
    missing.entries.pop_back();
    r = ResolveLayoutProfile(missing);
    CHECK(!r.ok && r.refusal == LayoutRefusal::EntryMissing);
    CHECK(contains(r.message, "UFunction"));

    // 7. Guessed origin (PDB default / another game) never admitted.
    auto guessed = base_profile();
    guessed.entries[0].origin = LayoutEntryFact::Origin::Guessed;
    r = ResolveLayoutProfile(guessed);
    CHECK(!r.ok && r.refusal == LayoutRefusal::EntryOriginGuessed);

    // 8. Unknown origin never admitted.
    auto unknown = base_profile();
    unknown.entries[0].origin = LayoutEntryFact::Origin::Unknown;
    r = ResolveLayoutProfile(unknown);
    CHECK(!r.ok && r.refusal == LayoutRefusal::EntryOriginGuessed);

    // 9. Unbounded width.
    auto wide = base_profile();
    wide.entries[0].width.reset();
    r = ResolveLayoutProfile(wide);
    CHECK(!r.ok && r.refusal == LayoutRefusal::EntryWidthUnbounded);

    // 10. Confidence below threshold.
    auto shaky = base_profile();
    shaky.entries[0].confidence = 50;
    r = ResolveLayoutProfile(shaky);
    CHECK(!r.ok && r.refusal == LayoutRefusal::ConfidenceLow);
    CHECK(contains(r.message, "below admission threshold"));

    // 11. Accept: validated profile resolves to the exact publication tuple.
    r = ResolveLayoutProfile(base_profile());
    CHECK(r.ok);
    CHECK(r.facts.major == 5 && r.facts.minor == 6);
    CHECK(r.facts.non_shipping && !r.facts.stats);
}

// ===========================================================================
// Gate: refusal preserves state; no silent rebind
// ===========================================================================
static void test_gate()
{
    GameThreadQueryGate gate;
    CHECK(!gate.armed());
    CHECK(gate.state() == GameThreadQueryGate::State::Disarmed);

    const auto ok_loader = ResolveLiveModuleFacts(base_facts());
    const auto ok_layout = ResolveLayoutProfile(base_profile());
    const auto no_loader = ResolveLiveModuleFacts(std::nullopt);
    const auto no_layout = ResolveLayoutProfile(std::nullopt);

    auto res = gate.Arm(no_loader, no_layout);
    CHECK(res.state == GameThreadQueryGate::State::Refused);
    CHECK(gate.state() == GameThreadQueryGate::State::Disarmed);
    CHECK(contains(gate.last_refusal_detail(), "S1 unresolved"));

    res = gate.Arm(ok_loader, no_layout);
    CHECK(res.state == GameThreadQueryGate::State::Refused);
    CHECK(gate.state() == GameThreadQueryGate::State::Disarmed);
    CHECK(contains(gate.last_refusal_detail(), "S2 unresolved"));

    res = gate.Arm(ok_loader, ok_layout);
    CHECK(res.state == GameThreadQueryGate::State::Armed);
    CHECK(gate.armed());

    // Refusal after arming preserves the armed state and records the detail.
    res = gate.Arm(no_loader, ok_layout);
    CHECK(res.state == GameThreadQueryGate::State::Refused);
    CHECK(gate.armed()); // preserved
    CHECK(contains(gate.last_refusal_detail(), "already armed"));
}

// ===========================================================================
// Real RuntimeOwner guards (existing immutable source, compiled as-is)
// ===========================================================================
struct OwnerControl
{
    std::mutex mutex;
    std::condition_variable changed;
    bool entered{};
    bool stopping{};            // main thread requested stop
    bool fail{};                // backend should throw
    bool exit_without_ready{};  // return without ever firing ready
    bool exited{};
    bool self_join_attempted{}; // init thread tried stop() on itself
    bool self_join_refused{};   // real guard made stop() return false
    std::function<void()> ready;
};

// Waits for stop/fail/exit; in the normal path fires ready (possibly late,
// i.e. after stop was already requested — exercising the real terminal-state
// guard) and then exits.
class ControlledBackend final : public RuntimeBackend
{
    std::shared_ptr<OwnerControl> c_;

public:
    explicit ControlledBackend(std::shared_ptr<OwnerControl> c) : c_(std::move(c)) {}
    void initialize(std::function<void()> ready) override
    {
        std::unique_lock lock(c_->mutex);
        c_->entered = true;
        c_->ready = std::move(ready);
        c_->changed.notify_all();
        c_->changed.wait(lock, [&] {
            return c_->stopping || c_->fail || c_->exit_without_ready;
        });
        if (c_->fail)
        {
            c_->exited = true;
            throw std::runtime_error("engine unavailable");
        }
        if (c_->exit_without_ready)
        {
            c_->exited = true;
            return;
        }
        auto signal = c_->ready;
        lock.unlock();
        signal(); // late-ready path when stopping was requested first
        lock.lock();
        c_->exited = true;
    }
    void request_stop() noexcept override
    {
        std::lock_guard lock(c_->mutex);
        c_->stopping = true;
        c_->changed.notify_all();
    }
};

// Receives the owner pointer and attempts stop() from the init thread.
class SelfJoinBackend final : public RuntimeBackend
{
    std::shared_ptr<OwnerControl> c_;
    std::atomic<RuntimeOwner*> owner_{nullptr};

public:
    explicit SelfJoinBackend(std::shared_ptr<OwnerControl> c) : c_(std::move(c)) {}
    void attach_owner(RuntimeOwner* owner) noexcept { owner_.store(owner); }
    void initialize(std::function<void()> ready) override
    {
        {
            std::lock_guard lock(c_->mutex);
            c_->entered = true;
            c_->ready = std::move(ready);
            c_->changed.notify_all();
        }
        if (auto* owner = owner_.load())
        {
            const bool stopped_from_init_thread = owner->stop();
            std::lock_guard lock(c_->mutex);
            c_->self_join_attempted = true;
            c_->self_join_refused = !stopped_from_init_thread;
            c_->changed.notify_all();
        }
        std::unique_lock lock(c_->mutex);
        c_->changed.wait(lock, [&] { return c_->stopping; });
        c_->exited = true;
    }
    void request_stop() noexcept override
    {
        std::lock_guard lock(c_->mutex);
        c_->stopping = true;
        c_->changed.notify_all();
    }
};

static void wait_entered(const std::shared_ptr<OwnerControl>& c)
{
    std::unique_lock lock(c->mutex);
    c->changed.wait(lock, [&] { return c->entered; });
}

static void test_owner_normal_lifecycle()
{
    auto control = std::make_shared<OwnerControl>();
    RuntimeOwner owner{std::make_unique<ControlledBackend>(control)};
    CHECK(owner.status().state == RuntimeState::Prepared);
    CHECK(owner.start());
    CHECK(!owner.start()); // second start refused while not Prepared
    wait_entered(control);
    CHECK(owner.status().state == RuntimeState::Starting);

    { // Fire ready from the test while still Starting: -> EngineInitialized.
        std::unique_lock lock(control->mutex);
        auto ready = control->ready;
        lock.unlock();
        ready();
    }
    CHECK(owner.status().state == RuntimeState::EngineInitialized);

    CHECK(owner.stop());
    CHECK(owner.status().state == RuntimeState::Stopped);
    // Backend then fires its late ready after stop; terminal state must hold.
    {
        std::unique_lock lock(control->mutex);
        control->changed.wait(lock, [&] { return control->exited; });
        auto late = control->ready;
        lock.unlock();
        if (late) late();
    }
    CHECK(owner.status().state == RuntimeState::Stopped);
}

static void test_owner_failed_without_ready()
{
    auto control = std::make_shared<OwnerControl>();
    RuntimeOwner owner{std::make_unique<ControlledBackend>(control)};
    CHECK(owner.start());
    wait_entered(control);
    {
        std::lock_guard lock(control->mutex);
        control->exit_without_ready = true; // returns; NO stop ever requested
        control->changed.notify_all();
    }
    // The init thread finishes on its own -> the owner records the honest
    // failure ("init returned without a stop request"), per runtime_owner.cpp:91.
    bool reached_failed = false;
    for (int i = 0; i < 2000 && !reached_failed; ++i)
    {
        reached_failed = owner.status().state == RuntimeState::Failed;
        if (!reached_failed) std::this_thread::sleep_for(1ms);
    }
    CHECK(reached_failed);
    const auto status = owner.status();
    CHECK(contains(status.error, "init returned without a stop request"));
    // Cleanup stop() must not hang and must NOT overwrite the Failed state.
    CHECK(owner.stop());
    CHECK(owner.status().state == RuntimeState::Failed);
    CHECK(owner.status().error == status.error);
}

static void test_owner_exception_preserves_failed()
{
    auto control = std::make_shared<OwnerControl>();
    RuntimeOwner owner{std::make_unique<ControlledBackend>(control)};
    CHECK(owner.start());
    wait_entered(control);
    {
        std::lock_guard lock(control->mutex);
        control->fail = true;
        control->changed.notify_all();
    }
    CHECK(owner.stop());
    const auto status = owner.status();
    CHECK(status.state == RuntimeState::Failed); // stop() never overwrites Failed
    CHECK(contains(status.error, "engine unavailable"));
}

static void test_owner_self_join_refused()
{
    auto control = std::make_shared<OwnerControl>();
    auto backend = std::make_unique<SelfJoinBackend>(control);
    auto* raw = backend.get();
    RuntimeOwner owner{std::move(backend)};
    raw->attach_owner(&owner);
    CHECK(owner.start());
    { // Wait until the init thread attempted stop() on itself.
        std::unique_lock lock(control->mutex);
        control->changed.wait(lock, [&] { return control->self_join_attempted; });
    }
    CHECK(control->self_join_refused); // real guard: stop() returned false
    CHECK(owner.stop());               // main thread stop joins normally
    CHECK(owner.status().state == RuntimeState::Stopped);
}

static void test_owner_late_ready_cannot_overwrite_stop()
{
    auto control = std::make_shared<OwnerControl>();
    RuntimeOwner owner{std::make_unique<ControlledBackend>(control)};
    CHECK(owner.start());
    wait_entered(control);
    // Request stop BEFORE ready ever fires; the backend then fires a late
    // ready inside initialize(). The real guard keeps the terminal state
    // (runtime_owner.cpp:83-84 allows only Starting -> EngineInitialized).
    std::thread stopper{[&] { owner.stop(); }};
    stopper.join();
    CHECK(owner.status().state == RuntimeState::Stopped);
}

// ===========================================================================
// Real game-thread identity affinity guards (header-only, as-is)
// ===========================================================================
static void test_identity_guards()
{
    using RC::Unreal::NativeThreadDetail::Identity;
    using RC::Unreal::NativeThreadDetail::Publication;
    Identity identity;

    // Too early: id() throws the honest error, raw accessors refuse admission.
    bool threw = false;
    try { (void)identity.id(); }
    catch (const std::runtime_error& e)
    {
        threw = contains(e.what(), "not yet initialized by UGameEngine::Tick");
    }
    CHECK(threw);
    CHECK(!identity.initialized());
    CHECK(!identity.in_current_raw());
    CHECK(identity.id_raw() == std::thread::id{});

    // First genuine publication wins.
    CHECK(identity.publish_current() == Publication::Published);
    CHECK(identity.initialized());
    CHECK(identity.in_current_raw());
    CHECK(identity.in_current());

    // Same thread: AlreadyCurrent.
    CHECK(identity.publish_current() == Publication::AlreadyCurrent);

    // Different thread: DifferentThread; original identity retained.
    std::atomic<bool> other_saw_different{false};
    std::thread other{[&] {
        other_saw_different =
            identity.publish_current() == Publication::DifferentThread;
    }};
    other.join();
    CHECK(other_saw_different.load());
    CHECK(identity.in_current()); // main thread still the published identity
}

// ===========================================================================
int main()
{
    test_loader_refusals();
    test_layout_refusals();
    test_gate();
    test_owner_normal_lifecycle();
    test_owner_failed_without_ready();
    test_owner_exception_preserves_failed();
    test_owner_self_join_refused();
    test_owner_late_ready_cannot_overwrite_stop();
    test_identity_guards();
    std::printf("seam_host_test: ALL CHECKS PASSED\n");
    return 0;
}
