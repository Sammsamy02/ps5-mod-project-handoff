// Host tests for the rung-5 input bridge (worker-real-input, 2026-10-04).
// Synthetic pad sequences are injected through the retained provider's OWN test
// surface (PadBackend function pointers, PS5PadSource.hpp:14-23). The retained
// RC::Input::PS5PadSource is the parity oracle for PadEdgeSource (non-hardened
// sequences must produce identical events; hardened sequences strictly fewer).
#include "PadEdgeSource.hpp"
#include "KeybindDispatchGate.hpp"
#include <PS5PadSource.hpp>
#include <Input/Handler.hpp>
#include <cstdio>
#include <thread>

using namespace modbridge::input;
using RC::Input::InputEvent;
using RC::Input::Key;
using RC::Input::ModifierKeys;
using RC::Input::PadBackend;
using RC::Input::PadBinding;
using RC::Input::PS5PadSource;
using ::PS5_PadData;

static int g_failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, msg); ++g_failures; } } while (0)

// ---- scripted backend (statics: PadBackend is plain function pointers) ----
static std::vector<PS5_PadData> g_samples;
static size_t g_read_index = 0;
static int tb_user_initialize(void*) { return 0; }
static int tb_pad_initialize() { return 0; }
static int tb_user_list(int users[4]) { users[0] = 1; users[1] = users[2] = users[3] = -1; return 0; }
static int tb_open(int, int, int, void*) { return 9; }
static int tb_read_state(int, PS5_PadData* data)
{
    if (g_samples.empty()) return -1;
    // Past the script: repeat the final cached sample (a duplicate read cannot
    // create another keydown in either implementation).
    *data = g_samples[g_read_index < g_samples.size() ? g_read_index++ : g_samples.size() - 1];
    return 0;
}
static int tb_close(int) { return 0; }
static int tb_user_terminate() { return 0; }
static PadBackend scripted_backend()
{
    PadBackend b{};
    b.user_initialize = tb_user_initialize; b.pad_initialize = tb_pad_initialize;
    b.user_list = tb_user_list; b.open = tb_open; b.read_state = tb_read_state;
    b.close = tb_close; b.user_terminate = tb_user_terminate;
    return b;
}
static PS5_PadData sample(uint64_t ts_us, uint32_t buttons, bool connected = true)
{
    PS5_PadData d{};
    d.buttons = buttons; d.connected = connected ? 1 : 0; d.timestamp = ts_us;
    return d;
}

struct CallPlan
{
    PS5_PadData sample;
    std::array<bool, RC::Input::max_keys> subscribed{};
    bool allow{true};
};
// One poll per plan entry: each poll() performs exactly one read_state and advances
// the scripted sequence once. Cached-read skip is covered by explicit identical-
// timestamp plan entries (a genuine duplicate ScePadReadState).
static void load_script(const std::vector<CallPlan>& plan)
{
    g_samples.clear();
    for (const auto& call : plan) g_samples.push_back(call.sample);
    g_read_index = 0;
}
static std::vector<InputEvent> run_retained(const std::vector<PadBinding>& bindings,
                                            const std::vector<CallPlan>& plan)
{
    load_script(plan);
    PS5PadSource source{bindings, scripted_backend(), -1};
    CHECK(source.activate(), "retained activate");
    std::vector<InputEvent> out;
    for (const auto& call : plan)
    {
        const auto& events = source.poll(call.subscribed, call.allow);
        out.insert(out.end(), events.begin(), events.end());
    }
    CHECK(source.deactivate(), "retained deactivate");
    return out;
}
static std::vector<InputEvent> run_adapter(const std::vector<PadBinding>& bindings,
                                           const std::vector<CallPlan>& plan, PadEdgePolicy policy,
                                           size_t* suppressed = nullptr)
{
    load_script(plan);
    PadEdgeSource source{bindings, scripted_backend(), -1, policy};
    CHECK(source.activate(), "adapter activate");
    std::vector<InputEvent> out;
    for (const auto& call : plan)
    {
        const auto& events = source.poll(call.subscribed, call.allow);
        out.insert(out.end(), events.begin(), events.end());
    }
    if (suppressed) *suppressed = source.suppressed_edge_count();
    CHECK(source.deactivate(), "adapter deactivate");
    return out;
}
static std::array<bool, RC::Input::max_keys> only(Key k)
{
    std::array<bool, RC::Input::max_keys> s{};
    s[static_cast<size_t>(k)] = true;
    return s;
}
static std::vector<PadBinding> item_spawner_profile()
{
    // INPUT-MAP.md §1 default profile.
    return {
        {PS5_PAD_BUTTON_OPTIONS, Key::F7, {}},
        {PS5_PAD_BUTTON_UP, Key::UP_ARROW, {}},
        {PS5_PAD_BUTTON_DOWN, Key::DOWN_ARROW, {}},
        {PS5_PAD_BUTTON_CROSS, Key::RETURN, {}},
        {PS5_PAD_BUTTON_LEFT, Key::LEFT_ARROW, {}},
        {PS5_PAD_BUTTON_RIGHT, Key::RIGHT_ARROW, {}},
    };
}
static std::string keys_of(const std::vector<InputEvent>& events)
{
    std::string s;
    for (const auto& e : events) { s += std::to_string(static_cast<int>(e.key)); s += ','; }
    return s;
}

int main()
{
    const auto sub_all_spawner = [](CallPlan& c) { for (const auto& b : item_spawner_profile()) c.subscribed[static_cast<size_t>(b.key)] = true; };

    // 1. single press parity (OPTIONS -> F7)
    {
        std::vector<CallPlan> plan;
        auto add = [&](PS5_PadData s) { CallPlan c{}; c.sample = s; sub_all_spawner(c); plan.push_back(c); };
        add(sample(1'000'000, 0));
        add(sample(2'000'000, PS5_PAD_BUTTON_OPTIONS));
        add(sample(2'000'000, PS5_PAD_BUTTON_OPTIONS)); // duplicate cached read: must not re-fire
        add(sample(3'000'000, 0));
        const auto retained = run_retained(item_spawner_profile(), plan);
        const auto adapter = run_adapter(item_spawner_profile(), plan, PadEdgePolicy{0});
        CHECK(retained.size() == 1 && retained[0].key == Key::F7, "retained single press");
        CHECK(keys_of(adapter) == keys_of(retained), "parity single press");
    }

    // 2. debounce: bounce collapses; re-arm after 40ms pad time
    {
        std::vector<CallPlan> plan;
        auto add = [&](PS5_PadData s) { CallPlan c{}; c.sample = s; sub_all_spawner(c); plan.push_back(c); };
        add(sample(1'000'000, 0));
        add(sample(5'000'000, PS5_PAD_BUTTON_OPTIONS)); // press -> emit
        add(sample(5'000'200, 0));                       // bounce release
        add(sample(5'000'400, PS5_PAD_BUTTON_OPTIONS));  // bounce re-press (suppressed by adapter)
        add(sample(5'000'600, 0));
        add(sample(5'010'000, PS5_PAD_BUTTON_OPTIONS));  // 10ms later (still suppressed: <40ms since emit)
        add(sample(5'012'000, 0));
        add(sample(5'060'000, PS5_PAD_BUTTON_OPTIONS));  // 60ms since emit -> emits
        const auto retained = run_retained(item_spawner_profile(), plan);
        size_t suppressed = 0;
        const auto adapter = run_adapter(item_spawner_profile(), plan, PadEdgePolicy{40'000}, &suppressed);
        CHECK(retained.size() == 4, "retained sees every physical edge (4)");
        CHECK(adapter.size() == 2 && adapter[0].key == Key::F7 && adapter[1].key == Key::F7, "adapter collapses bounce (2)");
        CHECK(suppressed == 2, "adapter dropped exactly 2 bounce edges");
    }

    // 3. no-repeat while held
    {
        std::vector<CallPlan> plan;
        auto add = [&](PS5_PadData s) { CallPlan c{}; c.sample = s; sub_all_spawner(c); plan.push_back(c); };
        add(sample(1'000'000, 0));
        for (int i = 0; i < 10; ++i) add(sample(2'000'000 + 1000ull * i, PS5_PAD_BUTTON_CROSS));
        const auto retained = run_retained(item_spawner_profile(), plan);
        const auto adapter = run_adapter(item_spawner_profile(), plan, PadEdgePolicy{0});
        CHECK(retained.size() == 1 && retained[0].key == Key::RETURN, "retained no-repeat");
        CHECK(keys_of(adapter) == keys_of(retained), "parity no-repeat");
    }

    // 4. simultaneous CROSS+UP in one sample: both keys, once each, same poll
    {
        std::vector<CallPlan> plan;
        auto add = [&](PS5_PadData s) { CallPlan c{}; c.sample = s; sub_all_spawner(c); plan.push_back(c); };
        add(sample(1'000'000, 0));
        add(sample(2'000'000, PS5_PAD_BUTTON_CROSS | PS5_PAD_BUTTON_UP));
        add(sample(2'000'500, PS5_PAD_BUTTON_CROSS | PS5_PAD_BUTTON_UP));
        const auto adapter = run_adapter(item_spawner_profile(), plan, PadEdgePolicy{40'000});
        CHECK(adapter.size() == 2, "simultaneous produces two events");
        CHECK(adapter.size() == 2 && ((adapter[0].key == Key::RETURN && adapter[1].key == Key::UP_ARROW) ||
              (adapter[0].key == Key::UP_ARROW && adapter[1].key == Key::RETURN)), "simultaneous keys");
    }

    // 5. chord requires every button
    {
        const std::vector<PadBinding> chord{{PS5_PAD_BUTTON_L1 | PS5_PAD_BUTTON_R1, Key::F7, {}}};
        std::vector<CallPlan> plan;
        auto add = [&](PS5_PadData s) { CallPlan c{}; c.sample = s; c.subscribed = only(Key::F7); plan.push_back(c); };
        add(sample(1'000'000, 0));
        add(sample(2'000'000, PS5_PAD_BUTTON_L1));
        add(sample(3'000'000, PS5_PAD_BUTTON_L1 | PS5_PAD_BUTTON_R1));
        add(sample(4'000'000, PS5_PAD_BUTTON_R1));
        const auto retained = run_retained(chord, plan);
        const auto adapter = run_adapter(chord, plan, PadEdgePolicy{40'000});
        CHECK(retained.size() == 1 && retained[0].key == Key::F7, "retained chord");
        CHECK(keys_of(adapter) == keys_of(retained), "parity chord");
    }

    // 6. unsubscribe / allow=false / no phantom edge on re-enable
    {
        std::vector<CallPlan> plan;
        CallPlan press_unsubbed{}; press_unsubbed.sample = sample(2'000'000, PS5_PAD_BUTTON_OPTIONS);
        CallPlan still_held_now_subscribed{}; still_held_now_subscribed.sample = sample(2'001'000, PS5_PAD_BUTTON_OPTIONS);
        still_held_now_subscribed.subscribed = only(Key::F7);
        CallPlan release{}; release.sample = sample(3'000'000, 0); release.subscribed = only(Key::F7);
        CallPlan press_disabled{}; press_disabled.sample = sample(4'000'000, PS5_PAD_BUTTON_OPTIONS);
        press_disabled.subscribed = only(Key::F7); press_disabled.allow = false;
        CallPlan release2{}; release2.sample = sample(5'000'000, 0); release2.subscribed = only(Key::F7);
        CallPlan press_ok{}; press_ok.sample = sample(6'000'000, PS5_PAD_BUTTON_OPTIONS);
        press_ok.subscribed = only(Key::F7); press_ok.allow = true;
        plan = {press_unsubbed, still_held_now_subscribed, release, press_disabled, release2, press_ok};
        const auto adapter = run_adapter(item_spawner_profile(), plan, PadEdgePolicy{40'000});
        CHECK(adapter.size() == 1 && adapter[0].key == Key::F7, "gating: exactly the real press after (re)enable");
        // Parity: retained provider has the same gating semantics (PS5PadSource.cpp:85,89).
        const auto retained = run_retained(item_spawner_profile(), plan);
        CHECK(keys_of(adapter) == keys_of(retained), "parity gating");
    }

    // 7. disconnect clears state; reconnect swallows baseline (no phantom edge)
    {
        std::vector<CallPlan> plan;
        auto add = [&](PS5_PadData s) { CallPlan c{}; c.sample = s; c.subscribed = only(Key::F7); plan.push_back(c); };
        add(sample(1'000'000, 0));
        add(sample(2'000'000, PS5_PAD_BUTTON_OPTIONS)); // emit
        add(sample(3'000'000, 0, false));                // disconnected while pressed
        add(sample(4'000'000, PS5_PAD_BUTTON_OPTIONS));  // re-attach already down: baseline, no emit
        add(sample(5'000'000, 0));
        add(sample(6'000'000, PS5_PAD_BUTTON_OPTIONS));  // real press: emit
        const auto retained = run_retained(item_spawner_profile(), plan);
        const auto adapter = run_adapter(item_spawner_profile(), plan, PadEdgePolicy{40'000});
        CHECK(retained.size() == 2, "retained disconnect semantics");
        CHECK(keys_of(adapter) == keys_of(retained), "parity disconnect");
    }

    // 8. backwards pad timestamp: re-baseline, no fabricated edge
    {
        std::vector<CallPlan> plan;
        auto add = [&](PS5_PadData s) { CallPlan c{}; c.sample = s; c.subscribed = only(Key::F7); plan.push_back(c); };
        add(sample(10'000'000, 0));
        add(sample(11'000'000, PS5_PAD_BUTTON_OPTIONS)); // emit
        add(sample(12'000'000, 0));
        add(sample(9'000'000, PS5_PAD_BUTTON_OPTIONS));  // regression: adapter refuses, retained fabricates
        add(sample(9'500'000, 0));
        add(sample(13'000'000, PS5_PAD_BUTTON_OPTIONS)); // emit
        const auto retained = run_retained(item_spawner_profile(), plan);
        size_t suppressed = 0;
        const auto adapter = run_adapter(item_spawner_profile(), plan, PadEdgePolicy{40'000}, &suppressed);
        CHECK(retained.size() == 3, "retained fabricates an edge on regression");
        CHECK(adapter.size() == 2, "adapter drops the regression edge");
    }

    // 9. KeybindDispatch: matching rules vendored from Handler::process_event
    {
        KeybindDispatch dispatch;
        int f7_hits = 0, return_hits = 0;
        dispatch.register_keydown_event(Key::F7, {}, [&] { ++f7_hits; });
        dispatch.register_keydown_event(Key::RETURN, {}, [&] { ++return_hits; });
        CHECK(dispatch.is_keydown_event_registered(Key::F7), "F7 registered");
        const auto subscribed = dispatch.subscribed_keys();
        CHECK(subscribed[static_cast<size_t>(Key::F7)] && subscribed[static_cast<size_t>(Key::RETURN)], "subscriptions mirror registration");
        std::vector<InputEvent> events{{Key::F7, {}}, {Key::RETURN, {}}, {static_cast<Key>(0xFF), {}}};
        KeybindDispatch::invoke(dispatch.match(events));
        CHECK(f7_hits == 1 && return_hits == 1, "both callbacks fire once; out-of-range key skipped");

        // Registration inside a callback stays safe (snapshot-then-invoke).
        KeybindDispatch dispatch2;
        int hits = 0;
        dispatch2.register_keydown_event(Key::RETURN, {}, [&] {
            ++hits;
            dispatch2.register_keydown_event(Key::RETURN, {}, [&] { ++hits; });
        });
        KeybindDispatch::invoke(dispatch2.match({{Key::RETURN, {}}}));
        CHECK(hits == 1, "in-callback registration does not invalidate current dispatch");
        KeybindDispatch::invoke(dispatch2.match({{Key::RETURN, {}}}));
        CHECK(hits == 3, "next dispatch sees old+new registration");

        // Modifier equality is exact (Handler.cpp:89).
        KeybindDispatch dispatch3;
        int mod_hits = 0;
        ModifierKeys shift{}; shift.keys = 1u << RC::Input::SHIFT;
        dispatch3.register_keydown_event(Key::F7, shift, [&] { ++mod_hits; });
        KeybindDispatch::invoke(dispatch3.match({{Key::F7, {}}}));
        CHECK(mod_hits == 0, "unmodified event does not match SHIFT bind");
        KeybindDispatch::invoke(dispatch3.match({{Key::F7, shift}}));
        CHECK(mod_hits == 1, "SHIFT event matches SHIFT bind");
    }

    // 10. GameThreadGate: unknown/other-thread refusal preserved
    {
        GameThreadGate gate;
        int runs = 0;
        CHECK(gate.deliver_in_game_thread([&] { ++runs; }) == GameThreadGate::Deliver::RefusedUnknownIdentity,
              "unknown identity refuses");
        CHECK(runs == 0, "refused action never runs");
        gate.publish_current(); // genuine Tick publisher, this thread
        CHECK(gate.deliver_in_game_thread([&] { ++runs; }) == GameThreadGate::Deliver::Delivered, "game thread delivers");
        CHECK(runs == 1, "delivered action ran");

        std::thread worker([&] {
            GameThreadGate gate2; // identity published by the worker "Tick" thread only
            gate2.publish_current();
            std::thread other([&] {
                CHECK(gate2.deliver_in_game_thread([&] { ++runs; }) == GameThreadGate::Deliver::RefusedOtherThread,
                      "other thread refuses");
            });
            other.join();
            CHECK(gate2.deliver_in_game_thread([&] { ++runs; }) == GameThreadGate::Deliver::Delivered,
                  "publisher thread delivers");
            gate2.publish_current(); // second publication from a different thread must not steal
        });
        worker.join();
        CHECK(runs == 2, "exactly one delivered run from the worker gate");
    }

    // 11. rung-5 mini-chain on host: pad press -> edge -> keybind -> game-thread delivery
    {
        std::vector<CallPlan> plan;
        CallPlan prep{}; prep.sample = sample(1'000'000, 0); prep.subscribed = only(Key::F7);
        CallPlan press{}; press.sample = sample(2'000'000, PS5_PAD_BUTTON_OPTIONS); press.subscribed = only(Key::F7);
        plan = {prep, press};

        KeybindDispatch dispatch;
        GameThreadGate gate;
        int menu_toggles = 0;
        int refused = 0;
        dispatch.register_keydown_event(Key::F7, {}, [&] {
            // The mod's own wrapper: main.lua:694 ExecuteInGameThread(toggleMenu)
            if (gate.deliver_in_game_thread([&] { ++menu_toggles; }) != GameThreadGate::Deliver::Delivered)
                ++refused;
        });
        const auto pump = [&] {
            load_script(plan);
            PadEdgeSource source{item_spawner_profile(), scripted_backend(), -1, PadEdgePolicy{40'000}};
            CHECK(source.activate(), "chain activate");
            for (const auto& call : plan)
                KeybindDispatch::invoke(dispatch.match(source.poll(call.subscribed, call.allow)));
            CHECK(source.deactivate(), "chain deactivate");
        };
        pump(); // before the first genuine Tick: edge fires, delivery refuses, no menu
        CHECK(refused == 1 && menu_toggles == 0, "pre-identity press refused, no toggle");
        gate.publish_current();
        load_script(plan); // same press again after identity exists
        {
            PadEdgeSource source{item_spawner_profile(), scripted_backend(), -1, PadEdgePolicy{40'000}};
            CHECK(source.activate(), "chain2 activate");
            CallPlan p{}; p.sample = sample(1'000'000, 0); p.subscribed = only(Key::F7);
            KeybindDispatch::invoke(dispatch.match(source.poll(p.subscribed, p.allow)));
            CallPlan q{}; q.sample = sample(5'000'000, PS5_PAD_BUTTON_OPTIONS); q.subscribed = only(Key::F7);
            KeybindDispatch::invoke(dispatch.match(source.poll(q.subscribed, q.allow)));
            CHECK(source.deactivate(), "chain2 deactivate");
        }
        CHECK(refused == 1 && menu_toggles == 1, "post-identity press delivers exactly one toggle");
    }

    std::printf(g_failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
