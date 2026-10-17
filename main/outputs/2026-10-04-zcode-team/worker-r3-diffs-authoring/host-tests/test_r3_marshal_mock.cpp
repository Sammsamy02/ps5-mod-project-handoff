// worker-r3-diffs-authoring host test — DIFF 1/DIFF 2 logic under proven mock shapes.
// Mirrors the worker-no-exec-mode-gate gate_pattern_mock / worker-query-s2-arm host-test
// discipline: standalone host clang++ build, -Wall -Wextra -Werror, programmatic
// assertions, writes nothing outside the worker dir. The REAL units are compiled against
// the prospero argv by compile_gate.py; THIS file verifies the drain/pre-check/ladder/
// mode-fold logic the diffs insert, decoupled from engine headers.
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static int g_checks = 0;
#define CHECK(cond)                                                                                                   \
    do {                                                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); return 1; }                        \
        ++g_checks;                                                                                                   \
    } while (0)

// ===================== mock shapes (mirror the real signatures) =====================
namespace mock {

using CallbackId = int32_t;
static constexpr CallbackId InvalidCallbackId = -1;

enum class GameThreadExecutionMethod { ProcessEvent, EngineTick, FuncPtrMarshal };

// NativeGameThreadIdentity.hpp semantics (ue4ss-native-game-thread-1), verbatim shape.
enum class Publication { Published, AlreadyCurrent, DifferentThread, Unavailable };
struct Identity {
    mutable std::mutex mutex_;
    std::thread::id id_{};
    auto publish_current() noexcept -> Publication
    {
        const auto current = std::this_thread::get_id();
        if (current == std::thread::id{}) return Publication::Unavailable;
        try {
            std::lock_guard lock{mutex_};
            if (id_ == std::thread::id{}) { id_ = current; return Publication::Published; }
            return id_ == current ? Publication::AlreadyCurrent : Publication::DifferentThread;
        } catch (...) { return Publication::Unavailable; }
    }
    auto id_raw() const noexcept -> std::thread::id
    {
        try { std::lock_guard lock{mutex_}; return id_; } catch (...) { return {}; }
    }
};

// DIFF 2 bridge mock: cancellation + publication mapping (Published/AlreadyCurrent -> true).
struct InitializerSide {
    Identity identity;
    std::atomic_bool pump_pending{false};
    bool startup_cancelled = false;
    bool disarm_warned = false;
    bool mode = false; // R3MarshalGate::kR3MarshalMode

    auto publish_bridge() -> bool
    {
        if (!mode) { disarm_warned = true; return false; } // disarmed bridges refuse loudly
        if (startup_cancelled) return false;
        const auto publication = identity.publish_current();
        if (publication == Publication::Unavailable) return false;
        return publication != Publication::DifferentThread;
    }
    auto pump_pending_bridge() -> bool { return pump_pending.load(std::memory_order_acquire); }
};

// LuaMod drain-side mock: the DIFF 1 drain gating + queue discipline.
struct SimpleAction { int payload; int* run_log; };
struct DrainWorld {
    InitializerSide* init_side;
    std::vector<SimpleAction> actions{};
    std::vector<SimpleAction> pending_actions{};
    std::recursive_mutex actions_mutex{};
    bool is_processing_actions = false;
    bool is_currently_executing_game_action = false;
    bool identity_published = false;
    bool pump_pending_logged = false;

    auto marshal_drain() -> void
    {
        // [R3] thread honesty, ORDER MATTERS: publication first (it DEFINES the game
        // thread), then the raw gate refuses every other thread from then on.
        if (!identity_published) {
            if (!init_side->publish_bridge()) return; // drain NOTHING before publication
            identity_published = true;
            if (init_side->pump_pending_bridge()) pump_pending_logged = true;
        }
        if (!is_game_thread_raw()) return;
        { std::lock_guard guard{actions_mutex}; is_processing_actions = true; }
        drain_simple(actions);
        {   // the pending-queue swap discipline (process_event_hook tail)
            std::lock_guard guard{actions_mutex};
            is_processing_actions = false;
            if (!pending_actions.empty()) {
                actions.insert(actions.end(), pending_actions.begin(), pending_actions.end());
                pending_actions.clear();
            }
        }
    }
    auto is_game_thread_raw() const -> bool
    {
        const auto known = init_side->identity.id_raw();
        return known != std::thread::id{} && known == std::this_thread::get_id();
    }
    static auto drain_simple(std::vector<SimpleAction>& q) -> void
    {
        std::vector<SimpleAction> to_run{};
        { std::lock_guard guard{DrainWorld::s_mutex_for(q)}; if (q.empty()) return; to_run = std::move(q); q.clear(); }
        std::vector<SimpleAction> deferred{};
        for (auto& a : to_run) {
            if (s_executing_flag_for(q)) { deferred.push_back(a); continue; }
            *a.run_log += 1;
        }
        if (!deferred.empty()) { std::lock_guard guard{DrainWorld::s_mutex_for(q)}; q.insert(q.end(), deferred.begin(), deferred.end()); }
    }
    // mock singletons standing in for the LuaMod statics (one queue per world here)
    static std::recursive_mutex& s_mutex_for(std::vector<SimpleAction>&) { static std::recursive_mutex m; return m; }
    static bool& s_executing_flag_for(std::vector<SimpleAction>&) { static bool f = false; return f; }
};

// DIFF 1 carrier pre-check + registration mock (UFunction.cpp:78-89 shape).
struct UFunctionMock {
    void* func_ptr = nullptr;
    bool func_native = false;
    int register_posthook_calls = 0;
};
struct ProcessInternalHolder { void* address = reinterpret_cast<void*>(0x51DE); };
static ProcessInternalHolder g_process_internal;

auto carrier_precheck(UFunctionMock* carrier, bool& refused_logged) -> bool
{
    refused_logged = false;
    if (!carrier) { refused_logged = true; return false; }
    if (!carrier->func_ptr) { refused_logged = true; return false; }
    if (carrier->func_ptr == g_process_internal.address) { refused_logged = true; return false; }
    if (!carrier->func_native) { refused_logged = true; return false; }
    return true;
}

struct ArmResult { bool ok; CallbackId cb; };
auto arm_drain(UFunctionMock* carrier, int carrier_id, int& armed_carrier_id, bool& armed, CallbackId& armed_cb) -> ArmResult
{
    if (armed) {
        if (carrier_id != armed_carrier_id) return {false, InvalidCallbackId}; // different carrier refused
        return {true, armed_cb};                                              // idempotent same-carrier
    }
    bool refused = false;
    if (!carrier_precheck(carrier, refused)) return {false, InvalidCallbackId};
    // RegistrationMock: RegisterPostHook -> GetFunctionHookData (save original + SetFuncPtr)
    // -> AddPostCallback; the mock records the registration and mints an id.
    carrier->register_posthook_calls += 1;
    const CallbackId real_cb = 7;
    armed = true; armed_carrier_id = carrier_id; armed_cb = real_cb;
    return {true, real_cb};
}

// DIFF 1 three-state ladder resolution (WithDelay-family shape).
struct LadderWorld {
    bool tick_available = false;
    bool pe_available = false;
    bool marshal_armed = false;
    bool threw = false;
    std::string throw_msg;
    auto ensure_tick() { if (!tick_available && !threw) { threw = true; throw_msg = "registration failed"; } }
    auto resolve(GameThreadExecutionMethod method) -> GameThreadExecutionMethod
    {
        threw = false;
        if (method == GameThreadExecutionMethod::EngineTick) {
            if (!tick_available) {
                if (pe_available) method = GameThreadExecutionMethod::ProcessEvent;
                else if (!marshal_armed) { threw = true; throw_msg = "neither available and marshal not armed"; }
                else method = GameThreadExecutionMethod::FuncPtrMarshal;
            }
        } else if (method == GameThreadExecutionMethod::ProcessEvent) {
            if (!pe_available) {
                if (tick_available) method = GameThreadExecutionMethod::EngineTick;
                else if (!marshal_armed) { threw = true; throw_msg = "neither available and marshal not armed"; }
                else method = GameThreadExecutionMethod::FuncPtrMarshal;
            }
        }
        return method;
    }
};

// DIFF 2 constexpr mode-fold pattern (mirrors R3MarshalGate + the EngineTick gate).
int mode0_registrations = 0, mode1_registrations = 0, mode1_refusals = 0;
template <bool kR3MarshalMode>
auto engine_tick_gate() -> bool // returns pump_pending after the gate
{
    if constexpr (kR3MarshalMode) {
        ++mode1_refusals; // refuse_registration_site + deferral line + pump_pending
        return true;
    } else {
        ++mode0_registrations; // RegisterEngineTickPreCallback x2, throw on ERROR_ID
        return false;
    }
}

} // namespace mock

int main()
{
    using namespace mock;

    // ---- 1. DIFF 2 mode fold: disarmed = registrations, armed = deferral + pump_pending
    CHECK(engine_tick_gate<false>() == false);
    CHECK(mode0_registrations == 1);
    CHECK(engine_tick_gate<true>() == true);
    CHECK(mode1_registrations == 0 && mode1_refusals == 1);

    // ---- 2. bridge publication mapping + cancellation + disarm refusal
    InitializerSide init{};
    CHECK(init.publish_bridge() == false && init.disarm_warned); // disarmed refuses
    init.mode = true;
    init.startup_cancelled = true;
    CHECK(init.publish_bridge() == false); // cancelled: never publish
    init.startup_cancelled = false;
    int main_runs = 0, late_worker_runs = 0;
    init.pump_pending.store(true); // the initializer deferred its pump BEFORE any drain
    DrainWorld world{&init};
    world.actions.push_back({41, &main_runs});
    world.marshal_drain(); // first accepted invocation: publishes, consumes pump_pending, drains
    CHECK(world.identity_published && main_runs == 1);
    CHECK(init.identity.id_raw() == std::this_thread::get_id()); // publication DEFINES the game thread
    // post-publication worker invocation: raw gate refuses, drains nothing
    world.actions.push_back({42, &late_worker_runs});
    std::thread worker([&] { world.marshal_drain(); });
    worker.join();
    CHECK(late_worker_runs == 0); // worker thread drained NOTHING
    CHECK(world.pump_pending_logged); // pump-pending consumed at publication, exactly once
    // RACE SEMANTICS (R3D 4.3 + CARRIER-CANDIDATES C-4): BEFORE any publication, the
    // first thread to invoke wins publish_current and BECOMES the game thread - the
    // drain cannot distinguish threads pre-publication, which is exactly why the
    // calibration must prove the carrier game-thread-stable before arming. Pinned here
    // so the behavior is contractual, not accidental.
    {
        InitializerSide init2{}; init2.mode = true;
        DrainWorld world2{&init2};
        int first_runs = 0, main_late = 0;
        world2.actions.push_back({44, &first_runs});
        std::thread first([&] { world2.marshal_drain(); }); // worker wins the race
        first.join();
        CHECK(world2.identity_published && first_runs == 1); // first-publisher wins
        world2.actions.push_back({45, &main_late});
        world2.marshal_drain(); // the true game thread is now refused until recalibration
        CHECK(main_late == 0);  // ...documenting the poisoning hazard C-4 owns
    }

    // ---- 3. carrier pre-check refusals (loud, no store) + arm idempotence
    UFunctionMock null_func{nullptr, true}; // non-null carrier, NULL FuncPtr
    UFunctionMock script_func{g_process_internal.address, false};
    UFunctionMock non_native{reinterpret_cast<void*>(0x9568000), false};
    UFunctionMock good{reinterpret_cast<void*>(0x97ba110), true};
    bool refused = false;
    CHECK(!carrier_precheck(nullptr, refused) && refused);
    CHECK(!carrier_precheck(&null_func, refused) && refused);
    CHECK(!carrier_precheck(&script_func, refused) && refused);
    CHECK(!carrier_precheck(&non_native, refused) && refused);
    int armed_id = -1; bool armed = false; CallbackId armed_cb = InvalidCallbackId;
    CHECK(!arm_drain(&script_func, 10, armed_id, armed, armed_cb).ok);
    CHECK(arm_drain(&good, 11, armed_id, armed, armed_cb).ok && armed_cb == 7);
    CHECK(arm_drain(&good, 11, armed_id, armed, armed_cb).ok);            // same carrier idempotent
    CHECK(!arm_drain(&non_native, 12, armed_id, armed, armed_cb).ok);     // different carrier refused once armed

    // ---- 4. ladder: EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw
    LadderWorld lw{};
    auto m = lw.resolve(GameThreadExecutionMethod::EngineTick);
    CHECK(lw.threw && m == GameThreadExecutionMethod::EngineTick); // nothing available, marshal unarmed -> throw
    lw.marshal_armed = true;
    m = lw.resolve(GameThreadExecutionMethod::EngineTick);
    CHECK(!lw.threw && m == GameThreadExecutionMethod::FuncPtrMarshal); // third rung taken
    lw.pe_available = true; lw.marshal_armed = false;
    m = lw.resolve(GameThreadExecutionMethod::EngineTick);
    CHECK(!lw.threw && m == GameThreadExecutionMethod::ProcessEvent); // second rung precedes marshal
    lw.pe_available = false; lw.tick_available = true;
    m = lw.resolve(GameThreadExecutionMethod::ProcessEvent);
    CHECK(!lw.threw && m == GameThreadExecutionMethod::EngineTick); // PE-default falls back to live tick

    std::printf("PASS: %d checks\n", g_checks);
    return 0;
}
